// ============================================================
// Monitoreo IoT - Firmware ESP32
// T-1.4: lectura del DHT22 con validación del rango del aula.
// T-1.5: inicio físico automático con referencia provisional; demo [SIMULADO].
// ============================================================
#include <Arduino.h>
#include <DHT.h>
#include <esp_adc_cal.h>
#include <esp_err.h>
#include <cstring>
#include <cstdarg>
#include "dht_validation.h"
#include "mq135_experimental.h"
#include "mq135_reference_log.h"
#include "mq135_acquisition.h"
#include "mq135_site_config.h"
#if !MQ135_SIMULATED_DEMO
#include "mq135_adc_filter.h"
#endif

namespace {
constexpr uint8_t DHT_PIN = 23;
constexpr unsigned long DHT_READ_INTERVAL_MS = 2000UL;
constexpr uint8_t MQ135_PIN = 34;
#if !MQ135_SIMULATED_DEMO
// Una sola selección para Arduino y las dos curvas esp_adc_cal.
constexpr auto MQ135_ADC_ATTENUATION = ADC_0db;
constexpr auto MQ135_ADC_CAL_ATTENUATION =
    static_cast<adc_atten_t>(MQ135_ADC_ATTENUATION);
static_assert(MQ135_ADC_CAL_ATTENUATION == ADC_ATTEN_DB_0,
              "Adquisición y conversión MQ-135 deben utilizar 0 dB");
#endif
static_assert(mq135_acquisition::WARMUP_MS == 30000UL, "Conservar los 30 segundos");
static_assert(mq135_acquisition::SAMPLE_INTERVAL_MS == 2000UL, "Conservar los 2000 ms");
using dht_validation::TEMPERATURE_MIN_C;
using dht_validation::TEMPERATURE_MAX_C;
using dht_validation::HUMIDITY_MIN_PERCENT;
using dht_validation::HUMIDITY_MAX_PERCENT;

DHT dht(DHT_PIN, DHT22);
unsigned long lastAttemptFinishedMs = 0;
mq135_acquisition::Controller<mq135_site_config::REFERENCE_CAPACITY> mq135;
esp_adc_cal_characteristics_t mq135AdcCharacteristics{};
bool mq135AdcConversionAvailable = false;
// Curva de diagnóstico independiente: nunca acredita verificación eléctrica.
// Los 4.89 V de VCC del MQ-135 NO son el Vref interno del ADC del ESP32.
constexpr std::uint32_t ADC_NOMINAL_VREF_MV = 1100;
struct Mq135AdcDiagnostic {
  esp_err_t tpStatus;
  esp_err_t vrefStatus;
  esp_adc_cal_value_t source;
  esp_adc_cal_value_t estimatorSource;
  bool available;
  bool measuredFallback;
  esp_adc_cal_characteristics_t characteristics;
};
Mq135AdcDiagnostic mq135AdcDiagnostic{};
struct DhtSnapshot {
  bool available;
  float temperature;
  float humidity;
  std::uint32_t timestampMs;
};
DhtSnapshot lastDhtSnapshot{};

// Salida MQ-135 acotada: nunca esperar espacio de transmisión en el loop.
char mq135SerialQueue[2048];
std::size_t mq135SerialHead = 0;
std::size_t mq135SerialTail = 0;
std::size_t mq135SerialCount = 0;
unsigned long mq135SerialOmitted = 0;

void discardMq135Output() {
  mq135SerialHead = mq135SerialTail = mq135SerialCount = 0;
}

void queueMq135Output(const char* message) {
  const std::size_t length = std::strlen(message);
  if (length > sizeof(mq135SerialQueue) - mq135SerialCount) {
    ++mq135SerialOmitted;
    return;
  }
  for (std::size_t i = 0; i < length; ++i) {
    mq135SerialQueue[mq135SerialHead] = message[i];
    mq135SerialHead = (mq135SerialHead + 1) % sizeof(mq135SerialQueue);
  }
  mq135SerialCount += length;
}

void queueMq135Printf(const char* format, ...) {
  char output[384];
  va_list arguments;
  va_start(arguments, format);
  const int written = std::vsnprintf(output, sizeof(output), format, arguments);
  va_end(arguments);
  if (written < 0 || static_cast<std::size_t>(written) >= sizeof(output)) {
    ++mq135SerialOmitted;
    return;
  }
  queueMq135Output(output);
}

void flushMq135Output() {
  if (mq135SerialOmitted > 0 &&
      sizeof(mq135SerialQueue) - mq135SerialCount >= 128) {
    const auto omitted = mq135SerialOmitted;
    mq135SerialOmitted = 0;
    queueMq135Printf("MQ-135: %lu mensajes omitidos por congestión serie\n", omitted);
  }
  const int available = Serial.availableForWrite();
  if (available <= 0 || mq135SerialCount == 0) return;
  std::size_t amount = sizeof(mq135SerialQueue) - mq135SerialTail;
  if (amount > mq135SerialCount) amount = mq135SerialCount;
  if (amount > static_cast<std::size_t>(available)) amount = available;
  if (amount > 64) amount = 64;
  const auto sent = Serial.write(
      reinterpret_cast<const uint8_t*>(mq135SerialQueue + mq135SerialTail), amount);
  mq135SerialTail = (mq135SerialTail + sent) % sizeof(mq135SerialQueue);
  mq135SerialCount -= sent;
}

// El método efectivo se obtiene del retorno de characterize, no de la mera
// presencia de eFuse: el SDK también puede deshabilitar su uso al compilar.
#if !MQ135_SIMULATED_DEMO
int mq135MinimumUsableAdcCode = 1;
enum class Mq135AutomaticStartup { WaitingForWarmup, CapturingReference, Stopped };
Mq135AutomaticStartup mq135AutomaticStartup = Mq135AutomaticStartup::Stopped;
struct LastMq135AdcDiagnostic {
  bool available;
  mq135_adc_filter::Result adc;
  bool voltageAvailable;
  std::uint32_t milliVolts;
  std::uint64_t elapsedMs;
};
LastMq135AdcDiagnostic lastMq135AdcDiagnostic{};

const char* adcCalibrationMethod(esp_adc_cal_value_t source, bool measuredFallback) {
  switch (source) {
    case ESP_ADC_CAL_VAL_EFUSE_TP: return "EFUSE_TP";
    case ESP_ADC_CAL_VAL_EFUSE_VREF: return "EFUSE_VREF";
    case ESP_ADC_CAL_VAL_DEFAULT_VREF:
      return measuredFallback ? "VREF_MEDIDA" : "VREF_NOMINAL_NO_MEDIDA";
    default: return "NO_DISPONIBLE";
  }
}

const char* adcEfusePresence(esp_err_t status) {
  if (status == ESP_OK) return "SI";
  if (status == ESP_ERR_NOT_SUPPORTED) return "NO";
  return "INDETERMINADO";
}

void reportLastMq135AdcDiagnostic() {
  const auto& last = lastMq135AdcDiagnostic;
  if (!last.available) {
    queueMq135Output("MQ-135 ADC FILTRADO [FISICO]: sin muestra posterior al calentamiento\n");
  } else if (!last.adc.isValid()) {
    queueMq135Printf(
        "MQ-135 ADC FILTRADO [FISICO]: muestras_internas=11; intervalo rechazado; codigo_invalido=%d; motivo=%s\n",
        last.adc.raw, mq135_acquisition::statusMessage(last.adc.status));
  } else if (!last.voltageAvailable) {
    queueMq135Printf(
        "MQ-135 ADC FILTRADO [FISICO]: muestras_internas=11; mediana=%d; mV no disponibles\n",
        last.adc.raw);
  } else {
    const auto config = mq135_site_config::electricalConfiguration();
    const bool withinOperatingRange = mq135AdcConversionAvailable &&
        last.milliVolts >= config.minimumAdcMilliVolts &&
        last.milliVolts <= config.maximumAdcMilliVolts;
    queueMq135Printf(
        "MQ-135 ADC FILTRADO [FISICO]: muestras_internas=11; mediana=%d; calculado=%lu mV; tiempo=%llu ms; metodo=%s; %s\n",
        last.adc.raw, static_cast<unsigned long>(last.milliVolts),
        static_cast<unsigned long long>(last.elapsedMs),
        adcCalibrationMethod(mq135AdcDiagnostic.source,
                             mq135AdcDiagnostic.measuredFallback),
        withinOperatingRange
            ? "DENTRO DEL RANGO OPERATIVO ADC; NO METROLOGICO"
            : "FUERA DEL RANGO OPERATIVO ADC O CONVERSION NO DISPONIBLE");
  }
}
#endif

void reportMq135AdcDiagnostic() {
#if MQ135_SIMULATED_DEMO
  queueMq135Output("ADC DIAGNOSTICO: inactivo [SIMULADO]; sin consulta eFuse ni ADC fisico\n");
#else
  queueMq135Output("ADC DIAGNOSTICO [FISICO]: GPIO34 / ADC1_CH6; resolucion=12 bits; atenuacion=0 dB\n");
  queueMq135Printf("ADC eFuse TP: %s (%s)\n",
                   adcEfusePresence(mq135AdcDiagnostic.tpStatus),
                   esp_err_to_name(mq135AdcDiagnostic.tpStatus));
  queueMq135Printf("ADC eFuse VREF: %s (%s)\n",
                   adcEfusePresence(mq135AdcDiagnostic.vrefStatus),
                   esp_err_to_name(mq135AdcDiagnostic.vrefStatus));
  queueMq135Printf("ADC metodo efectivo para mV diagnosticos: %s; Vref de curva=%lu mV\n",
                   adcCalibrationMethod(mq135AdcDiagnostic.source,
                                        mq135AdcDiagnostic.measuredFallback),
                   static_cast<unsigned long>(mq135AdcDiagnostic.characteristics.vref));
  queueMq135Printf("ADC metodo para estimador: %s; conversion=%s\n",
                   adcCalibrationMethod(mq135AdcDiagnostic.estimatorSource,
                                        mq135AdcDiagnostic.measuredFallback),
                   mq135AdcConversionAvailable ? "disponible bajo bloqueos existentes"
                                               : "BLOQUEADA");
  queueMq135Output("ADC DIAGNOSTICO: rango operativo 0 dB=100-950 mV; contraste puntual REV3=170-200 mV; no calibracion metrologica; esta consulta no inicia referencia ni habilita ppm\n");
  reportLastMq135AdcDiagnostic();  // Último turno; no adquiere lecturas extra.
#endif
}

void configureMq135() {
  discardMq135Output();  // No transmitir resultados pendientes de otra configuración.
  mq135AdcConversionAvailable = false;
  mq135AdcCharacteristics = {};
  mq135AdcDiagnostic = {};
  mq135AdcDiagnostic.source = ESP_ADC_CAL_VAL_NOT_SUPPORTED;
  mq135AdcDiagnostic.estimatorSource = ESP_ADC_CAL_VAL_NOT_SUPPORTED;
#if MQ135_SIMULATED_DEMO
  mq135.configureSimulatedDemo();
#else
  lastMq135AdcDiagnostic = {};
  mq135AutomaticStartup = Mq135AutomaticStartup::WaitingForWarmup;
  auto config = mq135_site_config::electricalConfiguration();
  // Consultas independientes, incluso con los bloqueos físicos activos.
  // Solo lectura: no escribe eFuse, no adquiere ADC, no mide ni enruta Vref.
  mq135AdcDiagnostic.tpStatus = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP);
  mq135AdcDiagnostic.vrefStatus = esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF);
  mq135AdcDiagnostic.measuredFallback = config.measuredAdcVrefMilliVolts > 0;
  const auto diagnosticFallback = mq135AdcDiagnostic.measuredFallback
      ? config.measuredAdcVrefMilliVolts : ADC_NOMINAL_VREF_MV;
  mq135AdcDiagnostic.source = esp_adc_cal_characterize(
      ADC_UNIT_1, MQ135_ADC_CAL_ATTENUATION, ADC_WIDTH_BIT_12, diagnosticFallback,
      &mq135AdcDiagnostic.characteristics);
  mq135AdcDiagnostic.available =
      mq135AdcDiagnostic.source == ESP_ADC_CAL_VAL_EFUSE_TP ||
      mq135AdcDiagnostic.source == ESP_ADC_CAL_VAL_EFUSE_VREF ||
      mq135AdcDiagnostic.source == ESP_ADC_CAL_VAL_DEFAULT_VREF;
  // Arduino-ESP32 2.0.17: el mismo conversor usado por analogReadMilliVolts.
  // Convertir el ADC registrado evita adquirir otra muestra para los mV.
  if (config.electricalVerified && config.adcConversionVerified) {
    const bool efuseAvailable =
        mq135AdcDiagnostic.tpStatus == ESP_OK ||
        mq135AdcDiagnostic.vrefStatus == ESP_OK;
    if (efuseAvailable || config.measuredAdcVrefMilliVolts > 0) {
      const auto source = esp_adc_cal_characterize(
          ADC_UNIT_1, MQ135_ADC_CAL_ATTENUATION, ADC_WIDTH_BIT_12,
          config.measuredAdcVrefMilliVolts, &mq135AdcCharacteristics);
      mq135AdcDiagnostic.estimatorSource = source;
      mq135AdcConversionAvailable =
          source == ESP_ADC_CAL_VAL_EFUSE_TP ||
          source == ESP_ADC_CAL_VAL_EFUSE_VREF ||
          (source == ESP_ADC_CAL_VAL_DEFAULT_VREF &&
           config.measuredAdcVrefMilliVolts > 0);
    }
  }
  // eFuse/fallback disponible no acredita seguridad ni contraste con multímetro.
  config.adcConversionVerified = config.adcConversionVerified &&
                                 mq135AdcConversionAvailable;
  mq135.configure(config);  // Revisión/evidencia explícitas o bloqueo por defecto.
  mq135MinimumUsableAdcCode = mq135.electricalReady()
      ? config.electrical.minimumUsableAdcCode : 1;
#endif
#if MQ135_SIMULATED_DEMO
  reportMq135AdcDiagnostic();  // Conservar la salida del entorno demo.
#endif
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("Firmware ESP32 iniciado");
  dht.begin();
  lastAttemptFinishedMs = millis();
#if !MQ135_SIMULATED_DEMO
  analogReadResolution(12);
  analogSetPinAttenuation(MQ135_PIN, MQ135_ADC_ATTENUATION);
#endif
  // La atenuación interna no protege GPIO34 frente a una AO excesiva.
  mq135.begin(static_cast<std::uint32_t>(millis()));
  configureMq135();
  if (mq135_site_config::SIMULATED_DEMO) {
    queueMq135Output("MQ-135 [SIMULADO]: demostracion sin AO ni adquisicion ADC fisica\n");
    queueMq135Output("MQ-135 [SIMULADO]: referencia demostrativa fija q=4; espera de 30000 ms\n");
  } else {
    queueMq135Output("MQ-135 [FISICO]: inicio automático tras calentamiento y tres muestras válidas; comandos: mq135 ayuda\n");
  }
}

namespace {
void readDhtIfDue() {
  const unsigned long attemptTimestampMs = millis();
  // La resta unsigned mantiene el intervalo cuando millis() desborda.
  if (attemptTimestampMs - lastAttemptFinishedMs < DHT_READ_INTERVAL_MS) {
    return;
  }

  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();  // Celsius por defecto.
  // Esperar el intervalo completo tras cada intento, incluso si falla.
  lastAttemptFinishedMs = millis();
  // Copia para trazabilidad; no cambia lecturas, validación ni intervalo DHT22.
  lastDhtSnapshot.available = false;

  const auto validation = dht_validation::validate(temperature, humidity);
  if (validation.hasNaN) {
    Serial.printf("[%lu ms] DHT22 ERROR: lectura NaN; muestra descartada\n",
                  attemptTimestampMs);
    return;
  }

  if (!validation.isValid()) {
    if (validation.temperatureOutOfRange) {
      Serial.printf(
          "[%lu ms] DHT22 DESCARTADA: temperatura %.1f C fuera de [%.1f, %.1f] C\n",
          attemptTimestampMs, temperature, TEMPERATURE_MIN_C, TEMPERATURE_MAX_C);
    }
    if (validation.humidityOutOfRange) {
      Serial.printf(
          "[%lu ms] DHT22 DESCARTADA: humedad %.1f %% HR fuera de [%.1f, %.1f] %% HR\n",
          attemptTimestampMs, humidity, HUMIDITY_MIN_PERCENT,
          HUMIDITY_MAX_PERCENT);
    }
    return;
  }

  lastDhtSnapshot = {true, temperature, humidity,
                     static_cast<std::uint32_t>(attemptTimestampMs)};
  Serial.printf("[%lu ms] DHT22 OK: T=%.1f C; HR=%.1f %%\n",
                attemptTimestampMs, temperature, humidity);
}

void updateMq135() {
  const auto timestampMs = static_cast<std::uint32_t>(millis());
  if (!mq135.poll(timestampMs)) return;

#if MQ135_SIMULATED_DEMO
  const auto observation = mq135.observeSimulated();
#else
  int adcValue = 0;
  bool voltageAvailable = false;
  double milliVolts = std::numeric_limits<double>::quiet_NaN();
  mq135_experimental::EnvironmentalConditions environment{
      false, std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::quiet_NaN(), 0};
  if (mq135.warmupComplete()) {
    // 11 lecturas consecutivas, sin delay; una sola observación cada 2000 ms.
    const auto filteredAdc = mq135_adc_filter::acquire(
        []() { return analogRead(MQ135_PIN); }, mq135MinimumUsableAdcCode);
    adcValue = filteredAdc.raw;
    // Diagnóstico de la MISMA mediana, separado de voltageAvailable/milliVolts.
    // No entregar un fallback nominal al estimador ni eludir sus bloqueos.
    lastMq135AdcDiagnostic = {
        true, filteredAdc, false, 0, mq135.warmupElapsedMs()};
    if (filteredAdc.isValid() && mq135AdcDiagnostic.available) {
      lastMq135AdcDiagnostic.milliVolts = esp_adc_cal_raw_to_voltage(
          static_cast<std::uint32_t>(adcValue), &mq135AdcDiagnostic.characteristics);
      lastMq135AdcDiagnostic.voltageAvailable = true;
    }
    if (mq135.electricalReady() && mq135AdcConversionAvailable &&
        filteredAdc.isValid()) {
      milliVolts = esp_adc_cal_raw_to_voltage(
          static_cast<std::uint32_t>(adcValue), &mq135AdcCharacteristics);
      voltageAvailable = true;
    }
    const auto dhtAgeMs = static_cast<std::uint32_t>(
        timestampMs - lastDhtSnapshot.timestampMs);
    if (lastDhtSnapshot.available && dhtAgeMs <= mq135.uptimeMs()) {
      environment = {true, lastDhtSnapshot.temperature, lastDhtSnapshot.humidity,
                     mq135.uptimeMs() - dhtAgeMs};
    }
  }
  const auto observation = mq135.observe(adcValue, voltageAvailable,
                                        milliVolts, environment);
#endif
  char output[384];
  const int written = mq135_acquisition::formatObservation(observation, output,
                                                          sizeof(output));
  if (written >= 0 && static_cast<std::size_t>(written) < sizeof(output)) {
    queueMq135Output(output);
  } else {
    queueMq135Output(mq135_site_config::SIMULATED_DEMO
        ? "CO2 experimental: bloqueado - formato de salida invalido [SIMULADO]\n"
        : "CO2 experimental: bloqueado - formato de salida inválido\n");
  }
  if (observation.captureStatus != mq135_experimental::Status::CaptureNotActive) {
    queueMq135Printf("Referencia MQ-135: %s; muestras=%u; captura=%s\n",
                  mq135_acquisition::statusMessage(observation.captureStatus),
                  static_cast<unsigned>(observation.capturedSamples),
                  observation.capturing ? "activa" : "finalizada");
  }
#if !MQ135_SIMULATED_DEMO
  using mq135_experimental::Status;
  if (mq135AutomaticStartup == Mq135AutomaticStartup::WaitingForWarmup &&
      mq135.warmupComplete()) {
    // Solicitar después de observar el turno: el registro exige muestras
    // posteriores a la solicitud. Las tres siguientes conservan los 2000 ms.
    const auto result = mq135.startReference();
    mq135AutomaticStartup = result == Status::Ok
        ? Mq135AutomaticStartup::CapturingReference : Mq135AutomaticStartup::Stopped;
    queueMq135Printf("MQ-135 inicio automático: referencia - %s\n",
                    mq135_acquisition::statusMessage(result));
  } else if (mq135AutomaticStartup == Mq135AutomaticStartup::CapturingReference &&
             !mq135.capturing()) {
    // Un único intento por arranque. Un fallo requiere recuperación manual;
    // no completar lotes con muestras antiguas ni reintentar silenciosamente.
    mq135AutomaticStartup = Mq135AutomaticStartup::Stopped;
    if (observation.captureStatus == Status::Ok && mq135.record() != nullptr) {
      const auto result = mq135.enableEmission();  // Conserva todos los bloqueos.
      queueMq135Printf("MQ-135 inicio automático: emisión - %s\n",
                      mq135_acquisition::statusMessage(result));
      // La primera estimación saldrá en el próximo turno, sin otra adquisición.
    } else {
      queueMq135Printf("MQ-135 inicio automático detenido: %s; recuperación manual\n",
                      mq135_acquisition::statusMessage(observation.captureStatus));
    }
  }
#endif
}

void handleMq135Command(const char* command) {
  using mq135_experimental::Status;
  Status result = Status::Ok;
  if (std::strcmp(command, "mq135 adc diagnostico") == 0) {
    reportMq135AdcDiagnostic();  // Reimprime datos del arranque, sin adquirir ADC.
    return;
  }
  if (mq135_site_config::SIMULATED_DEMO) {
    if (std::strcmp(command, "mq135 estado") == 0) {
      queueMq135Printf("MQ-135: %s [SIMULADO]; referencia demostrativa fija q=4; sin referencia fisica\n",
                       mq135_acquisition::stateName(mq135.state()));
    } else if (std::strcmp(command, "mq135 ayuda") == 0) {
      queueMq135Output("MQ-135 [SIMULADO]: mq135 estado | mq135 ayuda | mq135 adc diagnostico; comandos fisicos bloqueados\n");
      queueMq135Output("MQ-135 [SIMULADO]: volver al modo normal cargando el entorno esp32dev\n");
    } else {
      queueMq135Output("MQ-135 [SIMULADO]: comando rechazado; no modifica referencia ni emision fisicas\n");
    }
    return;
  }
#if !MQ135_SIMULATED_DEMO
  // Una orden manual de control toma prioridad sobre el inicio automático.
  // Diagnóstico, estado, ayuda y órdenes desconocidas no lo interrumpen.
  if (std::strcmp(command, "mq135 referencia iniciar") == 0 ||
      std::strcmp(command, "mq135 referencia cancelar") == 0 ||
      std::strcmp(command, "mq135 referencia invalidar") == 0 ||
      std::strcmp(command, "mq135 ppm habilitar") == 0 ||
      std::strcmp(command, "mq135 ppm deshabilitar") == 0) {
    mq135AutomaticStartup = Mq135AutomaticStartup::Stopped;
  }
#endif
  if (std::strcmp(command, "mq135 referencia iniciar") == 0) {
    // Sin reloj conectado: no inventar fecha/hora. La API admite fuente externa.
    result = mq135.startReference();
  } else if (std::strcmp(command, "mq135 referencia cancelar") == 0) {
    mq135.cancelReference();
  } else if (std::strcmp(command, "mq135 referencia invalidar") == 0) {
    mq135.invalidateReference();
    discardMq135Output();
  } else if (std::strcmp(command, "mq135 ppm habilitar") == 0) {
    // El comando es la habilitación explícita. El controlador conserva los
    // bloqueos eléctricos y la referencia física; no exige calibración HU-08.
    result = mq135.enableEmission();
  } else if (std::strcmp(command, "mq135 ppm deshabilitar") == 0) {
    mq135.disableEmission();
    discardMq135Output();
  } else if (std::strcmp(command, "mq135 estado") == 0) {
    queueMq135Printf("MQ-135: %s; captura=%s; muestras=%u; referencia=%s\n",
                  mq135_acquisition::stateName(mq135.state()),
                  mq135.capturing() ? "activa" : "inactiva",
                  static_cast<unsigned>(mq135.capturedSamples()),
                  mq135.record() ? "establecida" : "pendiente");
    queueMq135Printf("Referencia CA-03: %u muestras nuevas; media de q; sin límite de dispersión; intervalo=%lu ms\n",
                    static_cast<unsigned>(mq135_site_config::REFERENCE_SAMPLE_COUNT),
                    static_cast<unsigned long>(mq135_acquisition::SAMPLE_INTERVAL_MS));
    if (mq135.record()) {
      queueMq135Printf("Referencia fija: q=%.9g; revisión=%lu; 400 ppm SUPUESTOS\n",
                    mq135.record()->normalizedReference.qReference,
                    static_cast<unsigned long>(
                        mq135.record()->normalizedReference.circuitRevision));
      queueMq135Printf("Lote físico: muestras=%u; duración=%llu ms; dispersión informativa=%.3f %%\n",
                      static_cast<unsigned>(mq135.record()->sampleCount),
                      static_cast<unsigned long long>(mq135.record()->durationMs),
                      100.0 * mq135.record()->observedRelativeSpread);
    }
    return;
  } else if (std::strcmp(command, "mq135 ayuda") == 0) {
    queueMq135Output("mq135 estado | mq135 referencia iniciar/cancelar/invalidar\n");
    queueMq135Output("mq135 adc diagnostico: reimprimir eFuse y metodo, sin muestrear\n");
    queueMq135Output("mq135 ppm habilitar/deshabilitar; ver firmware/docs/mq135-experimental.md\n");
    return;
  } else {
    queueMq135Output("Comando MQ-135 no reconocido; usar mq135 ayuda\n");
    return;
  }
  queueMq135Printf("MQ-135 comando: %s\n", mq135_acquisition::statusMessage(result));
}

void processMq135Commands() {
  static char command[64];
  static std::size_t length = 0;
  static bool overflow = false;
  // Máximo 32 caracteres por vuelta; sin readStringUntil, delay ni espera.
  for (unsigned budget = 0; budget < 32 && Serial.available() > 0; ++budget) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\r') continue;
    if (character == '\n') {
      if (overflow) queueMq135Output("Comando MQ-135 descartado: demasiado largo\n");
      else if (length > 0) {
        command[length] = '\0';
        handleMq135Command(command);
      }
      length = 0;
      overflow = false;
    } else if (!overflow) {
      if (length + 1 < sizeof(command)) command[length++] = character;
      else overflow = true;
    }
  }
}
}  // namespace

void loop() {
  readDhtIfDue();
  updateMq135();
  processMq135Commands();
  flushMq135Output();
}
