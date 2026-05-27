# Sklenik IoT

Distribuovany system monitorovania sklenika pomocou LoRa komunikacie a Home Assistant.

## Architektura systemu

```
[ESP32 - Sklenik]  ---LoRa---  [ESP8266 - Vnutorna jednotka]  ---MQTT---  [Home Assistant na RPi 3B]
  - senzory                       - prijima data                             - vizualizacia
  - aktuatory                     - preposielanie na HA                      - dashboard
  - bateriove napajanie
```

## Pouzity hardver

| Komponent | Popis |
|-----------|-------|
| ESP32 | Hlavny uzol sklenika (senzory + aktuatory) |
| ESP8266 | Vnutorna prijimacia jednotka |
| DX-LR02 900T22D (x2) | LoRa moduly, transparentny rezim |
| Raspberry Pi 3B | Server - bezi Home Assistant |
| 12V 7Ah bateria | Napajanie sklenikoveho modulu |
| DC-DC menic | Konverzia 12V -> 3.3V pre ESP32 |

## Senzory

*(bude doplnene)*

## MQTT topiky

| Topik | Velicina | Jednotka |
|-------|----------|----------|
| `sklenik/temperature` | Teplota | °C |
| `sklenik/humidity` | Relativna vlhkost | % |

*(bude doplnene podla finalneho zoznamu senzorov)*

## JSON format sprav

```json
{
  "value": 23.4,
  "unit": "C",
  "ts": 1714000000,
  "node": "sklenik",
  "sensor": "DHT22"
}
```

## Instalacia a spustenie

*(bude doplnene)*

## Pristup na dashboard

Home Assistant bezi na `http://<IP-adresa-RPi>:8123`

---
*Semestralne zadanie - MISA - FEI*
