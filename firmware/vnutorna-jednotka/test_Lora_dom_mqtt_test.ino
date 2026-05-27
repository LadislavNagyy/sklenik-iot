#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <time.h>

// WiFi credentials
#define WIFI_SSID       "..."
#define WIFI_PASSWORD   "..."

// MQTT broker - Home Assistant on Raspberry Pi
#define MQTT_BROKER     "192.168.1.228"
#define MQTT_PORT       1883
#define MQTT_USER       "..."
#define MQTT_PASSWORD   "..."
#define MQTT_CLIENT_ID  "esp8266_sklenik_rx"

// LoRa SoftwareSerial pins (D1 Mini)
#define LORA_RX_PIN     D5
#define LORA_TX_PIN     D6

// MQTT topics
#define TOPIC_TEMPERATURE  "sklenik/temperature"
#define TOPIC_PRESSURE     "sklenik/pressure"
#define TOPIC_SOIL         "sklenik/soil_moisture"
#define TOPIC_LIGHT        "sklenik/light"
#define TOPIC_BATTERY      "sklenik/battery"

// NTP settings (UTC+1 base, +1 DST - Slovakia)
#define NTP_SERVER      "pool.ntp.org"
#define NTP_UTC_OFFSET  3600
#define NTP_DST_OFFSET  3600

// Soil moisture calibration (dry ADC / wet ADC)
#define SOIL_DRY_ADC    2500
#define SOIL_WET_ADC    1300


SoftwareSerial LoRaSerial(LORA_RX_PIN, LORA_TX_PIN);

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);

// Telemetry struct - must match ESP32 sender exactly
struct __attribute__((__packed__)) TelemetryData {
  uint8_t  hlavicka;
  float    teplota_vzduchu;
  float    tlak_vzduchu;
  uint16_t vlhkost_pody;
  uint16_t intenzita_svetla;
  float    napatie_baterie;
  uint32_t cislo_spravy;
} prijate_data;

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  // Uplne vypneme radio, aby sme zrusili staru asociaciu v routeri
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_OFF);
  delay(5000);

  // DHCP - router prideluje IP podla MAC rezervacie
  WiFi.mode(WIFI_STA);

  Serial.print("[WiFi] Connecting to: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("[WiFi] Connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection failed - will retry later.");
  }
}

bool connectMqtt() {
  if (mqttClient.connected()) return true;

  Serial.print("[MQTT] Connecting to broker...");

  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println(" connected!");
    return true;
  }

  Serial.print(" failed. rc=");
  Serial.println(mqttClient.state());
  return false;
}

// Publish one sensor value as JSON with retain=true
// Format: {"value":23.4,"unit":"C","ts":1714000000,"node":"sklenik","sensor":"BMP280"}
void publishSensor(const char* topic, float value, const char* unit, const char* sensor_name) {
  time_t now = time(nullptr);

  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"value\":%.2f,\"unit\":\"%s\",\"ts\":%lu,\"node\":\"sklenik\",\"sensor\":\"%s\"}",
    value, unit, (unsigned long)now, sensor_name
  );

  bool ok = mqttClient.publish(topic, payload, true);

  Serial.print("  [MQTT] ");
  Serial.print(topic);
  Serial.print(ok ? " -> OK" : " -> FAIL");
  Serial.print("  payload: ");
  Serial.println(payload);
}

void setup() {
  // persistent(false) musi byt uplne prve volanie WiFi, inak nacita stare udaje z flash
  WiFi.persistent(false);

  Serial.begin(115200);
  delay(100);
  Serial.print("MAC adresa: ");
  Serial.println(WiFi.macAddress());
  LoRaSerial.begin(9600);

  mqttClient.setBufferSize(256);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  delay(500);
  Serial.println("\n=== ESP8266 - Vnutorna jednotka (MQTT gateway) ===");

  connectWifi();

  // Sync time via NTP for JSON timestamps
  if (WiFi.status() == WL_CONNECTED) {
    configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    Serial.print("[NTP] Syncing time");
    int waited = 0;
    while (time(nullptr) < 100000UL && waited < 20) {
      delay(500);
      Serial.print(".");
      waited++;
    }
    Serial.println(" done.");
  }

  connectMqtt();
  Serial.println("[INIT] Setup complete. Waiting for LoRa data...\n");
}

void loop() {
  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection lost. Reconnecting...");
    connectWifi();
    if (WiFi.status() == WL_CONNECTED) {
      configTime(NTP_UTC_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    }
  }

  // MQTT watchdog - retry every 5s
  if (!mqttClient.connected()) {
    static unsigned long lastMqttRetry = 0;
    if (millis() - lastMqttRetry > 5000) {
      Serial.println("[MQTT] Connection lost. Reconnecting...");
      connectMqtt();
      lastMqttRetry = millis();
    }
  }
  mqttClient.loop();

  // Read incoming LoRa packet
  if (LoRaSerial.available() >= (int)sizeof(TelemetryData)) {

    if (LoRaSerial.peek() == 0xFF) {
      uint8_t* buf = (uint8_t*)&prijate_data;
      for (size_t i = 0; i < sizeof(TelemetryData); i++) {
        buf[i] = LoRaSerial.read();
      }

      Serial.print("\n--> Packet #");
      Serial.print(prijate_data.cislo_spravy);
      Serial.print(" | temp=");
      Serial.print(prijate_data.teplota_vzduchu);
      Serial.print("C | pressure=");
      Serial.print(prijate_data.tlak_vzduchu);
      Serial.println("hPa");

      if (!mqttClient.connected()) {
        Serial.println("  [WARN] MQTT not connected - packet dropped.");
        return;
      }

      // Convert raw ADC to percentages (calibration same as test code)
      int soil_pct  = constrain(map(prijate_data.vlhkost_pody,     SOIL_DRY_ADC, SOIL_WET_ADC, 0, 100), 0, 100);
      int light_pct = constrain(map(prijate_data.intenzita_svetla, 4095, 0, 0, 100), 0, 100);

      publishSensor(TOPIC_TEMPERATURE, prijate_data.teplota_vzduchu, "C",   "BMP280");
      publishSensor(TOPIC_PRESSURE,    prijate_data.tlak_vzduchu,    "hPa", "BMP280");
      publishSensor(TOPIC_SOIL,        (float)soil_pct,              "%",   "soil_sensor");
      publishSensor(TOPIC_LIGHT,       (float)light_pct,             "%",   "photoresistor");
      publishSensor(TOPIC_BATTERY,     prijate_data.napatie_baterie, "V",   "voltage_divider");

    } else {
      LoRaSerial.read(); // discard bad header byte
    }
  }
}
