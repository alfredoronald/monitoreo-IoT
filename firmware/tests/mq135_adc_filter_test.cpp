// DATOS SINTÉTICOS: no lee GPIO34, eFuse ni sensores y no verifica hardware.
// Suite C++11 de escritorio preparada sin ejecutarla. Conserva las anteriores.
#include "../include/mq135_adc_filter.h"
#include "../include/mq135_acquisition.h"
#include "../include/mq135_site_config.h"

#include <cstdlib>
#include <cstring>

namespace {
using mq135_experimental::Status;
using mq135_adc_filter::Samples;
unsigned checks = 0;
unsigned failures = 0;

void expect(bool condition, const char* name) {
  ++checks;
  if (!condition) ++failures;
  std::printf("[SINTÉTICO] %s: %s\n", condition ? "PASS" : "FAIL", name);
}

const mq135_experimental::EnvironmentalConditions noEnvironment{
    false, std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN(), 0};

mq135_acquisition::Configuration syntheticConfiguration() {
  // Exclusivo del TEST: estos datos nunca se aplican a mq135_site_config.h.
  auto config = mq135_acquisition::pendingConfiguration();
  config.electricalVerified = true;
  config.adcConversionVerified = true;
  config.measuredVcV = 5.0;
  config.minimumAdcMilliVolts = 150;
  config.maximumAdcMilliVolts = 2450;
  config.electrical.circuitRevision = 7;
  config.electrical.minimumUsableAdcCode = 10;
  config.electrical.minimumUsefulVaoV = 0.15;
  std::strcpy(config.verificationEvidenceId, "EVIDENCIA SINTÉTICA");
  std::strcpy(config.electrical.adcConversionId, "CONVERSIÓN SINTÉTICA");
  std::strcpy(config.electrical.circuitDescription, "AO directo SINTÉTICO");
  config.referenceSampleCount = mq135_site_config::REFERENCE_SAMPLE_COUNT;
  config.maxRelativeSpread = mq135_experimental::NO_SPREAD_LIMIT;
  return config;
}

void medianCases() {
  expect(mq135_adc_filter::INTERNAL_SAMPLE_COUNT == 11,
         "ráfaga física configurada con exactamente 11 códigos");
  const Samples unordered{{330, 384, 324, 305, 389, 320, 318, 331, 329, 332, 328}};
  const auto saved = unordered;
  const auto result = mq135_adc_filter::median(unordered);
  expect(result.isValid() && result.raw == 329 && unordered == saved,
         "mediana ordena una copia y devuelve el sexto código, sin promediar");
  const Samples ascending{{100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110}};
  const Samples descending{{110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100}};
  expect(mq135_adc_filter::median(ascending).raw == 105 &&
             mq135_adc_filter::median(descending).raw == 105,
         "orden inicial ascendente o descendente no cambia la mediana");
  const Samples extremes{{1, 2, 3, 4, 5, 330, 4090, 4091, 4092, 4093, 4094}};
  expect(mq135_adc_filter::median(extremes).raw == 330,
         "códigos extremos válidos no desplazan el elemento central");
  Samples repeated{};
  repeated.fill(330);
  expect(mq135_adc_filter::median(repeated).raw == 330,
         "once códigos repetidos conservan su valor");

  // Un único error NO se oculta aunque los otros diez códigos sean válidos.
  const int invalidCodes[] = {0, 4095, -1, 4096, 9};
  const Status expected[] = {Status::AdcZero, Status::AdcSaturated,
      Status::AdcOutOfRange, Status::AdcOutOfRange, Status::SignalTooSmall};
  for (std::size_t i = 0; i < 5; ++i) {
    for (std::size_t position = 0; position < repeated.size(); ++position) {
      Samples invalid = repeated;
      invalid[position] = invalidCodes[i];
      const auto rejected = mq135_adc_filter::median(invalid, 10);
      expect(!rejected.isValid() && rejected.status == expected[i] &&
                 rejected.raw == invalidCodes[i],
             "rechazar un código inválido en cualquier posición interna");
    }
  }
  expect(mq135_adc_filter::median(repeated, 0).status == Status::InvalidInput &&
             mq135_adc_filter::median(repeated, 4095).status == Status::InvalidInput,
         "reutilizar validación de umbral ADC inválido");
  std::size_t reads = 0;
  const auto acquired = mq135_adc_filter::acquire([&]() { return unordered[reads++]; });
  expect(reads == 11 && acquired.isValid() && acquired.raw == result.raw,
         "adaptador adquiere once códigos nuevos y devuelve una sola mediana");
}

void referenceTimingAndFailure() {
  using namespace mq135_acquisition;
  expect(WARMUP_MS == 30000 && SAMPLE_INTERVAL_MS == 2000 &&
             mq135_site_config::REFERENCE_SAMPLE_COUNT == 3,
         "filtrado conserva calentamiento, cadencia y referencia breve de tres");
  Controller<3> controller;
  controller.begin(0);
  const auto config = syntheticConfiguration();
  controller.configure(config);
  expect(controller.startReference() == Status::WarmupIncomplete,
         "filtro no permite capturar antes de 30 segundos");
  controller.poll(30000);
  controller.observe(2000, true, 2000.0, noEnvironment);
  expect(controller.startReference() == Status::Ok,
         "lote SINTÉTICO nuevo requiere solicitud explícita");
  const Samples burst{{2000, 1998, 2001, 2000, 2100, 2002, 1999, 2000, 1800, 2000, 2001}};
  std::size_t reads = 0;
  for (std::size_t i = 0; i < 3; ++i) {
    const auto now = static_cast<std::uint32_t>(32000 + i * 2000);
    expect(!controller.poll(now - 1) && controller.capturedSamples() == i,
           "sin muestra extra ni adquisición a los 1999 ms del turno anterior");
    expect(controller.poll(now), "turno de referencia vence a los 2000 ms");
    const auto filtered = mq135_adc_filter::acquire([&]() {
      return burst[(reads++) % burst.size()];
    }, config.electrical.minimumUsableAdcCode);
    // Conversión ficticia SOLO del ADC representativo, no del último código.
    // No sustituye ni prueba esp_adc_cal_raw_to_voltage o EFUSE_VREF.
    const double fakeMv = 1000.0 + filtered.raw * 0.5;
    const auto observation = controller.observe(filtered.raw, filtered.isValid(),
                                               fakeMv, noEnvironment);
    expect(filtered.isValid() && observation.adcRaw == 2000 &&
               observation.capturedSamples == i + 1 && reads == (i + 1) * 11 &&
               !observation.ppm.isValid() && (i == 2 || !controller.record()),
           "once lecturas internas cuentan una sola muestra nueva por turno");
    const auto duplicate = controller.observe(filtered.raw, true, fakeMv, noEnvironment);
    expect(duplicate.measurementStatus == Status::SampleNotDue &&
               controller.capturedSamples() == i + 1,
           "no duplicar muestras dentro del mismo intervalo");
  }
  const auto record = controller.record();
  expect(record && record->sampleCount == 3 && record->durationMs == 4000 &&
             record->capture.requestedIntervalMs == 2000 && reads == 33,
         "referencia guarda tres medianas separadas por 2000 ms, no 33 muestras");
  if (record) {
    for (std::size_t i = 0; i < 3; ++i) {
      expect(record->samples[i].acquired.adcRaw == 2000 &&
                 record->samples[i].acquired.vaoV == 2.0 &&
                 record->samples[i].acquired.acquisitionUptimeMs == 32000 + i * 2000,
             "registro conserva el mismo ADC representativo y su tensión/tiempo");
    }
    const double fixedQ = record->normalizedReference.qReference;
    expect(controller.enableEmission() == Status::Ok,
           "emisión de TEST requiere habilitación separada");
    controller.poll(38000);
    const auto next = controller.observe(2001, true, 2000.5, noEnvironment);
    expect(next.ppm.isValid() && record->normalizedReference.qReference == fixedQ,
           "el filtro no convierte la referencia fija en un promedio móvil");
  }

  const int invalidCodes[] = {0, 4095, -1, 4096, 9};
  for (int invalidCode : invalidCodes) {
    Controller<3> rejectedController;
    rejectedController.begin(0);
    rejectedController.configure(config);
    rejectedController.poll(30000);
    rejectedController.observe(2000, true, 2000.0, noEnvironment);
    rejectedController.startReference();
    rejectedController.poll(32000);
    rejectedController.observe(2000, true, 2000.0, noEnvironment);
    Samples invalid = burst;
    invalid.back() = invalidCode;
    std::size_t invalidReads = 0;
    const auto filtered = mq135_adc_filter::acquire(
        [&]() { return invalid[invalidReads++]; }, config.electrical.minimumUsableAdcCode);
    rejectedController.poll(34000);
    const auto rejected = rejectedController.observe(
        filtered.raw, filtered.isValid(), std::numeric_limits<double>::quiet_NaN(),
        noEnvironment);
    expect(invalidReads == 11 && !filtered.isValid() &&
               rejected.captureStatus == filtered.status &&
               !rejectedController.capturing() && !rejectedController.record() &&
               rejectedController.capturedSamples() == 0 && !rejected.ppm.isValid(),
           "error interno aborta el lote completo y no reutiliza tensión o mediana previa");
  }

  Controller<3> pending;
  pending.begin(0);
  const auto site = mq135_site_config::electricalConfiguration();
  pending.configure(site);
  pending.poll(30000);
  const auto median = mq135_adc_filter::median(burst);
  const auto blocked = pending.observe(median.raw, false,
      std::numeric_limits<double>::quiet_NaN(), noEnvironment);
  expect(site.electricalVerified && site.adcConversionVerified &&
             blocked.ppm.status == Status::ConversionUnavailable && !pending.record(),
         "el contraste REV3 no permite reutilizar una tensión ausente o no finita");
}
}  // namespace

int main() {
  std::puts("FILTRO MQ-135: DATOS SINTÉTICOS, NO VALIDACIÓN FÍSICA");
  medianCases();
  referenceTimingAndFailure();
  std::printf("SINTÉTICO: %u comprobaciones; %u fallos\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
