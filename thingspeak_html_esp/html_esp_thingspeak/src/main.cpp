#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "DHT.h"

// --- Cấu hình chân ---
#define DHTPIN 18        // chân kết nối DHT
#define DHTTYPE DHT11    // hoặc DHT22
#define LED_PIN 4        // chân LED
#define LED_PIN1 19 
#define LED_PIN2 21 
// --- DHT ---
DHT dht(DHTPIN, DHTTYPE);

// --- WiFi ---
const char* ssid = "DuyAn";
const char* password = "duyan5678";

// --- ThingSpeak Channel 1 (Sensor) ---
String apiKey1 = "GRCJC30IIGGHI4SX";
String server = "http://api.thingspeak.com";

// --- ThingSpeak Channel 2 (Control) ---
String readAPIKey2 = "MG3QHGIIU9UT6SPF";
String channelID2  = "2651233";

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 40000; // 40s

// --- NTP ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600); // GMT+7

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected.");

  dht.begin();
  pinMode(LED_PIN, OUTPUT);

  // Khởi động NTP
  timeClient.begin();
  timeClient.update();
}

void loop() {
  timeClient.update();

  // --- Đọc sensor và gửi lên Channel 1 mỗi 40s ---
  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      HTTPClient http;
      String url = server + "/update?api_key=" + apiKey1 +
                   "&field1=" + String(t) +
                   "&field2=" + String(h);

      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("📤 ThingSpeak Response (Sensor): " + payload);
      }
      http.end();
    }
  }

  // --- Đọc trạng thái LED (field1) & chế độ (field2) từ Channel 2 ---
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Field1: trạng thái LED
    String url = "http://api.thingspeak.com/channels/" + channelID2 +
                 "/fields/3/last.txt?api_key=" + readAPIKey2;
    http.begin(url);
    int httpCode = http.GET();
    String ledState = (httpCode > 0) ? http.getString() : "0";
    http.end();
    ledState.trim();

    // Field2: chế độ auto/manual
    url = "http://api.thingspeak.com/channels/" + channelID2 +
          "/fields/4/last.txt?api_key=" + readAPIKey2;
    http.begin(url);
    httpCode = http.GET();
    String mode = (httpCode > 0) ? http.getString() : "0";
    http.end();
    mode.trim();

    Serial.println("📥 Field1 (LED): " + ledState + " | Field2 (Mode): " + mode);

    // --- Điều khiển LED ---
    int hourNow = timeClient.getHours();
    if (mode == "0") {
      // Manual mode
      digitalWrite(LED_PIN, (ledState == "1") ? HIGH : LOW);
      Serial.println("Mode: MANUAL - LED = " + ledState);
    } else {
      // Auto mode (18h–22h)
      if (hourNow >= 18 && hourNow < 22) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Mode: AUTO - LED ON (18h-22h)");
      } else {
        digitalWrite(LED_PIN, LOW);
        Serial.println("Mode: AUTO - LED OFF");
      }
    }
  }

  delay(2000); // poll control channel mỗi 2s
}
