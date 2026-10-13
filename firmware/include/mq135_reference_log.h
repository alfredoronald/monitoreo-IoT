#ifndef MONITOREO_IOT_MQ135_REFERENCE_LOG_H
#define MONITOREO_IOT_MQ135_REFERENCE_LOG_H

#include <array>
#include "dht_validation.h"
#include "mq135_experimental.h"

namespace mq135_experimental {
constexpr std::uint32_t REFERENCE_LOG_SCHEMA_VERSION = 1;
constexpr std::uint64_t REFERENCE_WARMUP_MS = 30000;

// Fecha/hora aportada por una fuente externa identificada, nunca inventada.
// Si no hay reloj disponible: available=false y textos vacíos.
struct AcquisitionTime {
  bool available;
  char iso8601[40];  // Incluir Z o desplazamiento horario, si está disponible.
  char source[64];
};

struct EnvironmentalConditions {
  bool available;
  double temperatureC;
  double humidityPercent;
  std::uint64_t acquisitionUptimeMs;
};

struct ElectricalConfiguration {
  std::uint32_t circuitRevision;
  int minimumUsableAdcCode;
  double minimumUsefulVaoV;
  // Identifican documentos con valores, atenuación, calibración y topología.
  char adcConversionId[64];
  char circuitDescription[128];
};

struct CaptureMetadata {
  AcquisitionTime firstSampleTime;
  // Tiempos monotónicos extendidos del MISMO arranque, sin rollover de millis.
  std::uint64_t warmupStartedUptimeMs;
  std::uint64_t electricalVerifiedUptimeMs;
  std::uint32_t requestedIntervalMs;
  std::size_t minimumSamples;
  double maxRelativeSpread;
  ElectricalConfiguration electrical;
};

struct ReferenceSample {
  int adcRaw;
  double vcV;
  double vaoV;  // Reconstruida mediante la conversión eléctrica verificada.
  std::uint64_t acquisitionUptimeMs;
  std::uint32_t circuitRevision;
  EnvironmentalConditions environment;
};

struct LoggedSample {
  ReferenceSample acquired;
  double q;
};

template <std::size_t Capacity>
struct ReferenceLogRecord {
  std::uint32_t schemaVersion;
  CaptureMetadata capture;
  std::size_t sampleCount;
  std::uint64_t durationMs;  // Tiempo real entre primera y última muestra.
  std::array<LoggedSample, Capacity> samples;
  double observedRelativeSpread;
  ReferenceRecord normalizedReference;
  double modelA;
  double modelB;
  double assumedDevelopmentReferencePpm;
  bool referenceConcentrationIsAssumed;
};

template <std::size_t N>
inline bool hasText(const char (&value)[N]) {
  if (value[0] == '\0') return false;
  for (std::size_t i = 1; i < N; ++i) {
    if (value[i] == '\0') return true;
  }
  return false;
}

inline Status validateEnvironment(const EnvironmentalConditions& environment,
                                  std::uint64_t sampleUptimeMs) {
  if (!environment.available) return Status::Ok;
  if (!std::isfinite(environment.temperatureC) ||
      !std::isfinite(environment.humidityPercent) ||
      environment.temperatureC < dht_validation::TEMPERATURE_MIN_C ||
      environment.temperatureC > dht_validation::TEMPERATURE_MAX_C ||
      environment.humidityPercent < dht_validation::HUMIDITY_MIN_PERCENT ||
      environment.humidityPercent > dht_validation::HUMIDITY_MAX_PERCENT ||
      environment.acquisitionUptimeMs > sampleUptimeMs) {
    return Status::InvalidMetadata;
  }
  return Status::Ok;
}

// Registro de tamaño fijo, sin heap, reloj, ADC, NVS ni captura automática.
// El futuro llamador proporciona muestras nuevas con procedencia verificada.
// Las funciones matemáticas de mq135_experimental.h siguen siendo puras.
template <std::size_t Capacity>
class ReferenceLog {
  static_assert(Capacity >= 2, "El registro requiere al menos dos muestras");

 public:
  ReferenceLog() : record_{} {}

  const ReferenceLogRecord<Capacity>* record() const {
    return reference_.isSet() ? &record_ : nullptr;
  }

  void invalidate() {
    reference_.invalidate();
    record_ = {};
  }

  Status establish(const ReferenceSample* samples, std::size_t count,
                   const CaptureMetadata& metadata,
                   const VerifiedContext& context) {
    const Status contextStatus = validateContext(context);
    if (contextStatus != Status::Ok) return contextStatus;
    if (reference_.isSet()) return Status::ReferenceAlreadySet;
    if (samples == nullptr) return Status::InvalidInput;
    if (count > Capacity) return Status::RecordCapacityExceeded;
    if (metadata.electrical.circuitRevision != context.circuitRevision) {
      return Status::ConfigurationMismatch;
    }
    if (metadata.requestedIntervalMs == 0 || metadata.minimumSamples < 2 ||
        !validSpreadLimit(metadata.maxRelativeSpread) ||
        metadata.electrical.minimumUsableAdcCode < 1 ||
        metadata.electrical.minimumUsableAdcCode >= ADC_12_BIT_MAX ||
        !std::isfinite(metadata.electrical.minimumUsefulVaoV) ||
        metadata.electrical.minimumUsefulVaoV <= 0.0 ||
        !hasText(metadata.electrical.adcConversionId) ||
        !hasText(metadata.electrical.circuitDescription) ||
        (metadata.firstSampleTime.available &&
         (!hasText(metadata.firstSampleTime.iso8601) ||
          !hasText(metadata.firstSampleTime.source)))) {
      return Status::InvalidMetadata;
    }
    if (count < metadata.minimumSamples) return Status::InsufficientSamples;

    std::array<VoltageSample, Capacity> voltages{};
    for (std::size_t i = 0; i < count; ++i) {
      const ReferenceSample& sample = samples[i];
      if (sample.circuitRevision != context.circuitRevision) {
        return Status::ConfigurationMismatch;
      }
      if (sample.acquisitionUptimeMs < metadata.electricalVerifiedUptimeMs ||
          sample.acquisitionUptimeMs < metadata.warmupStartedUptimeMs ||
          sample.acquisitionUptimeMs - metadata.warmupStartedUptimeMs <
              REFERENCE_WARMUP_MS ||
          (i > 0 && sample.acquisitionUptimeMs <=
                        samples[i - 1].acquisitionUptimeMs)) {
        return Status::InvalidProvenance;
      }
      const Result q = validateAcquiredSample(
          sample.adcRaw, sample.vcV, sample.vaoV,
          metadata.electrical.minimumUsableAdcCode,
          metadata.electrical.minimumUsefulVaoV);
      if (!q.isValid()) return q.status;
      const Status environmentStatus =
          validateEnvironment(sample.environment, sample.acquisitionUptimeMs);
      if (environmentStatus != Status::Ok) return environmentStatus;
      voltages[i] = {sample.vcV, sample.vaoV};
    }

    // Solo cambia el estado tras validar TODO el lote. CA-03 usa NO_SPREAD_LIMIT.
    Reference candidate;
    const Status status = candidate.establish(
        voltages.data(), count, metadata.minimumSamples,
        metadata.maxRelativeSpread, context);
    if (status != Status::Ok) return status;

    record_ = {};
    record_.schemaVersion = REFERENCE_LOG_SCHEMA_VERSION;
    record_.capture = metadata;
    if (!metadata.firstSampleTime.available) record_.capture.firstSampleTime = {};
    record_.sampleCount = count;
    record_.durationMs = samples[count - 1].acquisitionUptimeMs -
                         samples[0].acquisitionUptimeMs;
    record_.normalizedReference = candidate.record();
    record_.modelA = MODEL_A;
    record_.modelB = MODEL_B;
    record_.assumedDevelopmentReferencePpm = ASSUMED_DEVELOPMENT_REFERENCE_PPM;
    record_.referenceConcentrationIsAssumed = true;
    double minimumQ = std::numeric_limits<double>::max();
    double maximumQ = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
      record_.samples[i].acquired = samples[i];
      if (!samples[i].environment.available) {
        record_.samples[i].acquired.environment = {
            false, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(), 0};
      }
      const double q = normalizedResistance(samples[i].vcV, samples[i].vaoV).value;
      record_.samples[i].q = q;
      if (q < minimumQ) minimumQ = q;
      if (q > maximumQ) maximumQ = q;
    }
    record_.observedRelativeSpread = maximumQ / candidate.record().qReference -
                                     minimumQ / candidate.record().qReference;
    reference_ = candidate;
    return Status::Ok;
  }

  Result estimate(const ReferenceSample& sample,
                  const VerifiedContext& context) const {
    const Status contextStatus = validateContext(context);
    if (contextStatus != Status::Ok) return failure(contextStatus);
    if (!reference_.isSet()) return failure(Status::ReferenceMissing);
    if (record_.normalizedReference.circuitRevision != context.circuitRevision ||
        sample.circuitRevision != context.circuitRevision) {
      return failure(Status::ConfigurationMismatch);
    }
    const Result q = validateAcquiredSample(
        sample.adcRaw, sample.vcV, sample.vaoV,
        record_.capture.electrical.minimumUsableAdcCode,
        record_.capture.electrical.minimumUsefulVaoV);
    if (!q.isValid()) return q;
    return reference_.estimate(sample.vcV, sample.vaoV, context);
  }

 private:
  Reference reference_;
  ReferenceLogRecord<Capacity> record_;
};
}  // namespace mq135_experimental

#endif  // MONITOREO_IOT_MQ135_REFERENCE_LOG_H
