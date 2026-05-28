#pragma once

// config.h - spolocna konfiguracia pre sklenik.ino aj dom.ino
// Kriterium 3.1: broker adresa, JSON format, perioda na jednom mieste
// POZOR: tento subor musi byt identicky v priecinku sklenik/ aj dom/

// Interval deep sleep [mikrosekundy]
// Testovanie: 10 000 000 (10s), produkcia: 60 000 000 (60s) alebo 3 600 000 000 (1h)
#define SLEEP_INTERVAL_US  3600000000ULL

// Synchronizacny bajt - zaciatok paketu (nesifruje sa, sluzi na detekciu)
#define PACKET_HEADER  0xFF

// MQTT broker - Raspberry Pi s Mosquitto
#define MQTT_BROKER  "192.168.1.228"
#define MQTT_PORT    1883

// Identifikator uzla zhodny s MQTT topikom (fei/<uzol>/<velicina>)
#define NODE_ID  "sklenik"

// MQTT topiky podla predpisanej hierarchie zo zadania: fei/<uzol>/<velicina>
#define TOPIC_TEMPERATURE  "fei/" NODE_ID "/temperature"
#define TOPIC_PRESSURE     "fei/" NODE_ID "/pressure"
#define TOPIC_SOIL         "fei/" NODE_ID "/soil_moisture"
#define TOPIC_LIGHT        "fei/" NODE_ID "/light"
#define TOPIC_PUMP         "fei/" NODE_ID "/pump"
#define TOPIC_STATUS       "fei/" NODE_ID "/status"
#define TOPIC_LOG          "fei/" NODE_ID "/log"

// XOR sifrovaci kluc pre zabezpecenie LoRa prenosu
static const uint8_t LORA_KEY[]   = {0xA3, 0x7F, 0x2E, 0x91, 0x5C, 0xD4, 0x68, 0x1B};
static const uint8_t LORA_KEY_LEN = sizeof(LORA_KEY);

// Datova struktura LoRa paketu - identicky layout na ESP32 (sklenik) aj ESP8266 (dom)
// __packed__: zakazuje zarovnanie pamate, zarucuje rovnaku velkost na oboch MCU
// chyby: bit 0 = BMP280 nenajdeny
struct __attribute__((__packed__)) TelemetryPacket {
  uint8_t  hlavicka;       // sync byte 0xFF (nesifruje sa)
  float    teplota;        // BMP280 [C]
  float    tlak;           // BMP280 [hPa]
  uint16_t adc_poda;       // raw ADC 12-bit - kapacitny senzor vlhkosti pody
  uint16_t adc_svetlo;     // raw ADC 12-bit - fotoresistor
  uint32_t cislo_spravy;   // poradove cislo paketu (RTC pamat, prezi deep sleep)
  uint8_t  pumpa_zapnuta;  // 1 ak bola pumpa aktivovana v tomto cykle, inak 0
  uint8_t  reset_dovod;    // esp_reset_reason() - dovod posledneho resetu/prebudenia
  uint8_t  chyby;          // bitove priznaky chyb: bit0=BMP280 chyba
  uint8_t  checksum;       // XOR vsetkych predchadzajucich bajtov
};
