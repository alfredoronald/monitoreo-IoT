#ifndef MONITOREO_IOT_MQ135_EXPERIMENTAL_H
#define MONITOREO_IOT_MQ135_EXPERIMENTAL_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace mq135_experimental {
// SUPUESTO DE DESARROLLO: 400 ppm no medidos. No es una calibracion real.
// Modelo GeorgK/MQ135 provisional hasta la calibracion definitiva de HU-08.
constexpr double ASSUMED_DEVELOPMENT_REFERENCE_PPM = 400.0;
constexpr double MODEL_A = 116.6020682;
constexpr double MODEL_B = 2.769034857;
constexpr int ADC_12_BIT_MAX = 4095;
// CA-03 provisional no exige una dispersión determinada; no relaja ADC/tensión.
constexpr double NO_SPREAD_LIMIT = -1.0;
inline bool validSpreadLimit(double limit) {
  return limit == NO_SPREAD_LIMIT ||
      (std::isfinite(limit) && limit >= 0.0 && limit < 1.0);
}
constexpr std::uint32_t REFERENCE_SCHEMA_VERSION = 1;

enum class Status {
  Ok,
  InvalidInput,
  NumericalError,
  CircuitNotVerified,
  WarmupIncomplete,
  ReferenceMissing,
  ReferenceAlreadySet,
  ConfigurationMismatch,
  InsufficientSamples,
  UnstableSamples,
  AdcOutOfRange,
  AdcSaturated,
  AdcZero,
  SignalTooSmall,
  InvalidMetadata,
  InvalidProvenance,
  RecordCapacityExceeded,
  ConversionUnavailable,
  VoltageOutsideAdcRange,
  EmissionDisabled,
  CaptureNotActive,
  SampleNotDue
};

struct Result {
  Status status;
  double value;
  bool isValid() const { return status == Status::Ok; }
};

inline Result failure(Status status) {
  return {status, std::numeric_limits<double>::quiet_NaN()};
}

// El mínimo útil debe justificarse para la placa y la conversión verificadas.
// El valor 1 solo excluye cero; no certifica calidad ni seguridad eléctrica.
inline Status validateAdc12Bit(int raw, int minimumUsableCode = 1) {
  if (minimumUsableCode < 1 || minimumUsableCode >= ADC_12_BIT_MAX) {
    return Status::InvalidInput;
  }
  if (raw < 0 || raw > ADC_12_BIT_MAX) return Status::AdcOutOfRange;
  if (raw == ADC_12_BIT_MAX) return Status::AdcSaturated;
  if (raw == 0) return Status::AdcZero;
  if (raw < minimumUsableCode) return Status::SignalTooSmall;
  return Status::Ok;
}

// VAO es la tensión en AO del módulo; si existe divisor, es su entrada.
// No asumir que la tensión en GPIO34 represente VAO sin verificar la topología.
inline Result normalizedResistance(double vcV, double vaoV) {
  if (!std::isfinite(vcV) || !std::isfinite(vaoV) || vcV <= 0.0 ||
      vaoV <= 0.0 || vaoV >= vcV) {
    return failure(Status::InvalidInput);
  }
  // Equivale a Vc / VAO - 1; evita restar 1 a un cociente cercano a 1.
  const double q = (vcV - vaoV) / vaoV;
  if (!std::isfinite(q) || q <= 0.0) {
    return failure(Status::NumericalError);
  }
  return {Status::Ok, q};
}

// Separa la validez matemática de la utilidad de una señal pequeña.
// No aplica una escala ADC supuesta ni reconstruye VAO desde códigos brutos.
inline Result validateAcquiredSample(int raw, double vcV, double vaoV,
                                    int minimumUsableCode,
                                    double minimumUsefulVaoV) {
  if (!std::isfinite(minimumUsefulVaoV) || minimumUsefulVaoV <= 0.0) {
    return failure(Status::InvalidInput);
  }
  const Status adcStatus = validateAdc12Bit(raw, minimumUsableCode);
  if (adcStatus != Status::Ok) return failure(adcStatus);
  const Result q = normalizedResistance(vcV, vaoV);
  if (!q.isValid()) return q;
  if (vaoV < minimumUsefulVaoV) return failure(Status::SignalTooSmall);
  return q;
}

// Funcion pura: no ADC, Arduino, Wi-Fi, MQTT ni referencia automatica.
// A = 116.6020682 se cancela al normalizar contra el supuesto de 400 ppm.
inline Result estimateExperimentalPpm(double vcV, double vaoV,
                                      double qReference) {
  if (!std::isfinite(qReference) || qReference <= 0.0) {
    return failure(Status::InvalidInput);
  }
  const Result q = normalizedResistance(vcV, vaoV);
  if (!q.isValid()) {
    return q;
  }
  const double ratio = q.value / qReference;
  if (!std::isfinite(ratio) || ratio <= 0.0) {
    return failure(Status::NumericalError);
  }
  const double ppm = ASSUMED_DEVELOPMENT_REFERENCE_PPM *
                     std::pow(ratio, -MODEL_B);
  if (!std::isfinite(ppm) || ppm <= 0.0) {
    return failure(Status::NumericalError);
  }
  return {Status::Ok, ppm};
}

struct VerifiedContext {
  bool electricalParametersVerified;
  bool warmupComplete;
  // Identificador no nulo de sensor, carga, divisor, alimentacion y ADC.
  // Debe cambiar cuando cambia cualquiera de esas condiciones.
  std::uint32_t circuitRevision;
};

inline Status validateContext(const VerifiedContext& context) {
  if (!context.electricalParametersVerified || context.circuitRevision == 0) {
    return Status::CircuitNotVerified;
  }
  return context.warmupComplete ? Status::Ok : Status::WarmupIncomplete;
}

struct VoltageSample {
  double vcV;
  double vaoV;
};

// Registro para persistencia explicita futura; no se escribe NVS desde aqui.
struct ReferenceRecord {
  std::uint32_t schemaVersion;
  std::uint32_t circuitRevision;
  double qReference;
};

class Reference {
 public:
  Reference() { invalidate(); }

  void invalidate() {
    record_ = {REFERENCE_SCHEMA_VERSION, 0,
               std::numeric_limits<double>::quiet_NaN()};
  }

  bool isSet() const { return record_.circuitRevision != 0; }
  ReferenceRecord record() const { return record_; }

  // Operacion explicita y unica por referencia. El llamador debe entregar un
  // lote NUEVO recogido tras verificar el circuito y terminar el calentamiento.
  // La estabilidad numerica no demuestra una concentracion conocida ni HU-08.
  Status establish(const VoltageSample* samples, std::size_t count,
                   std::size_t minimumSamples, double maxRelativeSpread,
                   const VerifiedContext& context) {
    const Status contextStatus = validateContext(context);
    if (contextStatus != Status::Ok) {
      return contextStatus;
    }
    if (isSet()) {
      return Status::ReferenceAlreadySet;
    }
    if (samples == nullptr || minimumSamples < 2 ||
        !validSpreadLimit(maxRelativeSpread)) {
      return Status::InvalidInput;
    }
    if (count < minimumSamples) {
      return Status::InsufficientSamples;
    }

    double mean = 0.0;
    double minimum = std::numeric_limits<double>::max();
    double maximum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
      const Result q = normalizedResistance(samples[i].vcV, samples[i].vaoV);
      if (!q.isValid()) {
        return q.status;
      }
      mean += (q.value - mean) / static_cast<double>(i + 1);
      if (q.value < minimum) minimum = q.value;
      if (q.value > maximum) maximum = q.value;
    }
    if (!std::isfinite(mean) || mean <= 0.0) {
      return Status::NumericalError;
    }
    const double spread = maximum / mean - minimum / mean;
    if (!std::isfinite(spread)) {
      return Status::NumericalError;
    }
    if (maxRelativeSpread != NO_SPREAD_LIMIT && spread > maxRelativeSpread) {
      return Status::UnstableSamples;
    }
    record_ = {REFERENCE_SCHEMA_VERSION, context.circuitRevision, mean};
    return Status::Ok;
  }

  // Restauracion explicita: el llamador verifica integridad y procedencia del
  // registro persistido. No aceptar una referencia de otra revision electrica.
  Status restore(const ReferenceRecord& saved,
                 const VerifiedContext& context) {
    const Status contextStatus = validateContext(context);
    if (contextStatus != Status::Ok) return contextStatus;
    if (isSet()) return Status::ReferenceAlreadySet;
    if (saved.schemaVersion != REFERENCE_SCHEMA_VERSION ||
        !std::isfinite(saved.qReference) || saved.qReference <= 0.0) {
      return Status::InvalidInput;
    }
    if (saved.circuitRevision != context.circuitRevision) {
      return Status::ConfigurationMismatch;
    }
    record_ = saved;
    return Status::Ok;
  }

  Result estimate(double vcV, double vaoV,
                  const VerifiedContext& context) const {
    const Status contextStatus = validateContext(context);
    if (contextStatus != Status::Ok) return failure(contextStatus);
    if (!isSet()) return failure(Status::ReferenceMissing);
    if (record_.circuitRevision != context.circuitRevision) {
      return failure(Status::ConfigurationMismatch);
    }
    return estimateExperimentalPpm(vcV, vaoV, record_.qReference);
  }

 private:
  ReferenceRecord record_;
};
}  // namespace mq135_experimental

#endif  // MONITOREO_IOT_MQ135_EXPERIMENTAL_H
