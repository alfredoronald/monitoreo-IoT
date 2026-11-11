#ifndef MONITOREO_IOT_MQ135_CALIBRATED_H
#define MONITOREO_IOT_MQ135_CALIBRATED_H
#include "mq135_calibration.h"

namespace mq135_calibrated {
// Ro y Rs en ohm; el límite de calentamiento/ADC se aplica en Controller.
inline mq135_experimental::Result estimate(double vcV, double vaoV,
    const mq135_calibration::LoadConfiguration& load, std::uint32_t revision,
    double roKohm, bool accepted) {
  using namespace mq135_experimental;
  if (!accepted) return failure(Status::ReferenceMissing);
  if (!std::isfinite(roKohm) || roKohm <= 0.0 ||
      !std::isfinite(roKohm * 1000.0)) return failure(Status::InvalidInput);
  const auto rs = mq135_calibration::resistanceOhms(load, revision, vcV, vaoV, true);
  if (!rs.isValid()) return rs;
  const double ratio = rs.value / (roKohm * 1000.0);
  const double ppm = MODEL_A * std::pow(ratio, -MODEL_B);
  if (!std::isfinite(ratio) || ratio <= 0.0 || !std::isfinite(ppm) || ppm <= 0.0)
    return failure(Status::NumericalError);
  return {Status::Ok, ppm};
}
}
#endif
