#ifndef MONITOREO_IOT_CONNECTION_RETRY_H
#define MONITOREO_IOT_CONNECTION_RETRY_H

#include <cstdint>

namespace connection_retry {
struct Decision {
  bool wifiConnected;
  bool wifiDisconnected;
  bool mqttConnected;
  bool mqttDisconnected;
  bool tryWifi;
  bool tryMqtt;
};

// Planificador puro: no espera, no conecta y no accede al hardware.
// Los intervalos deben ser positivos y menores que la mitad del ciclo uint32_t.
class Scheduler {
 public:
  Scheduler(std::uint32_t wifiIntervalMs, std::uint32_t mqttIntervalMs)
      : wifiIntervalMs_(wifiIntervalMs), mqttIntervalMs_(mqttIntervalMs) {}

  Decision poll(std::uint32_t now, bool wifiReady, bool mqttReady,
                bool wifiAttemptPending = false) {
    mqttReady = wifiReady && mqttReady;
    Decision decision{wifiReady && !wifiReady_, !wifiReady && wifiReady_,
                      mqttReady && !mqttReady_, !mqttReady && mqttReady_,
                      false, false};
    if (decision.wifiDisconnected) wifiAttempted_ = false;
    if (decision.wifiConnected || !wifiReady || decision.mqttDisconnected) {
      mqttAttempted_ = false;
    }
    wifiReady_ = wifiReady;
    mqttReady_ = mqttReady;

    // Un intento por turno. La resta unsigned soporta el desbordamiento de millis().
    // El intervalo no autoriza cancelar una asociación/autenticación o DHCP.
    if (!wifiReady && !wifiAttemptPending &&
        (!wifiAttempted_ || now - lastWifiAttemptMs_ >= wifiIntervalMs_)) {
      wifiAttempted_ = true;
      lastWifiAttemptMs_ = now;
      decision.tryWifi = true;
    } else if (wifiReady && !mqttReady &&
               (!mqttAttempted_ || now - lastMqttAttemptMs_ >= mqttIntervalMs_)) {
      mqttAttempted_ = true;
      lastMqttAttemptMs_ = now;
      decision.tryMqtt = true;
    }
    return decision;
  }

 private:
  const std::uint32_t wifiIntervalMs_;
  const std::uint32_t mqttIntervalMs_;
  std::uint32_t lastWifiAttemptMs_ = 0;
  std::uint32_t lastMqttAttemptMs_ = 0;
  bool wifiAttempted_ = false;
  bool mqttAttempted_ = false;
  bool wifiReady_ = false;
  bool mqttReady_ = false;
};
}  // namespace connection_retry

#endif  // MONITOREO_IOT_CONNECTION_RETRY_H
