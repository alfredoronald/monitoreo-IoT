// Issue #13: pruebas SINTÉTICAS C++11, sin Arduino, red ni sensores.
// Preparadas, NO ejecutadas. Ningún fixture se incorpora al firmware.
#include "../include/mqtt_publication.h"
#include "../include/mqtt_publication_config.h"
#include "../include/mq135_site_config.h"
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
using namespace mqtt_publication;
unsigned checks = 0;
unsigned failures = 0;

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTETICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

void freshSnapshot(Snapshot& snapshot, std::uint32_t now) {
  recordDht(snapshot, 28.5f, 50.2f, now);
  // Solo fixture de selección/transporte. Los bloqueos físicos se prueban abajo.
  record(snapshot.co2, SampleState::Valid, 415.7, now);
}

unsigned selected(const Cycle& cycle) {
  if (!cycle.due) return 0;
  unsigned count = 0;
  for (const auto& message : cycle.messages) {
    if (message.omission == Omission::None) ++count;
  }
  return count;
}

void intervalAndClock() {
  expect(validInterval(10000) && validInterval(15000) && validInterval(30000),
         "CA-01: limites inclusivos y valor inicial admitidos");
  expect(!validInterval(9999) && !validInterval(30001) && !validInterval(0),
         "CA-01: rechazar intervalos fuera del rango");
  expect(DEFAULT_INTERVAL_MS == 15000 &&
             validInterval(mqtt_publication_config::INTERVAL_MS),
         "valor inicial 15000 ms y configuracion publica dentro de rango");
  Snapshot snapshot;
  for (const auto interval : {10000U, 15000U, 30000U}) {
    Scheduler scheduler(interval);
    expect(!scheduler.poll(1234, true, snapshot).due,
           "primera conexion espera un intervalo completo");
    freshSnapshot(snapshot, 1234 + interval - 1);
    expect(!scheduler.poll(1234 + interval - 1, true, snapshot).due,
           "no publicar un milisegundo antes del intervalo");
    expect(selected(scheduler.poll(1234 + interval, true, snapshot)) == 3,
           "publicar las tres variables al vencer el intervalo configurable");
    expect(!scheduler.poll(1234 + interval, true, snapshot).due,
           "no repetir el ciclo en el mismo timestamp");
    freshSnapshot(snapshot, 1234 + 5 * interval);
    expect(scheduler.poll(1234 + 5 * interval, true, snapshot).due &&
               !scheduler.poll(1235 + 5 * interval, true, snapshot).due,
           "un retraso de varios intervalos produce solo un ciclo");
    expect(!scheduler.poll(1233 + 6 * interval, true, snapshot).due,
           "siguiente plazo contado desde el ciclo efectivo, sin recuperar atrasos");
  }
  Scheduler invalid(30001);
  invalid.poll(0, true, snapshot);
  expect(!invalid.poll(60000, true, snapshot).due, "intervalo invalido nunca publica");

  const auto start = std::numeric_limits<std::uint32_t>::max() - 1000U;
  Scheduler rollover;
  rollover.poll(start, true, snapshot);
  freshSnapshot(snapshot, static_cast<std::uint32_t>(start + 14000U));
  expect(!rollover.poll(static_cast<std::uint32_t>(start + 14999U), true, snapshot).due &&
             selected(rollover.poll(static_cast<std::uint32_t>(start + 15000U),
                                    true, snapshot)) == 3,
         "rollover conserva tanto el intervalo como la edad de la muestra");
}

void numericFormatAndTopics() {
  char payload[PAYLOAD_CAPACITY];
  expect(formatPayload(28.5, payload, sizeof(payload)) && std::strcmp(payload, "28.5") == 0,
         "CA-03: temperatura con un decimal");
  expect(formatPayload(50.2, payload, sizeof(payload)) && std::strcmp(payload, "50.2") == 0,
         "CA-03: humedad sin unidades");
  expect(formatPayload(415.7, payload, sizeof(payload)) && std::strcmp(payload, "415.7") == 0,
         "CA-03: CO2 sin JSON");
  expect(formatPayload(400.0, payload, sizeof(payload)) && std::strcmp(payload, "400.0") == 0,
         "entero conserva .0");
  expect(formatPayload(28.56, payload, sizeof(payload)) && std::strcmp(payload, "28.6") == 0,
         "redondeo a un decimal, sin truncar");
  expect(formatPayload(-12.34, payload, sizeof(payload)) && std::strcmp(payload, "-12.3") == 0,
         "formateador decimal admite signo sin exponente");
  expect(!formatPayload(std::numeric_limits<double>::quiet_NaN(), payload, sizeof(payload)) &&
             !formatPayload(std::numeric_limits<double>::infinity(), payload, sizeof(payload)),
         "no formatear NaN ni infinito");
  expect(!formatPayload(std::numeric_limits<double>::max(), payload, sizeof(payload)) &&
             payload[0] == '\0', "numero que excede el buffer no produce payload parcial");
  char tooSmall[4];
  expect(!formatPayload(28.5, tooSmall, sizeof(tooSmall)) &&
             !formatPayload(0, nullptr, 0), "buffers insuficientes o nulos rechazados");

  Snapshot snapshot;
  Scheduler scheduler;
  scheduler.poll(0, true, snapshot);
  freshSnapshot(snapshot, 14000);
  const auto cycle = scheduler.poll(15000, true, snapshot);
  expect(selected(cycle) == 3 &&
             std::strcmp(cycle.messages[0].topic, "ambiente/temperatura") == 0 &&
             std::strcmp(cycle.messages[1].topic, "ambiente/humedad") == 0 &&
             std::strcmp(cycle.messages[2].topic, "ambiente/co2") == 0,
         "CA-02: tres topics exactos y ninguna otra ruta");
  expect(std::strcmp(cycle.messages[0].payload, "28.5") == 0 &&
             std::strcmp(cycle.messages[1].payload, "50.2") == 0 &&
             std::strcmp(cycle.messages[2].payload, "415.7") == 0,
         "CA-03: payload correcto para cada topic");
}

void independentVariablesAndFreshness() {
  for (std::size_t index = 0; index < VARIABLE_COUNT; ++index) {
    for (const auto state : {SampleState::Invalid, SampleState::Discarded,
                             SampleState::Unavailable}) {
      Snapshot individual;
      Scheduler isolated;
      isolated.poll(0, true, individual);
      freshSnapshot(individual, 14000);
      Reading* readings[] = {&individual.temperature, &individual.humidity, &individual.co2};
      record(*readings[index], state, 0.0, 14000);
      const auto omitted = isolated.poll(15000, true, individual);
      expect(selected(omitted) == 2 && omitted.messages[index].omission != Omission::None,
             "CA-04: cada estado de error omite solo su variable en los tres topics");
    }
  }
  const auto nan = std::numeric_limits<float>::quiet_NaN();
  Snapshot snapshot;
  Scheduler scheduler;
  scheduler.poll(0, true, snapshot);
  auto cycle = scheduler.poll(15000, true, snapshot);
  expect(selected(cycle) == 0 && cycle.messages[0].omission == Omission::Unavailable &&
             cycle.messages[1].omission == Omission::Unavailable &&
             cycle.messages[2].omission == Omission::Unavailable,
         "sin lecturas no inventar valores de arranque");

  freshSnapshot(snapshot, 29000);
  recordDht(snapshot, nan, 50.2f, 29000);
  cycle = scheduler.poll(30000, true, snapshot);
  expect(selected(cycle) == 2 && cycle.messages[0].omission == Omission::Invalid &&
             cycle.messages[1].omission == Omission::None &&
             cycle.messages[2].omission == Omission::None,
         "CA-04: temperatura NaN omite solo temperatura, sin reutilizar anterior");
  expect(!std::isfinite(snapshot.temperature.value),
         "intento invalido borra el valor anterior del snapshot");

  freshSnapshot(snapshot, 44000);
  recordDht(snapshot, 28.5f, 90.1f, 44000);
  cycle = scheduler.poll(45000, true, snapshot);
  expect(selected(cycle) == 2 && cycle.messages[1].omission == Omission::Discarded &&
             cycle.messages[0].omission == Omission::None,
         "CA-04: humedad fuera de rango omite solo humedad");

  freshSnapshot(snapshot, 59000);
  record(snapshot.co2, SampleState::Unavailable, 0.0, 59000);
  cycle = scheduler.poll(60000, true, snapshot);
  expect(selected(cycle) == 2 && cycle.messages[2].omission == Omission::Unavailable,
         "CA-04: CO2 sin referencia/emision no impide publicar DHT22");

  recordDht(snapshot, nan, std::numeric_limits<float>::infinity(), 74000);
  record(snapshot.co2, SampleState::Discarded, 0.0, 74000);
  cycle = scheduler.poll(75000, true, snapshot);
  expect(selected(cycle) == 0 && cycle.messages[2].omission == Omission::Discarded,
         "todas invalidas: ciclo sin publicaciones");

  freshSnapshot(snapshot, 85000);
  cycle = scheduler.poll(90000, true, snapshot);
  expect(selected(cycle) == 3, "edad exactamente 5000 ms todavia reciente");
  freshSnapshot(snapshot, 99999);
  cycle = scheduler.poll(105000, true, snapshot);
  expect(selected(cycle) == 0 && cycle.messages[0].omission == Omission::Stale,
         "edad de 5001 ms no se reutiliza como muestra reciente");

  Snapshot partlyOld;
  Scheduler partial;
  partial.poll(0, true, partlyOld);
  freshSnapshot(partlyOld, 9000);
  recordDht(partlyOld, 28.5f, 50.2f, 14000);
  const auto partialCycle = partial.poll(15000, true, partlyOld);
  expect(selected(partialCycle) == 2 && partialCycle.messages[2].omission == Omission::Stale,
         "CO2 antiguo no impide publicar DHT22 reciente");

  freshSnapshot(snapshot, 119000);
  record(snapshot.co2, SampleState::Valid, std::numeric_limits<double>::max(), 119000);
  cycle = scheduler.poll(120000, true, snapshot);
  expect(selected(cycle) == 2 && cycle.messages[2].omission == Omission::Format,
         "error de formato afecta solo a su propio topic");

  // Datos con timestamp futuro (inconsistencia): resta unsigned los descarta.
  freshSnapshot(snapshot, 136000);
  cycle = scheduler.poll(135000, true, snapshot);
  expect(selected(cycle) == 0 && cycle.messages[0].omission == Omission::Stale,
         "timestamp futuro no pasa como lectura nueva");

  // Fixture específico para comprobar revisión además de la edad.
  freshSnapshot(snapshot, 149000);
  scheduler.poll(150000, true, snapshot);
  snapshot.temperature.timestampMs = 164000;
  snapshot.humidity.timestampMs = 164000;
  snapshot.co2.timestampMs = 164000;
  cycle = scheduler.poll(165000, true, snapshot);
  expect(selected(cycle) == 0 && cycle.messages[0].omission == Omission::AlreadyUsed,
         "mover timestamp no convierte una revision ya consumida en muestra nueva");

  Snapshot boundaries;
  recordDht(boundaries, 5.0f, 10.0f, 0);
  expect(boundaries.temperature.state == SampleState::Valid &&
             boundaries.humidity.state == SampleState::Valid, "limites inferiores DHT inclusivos");
  recordDht(boundaries, 35.0f, 90.0f, 2000);
  expect(boundaries.temperature.state == SampleState::Valid &&
             boundaries.humidity.state == SampleState::Valid, "limites superiores DHT inclusivos");
}

void reconnectAndTransportFailures() {
  Snapshot snapshot;
  Scheduler scheduler;
  expect(!scheduler.poll(0, false, snapshot).due, "desconectado no publica");
  freshSnapshot(snapshot, 60000);
  expect(!scheduler.poll(60000, false, snapshot).due &&
             !scheduler.poll(60000, true, snapshot).due,
         "volver de caida prolongada no genera mensajes atrasados");
  freshSnapshot(snapshot, 74000);
  expect(!scheduler.poll(74999, true, snapshot).due,
         "reconexion espera el intervalo completo");
  auto cycle = scheduler.poll(75000, true, snapshot);
  unsigned calls = 0;
  const auto failedFirst = transmit(cycle, [&calls](const char* topic, const char*) {
    ++calls;
    return std::strcmp(topic, "ambiente/temperatura") != 0;
  });
  expect(calls == 3 && failedFirst.attempted == 3 && failedFirst.sent == 2,
         "fallo del primer publish no cancela humedad ni CO2");
  expect(!scheduler.poll(75001, true, snapshot).due,
         "fallo de envio no provoca reintento inmediato del mismo ciclo");

  scheduler.poll(76000, false, snapshot);  // Pérdida del broker o WiFi.
  freshSnapshot(snapshot, 110000);
  scheduler.poll(110000, false, snapshot);
  expect(!scheduler.poll(111000, true, snapshot).due &&
             !scheduler.poll(125999, true, snapshot).due,
         "segunda recuperacion reinicia solo el plazo de publicacion");
  // La muestra adquirida durante la caída ya caducó: no enviarla.
  cycle = scheduler.poll(126000, true, snapshot);
  expect(cycle.due && selected(cycle) == 0,
         "primera ventana tras reconexion omite datos viejos si no hay adquisicion");
  freshSnapshot(snapshot, 140000);
  cycle = scheduler.poll(141000, true, snapshot);
  expect(selected(cycle) == 3 && std::strcmp(cycle.messages[2].payload, "415.7") == 0,
         "muestras nuevas restablecen publicacion normal sin backlog");
  const auto noCycle = scheduler.poll(141001, true, snapshot);
  calls = 0;
  transmit(noCycle, [&calls](const char*, const char*) { ++calls; return true; });
  expect(calls == 0, "transporte no llamado fuera de un ciclo vencido");
  scheduler.disconnected();  // Caída y reconexión aceptada en un mismo turno de red.
  expect(!scheduler.poll(141002, true, snapshot).due &&
             !scheduler.poll(156001, true, snapshot).due,
         "reconexion exitosa en el mismo turno tambien reinicia el plazo");
  freshSnapshot(snapshot, 156001);
  expect(selected(scheduler.poll(156002, true, snapshot)) == 3,
         "publicacion normal tras intervalo de reconexion inmediata");
}

mq135_acquisition::Observation acquire(mq135_acquisition::Controller<3>& controller,
                                     std::uint32_t now, int raw = 511,
                                     double milliVolts = 191.0) {
  controller.poll(now);
  const mq135_experimental::EnvironmentalConditions environment{
      true, 20.0, 50.0, controller.uptimeMs()};
  return controller.observe(raw, true, milliVolts, environment);
}

void physicalMq135Gates() {
  using mq135_experimental::Status;
  mq135_acquisition::Controller<3> controller;
  controller.begin(0);
  controller.configure(mq135_site_config::electricalConfiguration());
  Snapshot snapshot;
  auto observation = acquire(controller, 2000);
  recordMq135(snapshot, observation, controller.record() != nullptr, 2000);
  expect(snapshot.co2.state == SampleState::Unavailable,
         "CO2 no disponible durante calentamiento fisico");
  observation = acquire(controller, 30000);
  expect(mq135State(observation, false) == SampleState::Unavailable &&
             controller.startReference() == Status::Ok,
         "30 s no basta: se exige referencia fisica posterior");
  for (const auto now : {32000U, 34000U, 36000U}) {
    observation = acquire(controller, now);
    expect(mq135State(observation, controller.record() != nullptr) == SampleState::Unavailable,
           "captura de tres muestras no publica hasta habilitar emision");
  }
  expect(controller.record() != nullptr && controller.record()->sampleCount == 3 &&
             controller.enableEmission() == Status::Ok,
         "referencia de tres muestras conserva los bloqueos del controlador");
  observation = acquire(controller, 38000);
  recordMq135(snapshot, observation, controller.record() != nullptr, 38000);
  expect(snapshot.co2.state == SampleState::Valid && snapshot.co2.value > 0.0,
         "solo la estimacion fisica provisional valida habilita CO2");
  auto altered = observation;
  altered.source = mq135_acquisition::Source::Simulated;
  expect(mq135State(altered, true) == SampleState::Unavailable,
         "demo simulada jamas publicable como CO2 fisico");
  altered = observation;
  altered.warmupElapsedMs = 29999;
  expect(mq135State(altered, true) == SampleState::Unavailable &&
             mq135State(observation, false) == SampleState::Unavailable,
         "rechazar ppm incluso con estado valido si falta calentamiento o referencia");
  altered = observation;
  altered.ppm.value = std::numeric_limits<double>::infinity();
  expect(mq135State(altered, true) == SampleState::Invalid, "ppm infinito no publicable");

  observation = acquire(controller, 40000, 511, 99.0);
  expect(observation.measurementStatus == Status::VoltageOutsideAdcRange &&
             mq135State(observation, true) == SampleState::Discarded,
         "99 mV conserva rechazo del rango operativo fisico");
  observation = acquire(controller, 42000, 511, 951.0);
  expect(observation.measurementStatus == Status::VoltageOutsideAdcRange &&
             mq135State(observation, true) == SampleState::Discarded,
         "951 mV no produce publicacion CO2");
  observation = acquire(controller, 44000, 511, 100.0);
  expect(mq135State(observation, true) == SampleState::Valid, "100 mV inclusivo");
  observation = acquire(controller, 46000, 511, 950.0);
  expect(mq135State(observation, true) == SampleState::Valid, "950 mV inclusivo");
  observation = acquire(controller, 48000, 4095);
  expect(mq135State(observation, true) == SampleState::Discarded,
         "ADC saturado conserva rechazo aunque exista referencia");
  controller.disableEmission();
  observation = acquire(controller, 50000);
  expect(mq135State(observation, true) == SampleState::Unavailable,
         "emision manual deshabilitada omite CO2");
  controller.invalidateReference();
  observation = acquire(controller, 52000);
  expect(mq135State(observation, controller.record() != nullptr) == SampleState::Unavailable,
         "referencia invalidada no reutiliza ppm anterior");
}
}  // namespace

int main() {
  std::puts("T-1.7: PRUEBAS SINTETICAS, NO VALIDACION FISICA");
  intervalAndClock();
  numericFormatAndTopics();
  independentVariablesAndFreshness();
  reconnectAndTransportFailures();
  physicalMq135Gates();
  std::printf("SINTETICO: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
