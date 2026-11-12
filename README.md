# 🌡️ THERMOSENSE — Proyek IoT Kelompok 11
### Sistem Monitoring & Kendali Mikro-Iklim Greenhouse Berbasis ESP32

**Praktikum IoT: Laboratorium Jaringan Komputer & Sistem Tertanam — S1 Informatika FT UNIB**  
- **Minggu 1:** Toolchain PlatformIO, Simulator Wokwi, Git, & Telemetri Suhu DHT22
- **Minggu 2 (Lab-1):** GPIO Digital/Analog, ADC1 Terkalibrasi, Filter Moving Average, Kendali Histeresis Relai Pompa, Modulasi PWM Kipas LEDC, & Fitur Proteksi Dry-Run C6

---

## 📋 Deskripsi Proyek
**THERMOSENSE** adalah prototipe sistem kendali dan pemantauan iklim mikro (*greenhouse*) berbasis mikrokontroler **ESP32 DevKit V1**. Sistem ini mengintegrasikan:
1. **Sensor Kelembapan Tanah (Capacitive / Potensio pada GPIO 34 ADC1):** Membaca tegangan terkalibrasi pabrik `analogReadMilliVolts()`, disaring dengan *Moving Average Filter* (N=5), dan dikonversi menggunakan *inverse linear mapping* (0%–100%).
2. **Aktuator Pompa Irigasi (Relai pada GPIO 26):** Dikendalikan menggunakan algoritma **histeresis dua ambang (35%–60%)** guna mengeliminasi gejala *chattering* (lonjakan saklar on-off berlebih). Dilengkapi proteksi **Pump Run-Time Safety Cut-Off (C6)** yang mematikan pompa otomatis jika menyala >15 detik tanpa respons kenaikan air.
3. **Sensor Suhu & Kelembapan Udara (DHT22 pada GPIO 4):** Memantau iklim lingkungan mikro.
4. **Aktuator Kipas Pendingin (LED Biru PWM pada GPIO 18):** Kecepatan putaran diatur proporsional terhadap kenaikan suhu ($28^\circ\text{C} \to 0\%$ hingga $38^\circ\text{C} \to 100\%$) menggunakan perangkat keras **LEDC PWM 5 kHz (8-bit)**.
5. **Tombol Override Manual (Pushbutton GPIO 15):** Berfungsi mengaktifkan/menonaktifkan pompa secara manual atau mereset status kegagalan pengaman dry-run.
6. **Serial Monitor Telemetri JSON:** Mengirimkan payload terstruktur (115200 baud) yang siap diintegrasikan ke broker MQTT atau edge gateway.

---

## 🔧 Antarmuka Perangkat Keras (Pinout)

| Komponen Perangkat Keras | Pin ESP32 DevKit V1 | Mode / Fungsi | Keterangan Teknis & Keamanan |
|---|---|---|---|
| **Sensor DHT22** (Data) | **GPIO 4** | Digital I/O | Pembacaan suhu (°C) dan kelembapan (%RH) udara |
| **Soil Moisture / Potensio** | **GPIO 34** | Analog Input (ADC1_CH6) | Aman dari konflik radio Wi-Fi, pembacaan mV terkalibrasi eFuse |
| **Modul Relai Pompa + LED Hijau** | **GPIO 26** | Digital Output | Penggerak aktuator pompa irigasi dengan histeresis |
| **Emulator Kipas (LED Biru)** | **GPIO 18** | Hardware PWM (LEDC) | Frekuensi 5 kHz, resolusi 8-bit (0–255 duty cycle) |
| **Pushbutton Manual Override** | **GPIO 15** | Input (`INPUT_PULLUP`) | Tombol manual override dan reset proteksi dry-run |
| **UART0 Serial** | **TX0 / RX0** | Serial Bus (115200 baud) | Pengiriman payload telemetri JSON kontinu |

---

## 📁 Struktur Direktori Standar Arsitektur IoT

```
THERMOSENSE/
├── firmware/                   # Kode program mikrokontroler & simulasi
│   ├── .pio/                   # Artefak build kompilasi lokal (di-ignore Git)
│   ├── examples/               # Pustaka contoh kode praktikum
│   │   ├── lab01_blocking_nonblocking.cpp
│   │   ├── lab02_greenhouse.cpp
│   │   ├── lab03_deepsleep.cpp
│   │   └── lab03_i2c_scanner.cpp
│   ├── src/
│   │   └── main.cpp            # Program utama THERMOSENSE (Lab-1 + C6 Safety)
│   ├── diagram.json            # Skematik rangkaian virtual Wokwi Lab-1
│   ├── platformio.ini          # Konfigurasi PlatformIO & library
│   └── wokwi.toml              # Pemetaan biner firmware ke simulator Wokwi
├── edge-gateway/               # Modul gateway (Edge processing)
├── backend-api/                # Layanan REST API / MQTT subscriber
├── docker/                     # Konfigurasi kontainerisasi deployment
├── docs/                       # Dokumentasi praktikum
│   └── laporan-lab01-minggu-02.md # Laporan Analisis Lengkap Lab-1 (C1-C6)
├── .gitignore                  # Berkas pengecualian Git
└── README.md                   # Panduan proyek
```

---

## 🖥️ Panduan Menjalankan Simulasi Wokwi di VS Code

Berikut adalah panduan menjalankan simulasi proyek menggunakan Wokwi di VS Code:

### Langkah 1: Pastikan Ekstensi Terpasang & Lisensi Aktif
1. Pastikan ekstensi **PlatformIO IDE** dan **Wokwi Simulator** sudah terpasang di VS Code.
2. Jika baru pertama kali menggunakan Wokwi:
   - Tekan `F1` atau `Ctrl+Shift+P`
   - Ketik dan pilih: `Wokwi: Request a New License`
   - Ikuti tautan di browser untuk mengaktifkan lisensi komunitas gratis.

### Langkah 2: Lakukan Build Firmware (Wajib!)
Simulator Wokwi membutuhkan file biner hasil kompilasi:
1. Buka folder proyek di VS Code.
2. Buka berkas `firmware/src/main.cpp`.
3. Klik ikon **Build (Centang ✓)** di bilah status bawah PlatformIO (atau buka terminal di folder `firmware` lalu jalankan `pio run`).
4. Tunggu hingga proses kompilasi berstatus `[SUCCESS]`.
   *(File `.pio/build/esp32doit-devkit-v1/firmware.bin` akan terbentuk).*

### Langkah 3: Jalankan Simulasi Wokwi
1. Buka berkas `firmware/diagram.json`.
2. Klik tombol hijau **Start Simulation (Play)** di pojok kanan atas tampilan diagram Wokwi, atau tekan `F1` lalu pilih `Wokwi: Start Simulator`.
3. Terminal **Serial Monitor Wokwi** akan otomatis terbuka dan menampilkan keluaran JSON setiap 2 detik.

---

## 📊 Contoh Keluaran Serial Monitor (JSON Telemetry Lab-1)

```json
{"event":"boot","lab":"Lab-01 Greenhouse Kopi UNIB"}
{"device_id":"esp32-kopi-gh01","seq":1,"adc_raw":3288,"soil_mv_avg":2650,"soil_pct":"0.0","temp_c":"26.5","air_hum_pct":"75.0","pump_on":true,"relay_switches":1,"fan_pwm_pct":"0.0","ctrl_mode":"HYSTERESIS","fault":"NONE"}
{"device_id":"esp32-kopi-gh01","seq":2,"adc_raw":2325,"soil_mv_avg":1875,"soil_pct":"50.0","temp_c":"31.5","air_hum_pct":"68.0","pump_on":true,"relay_switches":1,"fan_pwm_pct":"34.9","ctrl_mode":"HYSTERESIS","fault":"NONE"}
{"device_id":"esp32-kopi-gh01","seq":3,"adc_raw":1940,"soil_mv_avg":1565,"soil_pct":"70.0","temp_c":"35.2","air_hum_pct":"60.0","pump_on":false,"relay_switches":2,"fan_pwm_pct":"71.8","ctrl_mode":"HYSTERESIS","fault":"NONE"}
{"event":"mode_change","manual_override":true}
{"event":"alarm","fault":"DRY_RUN_PROTECTION","msg":"Pompa dihentikan otomatis: tandon habis / selang bocor / sensor terlepas!"}
```

---

## 👥 Tim Pengembang (Kelompok 11)
- **Prodi:** S1 Informatika — Fakultas Teknik Universitas Bengkulu
- **Mata Kuliah:** Teknologi Internet of Things (IoT)

| Nama | NIM |
|---|---|
| Azka Fathiya | G1A024004 |
| Naifah Amanda Noveni | G1A024017 |
| Atikah Putri Utami | G1A024027 |
| Aulia Afdhal Hafidzah | G1A024029 |

