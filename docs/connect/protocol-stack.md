# Dokumentacja Architektury Sieciowej Systemu Detekcji Pojazdów ("IoT Forest Monitor")

## 1. Wstęp
Niniejszy dokument opisuje architekturę sieciową i stos technologiczny systemu Internetu Rzeczy (IoT) przeznaczonego do autonomicznego monitorowania terenów leśnych. Celem systemu jest detekcja nieautoryzowanych pojazdów silnikowych (quady, motocykle crossowe, samochody terenowe) za pomocą rozproszonej sieci czujników akustycznych/sejsmicznych. Z uwagi na brak infrastruktury zasilającej oraz silne tłumienie sygnału radiowego przez zalesienie, architektura opiera się na rozdzieleniu warstwy energooszczędnej transmisji radiowej (LoRaWAN) od warstwy aplikacji (MQTT).

## 2. Stos Technologiczny

Architektura systemu została podzielona na trzy główne domeny komunikacyjne, co gwarantuje maksymalną żywotność baterii w węzłach brzegowych oraz elastyczność integracyjną po stronie serwera.

### 2.1. Warstwa Fizyczna i Łącza danych
Warstwa odpowiedzialna za fizyczną transmisję danych w trudnym terenie bez znajomości protokołów wyższego rzędu (IP).

*   **Warstwa Fizyczna (PHY) - LoRa:** Wykorzystuje modulację z widmem rozproszonym (CSS - Chirp Spread Spectrum) w nielicencjonowanym paśmie Sub-GHz (868 MHz dla Europy). Pozwala to na głęboką penetrację sygnału przez gęstą roślinność leśną na dystansie od 2 do 5 km w zależności od ukształtowania terenu i wysokości montażu anteny odbiorczej.
*   **Warstwa Łącza Danych (MAC) - LoRaWAN:** Protokół organizujący topologię sieci typu Star-of-Stars. 
    *   **Klasa urządzeń:** Węzły sensoryczne pracują wyłącznie w energooszczędnej **Klasie A**. Moduł radiowy pozostaje w trybie głębokiego uśpienia i jest wybudzany przerwaniem sprzętowym tylko w momencie detekcji anomalii.
    *   **Bezpieczeństwo:** Zastosowano podwójne szyfrowanie AES-128 na poziomie sprzętowym (NwkSKey do autoryzacji w sieci oraz AppSKey do szyfrowania ładunku użytecznego/payloadu).

### 2.2. Warstwa Sieciowa i Transportowa
Warstwa pośrednicząca między sprzętem radiowym a serwerami chmurowymi. Brama (LoRaWAN Gateway) działa tu jako most, który hermetyzuje ramki radiowe do formatu zrozumiałego dla internetu.

*   **Warstwa Sieciowa - IP (Warstwa 3 OSI):** Odpowiada za adresację i routing pakietów z bramy rozmieszczonej w lesie do centralnego serwera w internecie (korzystając z połączenia LTE/4G lub Ethernet).
*   **Warstwa Transportowa - UDP / TCP (Warstwa 4 OSI):** Odpowiada za sam transfer danych. Protokół UDP jest często wykorzystywany przez oprogramowanie bramy (Packet Forwarder) ze względu na mniejszy narzut (szybkie przesłanie odebranej ramki do serwera), natomiast TCP stosuje się tam, gdzie wymagana jest gwarancja dostarczenia (np. przy bardziej zaawansowanych protokołach bram jak Basic Station).

### 2.3. Warstwa Aplikacji
Warstwa działająca w całości w infrastrukturze serwerowej (np. Raspberry Pi, VPS lub chmura TTN), zarządzająca odszyfrowanymi danymi i logiką alertowania.

*   **LoRaWAN Network Server (LNS):** Odbiera pakiety IP od bram, deduplikuje wiadomości, weryfikuje klucze sieciowe (NwkSKey) i deszyfruje ładunek użyteczny (AppSKey). Następnie tłumaczy surowe bajty na struktury danych (np. JSON).
*   **Protokół MQTT (Message Queuing Telemetry Transport):** Asynchroniczny protokół publikuj/subskrybuj pełniący rolę szyny danych (Message Broker, np. Eclipse Mosquitto). LNS **publikuje** rozkodowane JSON-y na zdefiniowanych tematach (np. `las/sektor_A/czujnik_004/alarm`).
*   **Aplikacja Końcowa (np. Node-RED):** Działa jako klient MQTT. **Subskrybuje** tematy z alarmami. Zdarzenie pojawienia się nowej wiadomości na brokerze wyzwala przepływ (Flow), który przetwarza zdarzenie i integruje się z zewnętrznymi API (np. bramka SMS/Push) w celu powiadomienia służb leśnych.

## 3. Schemat Przepływu Danych

1.  **Sensor:** Algorytm  wykrywa wzorzec dźwięku quada.
2.  **Sensor:** Mikrokontroler generuje ramkę LoRaWAN (np. 3 bajty) i emituje sygnał LoRa (868 MHz).
3.  **Gateway:** Odbiór ramki radiowej i przekierowanie jej przez sieć komórkową (UDP/TCP) do Network Servera.
4.  **Network Server:** Deszyfrowanie payloadu, formowanie obiektu JSON.
5.  **Network Server -> Broker MQTT:** Publikacja JSON-a na temacie alarmowym (QoS 1).
6.  **Aplikacja / Node-RED:** Odbiór subskrybowanej wiadomości z Brokera MQTT.
7.  **Aplikacja -> Użytkownik:** Wyzwolenie akcji i wysłanie wiadomości SMS do przypisanego strażnika leśnego.