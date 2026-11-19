#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>

/*
 * =====================================================================
 * PANDUAN PRAKTIKUM MINGGU KE-2 (LAB-1) â€” TEKNOLOGI IOT FT UNIB
 * Sistem Kendali Mikro-Iklim Greenhouse Kopi Berbasis ESP32
 * Sensor: DHT22 (Digital) & Capacitive Soil Moisture / Potensio (ADC1)
 * Aktuator: Relai Pompa (Histeresis) & LED PWM Kipas (LEDC 5 kHz)
 * Fitur Tambahan C6: Pump Run-Time Safety Cut-Off (Dry-Run Protection)
 * =====================================================================
 */

// --- Definisi Pin Aman ESP32 (Bebas Konflik ADC2 & Strapping GPIO 12) ---
#define PIN_DHT 4
#define TIPE_DHT DHT22
#define PIN_SOIL_ADC1 34       // Input-only ADC1_CH6 (Aman saat Wi-Fi aktif)
#define PIN_TOMBOL_MODE 15      // Pushbutton INPUT_PULLUP
#define PIN_RELAY_POMPA 26      // Output penggerak relai pompa & LED hijau
#define PIN_PWM_KIPAS 18        // Output PWM kecepatan kipas / LED biru

// =====================================================================
// SAKELAR KENDALI (Cukup ubah angka di sini untuk beralih mode):
// 1 = MODE NON-BLOCKING (millis) -> Responsif & Multitasking
// 0 = MODE BLOCKING (delay)     -> Memperlihatkan tombol/potensio macet
// =====================================================================
#define MODE_NON_BLOCKING 1

// Ubah ke 0 untuk melihat gejala CHATTERING (1 ambang), 1 untuk HISTERESIS (2 ambang)
#define GUNAKAN_HISTERESIS 0

// Konstanta Kalibrasi Dua Titik Sensor Tanah Kapasitif (dalam milivolt)
const int MV_TANAH_KERING = 2650; // Tegangan saat media kering (0% lengas)
const int MV_TANAH_BASAH = 1100;  // Tegangan saat jenuh air (100% lengas)

// Ambang Kendali Histeresis Pompa Irigasi (%) dan Rentang PWM Suhu Kipas (C)
const float AMBANG_POMPA_ON = 35.0f;
const float AMBANG_POMPA_OFF = 60.0f;
const float AMBANG_TUNGGAL = 45.0f; // Dipakai hanya jika GUNAKAN_HISTERESIS == 0

// Konfigurasi PWM Perangkat Keras LEDC (5 kHz, Resolusi 8-bit: 0 - 255)
const int PWM_FREQ_HZ = 5000;
const int PWM_RES_BIT = 8;
const int PWM_KANAL = 0;

// Buffer Moving Average (Jendela Geser N = 5 Sampel)
const int UKURAN_FILTER = 5;
uint32_t bufferMv[UKURAN_FILTER] = {0};
int indeksFilter = 0;
bool filterPenuh = false;

// Objek Sensor & Pewaktuan Non-Blocking
DHT dht(PIN_DHT, TIPE_DHT);
unsigned long waktuTelemetriTerakhir = 0;
unsigned long waktuKendaliTerakhir = 0;
unsigned long waktuBacaDHTTerakhir = 0;
unsigned long waktuDebounceTombol = 0;

const unsigned long INTERVAL_TELEMETRI_MS = 1000; // Pengiriman payload JSON ke Serial Monitor
const unsigned long INTERVAL_KENDALI_MS = 50;     // Sampling ADC & kendali pompa cepat (50 ms)
const unsigned long INTERVAL_BACA_DHT_MS = 2000;  // Interval baca DHT22 (sesuai spesifikasi sensor min 2 detik)

// Status Sistem
bool statusPompa = false;
bool modeOverridePaksa = false;
bool tombolSebelumnya = HIGH;
unsigned long seqPaket = 0;
unsigned long hitungTransisiRelai = 0;

// Cache Nilai Sensor
float suhuUdaraCache = 28.0f;
float lembapUdaraCache = 68.0f;
int adcRawTerakhir = 0;
uint32_t soilMvTerakhir = 0;
float soilPctTerakhir = 0.0f;

// --- Fitur C6: Pengaman Waktu Jalan Maksimum Pompa (Dry-Run Safety Cut-Off) ---
const unsigned long BATAS_WAKTU_DRY_RUN_MS = 15000; // 15 detik tanpa kenaikan kelembapan
unsigned long waktuMulaiPompaJalan = 0;
float soilPctMinPompa = 0.0f;
bool faultDryRun = false;
unsigned long waktuKedipPeringatanTerakhir = 0;
bool statusKedipPeringatan = false;

void setupPwmKipas() {
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
  ledcAttach(PIN_PWM_KIPAS, PWM_FREQ_HZ, PWM_RES_BIT);
#else
  ledcSetup(PWM_KANAL, PWM_FREQ_HZ, PWM_RES_BIT);
  ledcAttachPin(PIN_PWM_KIPAS, PWM_KANAL);
#endif
}

void tulisPwmKipas(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
  ledcWrite(PIN_PWM_KIPAS, duty);
#else
  ledcWrite(PWM_KANAL, duty);
#endif
}

void inisialisasiFilterSoil() {
  uint32_t mvAwal = analogReadMilliVolts(PIN_SOIL_ADC1);
  for (int i = 0; i < UKURAN_FILTER; i++) {
    bufferMv[i] = mvAwal;
  }
  indeksFilter = 0;
  filterPenuh = true;
}

uint32_t bacaSoilMilliVoltsTerfilter() {
  uint32_t mvMentah = analogReadMilliVolts(PIN_SOIL_ADC1);
  bufferMv[indeksFilter] = mvMentah;
  indeksFilter = (indeksFilter + 1) % UKURAN_FILTER;
  if (indeksFilter == 0) filterPenuh = true;

  int jumlahSampel = filterPenuh ? UKURAN_FILTER : indeksFilter;
  uint64_t total = 0;
  for (int i = 0; i < jumlahSampel; i++) total += bufferMv[i];
  return (uint32_t)(total / jumlahSampel);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SOIL_ADC1, INPUT);
  pinMode(PIN_TOMBOL_MODE, INPUT_PULLUP);
  pinMode(PIN_RELAY_POMPA, OUTPUT);
  digitalWrite(PIN_RELAY_POMPA, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL_ADC1, ADC_11db);

  inisialisasiFilterSoil();
  setupPwmKipas();
  dht.begin();
  Serial.println("{\"event\":\"boot\",\"lab\":\"Lab-01 Greenhouse Kopi UNIB\"}");
}

void loop() {
#if MODE_NON_BLOCKING == 1
  // =================================================================
  // >>> PILIHAN 1: MODE NON-BLOCKING (Menggunakan millis) <<<
  // Seluruh sistem responsif seketika, tombol & potensio tidak pernah macet
  // =================================================================
  unsigned long sekarang = millis();

  // 1. Pembacaan Tombol Override Manual Non-Blocking (Debounce 60 ms)
  bool bacaTombol = digitalRead(PIN_TOMBOL_MODE);
  if (bacaTombol == LOW && tombolSebelumnya == HIGH && (sekarang - waktuDebounceTombol > 60)) {
    waktuDebounceTombol = sekarang;

    // Jika sistem sedang terkunci fault C6, menekan tombol akan mereset status fault
    if (faultDryRun) {
      faultDryRun = false;
      digitalWrite(PIN_RELAY_POMPA, LOW);
      Serial.println("{\"event\":\"fault_reset\",\"msg\":\"Penguncian DRY_RUN_PROTECTION di-reset oleh tombol pengguna\"}");
    } else {
      modeOverridePaksa = !modeOverridePaksa;
      Serial.printf("{\"event\":\"mode_change\",\"manual_override\":%s}\n",
                    modeOverridePaksa ? "true" : "false");
    }
  }
  tombolSebelumnya = bacaTombol;

  // 2. Akuisisi Sensor Tanah & Evaluasi Kendali Pompa Cepat (Interval 50 ms)
  if (sekarang - waktuKendaliTerakhir >= INTERVAL_KENDALI_MS) {
    waktuKendaliTerakhir = sekarang;

    adcRawTerakhir = analogRead(PIN_SOIL_ADC1);
    soilMvTerakhir = bacaSoilMilliVoltsTerfilter();
    float soilPct = (float)(MV_TANAH_KERING - (int)soilMvTerakhir) * 100.0f
                  / (float)(MV_TANAH_KERING - MV_TANAH_BASAH);
    soilPct = constrain(soilPct, 0.0f, 100.0f);
    soilPctTerakhir = soilPct;

    // Pemulihan Otomatis Proteksi Dry-Run saat diputar ke arah basah
    if (faultDryRun && (soilPct >= AMBANG_POMPA_OFF || soilPct >= (soilPctMinPompa + 5.0f))) {
      faultDryRun = false;
      digitalWrite(PIN_RELAY_POMPA, LOW);
      Serial.println("{\"event\":\"fault_auto_cleared\",\"msg\":\"Potensiometer diputar ke arah basah, DRY_RUN_PROTECTION di-reset otomatis\"}");
    }

    // Evaluasi Kendali Pompa: Override Manual vs Histeresis vs Ambang Tunggal
    bool pompaSebelum = statusPompa;

    if (faultDryRun) {
      statusPompa = false;
    } else if (modeOverridePaksa) {
      statusPompa = true;
    } else {
#if GUNAKAN_HISTERESIS == 1
      if (soilPct < AMBANG_POMPA_ON) {
        statusPompa = true;
      } else if (soilPct >= AMBANG_POMPA_OFF) {
        statusPompa = false;
      }
#else
      statusPompa = (soilPct < AMBANG_TUNGGAL);
#endif
    }

    // Deteksi awal pompa menyala untuk inisialisasi pewaktu C6
    if (statusPompa && !pompaSebelum) {
      waktuMulaiPompaJalan = sekarang;
      soilPctMinPompa = soilPct;
    }

    // Evaluasi Fitur C6: Pengaman Waktu Jalan Maksimum Pompa (Dry-Run Protection > 15 detik)
    if (statusPompa && !faultDryRun) {
      if (soilPct < soilPctMinPompa) {
        soilPctMinPompa = soilPct;
      }
      if (soilPct >= (soilPctMinPompa + 1.0f)) {
        waktuMulaiPompaJalan = sekarang;
        soilPctMinPompa = soilPct;
      } else if (sekarang - waktuMulaiPompaJalan >= BATAS_WAKTU_DRY_RUN_MS) {
        faultDryRun = true;
        statusPompa = false;
        modeOverridePaksa = false;
        digitalWrite(PIN_RELAY_POMPA, LOW);
        Serial.println("{\"event\":\"alarm\",\"fault\":\"DRY_RUN_PROTECTION\",\"msg\":\"Pompa dihentikan otomatis: tandon habis / selang bocor / sensor terlepas!\"}");
      }
    }

    if (statusPompa != pompaSebelum) {
      hitungTransisiRelai++;
    }

    // Tulis status fisik pin relai (LED Hijau) bila tidak sedang alarm berkedip
    if (!faultDryRun) {
      digitalWrite(PIN_RELAY_POMPA, statusPompa ? HIGH : LOW);
    }
  }

  // 3. Kedipan Peringatan Alarm Visual Non-Blocking (Fitur C6)
  if (faultDryRun) {
    if (sekarang - waktuKedipPeringatanTerakhir >= 200) {
      waktuKedipPeringatanTerakhir = sekarang;
      statusKedipPeringatan = !statusKedipPeringatan;
      digitalWrite(PIN_RELAY_POMPA, statusKedipPeringatan ? HIGH : LOW);
    }
  }

  // 4. Pembacaan Sensor DHT22 & Kendali PWM Kipas (Interval 2000 ms)
  if (sekarang - waktuBacaDHTTerakhir >= INTERVAL_BACA_DHT_MS) {
    waktuBacaDHTTerakhir = sekarang;

    float suhu = dht.readTemperature();
    float lembap = dht.readHumidity();
    if (!isnan(suhu) && !isnan(lembap)) {
      suhuUdaraCache = suhu;
      lembapUdaraCache = lembap;
    }

    int nilaiDuty = map((long)(suhuUdaraCache * 10.0f), 280, 380, 0, 255);
    nilaiDuty = constrain(nilaiDuty, 0, 255);
    tulisPwmKipas((uint8_t)nilaiDuty);
  }

  // 5. Serialisasi Payload Telemetri JSON Terstruktur Berkala (1000 ms)
  if (sekarang - waktuTelemetriTerakhir >= INTERVAL_TELEMETRI_MS) {
    waktuTelemetriTerakhir = sekarang;

    int nilaiDuty = map((long)(suhuUdaraCache * 10.0f), 280, 380, 0, 255);
    nilaiDuty = constrain(nilaiDuty, 0, 255);
    float fanPct = (nilaiDuty * 100.0f) / 255.0f;

    JsonDocument doc;
    doc["device_id"] = "esp32-kopi-gh01";
    doc["seq"] = ++seqPaket;
    doc["mode_eksekusi"] = "non-blocking";
    doc["adc_raw"] = adcRawTerakhir;
    doc["soil_mv_avg"] = soilMvTerakhir;
    doc["soil_pct"] = serialized(String(soilPctTerakhir, 1));
    doc["temp_c"] = serialized(String(suhuUdaraCache, 1));
    doc["air_hum_pct"] = serialized(String(lembapUdaraCache, 1));
    doc["pump_on"] = statusPompa;
    doc["relay_switches"] = hitungTransisiRelai;
    doc["fan_pwm_pct"] = serialized(String(fanPct, 1));
    doc["ctrl_mode"] = faultDryRun ? "FAULT_LOCKED" :
                       (modeOverridePaksa ? "MANUAL" :
                       (GUNAKAN_HISTERESIS ? "HYSTERESIS" : "SINGLE_THR"));
    doc["fault"] = faultDryRun ? "DRY_RUN_PROTECTION" : "NONE";

    serializeJson(doc, Serial);
    Serial.println();
  }

#else
  // =================================================================
  // >>> PILIHAN 0: MODE BLOCKING (Menggunakan delay) <<<
  // Memperlihatkan secara nyata bagaimana delay() membuat tombol & potensio macet
  // =================================================================

  // 1. CPU DIPAKSA DIAM TOTAL 2 DETIK (BLOCKING!)
  // Selama 2 detik ini, tombol yang ditekan tidak akan terbaca dan putaran potensio tertahan
  delay(2000);

  // 2. Pembacaan Tombol Override Manual (Hanya sempat terbaca sesaat setelah delay selesai)
  bool bacaTombol = digitalRead(PIN_TOMBOL_MODE);
  if (bacaTombol == LOW && tombolSebelumnya == HIGH) {
    delay(60); // Debounce blocking tambahan
    modeOverridePaksa = !modeOverridePaksa;
    Serial.printf("{\"event\":\"mode_change\",\"manual_override\":%s}\n",
                  modeOverridePaksa ? "true" : "false");
  }
  tombolSebelumnya = bacaTombol;

  // 3. Pembacaan Potensiometer & Kendali Pompa
  adcRawTerakhir = analogRead(PIN_SOIL_ADC1);
  soilMvTerakhir = analogReadMilliVolts(PIN_SOIL_ADC1);
  float soilPct = (float)(MV_TANAH_KERING - (int)soilMvTerakhir) * 100.0f
                / (float)(MV_TANAH_KERING - MV_TANAH_BASAH);
  soilPct = constrain(soilPct, 0.0f, 100.0f);
  soilPctTerakhir = soilPct;

  bool pompaSebelum = statusPompa;
  if (modeOverridePaksa) {
    statusPompa = true;
  } else {
#if GUNAKAN_HISTERESIS == 1
    if (soilPct < AMBANG_POMPA_ON) {
      statusPompa = true;
    } else if (soilPct >= AMBANG_POMPA_OFF) {
      statusPompa = false;
    }
#else
    statusPompa = (soilPct < AMBANG_TUNGGAL);
#endif
  }

  if (statusPompa != pompaSebelum) {
    hitungTransisiRelai++;
  }
  digitalWrite(PIN_RELAY_POMPA, statusPompa ? HIGH : LOW);

  // 4. Pembacaan DHT22 & Kendali PWM Kipas
  float suhu = dht.readTemperature();
  float lembap = dht.readHumidity();
  if (!isnan(suhu) && !isnan(lembap)) {
    suhuUdaraCache = suhu;
    lembapUdaraCache = lembap;
  }
  int nilaiDuty = map((long)(suhuUdaraCache * 10.0f), 280, 380, 0, 255);
  nilaiDuty = constrain(nilaiDuty, 0, 255);
  tulisPwmKipas((uint8_t)nilaiDuty);
  float fanPct = (nilaiDuty * 100.0f) / 255.0f;

  // 5. Serialisasi Payload Telemetri JSON
  JsonDocument doc;
  doc["device_id"] = "esp32-kopi-gh01";
  doc["seq"] = ++seqPaket;
  doc["mode_eksekusi"] = "blocking";
  doc["adc_raw"] = adcRawTerakhir;
  doc["soil_mv_avg"] = soilMvTerakhir;
  doc["soil_pct"] = serialized(String(soilPctTerakhir, 1));
  doc["temp_c"] = serialized(String(suhuUdaraCache, 1));
  doc["air_hum_pct"] = serialized(String(lembapUdaraCache, 1));
  doc["pump_on"] = statusPompa;
  doc["relay_switches"] = hitungTransisiRelai;
  doc["fan_pwm_pct"] = serialized(String(fanPct, 1));
  doc["ctrl_mode"] = modeOverridePaksa ? "MANUAL" : (GUNAKAN_HISTERESIS ? "HYSTERESIS" : "SINGLE_THR");
  doc["fault"] = "NONE";

  serializeJson(doc, Serial);
  Serial.println();
#endif
}

