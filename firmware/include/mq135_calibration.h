#ifndef MONITOREO_IOT_MQ135_CALIBRATION_H
#define MONITOREO_IOT_MQ135_CALIBRATION_H

#include "mq135_experimental.h"
#include <cstring>

namespace mq135_calibration {
using mq135_experimental::Result;
using mq135_experimental::Status;

// La carga EFECTIVA vista desde AO, no R1/R2 ni un valor de otra placa.
// En modo estricto su evidencia debe comprobar RL interna y topologia.
// La estimacion experimental es una autorizacion aparte, nunca verificacion.
struct LoadConfiguration {
  double effectiveLoadOhms;
  bool topologyVerified;
  std::uint32_t circuitRevision;
  char evidenceId[96];
  bool experimentalEstimate;
};

inline Status validateLoad(const LoadConfiguration& load, std::uint32_t revision,
                           bool allowExperimental = false) {
  const bool permitted = load.experimentalEstimate
      ? allowExperimental && !load.topologyVerified : load.topologyVerified;
  if (!permitted || load.evidenceId[0] == '\0' ||
      std::memchr(load.evidenceId, '\0', sizeof(load.evidenceId)) == nullptr)
    return Status::LoadNotVerified;
  if (revision == 0 || load.circuitRevision != revision)
    return Status::ConfigurationMismatch;
  if (!std::isfinite(load.effectiveLoadOhms) || load.effectiveLoadOhms <= 0.0)
    return Status::InvalidInput;
  return Status::Ok;
}

inline Result resistanceOhms(const LoadConfiguration& load, std::uint32_t revision,
                             double vcV, double vaoV, bool allowExperimental = false) {
  const Status gate = validateLoad(load, revision, allowExperimental);
  if (gate != Status::Ok) return mq135_experimental::failure(gate);
  const auto q = mq135_experimental::normalizedResistance(vcV, vaoV);
  if (!q.isValid()) return q;
  const double rs = load.effectiveLoadOhms * q.value;
  if (!std::isfinite(rs) || rs <= 0.0)
    return mq135_experimental::failure(Status::NumericalError);
  return {Status::Ok, rs};
}

// Memoria constante: no retiene muestras, no calcula Ro ni convierte a ppm.
class Capture {
 public:
  Capture() { reset(); }
  void reset() {
    active_ = false;
    valid_ = rejected_ = 0;
    meanOhms_ = std::numeric_limits<double>::quiet_NaN();
    q_ = rs_ = mq135_experimental::failure(Status::CaptureNotActive);
  }
  Status start(bool physical) {
    if (!physical) return Status::InvalidProvenance;
    if (!active_) { reset(); active_ = true; }
    return Status::Ok;  // Repetir iniciar no borra una captura en curso.
  }
  bool stop() {
    const bool wasActive = active_;
    active_ = false;
    return wasActive;  // Conserva el resumen hasta la siguiente captura.
  }
  void observe(Status measurement, const LoadConfiguration& load,
               std::uint32_t revision, double vcV, double vaoV) {
    if (!active_) return;
    q_ = rs_ = mq135_experimental::failure(measurement);
    if (measurement == Status::Ok) {
      q_ = mq135_experimental::normalizedResistance(vcV, vaoV);
      rs_ = resistanceOhms(load, revision, vcV, vaoV, true);
    }
    if (rs_.isValid()) {
      // double representa exactamente estos contadores. Sin wrap ni perdida
      // silenciosa al convertir n para la actualizacion incremental.
      if (valid_ >= 9007199254740991ULL) {
        rs_ = mq135_experimental::failure(Status::RecordCapacityExceeded);
      } else {
        const double next = valid_ == 0 ? rs_.value
            : meanOhms_ + (rs_.value - meanOhms_) / static_cast<double>(valid_ + 1);
        if (std::isfinite(next) && next > 0.0) {
          meanOhms_ = next;
          ++valid_;
          return;
        }
        rs_ = mq135_experimental::failure(Status::NumericalError);
      }
    }
    if (rejected_ != std::numeric_limits<std::uint64_t>::max()) ++rejected_;
  }
  bool active() const { return active_; }
  std::uint64_t validSamples() const { return valid_; }
  std::uint64_t rejectedSamples() const { return rejected_; }
  double meanOhms() const { return meanOhms_; }
  Result lastQ() const { return q_; }
  Result lastResistance() const { return rs_; }

 private:
  bool active_;
  std::uint64_t valid_;
  std::uint64_t rejected_;
  double meanOhms_;
  Result q_;
  Result rs_;
};
}  // namespace mq135_calibration

#endif
