# Proyek IOT Kelompok 11

**THERMOSENSE** : Sistem Monitoring Suhu dan Kelembapan Ruangan Berbasis ESP32 dengan Indikator LED Otomatis

## Deskripsi

Sistem monitoring suhu dan kelembapan ruangan menggunakan mikrokontroler ESP32 dan sensor DHT22. Data suhu dan kelembapan ditampilkan pada Serial Monitor, dan LED indikator menyala otomatis ketika suhu melebihi ambang batas (32°C).

## Komponen

| Komponen | Pin ESP32 |
|---|---|
| ESP32 DevKit v4 | - |
| Sensor DHT22 | GPIO 15 (data), 3V3, GND |
| LED Merah | GPIO 2 (anoda), GND via resistor 220Ω |

## Cara Kerja

1. Sensor DHT22 membaca suhu dan kelembapan ruangan setiap 2 detik
2. Data ditampilkan di Serial Monitor
3. Jika suhu **≥ 32°C**, LED menyala (indikator suhu tinggi)
4. Jika suhu **< 32°C**, LED mati (suhu normal)

## Simulasi

Project ini dibuat dan disimulasikan di [Wokwi](https://wokwi.com/projects/479837929987968001):

- `diagram.json` — skematik rangkaian (ESP32 + DHT22 + LED)
- `src/sketch.ino` — kode program Arduino
- `libraries.txt` — daftar library yang dibutuhkan (DHT sensor library for ESPx)

## Library yang dibutuhkan

- [DHT sensor library for ESPx](https://github.com/beegee-tokyo/DHTesp)

## Anggota Kelompok 11

| Nama | NIM |
|---|---|
| Azka Fathiya | G1A024004 |
| Naifah Amanda Noveni | G1A024017 |
| Atikah Putri Utami | G1A024027 |
| Aulia Afdhal Hafidzah | G1A024029 |