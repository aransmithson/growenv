#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DPIN 4        // Pin to connect DHT sensor (GPIO number) D2
#define DTYPE DHT11   // Define DHT 11 or DHT22 sensor type

// ── OLED Display ──────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin (set to GPIO if your display has one)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Halt if display init fails
  }
  delay(100);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
}

// ─────────────────────────────────────────────────────────────
void updateDisplay(float temperature, float humidity, bool sensorOK, bool uploadOK) {
  display.clearDisplay();

  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("ESP8266 Monitor");

  // Separator line
  display.drawFastHLine(0, 10, SCREEN_WIDTH, WHITE);

  if (!sensorOK) {
    // Sensor error state
    display.setCursor(0, 20);
    display.setTextSize(1);
    display.print("Sensor Error!");
    display.setCursor(0, 35);
    display.print("Check DHT wiring");
  } else {
    // Temperature
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.print("Temp:");
    display.setTextSize(2);
    display.setCursor(0, 26);
    display.print(temperature, 1);
    display.setTextSize(1);
    display.print(" C");

    // Humidity
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print("Humidity:");
    display.setTextSize(2);
    display.setCursor(0, 56);
    display.print(humidity, 1);
    display.setTextSize(1);
    display.print(" %");
  }

  // Status footer
  display.setTextSize(1);
  display.setCursor(80, 56);
  if (!sensorOK) {
    display.print("ERR");
  } else if (uploadOK) {
    display.print("SENT");
  } else {
    display.print("LOCAL");
  }

  display.display();
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
  initDisplay();
  
  // Show boot screen briefly
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Connecting...");
  display.display();
  delay(1000);
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    float tc = dht.readTemperature(false);  // Celsius
    float tf = dht.readTemperature(true);   // Fahrenheit
    float hu = dht.readHumidity();

    bool sensorOK = !(isnan(tc) || isnan(hu));
    bool uploadOK = false;

    if (!sensorOK) {
      Serial.println("[DHT] Failed to read sensor!");
      updateDisplay(0, 0, false, false);
      return;
    }

    Serial.printf("[DHT] Temp: %.1f°C / %.1f°F  Hum: %.1f%%\n", tc, tf, hu);
    
    // Update display with fresh readings before uploading
    updateDisplay(tc, hu, true, false);
    
    uploadOK = postReading(tc, hu);
    
    // Update display again to show upload status
    updateDisplay(tc, hu, true, uploadOK);
  }
}
