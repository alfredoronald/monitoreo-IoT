// SOLO DATOS SINTETICOS. No acredita RL, Rs fisica, estabilidad ni calibracion Ro.
#include "../include/mq135_site_config.h"
#include "../include/mq135_adc_filter.h"
#include "../include/mqtt_publication.h"
#include "../include/serial_line_parser.h"
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using mq135_experimental::Status;
using mq135_acquisition::Controller;
unsigned checks = 0, failures = 0;
void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTETICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}
bool near(double actual, double expected) {
  return std::isfinite(actual) &&
      std::fabs(actual - expected) <= 1e-10 * (1.0 + std::fabs(expected));
}
mq135_calibration::LoadConfiguration syntheticLoad() {
  mq135_calibration::LoadConfiguration load{};
  load.effectiveLoadOhms = 1000.0;  // FICTICIO, nunca configuracion de placa.
  load.topologyVerified = true;
  load.circuitRevision = 7;
  std::strcpy(load.evidenceId, "FIXTURE SINTETICO; NO EVIDENCIA FISICA");
  return load;
}
mq135_acquisition::Configuration syntheticConfiguration(bool experimental = false) {
  auto config = mq135_acquisition::pendingConfiguration();
  config.electricalVerified = config.adcConversionVerified = true;
  config.directAoConnection = false;
  config.adcToAoVoltageRatio = 0.5;
  config.measuredVcV = 5.0;
  config.minimumAdcMilliVolts = 100;
  config.maximumAdcMilliVolts = 950;
  config.electrical.circuitRevision = 7;
  config.electrical.minimumUsableAdcCode = 1;
  config.electrical.minimumUsefulVaoV = 0.2;
  std::strcpy(config.verificationEvidenceId, "FIXTURE SINTETICO");
  std::strcpy(config.electrical.adcConversionId, "ADC SINTETICO");
  std::strcpy(config.electrical.circuitDescription, "DIVISOR SINTETICO");
  config.referenceSampleCount = 3;
  config.maxRelativeSpread = mq135_experimental::NO_SPREAD_LIMIT;
  config.calibrationLoad = syntheticLoad();
  config.calibrationLoad.experimentalEstimate = experimental;
  config.calibrationLoad.topologyVerified = !experimental;
  return config;
}
const mq135_experimental::EnvironmentalConditions noEnvironment{
    false, 0.0, 0.0, 0};
mq135_acquisition::Observation acquire(Controller<3>& controller, std::uint32_t now,
                                      int raw = 1000, double mv = 500.0,
                                      bool voltageAvailable = true) {
  controller.poll(now);
  return controller.observe(raw, voltageAvailable, mv, noEnvironment);
}

void formulaAndLoad() {
  using namespace mq135_calibration;
  const auto load = syntheticLoad();
  expect(near(resistanceOhms(load, 7, 5.0, 1.0).value, 4000.0),
         "Rs=RL*(Vc/VAO-1); salida en ohmios, no kOhm");
  expect(near(resistanceOhms(load, 7, 5.0, 1.0).value / 1000.0, 4.0),
         "conversion explicita a kOhm para futura ro_kohm");
  // Carga sintetica de modulo en paralelo con divisor; no transfiere RL a placa.
  auto parallel = load;
  parallel.effectiveLoadOhms = 1.0 / (1.0 / 10000.0 + 1.0 / 20000.0);
  const double vao = 5.0 * parallel.effectiveLoadOhms /
                     (20000.0 + parallel.effectiveLoadOhms);
  expect(near(resistanceOhms(parallel, 7, 5.0, vao).value, 20000.0),
         "usar carga efectiva y AO reconstruida recupera Rs del circuito ideal");
  expect(!near(resistanceOhms(parallel, 7, 5.0, vao * 0.5).value, 20000.0),
         "tension posterior al divisor NO es VAO");
  auto invalid = load;
  invalid.topologyVerified = false;
  expect(resistanceOhms(invalid, 7, 5, 1).status == Status::LoadNotVerified,
         "topologia sin verificar bloquea Rs");
  invalid = load; invalid.evidenceId[0] = '\0';
  expect(validateLoad(invalid, 7) == Status::LoadNotVerified, "evidencia vacia bloquea RL");
  invalid = load; std::memset(invalid.evidenceId, 'x', sizeof(invalid.evidenceId));
  expect(validateLoad(invalid, 7) == Status::LoadNotVerified, "evidencia sin terminador rechazada");
  expect(validateLoad(load, 3) == Status::ConfigurationMismatch &&
         validateLoad(load, 0) == Status::ConfigurationMismatch,
         "RL verificada no sirve para otra revision ni revision cero");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (double value : {0.0, -1.0, nan, inf, -inf}) {
    invalid = load; invalid.effectiveLoadOhms = value;
    expect(!resistanceOhms(invalid, 7, 5, 1).isValid(), "RL no positiva/no finita rechazada");
    expect(!resistanceOhms(load, 7, value, 1).isValid(), "Vc no positiva/no finita rechazada");
    expect(!resistanceOhms(load, 7, 5, value).isValid(), "VAO no positiva/no finita rechazada");
  }
  expect(!resistanceOhms(load, 7, 5, 5).isValid() &&
         !resistanceOhms(load, 7, 5, 6).isValid(), "VAO mayor o igual que Vc rechazada");
  invalid = load; invalid.effectiveLoadOhms = std::numeric_limits<double>::max();
  expect(resistanceOhms(invalid, 7, 5, 1).status == Status::NumericalError,
         "overflow de Rs rechazado");
  invalid.effectiveLoadOhms = std::numeric_limits<double>::denorm_min();
  expect(resistanceOhms(invalid, 7, 5, 4).status == Status::NumericalError,
         "underflow de Rs a cero rechazado");
  const auto defaults = mq135_site_config::electricalConfiguration();
  expect(!defaults.calibrationLoad.topologyVerified &&
         defaults.calibrationLoad.experimentalEstimate == (MQ135_EXPERIMENTAL_RS == 1) &&
         validateLoad(defaults.calibrationLoad, defaults.electrical.circuitRevision) != Status::Ok,
         "configuracion nominal nunca acredita RL ni topologia fisicamente verificadas");
  expect(MQ135_EXPERIMENTAL_RS
             ? near(defaults.calibrationLoad.effectiveLoadOhms, 6666.67)
             : defaults.calibrationLoad.effectiveLoadOhms == 0.0,
         "por defecto carga nominal experimental 6666.67 ohm; opt-out estricto sin carga");
  Controller<3> unconfigured;
  unconfigured.begin(0); unconfigured.configure(defaults); unconfigured.startCalibration();
  acquire(unconfigured, 30000);
  expect(unconfigured.calibration().lastQ().isValid() &&
         (MQ135_EXPERIMENTAL_RS
              ? unconfigured.calibration().validSamples() == 1 &&
                near(unconfigured.calibration().lastResistance().value, 6666.67 * 3.89)
              : unconfigured.calibration().validSamples() == 0 &&
                unconfigured.calibration().lastResistance().status == Status::LoadNotVerified),
         "captura nominal requiere opcion experimental; modo estricto sigue bloqueado");
}

void experimentalEstimate() {
  using namespace mq135_calibration;
  auto config = syntheticConfiguration(true);
  auto& load = config.calibrationLoad;
  load.effectiveLoadOhms = 6666.67;
  std::strcpy(load.evidenceId, "SUPUESTO NOMINAL SINTETICO; NO MEDICION");
  expect(!load.topologyVerified && load.experimentalEstimate &&
         validateLoad(load, 7) == Status::LoadNotVerified &&
         !resistanceOhms(load, 7, 5, 1).isValid(),
         "estimacion nunca abre API estricta ni verifica la topologia");
  expect(validateLoad(load, 7, true) == Status::Ok &&
         near(resistanceOhms(load, 7, 5, 1, true).value, 26666.68),
         "opt-in experimental calcula Rs estimada en ohmios");
  auto invalid = load;
  invalid.experimentalEstimate = false;
  expect(validateLoad(invalid, 7, true) == Status::LoadNotVerified,
         "permitir estimaciones no elude verificacion del modo estricto");
  invalid = load; invalid.topologyVerified = true;
  expect(validateLoad(invalid, 7, true) == Status::LoadNotVerified &&
         validateLoad(invalid, 7) == Status::LoadNotVerified,
         "supuesto experimental y bandera verificada incompatibles fallan cerrados");
  invalid = load; invalid.evidenceId[0] = '\0';
  expect(validateLoad(invalid, 7, true) == Status::LoadNotVerified,
         "estimacion exige referencia documental del supuesto");
  invalid = load; std::memset(invalid.evidenceId, 'x', sizeof(invalid.evidenceId));
  expect(validateLoad(invalid, 7, true) == Status::LoadNotVerified,
         "estimacion rechaza identificador sin terminador");
  expect(validateLoad(load, 3, true) == Status::ConfigurationMismatch &&
         validateLoad(load, 0, true) == Status::ConfigurationMismatch,
         "estimacion conserva control de revision electrica");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (double value : {0.0, -1.0, nan, inf, -inf}) {
    invalid = load; invalid.effectiveLoadOhms = value;
    expect(!resistanceOhms(invalid, 7, 5, 1, true).isValid() &&
           !resistanceOhms(load, 7, value, 1, true).isValid() &&
           !resistanceOhms(load, 7, 5, value, true).isValid(),
           "estimacion rechaza carga y tensiones no positivas o no finitas");
  }
  expect(!resistanceOhms(load, 7, 5, 5, true).isValid() &&
         !resistanceOhms(load, 7, 5, 6, true).isValid(),
         "estimacion rechaza AO mayor o igual que Vc");
  invalid = load; invalid.effectiveLoadOhms = std::numeric_limits<double>::max();
  expect(resistanceOhms(invalid, 7, 5, 1, true).status == Status::NumericalError,
         "estimacion rechaza overflow de Rs");
  invalid.effectiveLoadOhms = std::numeric_limits<double>::denorm_min();
  expect(resistanceOhms(invalid, 7, 5, 4, true).status == Status::NumericalError,
         "estimacion rechaza underflow de Rs");
  Controller<3> controller;
  controller.begin(0); controller.configure(config); controller.startCalibration();
  acquire(controller, 30000);
  acquire(controller, 32000, 1000, 250);
  expect(controller.calibration().validSamples() == 2 &&
         near(controller.calibration().meanOhms(), 43333.355) &&
         !controller.calibrationLoad().topologyVerified,
         "captura acumula promedio estimado sin promover verificacion");
  const auto mean = controller.calibration().meanOhms();
  acquire(controller, 34000, 4095);
  expect(controller.calibration().validSamples() == 2 &&
         controller.calibration().rejectedSamples() == 1 &&
         near(controller.calibration().meanOhms(), mean),
         "rechazo ADC no contamina promedio estimado");
  char output[512];
  const int length = mq135_acquisition::formatCalibration(
      controller.calibration(), load, 7, output, sizeof(output));
  expect(length > 0 && static_cast<std::size_t>(length) < sizeof(output) &&
         std::strstr(output, "[FISICO] [EXPERIMENTAL]") &&
         std::strstr(output, "Rs_estimada_ohm=NO_DISPONIBLE") &&
         std::strstr(output, "promedio_Rs_estimada_ohm=43333.355") &&
         std::strstr(output, "validas=2; rechazadas=1") &&
         std::strstr(output, "RL=NOMINAL_NO_MEDIDA") &&
         std::strstr(output, "RL_efectiva_ohm=6666.67") &&
         std::strstr(output, "topologia=SUPUESTA_NO_VERIFICADA") &&
         std::strstr(output, "NO es Ro") && !std::strstr(output, "ppm"),
         "Serial identifica estimacion, carga nominal, topologia supuesta y rechazo");
  controller.stopCalibration();
  mq135_acquisition::formatCalibration(controller.calibration(), load, 7, output, sizeof(output));
  expect(std::strstr(output, "INACTIVA") && std::strstr(output, "[EXPERIMENTAL]") &&
         std::strstr(output, "promedio_Rs_estimada_ohm=43333.355"),
         "resumen detenido conserva etiqueta y promedio estimados");
  controller.startCalibration();
  for (unsigned i = 0; i < 512; ++i) acquire(controller, 36000 + 2000 * i);
  expect(controller.calibration().validSamples() == 512 &&
         near(controller.calibration().meanOhms(), 26666.68) &&
         controller.record() == nullptr && !controller.calibrationLoad().topologyVerified,
         "512 muestras estimadas sin Ro ni referencia ppm ni verificacion inventada");
  mq135_acquisition::formatCalibration(controller.calibration(), load, 7, output, sizeof(output));
  expect(std::strstr(output, "Rs_estimada_ohm=26666.68") &&
         std::strstr(output, "promedio_Rs_estimada_ohm=26666.68"),
         "Serial imprime Rs y promedio numericos como estimados");
}

void accumulator() {
  mq135_calibration::Capture capture;
  const auto load = syntheticLoad();
  expect(!capture.active() && capture.validSamples() == 0 && !std::isfinite(capture.meanOhms()),
         "arranque desactivado sin promedio fisico ficticio");
  expect(capture.start(false) == Status::InvalidProvenance && !capture.active(),
         "fuente simulada no inicia captura fisica");
  expect(capture.start(true) == Status::Ok, "iniciar captura fisica habilita acumulador");
  capture.observe(Status::Ok, load, 7, 5, 1);
  expect(capture.validSamples() == 1 && near(capture.meanOhms(), 4000), "promedio de una muestra");
  capture.observe(Status::Ok, load, 7, 5, 2);
  expect(capture.validSamples() == 2 && near(capture.meanOhms(), 2750), "promedio de dos muestras");
  capture.start(true);
  expect(capture.validSamples() == 2, "iniciar repetido no reinicia captura activa");
  capture.observe(Status::AdcSaturated, load, 7, 5, 1);
  expect(capture.validSamples() == 2 && capture.rejectedSamples() == 1 &&
         near(capture.meanOhms(), 2750), "rechazo no modifica n valido ni promedio");
  expect(capture.stop() && !capture.stop() && near(capture.meanOhms(), 2750),
         "detener es idempotente y conserva resumen");
  capture.observe(Status::Ok, load, 7, 5, 1);
  expect(capture.validSamples() == 2, "inactiva no acumula");
  capture.start(true);
  expect(capture.validSamples() == 0 && capture.rejectedSamples() == 0 &&
         !std::isfinite(capture.meanOhms()), "nueva captura reinicia contadores y promedio");
  for (unsigned i = 1; i <= 512; ++i) {
    const double rs = static_cast<double>(i) * 1000.0;
    capture.observe(Status::Ok, load, 7, 5, 5.0 / (1.0 + rs / load.effectiveLoadOhms));
    if (i == 50)
      expect(capture.validSamples() == 50 && near(capture.meanOhms(), 25500),
             "50 muestras sin almacenar lote");
  }
  expect(capture.validSamples() == 512 && near(capture.meanOhms(), 256500),
         "512 muestras mantienen media incremental");
  capture.reset();
  capture.start(true);
  auto large = load; large.effectiveLoadOhms = 1e307;
  for (unsigned i = 0; i < 50; ++i) capture.observe(Status::Ok, large, 7, 5, 1);
  expect(capture.validSamples() == 50 && near(capture.meanOhms(), 4e307),
         "media estable aunque la suma directa desbordaria double");
  capture.reset(); capture.start(true);
  auto missing = load; missing.topologyVerified = false;
  capture.observe(Status::Ok, missing, 7, 5, 1);
  expect(capture.lastQ().isValid() && near(capture.lastQ().value, 4) &&
         !capture.lastResistance().isValid() && capture.validSamples() == 0 &&
         capture.rejectedSamples() == 1, "sin RL solo q adimensional; ninguna Rs aceptada");
  char output[512];
  const int length = mq135_acquisition::formatCalibration(capture, missing, 7,
                                                         output, sizeof(output));
  expect(length > 0 && static_cast<std::size_t>(length) < sizeof(output) &&
         std::strstr(output, "Rs_ohm=NO_DISPONIBLE") &&
         std::strstr(output, "promedio_Rs_ohm=NO_DISPONIBLE") &&
         std::strstr(output, "q_adimensional=4") && std::strstr(output, "NO es Ro") &&
         !std::strstr(output, "nan"), "Serial distingue diagnostico de resistencia fisica y Ro");
  capture.observe(Status::Ok, load, 7, 5, 1);
  mq135_acquisition::formatCalibration(capture, load, 7, output, sizeof(output));
  expect(std::strstr(output, "Rs_ohm=4000") && std::strstr(output, "promedio_Rs_ohm=4000") &&
         std::strstr(output, "validas=1"), "Serial informa Rs, media y contador con unidades");
}

void acquisitionAndModes(bool experimental) {
  Controller<3> controller;
  expect(controller.startCalibration() == Status::InvalidProvenance, "sin begin no iniciar");
  controller.begin(0);
  controller.configure(syntheticConfiguration(experimental));
  expect(!controller.calibration().active(), "controlador arranca en modo normal");
  controller.startCalibration();
  acquire(controller, 2000);
  expect(controller.calibration().validSamples() == 0 &&
         controller.calibration().lastResistance().status == Status::WarmupIncomplete,
         "calentamiento no produce Rs ni lecturas aceptadas");
  expect(controller.startReference() == Status::CalibrationActive &&
         controller.enableEmission() == Status::CalibrationActive,
         "comandos de referencia/ppm bloqueados durante captura");
  unsigned reads = 0;
  const auto median = mq135_adc_filter::acquire([&reads]() { ++reads; return 1000; });
  auto observation = acquire(controller, 30000, median.raw);
  expect(reads == 11 && controller.calibration().validSamples() == 1 &&
         near(controller.calibration().lastResistance().value, 4000),
         "11 lecturas internas son una muestra; VAO=500mV/0.5=1V");
  expect(!observation.ppm.isValid() && controller.record() == nullptr && !controller.capturing(),
         "captura no crea referencia provisional ni ppm");
  controller.observe(1000, true, 500, noEnvironment);
  expect(controller.calibration().validSamples() == 1, "no consumir dos veces un turno");
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  std::uint32_t now = 32000;
  for (int raw : {0, 4095, -1, 4096}) {
    observation = acquire(controller, now, raw); now += 2000;
    expect(!controller.calibration().lastResistance().isValid(), "ADC cero/saturado/fuera de rango rechazado");
  }
  for (double mv : {0.0, -10.0, 99.0, 951.0, nan, inf, -inf}) {
    observation = acquire(controller, now, 1000, mv); now += 2000;
    expect(!controller.calibration().lastResistance().isValid(), "tension ADC invalida rechazada");
  }
  acquire(controller, now, 1000, 500, false); now += 2000;
  expect(controller.calibration().validSamples() == 1 && controller.calibration().rejectedSamples() == 13,
         "rechazos ADC/conversion no contaminan acumulado");
  expect(controller.stopCalibration() && !controller.stopCalibration() &&
         controller.calibration().validSamples() == 1 && controller.record() == nullptr,
         "detener congela resumen y exige referencia nueva");
  auto pending = acquire(controller, now); now += 2000;
  expect(!pending.ppm.isValid(), "salida de captura no recupera ppm antiguas");
  expect(controller.startReference() == Status::Ok, "recuperacion permite referencia normal");
  for (unsigned i = 0; i < 3; ++i) { acquire(controller, now); now += 2000; }
  expect(controller.record() != nullptr && controller.record()->sampleCount == 3 &&
         controller.enableEmission() == Status::Ok, "tres muestras nuevas recuperan referencia y emision");
  observation = acquire(controller, now); now += 2000;
  expect(observation.ppm.isValid(), "modo normal vuelve a producir estimacion provisional");
  controller.startCalibration();
  expect(controller.record() == nullptr && controller.calibration().validSamples() == 0,
         "nueva captura elimina referencia anterior y reinicia media");
  acquire(controller, now);
  expect(controller.calibration().validSamples() == 1, "captura repetida acepta muestra nueva");
  controller.configure(syntheticConfiguration(experimental));
  expect(!controller.calibration().active() && controller.calibration().validSamples() == 0,
         "reconfiguracion descarta captura de otra configuracion");
  controller.configureSimulatedDemo();
  expect(controller.startCalibration() == Status::InvalidProvenance && !controller.calibration().active(),
         "entorno demo no puede calibrar fisicamente");
  Controller<3> impossible;
  impossible.begin(0);
  auto bad = syntheticConfiguration(experimental); bad.measuredVcV = 0.5;
  impossible.configure(bad); impossible.startCalibration();
  acquire(impossible, 30000);
  expect(impossible.calibration().validSamples() == 0 &&
         impossible.calibration().lastResistance().status == Status::InvalidInput,
         "VAO reconstruida mayor que Vc bloqueada en controlador");
  for (unsigned variant = 0; variant < 4; ++variant) {
    auto config = syntheticConfiguration(experimental);
    if (variant == 0) config.electricalVerified = false;
    if (variant == 1) config.adcConversionVerified = false;
    if (variant == 2) config.adcToAoVoltageRatio = 0.0;
    if (variant == 3) config.measuredVcV = nan;
    Controller<3> gated; gated.begin(0); gated.configure(config); gated.startCalibration();
    acquire(gated, 30000);
    expect(gated.calibration().validSamples() == 0 && !gated.calibration().lastQ().isValid(),
           "sin verificacion electrica/conversion/configuracion no q ni Rs");
  }
  Controller<3> rollover;
  const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - 1000;
  rollover.begin(start); rollover.configure(syntheticConfiguration(experimental)); rollover.startCalibration();
  acquire(rollover, static_cast<std::uint32_t>(start + 30000U));
  expect(rollover.calibration().validSamples() == 1, "captura respeta rollover de millis");
}

void parserCompatibility() {
  using namespace serial_line_parser;
  Parser parser;
  std::vector<std::string> commands;
  unsigned rejected = 0;
  Controller<3> controller; controller.begin(0); controller.configure(syntheticConfiguration());
  auto feed = [&](const std::string& input) {
    for (char c : input) parser.feed(c, [&](const char* command) {
      commands.push_back(command);
      switch (calibrationCommand(command)) {
        case CalibrationCommand::Start: controller.startCalibration(); break;
        case CalibrationCommand::Stop: controller.stopCalibration(); break;
        default: break;
      }
    }, [&]() { ++rejected; });
  };
  feed("mq135 calibra");
  expect(commands.empty(), "linea parcial no ejecuta comando");
  feed("cion iniciar\r\n");
  expect(commands.size() == 1 && controller.calibration().active(), "CRLF ejecuta iniciar una sola vez");
  acquire(controller, 30000);
  feed("mq135 calibracion iniciar\nmq135 calibracion estado\r");
  expect(controller.calibration().validSamples() == 1 &&
         calibrationCommand(commands.back().c_str()) == CalibrationCommand::State,
         "iniciar/estado repetidos no reinician acumulador ni adquieren ADC");
  feed("mq135 estado\nmq135 ayuda\nmq135 adc diagnostico\n");
  feed("wifi prueba desconectar\nmqtt prueba co2 omitir\n");
  expect(calibrationCommand(commands.back().c_str()) == CalibrationCommand::None &&
         commands[4] == "mq135 ayuda" && commands[6] == "wifi prueba desconectar",
         "parser conserva comandos MQ135, WiFi y MQTT existentes");
  feed("mq135 calibracion detener extra\n");
  expect(controller.calibration().active(), "comando con sufijo no detiene captura");
  feed(std::string(100, 'x') + "\nmq135 calibracion estado\n");
  feed(std::string("mq135 calibracion detener") + '\0' + "oculto\n");
  expect(rejected == 2 && controller.calibration().active(), "overflow y NUL descartan linea entera");
  feed("\n\r\nmq135 calibracion detener\nmq135 calibracion detener\n");
  expect(!controller.calibration().active() && controller.calibration().validSamples() == 1,
         "lineas vacias y detener repetido conservan resumen");
}

void mqttContinuity() {
  using namespace mqtt_publication;
  Snapshot snapshot;
  Scheduler scheduler;
  recordDht(snapshot, 25, 50, 0);
  record(snapshot.co2, SampleState::Valid, 450, 0);
  scheduler.poll(0, true, snapshot);
  recordDht(snapshot, 25, 50, 15000);
  record(snapshot.co2, SampleState::Valid, 450, 15000);
  const auto old = scheduler.poll(15000, true, snapshot);
  expect(old.due && old.messages[2].omission == Omission::None &&
         mayTransmit(old, snapshot, TOPICS[2]), "modo normal publica CO2");
  setCalibrationCapture(snapshot, true, 15001);
  expect(!snapshot.co2SuppressionAcknowledged,
         "solicitar captura no confirma fin de publish en curso");
  expect(snapshot.co2Suppressed && snapshot.co2.state == SampleState::Unavailable &&
         !std::isfinite(snapshot.co2.value) && !mayTransmit(old, snapshot, TOPICS[2]),
         "inicio invalida CO2 y cancela ciclo copiado antes de la orden");
  acknowledgeCalibrationCapture(snapshot);
  expect(snapshot.co2SuppressionAcknowledged && snapshot.co2Suppressed &&
         !mayTransmit(old, snapshot, TOPICS[2]),
         "ACK entre ciclos permite captura sin CO2 preparado anteriormente");
  const auto epoch = snapshot.co2Epoch;
  setCalibrationCapture(snapshot, true, 15002);
  expect(snapshot.co2Epoch == epoch, "iniciar repetido no altera epoca");
  recordDht(snapshot, 26, 51, 30000);
  // Incluso una escritura erronea de CO2 valido queda inhibida por el modo.
  record(snapshot.co2, SampleState::Valid, 450, 30000);
  const auto active = scheduler.poll(30000, true, snapshot);
  expect(active.due && active.messages[0].omission == Omission::None &&
         active.messages[1].omission == Omission::None &&
         active.messages[2].omission == Omission::Unavailable,
         "captura mantiene temperatura/humedad y omite CO2");
  unsigned sent = 0;
  transmit(active, [&](const char* topic, const char*) {
    expect(std::strcmp(topic, TOPICS[2]) != 0 && mayTransmit(active, snapshot, topic),
           "transporte recibe solo DHT22 durante captura");
    ++sent; return true;
  });
  expect(sent == 2, "dos publicaciones DHT validas durante captura");
  recordDht(snapshot, 26, std::numeric_limits<float>::quiet_NaN(), 45000);
  const auto invalidDht = scheduler.poll(45000, true, snapshot);
  expect(invalidDht.messages[0].omission == Omission::None &&
         invalidDht.messages[1].omission == Omission::Invalid,
         "validacion independiente DHT sigue activa en calibracion");
  setCalibrationCapture(snapshot, false, 45001);
  expect(!snapshot.co2SuppressionAcknowledged, "salir descarta ACK anterior");
  expect(!snapshot.co2Suppressed && snapshot.co2.state == SampleState::Unavailable &&
         !mayTransmit(old, snapshot, TOPICS[2]), "salir no revive CO2 de epoca anterior");
  setCalibrationCapture(snapshot, true, 45002);
  expect(!snapshot.co2SuppressionAcknowledged, "reiniciar exige ACK nuevo");
  setCalibrationCapture(snapshot, false, 45002);
  expect(!scheduler.poll(45002, true, snapshot).due, "detener no genera publicacion inmediata o rafaga");
  recordDht(snapshot, 25, 50, 60000);
  const auto recovering = scheduler.poll(60000, true, snapshot);
  expect(recovering.messages[2].omission == Omission::Unavailable,
         "sin referencia/muestra nuevas CO2 permanece omitido tras detener");
  recordDht(snapshot, 25, 50, 75000);
  record(snapshot.co2, SampleState::Valid, 451, 75000);
  const auto resumed = scheduler.poll(75000, true, snapshot);
  expect(resumed.messages[2].omission == Omission::None &&
         mayTransmit(resumed, snapshot, TOPICS[2]), "muestra nueva reanuda CO2 a cadencia normal");
  setCalibrationCapture(snapshot, true, 75001);
  scheduler.poll(76000, false, snapshot);
  expect(!scheduler.poll(90000, true, snapshot).due &&
         !scheduler.poll(104999, true, snapshot).due, "reconexion durante captura espera intervalo completo");
  recordDht(snapshot, 25, 50, 105000);
  const auto reconnect = scheduler.poll(105000, true, snapshot);
  expect(reconnect.due && reconnect.messages[0].omission == Omission::None &&
         reconnect.messages[2].omission == Omission::Unavailable,
         "reconexion preserva DHT y bloqueo CO2 sin backlog");
}
}  // namespace

int main() {
  formulaAndLoad(); experimentalEstimate(); accumulator();
  acquisitionAndModes(false); acquisitionAndModes(true);
  parserCompatibility(); mqttContinuity();
  std::printf("SINTETICO T-8.1: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
