# MQTT Topiky — sklenik-iot

Vsetky topiky su publikovane ESP8266 (dom) na MQTT brokera (Raspberry Pi 3B, Mosquitto).
Hierarcia: `fei/<uzol>/<velicina>`

## Telemetria

| Topik | Velicina | Jednotka | Typ | Senzor | retain |
|-------|----------|----------|-----|--------|--------|
| `fei/sklenik/temperature` | Teplota vzduchu | °C | float | BMP280 | true |
| `fei/sklenik/pressure` | Atmosfericky tlak | hPa | float | BMP280 | true |
| `fei/sklenik/soil_moisture` | Vlhkost pody | % | int (0–100) | kapacitny senzor | true |
| `fei/sklenik/light` | Intenzita svetla | % | int (0–100) | fotoodpor | true |
| `fei/sklenik/pump` | Stav cerpadla | 0 / 1 | int | aktuator | true |

## Stav uzla

| Topik | Velicina | Hodnoty | retain |
|-------|----------|---------|--------|
| `fei/sklenik/status` | LWT stav uzla | `online` / `offline` | true |

## Ladenie

| Topik | Velicina | retain |
|-------|----------|--------|
| `fei/sklenik/log` | Debug sprava z dom.ino | false |

## Format spravy (JSON)

Vsetky telemetricke topiky pouzivaju tento format:

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
| `node` | string | Identifikator uzla (`sklenik`) |
| `sensor` | string | Identifikator senzora |

Log sprava (`fei/sklenik/log`):
```json
{"msg": "paket #42 T=23.5C P=1013hPa poda=61% svetlo=78% pumpa=0 chyby=0x00 reset=8", "ts": 1748000000, "node": "dom"}
```

## Opravnenia (ACL)

| Uzivatel | Opravnenie | Topiky |
|----------|------------|--------|
| `mqtt_iot` | readwrite | `fei/#` |
| `homeassistant` | read | `fei/#` |
