#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <esp_task_wdt.h>
#include "config.h"

#define PIN_PUMPA        25
#define LORA_RX          16
#define LORA_TX          17
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22
#define PIN_POWER_PODA   13
#define PIN_DATA_PODA    32
#define PIN_POWER_FOTO   14
#define PIN_DATA_FOTO    35

#define POCET_VZORIEK  10
#define WDT_TIMEOUT_MS 30000

// Prahy cerpadla (raw ADC 12-bit, kapacitny senzor: vyssi ADC = suchsia poda)
#define PUMPA_ZAPNUT_ADC          2200
#define PUMPA_DOBA_MS             5000
#define PUMPA_MIN_INTERVAL_CYKLOV    5

// Prezivu deep sleep v RTC pamati
RTC_DATA_ATTR uint32_t cislo_spravy          = 0;
RTC_DATA_ATTR uint32_t cislo_poslednej_pumpy = 0;

HardwareSerial LoRaSerial(2);
Adafruit_BMP280 bmp;

uint8_t vypocitajChecksum(const TelemetryPacket* p) {
  const uint8_t* data = (const uint8_t*)p;
  uint8_t cs = 0;
  for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; i++) {
    cs ^= data[i];
  }
  return cs;
}

void xorCipher(TelemetryPacket* p) {
  uint8_t* data = (uint8_t*)p;
  for (size_t i = 1; i < sizeof(TelemetryPacket); i++) {
    data[i] ^= LORA_KEY[(i - 1) % LORA_KEY_LEN];
  }
}

void setup() {
  const esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms     = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,
    .trigger_panic  = true
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);

  pinMode(PIN_PUMPA,      OUTPUT); digitalWrite(PIN_PUMPA,      LOW);
  pinMode(PIN_POWER_PODA, OUTPUT); digitalWrite(PIN_POWER_PODA, LOW);
  pinMode(PIN_POWER_FOTO, OUTPUT); digitalWrite(PIN_POWER_FOTO, LOW);

  analogReadResolution(12);

  cislo_spravy++;
  uint8_t reset_dovod = (uint8_t)esp_reset_reason();
  Serial.print("\n=== ESP32 prebudenie #"); Serial.print(cislo_spravy);
  Serial.print(" reset="); Serial.print(reset_dovod); Serial.println(" ===");

  TelemetryPacket paket;
  memset(&paket, 0, sizeof(paket));
  paket.hlavicka     = PACKET_HEADER;
  paket.cislo_spravy = cislo_spravy;
  paket.reset_dovod  = reset_dovod;

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!bmp.begin(0x76)) {
    Serial.println("[BMP280] CHYBA: senzor nebol najdeny!");
    paket.chyby |= 0x01;
  } else {
    // Forced mode: jedno meranie na vyziadanie, potom senzor zaspi
    bmp.setSampling(
      Adafruit_BMP280::MODE_FORCED,
      Adafruit_BMP280::SAMPLING_X1,
      Adafruit_BMP280::SAMPLING_X4,
      Adafruit_BMP280::FILTER_X4,
      Adafruit_BMP280::STANDBY_MS_500
    );
    bmp.takeForcedMeasurement();
    paket.teplota = bmp.readTemperature();
    paket.tlak    = bmp.readPressure() / 100.0f;
    Serial.println("[BMP280] OK");
  }

  esp_task_wdt_reset();

  // Napajanie senzorov len pocas odberu vzoriek
  digitalWrite(PIN_POWER_PODA, HIGH);
  digitalWrite(PIN_POWER_FOTO, HIGH);
  delay(150);

  uint32_t sum_poda   = 0;
  uint32_t sum_svetlo = 0;
  for (int i = 0; i < POCET_VZORIEK; i++) {
    sum_poda   += analogRead(PIN_DATA_PODA);
    sum_svetlo += analogRead(PIN_DATA_FOTO);
    delay(50);
  }

  digitalWrite(PIN_POWER_PODA, LOW);
  digitalWrite(PIN_POWER_FOTO, LOW);

  paket.adc_poda   = (uint16_t)(sum_poda   / POCET_VZORIEK);
  paket.adc_svetlo = (uint16_t)(sum_svetlo / POCET_VZORIEK);

  esp_task_wdt_reset();

  // Cerpadlo: fixny cas behu + cooldown chrani pred zlyhanim senzora
  paket.pumpa_zapnuta = 0;
  uint32_t cyklov_od_pumpy = cislo_spravy - cislo_poslednej_pumpy;
  bool cooldown_ok = (cislo_poslednej_pumpy == 0) || (cyklov_od_pumpy >= PUMPA_MIN_INTERVAL_CYKLOV);

  if (paket.adc_poda > PUMPA_ZAPNUT_ADC && cooldown_ok) {
    Serial.print("  [PUMPA] Sucha poda (adc="); Serial.print(paket.adc_poda);
    Serial.println(") - zapinam cerpadlo");
    digitalWrite(PIN_PUMPA, HIGH);
    unsigned long start = millis();
    while (millis() - start < PUMPA_DOBA_MS) {
      esp_task_wdt_reset();
      delay(100);
    }
    digitalWrite(PIN_PUMPA, LOW);
    cislo_poslednej_pumpy = cislo_spravy;
    paket.pumpa_zapnuta = 1;
    Serial.println("  [PUMPA] Vypnute");
  } else if (paket.adc_poda > PUMPA_ZAPNUT_ADC && !cooldown_ok) {
    Serial.print("  [PUMPA] Cooldown (zostatok=");
    Serial.print(PUMPA_MIN_INTERVAL_CYKLOV - cyklov_od_pumpy);
    Serial.println(" cyklov)");
  }

  Serial.print("  T="); Serial.print(paket.teplota, 2); Serial.println(" C");
  Serial.print("  P="); Serial.print(paket.tlak,    2); Serial.println(" hPa");
  Serial.print("  ADC poda="); Serial.print(paket.adc_poda);
  Serial.print("  svetlo=");   Serial.println(paket.adc_svetlo);
  Serial.print("  Pumpa=");    Serial.print(paket.pumpa_zapnuta ? "ZAP" : "VYP");
  Serial.print("  Chyby=0x");  Serial.print(paket.chyby, HEX);
  Serial.print("  Reset=");    Serial.println(paket.reset_dovod);

  LoRaSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  delay(500);
  esp_task_wdt_reset();

  paket.checksum = vypocitajChecksum(&paket);
  xorCipher(&paket);

  LoRaSerial.write((uint8_t*)&paket, sizeof(paket));
  LoRaSerial.flush();
  delay(200);

  Serial.println("  -> odoslane");
  Serial.print("[SLEEP] Uspinam na ");
  Serial.print(SLEEP_INTERVAL_US / 1000000ULL);
  Serial.println(" s...");
  Serial.flush();

  esp_task_wdt_delete(NULL);
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_deep_sleep_start();
}

void loop() {
  // Po deep sleep ESP32 restartuje od setup()
}
