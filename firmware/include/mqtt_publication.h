#ifndef MONITOREO_IOT_MQTT_PUBLICATION_H
#define MONITOREO_IOT_MQTT_PUBLICATION_H

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include "dht_validation.h"
#include "mq135_acquisition.h"

namespace mqtt_publication {
constexpr std::uint32_t DEFAULT_INTERVAL_MS = 15000;
// Dos turnos de adquisición de 2000 ms más margen. No depende del periodo MQTT.
constexpr std::uint32_t MAX_SAMPLE_AGE_MS = 5000;
constexpr std::size_t VARIABLE_COUNT = 3;
constexpr std::size_t PAYLOAD_CAPACITY = 48;
constexpr const char* TOPICS[VARIABLE_COUNT] = {
    "ambiente/temperatura", "ambiente/humedad", "ambiente/co2"};

enum class SampleState { Unavailable, Valid, Invalid, Discarded };
enum class Omission { None, Unavailable, Invalid, Discarded, Stale, AlreadyUsed, Format };

struct Reading {
  SampleState state;
  double value;
  std::uint32_t timestampMs;
  std::uint32_t revision;
  Reading() : state(SampleState::Unavailable),
              value(std::numeric_limits<double>::quiet_NaN()),
              timestampMs(0), revision(0) {}
};

struct Snapshot {
  Reading temperature;
  Reading humidity;
  Reading co2;
  bool co2Suppressed = false;
  bool co2SuppressionAcknowledged = false;
  std::uint32_t co2Epoch = 0;
};

inline void record(Reading& reading, SampleState state, double value,
                   std::uint32_t now) {
  if (state == SampleState::Valid && !std::isfinite(value)) state = SampleState::Invalid;
  reading.state = state;
  // Una lectura fallida reemplaza la anterior; nunca conserva un valor válido viejo.
  reading.value = state == SampleState::Valid
      ? value : std::numeric_limits<double>::quiet_NaN();
  reading.timestampMs = now;
  ++reading.revision;
}

// Llamar bajo el mismo mux que protege las lecturas. Descarta CO2 al entrar
// Y al salir; el cambio de epoca invalida ciclos copiados antes de la orden.
inline void setCalibrationCapture(Snapshot& snapshot, bool active, std::uint32_t now) {
  if (snapshot.co2Suppressed == active) return;
  snapshot.co2Suppressed = active;
  snapshot.co2SuppressionAcknowledged = false;
  ++snapshot.co2Epoch;
  record(snapshot.co2, SampleState::Unavailable, 0.0, now);
}

// Solo la tarea de red: llamar entre ciclos, cuando no hay publish en curso.
inline void acknowledgeCalibrationCapture(Snapshot& snapshot) {
  snapshot.co2SuppressionAcknowledged = snapshot.co2Suppressed;
}

inline SampleState dhtState(double value, double minimum, double maximum) {
  if (!std::isfinite(value)) return SampleState::Invalid;
  return value < minimum || value > maximum ? SampleState::Discarded : SampleState::Valid;
}

inline void recordDht(Snapshot& snapshot, float temperature, float humidity,
                      std::uint32_t now) {
  // CA-04: límites existentes por variable. La validación de la pareja usada
  // por el monitor y la referencia MQ-135 sigue intacta en main.cpp.
  record(snapshot.temperature,
         dhtState(temperature, dht_validation::TEMPERATURE_MIN_C,
                  dht_validation::TEMPERATURE_MAX_C), temperature, now);
  record(snapshot.humidity,
         dhtState(humidity, dht_validation::HUMIDITY_MIN_PERCENT,
                  dht_validation::HUMIDITY_MAX_PERCENT), humidity, now);
}

inline SampleState mq135State(const mq135_acquisition::Observation& observation,
                              bool referenceAvailable) {
  using mq135_experimental::Status;
  using mq135_acquisition::State;
  if (observation.source != mq135_acquisition::Source::Physical ||
      observation.warmupElapsedMs < mq135_acquisition::WARMUP_MS ||
      observation.state == State::Calentando || !referenceAvailable) {
    return SampleState::Unavailable;
  }
  if (observation.adcStatus != Status::Ok ||
      observation.measurementStatus == Status::VoltageOutsideAdcRange ||
      observation.measurementStatus == Status::SignalTooSmall) {
    return SampleState::Discarded;
  }
  if (observation.measurementStatus == Status::ConversionUnavailable ||
      observation.measurementStatus == Status::CircuitNotVerified ||
      observation.ppm.status == Status::ReferenceMissing ||
      observation.ppm.status == Status::EmissionDisabled) {
    return SampleState::Unavailable;
  }
  if (observation.measurementStatus != Status::Ok || !observation.ppm.isValid() ||
      !std::isfinite(observation.ppm.value) || observation.ppm.value <= 0.0) {
    return SampleState::Invalid;
  }
  return observation.state == State::EstimacionDisponible
      ? SampleState::Valid : SampleState::Unavailable;
}

inline void recordMq135(Snapshot& snapshot,
                        const mq135_acquisition::Observation& observation,
                        bool referenceAvailable, std::uint32_t now) {
  record(snapshot.co2, mq135State(observation, referenceAvailable),
         observation.ppm.value, now);
}

inline bool validInterval(std::uint32_t intervalMs) {
  return intervalMs >= 10000 && intervalMs <= 30000;
}

inline bool formatPayload(double value, char* output, std::size_t capacity) {
  if (output == nullptr || capacity == 0) return false;
  output[0] = '\0';
  if (!std::isfinite(value)) return false;
  // Redondeo decimal fijo: sin JSON, unidades, exponentes ni salto de línea.
  const int written = std::snprintf(output, capacity, "%.1f", value);
  if (written < 0 || static_cast<std::size_t>(written) >= capacity) {
    output[0] = '\0';
    return false;
  }
  // Comprobar también el separador para no enviar comas si cambia la locale.
  std::size_t i = output[0] == '-' ? 1 : 0;
  const std::size_t firstDigit = i;
  while (output[i] >= '0' && output[i] <= '9') ++i;
  const bool valid = i > firstDigit && output[i] == '.' &&
      output[i + 1] >= '0' && output[i + 1] <= '9' && output[i + 2] == '\0';
  if (!valid) output[0] = '\0';
  return valid;
}

inline const char* omissionName(Omission reason) {
  switch (reason) {
    case Omission::None: return "seleccionada";
    case Omission::Unavailable: return "no disponible";
    case Omission::Invalid: return "lectura invalida";
    case Omission::Discarded: return "lectura descartada";
    case Omission::Stale: return "lectura antigua";
    case Omission::AlreadyUsed: return "sin muestra nueva";
    case Omission::Format: return "formato invalido";
  }
  return "no disponible";
}

struct Message {
  const char* topic;
  Omission omission;
  char payload[PAYLOAD_CAPACITY];
};
struct Cycle {
  bool due;
  Message messages[VARIABLE_COUNT];
  std::uint32_t co2Epoch;
};

// Revalidar justo antes de publicar, usando un snapshot ACTUAL, no la copia del
// inicio del ciclo. Un paquete ya entregado al transporte no puede retirarse.
inline bool mayTransmit(const Cycle& cycle, const Snapshot& current, const char* topic) {
  return std::strcmp(topic, TOPICS[2]) != 0 ||
      (!current.co2Suppressed && cycle.co2Epoch == current.co2Epoch);
}

// Propiedad exclusiva de la tarea de red. Sin Arduino, sensores ni transporte.
class Scheduler {
 public:
  explicit Scheduler(std::uint32_t intervalMs = DEFAULT_INTERVAL_MS)
      : intervalMs_(intervalMs), connected_(false), lastCycleMs_(0), usedRevisions_{} {}

  // También se llama si el intento de reconexión tiene éxito en el mismo turno
  // que detectó la caída; poll() no debe perder esa transición intermedia.
  void disconnected() { connected_ = false; }

  Cycle poll(std::uint32_t now, bool mqttReady, const Snapshot& snapshot) {
    Cycle cycle{};
    if (!validInterval(intervalMs_)) return cycle;
    if (!mqttReady) {
      disconnected();
      return cycle;  // Sin cola de ciclos pendientes ni consumo de muestras.
    }
    if (!connected_) {
      connected_ = true;
      lastCycleMs_ = now;
      return cycle;  // Un intervalo completo después de conectar/reconectar.
    }
    if (static_cast<std::uint32_t>(now - lastCycleMs_) < intervalMs_) return cycle;
    lastCycleMs_ = now;  // Saltar ciclos perdidos; jamás sumar intervalos atrasados.
    cycle.due = true;
    cycle.co2Epoch = snapshot.co2Epoch;
    const Reading readings[VARIABLE_COUNT] = {
        snapshot.temperature, snapshot.humidity, snapshot.co2};
    for (std::size_t i = 0; i < VARIABLE_COUNT; ++i) {
      const auto& reading = readings[i];
      auto& message = cycle.messages[i];
      message.topic = TOPICS[i];
      if ((i == 2 && snapshot.co2Suppressed) || reading.state == SampleState::Unavailable)
        message.omission = Omission::Unavailable;
      else if (reading.state == SampleState::Invalid) message.omission = Omission::Invalid;
      else if (reading.state == SampleState::Discarded) message.omission = Omission::Discarded;
      else if (static_cast<std::uint32_t>(now - reading.timestampMs) > MAX_SAMPLE_AGE_MS)
        message.omission = Omission::Stale;
      else if (reading.revision == usedRevisions_[i]) message.omission = Omission::AlreadyUsed;
      else if (!formatPayload(reading.value, message.payload, sizeof(message.payload)))
        message.omission = Omission::Format;
      else message.omission = Omission::None;
      // Un ciclo intenta cada muestra como máximo una vez, incluso si falla TCP.
      usedRevisions_[i] = reading.revision;
    }
    return cycle;
  }

 private:
  std::uint32_t intervalMs_;
  bool connected_;
  std::uint32_t lastCycleMs_;
  std::uint32_t usedRevisions_[VARIABLE_COUNT];
};

struct SendResult {
  unsigned attempted;
  unsigned sent;
};

template <typename Sender>
inline SendResult transmit(const Cycle& cycle, Sender send) {
  SendResult result{};
  if (!cycle.due) return result;
  for (const auto& message : cycle.messages) {
    if (message.omission != Omission::None) continue;
    ++result.attempted;
    if (send(message.topic, message.payload)) ++result.sent;
    // Un fallo individual no cancela los intentos de los demás topics.
  }
  return result;
}
}  // namespace mqtt_publication

#endif  // MONITOREO_IOT_MQTT_PUBLICATION_H
