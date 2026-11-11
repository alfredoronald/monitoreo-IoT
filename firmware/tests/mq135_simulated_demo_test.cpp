// Pruebas de software SINTÉTICAS. Preparadas sin ejecutar; no validan hardware.
#include "../include/mq135_acquisition.h"
#include "../include/mq135_site_config.h"

#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
using namespace mq135_acquisition;
using mq135_experimental::Status;
unsigned checks = 0;
unsigned failures = 0;
const mq135_experimental::EnvironmentalConditions noEnvironment{};

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTÉTICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

bool hasEstimate(const Observation& observation) {
  char output[384];
  const int length = formatObservation(observation, output, sizeof(output));
  return length >= 0 && static_cast<std::size_t>(length) < sizeof(output) &&
         std::strstr(output, "CO2 estimado provisional:") != nullptr;
}

Configuration physicalFixture() {
  // Evidencia inventada exclusivamente para probar aislamiento de referencias.
  auto config = pendingConfiguration();
  config.electricalVerified = config.adcConversionVerified = true;
  config.measuredVcV = 5.0;
  config.minimumAdcMilliVolts = 150;
  config.maximumAdcMilliVolts = 2450;
  std::strcpy(config.verificationEvidenceId, "SOLO TEST SINTETICO");
  config.electrical.circuitRevision = 1;
  config.electrical.minimumUsableAdcCode = 10;
  config.electrical.minimumUsefulVaoV = 0.15;
  std::strcpy(config.electrical.adcConversionId, "SOLO TEST SINTETICO");
  std::strcpy(config.electrical.circuitDescription, "SOLO TEST SINTETICO");
  config.referenceSampleCount = 2;
  config.maxRelativeSpread = 0.01;
  return config;
}

void warmupAndReplay() {
  Controller<2> demo;
  Controller<2> physical;
  demo.begin(0);
  demo.configureSimulatedDemo();
  physical.begin(0);
  physical.configure(physicalFixture());
  bool neverEarly = true;
  bool labeledWarmup = true;
  // Cada instante anterior al límite; entradas físicas matemáticamente válidas.
  for (std::uint32_t now = 0; now < WARMUP_MS; ++now) {
    demo.poll(now);
    physical.poll(now);
    const auto simulated = demo.observeSimulated();
    const auto acquired = physical.observe(1200, true, 1000.0, noEnvironment);
    char output[384];
    formatObservation(simulated, output, sizeof(output));
    neverEarly = neverEarly && !simulated.ppm.isValid() &&
                 !acquired.ppm.isValid() && !hasEstimate(simulated) &&
                 !hasEstimate(acquired) && std::strstr(output, "ppm") == nullptr;
    labeledWarmup = labeledWarmup &&
                    simulated.source == Source::Simulated &&
                    std::strstr(output, "CALENTANDO [SIMULADO]") != nullptr;
  }
  expect(neverEarly, "ningún instante de 0 a 29999 ms emite ppm");
  expect(labeledWarmup, "calentamiento simulado siempre identificado");
  expect(demo.poll(30000), "turno disponible exactamente a 30000 ms");
  auto first = demo.observeSimulated();
  expect(first.state == State::EstimacionDisponible && first.ppm.isValid() &&
             first.warmupElapsedMs == WARMUP_MS && first.adcRaw == 1200 &&
             std::fabs(first.ppm.value - 400.0) < 1e-10,
         "primera muestra solo después de la espera, sin consumir fixtures antes");
  char output[384];
  const auto length = formatObservation(first, output, sizeof(output));
  expect(length > 0 && static_cast<std::size_t>(length) < sizeof(output) &&
             std::strstr(output, "CO2 estimado provisional: 400.00 ppm [SIMULADO]") &&
             std::strstr(output, "MQ-135 ADC:") == nullptr &&
             std::strstr(output, "[30000 ms]") &&
             std::strstr(output, "no medidos [SIMULADO]"),
         "salida numérica y ADC sintético no se confunden con salida física");
  expect(!hasEstimate(demo.observeSimulated()), "no emitir dos veces en un turno");
  expect(!demo.poll(31999) && !hasEstimate(demo.observeSimulated()),
         "intervalo de 2000 ms no se acorta en demo");
  double previous = first.ppm.value;
  for (std::size_t index = 1; index <= mq135_simulated_demo::SAMPLE_COUNT; ++index) {
    demo.poll(static_cast<std::uint32_t>(30000 + index * SAMPLE_INTERVAL_MS));
    const auto next = demo.observeSimulated();
    const auto fixture = mq135_simulated_demo::sampleAt(index);
    const auto expected = mq135_experimental::estimateExperimentalPpm(
        fixture.vcV, fixture.vaoV, mq135_simulated_demo::FIXED_Q_REFERENCE);
    formatObservation(next, output, sizeof(output));
    expect(next.ppm.isValid() && next.source == Source::Simulated &&
               std::fabs(next.ppm.value - expected.value) < 1e-9 &&
               std::strstr(output, "ppm [SIMULADO]") &&
               std::strstr(output, "MQ-135 ADC:") == nullptr,
           "cada turno conserva referencia fija y etiqueta simulada");
    if (index <= 2) expect(next.ppm.value > previous, "modelo responde al cambio sintético");
    if (index == mq135_simulated_demo::SAMPLE_COUNT)
      expect(std::fabs(next.ppm.value - first.ppm.value) < 1e-10,
             "secuencia se repite sin recalibrar referencia");
    previous = next.ppm.value;
  }
  expect(!demo.electricalReady() && demo.record() == nullptr && !demo.capturing(),
         "demo no verifica electricidad ni establece un registro físico");
}

void isolationAndReset() {
  expect(mq135_site_config::electricalConfiguration().adcConversionVerified &&
             mq135_site_config::electricalConfiguration().electricalVerified,
         "la configuración física REV3 tiene evidencia externa independiente de la demo");
  expect(mq135_site_config::SIMULATED_DEMO == (MQ135_SIMULATED_DEMO == 1),
         "selección de compilación explícita");
  Controller<2> controller;
  controller.begin(0);
  controller.configure(physicalFixture());
  controller.poll(30000);
  controller.observe(1200, true, 1000.0, noEnvironment);
  expect(controller.startReference() == Status::Ok, "preparar lote físico de TEST");
  for (std::uint32_t now : {32000U, 34000U}) {
    controller.poll(now);
    controller.observe(1200, true, 1000.0, noEnvironment);
  }
  expect(controller.record() && controller.enableEmission() == Status::Ok,
         "referencia previa existe exclusivamente con evidencia de TEST");
  controller.configureSimulatedDemo();
  expect(controller.record() == nullptr && !controller.electricalReady() &&
             controller.startReference() == Status::InvalidProvenance &&
             controller.enableEmission() == Status::InvalidProvenance,
         "entrar en demo borra referencia física y rechaza APIs de captura/habilitación");
  controller.poll(36000);
  const auto wrongPhysical = controller.observe(1200, true, 1000.0, noEnvironment);
  expect(!wrongPhysical.ppm.isValid() && !hasEstimate(wrongPhysical) &&
             wrongPhysical.source == Source::Simulated &&
             wrongPhysical.measurementStatus == Status::InvalidProvenance,
         "API física no permite inyectar AO en modo simulado");
  expect(!hasEstimate(controller.observeSimulated()),
         "llamada rechazada también consume turno, sin duplicados");
  controller.poll(38000);
  expect(hasEstimate(controller.observeSimulated()), "demo permanece operativa tras rechazo");
  controller.disableEmission();
  controller.poll(40000);
  expect(controller.observeSimulated().ppm.status == Status::EmissionDisabled,
         "deshabilitar emisión bloquea demo también");
  controller.configureSimulatedDemo();
  controller.invalidateReference();
  controller.poll(42000);
  expect(controller.observeSimulated().ppm.status == Status::ReferenceMissing,
         "invalidar referencia demo bloquea sin reconstruirla automáticamente");
  controller.configureSimulatedDemo();
  controller.configure(mq135_site_config::electricalConfiguration());
  controller.poll(44000);
  const auto normal = controller.observe(511, true, 191.0, noEnvironment);
  expect(controller.source() == Source::Physical && controller.record() == nullptr &&
             normal.source == Source::Physical && !normal.ppm.isValid() &&
             normal.ppm.status == Status::ReferenceMissing &&
             controller.enableEmission() == Status::ReferenceMissing,
         "regreso al modo físico REV3 exige referencia física sin heredar la demo");
  controller.poll(46000);
  const auto wrongDemo = controller.observeSimulated();
  expect(wrongDemo.source == Source::Simulated && !hasEstimate(wrongDemo) &&
             wrongDemo.ppm.status == Status::InvalidProvenance,
         "API demo rechazada en modo físico mantiene procedencia simulada");
  controller.begin(50000);
  controller.configureSimulatedDemo();
  controller.poll(79999);
  expect(!hasEstimate(controller.observeSimulated()) && !controller.warmupComplete(),
         "nuevo arranque exige otros 30 segundos desde begin");
  controller.poll(80000);
  expect(!hasEstimate(controller.observeSimulated()), "no crear turno extra al finalizar espera");
  controller.poll(81999);
  expect(hasEstimate(controller.observeSimulated()), "demo usa siguiente turno tras espera con jitter");
}

void defensiveValidation() {
  using mq135_simulated_demo::Sample;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const Sample invalid[] = {
      {-1, 5.0, 1.0}, {4096, 5.0, 1.0}, {4095, 5.0, 1.0},
      {0, 5.0, 1.0}, {9, 5.0, 1.0}, {1200, 5.0, 0.01},
      {1200, 5.0, 0.0}, {1200, 5.0, -1.0}, {1200, 5.0, 5.0},
      {1200, 5.0, 6.0}, {1200, 0.0, 1.0}, {1200, nan, 1.0},
      {1200, inf, 1.0}, {1200, 5.0, nan}, {1200, 5.0, inf}};
  for (const auto& sample : invalid)
    expect(!mq135_simulated_demo::estimate(sample, 4.0).isValid(),
           "demo conserva validaciones ADC, umbrales y tensiones no finitas");
  for (double reference : {0.0, -1.0, nan, inf,
                           std::numeric_limits<double>::max(),
                           std::numeric_limits<double>::denorm_min()})
    expect(!mq135_simulated_demo::estimate({1200, 5.0, 1.0}, reference).isValid(),
           "referencia inválida, overflow y underflow no producen ppm demo");

  Controller<2> demo;
  demo.begin(0);
  demo.configureSimulatedDemo();
  demo.poll(30000);
  const auto valid = demo.observeSimulated();
  for (double value : {nan, inf, -inf, 0.0, -1.0}) {
    auto corrupt = valid;
    corrupt.ppm.value = value;
    char output[384];
    formatObservation(corrupt, output, sizeof(output));
    expect(!hasEstimate(corrupt) && std::strstr(output, "[SIMULADO]"),
           "formateador rechaza ppm inválidas conservando etiqueta");
  }
  auto early = valid;
  early.warmupElapsedMs = WARMUP_MS - 1;
  expect(!hasEstimate(early), "formateador bloquea estado inconsistente antes de 30 segundos");
  auto unknown = valid;
  unknown.source = static_cast<Source>(99);
  expect(!hasEstimate(unknown), "procedencia desconocida no se imprime como lectura física");
  auto badAdc = valid;
  badAdc.adcStatus = Status::AdcSaturated;
  expect(!hasEstimate(badAdc), "formateador no elude validación ADC en demo");
  auto badMeasurement = valid;
  badMeasurement.measurementStatus = Status::InvalidInput;
  expect(!hasEstimate(badMeasurement), "formateador no elude rechazo de la muestra demo");
  char shortBuffer[64];
  expect(formatObservation(valid, shortBuffer, sizeof(shortBuffer)) >=
             static_cast<int>(sizeof(shortBuffer)),
         "truncamiento de etiqueta detectado para descartar todo el mensaje");
}

void rollover() {
  Controller<2> demo;
  const auto start = std::numeric_limits<std::uint32_t>::max() - 1000U;
  demo.begin(start);
  demo.configureSimulatedDemo();
  demo.poll(static_cast<std::uint32_t>(start + 29999U));
  expect(!hasEstimate(demo.observeSimulated()), "rollover no acorta el calentamiento");
  demo.poll(static_cast<std::uint32_t>(start + 30000U));
  expect(demo.warmupComplete() && !hasEstimate(demo.observeSimulated()),
         "rollover conserva frontera temporal e intervalo");
  demo.poll(static_cast<std::uint32_t>(start + 31999U));
  const auto result = demo.observeSimulated();
  expect(result.ppm.isValid() && result.warmupElapsedMs == 31999 &&
             result.source == Source::Simulated,
         "primera emisión tras rollover respeta tiempo relativo y procedencia");
}
}  // namespace

int main() {
  std::puts("DEMO ESP32: PRUEBAS SINTÉTICAS, NO MEDICIONES NI CALIBRACIÓN");
  warmupAndReplay();
  isolationAndReset();
  defensiveValidation();
  rollover();
  std::printf("SINTÉTICO: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
