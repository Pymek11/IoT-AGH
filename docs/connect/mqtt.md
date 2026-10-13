## Jak działa protokół MQTT?

MQTT (Message Queuing Telemetry Transport) to niezwykle lekki protokół komunikacyjny typu klient-serwer, stworzony specjalnie dla środowisk Internetu Rzeczy (IoT), gdzie przepustowość sieci jest ograniczona, a opóźnienia mogą być duże.

W przeciwieństwie do tradycyjnych zapytań HTTP, gdzie aplikacja musi stale pytać serwer , MQTT działa asynchronicznie, co pozwala na natychmiastowe przesyłanie informacji bez obciążania sieci.

### 1. Wzorzec Publikuj/Subskrybuj (Publish/Subscribe)
Architektura MQTT całkowicie oddziela nadawcę wiadomości od jej odbiorcy.
* **Broker MQTT:** Centralny serwer (np. system Mosquitto), który odbiera wszystkie wiadomości, filtruje je i rozsyła do zainteresowanych odbiorców.
* **Klient (Publisher/Nadawca):** W Twoim projekcie to Serwer Sieciowy LoRaWAN, który po odebraniu danych z lasu publikuje (Publish) pakiet informacji do Brokera.
* **Klient (Subscriber/Odbiorca):** Twoja aplikacja dla straży leśnej (np. Node-RED), która zgłasza Brokerowi chęć otrzymywania (Subscribe) konkretnych danych. Odbiorca nie musi wiedzieć, skąd fizycznie pochodzą dane, nasłuchuje jedynie wybranego kanału.

### 2. Tematy (Topics) i struktura wiadomości
Wiadomości w MQTT nie są wysyłane na adresy IP, ale na konkretne Tematy, które przypominają strukturę katalogów na dysku.
* Przykładowy temat: `las/nadlesnictwo_polnoc/czujnik_004/alarm`
* Klient może subskrybować pojedynczy temat lub użyć tzw. **Wildcards** (znaków wieloznacznych), aby nasłuchiwać szerszego zakresu danych:
  * Znak `+` zastępuje jeden poziom (np. `las/+/czujnik_004/alarm` obejmie czujnik_004 we wszystkich nadleśnictwach).
  * Znak `#` zastępuje wszystkie kolejne poziomy (np. `las/nadlesnictwo_polnoc/#` odbierze wszystko – alarmy, statusy baterii, błędy – z danego nadleśnictwa).

### 3. Poziomy jakości usług
MQTT pozwala zdecydować, jak ważna jest dana wiadomość, oferując trzy poziomy gwarancji dostarczenia:
* **QoS 0 (At most once):** "Wyślij i zapomnij". Wiadomość jest wysyłana raz i nie wymaga potwierdzenia. Przydatne dla regularnie powtarzających się mało ważnych danych (np. status baterii).
* **QoS 1 (At least once):** Gwarantuje dostarczenie wiadomości, wymagając od odbiorcy potwierdzenia odbioru. Jeśli potwierdzenie nie nadejdzie, wiadomość jest wysyłana ponownie. Najlepszy kompromis dla alarmów o wjeździe quada do lasu.
* **QoS 2 (Exactly once):** Czterostopniowy proces wymiany potwierdzeń gwarantujący, że wiadomość dotrze dokładnie jeden raz i nie zostanie zduplikowana. Używany w systemach krytycznych (np. finansowych), ale najbardziej obciąża sieć.

### 4. LWT (Last Will and Testament)
Jeśli klient (np. aplikacja nasłuchująca) nagle zerwie połączenie z Brokerem z powodu awarii zasilania, Broker automatycznie opublikuje zdefiniowaną wcześniej wiadomość informującą innych w sieci, że dany element systemu uległ awarii.

### 5. Do zrobienia na Raspberry Pi