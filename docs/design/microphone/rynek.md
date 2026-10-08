# Dostępność i Szacowane Ceny Komponentów (Rynek Polski)

Poniższe zestawienie przedstawia dostępność, formę montażową oraz orientacyjne ceny brutto dla analizowanych mikrofonów i elementów analogowego toru wybudzania.

| Komponent | Typ / Forma | Dostępność PL | Średnia cena brutto | Główne źródła zakupu |
| :--- | :--- | :--- | :--- | :--- |
| **INMP441** | Gotowy moduł cyfrowy (I2S breakout) | **Bardzo wysoka** (od ręki) | **14 – 20 PLN** | Allegro, Kamami, Botland |
| **MP23ABS1TR** | Układ scalony SMD (Analog MEMS RHLGA) | **Wysoka** (od ręki) | **8 – 12 PLN** | Kamami, TME, Mouser PL |
| **SparkFun BOB-19389** *(SPH8878LR5H-1)* | Gotowy moduł analogowy (Breakout) | **Średnia / Niska** (często na zamówienie) | **44 – 49 PLN** | Botland, Kamami |
| **TLV8542 / TLV7011** *(Tor Wake-on-Sound)* | Układy scalone SMD (Op-Amp nano-power / Komparator) | **Wysoka** (od ręki) | **4 – 8 PLN** / szt. | TME, Mouser PL |

---

## Rekomendacje Zakupowe i Wnioski

1. **Szybkie prototypowanie (Cyfrowy tor I2S):**
   * Wybór: **INMP441** (~15 PLN).
   * Zapewnia najniższy próg wejścia – nie wymaga lutowania SMD ani projektowania filtrów analogowych. Bezpośrednie podłączenie do linii I2S mikrokontrolera (ESP32-S3).

2. **Wersja docelowa pod kątem kosztu i baterii (Własne PCB / Ultra-Low Power):**
   * Wybór: **MP23ABS1TR** (~8–10 PLN) + **TLV8542** (~5 PLN).
   * Całkowity koszt komponentów toru akustycznego zamyka się w granicach 15–20 PLN, zapewniając jednocześnie pobór prądu rzędu ~125 µA w trybie stałego nasłuchu.

3. **Moduł SparkFun BOB-19389:**
   * Dobry do wstępnych testów analogowych, jednak relatywnie drogi (~45 PLN) w przeliczeniu na pojedynczy węzeł pomiarowy.