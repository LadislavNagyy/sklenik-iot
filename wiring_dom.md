# Schéma zapojenia — ESP8266 (vnútorná jednotka)

## Komponenty
- ESP8266 NodeMCU (3.3V logika)
- DX-LR02 LoRa modul (UART cez SoftwareSerial)
- USB / 5V napájanie (z domácej siete)

## Zapojenie pinov

```
ESP8266 Pin  Komponent              Poznamka
-----------  ---------------------  ---------------------------
D5 (GPIO14)  DX-LR02 TX             LoRa prijem (SoftwareSerial RX)
D6 (GPIO12)  DX-LR02 RX             LoRa odoslanie (SoftwareSerial TX)
3V3          DX-LR02 VCC            napajanie modulu
GND          DX-LR02 GND
             DX-LR02 M0, M1 -> neurcene (transparentny rezim)

USB          ESP8266 napajanie      5V z USB adaptéra
```

## Schéma napájania

```
[5V USB adaptér]
      |
  [ESP8266 NodeMCU]
      |
  [3.3V regulátor onboard]
      |
  [DX-LR02 LoRa modul]
```

## Funkcia uzla

ESP8266 nemeí žiadne senzory — funguje výhradne ako LoRa-to-MQTT brána.
Prijíma binárne pakety od ESP32 cez LoRa (SoftwareSerial 9600 baud),
dešifruje XOR šifrou, validuje checksum a publikuje na MQTT broker.
WiFi pripojenie so statickou IP (192.168.1.100).
