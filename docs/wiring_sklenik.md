# Schéma zapojenia — ESP32 (skleníkový uzol)

## Komponenty
- ESP32 DevKit (3.3V logika)
- BMP280 (I2C, teplota + tlak)
- Kapacitný senzor vlhkosti pôdy (analógový, 3.3V)
- Fotoodpor s deličom napätia (analógový)
- Vodné čerpadlo 12V (cez MOSFET / relé)
- DX-LR02 LoRa modul (UART)
- 12V 7Ah batéria + DC-DC menič 12V→3.3V

## Zapojenie pinov

```
ESP32 Pin    Komponent              Poznamka
-----------  ---------------------  ---------------------------
3V3          BMP280 VCC             napajanie senzora
GND          BMP280 GND
GPIO21 (SDA) BMP280 SDA             I2C data
GPIO22 (SCL) BMP280 SCL             I2C hodiny

GPIO13       Sensor pody VCC        napajanie zapnute len pocas merania
GPIO32       Sensor pody AOUT       ADC vstup (12-bit, 0-3.3V)
GND          Sensor pody GND

GPIO14       Fotoodpor VCC          napajanie zapnute len pocas merania
GPIO35       Fotoodpor AOUT         ADC vstup (12-bit) - pozor: len vstupny pin
GND          Fotoodpor GND
             (fotoodpor + 10k ohm delič napätia na GND)

GPIO25       Relé / MOSFET gate     riadenie cerpadla
GND          Relé GND

GPIO16 (RX2) DX-LR02 TX             LoRa prijem
GPIO17 (TX2) DX-LR02 RX             LoRa odoslanie
3V3          DX-LR02 VCC
GND          DX-LR02 GND
             DX-LR02 M0, M1 -> neurcene (transparentny rezim, nie su pouzite)

3V3 (z DC-DC) ESP32 3V3 / VIN
GND           ESP32 GND
```

## Schéma napájania

```
[12V 7Ah batéria]
      |
  [DC-DC menič 12V → 3.3V]
      |
  [ESP32 3V3]
      |
  [BMP280] [Senzor pôdy*] [Fotoodpor*]

  [12V batéria] → [Relé/MOSFET] → [Čerpadlo 12V]
                        ↑
                  [GPIO25 ESP32]
```
*napájanie cez GPIO pin — zapínané len počas merania (150ms)

## Kalibrácia senzorov (kategória B — ADC)

| Senzor | Podmienka | ADC hodnota | Fyzikálna hodnota |
|--------|-----------|-------------|-------------------|
| Kapacitný senzor pôdy | Suchá pôda | 2015 | 0% vlhkosť |
| Kapacitný senzor pôdy | Mokrá pôda | 1434 | 100% vlhkosť |
| Fotoodpor | Tma | 0 | 0% intenzita |
| Fotoodpor | Plné slnko (vonku, bez oblakov) | 4095 | ~95% intenzita (ref. 4311 → 100%) |
| BMP280 | Offset voči ref. teplomeru | −2.7 °C | korekcia v kóde |

Kalibrácia vykonaná s 12-bit ADC (0–4095). Lineárna interpolácia funkciou `map()`.
