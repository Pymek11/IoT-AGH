// Copy this file to secrets.h (git-ignored) and fill in the values.

#ifndef SECRETS_H
#define SECRETS_H

static const char *WIFI_SSID = "YOUR_WIFI_NAME";        // 2.4 GHz network only
static const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

static const char *TB_HOST = "79.76.56.250";            // ThingsBoard server
static const uint16_t TB_PORT = 1883;                   // MQTT port
// Access token of the ThingsBoard GATEWAY device "lopy" (Devices -> lopy -> Copy access token)
static const char *TB_GATEWAY_TOKEN = "YOUR_LOPY_ACCESS_TOKEN";

#endif // SECRETS_H
