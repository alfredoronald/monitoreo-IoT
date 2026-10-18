#ifndef MONITOREO_IOT_MQ135_SITE_CONFIG_H
#define MONITOREO_IOT_MQ135_SITE_CONFIG_H

#include "mq135_acquisition.h"
#include <cstring>

#ifndef MQ135_SIMULATED_DEMO
#define MQ135_SIMULATED_DEMO 0
#endif
#if MQ135_SIMULATED_DEMO != 0 && MQ135_SIMULATED_DEMO != 1
#error "MQ135_SIMULATED_DEMO debe ser 0 o 1"
#endif

namespace mq135_site_config {
constexpr bool SIMULATED_DEMO = MQ135_SIMULATED_DEMO == 1;
// Inicio automático físico; "mq135 ppm habilitar" conserva la recuperación manual.
// Las verificaciones de entrada y conversión siguen en electricalConfiguration.
// Tres turnos físicos nuevos (4 s entre primero y último), no calibración HU-08.
constexpr std::size_t REFERENCE_SAMPLE_COUNT = 3;
constexpr std::size_t REFERENCE_CAPACITY = REFERENCE_SAMPLE_COUNT;
static_assert(REFERENCE_SAMPLE_COUNT >= 2 &&
              REFERENCE_SAMPLE_COUNT <= REFERENCE_CAPACITY,
              "El lote de referencia debe caber completo en RAM");

inline mq135_acquisition::Configuration electricalConfiguration() {
  // REV3: divisor comunicado + ADC de 12 bits / 0 dB contrastado puntualmente.
  // R1=10 kΩ entre AO y GPIO34; R2=10 kΩ a GND.
  // AO=0.24 V / GPIO34=0.12 V: k nominal y medido=0.50; GND compartida.
  // Compatibilidad del divisor para este montaje alimentado desde VIN/5V.
  // No equivale a validar transitorios, otra fuente o toda la escala ADC.
  auto config = mq135_acquisition::pendingConfiguration();
  config.electricalVerified = true;  // Divisor instalado y comprobado por el responsable.
  config.directAoConnection = false;
  config.adcToAoVoltageRatio = 0.5;  // VAO = VADC / 0.5; nunca usar VADC como AO.
  config.measuredVcV = 4.89;  // Medición puntual comunicada, no estabilidad probada.
  config.electrical.circuitRevision = 3;
  std::strcpy(config.verificationEvidenceId,
              "docs/evidencias/t1-5/mq135-divisor-10k-rev3.md");
  std::strcpy(config.electrical.circuitDescription,
              "MQ-135 VIN/5V=4.89 V; AO-R1=10k-GPIO34; R2=10k GPIO34-GND; GND comun; k=0.50");
  std::strcpy(config.electrical.adcConversionId,
              "EFUSE_VREF-ADC1_CH6-12bit-0dB-REV3-OPERATIVO-100-950mV");
  // Mediana física=511 -> 191 mV; multímetro GPIO34–GND=170–200 mV.
  // Rango operativo recomendado para ESP32 a 0 dB: 100–950 mV (Espressif,
  // ESP-IDF v4.4.8, guía ADC). No confundirlo con el contraste de 170–200 mV.
  // La coincidencia puntual no demuestra precisión ni calibración metrológica.
  config.minimumAdcMilliVolts = 100;
  config.maximumAdcMilliVolts = 950;
  config.electrical.minimumUsableAdcCode = 1;  // Excluye cero; no prueba precisión.
  config.electrical.minimumUsefulVaoV =
      config.minimumAdcMilliVolts / 1000.0 / config.adcToAoVoltageRatio;
  // Contraste REV3 comunicado: 191 mV cae en 170–200 mV, mismo nodo/montaje,
  // 0 dB y método EFUSE_VREF. Sin pares sincronizados ni incertidumbre aportada.
  // Verificación experimental puntual, no calibración de CO2 HU-08.
  // configureMq135 conserva el bloqueo si el SDK no aporta conversión válida.
  config.adcConversionVerified = true;
  config.referenceSampleCount = REFERENCE_SAMPLE_COUNT;
  config.maxRelativeSpread = mq135_experimental::NO_SPREAD_LIMIT;
  // TP no disponible; VREF eFuse=1058 mV comunicado. Se consulta en el chip:
  // NO poner 1058 en measuredAdcVrefMilliVolts, que es un fallback MEDIDO externo.
  return config;  // El inicio automático usa las mismas verificaciones que los comandos.
}
}  // namespace mq135_site_config

#endif  // MONITOREO_IOT_MQ135_SITE_CONFIG_H
