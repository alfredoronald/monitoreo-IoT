// T-1.6: pruebas SINTÉTICAS del planificador usado por el firmware.
// C++11 de escritorio; sin Arduino, red, credenciales ni hardware.
// No acreditan el plazo de reconexión ni la concurrencia física del ESP32.
#include "../include/connection_retry.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
using connection_retry::Decision;
using connection_retry::Scheduler;
unsigned checks = 0;
unsigned failures = 0;

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTÉTICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

bool noAttempt(const Decision& decision) {
  return !decision.tryWifi && !decision.tryMqtt;
}

void startupAndIntervals() {
  Scheduler retries(5000, 2000);
  auto result = retries.poll(0, false, false);
  expect(result.tryWifi && !result.tryMqtt,
         "arranque intenta WiFi inmediatamente y no MQTT sin red");
  expect(!result.wifiConnected && !result.wifiDisconnected &&
             !result.mqttConnected && !result.mqttDisconnected,
         "arranque no inventa conexiones ni desconexiones");
  expect(noAttempt(retries.poll(0, false, false)),
         "no repetir intento en el mismo turno");
  expect(noAttempt(retries.poll(4999, false, false)),
         "WiFi espera su intervalo sin ordenar conexión antes de tiempo");
  result = retries.poll(5000, false, false);
  expect(result.tryWifi && !result.tryMqtt, "WiFi reintenta a los 5000 ms");
  result = retries.poll(6000, true, false);
  expect(result.wifiConnected && !result.wifiDisconnected &&
             result.tryMqtt && !result.tryWifi,
         "WiFi disponible notifica conexión e intenta MQTT inmediatamente");
  expect(!result.mqttConnected, "un intento MQTT no equivale a conexión aceptada");
  expect(noAttempt(retries.poll(6000, true, false)),
         "MQTT no repite intento en el mismo instante");
  expect(noAttempt(retries.poll(7999, true, false)),
         "MQTT no reintenta antes de 2000 ms");
  result = retries.poll(8000, true, false);
  expect(result.tryMqtt && !result.tryWifi, "MQTT reintenta a los 2000 ms");
  result = retries.poll(8100, true, true);
  expect(result.mqttConnected && !result.mqttDisconnected && noAttempt(result),
         "MQTT aceptado notifica conexión sin reconectar WiFi");
  result = retries.poll(9000, true, true);
  expect(!result.wifiConnected && !result.mqttConnected && noAttempt(result),
         "conexión estable no repite mensajes ni intentos");
  expect(noAttempt(retries.poll(60000, true, true)),
         "vencer temporizadores no reinicia conexiones sanas");
}

void lossesAndRecovery() {
  Scheduler retries(5000, 2000);
  auto result = retries.poll(0, true, true);
  expect(result.wifiConnected && result.mqttConnected && noAttempt(result),
         "estados conectados iniciales se notifican una vez");
  result = retries.poll(100, false, true);
  expect(result.wifiDisconnected && result.mqttDisconnected &&
             result.tryWifi && !result.tryMqtt && !result.mqttConnected,
         "pérdida WiFi invalida MQTT aunque el socket conserve su estado anterior");
  result = retries.poll(200, false, true);
  expect(!result.wifiDisconnected && !result.mqttDisconnected && noAttempt(result),
         "pérdida persistente no repite eventos ni elude intervalo WiFi");
  expect(noAttempt(retries.poll(5099, false, false)),
         "reintento WiFi respeta el intervalo desde la desconexión");
  expect(retries.poll(5100, false, false).tryWifi,
         "red ausente sigue reintentando sin un límite de intentos");
  result = retries.poll(5200, true, false);
  expect(result.wifiConnected && result.tryMqtt && !result.tryWifi,
         "recuperar WiFi permite MQTT inmediatamente con nueva conexión");
  result = retries.poll(5220, true, true);
  expect(result.mqttConnected && noAttempt(result),
         "MQTT recuperado genera una nueva notificación de conexión");
  result = retries.poll(5300, true, false);
  expect(result.mqttDisconnected && result.tryMqtt && !result.tryWifi &&
             !result.wifiDisconnected,
         "reinicio del broker reintenta MQTT sin tocar WiFi");
  expect(noAttempt(retries.poll(7299, true, false)),
         "broker ausente respeta intervalo MQTT tras primer intento fallido");
  expect(retries.poll(7300, true, false).tryMqtt,
         "broker ausente reintenta periódicamente");
  result = retries.poll(7400, true, true);
  expect(result.mqttConnected && !result.wifiConnected && noAttempt(result),
         "recuperación del broker no inventa una nueva conexión WiFi");
}

void unfinishedWifiAttempt() {
  Scheduler retries(5000, 2000);
  expect(retries.poll(0, false, false).tryWifi, "iniciar asociación una sola vez");
  expect(noAttempt(retries.poll(5000, false, false, true)),
         "no cancelar autenticación en curso al vencer los 5000 ms");
  expect(noAttempt(retries.poll(15000, false, false, true)),
         "no repetir conexión ni intentar MQTT mientras se espera asociación o IP");
  expect(retries.poll(15020, false, false, false).tryWifi,
         "terminar intento pendiente permite reintentar sin reiniciar el intervalo");
  const auto connected = retries.poll(16000, true, false, true);
  expect(connected.wifiConnected && connected.tryMqtt && !connected.tryWifi,
         "IP disponible permite MQTT aunque la marca pendiente aún no se haya limpiado");

  const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - 1000;
  Scheduler rollover(5000, 2000);
  rollover.poll(start, false, false);
  expect(noAttempt(rollover.poll(static_cast<std::uint32_t>(start + 5000U), false, false, true)),
         "rollover tampoco cancela una asociación en curso");
  expect(rollover.poll(static_cast<std::uint32_t>(start + 5020U), false, false).tryWifi,
         "rollover conserva el reintento tras finalizar la asociación fallida");
}

void clockRolloverAndBootTime() {
  const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - 1000;
  Scheduler wifiRetries(5000, 2000);
  expect(wifiRetries.poll(start, false, false).tryWifi,
         "arranque con millis distinto de cero intenta WiFi inmediatamente");
  expect(noAttempt(wifiRetries.poll(static_cast<std::uint32_t>(start + 4999U),
                                   false, false)),
         "rollover no adelanta reintento WiFi");
  expect(wifiRetries.poll(static_cast<std::uint32_t>(start + 5000U),
                         false, false).tryWifi,
         "rollover conserva intervalo WiFi");

  Scheduler mqttRetries(5000, 2000);
  expect(mqttRetries.poll(start, true, false).tryMqtt,
         "WiFi disponible al arranque intenta MQTT inmediatamente");
  expect(noAttempt(mqttRetries.poll(static_cast<std::uint32_t>(start + 1999U),
                                   true, false)),
         "rollover no adelanta reintento MQTT");
  expect(mqttRetries.poll(static_cast<std::uint32_t>(start + 2000U),
                         true, false).tryMqtt,
         "rollover conserva intervalo MQTT");
}

void longOutagesAndIndependentIntervals() {
  Scheduler retries(5000, 2000);
  unsigned wifiAttempts = 0;
  unsigned mqttAttempts = 0;
  // Estados falsos simulan servicios caídos, no operaciones de red reales.
  for (std::uint32_t now = 0; now <= 60000; now += 100) {
    const auto result = retries.poll(now, false, false);
    if (result.tryWifi) ++wifiAttempts;
    if (result.tryMqtt) ++mqttAttempts;
  }
  expect(wifiAttempts == 13 && mqttAttempts == 0,
         "caída prolongada mantiene reintentos WiFi y nunca intenta MQTT sin red");
  auto result = retries.poll(60001, true, false);
  expect(result.wifiConnected && result.tryMqtt,
         "restaurar WiFi tras caída prolongada permite recuperar MQTT");
  for (std::uint32_t now = 60101; now <= 80001; now += 100) {
    result = retries.poll(now, true, false);
    if (result.tryWifi) ++wifiAttempts;
    if (result.tryMqtt) ++mqttAttempts;
  }
  expect(wifiAttempts == 13 && mqttAttempts == 10,
         "fallos MQTT persistentes no reinician WiFi y respetan 2000 ms");

  // La misma política usa los intervalos entregados por config.h, no constantes
  // escondidas: este fixture distinto solo ejercita parametrización sintética.
  Scheduler custom(7000, 3000);
  custom.poll(123, false, false);
  expect(noAttempt(custom.poll(7122, false, false)) &&
             custom.poll(7123, false, false).tryWifi,
         "intervalo WiFi configurable se aplica sin adelantos");
  expect(custom.poll(7200, true, false).tryMqtt &&
             noAttempt(custom.poll(10199, true, false)) &&
             custom.poll(10200, true, false).tryMqtt,
         "intervalo MQTT configurable se aplica independientemente de WiFi");
}
}  // namespace

int main() {
  std::puts("T-1.6: PLANIFICACIÓN SINTÉTICA, NO VALIDACIÓN FÍSICA");
  startupAndIntervals();
  lossesAndRecovery();
  unfinishedWifiAttempt();
  clockRolloverAndBootTime();
  longOutagesAndIndependentIntervals();
  std::printf("SINTÉTICO: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
