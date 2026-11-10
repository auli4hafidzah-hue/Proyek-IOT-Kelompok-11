#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ==========================================
// KONFIGURASI PIN THERMOSENSE
// ==========================================

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_LED_ALERT 2
#define PIN_TOMBOL_DARURAT 15

// 0 = Blocking
// 1 = Non-Blocking
#define MODE_NON_BLOCKING 1

// ==========================================
// KONFIGURASI SISTEM
// ==========================================

DHT dht(PIN_DHT, TIPE_DHT);

unsigned long waktuBacaTerakhir = 0;
const unsigned long INTERVAL_TELEMETRI_MS = 2000;

const float AMBANG_SUHU_C = 32.0;

unsigned long nomorPaket = 0;

bool statusTombolSebelumnya = HIGH;

// ==========================================
// FUNGSI OUTPUT JSON
// ==========================================

void kirimEvent(const char* eventName, const char* message) {
  JsonDocument doc;

  doc["event"] = eventName;
  doc["msg"] = message;
  doc["uptime_ms"] = millis();

  serializeJson(doc, Serial);
  Serial.println();
  Serial.flush();
}

// ==========================================
// FUNGSI TELEMETRI SENSOR
// ==========================================

void kirimTelemetriJson(
  float suhu,
  float kelembapan,
  bool alertSuhu
) {
  JsonDocument doc;

  doc["device_id"] = "THERMOSENSE-UNIB-11";
  doc["seq"] = ++nomorPaket;
  doc["uptime_ms"] = millis();

  doc["mode"] = MODE_NON_BLOCKING
                ? "non-blocking"
                : "blocking";

  doc["temp_c"] = suhu;
  doc["hum_pct"] = kelembapan;
  doc["alert"] = alertSuhu;

  serializeJson(doc, Serial);
  Serial.println();
  Serial.flush();
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  delay(500);

  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_TOMBOL_DARURAT, INPUT_PULLUP);

  digitalWrite(PIN_LED_ALERT, LOW);

  dht.begin();

  kirimEvent(
    "boot",
    "THERMOSENSE Aktif"
  );

  kirimEvent(
    "serial_test",
    "Komunikasi Serial ESP32 Berhasil"
  );

}

// ==========================================
// LOOP
// ==========================================

void loop() {

  // ----------------------------------------
  // PEMERIKSAAN TOMBOL DARURAT
  // ----------------------------------------

  bool statusTombol = digitalRead(PIN_TOMBOL_DARURAT);

  if (statusTombol == LOW &&
      statusTombolSebelumnya == HIGH) {

    JsonDocument doc;

    doc["event"] = "emergency_button";
    doc["pressed_at_ms"] = millis();

    serializeJson(doc, Serial);
    Serial.println();

    digitalWrite(
      PIN_LED_ALERT,
      !digitalRead(PIN_LED_ALERT)
    );

    delay(50);
  }

  statusTombolSebelumnya = statusTombol;

  // ----------------------------------------
  // MODE BLOCKING
  // ----------------------------------------

#if MODE_NON_BLOCKING == 0

  delay(INTERVAL_TELEMETRI_MS);

  kirimEvent(
    "serial_test",
    "Serial masih aktif"
  );

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {

    kirimEvent(
      "sensor_error",
      "Gagal membaca DHT22"
    );

  } else {

    bool alert = (t >= AMBANG_SUHU_C);

    digitalWrite(
      PIN_LED_ALERT,
      alert ? HIGH : LOW
    );

    kirimTelemetriJson(t, h, alert);
  }

#else

  // ----------------------------------------
  // MODE NON-BLOCKING
  // ----------------------------------------

  unsigned long sekarang = millis();

  if (sekarang - waktuBacaTerakhir >=
      INTERVAL_TELEMETRI_MS) {

    waktuBacaTerakhir = sekarang;

    // Tes komunikasi Serial
    kirimEvent(
      "serial_test",
      "Serial masih aktif"
    );

    // Membaca sensor DHT22
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Pemeriksaan data sensor
    if (isnan(h) || isnan(t)) {

      kirimEvent(
        "sensor_error",
        "Gagal membaca DHT22"
      );

    } else {

      bool alert = (t >= AMBANG_SUHU_C);

      // Pengaturan LED otomatis
      digitalWrite(
        PIN_LED_ALERT,
        alert ? HIGH : LOW
      );

      // Mengirim data JSON
      kirimTelemetriJson(t, h, alert);
    }
  }

#endif

}