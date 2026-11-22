// LoRa P2P receiver + Wi-Fi -> ThingsBoard MQTT gateway, for the Pycom LoPy 1.0 (ESP32 + SX1272)
//
// Sensor node: XIAO ESP32S3 + Wio-SX1262 (lora/node_xiao).
//
// Data flow:
//   node --LoRa--> this board --Wi-Fi/MQTT--> ThingsBoard gateway device "lopy"
//   Every packet is published through the ThingsBoard gateway API (v1/gateway/telemetry) as
//   telemetry of the device "sensor-<id>" (profile "Sensor", which runs the alarm rules and feeds
//   the "Forest ATV Detection" dashboard). Receiver health goes to the gateway device itself.
//
// Wi-Fi / ThingsBoard settings: copy include/secrets.example.h to include/secrets.h and fill it in.
//
// Flashing: bridge G23 <-> GND, connect USB, upload, remove the wire, reset.

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <sys/time.h>

#include "lora_link.h"

#if __has_include( "secrets.h" )
#include "secrets.h"
#else
#error "Missing include/secrets.h - copy include/secrets.example.h to include/secrets.h and fill it in"
#endif

// ---- LoPy SX1272 pins ----
#define LORA_SCK   5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_NSS  17
#define LORA_RST  18
#define LORA_DIO0 23

constexpr const char *MQTT_CLIENT_ID    = "lopy-receiver";
constexpr const char *TOPIC_GW_TELEMETRY = "v1/gateway/telemetry";
constexpr const char *TOPIC_GW_CONNECT   = "v1/gateway/connect";
constexpr const char *TOPIC_OWN_TELEMETRY = "v1/devices/me/telemetry";
constexpr const char *DEVICE_PROFILE     = "Sensor";   // profile used for auto-created sensor devices

constexpr uint32_t WIFI_RETRY_MS   = 30000;
constexpr uint32_t MQTT_RETRY_MS   = 5000;
constexpr uint32_t STATS_PERIOD_MS = 60000;
constexpr int64_t  MIN_VALID_EPOCH = 1700000000;       // anything below means SNTP has not synced yet

SPIClass loraSPI( VSPI );
SX1272 radio = new Module( LORA_NSS, LORA_DIO0, LORA_RST, RADIOLIB_NC, loraSPI );

WiFiClient net;
PubSubClient mqtt( net );

volatile bool packetReceived = false;
void IRAM_ATTR onReceive() { packetReceived = true; }

// Packets wait here while Wi-Fi / MQTT is down, so a server outage does not lose detections
struct Packet
{
    uint8_t id;
    uint8_t type;
    uint8_t counter;
    float rssi;
    float snr;
    int64_t tsMs;      // 0 = time unknown, the server stamps it on arrival
};
constexpr size_t QUEUE_SIZE = 32;
static Packet queue[QUEUE_SIZE];
static size_t qHead = 0;
static size_t qCount = 0;

static uint32_t received = 0;
static uint32_t dropped = 0;
static uint32_t lastMqttTry = 0;
static uint32_t lastWifiTry = 0;
static uint32_t lastStats = 0;
static bool timeConfigured = false;
static bool announced[256];   // sensors already announced to the gateway API since the last MQTT connect

static int64_t nowMs()
{
    struct timeval tv;
    gettimeofday( &tv, nullptr );
    if ( tv.tv_sec < MIN_VALID_EPOCH )
    {
        return 0;
    }
    return (int64_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void enqueue( const Packet &p )
{
    if ( qCount == QUEUE_SIZE )
    {
        qHead = ( qHead + 1 ) % QUEUE_SIZE;   // drop the oldest
        qCount--;
        dropped++;
    }
    queue[( qHead + qCount ) % QUEUE_SIZE] = p;
    qCount++;
}

static void connectWiFi()
{
    lastWifiTry = millis();
    WiFi.mode( WIFI_STA );
    WiFi.begin( WIFI_SSID, WIFI_PASS );
    Serial.print( "Wi-Fi" );
    uint32_t start = millis();
    while ( WiFi.status() != WL_CONNECTED && millis() - start < 20000 )
    {
        delay( 500 );
        Serial.print( "." );
    }
    if ( WiFi.status() == WL_CONNECTED )
    {
        Serial.printf( " OK, IP: %s\n", WiFi.localIP().toString().c_str() );
        if ( !timeConfigured )
        {
            configTime( 0, 0, "pool.ntp.org", "time.google.com" );   // for packet timestamps
            timeConfigured = true;
        }
    }
    else
    {
        Serial.println( " not connected (check SSID/password, 2.4 GHz only) - radio keeps working" );
    }
}

// Non-blocking in practice: short socket timeout, retried every MQTT_RETRY_MS, radio keeps receiving
static void ensureMqtt()
{
    if ( mqtt.connected() || WiFi.status() != WL_CONNECTED )
    {
        return;
    }
    if ( millis() - lastMqttTry < MQTT_RETRY_MS )
    {
        return;
    }
    lastMqttTry = millis();
    Serial.printf( "ThingsBoard %s:%u ... ", TB_HOST, (unsigned) TB_PORT );
    if ( mqtt.connect( MQTT_CLIENT_ID, TB_GATEWAY_TOKEN, nullptr ) )
    {
        Serial.println( "OK" );
        memset( announced, 0, sizeof( announced ) );
    }
    else
    {
        Serial.printf( "error, state=%d (-2 = no connection, 4/5 = bad token)\n", mqtt.state() );
    }
}

// Tells ThingsBoard that "sensor-<id>" is a device behind this gateway; creates it with profile "Sensor" if missing
static bool announceSensor( uint8_t id )
{
    if ( announced[id] )
    {
        return true;
    }
    char json[96];
    snprintf( json, sizeof( json ), "{\"device\":\"sensor-%u\",\"type\":\"%s\"}", id, DEVICE_PROFILE );
    if ( !mqtt.publish( TOPIC_GW_CONNECT, json ) )
    {
        return false;
    }
    announced[id] = true;
    return true;
}

static bool publishPacket( const Packet &p )
{
    if ( !announceSensor( p.id ) )
    {
        return false;
    }

    char values[96];
    snprintf( values, sizeof( values ), "\"typ\":%u,\"licznik\":%u,\"rssi\":%.1f,\"snr\":%.1f",
              p.type, p.counter, p.rssi, p.snr );

    char json[256];
    if ( p.tsMs > 0 )
    {
        snprintf( json, sizeof( json ), "{\"sensor-%u\":[{\"ts\":%lld,\"values\":{%s}}]}",
                  p.id, (long long) p.tsMs, values );
    }
    else
    {
        snprintf( json, sizeof( json ), "{\"sensor-%u\":[{%s}]}", p.id, values );
    }
    return mqtt.publish( TOPIC_GW_TELEMETRY, json );
}

static void flushQueue()
{
    while ( qCount > 0 && mqtt.connected() )
    {
        if ( !publishPacket( queue[qHead] ) )
        {
            Serial.println( "  -> publish failed, will retry" );
            return;
        }
        Serial.printf( "  -> sent to ThingsBoard (sensor-%u)\n", queue[qHead].id );
        qHead = ( qHead + 1 ) % QUEUE_SIZE;
        qCount--;
    }
}

static void publishGatewayStats()
{
    if ( !mqtt.connected() || millis() - lastStats < STATS_PERIOD_MS )
    {
        return;
    }
    lastStats = millis();
    char json[160];
    snprintf( json, sizeof( json ),
              "{\"odebrane\":%lu,\"odrzucone\":%lu,\"w_kolejce\":%u,\"wifi_rssi\":%d,\"uptime_s\":%lu}",
              (unsigned long) received, (unsigned long) dropped, (unsigned) qCount,
              WiFi.RSSI(), (unsigned long) ( millis() / 1000 ) );
    mqtt.publish( TOPIC_OWN_TELEMETRY, json );
}

static void handleRadio()
{
    uint8_t buf[64];
    size_t len = radio.getPacketLength();
    if ( len > sizeof( buf ) )
    {
        len = sizeof( buf );
    }
    int st = radio.readData( buf, len );
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();
    radio.startReceive();   // back to listening right away

    if ( st != RADIOLIB_ERR_NONE || len < LORA_PACKET_LEN )
    {
        Serial.printf( "Packet rejected (code %d, length %u)\n", st, (unsigned) len );
        return;
    }

    received++;
    Serial.printf( "LoRa: sensor=%u type=%u #%u | RSSI=%.1f dBm SNR=%.1f dB\n",
                   buf[0], buf[1], buf[2], rssi, snr );

    Packet p = { buf[0], buf[1], buf[2], rssi, snr, nowMs() };
    enqueue( p );
}

void setup()
{
    Serial.begin( 115200 );
    delay( 1000 );
    Serial.println( "\n=== LoPy: LoRa receiver -> Wi-Fi -> ThingsBoard ===" );

    loraSPI.begin( LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS );
    int st = radio.begin( LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR, LORA_SYNC_WORD,
                          LORA_POWER_DBM, LORA_PREAMBLE );
    if ( st != RADIOLIB_ERR_NONE )
    {
        Serial.printf( "radio.begin() error: %d (-2 = radio not found)\n", st );
        while ( true ) delay( 1000 );
    }
    radio.setPacketReceivedAction( onReceive );
    radio.startReceive();
    Serial.printf( "Radio OK, listening on %.1f MHz SF%u\n", LORA_FREQ_MHZ, (unsigned) LORA_SF );

    // Optional Wi-Fi antenna select on the LoPy (if Wi-Fi is weak): uncomment, try HIGH, then LOW.
    // pinMode( 16, OUTPUT );
    // digitalWrite( 16, HIGH );

    mqtt.setServer( TB_HOST, TB_PORT );
    mqtt.setBufferSize( 512 );
    mqtt.setKeepAlive( 30 );
    mqtt.setSocketTimeout( 3 );   // default is 15 s, which would stall radio reads when the server is down

    connectWiFi();
}

void loop()
{
    if ( WiFi.status() != WL_CONNECTED && millis() - lastWifiTry > WIFI_RETRY_MS )
    {
        connectWiFi();
    }
    ensureMqtt();
    mqtt.loop();

    if ( packetReceived )
    {
        packetReceived = false;
        handleRadio();
    }

    flushQueue();
    publishGatewayStats();
}
