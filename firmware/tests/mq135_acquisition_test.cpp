// Pruebas SINTÉTICAS independientes de integración T-1.5. No son mediciones.
// Ejecutable de escritorio C++11; no Arduino, ESP32, reloj real ni hardware.
// Preparado sin ejecutarlo. La suite anterior de 131 comprobaciones no cambia.
#include "../include/mq135_acquisition.h"
#include "../include/mq135_site_config.h"

#include <cstdlib>
#include <cstring>

namespace {
using namespace mq135_acquisition;
using mq135_experimental::Status;
unsigned checks = 0;
unsigned failures = 0;

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTÉTICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

const mq135_experimental::EnvironmentalConditions noEnvironment{
    false, std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN(), 0};

Configuration syntheticConfiguration() {
  // Todos los parámetros siguientes son ficticios y solo ejercitan el código.
  auto config = pendingConfiguration();
  config.electricalVerified = true;
  config.adcConversionVerified = true;
  config.measuredVcV = 5.0;
  config.minimumAdcMilliVolts = 150;
  config.maximumAdcMilliVolts = 2450;
  std::strcpy(config.verificationEvidenceId, "EVIDENCIA SINTÉTICA");
  config.electrical.circuitRevision = 7;
  config.electrical.minimumUsableAdcCode = 10;
  config.electrical.minimumUsefulVaoV = 0.15;
  std::strcpy(config.electrical.adcConversionId, "CONVERSIÓN SINTÉTICA");
  std::strcpy(config.electrical.circuitDescription, "AO directo SINTÉTICO");
  config.referenceSampleCount = 3;
  config.maxRelativeSpread = 0.01;
  return config;
}

Observation acquire(Controller<3>& controller, std::uint32_t now,
                    int raw = 2000, double mv = 2000.0, bool available = true) {
  controller.poll(now);
  return controller.observe(raw, available, mv, noEnvironment);
}

void heatingAndTiming() {
  Controller<3> controller;
  controller.begin(0);
  expect(controller.state() == State::Calentando, "estado inicial CALENTANDO");
  expect(!controller.poll(0) && !controller.poll(1999),
         "sin adquisición antes de 2000 ms");
  expect(controller.poll(2000), "adquisición vence a los 2000 ms");
  const auto warm = controller.observe(2000, true, 2000.0, noEnvironment);
  expect(!warm.ppm.isValid() && warm.ppm.status == Status::WarmupIncomplete &&
             warm.state == State::Calentando,
         "sin ppm durante calentamiento aun con voltajes sintéticos válidos");
  char output[384];
  formatObservation(warm, output, sizeof(output));
  expect(std::strstr(output, "CALENTANDO") != nullptr &&
             std::strstr(output, "ppm") == nullptr &&
             std::strstr(output, "ADC:") == nullptr,
         "salida de calentamiento sin ppm ni ADC ficticio");
  expect(!controller.poll(2001) && !controller.poll(3999) && controller.poll(4000),
         "no acortar el intervalo después de una adquisición");
  controller.observe(0, false, 0.0, noEnvironment);
  expect(controller.startReference() == Status::WarmupIncomplete,
         "captura bloqueada antes de 30 segundos");
  acquire(controller, 29999);
  expect(!controller.warmupComplete(), "29999 ms todavía CALENTANDO");
  controller.poll(30000);
  expect(controller.warmupComplete(), "30 segundos termina la espera");
  const auto early = controller.observe(2000, true, 2000.0, noEnvironment);
  expect(!early.ppm.isValid() && early.ppm.status == Status::SampleNotDue,
         "no insertar adquisición extra al cambiar de estado");

  Controller<3> rollover;
  const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - 1000;
  rollover.begin(start);
  expect(!rollover.poll(static_cast<std::uint32_t>(start + 1999U)) &&
             rollover.poll(static_cast<std::uint32_t>(start + 2000U)),
         "intervalo conserva 2000 ms al desbordar millis");
  rollover.observe(0, false, 0.0, noEnvironment);
  rollover.poll(static_cast<std::uint32_t>(start + 30000U));
  expect(rollover.warmupComplete() &&
             rollover.uptimeMs() == static_cast<std::uint64_t>(start) + 30000,
         "calentamiento y tiempo extendido sobreviven al rollover");
}

void configurationGates() {
  expect(mq135_site_config::electricalConfiguration().adcConversionVerified &&
             mq135_site_config::electricalConfiguration().electricalVerified,
         "divisor REV3 y contraste puntual tienen evidencia externa documentada");
  Controller<3> controller;
  controller.begin(0);
  auto observation = acquire(controller, 30000);
  expect(observation.state == State::VerificacionPendiente &&
             !observation.ppm.isValid() &&
             observation.ppm.status == Status::CircuitNotVerified,
         "ADC válido no verifica el circuito");
  expect(controller.startReference() == Status::CircuitNotVerified &&
             controller.enableEmission() == Status::CircuitNotVerified,
         "sin verificación no capturar ni habilitar ppm");
  char output[384];
  formatObservation(observation, output, sizeof(output));
  expect(std::strstr(output, "MQ-135 ADC: 2000") != nullptr &&
             std::strstr(output, "verificación eléctrica pendiente") != nullptr &&
             std::strstr(output, "bloqueado") != nullptr &&
             std::strstr(output, "CO2 estimado provisional:") == nullptr,
         "monitor conserva ADC y explica bloqueo");

  auto config = syntheticConfiguration();
  config.verificationEvidenceId[0] = '\0';
  expect(controller.configure(config) == Status::CircuitNotVerified,
         "bandera true sin evidencia no verifica circuito");
  config = syntheticConfiguration();
  config.adcConversionVerified = false;
  expect(controller.configure(config) == Status::ConversionUnavailable,
         "sin conversión ADC verificada no estimar");
  config = syntheticConfiguration();
  config.measuredVcV = std::numeric_limits<double>::quiet_NaN();
  expect(controller.configure(config) == Status::InvalidMetadata,
         "Vc medida ausente bloquea el modelo");
  config = syntheticConfiguration();
  config.maximumAdcMilliVolts = 0;
  expect(controller.configure(config) == Status::InvalidMetadata,
         "sin rango útil documentado no estimar");
  config = syntheticConfiguration();
  config.directAoConnection = false;
  expect(controller.configure(config) == Status::InvalidMetadata,
         "divisor declarado sin relación de tensión coherente se rechaza");
  config = syntheticConfiguration();
  expect(controller.configure(config) == Status::Ok,
         "configuración completa SINTÉTICA admitida explícitamente");
  observation = acquire(controller, 32000);
  expect(observation.state == State::ReferenciaPendiente &&
             observation.ppm.status == Status::ReferenceMissing &&
             controller.record() == nullptr,
         "verificación sin referencia no publica ppm ni captura sola");
  expect(controller.enableEmission() == Status::ReferenceMissing,
         "comando de ppm no elude referencia pendiente");
}

void referenceAndEstimation() {
  Controller<3> controller;
  controller.begin(0);
  controller.configure(syntheticConfiguration());
  acquire(controller, 30000);
  expect(controller.startReference() == Status::Ok && controller.capturing(),
         "captura explícita después de calentamiento y verificación");
  auto observation = acquire(controller, 32000);
  expect(controller.record() == nullptr && observation.capturedSamples == 1 &&
             observation.ppm.status == Status::ReferenceMissing,
         "una muestra no establece q_referencia");
  acquire(controller, 34000);
  observation = acquire(controller, 36000);
  expect(controller.record() != nullptr && !controller.capturing() &&
             observation.captureStatus == Status::Ok &&
             observation.ppm.status == Status::EmissionDisabled &&
             observation.state == State::AdcDisponible,
         "lote estable fija referencia sin activar emisión automáticamente");
  if (controller.record() == nullptr) return;
  const double fixedQ = controller.record()->normalizedReference.qReference;
  expect(std::fabs(fixedQ - 1.5) < 1e-12 &&
             controller.record()->sampleCount == 3 &&
             controller.record()->durationMs == 4000 &&
             controller.record()->samples[0].acquired.acquisitionUptimeMs == 32000 &&
             !controller.record()->capture.firstSampleTime.available,
         "registro conserva q, lote nuevo, duración y fecha ausente");
  expect(controller.startReference() == Status::ReferenceAlreadySet,
         "no sobrescribir referencia con otro lote");
  expect(controller.enableEmission() == Status::Ok,
         "emisión requiere habilitación deliberada con referencia válida");
  observation = acquire(controller, 38000);
  expect(observation.state == State::EstimacionDisponible &&
             observation.ppm.isValid() &&
             std::fabs(observation.ppm.value - 400.0) < 1e-10,
         "flujo habilitado con datos SINTÉTICOS válidos");
  char output[384];
  const int length = formatObservation(observation, output, sizeof(output));
  expect(length >= 0 && static_cast<std::size_t>(length) < sizeof(output) &&
             std::strcmp(output, "MQ-135 ADC: 2000\nCO2 estimado provisional: 400.00 ppm\n") == 0 &&
             controller.record()->referenceConcentrationIsAssumed &&
             controller.record()->assumedDevelopmentReferencePpm == 400.0,
         "formato físico de dos líneas conserva supuesto de 400 ppm en el registro");
  auto invalidResult = observation;
  invalidResult.ppm.value = std::numeric_limits<double>::infinity();
  formatObservation(invalidResult, output, sizeof(output));
  expect(std::strstr(output, "CO2 estimado provisional:") == nullptr &&
             std::strstr(output, "resultado numérico inválido") != nullptr,
         "formateador tampoco publica un resultado infinito inconsistente");
  invalidResult.ppm.value = std::numeric_limits<double>::quiet_NaN();
  formatObservation(invalidResult, output, sizeof(output));
  expect(std::strstr(output, "CO2 estimado provisional:") == nullptr,
         "formateador no publica NaN aunque el estado declare disponibilidad");
  const auto repeated = controller.observe(2000, true, 2000.0, noEnvironment);
  expect(!repeated.ppm.isValid() && repeated.ppm.status == Status::SampleNotDue,
         "no estimar dos veces dentro de un mismo intervalo");
  observation = acquire(controller, 40000, 1500, 1500.0);
  expect(observation.ppm.isValid() &&
             controller.record()->normalizedReference.qReference == fixedQ &&
             controller.record()->sampleCount == 3 &&
             controller.record()->samples[0].acquired.adcRaw == 2000,
         "lecturas posteriores conservan q_referencia y evidencia original");
  observation = acquire(controller, 42000, 4095);
  expect(!observation.ppm.isValid() && observation.ppm.status == Status::AdcSaturated &&
             observation.state == State::AdcDisponible &&
             controller.record()->normalizedReference.qReference == fixedQ,
         "ADC saturado bloquea ppm sin recalcular la referencia");
  formatObservation(observation, output, sizeof(output));
  expect(std::strstr(output, "saturado") != nullptr &&
             std::strstr(output, "CO2 estimado provisional:") == nullptr,
         "salida de saturación sin ppm numéricas");
  const double invalidMv[] = {0.0, -1.0, 149.0, 2451.0,
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity()};
  std::uint32_t time = 44000;
  for (double mv : invalidMv) {
    observation = acquire(controller, time, 2000, mv);
    expect(!observation.ppm.isValid(), "rechazar tensiones inválidas o fuera de rango");
    time += 2000;
  }
  observation = acquire(controller, time, 2000, 2000.0, false);
  expect(!observation.ppm.isValid() &&
             observation.ppm.status == Status::ConversionUnavailable,
         "fallo de conversión no reutiliza tensión previa");
  time += 2000;
  controller.disableEmission();
  observation = acquire(controller, time);
  expect(!observation.ppm.isValid() && observation.ppm.status == Status::EmissionDisabled,
         "deshabilitar emisión conserva bloqueo en muestras siguientes");
  auto changed = syntheticConfiguration();
  changed.electrical.circuitRevision = 8;
  expect(controller.configure(changed) == Status::Ok && controller.record() == nullptr &&
             controller.state() == State::ReferenciaPendiente,
         "cambiar revisión eléctrica invalida referencia y emisión");
  observation = acquire(controller, time + 2000);
  expect(!observation.ppm.isValid() && observation.ppm.status == Status::ReferenceMissing,
         "configuración nueva no utiliza referencia vieja");
}

void captureFailures() {
  Controller<3> controller;
  controller.begin(0);
  controller.configure(syntheticConfiguration());
  acquire(controller, 30000);
  controller.startReference();
  acquire(controller, 32000);
  auto observation = acquire(controller, 34000, 4095);
  expect(!controller.capturing() && controller.record() == nullptr &&
             observation.captureStatus == Status::AdcSaturated &&
             controller.capturedSamples() == 0,
         "muestra inválida aborta lote entero");
  expect(controller.startReference() == Status::Ok, "reinicio de lote es explícito");
  acquire(controller, 36000);
  acquire(controller, 38000, 1500, 1500.0);
  observation = acquire(controller, 40000);
  expect(observation.captureStatus == Status::UnstableSamples &&
             controller.record() == nullptr && !controller.capturing(),
         "lote inestable no establece referencia ni reintenta solo");
  controller.startReference();
  acquire(controller, 42000);
  controller.cancelReference();
  expect(!controller.capturing() && controller.capturedSamples() == 0 &&
             controller.record() == nullptr, "cancelar descarta lote parcial");
  auto sameRevision = syntheticConfiguration();
  controller.startReference();
  acquire(controller, 44000);
  sameRevision.measuredVcV = 4.9;
  controller.configure(sameRevision);
  expect(!controller.capturing() && controller.capturedSamples() == 0,
         "reaplicar parámetros con el mismo ID también invalida lote");

  auto smallVc = syntheticConfiguration();
  smallVc.measuredVcV = 1.0;
  controller.configure(smallVc);
  observation = acquire(controller, 46000);
  expect(!observation.ppm.isValid() && observation.ppm.status == Status::InvalidInput,
         "VAO mayor o igual a Vc bloquea cálculo matemático");
  controller.configure(syntheticConfiguration());
  controller.startReference();
  acquire(controller, 48000);
  acquire(controller, 50000);
  acquire(controller, 52000);
  expect(controller.record() != nullptr, "nuevo lote válido tras cancelación");
  controller.invalidateReference();
  expect(controller.record() == nullptr && controller.enableEmission() == Status::ReferenceMissing,
         "invalidación explícita bloquea ppm y requiere nueva captura");
}

void ca03Policy() {
  const auto site = mq135_site_config::electricalConfiguration();
  expect(site.referenceSampleCount == 3 &&
             site.maxRelativeSpread == mq135_experimental::NO_SPREAD_LIMIT &&
             site.electricalVerified && site.adcConversionVerified &&
             !site.directAoConnection && site.adcToAoVoltageRatio == 0.5 &&
             site.electrical.circuitRevision == 3 &&
             site.minimumAdcMilliVolts == 100 && site.maximumAdcMilliVolts == 950 &&
             std::fabs(site.electrical.minimumUsefulVaoV - 0.20) < 1e-12 &&
             std::strcmp(site.verificationEvidenceId,
                         "docs/evidencias/t1-5/mq135-divisor-10k-rev3.md") == 0 &&
             std::strcmp(site.electrical.adcConversionId,
                         "EFUSE_VREF-ADC1_CH6-12bit-0dB-REV3-OPERATIVO-100-950mV") == 0,
         "REV3 conserva tres muestras y el rango operativo recomendado a 0 dB");
  // Fixture exclusivamente SINTÉTICO para ejercitar la ruta física del
  // controlador puro. No se conecta hardware ni se acredita verificación real.
  auto config = syntheticConfiguration();
  config.referenceSampleCount = site.referenceSampleCount;
  config.maxRelativeSpread = site.maxRelativeSpread;
  Controller<3> controller;
  controller.begin(0);
  controller.configure(config);
  expect(controller.startReference() == Status::WarmupIncomplete,
         "política CA-03 no captura antes del calentamiento");
  controller.poll(30000);
  controller.observe(2000, true, 2000.0, noEnvironment);
  expect(controller.startReference() == Status::Ok,
         "lote de TEST solicitado explícitamente tras calentamiento");
  for (std::size_t i = 0; i < 3; ++i) {
    const auto now = static_cast<std::uint32_t>(32000 + i * SAMPLE_INTERVAL_MS);
    const mq135_experimental::EnvironmentalConditions environment{
        true, 25.0, 50.0, now};
    controller.poll(now);
    const auto result = controller.observe(2000, true, 2000.0, environment);
    expect(!result.ppm.isValid() && result.source == Source::Physical &&
               result.capturedSamples == i + 1 &&
               (i == 2 || controller.record() == nullptr),
           "cada turno nuevo cuenta una muestra; no fija referencia ni emite antes de completar lote");
  }
  const auto record = controller.record();
  expect(record && record->sampleCount == 3 && record->durationMs == 4000 &&
             record->samples[0].acquired.acquisitionUptimeMs == 32000 &&
             record->samples[2].acquired.acquisitionUptimeMs == 36000 &&
             record->samples[2].acquired.environment.available &&
             record->samples[2].acquired.environment.temperatureC == 25.0 &&
             record->samples[2].acquired.environment.humidityPercent == 50.0 &&
             record->samples[2].acquired.vcV == config.measuredVcV,
         "ventana de tres conserva tiempos, q, condiciones DHT y parámetros eléctricos");
  expect(controller.enableEmission() == Status::Ok,
         "habilitación separada acepta referencia de TEST sin exigir HU-08");
  controller.poll(38000);
  const auto emission = controller.observe(2000, true, 2000.0, noEnvironment);
  expect(emission.ppm.isValid() && emission.source == Source::Physical &&
             controller.record() == record,
         "emisión posterior conserva referencia fija y origen físico del controlador");

  controller.invalidateReference();
  expect(controller.startReference() == Status::Ok, "otro lote requiere solicitud explícita");
  for (std::size_t i = 0; i < 3; ++i) {
    controller.poll(static_cast<std::uint32_t>(40000 + i * SAMPLE_INTERVAL_MS));
    const auto result = controller.observe(2000, true, i == 2 ? 1900.0 : 2000.0,
                                           noEnvironment);
    if (i == 2) {
      const auto varied = controller.record();
      const double meanQ = (1.5 + 1.5 + (5.0 - 1.9) / 1.9) / 3.0;
      expect(result.captureStatus == Status::Ok && varied &&
                 varied->observedRelativeSpread > 0.05 && !controller.capturing() &&
                 std::fabs(varied->normalizedReference.qReference - meanQ) < 1e-12,
             "CA-03 acepta media de tres q válidos sin imponer el antiguo 5 %");
    }
  }
}

void rev3OperatingRange() {
  // ADC y mV inyectados son SINTÉTICOS e independientes: no prueban la curva
  // EFUSE_VREF ni constituyen nuevos pares medidos del montaje REV3.
  auto config = mq135_site_config::electricalConfiguration();
  std::strcpy(config.verificationEvidenceId, "EVIDENCIA SINTÉTICA REV3");
  std::strcpy(config.electrical.adcConversionId, "CONVERSIÓN SINTÉTICA REV3");
  std::strcpy(config.electrical.circuitDescription, "DIVISOR SINTÉTICO k=0.5");
  Controller<3> controller;
  controller.begin(0);
  expect(controller.configure(config) == Status::Ok,
         "fixture REV3 admite rango operativo sin acreditar conversión física");
  acquire(controller, 30000, 511, 191.0);
  expect(controller.startReference() == Status::Ok,
         "referencia de TEST comienza después de 30 segundos");
  acquire(controller, 32000, 511, 191.0);
  acquire(controller, 34000, 511, 191.0);
  acquire(controller, 36000, 511, 191.0);
  const auto record = controller.record();
  expect(record && record->sampleCount == 3 && record->durationMs == 4000 &&
             std::fabs(record->samples[0].acquired.vaoV - 0.382) < 1e-12,
         "referencia conserva tres turnos a 2000 ms y reconstruye VAO con k=0.5");
  if (!record) return;
  const double fixedQ = record->normalizedReference.qReference;
  expect(controller.enableEmission() == Status::Ok,
         "emisión de TEST requiere referencia aceptada");

  std::uint32_t now = 38000;
  const double validMv[] = {100.0, 150.0, 170.0, 200.0, 300.0, 500.0, 950.0};
  for (double mv : validMv) {
    const auto observation = acquire(controller, now, 1234, mv);
    const auto expected = mq135_experimental::estimateExperimentalPpm(
        config.measuredVcV, mv / 1000.0 / config.adcToAoVoltageRatio, fixedQ);
    expect(observation.measurementStatus == Status::Ok &&
               observation.ppm.isValid() && expected.isValid() &&
               std::fabs(observation.ppm.value - expected.value) < 1e-10 &&
               controller.record()->normalizedReference.qReference == fixedQ,
           "100–950 mV inclusivos permiten GeorgK con referencia fija fuera del contraste puntual");
    now += SAMPLE_INTERVAL_MS;
  }
  const double invalidMv[] = {0.0, -1.0, 99.0, 951.0,
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::infinity()};
  for (double mv : invalidMv) {
    const auto observation = acquire(controller, now, 1234, mv);
    const auto expected = std::isfinite(mv) ? Status::VoltageOutsideAdcRange
                                           : Status::ConversionUnavailable;
    char output[384];
    formatObservation(observation, output, sizeof(output));
    expect(observation.measurementStatus == expected && !observation.ppm.isValid() &&
               std::strstr(output, "CO2 estimado provisional:") == nullptr &&
               controller.record()->normalizedReference.qReference == fixedQ,
           "fuera del rango operativo o mV no finitos bloquean ppm sin cambiar referencia");
    now += SAMPLE_INTERVAL_MS;
  }
  const int invalidRaw[] = {0, 4095};
  for (int raw : invalidRaw) {
    const auto observation = acquire(controller, now, raw, 300.0);
    expect(!observation.ppm.isValid() &&
               observation.ppm.status == (raw == 0 ? Status::AdcZero : Status::AdcSaturated),
           "ampliar mV no admite ADC cero ni saturado");
    now += SAMPLE_INTERVAL_MS;
  }
  expect(std::strcmp(statusMessage(Status::VoltageOutsideAdcRange),
                     "tensión fuera del rango operativo ADC") == 0,
         "bloqueo de rango no afirma verificación metrológica");
}

void documentedDivider() {
  // SOLO SINTÉTICO: compensa VADC/VAO conocido sin acreditar hardware real.
  Controller<3> controller;
  controller.begin(0);
  auto config = syntheticConfiguration();
  config.directAoConnection = false;
  config.adcToAoVoltageRatio = 0.5;
  expect(controller.configure(config) == Status::Ok,
         "divisor documentado admite reconstrucción de AO sin cambiar el modelo");
  acquire(controller, 30000, 1000, 1000.0);
  controller.startReference();
  acquire(controller, 32000, 1000, 1000.0);
  acquire(controller, 34000, 1000, 1000.0);
  acquire(controller, 36000, 1000, 1000.0);
  const auto record = controller.record();
  expect(record && record->samples[0].acquired.vaoV == 2.0 &&
             std::fabs(record->normalizedReference.qReference - 1.5) < 1e-12,
         "referencia usa VAO reconstruida, no la tensión atenuada del GPIO");
  controller.enableEmission();
  const auto observation = acquire(controller, 38000, 1250, 1250.0);
  const auto expected = mq135_experimental::estimateExperimentalPpm(5.0, 2.5, 1.5);
  expect(observation.ppm.isValid() && expected.isValid() &&
             std::fabs(observation.ppm.value - expected.value) < 1e-10,
         "ppm conserva GeorgK normalizado después de reconstruir AO");
  const double invalidRatios[] = {0.0, -0.1, 1.0, 1.1,
      std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()};
  for (double ratio : invalidRatios) {
    config.adcToAoVoltageRatio = ratio;
    expect(controller.configure(config) == Status::InvalidMetadata,
           "divisor sin ratio finito en (0,1) se rechaza e invalida referencia");
  }
  config.directAoConnection = true;
  config.adcToAoVoltageRatio = 0.5;
  expect(controller.configure(config) == Status::InvalidMetadata,
         "AO directo no admite una escala propia de divisor");
}
}  // namespace

int main() {
  std::puts("INTEGRACIÓN MQ-135: DATOS SINTÉTICOS, NO VALIDACIÓN FÍSICA");
  heatingAndTiming();
  configurationGates();
  referenceAndEstimation();
  captureFailures();
  ca03Policy();
  rev3OperatingRange();
  documentedDivider();
  std::printf("SINTÉTICO: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
