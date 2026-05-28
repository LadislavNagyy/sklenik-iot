// ESP8266 - vnutorna jednotka
// Prijima LoRa pakety zo sklenika, dekoduje, validuje a publikuje na MQTT broker.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <time.h>
#include "config.h"

#define WIFI_SSID       "..."
#define WIFI_PASSWORD   "..."

#define MQTT_USER       "..."
#define MQTT_PASSWORD   "..."
#define MQTT_CLIENT_ID "esp8266_sklenik_rx"

#define LORA_RX_PIN  D5
#define LORA_TX_PIN  D6

#define NTP_SERVER      "pool.ntp.org"
#define NTP_UTC_OFFSET  3600
#define NTP_DST_OFFSET  3600

// Kalibrovane hodnoty senzora vlhkosti pody (kapacitny, 12-bit ADC)
#define SOIL_DRY_ADC  2015   // sucha poda -> 0% (kalibracia: max namerany 2015)
#define SOIL_WET_ADC  1434   // mokra poda -> 100% (kalibracia: uplne mokra 1434)

// Kalibrovane hodnoty fotoodporu (12-bit ADC)
#define LIGHT_DARK_ADC    0  // tma -> 0%
#define LIGHT_BRIGHT_ADC  4311  // plne slnko -> 95% pri ADC=4095 (kalibracia: vonku bez oblakov)

// Staticka IP - router drzi staru asociaciu 5s po resete
IPAddress STATIC_IP(192, 168, 1, 100);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);
IPAddress DNS(8, 8, 8, 8);

SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN);
WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// XOR desifrovanie - rovnaky algoritmus ako na ESP32
void xorCipher(TelemetryPacket* p) {
  uint8_t* data = (uint8_t*)p;
  for (size_t i = 1; i < sizeof(TelemetryPacket); i++)
    data[i] ^= LORA_KEY[(i - 1) % LORA_KEY_LEN];
}

// Overi XOR kontrolny sucet prijateho paketu
bool checksumOK(const TelemetryPacket* p) {
  const uint8_t* data = (const uint8_t*)p;
  uint8_t cs = 0;
  for (size_t i = 0; i < sizeof(TelemetryPacket) - 1; i++) cs ^= data[i];
  return cs == p->checksum;
}

// Rozsahova validacia - pri BMP280 chybe preskoci kontrolu teploty/tlaku
bool isValidData(const TelemetryPacket* p) {
  if (!(p->chyby & 0x01)) {
    if (p->teplota < -40.0f || p->teplota > 85.0f)  return false;
    if (p->tlak    < 300.0f || p->tlak    > 1100.0f) return false;
  }
  return true;
}

// Publikuje textovu spravu na TOPIC_LOG s timestampom
void publishLog(const char* msg) {
  if (!mqttClient.connected()) return;
  time_t now = time(nullptr);
  char payload[220];
  snprintf(payload, sizeof(payload),
    "{\"msg\":\"%s\",\"ts\":%lu,\"node\":\"dom\"}", msg, (unsigned long)now);
  mqttClient.publish(TOPIC_LOG, payload, false);
  Serial.print("[LOG] "); Serial.println(msg);
}

// WiFi reconnect so statickou IP; 5s WIFI_OFF kvoli Speedport routeru
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect(true);
  delay(5000);
  WiFi.mode(WIFI_OFF);
  delay(5000);
  WiFi.mode(WIFI_STA);
  WiFi.config(STATIC_IP, GATEWAY, SUBNET, DNS);
  Serial.print("[WiFi] Pripajam sa...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println(" zlyhalo.");
  }
}

// MQTT connect s LWT: pri vypnuti broker publikuje "offline" na TOPIC_STATUS
bool connectMqtt() {
  if (mqttClient.connected()) return true;
  Serial.print("[MQTT] Pripajam sa...");
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                         TOPIC_STATUS, 1, true, "offline")) {
    mqttClient.publish(TOPIC_STATUS, "online", true);
    Serial.println(" OK");
    return true;
  }
  Serial.print(" zlyhalo rc="); Serial.println(mqttClient.state());
  return false;
}

// Publikuje jednu velicinu ako JSON s retain=true
void publishValue(const char* topic, float value, const char* unit, const char* sensor) {
  time_t now = time(nullptr);
  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"value\":%.2f,\"unit\":\"%s\",\"ts\":%lu,\"node\":\"" NODE_ID "\",\"sensor\":\"%s\"}",
    value, unit, (unsigned long)now, sensor);
  bool ok = mqttClient.publish(topic, payload, true);
  Serial.print("  "); Serial.print(topic); Serial.println(ok ? " OK" : " FAIL");
}

void setup() {
  WiFi.persistent(false);
  Serial.begin(115200);
  delay(100);
  Serial.print("MAC: "); Serial.println(WiFi.macAddress());
  LoRaSerial.begin(9600);

  mqttClient.setBufferSize(512);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  Serial.println("\n=== ESP8266 - Vnutorna jednotka ===");

  connectWifi();

  if (WiFi.status() == WL_CONNECTED) {
    configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    Serial.print("[NTP] Synchronizujem");
    int waited = 0;
    while (time(nullptr) < 100000UL && waited < 20) { delay(500); Serial.print("."); waited++; }
    Serial.println(" OK");
  }

  if (connectMqtt()) publishLog("dom: start, cakam na LoRa data");
  Serial.println("[INIT] Cakam na LoRa data...\n");
}

void loop() {
  // WiFi watchdog - reconnect pri vypadku
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnect...");
    connectWifi();
    if (WiFi.status() == WL_CONNECTED) {
      configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
      publishLog("dom: WiFi reconnect OK");
    }
  }

  // MQTT watchdog - reconnect s 5s backoff
  if (!mqttClient.connected()) {
    static unsigned long poslednyRetry = 0;
    if (millis() - poslednyRetry > 5000) {
      Serial.println("[MQTT] Reconnect...");
      if (connectMqtt()) publishLog("dom: MQTT reconnect OK");
      poslednyRetry = millis();
    }
  }
  mqttClient.loop();

  // Caka na cely paket; zahadzuje bajty kym nenajde sync hlavicku 0xFF
  if (LoRaSerial.available() < (int)sizeof(TelemetryPacket)) return;
  if (LoRaSerial.peek() != PACKET_HEADER) { LoRaSerial.read(); return; }

  TelemetryPacket paket;
  uint8_t* buf = (uint8_t*)&paket;
  for (size_t i = 0; i < sizeof(TelemetryPacket); i++) buf[i] = LoRaSerial.read();

  xorCipher(&paket);

  if (!checksumOK(&paket)) {
    Serial.println("[WARN] Zly checksum.");
    publishLog("WARN: zly checksum, paket zahodeny");
    return;
  }

  if (!isValidData(&paket)) {
    Serial.println("[WARN] Data mimo rozsah.");
    char msg[80];
    snprintf(msg, sizeof(msg), "WARN: data mimo rozsah T=%.1f P=%.1f chyby=0x%02X",
             paket.teplota, paket.tlak, paket.chyby);
    publishLog(msg);
    return;
  }

  // Duplikat - rovnake cislo_spravy uz bolo spracovane (ESP32 posiela paket 5-krat)
  static uint32_t posledne_cislo = 0;
  if (paket.cislo_spravy == posledne_cislo) {
    Serial.println("[INFO] Duplikat, preskocene.");
    return;
  }
  posledne_cislo = paket.cislo_spravy;

  if (!mqttClient.connected()) {
    Serial.println("[WARN] MQTT odpojene, paket zahodeny."); return;
  }

  // Prepocet raw ADC na fyzikalne jednotky
  int soil_pct  = constrain(map(paket.adc_poda,   SOIL_DRY_ADC,    SOIL_WET_ADC,    0, 100), 0, 100);
  int light_pct = constrain(map(paket.adc_svetlo,  LIGHT_DARK_ADC,  LIGHT_BRIGHT_ADC, 0, 100), 0, 100);

  Serial.println("-----------------------------");
  Serial.print("Paket #");          Serial.println(paket.cislo_spravy);
  Serial.print("  Teplota:  ");     Serial.print(paket.teplota, 2);  Serial.println(" C");
  Serial.print("  Tlak:     ");     Serial.print(paket.tlak,    2);  Serial.println(" hPa");
  Serial.print("  Vlhkost pody: "); Serial.print(soil_pct);  Serial.print("% ("); Serial.print(paket.adc_poda);   Serial.println(")");
  Serial.print("  Svetlo:   ");     Serial.print(light_pct); Serial.print("% ("); Serial.print(paket.adc_svetlo); Serial.println(")");
  Serial.print("  Pumpa:    ");     Serial.println(paket.pumpa_zapnuta ? "ZAP" : "VYP");
  Serial.print("  Chyby:    0x");   Serial.println(paket.chyby, HEX);
  Serial.print("  Reset:    ");     Serial.println(paket.reset_dovod);
  Serial.println("-----------------------------");

  // Publikovanie vsetkych velicin na MQTT s retain=true
  publishValue(TOPIC_TEMPERATURE, paket.teplota,              "\xC2\xB0""C", "BMP280");
  publishValue(TOPIC_PRESSURE,    paket.tlak,                 "hPa",         "BMP280");
  publishValue(TOPIC_SOIL,        (float)soil_pct,            "%",           "soil_sensor");
  publishValue(TOPIC_LIGHT,       (float)light_pct,           "%",           "photoresistor");
  publishValue(TOPIC_PUMP,        (float)paket.pumpa_zapnuta, "",            "pump");

  // Zhrnutie paketu ako log sprava na TOPIC_LOG
  char logMsg[120];
  snprintf(logMsg, sizeof(logMsg),
    "paket #%lu T=%.1fC P=%.0fhPa poda=%d%% svetlo=%d%% pumpa=%d chyby=0x%02X reset=%d",
    (unsigned long)paket.cislo_spravy,
    paket.teplota, paket.tlak, soil_pct, light_pct,
    paket.pumpa_zapnuta, paket.chyby, paket.reset_dovod);
  publishLog(logMsg);
}
