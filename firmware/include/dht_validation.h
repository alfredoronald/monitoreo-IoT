#ifndef MONITOREO_IOT_DHT_VALIDATION_H
#define MONITOREO_IOT_DHT_VALIDATION_H

#include <cmath>

namespace dht_validation {
constexpr float TEMPERATURE_MIN_C = 5.0f;
constexpr float TEMPERATURE_MAX_C = 35.0f;
constexpr float HUMIDITY_MIN_PERCENT = 10.0f;
constexpr float HUMIDITY_MAX_PERCENT = 90.0f;

struct ValidationResult {
  bool hasNaN;
  bool temperatureOutOfRange;
  bool humidityOutOfRange;

  bool isValid() const {
    return !hasNaN && !temperatureOutOfRange && !humidityOutOfRange;
  }
};

// Los límites son inclusivos; cualquier valor inválido descarta la pareja.
inline ValidationResult validate(float temperature, float humidity) {
  if (std::isnan(temperature) || std::isnan(humidity)) {
    return {true, false, false};
  }

  return {false,
          temperature < TEMPERATURE_MIN_C || temperature > TEMPERATURE_MAX_C,
          humidity < HUMIDITY_MIN_PERCENT || humidity > HUMIDITY_MAX_PERCENT};
}
}  // namespace dht_validation

#endif  // MONITOREO_IOT_DHT_VALIDATION_H
