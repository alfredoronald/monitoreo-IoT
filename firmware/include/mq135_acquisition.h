#ifndef MONITOREO_IOT_MQ135_ACQUISITION_H
#define MONITOREO_IOT_MQ135_ACQUISITION_H

#include <cstdio>
#include "mq135_experimental.h"
#include "mq135_reference_log.h"
#include "mq135_simulated_demo.h"
#include "mq135_calibration.h"

namespace mq135_acquisition {
using mq135_experimental::Status;
constexpr std::uint32_t WARMUP_MS = 30000;
constexpr std::uint32_t SAMPLE_INTERVAL_MS = 2000;
static_assert(WARMUP_MS == mq135_experimental::REFERENCE_WARMUP_MS,
              "La ventana de adquisición y referencia debe coincidir");

enum class State {
  Calentando,
  AdcDisponible,
  VerificacionPendiente,
  ReferenciaPendiente,
  EstimacionDisponible
};

enum class Source { Physical, Simulated };

inline const char* stateName(State state) {
  switch (state) {
    case State::Calentando: return "CALENTANDO";
    case State::AdcDisponible: return "ADC_DISPONIBLE";
    case State::VerificacionPendiente: return "VERIFICACION_PENDIENTE";
    case State::ReferenciaPendiente: return "REFERENCIA_PENDIENTE";
    case State::EstimacionDisponible: return "ESTIMACION_DISPONIBLE";
  }
  return "VERIFICACION_PENDIENTE";
}

inline const char* statusMessage(Status status) {
  switch (status) {
    case Status::Ok: return "válido";
    case Status::InvalidInput: return "condición matemática o parámetro inválido";
    case Status::NumericalError: return "resultado numérico inválido";
    case Status::CircuitNotVerified: return "verificación eléctrica pendiente";
    case Status::WarmupIncomplete: return "calentamiento de firmware pendiente";
    case Status::ReferenceMissing: return "referencia experimental pendiente";
    case Status::ReferenceAlreadySet: return "referencia ya establecida";
    case Status::ConfigurationMismatch: return "configuración eléctrica distinta";
    case Status::InsufficientSamples: return "cantidad de muestras insuficiente";
    case Status::UnstableSamples: return "lote de referencia inestable";
    case Status::AdcOutOfRange: return "ADC fuera de [0, 4095]";
    case Status::AdcSaturated: return "ADC saturado en 4095";
    case Status::AdcZero: return "ADC cero; sin tensión útil";
    case Status::SignalTooSmall: return "señal inferior al mínimo útil";
    case Status::InvalidMetadata: return "metadatos incompletos o inválidos";
    case Status::InvalidProvenance: return "procedencia o tiempo inválidos";
    case Status::RecordCapacityExceeded: return "capacidad del registro excedida";
    case Status::ConversionUnavailable: return "conversión ADC verificada no disponible";
    case Status::VoltageOutsideAdcRange: return "tensión fuera del rango operativo ADC";
    case Status::EmissionDisabled: return "emisión de ppm deshabilitada explícitamente";
    case Status::CaptureNotActive: return "captura de referencia no activa";
    case Status::SampleNotDue: return "intervalo de adquisición pendiente";
    case Status::LoadNotVerified: return "RL efectiva/topologia sin verificar o configurar";
    case Status::CalibrationActive: return "captura de calibracion activa; detener primero";
  }
  return "condición inválida";
}

struct Configuration {
  bool electricalVerified;  // Compatibilidad de GPIO34, independiente de HU-08.
  bool adcConversionVerified;  // ADC→tensión útil, no calibración de CO2.
  bool directAoConnection;
  double adcToAoVoltageRatio;  // VADC / VAO: 1 directo; ratio documentado con divisor.
  double measuredVcV;
  std::uint32_t minimumAdcMilliVolts;
  std::uint32_t maximumAdcMilliVolts;
  std::uint32_t measuredAdcVrefMilliVolts;  // Solo para fallback medido, si existe.
  char verificationEvidenceId[64];
  mq135_experimental::ElectricalConfiguration electrical;
  std::size_t referenceSampleCount;
  double maxRelativeSpread;
  mq135_calibration::LoadConfiguration calibrationLoad;
};

inline Configuration pendingConfiguration() {
  Configuration config{};
  config.directAoConnection = true;  // Describe el montaje, sin verificarlo.
  config.adcToAoVoltageRatio = 1.0;
  config.measuredVcV = std::numeric_limits<double>::quiet_NaN();
  config.electrical.minimumUsefulVaoV = std::numeric_limits<double>::quiet_NaN();
  config.maxRelativeSpread = std::numeric_limits<double>::quiet_NaN();
  return config;
}

struct Observation {
  State state;
  int adcRaw;
  Status adcStatus;
  Status measurementStatus;
  Status captureStatus;
  bool capturing;
  std::size_t capturedSamples;
  mq135_experimental::Result ppm;
  Source source;
  std::uint64_t warmupElapsedMs;
};

// Formato compartido por main.cpp y las pruebas, sin Arduino ni acceso físico.
inline int formatObservation(const Observation& observation, char* buffer,
                             std::size_t capacity) {
  // Fail closed ante procedencia desconocida; jamás formatearla como física.
  const bool simulated = observation.source != Source::Physical;
  const char* label = simulated ? " [SIMULADO]" : "";
  if (observation.state == State::Calentando ||
      observation.warmupElapsedMs < WARMUP_MS) {
    return std::snprintf(buffer, capacity, "MQ-135: CALENTANDO%s\n", label);
  }
  const bool validPpm = observation.state == State::EstimacionDisponible &&
      observation.ppm.isValid() && std::isfinite(observation.ppm.value) &&
      observation.ppm.value > 0.0 && observation.adcStatus == Status::Ok &&
      observation.measurementStatus == Status::Ok &&
      (observation.source == Source::Physical ||
       observation.source == Source::Simulated);
  if (simulated) {
    if (validPpm) {
      return std::snprintf(
          buffer, capacity,
          "[%llu ms] MQ-135 ADC sintetico: %d | %s [SIMULADO]\n"
          "CO2 estimado provisional: %.2f ppm [SIMULADO]\n"
          "Referencia demostrativa fija: 400 ppm asumidos, no medidos [SIMULADO]\n"
          "Pendiente de calibración y validación en HU-08 [SIMULADO]\n",
          static_cast<unsigned long long>(observation.warmupElapsedMs),
          observation.adcRaw, stateName(observation.state), observation.ppm.value);
    }
    const Status reason = observation.ppm.status == Status::Ok
                              ? Status::NumericalError : observation.ppm.status;
    return std::snprintf(
        buffer, capacity,
        "MQ-135 ADC sintetico: %d | %s [SIMULADO]\n"
        "CO2 experimental: bloqueado - %s [SIMULADO]\n",
        observation.adcRaw, stateName(observation.state), statusMessage(reason));
  }
  if (validPpm) {
    // CA-03: 400 ppm son un supuesto de desarrollo, no una medición de CO2.
    // HU-08 calibrará y validará el MQ-135; no es un requisito de este modelo.
    // La advertencia permanece documentada sin repetirla en cada muestra física.
    return std::snprintf(
        buffer, capacity,
        "MQ-135 ADC: %d\n"
        "CO2 estimado provisional: %.2f ppm\n",
        observation.adcRaw, observation.ppm.value);
  }
  const Status reason = observation.ppm.status == Status::Ok
                            ? Status::NumericalError : observation.ppm.status;
  return std::snprintf(
      buffer, capacity,
      "MQ-135 ADC: %d\n"
      "CO2 experimental: bloqueado - %s\n",
      observation.adcRaw, statusMessage(reason));
}

inline int formatCalibration(const mq135_calibration::Capture& capture,
                             const mq135_calibration::LoadConfiguration& load,
                             std::uint32_t revision, char* buffer, std::size_t capacity) {
  const auto loadStatus = mq135_calibration::validateLoad(load, revision, true);
  char rs[32] = "NO_DISPONIBLE";
  char mean[32] = "NO_DISPONIBLE";
  char q[32] = "NO_DISPONIBLE";
  if (capture.lastResistance().isValid())
    std::snprintf(rs, sizeof(rs), "%.9g", capture.lastResistance().value);
  if (capture.validSamples() > 0)
    std::snprintf(mean, sizeof(mean), "%.9g", capture.meanOhms());
  if (capture.lastQ().isValid())
    std::snprintf(q, sizeof(q), "%.9g", capture.lastQ().value);
  const char* lastStatus = capture.lastResistance().status == Status::CaptureNotActive
      ? "sin muestras procesadas" : statusMessage(capture.lastResistance().status);
  return std::snprintf(buffer, capacity,
      "MQ-135 CALIBRACION [FISICO]%s: %s; %s=%s; %s=%s; "
      "validas=%llu; rechazadas=%llu; q_adimensional=%s; RL=%s; ultima=%s; "
      "RL_efectiva_ohm=%.9g; topologia=%s; promedio Rs NO es Ro\n",
      load.experimentalEstimate ? " [EXPERIMENTAL]" : "",
      capture.active() ? "ACTIVA" : "INACTIVA",
      load.experimentalEstimate ? "Rs_estimada_ohm" : "Rs_ohm", rs,
      load.experimentalEstimate ? "promedio_Rs_estimada_ohm" : "promedio_Rs_ohm", mean,
      static_cast<unsigned long long>(capture.validSamples()),
      static_cast<unsigned long long>(capture.rejectedSamples()), q,
      load.experimentalEstimate && loadStatus == Status::Ok
          ? "NOMINAL_NO_MEDIDA" : statusMessage(loadStatus),
      lastStatus, load.effectiveLoadOhms,
      load.experimentalEstimate ? "SUPUESTA_NO_VERIFICADA" :
          (load.topologyVerified ? "VERIFICADA" : "NO_VERIFICADA"));
}

template <std::size_t Capacity>
class Controller {
 public:
  Controller() : config_(pendingConfiguration()), samples_{}, captureMetadata_{},
                 begun_(false), lastMillis_(0), lastUpdateMillis_(0), uptimeMs_(0),
                 warmupStartedMs_(0), verifiedAtMs_(0), captureRequestedMs_(0),
                 sampleDue_(false), capturing_(false), captured_(0), emission_(false),
                 source_(Source::Physical), demoReference_(
                     std::numeric_limits<double>::quiet_NaN()), demoIndex_(0) {}

  void begin(std::uint32_t now) {
    begun_ = true;
    lastMillis_ = lastUpdateMillis_ = now;
    uptimeMs_ = warmupStartedMs_ = now;
    sampleDue_ = false;
    calibration_.reset();
    config_ = pendingConfiguration();
    source_ = Source::Physical;
    demoIndex_ = 0;
    invalidateReference();
  }

  // Llamar en cada vuelta de loop. La resta unsigned soporta rollover.
  bool poll(std::uint32_t now) {
    if (!begun_) return false;
    uptimeMs_ += static_cast<std::uint32_t>(now - lastMillis_);
    lastMillis_ = now;
    sampleDue_ = static_cast<std::uint32_t>(now - lastUpdateMillis_) >=
                 SAMPLE_INTERVAL_MS;
    if (sampleDue_) lastUpdateMillis_ = now;
    return sampleDue_;
  }

  std::uint64_t uptimeMs() const { return uptimeMs_; }
  Source source() const { return source_; }
  std::uint64_t warmupElapsedMs() const {
    return begun_ ? uptimeMs_ - warmupStartedMs_ : 0;
  }
  bool warmupComplete() const {
    return begun_ && uptimeMs_ - warmupStartedMs_ >= WARMUP_MS;
  }
  bool electricalReady() const { return configurationStatus() == Status::Ok; }
  bool capturing() const { return capturing_; }
  std::size_t capturedSamples() const {
    return reference_.record() ? reference_.record()->sampleCount : captured_;
  }
  const mq135_experimental::ReferenceLogRecord<Capacity>* record() const {
    return reference_.record();
  }

  // Toda reaplicación invalida la referencia, incluso si se conserva el ID.
  // Los booleanos deben reflejar evidencia externa, nunca inferirse del ADC.
  Status configure(const Configuration& config) {
    calibration_.reset();
    invalidateReference();
    source_ = Source::Physical;
    config_ = config;
    verifiedAtMs_ = uptimeMs_;
    return configurationStatus();
  }

  // Selección explícita del entorno demo. Nunca fabrica evidencia eléctrica,
  // VerifiedContext ni ReferenceLog. Su referencia no puede pasar al modo físico.
  void configureSimulatedDemo() {
    calibration_.reset();
    invalidateReference();
    config_ = pendingConfiguration();
    source_ = Source::Simulated;
    demoReference_ = mq135_simulated_demo::FIXED_Q_REFERENCE;
    demoIndex_ = 0;
    emission_ = true;  // Exclusivamente demo; observe exige la espera completa.
  }

  Status startReference(const mq135_experimental::AcquisitionTime& wallTime = {}) {
    if (calibration_.active()) return Status::CalibrationActive;
    if (source_ != Source::Physical) return Status::InvalidProvenance;
    if (!warmupComplete()) return Status::WarmupIncomplete;
    const Status ready = configurationStatus();
    if (ready != Status::Ok) return ready;
    if (reference_.record() != nullptr) return Status::ReferenceAlreadySet;
    if (capturing_) return Status::InvalidInput;
    if (config_.referenceSampleCount < 2) return Status::InsufficientSamples;
    if (config_.referenceSampleCount > Capacity) return Status::RecordCapacityExceeded;
    if (!mq135_experimental::validSpreadLimit(config_.maxRelativeSpread))
      return Status::InvalidMetadata;
    if (wallTime.available && (!mq135_experimental::hasText(wallTime.iso8601) ||
                              !mq135_experimental::hasText(wallTime.source))) {
      return Status::InvalidMetadata;
    }
    captureMetadata_ = {};
    captureMetadata_.firstSampleTime = wallTime;
    captureMetadata_.warmupStartedUptimeMs = warmupStartedMs_;
    captureMetadata_.electricalVerifiedUptimeMs = verifiedAtMs_;
    captureMetadata_.requestedIntervalMs = SAMPLE_INTERVAL_MS;
    captureMetadata_.minimumSamples = config_.referenceSampleCount;
    captureMetadata_.maxRelativeSpread = config_.maxRelativeSpread;
    captureMetadata_.electrical = config_.electrical;
    captureRequestedMs_ = uptimeMs_;
    captured_ = 0;
    capturing_ = true;
    return Status::Ok;
  }

  void cancelReference() { capturing_ = false; captured_ = 0; }
  void invalidateReference() {
    cancelReference();
    reference_.invalidate();
    demoReference_ = std::numeric_limits<double>::quiet_NaN();
    emission_ = false;
  }

  Status enableEmission() {
    if (calibration_.active()) return Status::CalibrationActive;
    if (source_ != Source::Physical) return Status::InvalidProvenance;
    const Status gate = prerequisiteStatus();
    if (gate != Status::Ok) return gate;
    emission_ = true;
    return Status::Ok;
  }
  void disableEmission() { emission_ = false; }

  const mq135_calibration::Capture& calibration() const { return calibration_; }
  const mq135_calibration::LoadConfiguration& calibrationLoad() const {
    return config_.calibrationLoad;
  }
  std::uint32_t circuitRevision() const { return config_.electrical.circuitRevision; }
  Status calibrationLoadStatus() const {
    return mq135_calibration::validateLoad(config_.calibrationLoad,
                                          config_.electrical.circuitRevision, true);
  }
  Status startCalibration() {
    const bool wasActive = calibration_.active();
    const auto status = calibration_.start(begun_ && source_ == Source::Physical);
    if (status == Status::Ok && !wasActive) {
      invalidateReference();
      sampleDue_ = false;  // Solo turnos posteriores a la orden.
    }
    return status;
  }
  bool stopCalibration() {
    if (!calibration_.stop()) return false;
    invalidateReference();  // Recuperacion con tres muestras NUEVAS, nunca Rs -> Ro.
    sampleDue_ = false;
    return true;
  }

  State state() const {
    if (!warmupComplete()) return State::Calentando;
    if (source_ == Source::Simulated) {
      return std::isfinite(demoReference_) && demoReference_ > 0.0
                 ? State::AdcDisponible : State::ReferenciaPendiente;
    }
    if (!electricalReady()) return State::VerificacionPendiente;
    if (reference_.record() == nullptr) return State::ReferenciaPendiente;
    // La disponibilidad de una estimación depende también de la muestra actual.
    return State::AdcDisponible;
  }

  Observation observe(int raw, bool voltageAvailable, double adcMilliVolts,
                      const mq135_experimental::EnvironmentalConditions& environment) {
    Observation result{state(), raw, Status::Ok, Status::Ok,
                       Status::CaptureNotActive, capturing_, captured_,
                       mq135_experimental::failure(Status::ReferenceMissing),
                       source_, warmupElapsedMs()};
    if (source_ != Source::Physical) {
      sampleDue_ = false;
      result.ppm = mq135_experimental::failure(Status::InvalidProvenance);
      result.measurementStatus = Status::InvalidProvenance;
      return result;
    }
    if (!sampleDue_) {
      result.ppm = mq135_experimental::failure(Status::SampleNotDue);
      result.measurementStatus = Status::SampleNotDue;
      return result;
    }
    sampleDue_ = false;
    if (!warmupComplete()) {
      calibration_.observe(Status::WarmupIncomplete, config_.calibrationLoad,
                           config_.electrical.circuitRevision, 0.0, 0.0);
      result.ppm = mq135_experimental::failure(Status::WarmupIncomplete);
      result.measurementStatus = Status::WarmupIncomplete;
      return result;
    }
    result.adcStatus = mq135_experimental::validateAdc12Bit(
        raw, electricalReady() ? config_.electrical.minimumUsableAdcCode : 1);
    Status status = result.adcStatus;
    if (status == Status::Ok) status = configurationStatus();
    if (status == Status::Ok && (!voltageAvailable || !std::isfinite(adcMilliVolts))) {
      status = Status::ConversionUnavailable;
    }
    if (status == Status::Ok &&
        (adcMilliVolts < config_.minimumAdcMilliVolts ||
         adcMilliVolts > config_.maximumAdcMilliVolts)) {
      status = Status::VoltageOutsideAdcRange;
    }
    // Reconstruir AO antes del modelo: el ADC mide el nodo posterior al divisor.
    // Ratio 1 conserva el montaje directo; configurar ratio no verifica hardware.
    const double vaoV = adcMilliVolts / 1000.0 / config_.adcToAoVoltageRatio;
    if (status == Status::Ok) {
      status = mq135_experimental::validateAcquiredSample(
          raw, config_.measuredVcV, vaoV, config_.electrical.minimumUsableAdcCode,
          config_.electrical.minimumUsefulVaoV).status;
    }
    result.measurementStatus = status;
    if (calibration_.active()) {
      calibration_.observe(status, config_.calibrationLoad,
                           config_.electrical.circuitRevision, config_.measuredVcV, vaoV);
      result.ppm = mq135_experimental::failure(Status::CalibrationActive);
      return result;  // Sin referencia provisional ni conversion a ppm en captura.
    }
    mq135_experimental::ReferenceSample sample{
        raw, config_.measuredVcV, vaoV, uptimeMs_,
        config_.electrical.circuitRevision, environment};

    if (capturing_) {
      if (status != Status::Ok) {
        // Una muestra inválida aborta el lote; no se rellena con muestras viejas.
        result.captureStatus = status;
        cancelReference();
      } else if (uptimeMs_ <= captureRequestedMs_) {
        result.captureStatus = Status::InvalidProvenance;
        cancelReference();
      } else {
        result.captureStatus = mq135_experimental::validateEnvironment(environment, uptimeMs_);
        if (result.captureStatus != Status::Ok) {
          cancelReference();
        } else {
          // Fecha proporcionada a startReference identifica la PRIMERA muestra,
          // no el instante de recepción del comando; sin reloj se deja ausente.
          samples_[captured_++] = sample;
          if (captured_ == config_.referenceSampleCount) {
            result.captureStatus = reference_.establish(
                samples_.data(), captured_, captureMetadata_, context());
            capturing_ = false;
            if (result.captureStatus != Status::Ok) captured_ = 0;
          }
        }
      }
    }
    result.state = state();
    result.capturing = capturing_;
    result.capturedSamples = capturedSamples();
    if (status != Status::Ok) {
      result.ppm = mq135_experimental::failure(status);
    } else {
      const Status gate = prerequisiteStatus();
      if (gate != Status::Ok) result.ppm = mq135_experimental::failure(gate);
      else if (!emission_) result.ppm = mq135_experimental::failure(Status::EmissionDisabled);
      else result.ppm = reference_.estimate(sample, context());
    }
    if (result.ppm.isValid()) result.state = State::EstimacionDisponible;
    return result;
  }

  Observation observeSimulated() {
    // Etiqueta fija incluso si se invoca accidentalmente desde el modo físico.
    Observation result{state(), 0, Status::Ok, Status::Ok,
                       Status::CaptureNotActive, false, 0,
                       mq135_experimental::failure(Status::ReferenceMissing),
                       Source::Simulated, warmupElapsedMs()};
    Status gate = Status::Ok;
    if (source_ != Source::Simulated) gate = Status::InvalidProvenance;
    else if (!sampleDue_) gate = Status::SampleNotDue;
    else if (!warmupComplete()) gate = Status::WarmupIncomplete;
    else if (!std::isfinite(demoReference_) || demoReference_ <= 0.0)
      gate = Status::ReferenceMissing;
    else if (!emission_) gate = Status::EmissionDisabled;
    sampleDue_ = false;
    if (gate != Status::Ok) {
      result.measurementStatus = gate;
      result.ppm = mq135_experimental::failure(gate);
      return result;
    }
    const auto sample = mq135_simulated_demo::sampleAt(demoIndex_);
    demoIndex_ = (demoIndex_ + 1) % mq135_simulated_demo::SAMPLE_COUNT;
    result.adcRaw = sample.adcRaw;
    result.adcStatus = mq135_experimental::validateAdc12Bit(
        sample.adcRaw, mq135_simulated_demo::MINIMUM_ADC_CODE);
    result.ppm = mq135_simulated_demo::estimate(sample, demoReference_);
    result.measurementStatus = result.ppm.status;
    if (result.ppm.isValid()) result.state = State::EstimacionDisponible;
    return result;
  }

 private:
  Status configurationStatus() const {
    if (!begun_ || !config_.electricalVerified ||
        !mq135_experimental::hasText(config_.verificationEvidenceId) ||
        config_.electrical.circuitRevision == 0) return Status::CircuitNotVerified;
    if (!config_.adcConversionVerified) return Status::ConversionUnavailable;
    if (!std::isfinite(config_.adcToAoVoltageRatio) ||
        config_.adcToAoVoltageRatio <= 0.0 || config_.adcToAoVoltageRatio > 1.0 ||
        (config_.directAoConnection && config_.adcToAoVoltageRatio != 1.0) ||
        (!config_.directAoConnection && config_.adcToAoVoltageRatio >= 1.0) ||
        !std::isfinite(config_.measuredVcV) ||
        config_.measuredVcV <= 0.0 || config_.minimumAdcMilliVolts == 0 ||
        config_.maximumAdcMilliVolts <= config_.minimumAdcMilliVolts ||
        config_.electrical.minimumUsableAdcCode < 1 ||
        config_.electrical.minimumUsableAdcCode >= mq135_experimental::ADC_12_BIT_MAX ||
        !std::isfinite(config_.electrical.minimumUsefulVaoV) ||
        config_.electrical.minimumUsefulVaoV <= 0.0 ||
        !mq135_experimental::hasText(config_.electrical.adcConversionId) ||
        !mq135_experimental::hasText(config_.electrical.circuitDescription)) {
      return Status::InvalidMetadata;
    }
    return Status::Ok;
  }

  mq135_experimental::VerifiedContext context() const {
    return {electricalReady(), warmupComplete(), config_.electrical.circuitRevision};
  }

  Status prerequisiteStatus() const {
    if (!warmupComplete()) return Status::WarmupIncomplete;
    const Status ready = configurationStatus();
    if (ready != Status::Ok) return ready;
    if (reference_.record() == nullptr) return Status::ReferenceMissing;
    if (reference_.record()->normalizedReference.circuitRevision !=
        config_.electrical.circuitRevision) return Status::ConfigurationMismatch;
    return Status::Ok;
  }

  Configuration config_;
  mq135_calibration::Capture calibration_;
  mq135_experimental::ReferenceLog<Capacity> reference_;
  std::array<mq135_experimental::ReferenceSample, Capacity> samples_;
  mq135_experimental::CaptureMetadata captureMetadata_;
  bool begun_;
  std::uint32_t lastMillis_;
  std::uint32_t lastUpdateMillis_;
  std::uint64_t uptimeMs_;
  std::uint64_t warmupStartedMs_;
  std::uint64_t verifiedAtMs_;
  std::uint64_t captureRequestedMs_;
  bool sampleDue_;
  bool capturing_;
  std::size_t captured_;
  bool emission_;
  Source source_;
  double demoReference_;
  std::size_t demoIndex_;
};
}  // namespace mq135_acquisition

#endif  // MONITOREO_IOT_MQ135_ACQUISITION_H
