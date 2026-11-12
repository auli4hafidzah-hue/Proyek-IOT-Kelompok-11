# LAPORAN PRAKTIKUM SISTEM IOT — MINGGU KE-2 (LAB-1)
## GPIO Digital/Analog, ADC1, PWM LEDC, Sensor DHT22, & Soil Moisture
**Mata Kuliah:** Teknologi Internet of Things (IoT)  
**Program Studi:** S1 Informatika — Fakultas Teknik Universitas Bengkulu  
**Dosen Pengampu / Tim Pengajar:** Laboratorium Jaringan Komputer & Sistem Tertanam  

---

### Identitas Kelompok 11
| Nama Anggota | NPM | Peran & Kontribusi |
|---|---|---|
| **Azka Fathiya** | G1A024004 | Analisis Karakterisasi ADC1 & Uji Kalibrasi Sensor Tanah |
| **Naifah Amanda Noveni** | G1A024017 | Penyusunan Arsitektur Firmware, Histeresis, & Fitur Proteksi C6 |
| **Atikah Putri Utami** | G1A024027 | Analisis Kendali Proporsional PWM Kipas & Perhitungan Matematis |
| **Aulia Afdhal Hafidzah** | G1A024029 | Perancangan Skematik Wokwi Simulator & Pengujian Chattering |

---

## 1. Pendahuluan dan Tujuan Praktikum

Praktikum Lab-1 Minggu ke-2 berfokus pada implementasi subsistem edge controller untuk prototipe pengendali iklim mikro (*micro-climate control*) pada rumah kaca (*greenhouse*) tanaman kopi. Sistem bertugas memantau dua parameter lingkungan penting:
1. **Kelembapan Tanah (*Soil Moisture*):** Diukur menggunakan sensor analog kapasitif (atau potensiometer emulasi 0–3,3 V) yang terhubung ke ADC1 ESP32, untuk mengendalikan aktuator relai pompa irigasi.
2. **Suhu dan Kelembapan Udara:** Diukur menggunakan sensor digital DHT22 untuk mengendalikan aktuator kipas sirkulasi (*cooling fan*) secara proporsional berbasis perangkat keras PWM (LEDC).

### Tujuan Praktikum:
1. Merangkai antarmuka sensor digital (DHT22) dan sensor analog (*Capacitive Soil Moisture*) pada pin ESP32 yang aman dari konflik radio Wi-Fi (ADC1) dan bebas dari pin *strapping* boot.
2. Mengimplementasikan kalibrasi dua titik terbalik (*inverse linear mapping*) menggunakan fungsi terkalibrasi `analogReadMilliVolts()` dan penyaringan derau menggunakan *Moving Average Filter*.
3. Menganalisis dan membuktikan secara empiris keunggulan algoritma kendali **histeresis dua ambang** (35%–60%) dibandingkan ambang tunggal (45%) dalam menekan fenomena *chattering* pada relai pompa.
4. Mengendalikan kecepatan kipas pendingin secara halus menggunakan sinyal modulasi lebar pulsa (*Pulse Width Modulation* - LEDC hardware 5 kHz, resolusi 8-bit).
5. Mengembangkan algoritma pengaman kegagalan sistem (*Fail-Safe C6*): **Pump Run-Time Safety Cut-Off** untuk mencegah pompa terbakar saat terjadi kekeringan tandon air (*dry-run protection*).

---

## 2. Arsitektur Perangkat Keras dan Alokasi Pin GPIO ESP32

Pemilihan pin I/O pada mikrokontroler ESP32 DevKit V1 mengikuti aturan keamanan perangkat keras berikut:

| Nama Komponen | Pin Fisik ESP32 | Mode Konfigurasi | Keterangan Teknis & Keamanan |
|---|---|---|---|
| **Sensor DHT22 (Data)** | **GPIO 4** | Digital I/O Bidirectional | Aman untuk komunikasi satu-kabel (*single-bus*). |
| **Capacitive Soil Moisture / Potensio** | **GPIO 34 (ADC1_CH6)** | Analog Input-Only | Terletak pada blok **ADC1**, sehingga **tetap dapat berfungsi stabil saat koneksi Wi-Fi aktif**. Pin ini tidak memiliki sirkuit transistor *pull-up* internal. |
| **Pushbutton Override Manual** | **GPIO 15** | Digital Input (`INPUT_PULLUP`) | Menggunakan internal pull-up. Menekan tombol menghubungkan pin ke GND untuk memicu interupsi/perubahan mode. |
| **Modul Relai Pompa + LED Hijau** | **GPIO 26** | Digital Output (`OUTPUT`) | Mengendalikan modul relai optocoupler untuk pompa irigasi. Dipasang LED indikator hijau dengan resistor 220 Ω. |
| **Emulator Kipas PWM (LED Biru)** | **GPIO 18** | Periferal Hardware PWM (LEDC) | Frekuensi 5 kHz, resolusi 8-bit (0–255 duty cycle), aman dari konflik periferal internal. |
| **UART0 Serial Monitor** | **TX0 / RX0** | Serial Bus (115200 baud) | Mengirimkan telemetri status dalam format data terstruktur JSON. |

> **Catatan Keselamatan Pin:** GPIO 12 dihindari dari beban eksternal untuk mencegah salah pembacaan tegangan flash SPI saat masa booting ESP32. Pin ADC blok 2 (GPIO 0, 2, 4, 12–15, 25–27) dihindari dari pembacaan analog agar tidak terjadi *resource contention* dengan subsistem RF Wi-Fi.

---

## 3. Hasil Pengamatan dan Data Eksperimental

### 3.1 Tabel Pengamatan 3.1 — Karakterisasi Kuantisasi ADC1 (GPIO 34) & Pemetaan Kelembapan Tanah

*Konstanta Kalibrasi Terbalik:*  
- Tegangan Media Kering ($0\%$ Lengas): $MV\_TANAH\_KERING = 2650\text{ mV}$  
- Tegangan Media Basah Jenuh ($100\%$ Lengas): $MV\_TANAH\_BASAH = 1100\text{ mV}$  
- Rumus Pemetaan:
$$\text{soilPct} = \frac{MV\_TANAH\_KERING - V_{\text{terukur}}}{MV\_TANAH\_KERING - MV\_TANAH\_BASAH} \times 100\%$$

| No. Uji | Kondisi Input / Posisi Potensio | adc_raw (0–4095) | soil_mv_avg (mV) | soil_pct (%) | Status Relai Pompa | Keterangan Logika Kendali |
|:---:|---|:---:|:---:|:---:|:---:|---|
| **1** | Ujung Kering ($\sim 0\%$) | 3288 | 2650 | 0.0% | **ON (Menyala)** | Kelembapan di bawah ambang bawah ($< 35\%$), pompa aktif mengairi tanah. |
| **2** | Agak Kering ($\sim 30\%$) | 2712 | 2185 | 30.0% | **ON (Menyala)** | Nilai $30\% < 35\%$, pompa tetap menyala aktif. |
| **3** | Lembap Ideal ($\sim 50\%$) | 2325 | 1875 | 50.0% | **MEMPERTAHANKAN** | Berada di dalam *Deadband* ($35\% - 60\%$). Status pompa mempertahankan kondisi sebelumnya. |
| **4** | Basah Cukup ($\sim 70\%$) | 1940 | 1565 | 70.0% | **OFF (Mati)** | Nilai $\ge 60\%$, ambang batas atas histeresis tercapai, pompa dimatikan. |
| **5** | Jenuh Air ($\sim 100\%$) | 1362 | 1100 | 100.0% | **OFF (Mati)** | Nilai $100\% \ge 60\%$, kondisi tanah basah jenuh, pompa mati aman. |

---

### 3.2 Tabel Pengamatan 3.2 — Perbandingan Eksperimen Kendali Ambang Tunggal vs Histeresis (Uji Derau 30 Detik)

*Pengujian dilakukan dengan memberikan variasi input analog berfluktuasi cepat (derau riak $\pm 1,5\%$) di sekitar nilai ambang batas selama durasi pengujian 30 detik.*

| Skema Algoritma Kendali | Jumlah Klik Relai (`relay_switches`) | Analisis Dampak terhadap Umur Relai & Beban Jaringan MQTT |
|---|:---:|---|
| **Mode 0: Ambang Tunggal (45%)** | **34 kali** | **Kritis / Merusak:** Terjadi gejala *chattering* di mana relai berosilasi ON/OFF puluhan kali dalam 30 detik. Hal ini menimbulkan loncatan bunga api listrik (*arcing*) pada kontak tembaga bimetal relai, menaikkan temperatur kontak, dan membanjiri jalur komunikasi jaringan dengan lonjakan pesan telemetri status relai (*network flooding*). |
| **Mode 1: Histeresis (35%–60%)** | **1 kali** | **Optimal & Stabil:** Relai hanya bertransisi sekali ketika nilai benar-benar menyeberangi batas histeresis ($35\%$ untuk ON dan $60\%$ untuk OFF). Kontak mekanis relai awet, bebas gangguan transien, dan payload MQTT hanya dikirimkan pada kondisi perubahan state yang valid. |

---

## 4. Jawaban Analisis Komprehensif (Level Taksonomi Bloom C1 – C6)

### 4.1 [C1 – Mengingat] Karakteristik Fisik Pin GPIO 34 ESP32
**Pertanyaan:** Mengapa pin GPIO 34 pada ESP32 dikonfigurasi hanya dengan `pinMode(34, INPUT)` dan tidak boleh menggunakan `INPUT_PULLUP` maupun dijadikan pin keluaran relai?

**Jawaban:**  
Berdasarkan arsitektur perangkat keras *Espressif ESP32 Silicon Datasheet*, pin GPIO 34, 35, 36 (VP), dan 39 (VN) termasuk dalam kelompok pin **GPI (General Purpose Input)** atau *Input-Only Pins*.
1. **Ketiadaan Output Driver Buffer:** Pin GPIO 34 tidak memiliki transistor FET penggerak keluaran (*output push-pull buffer stage*). Sirkuit silikon pin ini terhubung langsung ke multiplexer ADC tanpa jalur dari register output peripheral (`GPIO_OUT_REG`). Oleh karena itu, jika program menetapkan `pinMode(34, OUTPUT)` atau menjalankan `digitalWrite(34, HIGH)`, mikrokontroler secara fisik tidak mampu memasok arus keluar (*sinking/sourcing current*) untuk menggerakkan modul relai.
2. **Ketiadaan Matriks Resistor Pull-Up/Pull-Down Terintegrasi:** Berbeda dengan pin serbaguna lainnya (GPIO 0–33), silikon internal pada kelompok GPI tidak dilengkapi dengan jaringan resistor tarik ke atas (*pull-up*) maupun tarik ke bawah (*pull-down*). Akibatnya, konfigurasi `INPUT_PULLUP` pada GPIO 34 tidak memiliki efek perangkat keras apa pun, dan pin akan tetap berada dalam kondisi mengambang (*floating*) kecuali dipasangi resistor eksternal.

---

### 4.2 [C2 – Memahami] Perbandingan `analogRead()` vs `analogReadMilliVolts()` dan Non-Linearitas ADC ESP32
**Pertanyaan:** Jelaskan perbedaan nilai ukur pada ESP32 antara pemanggilan fungsi `analogRead(34)` dan fungsi `analogReadMilliVolts(34)`! Mengapa kurva ADC mentah ESP32 memiliki deviasi non-linear pada tegangan di bawah 150 mV dan di atas 2450 mV?

**Jawaban:**  
1. **Perbedaan Nilai Ukur:**
   - `analogRead(34)` mengembalikan nilai cacahan digital mentah (*raw digital counts*) berbasis konversi kuantisasi 12-bit tanpa kalibrasi, menghasilkan bilangan bulat antara $0$ hingga $4095$ ($2^{12} - 1$). Nilai ini sangat rentan terhadap variasi referensi tegangan silikon bawaan (*chip-to-chip Vref variations*) yang dapat berkisar antara $1,05\text{ V}$ hingga $1,15\text{ V}$.
   - `analogReadMilliVolts(34)` secara otomatis membaca nilai kuantisasi ADC dan mengalikannya dengan tabel kalibrasi bawaan chip yang tersimpan di memori non-volatile **eFuse Factory Calibration** (menggunakan metode *Two Point Values* atau *eFuse Vref Calibration* dari Espressif). Fungsi ini langsung menghasilkan nilai riil dalam satuan milivolt ($\text{mV}$), sehingga hasil pengukuran antar modul ESP32 seragam dan akurat.
2. **Penyebab Deviasi Non-Linearitas ADC ESP32:**
   - **Batas Bawah ($< 150\text{ mV}$):** Disebabkan oleh tegangan *input offset* pada komparator arsitektur *Successive Approximation Register* (SAR) ADC internal serta adanya *dead-zone* transistor diferensial masukan. Sinyal yang mendekati tegangan ground ($0\text{ V}$) tidak dapat diresolusikan dengan tepat dan kerap kali terbaca $0$ count meskipun terdapat tegangan kecil riil sebesar $50–100\text{ mV}$.
   - **Batas Atas ($> 2450\text{ mV}$ pada atenuasi 11 dB):** Terjadi saturasi pada jaringan atenuasi kapasitif (*gain compression*) saat mendekati batas rel tegangan saturasi komparator internal. Akibatnya, tegangan di atas $2,5\text{ V}$ hingga $3,3\text{ V}$ memiliki kemiringan respons (*slope*) yang mendatar, sehingga pembacaan count melonjak ke nilai maksimum ($4095$) sebelum tegangan input mencapai $3,3\text{ V}$.

---

### 4.3 [C3 – Menerapkan] Perhitungan Manual Duty Cycle PWM dan Voltase Efektif ($V_{\text{eff}}$)
**Pertanyaan:** Berdasarkan data Tabel 3.1 Anda, hitunglah nilai duty cycle PWM (dalam skala 0–255 dan dalam voltase efektif $V_{\text{eff}}$ dari 3,3 V) ketika suhu DHT22 terbaca tepat $31,5^\circ\text{C}$ dan $35,2^\circ\text{C}$! Bandingkan hasil hitungan tangan Anda dengan nilai `fan_pwm_pct` di Serial Monitor!

**Jawaban:**  
Fungsi transfer modulasi PWM kipas pada kode program dikonfigurasi menggunakan fungsi pemetaan:
$$\text{nilaiDuty} = \text{map}\left(\text{round}(T \times 10), 280, 380, 0, 255\right)$$
dengan rentang suhu $28,0^\circ\text{C}$ (Duty 0) hingga $38,0^\circ\text{C}$ (Duty 255).  
Persentase kecepatan kipas:
$$\text{fan\_pwm\_pct} = \frac{\text{nilaiDuty}}{255} \times 100\%$$

#### Kasus A: Suhu $T = 31,5^\circ\text{C}$
1. **Perhitungan Duty Cycle:**
   $$\text{Input map} = 31,5 \times 10 = 315$$
   $$\text{nilaiDuty} = \frac{315 - 280}{380 - 280} \times (255 - 0) + 0 = \frac{35}{100} \times 255 = 89,25 \xrightarrow{\text{integer}} \mathbf{89}$$
2. **Persentase PWM (`fan_pwm_pct`):**
   $$\text{fan\_pwm\_pct} = \frac{89}{255} \times 100\% = \mathbf{34,9\%}$$
   *(Cocok persis dengan nilai keluaran telemetri JSON di Serial Monitor: `"fan_pwm_pct":"34.9"`).*
3. **Perhitungan Voltase Keluaran:**
   - **Tegangan Rata-rata DC ($V_{\text{avg}}$):**
     $$V_{\text{avg}} = V_{\text{cc}} \times \left(\frac{\text{nilaiDuty}}{255}\right) = 3,3\text{ V} \times 0,349 = \mathbf{1,15\text{ V}}$$
   - **Tegangan Efektif RMS ($V_{\text{eff}}$ untuk beban daya resistif/motor):**
     $$V_{\text{eff}} = V_{\text{cc}} \times \sqrt{\frac{\text{nilaiDuty}}{255}} = 3,3\text{ V} \times \sqrt{0,349} = 3,3 \times 0,5908 = \mathbf{1,95\text{ V}}$$

#### Kasus B: Suhu $T = 35,2^\circ\text{C}$
1. **Perhitungan Duty Cycle:**
   $$\text{Input map} = 35,2 \times 10 = 352$$
   $$\text{nilaiDuty} = \frac{352 - 280}{380 - 280} \times 255 = \frac{72}{100} \times 255 = 183,6 \xrightarrow{\text{integer}} \mathbf{183}$$
2. **Persentase PWM (`fan_pwm_pct`):**
   $$\text{fan\_pwm\_pct} = \frac{183}{255} \times 100\% = \mathbf{71,8\%}$$
   *(Cocok persis dengan nilai keluaran telemetri JSON di Serial Monitor: `"fan_pwm_pct":"71.8"`).*
3. **Perhitungan Voltase Keluaran:**
   - **Tegangan Rata-rata DC ($V_{\text{avg}}$):**
     $$V_{\text{avg}} = 3,3\text{ V} \times 0,7176 = \mathbf{2,37\text{ V}}$$
   - **Tegangan Efektif RMS ($V_{\text{eff}}$):**
     $$V_{\text{eff}} = 3,3\text{ V} \times \sqrt{0,7176} = 3,3 \times 0,8471 = \mathbf{2,80\text{ V}}$$

---

### 4.4 [C4 – Menganalisis] Analisis Filter Moving Average dan Kompromi Ukuran Sampel ($N$)
**Pertanyaan:** Analisislah cara kerja fungsi `bacaSoilMilliVoltsTerfilter()` pada kode di atas! Apa kompromi (*trade-off*) yang terjadi apabila konstanta `UKURAN_FILTER` diperbesar dari $N = 5$ menjadi $N = 60$ sampel pada interval baca 1000 ms?

**Jawaban:**  
1. **Prinsip Kerja Buffer Geser Lingkar (*Circular Ring Buffer*):**  
   Fungsi mengimplementasikan filter lolos-rendah digital (*Finite Impulse Response Moving Average*):
   - Sebuah array statis `bufferMv[UKURAN_FILTER]` bertindak sebagai memori penyimpan sampel.
   - Variabel `indeksFilter` mencatat slot penulisan berikutnya yang selalu dimajukan dengan operasi modulo `(indeksFilter + 1) % UKURAN_FILTER`.
   - Ketika buffer telah penuh terisi (`filterPenuh == true`), fungsi menjumlahkan $N$ sampel data terakhir dan membaginya dengan $N$, sehingga menghasilkan estimasi rata-rata sinyal tanpa beban overhead pergeseran memori (*zero memory-shift overhead*).
2. **Analisis Kompromi (*Trade-off*) jika $N$ diubah dari $5$ ke $60$ (Interval $1000\text{ ms}$):**
   - **Keuntungan (Noise Attenuation):** Derau frekuensi tinggi yang berasal dari interferensi jalur catu daya, sinyal induksi elektromagnetik pompa, dan riak transien ADC teredam sangat kuat. Kurva kelembapan tanah menjadi sangat mulus (*ultra-stable*).
   - **Kerugian Kritis (Phase Lag / Latensi Respons):** Waktu respons sistem terhadap perubahan fisik (*step response settling time*) meningkat secara drastis dari **$5\text{ detik}$** ($5 \times 1000\text{ ms}$) menjadi **$60\text{ detik}$ (1 menit penuh)**.
   - **Dampak Agronomi & Kendali:** Karakteristik kapilaritas air di tanah saat irigasi memerlukan deteksi tepat waktu. Jika sistem mengalami *delay* respons selama 60 detik, pompa akan terus menyemburkan air selama 1 menit meskipun tanah di sekitar perakaran sebenarnya sudah basah jenuh. Hal ini menyebabkan pemborosan air, genangan (*waterlogging*), dan penurunan efisiensi energi.

---

### 4.5 [C5 – Mengevaluasi] Evaluasi Kerusakan Mekanis Kontak Relai dan Desain Celah Histeresis Optimal
**Pertanyaan:** Berdasarkan bukti kuantitatif Tabel 3.2, evaluasilah apa yang akan terjadi pada kuota pesan cloud dan kontak mekanis relai pompa air 220 V AC di kebun kopi jika pengembang menggunakan logika tanpa histeresis! Berapa lebar celah histeresis (*deadband*) yang menurut tim Anda paling optimal untuk media tanam tanah?

**Jawaban:**  
1. **Dampak Pemakaian Logika Ambang Tunggal (Tanpa Histeresis):**
   - **Kerusakan Fisik Kontak Relai 220V AC:** Pompa air AC merupakan beban induktif tinggi (*highly inductive load*). Setiap kali relai terbuka atau tertutup (*chattering* 34 kali dalam 30 detik), terjadi pemutusan arus induktif yang memicu lonjakan tegangan balik (*back-EMF*) ribuan volt. Hal ini menimbulkan percikan bunga api listrik (*electrical arcing*) yang melelehkan permukaan kontak bimetal, menyebabkan kontak saling mengelas (*contact welding* / macet ON permanen), dan menimbulkan kebakaran modul relai dalam hitungan hari.
   - **Pemborosan Kuota Cloud / Broker MQTT:** Dalam skenario produksi IoT berbasis cloud (seperti AWS IoT, Adafruit IO, atau broker EMQX privat), setiap perubahan status relai memicu penerbitan paket telemetri atau peringatan (*event-driven MQTT publishing*). Chattering memicu ratusan pesan per menit, menyebabkan pemborosan paket data seluler IoT di perkebunan, pemblokiran akun akibat pelanggaran *rate limiting*, serta membebani antrean broker.
2. **Lebar Celah Histeresis (*Deadband*) Paling Optimal:**
   - **Rekomendasi Tim:** Celah histeresis sebesar **$25\%$ lengas** dengan batas aktif **$\text{ON} = 35\%$** dan batas nonaktif **$\text{OFF} = 60\%$** merupakan pengaturan paling optimal untuk media tanam tanah perkebunan kopi.
   - **Justifikasi Agronomi & Fisika Tanah:** Tanah kebun kopi memiliki konduktivitas hidrolik yang membutuhkan waktu untuk menyerap air dari permukaan ke kedalaman sensor. Titik layu permanen (*permanent wilting point*) tanaman kopi berada di sekitar kelembapan $30\%–35\%$, sedangkan kapasitas lapang (*field capacity*) berada di kisaran $60\%–65\%$. Menyalakan pompa pada $35\%$ mencegah cekaman kekeringan (*water stress*), dan mematikannya pada $60\%$ memastikan tanah tidak tergenang (*aerasi akar kopi tetap terjaga*), sekaligus memberikan jeda siklus irigasi yang panjang sehingga relai hanya beroperasi beberapa kali dalam satu hari.

---

### 4.6 [C6 – Mencipta] Implementasi Fitur Pengaman Waktu Jalan Maksimum Pompa (*Pump Run-Time Safety Cut-Off*)
**Pertanyaan:** Kembangkan kode `main.cpp` dengan menambahkan fitur Pengaman Waktu Jalan Maksimum Pompa (*Pump Run-Time Safety Cut-Off*): apabila pompa menyala terus-menerus selama lebih dari **15 detik** tetapi nilai `soil_pct` tidak kunjung naik (indikasi tandon air habis atau selang bocor/sensor terlepas dari tanah), sistem wajib mematikan pompa secara otomatis, mengunci status `"fault": "DRY_RUN_PROTECTION"`, dan menyalakan kedipan peringatan! Lampirkan kode dan tangkapan layar buktinya.

**Jawaban & Pembuktian Implementasi:**  
Fitur ini telah diintegrasikan langsung ke dalam [firmware/src/main.cpp](file:///c:/Users/NAIFAH%20AMANDA%20NOVENI/Downloads/Project_IOT_Kelompok_11_THERMOSENSE/firmware/src/main.cpp).

#### 1. Potongan Kode Logika Proteksi Run-Time:
```cpp
// Deteksi saat pompa pertama kali mulai menyala
if (statusPompa && !pompaSebelum) {
  waktuMulaiPompaJalan = sekarang;
  soilPctAwalPompa = soilPct;
}

// Evaluasi Pengaman Run-Time Cut-Off (Batas Maksimal 15 Detik)
if (statusPompa && !faultDryRun) {
  // Jika kelembapan tanah bertambah minimal 1.0%, reset waktu pengaman (air terbukti mengalir)
  if (soilPct >= (soilPctAwalPompa + 1.0f)) {
    waktuMulaiPompaJalan = sekarang;
    soilPctAwalPompa = soilPct;
  } 
  // Jika telah berjalan lebih dari 15 detik tanpa kenaikan kelembapan
  else if (sekarang - waktuMulaiPompaJalan >= 15000) {
    faultDryRun = true;           // Kunci status kegagalan
    statusPompa = false;          // Paksa pompa mati
    modeOverridePaksa = false;
    Serial.println("{\"event\":\"alarm\",\"fault\":\"DRY_RUN_PROTECTION\","
                   "\"msg\":\"Pompa dihentikan otomatis: tandon habis / selang bocor / sensor terlepas!\"}");
  }
}

// Mekanisme Kedipan Peringatan Visual (200 ms non-blocking)
if (faultDryRun) {
  if (sekarang - waktuKedipPeringatanTerakhir >= 200) {
    waktuKedipPeringatanTerakhir = sekarang;
    statusKedipPeringatan = !statusKedipPeringatan;
    digitalWrite(PIN_RELAY_POMPA, statusKedipPeringatan ? HIGH : LOW);
  }
}
```

#### 2. Mekanisme Pemulihan (*Fail-Safe Reset*):
Pengguna dapat mereset kondisi penguncian kesalahan dengan menekan tombol darurat kuning (GPIO 15). Sistem akan mendeteksi penekanan tombol, mengembalikan status `faultDryRun = false`, memadamkan kedipan peringatan, dan menerbitkan event JSON konfirmasi:
```json
{"event":"fault_reset","msg":"Penguncian DRY_RUN_PROTECTION di-reset oleh pengguna"}
```

---

## 5. Contoh Payload Telemetri JSON Terstruktur

Pada pengujian Serial Monitor (115200 baud), sistem menghasilkan format telemetri JSON kontinu yang siap diintegrasikan ke broker MQTT atau edge gateway:

### A. Kondisi Operasional Normal (Histeresis Aktif):
```json
{"event":"boot","lab":"Lab-01 Greenhouse Kopi UNIB"}
{"device_id":"esp32-kopi-gh01","seq":1,"adc_raw":3288,"soil_mv_avg":2650,"soil_pct":"0.0","temp_c":"26.5","air_hum_pct":"75.0","pump_on":true,"relay_switches":1,"fan_pwm_pct":"0.0","ctrl_mode":"HYSTERESIS","fault":"NONE"}
{"device_id":"esp32-kopi-gh01","seq":2,"adc_raw":2325,"soil_mv_avg":1875,"soil_pct":"50.0","temp_c":"31.5","air_hum_pct":"68.0","pump_on":true,"relay_switches":1,"fan_pwm_pct":"34.9","ctrl_mode":"HYSTERESIS","fault":"NONE"}
{"device_id":"esp32-kopi-gh01","seq":3,"adc_raw":1940,"soil_mv_avg":1565,"soil_pct":"70.0","temp_c":"35.2","air_hum_pct":"60.0","pump_on":false,"relay_switches":2,"fan_pwm_pct":"71.8","ctrl_mode":"HYSTERESIS","fault":"NONE"}
```

### B. Kondisi Terkunci Fault C6 (*Dry-Run Safety Cut-Off* Terpicu):
```json
{"event":"alarm","fault":"DRY_RUN_PROTECTION","msg":"Pompa dihentikan otomatis: tandon habis / selang bocor / sensor terlepas!"}
{"device_id":"esp32-kopi-gh01","seq":18,"adc_raw":3288,"soil_mv_avg":2650,"soil_pct":"0.0","temp_c":"32.0","air_hum_pct":"65.0","pump_on":false,"relay_switches":2,"fan_pwm_pct":"39.2","ctrl_mode":"FAULT_LOCKED","fault":"DRY_RUN_PROTECTION"}
```

---

## 6. Ceklis Evaluasi dan Kesimpulan

| No. | Indikator Unjuk Kerja Laboratorium | Status | Keterangan Evaluasi |
|:---:|---|:---:|---|
| 1 | Rangkaian terhubung pada pin aman (GPIO 34 ADC1, GPIO 4, 15, 18, 26). | **LULUS** | Seluruh pin bebas konflik Wi-Fi RF dan strapping boot. |
| 2 | Pembacaan sensor tanah terfilter (*Moving Average*) & pemetaan terbalik $0\%–100\%$. | **LULUS** | Kalibrasi dua titik ($2650\text{ mV}$ kering & $1100\text{ mV}$ basah) bekerja linier. |
| 3 | Demonstrasi eliminasi *chattering* relai via histeresis dan modulasi kecerahan PWM kipas. | **LULUS** | Chattering ditekan dari 34 klik menjadi 1 klik; PWM LEDC 5 kHz responsif terhadap suhu. |
| 4 | Fitur proteksi C6 (*Dry-Run Safety Cut-Off*) dan dokumentasi laporan pada repositori Git. | **LULUS** | Pompa auto cut-off saat 15 detik tanpa respons, alarm visual berkedip, ter-push ke cabang Git. |

**Kesimpulan:**  
Seluruh tujuan praktikum Lab-1 Minggu ke-2 berhasil dicapai dengan sempurna. Penggunaan histeresis dua ambang terbukti esensial dalam menjaga durabilitas aktuator fisik, filter moving average efektif meredam noise ADC ESP32, periferal perangkat keras LEDC mampu mengendalikan kecepatan aktuator kipas secara proporsional, dan algoritma pengaman C6 berhasil melindungi infrastruktur irigasi dari kerusakan akibat kegagalan pasokan air.
