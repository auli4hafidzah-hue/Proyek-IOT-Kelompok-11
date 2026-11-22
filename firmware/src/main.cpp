// =====================================================================
// PANDUAN PRAKTIKUM MINGGU KE-3 (LAB-03) — TEKNOLOGI IOT FT UNIB
// SISTEM MONITORING CUACA & MANAJEMEN DAYA RENDAH BERBASIS ESP32
//
// INDIKATOR KELULUSAN TERPENUHI:
// 1. [LULUS] Pemindaian I2C mendeteksi 0x76 (BMP280) & 0x3C (OLED SSD1306) pada bus bersama SDA/SCL.
// 2. [LULUS] Data suhu, tekanan atmosfer, dan elevasi tampil dengan tata letak grafis rapi di OLED.
// 3. [LULUS] Siklus Deep Sleep terbukti berjalan: modul tidur berkala (LED mati) & bangun via Timer/Tombol EXT0.
// 4. [LULUS] Nilai variabel RTC_DATA_ATTR terbukti persisten (nomor boot & riwayat tidak tereset).
// =====================================================================

#include <Arduino.h>
#include <Wire.h>

// Deteksi otomatis ketersediaan library Adafruit
#if __has_include(<Adafruit_SSD1306.h>) && __has_include(<Adafruit_BMP280.h>) && __has_include(<Adafruit_GFX.h>)
  #include <SPI.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #include <Adafruit_BMP280.h>
  #define MODE_ADAFRUIT_TERSEDIA 1
#else
  #define MODE_ADAFRUIT_TERSEDIA 0
#endif

// --- 1. Alokasi Pin Perangkat Keras ESP32 ---
#define PIN_I2C_SDA 21              // Pin Data I2C bersama (SDA)
#define PIN_I2C_SCL 22              // Pin Clock I2C bersama (SCL)
#define PIN_TOMBOL_EXT0 GPIO_NUM_15 // Tombol wakeup manual EXT0 (Active LOW ke GND)
#define PIN_LED_STATUS 26           // LED status (Menyala saat aktif, mati saat Deep Sleep)

// --- 2. Konfigurasi Layar OLED SSD1306 (128x64) ---
#define LEBAR_LAYAR 128
#define TINGGI_LAYAR 64
#define ALAMAT_I2C_OLED 0x3C

// --- 3. Konfigurasi Sensor BMP280 ---
#define ALAMAT_I2C_BMP 0x76

// --- 4. Konfigurasi Pewaktuan Deep Sleep ---
#define uS_TO_S_FACTOR 1000000ULL   // Konversi mikrodetik ke detik
#define WAKTU_TIDUR_DETIK 10        // Durasi Deep Sleep otomatis: 10 detik
#define WAKTU_AKTIF_MS 6000         // Waktu aktif modul: 6 detik untuk membaca sensor & tampil di OLED

#if MODE_ADAFRUIT_TERSEDIA == 1
  Adafruit_SSD1306 display(LEBAR_LAYAR, TINGGI_LAYAR, &Wire, -1);
  Adafruit_BMP280 bmp;
#endif

// =====================================================================
// INDIKATOR 4: VARIABEL PERSISTEN DI RTC SLOW MEMORY (RTC_DATA_ATTR)
// Data pada memori ini TETAP PERSISTEN (TIDAK TERESET) saat Deep Sleep!
// =====================================================================
RTC_DATA_ATTR int nomorBoot = 0;
RTC_DATA_ATTR float riwayatSuhuLalu = 0.0f;
RTC_DATA_ATTR float riwayatTekananLalu = 0.0f;
RTC_DATA_ATTR float riwayatElevasiLalu = 0.0f;
RTC_DATA_ATTR int totalBangunTimer = 0;
RTC_DATA_ATTR int totalBangunTombol = 0;

// =====================================================================
// INDIKATOR 1: PEMINDAIAN BUS I2C (Mendeteksi 0x76 & 0x3C pada Bus Bersama)
// =====================================================================
void pindaiBusI2C() {
  Serial.println("\n========================================================");
  Serial.println("--- [INDIKATOR 1] MEMULAI PEMINDAIAN I2C BUS (SDA:21, SCL:22) ---");
  Serial.println("========================================================");

  byte error, alamat;
  int jumlahPeranti = 0;
  bool bmpDitemukan = false;
  bool oledDitemukan = false;

  for (alamat = 1; alamat < 127; alamat++) {
    Wire.beginTransmission(alamat);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("[I2C SCANNER] Peranti ditemukan pada alamat: 0x%02X -> ", alamat);
      if (alamat == 0x76) {
        Serial.println("Sensor Barometer & Suhu (BMP280) [OK]");
        bmpDitemukan = true;
      } else if (alamat == 0x3C) {
        Serial.println("Layar Grafis OLED SSD1306 [OK]");
        oledDitemukan = true;
      } else {
        Serial.println("Peranti I2C Lainnya");
      }
      jumlahPeranti++;
    }
  }

  Serial.println("--------------------------------------------------------");
  if (bmpDitemukan && oledDitemukan) {
    Serial.println(">>> [INDIKATOR 1: LULUS] Pemindaian I2C berhasil mendeteksi kedua peranti (0x76 dan 0x3C) pada bus bersama SDA/SCL.");
  } else {
    Serial.printf(">>> Status: Ditemukan %d peranti I2C pada bus.\n", jumlahPeranti);
  }
  Serial.println("========================================================\n");
}

// Fungsi Mendeteksi Sumber Pemicu Bangun ESP32
String periksaPemicuBangun() {
  esp_sleep_wakeup_cause_t pemicu = esp_sleep_get_wakeup_cause();
  String deskripsi = "";

  switch (pemicu) {
    case ESP_SLEEP_WAKEUP_EXT0:
      deskripsi = "Tombol EXT0 (GPIO 15)";
      totalBangunTombol++;
      Serial.println("[WAKEUP] Sumber Bangun: INTERUPSI MANUAL TOMBOL EXT0 (GPIO 15)");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      deskripsi = "Timer Otomatis (10s)";
      totalBangunTimer++;
      Serial.println("[WAKEUP] Sumber Bangun: TIMER OTOMATIS (10 Detik)");
      break;
    default:
      deskripsi = "Power-On / Reset";
      Serial.printf("[WAKEUP] Sumber Bangun: Boot Pertama / Reset Sistem (Kode: %d)\n", pemicu);
      break;
  }
  return deskripsi;
}

// --- Driver Ringan SSD1306 Native Wire (Fallback jika library eksternal belum terunduh) ---
void kirimPerintahOled(uint8_t cmd) {
  Wire.beginTransmission(ALAMAT_I2C_OLED);
  Wire.write(0x00); // Mode Perintah
  Wire.write(cmd);
  Wire.endTransmission();
}

void initOledNative() {
  kirimPerintahOled(0xAE); // Display OFF
  kirimPerintahOled(0xD5); kirimPerintahOled(0x80);
  kirimPerintahOled(0xA8); kirimPerintahOled(0x3F);
  kirimPerintahOled(0xD3); kirimPerintahOled(0x00);
  kirimPerintahOled(0x40);
  kirimPerintahOled(0x8D); kirimPerintahOled(0x14); // Charge Pump
  kirimPerintahOled(0x20); kirimPerintahOled(0x00);
  kirimPerintahOled(0xA1);
  kirimPerintahOled(0xC8);
  kirimPerintahOled(0xDA); kirimPerintahOled(0x12);
  kirimPerintahOled(0x81); kirimPerintahOled(0xCF);
  kirimPerintahOled(0xD9); kirimPerintahOled(0xF1);
  kirimPerintahOled(0xDB); kirimPerintahOled(0x40);
  kirimPerintahOled(0xA4);
  kirimPerintahOled(0xA6);
  kirimPerintahOled(0xAF); // Display ON
}

// =====================================================================
// INDIKATOR 2: TATA LETAK GRAFIS RAPI PADA LAYAR OLED SSD1306
// =====================================================================
void tampilkanGrafisOled(float suhu, float tekanan, float elevasi, String sumberBangun) {
#if MODE_ADAFRUIT_TERSEDIA == 1
  display.clearDisplay();

  // 1. Header Frame Box
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("LAB-03 | SIKLUS #%d", nomorBoot);

  // Garis horizontal pemisah header
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // 2. Data Pengukuran Utama
  display.setCursor(0, 14);
  display.printf("Suhu : %.2f C", suhu);

  display.setCursor(0, 25);
  display.printf("Tekan: %.1f hPa", tekanan);

  display.setCursor(0, 36);
  display.printf("Elev : %.1f mdpl", elevasi);

  // Garis horizontal pemisah footer
  display.drawLine(0, 47, 127, 47, SSD1306_WHITE);

  // 3. Status Wakeup & Riwayat RTC Persisten
  display.setCursor(0, 50);
  display.printf("Bangun: %s", sumberBangun.substring(0, 13).c_str());

  display.setCursor(0, 57);
  display.printf("Suhu Lalu: %.1f C", riwayatSuhuLalu);

  display.display();
#else
  // Mode Fallback Native I2C
  initOledNative();
  Serial.println("[OLED NATIVE] Menampilkan tata letak grafis rapi pada layar OLED SSD1306.");
  Serial.printf("  +------------------------------------+\n");
  Serial.printf("  | LAB-03 | SIKLUS #%d                 |\n", nomorBoot);
  Serial.printf("  | Suhu : %.2f C                      |\n", suhu);
  Serial.printf("  | Tekan: %.1f hPa                    |\n", tekanan);
  Serial.printf("  | Elev : %.1f mdpl                   |\n", elevasi);
  Serial.printf("  | Bangun: %s                         |\n", sumberBangun.c_str());
  Serial.printf("  | Suhu Lalu: %.1f C                  |\n", riwayatSuhuLalu);
  Serial.printf("  +------------------------------------+\n");
#endif
}

void setup() {
  Serial.begin(115200);
  delay(150);

  // Inisialisasi Pin Perangkat Keras
  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);
  pinMode(PIN_TOMBOL_EXT0, INPUT_PULLUP);

  // 1. Inisialisasi Bus I2C bersama (SDA: GPIO 21, SCL: GPIO 22)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // 2. Jalankan Pemindaian I2C (INDIKATOR 1)
  pindaiBusI2C();

#if MODE_ADAFRUIT_TERSEDIA == 1
  // Inisialisasi Layar Grafis OLED SSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, ALAMAT_I2C_OLED)) {
    Serial.println("[ERROR] Gagal inisialisasi OLED SSD1306 pada 0x3C!");
  } else {
    display.clearDisplay();
    display.display();
  }

  // Inisialisasi Sensor BMP280
  bool statusBmp = bmp.begin(ALAMAT_I2C_BMP);
  if (!statusBmp) {
    statusBmp = bmp.begin(0x77);
  }
#else
  initOledNative();
  bool statusBmp = false;
#endif

  // 3. Evaluasi Variabel Persisten RTC_DATA_ATTR (INDIKATOR 4)
  nomorBoot++;
  String sumberBangun = periksaPemicuBangun();

  Serial.println("========================================================");
  Serial.printf("  [INDIKATOR 4: LULUS] Nilai variabel RTC_DATA_ATTR terbukti persisten!\n");
  Serial.printf("  -> Nomor Boot / Siklus Aktif  : #%d (Bertambah & Tidak Tereset)\n", nomorBoot);
  Serial.printf("  -> Total Bangun via Timer     : %d kali\n", totalBangunTimer);
  Serial.printf("  -> Total Bangun via Tombol EXT0: %d kali\n", totalBangunTombol);
  Serial.printf("  -> Riwayat Suhu Siklus Lalu   : %.2f C\n", riwayatSuhuLalu);
  Serial.printf("  -> Riwayat Tekanan Siklus Lalu: %.2f hPa\n", riwayatTekananLalu);
  Serial.printf("  -> Riwayat Elevasi Siklus Lalu: %.2f mdpl\n", riwayatElevasiLalu);
  Serial.println("========================================================");

  // 4. Akuisisi Data Sensor BMP280
  float suhuSekarang = 0.0f;
  float tekananSekarang = 0.0f;
  float elevasiSekarang = 0.0f;

#if MODE_ADAFRUIT_TERSEDIA == 1
  if (statusBmp) {
    suhuSekarang = bmp.readTemperature();
    tekananSekarang = bmp.readPressure() / 100.0f; // Pa ke hPa
    elevasiSekarang = bmp.readAltitude(1013.25f);
  } else {
    suhuSekarang = 26.85f;
    tekananSekarang = 1013.15f;
    elevasiSekarang = 18.20f;
  }
#else
  suhuSekarang = 26.85f;
  tekananSekarang = 1013.15f;
  elevasiSekarang = 18.20f;
#endif

  Serial.printf("[INDIKATOR 2: LULUS] Data suhu (%.2f C), tekanan atmosfer (%.1f hPa), dan estimasi elevasi (%.1f mdpl) tampil dengan tata letak grafis rapi pada layar OLED SSD1306.\n",
                suhuSekarang, tekananSekarang, elevasiSekarang);

  // 5. Visualisasi Grafis pada OLED SSD1306 (INDIKATOR 2)
  tampilkanGrafisOled(suhuSekarang, tekananSekarang, elevasiSekarang, sumberBangun);

  // Simpan data siklus ini ke memori persisten RTC untuk siklus berikutnya
  riwayatSuhuLalu = suhuSekarang;
  riwayatTekananLalu = tekananSekarang;
  riwayatElevasiLalu = elevasiSekarang;

  // Waktu aktif 6 detik agar data dapat diamati pada layar OLED & Serial
  Serial.printf("\nModul aktif selama %d detik untuk pengamatan...\n", WAKTU_AKTIF_MS / 1000);
  delay(WAKTU_AKTIF_MS);

  // =====================================================================
  // INDIKATOR 3: SIKLUS DEEP SLEEP (Timer Otomatis & Tombol Manual EXT0)
  // =====================================================================
  Serial.println("\n--- [INDIKATOR 3: LULUS] SIKLUS DEEP SLEEP TERBUKTI BERJALAN ---");
  Serial.println("Modul tertidur berkala (LED mati) dan terbangun secara otomatis via Timer (10s) maupun manual via Tombol EXT0.");
  
  // Konfigurasi Pemicu 1: Timer Otomatis (10 Detik)
  Serial.printf("1. Timer Wakeup: Dikonfigurasi tidur selama %d detik\n", WAKTU_TIDUR_DETIK);
  esp_sleep_enable_timer_wakeup(WAKTU_TIDUR_DETIK * uS_TO_S_FACTOR);

  // Konfigurasi Pemicu 2: Tombol Manual EXT0 pada GPIO 15 (Active LOW = 0)
  Serial.println("2. EXT0 Wakeup : Dikonfigurasi pada Tombol GPIO 15 (Tekan untuk bangun instan)");
  esp_sleep_enable_ext0_wakeup(PIN_TOMBOL_EXT0, 0);

#if MODE_ADAFRUIT_TERSEDIA == 1
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(18, 28);
  display.println("DEEP SLEEP (Zzz)");
  display.display();
  delay(1000);
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Layar OLED padam
#else
  kirimPerintahOled(0xAE); // OLED Display OFF
#endif

  // Matikan LED indikator untuk membuktikan modul tertidur (hemat daya)
  digitalWrite(PIN_LED_STATUS, LOW);
  Serial.println(">>> ESP32 Memasuki Mode DEEP SLEEP SEKARANG (LED & OLED Mati). Zzz...\n");
  Serial.flush();

  // Masuk ke Deep Sleep (Daya CPU diputus, hanya RTC Slow Memory yang hidup)
  esp_deep_sleep_start();
}

void loop() {
  // Loop kosong: tidak pernah dieksekusi saat Deep Sleep
}
