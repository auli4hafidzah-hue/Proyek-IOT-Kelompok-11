#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

/*
 * =====================================================================
 * Proyek IoT Kelompok 11
 * THERMOSENSE : Sistem Monitoring Suhu dan Kelembapan Ruangan
 * Berbasis ESP32 dengan Indikator LED Otomatis
 * =====================================================================
 * Pinout:
 * - DHT22 Data        : GPIO 4
 * - LED Indikator     : GPIO 2 (via resistor 220 Ohm)
 * - Pushbutton Darurat: GPIO 15 (INPUT_PULLUP)
 * =====================================================================
 */

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_LED_ALERT 2
#define PIN_TOMBOL_DARURAT 15

// Ubah ke 0 untuk menguji mode BLOCKING (delay), 1 untuk NON-BLOCKING (millis)
#define MODE_NON_BLOCKING 1

DHT dht(PIN_DHT, TIPE_DHT);
unsigned long waktuBacaTerakhir = 0;
unsigned long intervalTelemetri = 2000; // Default 2000 ms (500 ms saat mode siaga cepat)
const float AMBANG_SUHU_C = 32.0;
unsigned long nomorPaket = 0;
bool statusTombolSebelumnya = HIGH;
bool modeSiagaCepat = false;

void kirimTelemetriJson(float suhu, float kelembapan, bool alertSuhu) {
  JsonDocument doc;
  doc["device_id"] = "esp32-thermosense-kelompok11";
  doc["seq"] = ++nomorPaket;
  doc["uptime_ms"] = millis();
  doc["mode"] = MODE_NON_BLOCKING ? "non-blocking" : "blocking";
  doc["interval_ms"] = intervalTelemetri;
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
  Serial.println("{\"event\":\"boot\",\"msg\":\"THERMOSENSE ESP32 Kelompok 11 Aktif\"}");
}

void loop() {
  // 1. Pemeriksaan input tombol darurat secara kontinu
  bool statusTombol = digitalRead(PIN_TOMBOL_DARURAT);
  if (statusTombol == LOW && statusTombolSebelumnya == HIGH) {
    modeSiagaCepat = !modeSiagaCepat;
    intervalTelemetri = modeSiagaCepat ? 500 : 2000; // Fitur C6: toggle 500ms siaga / 2000ms normal

    Serial.printf("{\"event\":\"emergency_button\",\"pressed_at_ms\":%lu,\"mode_siaga\":%s,\"interval_ms\":%lu}\n",
                  millis(), modeSiagaCepat ? "true" : "false", intervalTelemetri);
    digitalWrite(PIN_LED_ALERT, !digitalRead(PIN_LED_ALERT));
    delay(50); // Debounce singkat
  }
  statusTombolSebelumnya = statusTombol;

  // 2. Pembacaan sensor berkala: Mode Blocking vs Non-Blocking
#if MODE_NON_BLOCKING == 0
  delay(intervalTelemetri); // BLOCKING: CPU berhenti total selama interval!
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    bool alert = (t >= AMBANG_SUHU_C);
    digitalWrite(PIN_LED_ALERT, alert ? HIGH : LOW);
    kirimTelemetriJson(t, h, alert);
  }
#else
  unsigned long sekarang = millis();
  if (sekarang - waktuBacaTerakhir >= intervalTelemetri) {
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
