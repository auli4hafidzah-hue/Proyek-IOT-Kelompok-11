// Contoh Lab 03: Deep Sleep Mode ESP32 Power Optimization
#include <Arduino.h>

#define uS_TO_S_FACTOR 1000000ULL  // Faktor konversi mikrodetik ke detik
#define TIME_TO_SLEEP  10          // Waktu tidur dalam detik

RTC_DATA_ATTR int bootCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
}

void loop() {
  // Tidak pernah dieksekusi saat deep sleep
}

