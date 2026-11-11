// Contoh Lab 01: Blocking vs Non-Blocking (Pengujian delay vs millis)
#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_LED_ALERT 2
#define PIN_TOMBOL_DARURAT 15

#define MODE_NON_BLOCKING 1

DHT dht(PIN_DHT, TIPE_DHT);
unsigned long waktuBacaTerakhir = 0;
const unsigned long INTERVAL_TELEMETRI_MS = 2000;
const float AMBANG_SUHU_C = 32.0;
unsigned long nomorPaket = 0;
bool statusTombolSebelumnya = HIGH;

void kirimTelemetriJson(float suhu, float kelembapan, bool alertSuhu) {
  JsonDocument doc;
  doc["device_id"] = "esp32-unib-node01";
  doc["seq"] = ++nomorPaket;
  doc["uptime_ms"] = millis();
  doc["mode"] = MODE_NON_BLOCKING ? "non-blocking" : "blocking";
  doc["temp_c"] = serialized(String(suhu, 2));
  doc["hum_pct"] = serialized(String(kelembapan, 2));
  doc["alert"] = alertSuhu;

  serializeJson(doc, Serial);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_TOMBOL_DARURAT, INPUT_PULLUP);
  dht.begin();
  Serial.println("{\"event\":\"boot\",\"msg\":\"Simpul IoT UNIB Aktif\"}");
}

void loop() {
  bool statusTombol = digitalRead(PIN_TOMBOL_DARURAT);
  if (statusTombol == LOW && statusTombolSebelumnya == HIGH) {
    Serial.printf("{\"event\":\"emergency_button\",\"pressed_at_ms\":%lu}\n", millis());
    digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
    delay(50);
  }
  statusTombolSebelumnya = statusTombol;

#if MODE_NON_BLOCKING == 0
  delay(INTERVAL_TELEMETRI_MS);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    bool alert = (t >= AMBANG_SUHU_C);
    digitalWrite(PIN_LED_ALERT, alert ? HIGH : LOW);
    kirimTelemetriJson(t, h, alert);
  }
#else
  unsigned long sekarang = millis();
  if (sekarang - waktuBacaTerakhir >= INTERVAL_TELEMETRI_MS) {
    waktuBacaTerakhir = sekarang;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      bool alert = (t >= AMBANG_SUHU_C);
      digitalWrite(PIN_LED_ALERT, alert ? HIGH : LOW);
      kirimTelemetriJson(t, h, alert);
    }
  }
#endif
}

