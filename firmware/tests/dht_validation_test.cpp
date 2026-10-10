// T-1.4 / CA-02: PRUEBA CONTROLADA CON DATOS SINTÉTICOS.
// Ejecutable de escritorio independiente; no lee el DHT22 ni utiliza Arduino.
// Este archivo está fuera de src/ y no pertenece al firmware de producción.
// Requiere un compilador C++11 o posterior. No utiliza PlatformIO Test.
#include "../include/dht_validation.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
struct TestCase {
  const char* name;
  float temperature;
  float humidity;
  dht_validation::ValidationResult expected;
};
}  // namespace

int main() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  // La otra variable permanece dentro de rango para aislar cada límite.
  const TestCase cases[] = {
      {"temperatura 4.9", 4.9f, 50.0f, {false, true, false}},
      {"temperatura 5.0", 5.0f, 50.0f, {false, false, false}},
      {"temperatura 35.0", 35.0f, 50.0f, {false, false, false}},
      {"temperatura 35.1", 35.1f, 50.0f, {false, true, false}},
      {"humedad 9.9", 20.0f, 9.9f, {false, false, true}},
      {"humedad 10.0", 20.0f, 10.0f, {false, false, false}},
      {"humedad 90.0", 20.0f, 90.0f, {false, false, false}},
      {"humedad 90.1", 20.0f, 90.1f, {false, false, true}},
      {"NaN en temperatura", nan, 50.0f, {true, false, false}},
      {"NaN en humedad", 20.0f, nan, {true, false, false}},
      {"ambas fuera de rango", 35.1f, 90.1f, {false, true, true}},
      {"NaN en ambas", nan, nan, {true, false, false}},
  };

  std::puts("T-1.4 / CA-02 - DATOS SINTETICOS - NO SON MEDICIONES REALES");
  std::puts("Se comprueba la misma validacion utilizada por main.cpp.");
  std::printf("Limites oficiales: T=[%.1f, %.1f] C; HR=[%.1f, %.1f] %%\n",
              dht_validation::TEMPERATURE_MIN_C,
              dht_validation::TEMPERATURE_MAX_C,
              dht_validation::HUMIDITY_MIN_PERCENT,
              dht_validation::HUMIDITY_MAX_PERCENT);

  unsigned int total = 0;
  unsigned int failures = 0;
  for (const auto& test : cases) {
    ++total;
    const auto actual =
        dht_validation::validate(test.temperature, test.humidity);
    const bool passed =
        actual.isValid() == test.expected.isValid() &&
        actual.hasNaN == test.expected.hasNaN &&
        actual.temperatureOutOfRange == test.expected.temperatureOutOfRange &&
        actual.humidityOutOfRange == test.expected.humidityOutOfRange;
    if (!passed) {
      ++failures;
    }

    std::printf(
        "[SINTETICO] %s | %s | T=%.1f C; HR=%.1f %% | "
        "pareja esperada=%s; obtenida=%s\n",
        passed ? "PASS" : "FAIL", test.name, test.temperature, test.humidity,
        test.expected.isValid() ? "ACEPTADA" : "RECHAZADA",
        actual.isValid() ? "ACEPTADA" : "RECHAZADA");
    std::printf(
        "  Motivos obtenidos: NaN=%d; temperatura fuera=%d; humedad fuera=%d\n",
        static_cast<int>(actual.hasNaN),
        static_cast<int>(actual.temperatureOutOfRange),
        static_cast<int>(actual.humidityOutOfRange));
    if (!passed) {
      std::printf(
          "  Motivos esperados: NaN=%d; temperatura fuera=%d; humedad fuera=%d\n",
          static_cast<int>(test.expected.hasNaN),
          static_cast<int>(test.expected.temperatureOutOfRange),
          static_cast<int>(test.expected.humidityOutOfRange));
    }
  }

  std::printf("RESULTADO SINTETICO: %u/%u casos correctos; %u fallos\n",
              total - failures, total, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
