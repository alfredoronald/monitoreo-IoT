#ifndef MONITOREO_IOT_MQ135_ADC_FILTER_H
#define MONITOREO_IOT_MQ135_ADC_FILTER_H

#include <array>
#include "mq135_experimental.h"

namespace mq135_adc_filter {
constexpr std::size_t INTERNAL_SAMPLE_COUNT = 11;
static_assert(INTERNAL_SAMPLE_COUNT % 2 == 1, "La mediana requiere un lote impar");
using Samples = std::array<int, INTERNAL_SAMPLE_COUNT>;

struct Result {
  mq135_experimental::Status status;
  // Mediana únicamente si status == Ok. En caso de error conserva el código
  // rechazado para propagar su validación; no representa una muestra filtrada.
  int raw;
  bool isValid() const { return status == mq135_experimental::Status::Ok; }
};

inline Result median(const Samples& samples, int minimumUsableCode = 1) {
  // No ocultar ceros, saturación ni otros códigos inválidos con la mediana.
  // Un error interno invalida el intervalo completo, sin reutilizar el anterior.
  for (int raw : samples) {
    const auto status = mq135_experimental::validateAdc12Bit(raw, minimumUsableCode);
    if (status != mq135_experimental::Status::Ok) return {status, raw};
  }
  Samples sorted = samples;
  // Inserción acotada a 11 elementos: sin heap, esperas ni dependencia Arduino.
  for (std::size_t i = 1; i < sorted.size(); ++i) {
    const int value = sorted[i];
    std::size_t j = i;
    while (j > 0 && sorted[j - 1] > value) {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = value;
  }
  return {mq135_experimental::Status::Ok, sorted[sorted.size() / 2]};
}

// Una ráfaga nueva por turno físico; el llamador entrega solo UN resultado al
// controlador. Las 11 lecturas internas nunca son 11 muestras de referencia.
template <typename Reader>
inline Result acquire(Reader read, int minimumUsableCode = 1) {
  Samples samples{};
  for (auto& raw : samples) raw = read();
  return median(samples, minimumUsableCode);
}
}  // namespace mq135_adc_filter

#endif  // MONITOREO_IOT_MQ135_ADC_FILTER_H
