#ifndef MONITOREO_IOT_MQ135_HISTORICAL_DEMO_H
#define MONITOREO_IOT_MQ135_HISTORICAL_DEMO_H

#include <vector>

#include "../include/mq135_experimental.h"

// Análisis de escritorio. Ningún dato de este espacio de nombres es una
// referencia válida para el firmware ni acredita verificación eléctrica.
namespace mq135_historical_demo {
using mq135_experimental::Result;
using mq135_experimental::Status;

struct HistoricalSample {
  std::uint32_t uptimeMs;
  int adcRaw;
};

// Transcripción exacta de los datos históricos proporcionados por el usuario.
// No contiene mediciones de tensión, CO2, ambiente ni fecha/hora civil.
constexpr HistoricalSample HISTORICAL_SAMPLES[] = {
    {30114, 64}, {32120, 80}, {34126, 89}, {36132, 120}, {38138, 87},
    {40144, 112}, {42150, 97}, {44156, 86}, {46162, 80}, {48168, 87}};
constexpr std::size_t HISTORICAL_SAMPLE_COUNT =
    sizeof(HISTORICAL_SAMPLES) / sizeof(HISTORICAL_SAMPLES[0]);
constexpr std::size_t DEMONSTRATIVE_REFERENCE_COUNT = 3;

struct Scenario {
  // Hipótesis numéricas explícitas, no mediciones ni calibración del ESP32.
  double illustrativeFullScaleV;
  double assumedVcV;
};

inline Status validateScenario(const Scenario& scenario) {
  return std::isfinite(scenario.illustrativeFullScaleV) &&
                 scenario.illustrativeFullScaleV > 0.0 &&
                 std::isfinite(scenario.assumedVcV) && scenario.assumedVcV > 0.0
             ? Status::Ok
             : Status::InvalidInput;
}

inline Result approximateVao(int adcRaw, const Scenario& scenario) {
  const Status adcStatus = mq135_experimental::validateAdc12Bit(adcRaw);
  if (adcStatus != Status::Ok) return mq135_experimental::failure(adcStatus);
  if (validateScenario(scenario) != Status::Ok) {
    return mq135_experimental::failure(Status::InvalidInput);
  }
  // Escala ideal HIPOTÉTICA: 11 dB no demuestra este fondo de escala ni
  // linealidad. AO directo solo describe el montaje histórico comunicado.
  const double vao = scenario.illustrativeFullScaleV *
                     (static_cast<double>(adcRaw) /
                      mq135_experimental::ADC_12_BIT_MAX);
  if (!std::isfinite(vao) || vao <= 0.0) {
    return mq135_experimental::failure(Status::NumericalError);
  }
  if (vao >= scenario.assumedVcV) {
    return mq135_experimental::failure(Status::InvalidInput);
  }
  return {Status::Ok, vao};
}

struct Row {
  HistoricalSample sample;
  bool usedForReference;
  Result approximateVaoV;
  Result q;
  Result experimentalPpm;
};

struct Analysis {
  std::vector<Row> rows;
  Result qReference = mq135_experimental::failure(Status::ReferenceMissing);
  double meanReferenceAdc = std::numeric_limits<double>::quiet_NaN();
  double relativeQSpread = std::numeric_limits<double>::quiet_NaN();
  std::size_t referenceCount = 0;
};

// Solo diagnóstico retrospectivo: no usa Reference/ReferenceLog, no crea un
// VerifiedContext y no evade sus bloqueos mediante una verificación ficticia.
// La referencia ilustrativa no se acepta como lote estable del sistema real.
inline Analysis analyze(const HistoricalSample* samples, std::size_t count,
                        std::size_t referenceCount, const Scenario& scenario) {
  Analysis analysis;
  if (samples == nullptr || referenceCount < 2 || referenceCount >= count ||
      validateScenario(scenario) != Status::Ok) {
    analysis.qReference = mq135_experimental::failure(Status::InvalidInput);
    return analysis;
  }
  analysis.referenceCount = referenceCount;
  double meanQ = 0.0;
  double meanAdc = 0.0;
  double minimumQ = std::numeric_limits<double>::max();
  double maximumQ = 0.0;
  Status referenceStatus = Status::Ok;
  for (std::size_t i = 0; i < count; ++i) {
    const bool reference = i < referenceCount;
    Row row = {samples[i], reference, approximateVao(samples[i].adcRaw, scenario),
               mq135_experimental::failure(Status::ReferenceMissing),
               mq135_experimental::failure(Status::ReferenceMissing)};
    // Conserva el orden temporal y la procedencia posterior a la espera.
    if (samples[i].uptimeMs < 30000 ||
        (i > 0 && samples[i].uptimeMs <= samples[i - 1].uptimeMs)) {
      row.q = mq135_experimental::failure(Status::InvalidProvenance);
    } else if (!row.approximateVaoV.isValid()) {
      row.q = row.approximateVaoV;
    } else {
      row.q = mq135_experimental::normalizedResistance(
          scenario.assumedVcV, row.approximateVaoV.value);
    }
    if (reference) {
      if (!row.q.isValid()) {
        if (referenceStatus == Status::Ok) referenceStatus = row.q.status;
      } else {
        meanQ += (row.q.value - meanQ) / static_cast<double>(i + 1);
        meanAdc += (samples[i].adcRaw - meanAdc) / static_cast<double>(i + 1);
        if (row.q.value < minimumQ) minimumQ = row.q.value;
        if (row.q.value > maximumQ) maximumQ = row.q.value;
      }
    }
    analysis.rows.push_back(row);
  }
  if (referenceStatus != Status::Ok) {
    analysis.qReference = mq135_experimental::failure(referenceStatus);
    return analysis;  // No omitir muestras inválidas para rescatar el lote.
  }
  const double spread = maximumQ / meanQ - minimumQ / meanQ;
  if (!std::isfinite(meanQ) || meanQ <= 0.0 || !std::isfinite(meanAdc) ||
      !std::isfinite(spread) || spread < 0.0) {
    analysis.qReference = mq135_experimental::failure(Status::NumericalError);
    return analysis;
  }
  analysis.qReference = {Status::Ok, meanQ};
  analysis.meanReferenceAdc = meanAdc;
  analysis.relativeQSpread = spread;  // Observada, sin aceptación física.

  // Promedio de q_i, NO q(promedio ADC). Se fija una vez antes de evaluar.
  const double fixedReference = analysis.qReference.value;
  for (std::size_t i = referenceCount; i < count; ++i) {
    Row& row = analysis.rows[i];
    row.experimentalPpm = row.q.isValid()
        ? mq135_experimental::estimateExperimentalPpm(
              scenario.assumedVcV, row.approximateVaoV.value, fixedReference)
        : mq135_experimental::failure(row.q.status);
  }
  return analysis;
}
}  // namespace mq135_historical_demo

#endif  // MONITOREO_IOT_MQ135_HISTORICAL_DEMO_H
