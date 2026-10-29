#include "../include/mq135_acquisition.h"
#include "../include/mqtt_publication.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  using namespace mq135_acquisition;
  using mq135_experimental::Status;
  auto cfg = pendingConfiguration();
  cfg.electricalVerified = cfg.adcConversionVerified = true;
  cfg.directAoConnection = false;
  cfg.adcToAoVoltageRatio = 0.5;
  cfg.measuredVcV = 4.54;
  cfg.minimumAdcMilliVolts = 100; cfg.maximumAdcMilliVolts = 950;
  cfg.electrical.minimumUsableAdcCode = 1;
  cfg.electrical.minimumUsefulVaoV = 0.2;
  cfg.electrical.circuitRevision = 3;
  std::strcpy(cfg.verificationEvidenceId, "SINTETICO");
  std::strcpy(cfg.electrical.adcConversionId, "SINTETICO");
  std::strcpy(cfg.electrical.circuitDescription, "SINTETICO");
  cfg.referenceSampleCount = 3; cfg.maxRelativeSpread = -1.0;
  cfg.calibrationLoad.effectiveLoadOhms = 6666.67;
  cfg.calibrationLoad.experimentalEstimate = true;
  cfg.calibrationLoad.circuitRevision = 3;
  std::strcpy(cfg.calibrationLoad.evidenceId, "SINTETICO");
  const double rs = 6666.67 * (4.54 / 1.0 - 1.0);
  cfg.calibratedRoKohm = rs * std::pow(420.0 / 116.6020682, 1.0 / 2.769034857) / 1000.0;
  cfg.calibrationAccepted = true;
  const auto estimate = mq135_calibrated::estimate(4.54, 1.0, cfg.calibrationLoad, 3,
                                                 cfg.calibratedRoKohm, true);
  assert(estimate.isValid() && std::abs(estimate.value - 420.0) < 0.000001);
  assert(!mq135_calibrated::estimate(4.54, 1, cfg.calibrationLoad, 3, 0, true).isValid());
  assert(!mq135_calibrated::estimate(4.54, 1, cfg.calibrationLoad, 3, -1, true).isValid());
  assert(!mq135_calibrated::estimate(4.54, 1, cfg.calibrationLoad, 3,
    std::numeric_limits<double>::quiet_NaN(), true).isValid());
  assert(!mq135_calibrated::estimate(4.54, 1, cfg.calibrationLoad, 3, 20, false).isValid());
  assert(!mq135_calibrated::estimate(4.54, 1, cfg.calibrationLoad, 4, 20, true).isValid());

  Controller<3> controller;
  controller.begin(0); assert(controller.configure(cfg) == Status::Ok);
  assert(controller.enableEmission() == Status::WarmupIncomplete);
  controller.poll(30000);
  assert(controller.enableEmission() == Status::Ok);
  assert(controller.record() == nullptr && controller.referenceAvailable());
  const mq135_experimental::EnvironmentalConditions env{};
  auto good = controller.observe(1000, true, 500.0, env);
  assert(good.ppm.isValid() && std::abs(good.ppm.value - 420) < 0.000001);
  assert(mqtt_publication::mq135State(good, controller.referenceAvailable()) ==
         mqtt_publication::SampleState::Valid);
  char text[384]; formatObservation(good, text, sizeof(text), true);
  assert(std::strstr(text, "Ro configurado") && !std::strstr(text, "provisional"));
  controller.poll(32000);
  assert(!controller.observe(4095, true, 500, env).ppm.isValid());
  controller.poll(34000);
  assert(!controller.observe(1000, true, 951, env).ppm.isValid());
  assert(controller.startCalibration() == Status::Ok);
  controller.poll(36000);
  assert(controller.observe(1000, true, 500, env).ppm.status == Status::CalibrationActive);
  assert(controller.stopCalibration());
  assert(controller.enableEmission() == Status::Ok);
  cfg.calibrationAccepted = false;
  controller.configure(cfg);
  assert(controller.enableEmission() == Status::ReferenceMissing);
  assert(!controller.referenceAvailable());
  cfg.calibrationAccepted = true; cfg.calibratedRoKohm = 0;
  assert(controller.configure(cfg) == Status::InvalidMetadata);
  controller.configureSimulatedDemo(); controller.poll(38000);
  assert(controller.observeSimulated().source == Source::Simulated);
  std::puts("SINTETICO: conversion, unidades, protecciones, captura y procedencia: PASS");
}
