# 🌡️ THERMOSENSE — Proyek IoT Kelompok 11
### Sistem Monitoring Suhu dan Kelembapan Ruangan Berbasis ESP32 dengan Indikator LED Otomatis

**Praktikum Minggu 1: Toolchain, Simulator Wokwi, & Git**  
*Laboratorium Jaringan Komputer & Sistem Tertanam — S1 Informatika FT UNIB*

---

## 📋 Deskripsi Proyek
**THERMOSENSE** adalah sistem pemantauan suhu dan kelembapan ruangan berbasis mikrokontroler **ESP32 DevKit V1** dan sensor **DHT22**. Sistem ini mengimplementasikan pembacaan data non-blocking menggunakan `millis()` dan serialisasi payload telemetri standar format **JSON**. Indikator **LED Merah** menyala otomatis saat suhu mencapai atau melebihi batas aman (**≥ 32°C**). Terdapat pula tombol darurat (**Pushbutton** pada GPIO 15) untuk menguji responsibilitas interupsi/perubahan mode siaga.

---

## 🔧 Antarmuka Perangkat Keras (Pinout)

| Komponen Perangkat Keras | Pin ESP32 DevKit V1 | Keterangan |
|---|---|---|
| **Sensor DHT22** (Data) | **GPIO 4** | Mengukur suhu (°C) dan kelembapan (%RH) |
| **Sensor DHT22** (VCC / GND) | 3V3 / GND.1 | Jalur daya 3.3 Volt |
| **LED Merah** (Alert) | **GPIO 2** | Anoda via resistor 220 Ω, Katoda ke GND.2 |
| **Pushbutton** (Darurat) | **GPIO 15** | Input `INPUT_PULLUP`, terminal lain ke GND.2 |
| **UART0 Serial** | TX0 / RX0 | Komunikasi serial telemetri JSON (115200 baud) |

---

## 📁 Struktur Direktori Standar Arsitektur IoT

```
THERMOSENSE/
├── firmware/                   # Kode program mikrokontroler & simulasi
│   ├── .pio/                   # Artefak build kompilasi lokal (di-ignore Git)
│   ├── examples/               # Contoh kode praktikum (blocking/non-blocking, dll)
│   │   ├── lab01_blocking_nonblocking.cpp
│   │   ├── lab02_greenhouse.cpp
│   │   ├── lab03_deepsleep.cpp
│   │   └── lab03_i2c_scanner.cpp
│   ├── src/
│   │   └── main.cpp            # Program utama THERMOSENSE
│   ├── diagram.json            # Skematik rangkaian virtual Wokwi
│   ├── platformio.ini          # Konfigurasi PlatformIO & library
│   └── wokwi.toml              # Pemetaan biner firmware ke simulator Wokwi
├── edge-gateway/               # Modul gateway (Edge processing)
├── backend-api/                # Layanan REST API / MQTT subscriber
├── docker/                     # Konfigurasi kontainerisasi deployment
├── docs/                       # Dokumentasi praktikum
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

## 📊 Contoh Keluaran Serial Monitor (JSON Telemetry)

```json
{"event":"boot","msg":"THERMOSENSE ESP32 Kelompok 11 Aktif"}
{"device_id":"esp32-thermosense-kelompok11","seq":1,"uptime_ms":2005,"mode":"non-blocking","interval_ms":2000,"temp_c":28.40,"hum_pct":65.20,"alert":false}
{"device_id":"esp32-thermosense-kelompok11","seq":2,"uptime_ms":4007,"mode":"non-blocking","interval_ms":2000,"temp_c":33.10,"hum_pct":70.00,"alert":true}
{"event":"emergency_button","pressed_at_ms":5120,"mode_siaga":true,"interval_ms":500}
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

