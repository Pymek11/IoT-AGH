# IoT-AGH

## Gałąź `feat/mic-continuous-vib`: ciągłe nagrywanie mikrofonu i stanu 801S

Firmware nagrywa mikrofon bez przerwy i loguje stan czujnika drgań 801S zsynchronizowany z audio.
Nagrywanie uruchamia się i zatrzymuje komendą `START` / `STOP` po USB Serial. Skrypt na PC zapisuje WAV i CSV.

### Podłączenie (ESP32 DevKit V1, etykiety na płytce = numery GPIO)

| Element | Pin modułu | ESP32 |
|---|---|---|
| INMP441 | SD | D19 (GPIO19) |
| INMP441 | WS | D4 (GPIO4) |
| INMP441 | SCK | D5 (GPIO5) |
| INMP441 | L/R | GND (lewy kanał) |
| INMP441 | VDD / GND | 3V3 / GND |
| 801S | DO | D23 (GPIO23), odczyt tylko cyfrowy (brak ADC na tym pinie) |
| LED drgań | niebieska dioda na płytce | GPIO2 (`VIB_LED_PIN`) |

Uwagi:
- Niebieska dioda świeci, gdy 801S zgłasza drgania, i gaśnie 100 ms po ostatnim impulsie
  (`VIB_LED_HOLD_MS`). Służy do strojenia potencjometru i działa też bez nagrywania. Najwygodniej
  stroić ją w środowisku `esp32dev-vibdiag`.
- SD mikrofonu jest na GPIO19, a nie na GPIO2: GPIO2 to pin strapping z diodą, która obciążałaby linię danych.
- 801S zasilaj z 3V3. Przy zasilaniu 5V na wyjściu DO pojawi się 5V, czego GPIO ESP32 nie toleruje.

Wszystkie parametry (piny, polaryzacja, debounce, wzmocnienie, LED) są w `Source/config.h`.

### Wgrywanie (PlatformIO, folder `Source/`)

| Środowisko | Co robi | Komenda |
|---|---|---|
| `esp32dev-rec` (domyślne) | nagrywanie, 921600 baud | `pio run -e esp32dev-rec -t upload` |
| `esp32dev-vibdiag` | diagnostyka 801S: tekst co 250 ms, bez mikrofonu | `pio run -e esp32dev-vibdiag -t upload`, a potem `pio device monitor -e esp32dev-vibdiag` |
| `esp32dev` | dotychczasowy detektor silnika | `pio run -e esp32dev -t upload` |

Przed wgrywaniem i uruchomieniem skryptu zamknij monitor portu szeregowego.

### Nagrywanie na PC

```
cd Source/external_tools/recorder
pip install -r requirements.txt
python rec_capture.py                  # port wykrywany automatycznie, Ctrl+C kończy
python rec_capture.py COM5 -s 30 -o recordings
python plot_rec.py recordings/rec_20261107_120000.wav   # audio + stan 801S na jednym wykresie
```

Pliki wynikowe:
- `rec_YYYYMMDD_HHMMSS.wav`: 16-bit PCM, mono, 12 kHz (częstotliwość jak w `mic.h` na gałęzi mikrofonu)
- `rec_YYYYMMDD_HHMMSS.csv`: kolumny `sample_index, t_s, state, note`, po jednym wierszu na każdą zmianę stanu 801S
  (`state` to poziom pinu, 1 = HIGH; `note`: `start`, `edge`, `polled`, `end`)
- `rec_YYYYMMDD_HHMMSS_blocks.csv`: stan 801S, szczyt i liczba przesterowanych próbek dla każdego bloku audio (1024 próbki)

Po nagraniu skrypt wypisuje poziomy i ostrzega o przesterowaniu albo zbyt cichym nagraniu.
Wzmocnienie ustawia `REC_PCM_SHIFT` w `config.h`: o 1 mniej daje +6 dB.

### Protokół (`modules/recorder/rec_protocol.h`)

Ramka: `A5 5A | typ u8 | długość u16 | dane | crc16`. Typy ramek: `START` (0x10), `STOP` (0x11),
`AUDIO` (0x20, indeks pierwszej próbki + stan 801S + próbki), `VIB` (0x30, indeks próbki + stan).
Logi tekstowe mogą pojawiać się między ramkami, a skrypt je pomija.
Indeks próbki przy zdarzeniu 801S ma dokładność rzędu kilku ms (jeden bufor DMA I2S).
