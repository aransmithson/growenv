#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#define DPIN 4        // Pin to connect DHT sensor (GPIO number) D2
#define DTYPE DHT11   // Define DHT 11 or DHT22 sensor type

// ── WiFi credentials ──────────────────────────────────────────
const char* WIFI_SSID     = "VM6384625";
const char* WIFI_PASSWORD = "ansArrbfje42pxsr";

// ── Cloudflare Worker endpoint ────────────────────────────────
const char* SERVER_URL = "http://sparkling-poetry-f04f.aransmithson.workers.dev/log";
const char* DEVICE_ID  = "esp8266-1";

// ── Timing ────────────────────────────────────────────────────
const unsigned long READ_INTERVAL = 300000;  // 5 minutes between uploads
unsigned long lastReadTime = 0;

DHT dht(DPIN, DTYPE);

// ─────────────────────────────────────────────────────────────
void connectToWiFi() {
  Serial.printf("\nConnecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ─────────────────────────────────────────────────────────────
bool postReading(float temperature, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected — reconnecting...");
    connectToWiFi();
  }

  WiFiClient client;
  HTTPClient http;

  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  StaticJsonDocument<128> doc;
  doc["temperature"] = temperature;
  doc["humidity"]    = humidity;
  doc["device_id"]   = DEVICE_ID;

  String payload;
  serializeJson(doc, payload);

  Serial.print("[HTTP] POST → ");
  Serial.println(payload);

  int statusCode = http.POST(payload);

  if (statusCode > 0) {
    Serial.printf("[HTTP] Response: %d — %s\n",
                  statusCode, http.getString().c_str());
  } else {
    Serial.printf("[HTTP] Error: %s\n",
                  http.errorToString(statusCode).c_str());
  }

  http.end();
  return (statusCode == 200);
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  dht.begin();
  connectToWiFi();
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    float tc = dht.readTemperature(false);  // Celsius
    float tf = dht.readTemperature(true);   // Fahrenheit
    float hu = dht.readHumidity();

    if (isnan(tc) || isnan(hu)) {
      Serial.println("[DHT] Failed to read sensor!");
      return;
    }

    Serial.printf("[DHT] Temp: %.1f°C / %.1f°F  Hum: %.1f%%\n", tc, tf, hu);
    postReading(tc, hu);
  }
}
