## Czujnik drgań

Drgania to ruch oscylacyjny, a akcelerometr mierzy przyspieszenie, które zmienia się z ich częstotliwością, więc z jego przebiegu odczytujemy zarówno siłę, jak i widmo drgań. Akcelerometry MEMS są tanie, małe, energooszczędne i mają cyfrowe wyjście z wbudowanym wykrywaniem ruchu, więc łatwo je podłączyć do ESP32 w urządzeniu na baterii, bez osobnego toru analogowego jak przy geofonie.

### LIS3DH (STMicroelectronics)

Jako czujnik drgań wybraliśmy moduł z trójosiowym akcelerometrem MEMS LIS3DH firmy STMicroelectronics. Układ mierzy przyspieszenie w zakresach ±2, ±4, ±8 i ±16 g z częstotliwością próbkowania od 1 Hz do 5,3 kHz. To z dużym zapasem obejmuje niskie częstotliwości drgań gruntu przy przejeździe pojazdu. Komunikuje się przez I2C lub SPI i działa przy zasilaniu 1,71–3,6 V, więc podłączamy go bezpośrednio do ESP32 i jego linii 3,3 V.

W trybie niskiego poboru układ pobiera od 2 µA, a dwa programowalne generatory przerwań wykrywają ruch bez udziału procesora – akcelerometr wybudza go dopiero przy drganiach. Wbudowana 32-poziomowa kolejka FIFO zbiera próbki bez ciągłego odczytu. Zakres pracy od −40 do +85 °C pozwala na użycie w lesie przez cały rok, a moduł kosztuje około 26 zł.

Akcelerometr MEMS jest mniej czuły niż geofon, więc wykryje drgania tylko w niewielkiej odległości od toru przejazdu.

### Alternatywy

#### ADXL345 (Analog Devices)

Trójosiowy akcelerometr MEMS, funkcjonalnie bardzo bliski LIS3DH: zakresy ±2–±16 g, rozdzielczość do 13 bitów (4 mg/LSB), I2C i SPI, zasilanie 2,0–3,6 V. Pobiera ok. 23 µA w trybie pomiaru i 0,1 µA w uśpieniu. Ma przerwania aktywności i bezczynności, wykrywanie stuknięć i spadku swobodnego oraz FIFO. Nadaje się na zamiennik LIS3DH, gdyby ten był niedostępny albo sprawiał problemy. Jest trochę gorszy, a jego jedyny plus względem LIS3DH to, że kosztuje 10 zł mniej.

#### MPU-6050 (TDK InvenSense)

Moduł 6-osiowy: akcelerometr ±2–±16 g i żyroskop ±250–±2000 °/s, przetwornik 16-bitowy, tylko I2C, zasilanie 2,375–3,46 V, FIFO 1024 B i przerwanie wykrycia ruchu. Do wykrywania drgań według naszych informacji żyroskop jest zbędny, a z nim układ pobiera ok. 3,8 mA. W trybie niskiego poboru sam akcelerometr pobiera 10–140 µA, ale próbkuje wtedy tylko z częstotliwością 1–40 Hz. Jest tani i ma dużo bibliotek, ale przy zasilaniu bateryjnym przegrywa z LIS3DH, więc odrzucamy go jako czujnik główny.

#### Geofon SM-24

Czujnik sejsmiczny, w którym cewka porusza się w polu magnesu i generuje napięcie proporcjonalne do prędkości drgań gruntu. Ma czułość 28,8 V/(m/s), częstotliwość własną 10 Hz, pasmo 10–240 Hz, rezystancję cewki 375 Ω i zakres pracy od −40 do +100 °C. Sam pomiar nie wymaga zasilania, ale sygnał trzeba wzmocnić i podać na przetwornik ADC, a czujnik musi być dobrze sprzężony z gruntem. To najczulsza z tych opcji i standard w sejsmice, więc daje szansę na większy zasięg wykrywania. Kosztuje jednak 400 zł oraz wymaga własnego toru analogowego. Traktujemy go jako rozszerzenie, gdyby pomiary pokazały, że LIS3DH ma za mały zasięg.

### Porównanie

| Moduł | Typ | Interfejs | Pobór prądu | Rola w projekcie |
|---|---|---|---|---|
| LIS3DH | Akcelerometr MEMS, 3 osie | I2C / SPI | od 2 µA | Czujnik główny |
| ADXL345 | Akcelerometr MEMS, 3 osie | I2C / SPI | ok. 23 µA (pomiar) | Zamiennik |
| MPU-6050 | Akcelerometr + żyroskop | I2C | 3,8 mA z żyroskopem, 10–140 µA sam akcelerometr | Odrzucony |
| SM-24 | Geofon (pasywny) | Analogowy, wymaga wzmacniacza i ADC | Brak dla samego czujnika | Rozszerzenie przy zbyt małym zasięgu |
