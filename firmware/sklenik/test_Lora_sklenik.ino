#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

// --- Piny ---
#define PIN_PUMPA        25
#define LORA_RX          16 
#define LORA_TX          17 
#define PIN_I2C_SDA      21 
#define PIN_I2C_SCL      22 
#define PIN_POWER_PODA   13 
#define PIN_DATA_PODA    32 
#define PIN_POWER_FOTO   14 
#define PIN_DATA_FOTO    35 
#define PIN_DATA_BATERIA 34 

HardwareSerial LoRaSerial(2);
Adafruit_BMP280 bmp;

// Datova struktura (identicka s prijimacom)
struct __attribute__((__packed__)) TelemetryData {
  uint8_t hlavicka;        
  float teplota_vzduchu;   
  float tlak_vzduchu;      
  uint16_t vlhkost_pody;   
  uint16_t intenzita_svetla; 
  float napatie_baterie;   
  uint32_t cislo_spravy;
} telemetry;

void setup() {
  Serial.begin(115200);
  LoRaSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  
  pinMode(PIN_PUMPA, OUTPUT); digitalWrite(PIN_PUMPA, LOW);
  pinMode(PIN_POWER_PODA, OUTPUT); digitalWrite(PIN_POWER_PODA, LOW);
  pinMode(PIN_POWER_FOTO, OUTPUT); digitalWrite(PIN_POWER_FOTO, LOW);

  analogReadResolution(12);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!bmp.begin(0x76)) {
    Serial.println("CHYBA: BMP280 nenajdeny!");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }

  telemetry.hlavicka = 0xFF;
  telemetry.cislo_spravy = 0;
  
  delay(1000);
}

void loop() {
  telemetry.cislo_spravy++;

  // 1. Digitalne data z BMP280 (ma vlastny interny filter)
  telemetry.teplota_vzduchu = bmp.readTemperature();
  telemetry.tlak_vzduchu = bmp.readPressure() / 100.0;

  // 2. PRIEMEROVANIE ANALOGOVYCH DAT
  digitalWrite(PIN_POWER_PODA, HIGH);
  digitalWrite(PIN_POWER_FOTO, HIGH);
  delay(150); // Cas na ustálenie napätia a kondenzátorov

  uint32_t sum_poda = 0;
  uint32_t sum_svetlo = 0;
  uint32_t sum_bateria = 0;
  const int pocet_vzoriek = 20;

  // Citame hodnoty 20-krat s odstupom 50ms (spolu 1 sekunda merania)
  for (int i = 0; i < pocet_vzoriek; i++) {
    sum_poda += analogRead(PIN_DATA_PODA);
    sum_svetlo += analogRead(PIN_DATA_FOTO);
    sum_bateria += analogRead(PIN_DATA_BATERIA);
    delay(50);
  }

  digitalWrite(PIN_POWER_PODA, LOW);
  digitalWrite(PIN_POWER_FOTO, LOW);

  // Ulozenie spriemerovanych hodnot do struktury
  telemetry.vlhkost_pody = sum_poda / pocet_vzoriek;
  telemetry.intenzita_svetla = sum_svetlo / pocet_vzoriek;
  
  uint16_t adc_bateria = sum_bateria / pocet_vzoriek;
  
  // 3. Vypocet napatia (kalibracia ak treba, zmen 3.3 na inu hodnotu)
  float napatie_na_pine = (adc_bateria / 4095.0) * 3.3; 
  telemetry.napatie_baterie = napatie_na_pine * ((1000.0 + 220.0) / 220.0);

  // 4. Odoslanie dat
  LoRaSerial.write((uint8_t*)&telemetry, sizeof(telemetry));
  LoRaSerial.flush();

  // Vypis pre ladenie
  Serial.print("Odosielam spravu #"); Serial.println(telemetry.cislo_spravy);
  Serial.print("Spriemerovana Poda: "); Serial.println(telemetry.vlhkost_pody);
  Serial.print("Napatie Baterie: "); Serial.print(telemetry.napatie_baterie); Serial.println(" V\n");

  delay(10000); // V ostrej prevadzke zmen na dhlsi cas
}