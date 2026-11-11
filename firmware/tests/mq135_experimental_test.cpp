// T-1.5: DATOS SINTETICOS; no son mediciones ni calibracion fisica.
// Ejecutable independiente C++11; sin Arduino ni PlatformIO Test.
// Preparado para ejecucion posterior autorizada; no ejecutado en esta entrega.
#include "../include/mq135_experimental.h"
#include "../include/mq135_reference_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
using namespace mq135_experimental;
unsigned int checks = 0;
unsigned int failures = 0;

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTETICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

bool near(double actual, double expected) {
  return std::isfinite(actual) &&
         std::fabs(actual - expected) <=
             1e-10 * (1.0 + std::fabs(expected));
}

void expectFailure(Result result, Status status, const char* name) {
  expect(!result.isValid() && result.status == status &&
             std::isnan(result.value), name);
}

void mathematicalLimits() {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double largest = std::numeric_limits<double>::max();
  const double smallest = std::numeric_limits<double>::denorm_min();
  const double invalidVoltages[][2] = {
      {0.0, 1.0}, {-1.0, 0.5}, {5.0, 0.0}, {5.0, -1.0},
      {5.0, 5.0}, {5.0, 6.0}, {nan, 1.0}, {5.0, nan},
      {inf, 1.0}, {5.0, inf}, {-inf, 1.0}, {5.0, -inf}};
  for (const auto& input : invalidVoltages) {
    expectFailure(estimateExperimentalPpm(input[0], input[1], 1.0),
                  Status::InvalidInput, "tension invalida rechazada");
  }
  const double invalidReferences[] = {0.0, -1.0, nan, inf, -inf};
  for (double reference : invalidReferences) {
    expectFailure(estimateExperimentalPpm(5.0, 2.5, reference),
                  Status::InvalidInput, "referencia invalida rechazada");
  }
  expect(near(normalizedResistance(5.0, 1.0).value, 4.0), "q = Vc/VAO - 1");
  expect(near(estimateExperimentalPpm(5.0, 1.0, 4.0).value, 400.0),
         "muestra igual a referencia devuelve supuesto de 400 ppm");
  const Result belowVc = normalizedResistance(5.0, std::nextafter(5.0, 0.0));
  expect(belowVc.isValid() && belowVc.value > 0.0,
         "VAO inmediatamente inferior a Vc conserva q positivo");
  expectFailure(normalizedResistance(largest, smallest),
                Status::NumericalError, "overflow de q rechazado");
  expectFailure(estimateExperimentalPpm(5.0, 1.0, smallest),
                Status::NumericalError, "overflow de q/q_referencia rechazado");
  expectFailure(estimateExperimentalPpm(1.0, std::nextafter(1.0, 0.0), largest),
                Status::NumericalError, "underflow de cociente rechazado");
  expectFailure(estimateExperimentalPpm(2.0, 1.0, 1e200),
                Status::NumericalError, "overflow de potencia rechazado");
  expectFailure(estimateExperimentalPpm(1.0, 1e-200, 1.0),
                Status::NumericalError, "underflow de potencia rechazado");
  expectFailure(estimateExperimentalPpm(2.0, 1.0, 1e111),
                Status::NumericalError, "overflow al multiplicar por 400 rechazado");
  expect(estimateExperimentalPpm(5.0, 2.0, 1.0).value < 400.0 &&
             estimateExperimentalPpm(5.0, 3.0, 1.0).value > 400.0,
         "ppm decrece con q y aumenta con VAO para Vc constante");
}

void normalizationAndLoad() {
  // Circuito ideal SINTETICO: Rs en serie con RL, AO sobre RL.
  const double vc = 5.0;
  const double rsReference = 10000.0;
  const double rsCurrent = 20000.0;
  const double modelA = 116.6020682;
  // Contraste con el modelo original usando R0, sin normalizar q.
  const double r0 = rsReference * std::pow(400.0 / modelA, 1.0 / MODEL_B);
  const double expected = modelA * std::pow(rsCurrent / r0, -MODEL_B);
  const double loads[] = {1000.0, 10000.0, 47000.0};
  for (double load : loads) {
    const double vaoReference = vc * load / (rsReference + load);
    const double vaoCurrent = vc * load / (rsCurrent + load);
    const double qReference = normalizedResistance(vc, vaoReference).value;
    expect(near(estimateExperimentalPpm(vc, vaoCurrent, qReference).value,
                expected), "RL constante cancela con su propia referencia");
  }
  // Mismo Rs; duplicar RL cambia q de 1 a 0.5. La referencia vieja falla.
  const double vaoChangedLoad = vc * 20000.0 / (rsReference + 20000.0);
  const Result withOldReference = estimateExperimentalPpm(vc, vaoChangedLoad, 1.0);
  expect(near(withOldReference.value, 400.0 * std::pow(2.0, MODEL_B)),
         "cambio de carga introduce factor (RL_actual/RL_ref)^B");
  expect(near(estimateExperimentalPpm(3.0, 1.5, 1.0).value, 400.0),
         "usar Vc correspondiente a la muestra conserva la normalizacion ideal");
  // Divisor sintetico 10k/10k: factor 0.5, carga total 20k sobre AO.
  const double effectiveLoad = 1.0 / (1.0 / 10000.0 + 1.0 / 20000.0);
  const double loadedAo = vc * effectiveLoad / (rsReference + effectiveLoad);
  const double adcVoltage = loadedAo * 0.5;
  expect(near(normalizedResistance(vc, adcVoltage / 0.5).value,
              rsReference / effectiveLoad),
         "VAO reconstruido refleja RL en paralelo con el divisor");
  expect(!near(normalizedResistance(vc, adcVoltage).value,
               rsReference / effectiveLoad),
         "usar VADC directamente como VAO produce q incorrecto");
}

void referenceLifecycle() {
  const VerifiedContext verified = {true, true, 7};  // Contexto SINTETICO.
  const VerifiedContext unverified = {false, true, 7};
  const VerifiedContext cold = {true, false, 7};
  const VerifiedContext noRevision = {true, true, 0};
  const VerifiedContext changed = {true, true, 8};
  const VoltageSample stable[] = {{5.0, 2.5}, {5.0, 2.5}, {5.0, 2.5}};
  const VoltageSample unstable[] = {{5.0, 1.0}, {5.0, 2.5}, {5.0, 3.0}};
  const VoltageSample invalid[] = {{5.0, 2.5}, {5.0, 0.0}, {5.0, 2.5}};
  Reference reference;
  expectFailure(reference.estimate(5.0, 2.5, verified), Status::ReferenceMissing,
                "adquisicion no establece referencia automaticamente");
  expect(reference.establish(stable, 3, 3, 0.0, unverified) ==
             Status::CircuitNotVerified && !reference.isSet(),
         "no capturar antes de verificar circuito");
  expect(reference.establish(stable, 3, 3, 0.0, cold) ==
             Status::WarmupIncomplete && !reference.isSet(),
         "no capturar antes de terminar calentamiento");
  expect(reference.establish(stable, 3, 3, 0.0, noRevision) ==
             Status::CircuitNotVerified, "revision cero no verificada");
  expect(reference.establish(nullptr, 3, 3, 0.0, verified) == Status::InvalidInput,
         "puntero nulo rechazado");
  expect(reference.establish(stable, 3, 1, 0.0, verified) == Status::InvalidInput,
         "politica exige al menos dos muestras");
  expect(reference.establish(stable, 2, 3, 0.0, verified) ==
             Status::InsufficientSamples, "lote incompleto rechazado");
  const double invalidTolerances[] = {
      -0.1, 1.0, std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity()};
  for (double tolerance : invalidTolerances) {
    expect(reference.establish(stable, 3, 3, tolerance, verified) ==
               Status::InvalidInput, "tolerancia invalida rechazada");
  }
  expect(reference.establish(unstable, 3, 3, 0.01, verified) ==
             Status::UnstableSamples && !reference.isSet(),
         "lote inestable no modifica referencia");
  expect(reference.establish(invalid, 3, 3, 0.01, verified) ==
             Status::InvalidInput && !reference.isSet(),
         "una muestra invalida descarta lote completo");
  // q = 1 y 2: media 1.5, dispersion relativa 2/3.
  const VoltageSample spreadBoundary[] = {{3.0, 1.5}, {3.0, 1.0}};
  Reference boundary;
  expect(boundary.establish(spreadBoundary, 2, 2, 2.0 / 3.0, verified) ==
             Status::Ok && near(boundary.record().qReference, 1.5),
         "limite inclusivo de estabilidad y promedio de q");
  expect(reference.establish(stable, 3, 3, 0.0, verified) == Status::Ok,
         "lote estable establece referencia explicitamente");
  const ReferenceRecord saved = reference.record();
  expect(near(saved.qReference, 1.0), "q_referencia es promedio del lote");
  reference.estimate(5.0, 1.0, verified);
  reference.estimate(5.0, 3.0, verified);
  expect(reference.record().qReference == saved.qReference,
         "estimaciones sucesivas conservan referencia");
  expect(reference.establish(unstable, 3, 3, 0.01, verified) ==
             Status::ReferenceAlreadySet &&
             reference.record().qReference == saved.qReference,
         "captura no sobrescribe referencia existente");
  expectFailure(reference.estimate(5.0, 2.5, changed),
                Status::ConfigurationMismatch, "otra configuracion bloquea ppm");
  expectFailure(reference.estimate(5.0, 2.5, unverified),
                Status::CircuitNotVerified, "sin verificacion no estima");
  expectFailure(reference.estimate(5.0, 2.5, cold),
                Status::WarmupIncomplete, "sin calentamiento no estima");
  Reference restored;
  expect(restored.restore(saved, unverified) == Status::CircuitNotVerified &&
             !restored.isSet(), "registro previo no evita verificacion");
  expect(restored.restore(saved, cold) == Status::WarmupIncomplete,
         "registro previo no evita calentamiento");
  expect(restored.restore(saved, changed) == Status::ConfigurationMismatch,
         "restauracion rechaza revision distinta");
  ReferenceRecord corrupted = saved;
  corrupted.schemaVersion = 0;
  expect(restored.restore(corrupted, verified) == Status::InvalidInput,
         "esquema desconocido rechazado");
  corrupted = saved;
  corrupted.qReference = std::numeric_limits<double>::quiet_NaN();
  expect(restored.restore(corrupted, verified) == Status::InvalidInput,
         "registro no finito rechazado");
  corrupted.qReference = 0.0;
  expect(restored.restore(corrupted, verified) == Status::InvalidInput,
         "registro con q cero rechazado");
  expect(restored.restore(saved, verified) == Status::Ok &&
             near(restored.estimate(5.0, 2.5, verified).value, 400.0),
         "restauracion explicita conserva supuesto y referencia");
  expect(restored.restore(saved, verified) == Status::ReferenceAlreadySet,
         "restauracion no sobrescribe referencia");
  restored.invalidate();
  expect(!restored.isSet(), "invalidacion explicita elimina referencia");
  expectFailure(restored.estimate(5.0, 2.5, verified), Status::ReferenceMissing,
                "referencia invalidada bloquea estimacion");
}
// Casos adicionales: no se alteran los casos matemáticos anteriores.
void adcValidation() {
  expect(validateAdc12Bit(4095) == Status::AdcSaturated,
         "ADC 4095 es saturacion, no tension valida");
  expect(validateAdc12Bit(-1) == Status::AdcOutOfRange &&
             validateAdc12Bit(4096) == Status::AdcOutOfRange,
         "ADC fuera de [0,4095] rechazado");
  expect(validateAdc12Bit(0) == Status::AdcZero, "ADC cero identificado");
  expect(validateAdc12Bit(1) == Status::Ok &&
             validateAdc12Bit(4094) == Status::Ok,
         "limites numericos aceptados sin afirmar calidad electrica");
  expect(validateAdc12Bit(9, 10) == Status::SignalTooSmall &&
             validateAdc12Bit(10, 10) == Status::Ok,
         "umbral util sintetico distingue senal pequena");
  expect(validateAdc12Bit(100, 0) == Status::InvalidInput &&
             validateAdc12Bit(100, 4095) == Status::InvalidInput,
         "politica ADC invalida rechazada");
  expectFailure(validateAcquiredSample(4095, 5.0, 2.5, 10, 0.1),
                Status::AdcSaturated, "no estimar con ADC saturado");
  expectFailure(validateAcquiredSample(4096, 5.0, 2.5, 10, 0.1),
                Status::AdcOutOfRange, "no estimar con ADC fuera de rango");
  expectFailure(validateAcquiredSample(0, 5.0, 2.5, 10, 0.1),
                Status::AdcZero, "no estimar con ADC cero");
  expectFailure(validateAcquiredSample(9, 5.0, 2.5, 10, 0.1),
                Status::SignalTooSmall, "senal pequena segun umbral de codigo");
  expectFailure(validateAcquiredSample(100, 5.0, 0.05, 10, 0.1),
                Status::SignalTooSmall, "senal pequena segun VAO reconstruida");
  expect(validateAcquiredSample(100, 5.0, 0.1, 10, 0.1).isValid(),
         "limite inclusivo de VAO util");
  const double invalidVao[] = {0.0, -1.0, 5.0, 6.0,
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity()};
  for (double vao : invalidVao) {
    expectFailure(validateAcquiredSample(2000, 5.0, vao, 10, 0.1),
                  Status::InvalidInput, "tension invalida antes del modelo");
  }
  expectFailure(validateAcquiredSample(100, 5.0, 2.5, 10, 0.0),
                Status::InvalidInput, "umbral de tension debe estar definido");
  expectFailure(validateAcquiredSample(100, 5.0, 2.5, 10,
                                      std::numeric_limits<double>::infinity()),
                Status::InvalidInput, "umbral de tension no finito rechazado");
  expectFailure(validateAcquiredSample(100,
                    std::numeric_limits<double>::max(), 0.1, 10, 0.1),
                Status::NumericalError, "q no finito en muestra adquirida");
}

CaptureMetadata syntheticCaptureMetadata() {
  CaptureMetadata metadata{};
  // Fecha ARTIFICIAL de prueba: no describe una adquisición real.
  metadata.firstSampleTime.available = true;
  std::strcpy(metadata.firstSampleTime.iso8601, "2000-01-01T00:00:00Z");
  std::strcpy(metadata.firstSampleTime.source, "reloj SINTETICO");
  metadata.warmupStartedUptimeMs = 0;
  metadata.electricalVerifiedUptimeMs = 31000;
  metadata.requestedIntervalMs = 2000;
  metadata.minimumSamples = 3;
  metadata.maxRelativeSpread = 0.01;
  metadata.electrical.circuitRevision = 42;
  metadata.electrical.minimumUsableAdcCode = 10;
  metadata.electrical.minimumUsefulVaoV = 0.1;
  std::strcpy(metadata.electrical.adcConversionId, "conversion SINTETICA");
  std::strcpy(metadata.electrical.circuitDescription, "circuito SINTETICO");
  return metadata;
}

void tracedReference() {
  const VerifiedContext verified = {true, true, 42};
  const VerifiedContext unverified = {false, true, 42};
  const VerifiedContext cold = {true, false, 42};
  const VerifiedContext changed = {true, true, 43};
  const CaptureMetadata metadata = syntheticCaptureMetadata();
  const ReferenceSample stable[] = {
      {2000, 5.0, 2.5, 32000, 42, {true, 20.0, 50.0, 31000}},
      {2001, 5.0, 2.5, 34000, 42, {true, 21.0, 51.0, 33000}},
      {1999, 5.0, 2.5, 36000, 42, {false, 0.0, 0.0, 0}}};
  ReferenceLog<3> log;
  expect(log.record() == nullptr, "sin referencia no hay registro exportable");
  expectFailure(log.estimate(stable[0], verified), Status::ReferenceMissing,
                "registro trazable bloquea ppm sin referencia");
  expect(log.establish(stable, 3, metadata, unverified) ==
             Status::CircuitNotVerified && log.record() == nullptr,
         "registro exige verificacion electrica");
  expect(log.establish(stable, 3, metadata, cold) == Status::WarmupIncomplete,
         "registro exige calentamiento completo");
  expect(log.establish(nullptr, 3, metadata, verified) == Status::InvalidInput,
         "registro rechaza lote nulo");
  expect(log.establish(stable, 4, metadata, verified) ==
             Status::RecordCapacityExceeded, "no truncar valores ADC del lote");
  expect(log.establish(stable, 2, metadata, verified) ==
             Status::InsufficientSamples, "registro exige lote completo");

  ReferenceSample modified[] = {stable[0], stable[1], stable[2]};
  modified[0].adcRaw = 4095;
  expect(log.establish(modified, 3, metadata, verified) == Status::AdcSaturated &&
             log.record() == nullptr, "lote saturado no fija referencia");
  modified[0].adcRaw = -1;
  expect(log.establish(modified, 3, metadata, verified) == Status::AdcOutOfRange,
         "lote fuera de rango rechazado");
  modified[0].adcRaw = 0;
  expect(log.establish(modified, 3, metadata, verified) == Status::AdcZero,
         "lote con ADC cero rechazado");
  modified[0].adcRaw = 9;
  expect(log.establish(modified, 3, metadata, verified) == Status::SignalTooSmall,
         "lote con senal pequena rechazado");
  modified[0] = stable[0];
  modified[0].vaoV = 5.0;
  expect(log.establish(modified, 3, metadata, verified) == Status::InvalidInput,
         "lote con VAO igual a Vc rechazado");
  modified[0].vaoV = 1.0;
  expect(log.establish(modified, 3, metadata, verified) == Status::UnstableSamples &&
             log.record() == nullptr, "lote inestable no genera registro parcial");
  modified[0] = stable[0];
  modified[0].acquisitionUptimeMs = 30000;
  expect(log.establish(modified, 3, metadata, verified) == Status::InvalidProvenance,
         "no reutilizar muestra previa a verificacion electrica");
  modified[0].acquisitionUptimeMs = 29999;
  CaptureMetadata beforeWarmup = metadata;
  beforeWarmup.electricalVerifiedUptimeMs = 1000;
  expect(log.establish(modified, 3, beforeWarmup, verified) ==
             Status::InvalidProvenance, "bandera warmup no acepta muestra prematura");
  modified[0] = stable[0];
  modified[1].acquisitionUptimeMs = modified[0].acquisitionUptimeMs;
  expect(log.establish(modified, 3, metadata, verified) == Status::InvalidProvenance,
         "tiempos duplicados o no crecientes rechazados");
  modified[1] = stable[1];
  modified[0].circuitRevision = 43;
  expect(log.establish(modified, 3, metadata, verified) ==
             Status::ConfigurationMismatch, "no mezclar configuraciones en lote");
  CaptureMetadata incomplete = metadata;
  incomplete.electrical.circuitRevision = 43;
  expect(log.establish(stable, 3, incomplete, verified) ==
             Status::ConfigurationMismatch, "configuracion del registro debe coincidir");
  incomplete = metadata;
  incomplete.requestedIntervalMs = 0;
  expect(log.establish(stable, 3, incomplete, verified) == Status::InvalidMetadata,
         "intervalo no registrado rechazado");
  incomplete = metadata;
  incomplete.electrical.adcConversionId[0] = '\0';
  expect(log.establish(stable, 3, incomplete, verified) == Status::InvalidMetadata,
         "conversion ADC sin identificar rechazada");
  incomplete = metadata;
  incomplete.firstSampleTime.source[0] = '\0';
  expect(log.establish(stable, 3, incomplete, verified) == Status::InvalidMetadata,
         "fecha disponible exige fuente trazable");
  modified[0] = stable[0];
  modified[0].environment.temperatureC =
      std::numeric_limits<double>::quiet_NaN();
  expect(log.establish(modified, 3, metadata, verified) == Status::InvalidMetadata,
         "DHT disponible debe contener datos validos");
  modified[0] = stable[0];
  modified[0].environment.acquisitionUptimeMs = 32001;
  expect(log.establish(modified, 3, metadata, verified) == Status::InvalidMetadata,
         "DHT futuro respecto de muestra rechazado");

  const Status established = log.establish(stable, 3, metadata, verified);
  expect(established == Status::Ok && log.record() != nullptr,
         "registro completo establece referencia explicitamente");
  // Evitar desreferenciar nullptr si la preparación contiene un defecto.
  if (log.record() == nullptr) return;
  const auto saved = *log.record();
  expect(saved.schemaVersion == REFERENCE_LOG_SCHEMA_VERSION &&
             saved.sampleCount == 3 && saved.durationMs == 4000 &&
             saved.capture.requestedIntervalMs == 2000,
         "registro conserva esquema cantidad intervalo y duracion real");
  expect(saved.capture.firstSampleTime.available &&
             std::strcmp(saved.capture.firstSampleTime.iso8601,
                         metadata.firstSampleTime.iso8601) == 0 &&
             std::strcmp(saved.capture.firstSampleTime.source,
                         metadata.firstSampleTime.source) == 0,
         "registro conserva fecha y fuente sinteticas sin reloj interno");
  expect(saved.capture.warmupStartedUptimeMs == 0 &&
             saved.capture.electricalVerifiedUptimeMs == 31000 &&
             saved.capture.minimumSamples == 3 &&
             saved.capture.maxRelativeSpread == 0.01 &&
             near(saved.observedRelativeSpread, 0.0),
         "registro conserva procedencia y criterio de estabilidad");
  expect(saved.normalizedReference.circuitRevision == 42 &&
             saved.capture.electrical.circuitRevision == 42 &&
             saved.capture.electrical.minimumUsableAdcCode == 10 &&
             saved.capture.electrical.minimumUsefulVaoV == 0.1 &&
             std::strcmp(saved.capture.electrical.adcConversionId,
                         metadata.electrical.adcConversionId) == 0 &&
             std::strcmp(saved.capture.electrical.circuitDescription,
                         metadata.electrical.circuitDescription) == 0,
         "registro conserva configuracion electrica y umbrales");
  expect(near(saved.normalizedReference.qReference, 1.0) &&
             saved.modelA == 116.6020682 && saved.modelB == 2.769034857 &&
             saved.assumedDevelopmentReferencePpm == 400.0 &&
             saved.referenceConcentrationIsAssumed,
         "registro conserva q promedio coeficientes y supuesto de 400 ppm");
  for (std::size_t i = 0; i < 3; ++i) {
    const auto& acquired = saved.samples[i].acquired;
    expect(acquired.adcRaw == stable[i].adcRaw && acquired.vcV == stable[i].vcV &&
               acquired.vaoV == stable[i].vaoV &&
               acquired.acquisitionUptimeMs == stable[i].acquisitionUptimeMs &&
               acquired.circuitRevision == stable[i].circuitRevision &&
               near(saved.samples[i].q, 1.0),
           "cada muestra conserva ADC Vc VAO tiempo revision y q");
  }
  expect(saved.samples[0].acquired.environment.available &&
             saved.samples[0].acquired.environment.temperatureC == 20.0 &&
             saved.samples[0].acquired.environment.humidityPercent == 50.0 &&
             saved.samples[0].acquired.environment.acquisitionUptimeMs == 31000,
         "registro conserva DHT disponible y su antiguedad");
  expect(!saved.samples[2].acquired.environment.available &&
             std::isnan(saved.samples[2].acquired.environment.temperatureC) &&
             std::isnan(saved.samples[2].acquired.environment.humidityPercent),
         "DHT ausente no se representa como medicion cero");
  ReferenceSample later = stable[0];
  later.acquisitionUptimeMs = 40000;
  later.vaoV = 3.0;
  expect(log.estimate(later, verified).isValid(), "estimacion posterior sintetica");
  later.vaoV = 1.0;
  expect(log.estimate(later, verified).isValid() &&
             log.record()->normalizedReference.qReference ==
                 saved.normalizedReference.qReference &&
             log.record()->sampleCount == saved.sampleCount &&
             log.record()->samples[0].acquired.adcRaw == stable[0].adcRaw,
         "lecturas posteriores conservan referencia y registro original");
  expect(log.establish(stable, 3, metadata, verified) == Status::ReferenceAlreadySet,
         "no sobrescribir registro con nuevas adquisiciones");
  expectFailure(log.estimate(later, changed), Status::ConfigurationMismatch,
                "registro bloquea ppm si cambia configuracion electrica");
  later.circuitRevision = 43;
  expectFailure(log.estimate(later, verified), Status::ConfigurationMismatch,
                "revision de muestra debe coincidir con referencia");
  later = stable[0];
  later.adcRaw = 4095;
  expectFailure(log.estimate(later, verified), Status::AdcSaturated,
                "referencia existente no admite ADC saturado");
  later = stable[0];
  later.vcV = std::numeric_limits<double>::max();
  later.vaoV = 0.1;
  expectFailure(log.estimate(later, verified), Status::NumericalError,
                "referencia existente rechaza q no finito");
  later.vaoV = 1.0;
  expectFailure(log.estimate(later, verified), Status::NumericalError,
                "referencia existente rechaza underflow de ppm a cero");
  log.invalidate();
  expect(log.record() == nullptr, "invalidacion retira registro activo");
  expectFailure(log.estimate(stable[0], verified), Status::ReferenceMissing,
                "registro invalidado bloquea ppm");
  CaptureMetadata noClock = metadata;
  noClock.firstSampleTime.available = false;
  ReferenceLog<3> withoutClock;
  const Status withoutClockStatus =
      withoutClock.establish(stable, 3, noClock, verified);
  expect(withoutClockStatus == Status::Ok && withoutClock.record() != nullptr &&
             !withoutClock.record()->capture.firstSampleTime.available &&
             withoutClock.record()->capture.firstSampleTime.iso8601[0] == '\0' &&
             withoutClock.record()->capture.firstSampleTime.source[0] == '\0',
         "sin reloj conserva tiempos relativos y no inventa fecha");
}
}  // namespace

int main() {
  std::puts("T-1.5: PRUEBAS SINTETICAS, NO MEDICIONES REALES");
  mathematicalLimits();
  normalizationAndLoad();
  referenceLifecycle();
  adcValidation();
  tracedReference();
  std::printf("SINTETICO: %u comprobaciones, %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
