# IoT-AGH

## Gałąź `feat/mic-vib-triggered`: nagrywanie wyzwalane czujnikiem drgań 801S

Płytka czeka bezczynnie, aż 801S wykryje drgania. Wtedy nagrywa i wysyła audio razem ze stanem 801S.
Każde kolejne wykrycie (albo pin trzymany w stanie aktywnym) przedłuża nagranie. Nagranie kończy się
po `TRIG_HOLD_MS` bez drgań. Każde nagranie trafia do osobnego WAV i CSV.
Komenda `START` uzbraja płytkę, a `STOP` ją rozbraja (`rec_capture.py` wysyła je sam).
Wersja z ciągłym nagrywaniem jest na gałęzi `feat/mic-continuous-vib`.

### Jak działa wyzwalanie

- **Filtr szpilek:** nagranie startuje dopiero po `TRIG_PULSES` impulsach (przejściach 801S do stanu
  aktywnego) w oknie `TRIG_WINDOW_MS`. Wartość 1 oznacza start od pierwszego impulsu.
  Do testów jest teraz włączone `TRIG_ON_LEVEL 1`: po uzbrojeniu nagranie startuje przy pierwszym
  stanie HIGH 801S. Docelowo ustaw `TRIG_ON_LEVEL 0` i np. `TRIG_PULSES 3`.
- **Pre-trigger (domyślnie):** I2S działa cały czas i zapisuje ostatnie `TRIG_PRE_MS` (1 s, zaokrąglone do
  bloków, czyli 1,02 s) do bufora kołowego. Po wyzwoleniu ten bufor jest wysyłany jako początek nagrania,
  więc początek przejazdu nie jest ucinany.
- **I2S na żądanie** (`TRIG_I2S_ON_DEMAND 1`, środowisko `esp32dev-rec-ondemand`): mikrofon włącza się
  dopiero przy wyzwoleniu. Pobiera mniej prądu, ale nie ma dźwięku sprzed wyzwolenia, a pierwsze ~0,1–0,2 s
  to rozruch INMP441 (cisza albo szum). Zdarzenia 801S sprzed startu I2S trafiają do próbki 0.
- W CSV moment wyzwolenia ma w kolumnie `note` wartość `trigger`. Pierwszy wiersz (`start`) podaje długość pre-bufora.

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

Wszystkie parametry są w `Source/config.h`:

| `#define` | Domyślnie | Znaczenie |
|---|---|---|
| `MICR_SD_PIN` / `MICR_WS_PIN` / `MICR_SCK_PIN` | 19 / 4 / 5 | piny I2S |
| `VIB_PIN` | 23 | wyjście DO czujnika 801S |
| `VIB_ACTIVE_HIGH` | 1 | 1 = drgania to stan HIGH |
| `VIB_DEBOUNCE_US` | 2000 | zbocza bliżej niż 2 ms od poprzedniego są pomijane |
| `VIB_LED_PIN` / `VIB_LED_HOLD_MS` | 2 / 100 | dioda drgań |
| `REC_PCM_SHIFT` | 8 | wzmocnienie przy konwersji 24 → 16 bit (o 1 mniej = +6 dB) |
| `TRIG_PULSES` / `TRIG_WINDOW_MS` | 1 / 500 | filtr szpilek (N impulsów w T ms), docelowo np. 3 / 500 |
| `TRIG_ON_LEVEL` | 1 (test) | 1 = sam stan HIGH po uzbrojeniu startuje nagranie, bez filtra szpilek |
| `TRIG_HOLD_MS` | 5000 | koniec nagrania po tylu ms bez drgań (X) |
| `TRIG_PRE_MS` | 1000 | długość pre-bufora |
| `TRIG_I2S_ON_DEMAND` | 0 | 1 = I2S włączany dopiero na trigger |
| `TRIG_ARMED_AT_BOOT` | 0 | 1 = płytka uzbrojona od startu, bez `START` |

Częstotliwość próbkowania (12 kHz) jest w `modules/microphone/mic.h`, tak jak na gałęzi mikrofonu.

### Wgrywanie (PlatformIO, folder `Source/`)

| Środowisko | Co robi | Komenda |
|---|---|---|
| `esp32dev-rec` (domyślne) | nagrywanie wyzwalane z pre-buforem, 921600 baud | `pio run -e esp32dev-rec -t upload` |
| `esp32dev-rec-ondemand` | nagrywanie wyzwalane, I2S włączany na trigger | `pio run -e esp32dev-rec-ondemand -t upload` |
| `esp32dev-vibdiag` | diagnostyka 801S: tekst co 250 ms, bez mikrofonu | `pio run -e esp32dev-vibdiag -t upload`, a potem `pio device monitor -e esp32dev-vibdiag` |
| `esp32dev` | dotychczasowy detektor silnika | `pio run -e esp32dev -t upload` |

Przed wgrywaniem i uruchomieniem skryptu zamknij monitor portu szeregowego.

### Nagrywanie na PC

```
cd Source/external_tools/recorder
pip install -r requirements.txt
python rec_capture.py -o recordings    # uzbraja płytkę, zapisuje każde nagranie, Ctrl+C kończy sesję
python rec_capture.py COM5 -s 600 -o recordings   # sesja trwająca 10 minut
python plot_rec.py recordings/rec_20261107_120000.wav   # audio + stan 801S na jednym wykresie
```

Pliki wynikowe:
- `rec_YYYYMMDD_HHMMSS.wav`: 16-bit PCM, mono, 12 kHz (częstotliwość jak w `mic.h` na gałęzi mikrofonu)
- `rec_YYYYMMDD_HHMMSS.csv`: kolumny `sample_index, t_s, state, note`, po jednym wierszu na każdą zmianę stanu 801S
  (`state` to poziom pinu, 1 = HIGH; `note`: `start`, `edge`, `polled`, `trigger edge`, `end`)
- `rec_YYYYMMDD_HHMMSS_blocks.csv`: stan 801S, szczyt i liczba przesterowanych próbek dla każdego bloku audio (1024 próbki)

Po nagraniu skrypt wypisuje poziomy i ostrzega o przesterowaniu albo zbyt cichym nagraniu.
Wzmocnienie ustawia `REC_PCM_SHIFT` w `config.h`: o 1 mniej daje +6 dB.

### Protokół (`modules/recorder/rec_protocol.h`)

Ramka: `A5 5A | typ u8 | długość u16 | dane | crc16`. Typy ramek: `START` (0x10), `STOP` (0x11),
`ARMED` (0x12, odpowiedź na komendy START/STOP),
`AUDIO` (0x20, indeks pierwszej próbki + stan 801S + próbki), `VIB` (0x30, indeks próbki + stan).
Logi tekstowe mogą pojawiać się między ramkami, a skrypt je pomija.
Indeks próbki przy zdarzeniu 801S ma dokładność rzędu kilku ms (jeden bufor DMA I2S).
