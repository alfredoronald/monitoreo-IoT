// ============================================================
// Monitoreo IoT - Firmware ESP32
// T-1.4: lectura del DHT22 con validación del rango del aula.
// ============================================================
#include <Arduino.h>
#include <DHT.h>
#include "dht_validation.h"

namespace {
constexpr uint8_t DHT_PIN = 23;
constexpr unsigned long DHT_READ_INTERVAL_MS = 2000UL;
using dht_validation::TEMPERATURE_MIN_C;
using dht_validation::TEMPERATURE_MAX_C;
using dht_validation::HUMIDITY_MIN_PERCENT;
using dht_validation::HUMIDITY_MAX_PERCENT;

DHT dht(DHT_PIN, DHT22);
unsigned long lastAttemptFinishedMs = 0;
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("Firmware ESP32 iniciado");
  dht.begin();
  lastAttemptFinishedMs = millis();
}

void loop() {
  const unsigned long attemptTimestampMs = millis();
  // La resta unsigned mantiene el intervalo cuando millis() desborda.
  if (attemptTimestampMs - lastAttemptFinishedMs < DHT_READ_INTERVAL_MS) {
    return;
  }

  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();  // Celsius por defecto.
  // Esperar el intervalo completo tras cada intento, incluso si falla.
  lastAttemptFinishedMs = millis();

  const auto validation = dht_validation::validate(temperature, humidity);
  if (validation.hasNaN) {
    Serial.printf("[%lu ms] DHT22 ERROR: lectura NaN; muestra descartada\n",
                  attemptTimestampMs);
    return;
  }

  if (!validation.isValid()) {
    if (validation.temperatureOutOfRange) {
      Serial.printf(
          "[%lu ms] DHT22 DESCARTADA: temperatura %.1f C fuera de [%.1f, %.1f] C\n",
          attemptTimestampMs, temperature, TEMPERATURE_MIN_C, TEMPERATURE_MAX_C);
    }
    if (validation.humidityOutOfRange) {
      Serial.printf(
          "[%lu ms] DHT22 DESCARTADA: humedad %.1f %% HR fuera de [%.1f, %.1f] %% HR\n",
          attemptTimestampMs, humidity, HUMIDITY_MIN_PERCENT,
          HUMIDITY_MAX_PERCENT);
    }
    return;
  }

  Serial.printf("[%lu ms] DHT22 OK: T=%.1f C; HR=%.1f %%\n",
                attemptTimestampMs, temperature, humidity);
}
