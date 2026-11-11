#ifndef MONITOREO_IOT_MQ135_SIMULATED_DEMO_H
#define MONITOREO_IOT_MQ135_SIMULATED_DEMO_H

#include "mq135_experimental.h"

namespace mq135_simulated_demo {
// Fixtures inventados para demostrar software. No describen el circuito real,
// no convierten ADC a voltaje y no usan las mediciones de 0.34 V / 0.42 V.
constexpr double FIXED_Q_REFERENCE = 4.0;
constexpr int MINIMUM_ADC_CODE = 10;
constexpr double MINIMUM_VAO_V = 0.15;
constexpr std::size_t SAMPLE_COUNT = 4;

struct Sample {
  int adcRaw;
  double vcV;
  double vaoV;
};

inline Sample sampleAt(std::size_t index) {
  static const Sample samples[SAMPLE_COUNT] = {
      {1200, 5.0, 1.0}, {1400, 5.0, 1.2},
      {1600, 5.0, 1.4}, {1400, 5.0, 1.2}};
  return samples[index % SAMPLE_COUNT];
}

inline mq135_experimental::Result estimate(const Sample& sample,
                                           double qReference) {
  const auto valid = mq135_experimental::validateAcquiredSample(
      sample.adcRaw, sample.vcV, sample.vaoV, MINIMUM_ADC_CODE, MINIMUM_VAO_V);
  if (!valid.isValid()) return valid;
  return mq135_experimental::estimateExperimentalPpm(
      sample.vcV, sample.vaoV, qReference);
}
}  // namespace mq135_simulated_demo

#endif  // MONITOREO_IOT_MQ135_SIMULATED_DEMO_H
