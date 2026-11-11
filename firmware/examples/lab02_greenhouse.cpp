// Contoh Lab 02: Greenhouse Environmental Monitoring
#include <Arduino.h>
#include <DHT.h>

#define PIN_DHT 4
#define TIPE_DHT DHT22

DHT dht(PIN_DHT, TIPE_DHT);

void setup() {
  Serial.begin(115200);
  dht.begin();
  Serial.println("Greenhouse Monitoring Node Initialized");
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    Serial.printf("Temp: %.2f C, Humidity: %.2f %%\n", t, h);
  }
  delay(2000);
}

