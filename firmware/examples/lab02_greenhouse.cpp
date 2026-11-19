// Contoh Lab 02: Greenhouse Environmental Monitoring & Control
#include <Arduino.h>
#include <DHT.h>

#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_SOIL_ADC1 34
#define PIN_RELAY_POMPA 26
#define PIN_PWM_KIPAS 18

// Ambang batas kendali tanah (%)
const float AMBANG_POMPA_ON = 35.0f;
const float AMBANG_POMPA_OFF = 60.0f;

const int MV_TANAH_KERING = 2650;
const int MV_TANAH_BASAH = 1100;

DHT dht(PIN_DHT, TIPE_DHT);
bool statusPompa = false;
unsigned long waktuBacaTerakhir = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SOIL_ADC1, INPUT);
  pinMode(PIN_RELAY_POMPA, OUTPUT);
  digitalWrite(PIN_RELAY_POMPA, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL_ADC1, ADC_11db);

  dht.begin();
  Serial.println("Greenhouse Monitoring & Control Initialized");
}

void loop() {
  // Pembacaan sensor analog kelembapan tanah (responsif tanpa blocking delay)
  uint32_t soilMv = analogReadMilliVolts(PIN_SOIL_ADC1);
  float soilPct = (float)(MV_TANAH_KERING - (int)soilMv) * 100.0f
                / (float)(MV_TANAH_KERING - MV_TANAH_BASAH);
  soilPct = constrain(soilPct, 0.0f, 100.0f);

  // Kendali Histeresis Pompa (LED Hijau)
  if (soilPct < AMBANG_POMPA_ON) {
    statusPompa = true;  // Tanah kering: nyalakan pompa & LED hijau
  } else if (soilPct >= AMBANG_POMPA_OFF) {
    statusPompa = false; // Tanah basah: matikan pompa & LED hijau saat diputar berlawanan
  }
  digitalWrite(PIN_RELAY_POMPA, statusPompa ? HIGH : LOW);

  // Pembacaan DHT22 & Cetak Status setiap 2000 ms
  unsigned long sekarang = millis();
  if (sekarang - waktuBacaTerakhir >= 2000) {
    waktuBacaTerakhir = sekarang;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    Serial.printf("[Sensor] Tanah: %.1f %% (%u mV) | Status Pompa: %s\n",
                  soilPct, soilMv, statusPompa ? "ON (LED Hijau HIDUP)" : "OFF (LED Hijau MATI)");
    if (!isnan(h) && !isnan(t)) {
      Serial.printf("[Sensor] Suhu: %.1f C, Kelembapan Udara: %.1f %%\n", t, h);
    }
  }

  delay(50); // Delay kecil untuk stabilitas pembacaan ADC dan respons instan
}
