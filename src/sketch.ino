
#include <DHTesp.h>

#define DHT_PIN 15
#define LED_PIN 2

DHTesp dht;

const float BATAS_SUHU = 32.0;

unsigned long waktuSebelum = 0;
const unsigned long interval = 2000;

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inisialisasi sensor DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);

  Serial.println();
  Serial.println("================================");
  Serial.println("MONITORING SUHU DAN KELEMBAPAN");
  Serial.println("ESP32 + DHT22");
  Serial.println("================================");

  delay(2000);
}

void loop() {
  unsigned long sekarang = millis();

  if (sekarang - waktuSebelum >= interval) {
    waktuSebelum = sekarang;

    // Membaca data sensor
    TempAndHumidity data = dht.getTempAndHumidity();

    float suhu = data.temperature;
    float kelembapan = data.humidity;

    // Memeriksa apakah sensor berhasil membaca
    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("Gagal membaca DHT22!");
      Serial.println(dht.getStatusString());

      digitalWrite(LED_PIN, LOW);
      return;
    }

    // Menentukan status suhu
    bool suhuTinggi = suhu >= BATAS_SUHU;

    // Mengontrol LED
    digitalWrite(LED_PIN, suhuTinggi ? HIGH : LOW);

    // Menampilkan hasil
    Serial.println("--------------------------------");

    Serial.print("Suhu       : ");
    Serial.print(suhu, 1);
    Serial.println(" °C");

    Serial.print("Kelembapan : ");
    Serial.print(kelembapan, 1);
    Serial.println(" %");

    Serial.print("Status LED : ");

    if (suhuTinggi) {
      Serial.println("MENYALA - SUHU TINGGI");
    } else {
      Serial.println("MATI - SUHU NORMAL");
    }

    Serial.println("--------------------------------");
  }
}
