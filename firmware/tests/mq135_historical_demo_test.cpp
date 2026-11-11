#include <cstdlib>
#include <iostream>

#include "../tools/mq135_historical_demo.h"

namespace {
using namespace mq135_historical_demo;
unsigned failures = 0;

void expect(bool condition, const char* description) {
  if (!condition) {
    ++failures;
    std::cerr << "FALLO: " << description << '\n';
  }
}

bool near(double left, double right) {
  return std::isfinite(left) && std::isfinite(right) &&
         std::fabs(left - right) <= 1e-9 * (1.0 + std::fabs(right));
}
}  // namespace

int main() {
  const Scenario illustrative = {3.3, 5.0};
  const Analysis baseline = analyze(HISTORICAL_SAMPLES, HISTORICAL_SAMPLE_COUNT,
                                    DEMONSTRATIVE_REFERENCE_COUNT, illustrative);
  expect(baseline.rows.size() == 10 && baseline.referenceCount == 3,
         "diez registros, tres de referencia y siete de evaluación");
  expect(baseline.qReference.isValid() &&
             near(baseline.qReference.value, 80.40561159346272) &&
             near(baseline.meanReferenceAdc, 77.66666666666667),
         "promedios algebraicos del escenario, no calibración física");
  expect(near(baseline.relativeQSpread, 0.33868318785587787),
         "reporta dispersión del lote sin declararlo estable");
  if (baseline.rows.size() == 10) {
    for (std::size_t i = 0; i < baseline.rows.size(); ++i) {
      const Row& row = baseline.rows[i];
      expect(row.sample.uptimeMs == HISTORICAL_SAMPLES[i].uptimeMs &&
                 row.sample.adcRaw == HISTORICAL_SAMPLES[i].adcRaw,
             "transcripción y tiempos históricos conservados");
      expect(row.usedForReference == (i < 3) &&
                 row.experimentalPpm.isValid() == (i >= 3),
             "referencia y evaluación disjuntas, sin ppm para referencia");
    }
    expect(near(baseline.rows[3].experimentalPpm.value, 1433.9369375250571) &&
               near(baseline.rows[8].experimentalPpm.value, 458.19180083526464),
           "estimaciones algebraicas posteriores reproducibles");
  }
  const double voltageAtMeanAdc = illustrative.illustrativeFullScaleV *
                                  (baseline.meanReferenceAdc / 4095.0);
  const Result qAtMeanAdc = mq135_experimental::normalizedResistance(
      illustrative.assumedVcV, voltageAtMeanAdc);
  expect(qAtMeanAdc.isValid() &&
             !near(qAtMeanAdc.value, baseline.qReference.value),
         "promedio de q no se sustituye por q del promedio ADC");

  HistoricalSample altered[HISTORICAL_SAMPLE_COUNT];
  for (std::size_t i = 0; i < HISTORICAL_SAMPLE_COUNT; ++i) {
    altered[i] = HISTORICAL_SAMPLES[i];
  }
  // Modificación SINTÉTICA para probar independencia; no otra lectura física.
  altered[9].adcRaw = 200;
  const Analysis laterChanged = analyze(altered, HISTORICAL_SAMPLE_COUNT, 3,
                                       illustrative);
  expect(laterChanged.qReference.value == baseline.qReference.value &&
             laterChanged.relativeQSpread == baseline.relativeQSpread,
         "las evaluaciones posteriores no alteran la referencia fija");

  expect(approximateVao(4095, illustrative).status == Status::AdcSaturated,
         "rechaza saturación antes de convertir");
  expect(approximateVao(-1, illustrative).status == Status::AdcOutOfRange &&
             approximateVao(4096, illustrative).status == Status::AdcOutOfRange,
         "rechaza ADC fuera de rango");
  expect(approximateVao(0, illustrative).status == Status::AdcZero,
         "rechaza cero sin dividir");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  const Scenario invalidScenarios[] = {
      {0.0, 5.0}, {-1.0, 5.0}, {nan, 5.0}, {infinity, 5.0},
      {3.3, 0.0}, {3.3, -1.0}, {3.3, nan}, {3.3, infinity}};
  for (const Scenario& invalid : invalidScenarios) {
    expect(approximateVao(64, invalid).status == Status::InvalidInput,
           "rechaza tensiones de escenario inválidas y no finitas");
  }
  expect(approximateVao(2048, {10.0, 5.0}).status == Status::InvalidInput,
         "rechaza VAO mayor o igual a Vc");
  expect(approximateVao(1365, {3.0, 1.0}).status == Status::InvalidInput,
         "rechaza el límite exacto VAO igual a Vc");
  expect(approximateVao(1, {std::numeric_limits<double>::denorm_min(), 5.0})
             .status == Status::NumericalError,
         "rechaza underflow de tensión aproximada a cero");
  const Analysis overflowing = analyze(HISTORICAL_SAMPLES,
      HISTORICAL_SAMPLE_COUNT, 3, {3.3, std::numeric_limits<double>::max()});
  expect(overflowing.qReference.status == Status::NumericalError,
         "rechaza q no finito y bloquea la referencia");
  expect(mq135_experimental::estimateExperimentalPpm(
             5.0, 2.5, std::numeric_limits<double>::max()).status ==
             Status::NumericalError,
         "el estimador compartido rechaza ppm no finitos por overflow");

  altered[0].adcRaw = 0;
  const Analysis invalidReference = analyze(altered, HISTORICAL_SAMPLE_COUNT, 3,
                                           illustrative);
  expect(invalidReference.qReference.status == Status::AdcZero,
         "una muestra inválida descarta la referencia completa");
  for (const Row& row : invalidReference.rows) {
    expect(!row.experimentalPpm.isValid(),
           "sin referencia válida no produce ninguna estimación");
  }
  altered[0] = HISTORICAL_SAMPLES[0];
  altered[3].adcRaw = 4095;
  const Analysis invalidEvaluation = analyze(altered, HISTORICAL_SAMPLE_COUNT,
                                            3, illustrative);
  expect(invalidEvaluation.qReference.isValid() &&
             invalidEvaluation.rows[3].experimentalPpm.status ==
                 Status::AdcSaturated &&
             invalidEvaluation.qReference.value == baseline.qReference.value,
         "saturación posterior se rechaza sin recalcular referencia");
  altered[3] = HISTORICAL_SAMPLES[3];
  altered[3].uptimeMs = altered[2].uptimeMs;
  const Analysis invalidTime = analyze(altered, HISTORICAL_SAMPLE_COUNT, 3,
                                      illustrative);
  expect(invalidTime.rows[3].experimentalPpm.status == Status::InvalidProvenance,
         "rechaza tiempos duplicados en evaluación");
  expect(!analyze(nullptr, 10, 3, illustrative).qReference.isValid() &&
             !analyze(HISTORICAL_SAMPLES, 10, 10, illustrative)
                  .qReference.isValid(),
         "exige datos y una partición que reserve evaluación");

  // Escenario SINTÉTICO: escalar Vc y VAO a la vez conserva q y ppm.
  const Analysis scaled = analyze(HISTORICAL_SAMPLES, HISTORICAL_SAMPLE_COUNT,
                                 3, {6.6, 10.0});
  expect(near(scaled.qReference.value, baseline.qReference.value),
         "normalización conserva q ante un escalado común hipotético");
  for (std::size_t i = 3; i < HISTORICAL_SAMPLE_COUNT; ++i) {
    expect(near(scaled.rows[i].experimentalPpm.value,
                baseline.rows[i].experimentalPpm.value),
           "normalización conserva estimaciones bajo el mismo escalado");
  }
  std::cout << "Comprobaciones de algoritmo retrospectivo; no validación física. "
            << failures << " fallos.\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
