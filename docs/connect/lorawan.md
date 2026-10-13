## Protokół LoRaWAN

LoRaWAN to protokół sieciowy (warstwa MAC) zbudowany na szczycie technologii modulacji radiowej LoRa (warstwa fizyczna). W kontekście monitoringu lasu, jego działanie opiera się na kilku kluczowych mechanizmach, które pozwalają na komunikację na duże odległości przy minimalnym zużyciu prądu.

### 1. Topologia "Gwiazda gwiazd" (Star-of-Stars)
W przeciwieństwie do sieci Wi-Fi Mesh czy Zigbee, czujniki w lesie (End Nodes) nie komunikują się między sobą. Każdy czujnik po prostu nadaje sygnał radiowy w eter. Sygnał ten może zostać odebrany przez jedną lub kilka bram (Gateways) jednocześnie. Bramy nie analizują danych – działają jak przezroczyste przekaźniki, które pakują odebrany sygnał w pakiety TCP/IP i przesyłają do centralnego Serwera Sieciowego (Network Server). Serwer ten odrzuca duplikaty (jeśli kilka bram odebrało ten sam sygnał) i kieruje dane dalej.

### 2. Klasy urządzeń (Zarządzanie energią)
LoRaWAN dzieli urządzenia na trzy klasy. W Twoim projekcie detekcji pojazdów czujniki będą pracować wyłącznie w Klasie A:
* **Klasa A (Bateryjna):** Urządzenie przez 99% czasu pozostaje w trybie głębokiego uśpienia. Budzi się i włącza moduł radiowy tylko z własnej inicjatywy (np. gdy mikrofon wykryje dźwięk silnika). Bezpośrednio po nadaniu alarmu, czujnik otwiera dwa krótkie okna odbiorcze (RX1 i RX2), aby sprawdzić, czy serwer ma dla niego jakieś instrukcje (np. zmianę czułości mikrofonu). Jeśli serwer milczy, układ ponownie zasypia.
* **Klasa B i C:** Oferują zaplanowane (B) lub ciągłe (C) okna nasłuchu, ale zużywają znacznie więcej energii. Klasa C wymaga stałego zasilania z sieci elektrycznej i jest stosowana zazwyczaj w inteligentnych latarniach ulicznych czy zaworach, a nie w leśnych czujnikach.

### 3. Zabezpieczenia i podwójne szyfrowanie
LoRaWAN jest bezpieczny na poziomie samej architektury, wykorzystując 128-bitowe szyfrowanie AES na dwóch niezależnych płaszczyznach:
* **Klucz Sesyjny Sieci (NwkSKey):** Służy do autoryzacji urządzenia w sieci. Serwer Sieciowy używa go do weryfikacji, czy sygnał na pewno pochodzi od Twojego czujnika, odrzucając próby zakłócania lub podszywania się (tzw. spoofing).
* **Klucz Sesyjny Aplikacji (AppSKey):** Służy do szyfrowania samego "ładunku" wiadomości (payload), czyli np. informacji "wykryto pojazd, 80% baterii". Serwer Sieciowy nie ma możliwości odczytania tych danych – rozszyfrowuje je dopiero Twoja ostateczna aplikacja (np. na serwerze z MQTT). To tzw. szyfrowanie End-to-End.

### 4. Adaptive Data Rate (ADR)
To mechanizm optymalizacji baterii i pasma. Serwer Sieciowy stale analizuje jakość sygnału docierającego z czujników. Jeśli dany czujnik jest zamontowany na skraju lasu i ma świetny zasięg do bramy, serwer wyśle mu komendę, aby ten zmniejszył zużywaną moc. Jeśli z kolei czujnik znajduje się w mokrym, gęstym młodniku, serwer przydzieli mu wyższą moc nadawania (Spreading Factor), aby sygnał zdołał dotrwać mimo przeszkód, co jednak kosztuje więcej energii z baterii.