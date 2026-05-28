# Sklenik IoT — MISA

Semestralne zadanie MISA — FEI STU Bratislava

## Architektura systemu

```
[ESP32 - Sklenik]  ---LoRa 868MHz---  [ESP8266 - Dom]  ---WiFi/MQTT---  [Raspberry Pi 3B]
  BMP280 (T, P)       DX-LR02 x2       gateway uzol        Mosquitto       Home Assistant
  senzor pody                           MQTT klient         broker          dashboard
  fotoodpor
  cerpadlo 12V
  bateria 12V 7Ah
```

Sklenikovy modul (ESP32) sa prebudi zo spánku (deep sleep), zmeria vsetky veliciny, zapne cerpadlo,
ak je potrebne a odosle sifrovany LoRa paket. Vnutorna jednotka (ESP8266) paket prijme,
overi checksum, dessifruje a publikuje jednotlive veliciny na MQTT brokera. Home Assistant
na RPi vizualizuje data a uklada historiu.

## Hardver

| Komponent | Popis | Uzol |
|-----------|-------|------|
| ESP32 DevKit | MCU sklenikoveho uzla | sklenik |
| ESP8266 NodeMCU | MCU vnutornej jednotky | dom |
| DX-LR02 900T22D (x2) | LoRa moduly, transparentny UART rezim, 868 MHz | oba |
| BMP280 | Teplota (-40..85°C) a tlak (300..1100 hPa), I2C | sklenik |
| Kapacitny senzor vlhkosti pody | Analogovy, 12-bit ADC, kalibrovany | sklenik |
| Fotoodpor + delit napatia | Intenzita svetla, analogovy, 12-bit ADC, kalibrovany | sklenik |
| Vodne cerpadlo 12V | Automaticke polievanie | sklenik |
| Rele / MOSFET | Spinanie cerpadla z GPIO25 | sklenik |
| 12V 7Ah bateria | Napajanie sklenikoveho uzla | sklenik |
| DC-DC menic 12V→3.3V | Napajanie ESP32 | sklenik |
| Raspberry Pi 3B | MQTT broker + Home Assistant | server |

## Senzory a kalibrácia

### BMP280 (kategoria A — digitálny, I2C)
- Teplota: rozsah -40 až 85 °C, offset korekcia -2.7 °C (merany rozdiel voči referencnemu teplomeru)
- Tlak: rozsah 300 až 1100 hPa
- Rezim: forced mode — jedno meranie, potom senzor zaspi 

### Kapacitny senzor vlhkosti pody (kategoria B — analogovy, ADC, kalibracia)
- Kalibracne body: sucha poda ADC=2212 → 0 %, mokra poda ADC=1372 → 100 %
- Napajanie: GPIO13, zapinate len pocas merania (150 ms)
- Priemer z 10 vzoriek (50 ms rozostup)

### Fotoodpor (kategoria B — analogovy, ADC, kalibracia)
- Kalibracne body: tma ADC=0 → 0 %, plne svetlo ADC=2968 → 100 %
- Napajanie: GPIO14, zapinate len pocas merania
- Pouzity na nocnu ochranu cerpadla (pod 20 % sa nepolieva)

## MQTT Topiky

Vsetky topiky su v hierarchii `fei/<uzol>/<velicina>`. Spravy su publikovane s `retain=true`
(okrem logu). ESP8266 sa prihlasuje pod uzivatelom `mqtt_iot`, Home Assistant pod `homeassistant`.

| Topik | Velicina | Jednotka | Senzor | retain |
|-------|----------|----------|--------|--------|
| `fei/sklenik/temperature` | Teplota vzduchu | °C | BMP280 | true |
| `fei/sklenik/pressure` | Atmosfericky tlak | hPa | BMP280 | true |
| `fei/sklenik/soil_moisture` | Vlhkost pody | % | kapacitny senzor | true |
| `fei/sklenik/light` | Intenzita svetla | % | fotoodpor | true |
| `fei/sklenik/pump` | Stav cerpadla | 0/1 | aktuator | true |
| `fei/sklenik/status` | Stav uzla (LWT) | online/offline | — | true |
| `fei/sklenik/log` | Debug log sprava | text | ESP8266 | false |

## JSON format správ

Vsetky merania su publikovane v tomto formate:

```json
{
  "value": 23.45,
  "unit": "°C",
  "ts": 1748000000,
  "node": "sklenik",
  "sensor": "BMP280"
}
```

| Pole | Typ | Popis |
|------|-----|-------|
| `value` | float | Namerena hodnota |
| `unit` | string | Fyzikalna jednotka |
| `ts` | uint32 | Unix timestamp (NTP, UTC+1+DST) |
| `node` | string | Identifikator uzla |
| `sensor` | string | Identifikator senzora |

Log sprava (`fei/sklenik/log`):
```json
{"msg": "paket #42 T=23.5C P=1013hPa poda=61% svetlo=78% pumpa=0 chyby=0x00 reset=8", "ts": 1748000000, "node": "dom"}
```

## Perioda merania — odovodnenie (kritérium 3.2)

ESP32 sa prebudi raz za **1 hodinu** (`SLEEP_INTERVAL_US = 3 600 000 000 µs`). Tato perioda je
zvolena z nasledujucich dovodov:

- **BMP280**: teplota a tlak sa v skleniku menia pomaly (tepelna casova konstanta radu
  minut az desiatok minut). 1 hodina postacuje na zachytenie vsetkych relevantnych trendov
  v uzavrenom skleniku.
- **Vlhkost pody**: kapacitny senzor reaguje pomaly — voda sa vsaka do substratu v radoch
  minut az hodin. Zmena o 1 % trva typicky hodiny, meranie raz za hodinu je dostacujuce.
- **Bateria**: deep sleep spotreba ESP32 je ~10 µA, DC-DC konvertor ~3 mA. Pri 1-hodinovej
  periode je uzol aktivny ~5 s a spi 3595 s. Priemerny odber z 12V baterie ~3.1 mA,
  zivotnost 7Ah baterie ~75 dni (~2.5 mesiaca).
- **LoRa moduly**: DX-LR02 v transparentnom rezime, paket 23 bajtov,
  9600 baud. Kazdy cyklus posiela paket 5-krat (ochrana pred RF chybami), aby sme mali istotu, 
  ze vnutorna jednotka zachyti vsetky data.
  Perioda 1 hodina.

## Nahranie firmveru

### Poziadavky

- Arduino IDE 2.x
- Nainstalovane dosky: **ESP32 by Espressif** a **ESP8266 by ESP8266 Community** (Board Manager)
- Kniznice (Library Manager): `Adafruit BMP280`, `Adafruit Unified Sensor`, `PubSubClient`

### ESP32 — sklenikovy uzol

1. Otvorit `firmware/sklenik/sklenik.ino` v Arduino IDE
2. Skontrolovat ze `firmware/sklenik/config.h` je v tom istom priecinku
3. Nastavit v `dom.ino` svoje WiFi a MQTT udaje (ak sa menia)
4. Board: **ESP32 Dev Module**, Port: COM/ttyUSB priradeny ESP32
5. Upload (Ctrl+U)
6. Serial Monitor (115200 baud) — overit vystup `=== ESP32 prebudenie #1 ===`

### ESP8266 — vnutorna jednotka (dom)

1. Otvorit `firmware/dom/dom.ino` v Arduino IDE
2. Skontrolovat ze `firmware/dom/config.h` je v tom istom priecinku
3. Doplnit realne hodnoty `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_USER`, `MQTT_PASSWORD`
4. Board: **NodeMCU 1.0 (ESP-12E Module)**, Port: COM/ttyUSB priradeny ESP8266
5. Upload
6. Serial Monitor (115200 baud) — overit vystup `=== ESP8266 - Vnutorna jednotka ===`

## Instalacny navod

Pozrite `server/SETUP.md` — kompletny postup od cistej instalacie OS po spusteny dashboard.

Strucny prehlad:
1. Naflashovat **Home Assistant OS** na SD kartu pomocou Raspberry Pi Imager
2. Zapnut RPi — HA dostupny na `http://homeassistant.local:8123`
3. Nainstaovat **Mosquitto broker** add-on (Settings → Add-ons), pridat login `mqtt_iot`
4. Nainstaovat **File Editor** add-on, upravit `/config/configuration.yaml` podla `server/home-assistant/configuration.yaml`
5. Settings → System → Restart (Core)
6. HA Recorder automaticky uklada vsetky entity (SQLite, predvolene 10 dni)

## Pristup na dashboard

- **Lokalna siet**: `http://192.168.1.228:8123`
- **Verejny pristup**: pomocou Tailscale VPN

## Struktura repozitara

```
vypracovanie/
  firmware/
    sklenik/          # ESP32 — sklenikovy uzol
      sklenik.ino
      config.h        # spolocna konfiguracia (topics, struct, klic)
    dom/              # ESP8266 — vnutorna jednotka
      dom.ino
      config.h        # identicky subor ako sklenik/config.h
  server/
    mosquitto/
      mosquitto.conf  # konfiguracia brokera (bez hesiel)
      acl             # opravnenia uzlov
    home-assistant/
      configuration.yaml   # MQTT senzory pre HA
    SETUP.md          # kompletny instalacny navod
  docs/
    wiring_sklenik.md # schema zapojenia ESP32 uzla
    wiring_dom.md     # schema zapojenia ESP8266 uzla
  README.md
```
