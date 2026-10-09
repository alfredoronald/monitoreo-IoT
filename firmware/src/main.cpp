// ============================================================
// Monitoreo IoT - Firmware ESP32
// Archivo inicial: solo la estructura mínima de Arduino.
// El desarrollo (DHT22, MQ-135, MQTT) se hace en la task de
// firmware, rama feature/firmware-esp32.
// ============================================================
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println("Firmware ESP32 iniciado");
}

void loop() {
}
