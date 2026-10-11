// ============================================================
// Monitoreo IoT - Firmware ESP32
// T-1.4: lectura del DHT22 con validación del rango del aula.
// T-1.5: inicio físico automático con referencia provisional; demo [SIMULADO].
// T-1.6: WiFi/MQTT con reconexión, sin esperar conexiones en loop().
// ============================================================
#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_adc_cal.h>
#include <cstring>
#include <cstdarg>
#include "../include/config.h"  // Privado; ruta explícita para evitar otro config.h.
#include "connection_retry.h"
#include "dht_validation.h"
#include "mq135_experimental.h"
#include "mq135_reference_log.h"
#include "mq135_acquisition.h"
#include "mq135_site_config.h"
#if !MQ135_SIMULATED_DEMO
#include "mq135_adc_filter.h"
#endif

// Versión final: diagnóstico temporal excluido; activar solo para investigar.
#ifndef WIFI_TEMPORARY_DIAGNOSTIC
#define WIFI_TEMPORARY_DIAGNOSTIC 0
#endif

// Versión final: comandos y métricas de prueba temporal excluidos.
#ifndef WIFI_TEMPORARY_TEST
#define WIFI_TEMPORARY_TEST 0
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

constexpr unsigned long WIFI_ATTEMPT_WATCHDOG_MS = 20000UL;
enum class WifiEventKind { Started, Associated, Disconnected, Stopped, GotIp, ScanDone };
struct WifiObservation {
  unsigned long timestampMs;
  WifiEventKind kind;
  bool fromArduino;
  uint32_t code;
  uint8_t count;
};
QueueHandle_t wifiObservations = nullptr;

void reportWifiSdkResult(const char* operation, esp_err_t result) {
#if !WIFI_TEMPORARY_DIAGNOSTIC
  if (result == ESP_OK) return;  // En producción, conservar solo errores del SDK.
#endif
  Serial.printf("[%lu ms] WiFi: %s; resultado=%s (0x%X)\n", millis(), operation,
                esp_err_to_name(result), static_cast<unsigned>(result));
}

void queueWifiObservation(WifiEventKind kind, bool fromArduino,
                          uint32_t code = 0, uint8_t count = 0) {
  const WifiObservation observation{millis(), kind, fromArduino, code, count};
  if (wifiObservations != nullptr) xQueueSend(wifiObservations, &observation, 0);
}

// Los eventos SDK conservan el estado del intento de conexión. El diagnóstico
// añade la comparación con Arduino. Nunca imprimir ni conectar aquí.
void onWifiSdkEvent(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base == IP_EVENT) {
    if (id == IP_EVENT_STA_GOT_IP) queueWifiObservation(WifiEventKind::GotIp, false);
    return;
  }
  switch (id) {
#if WIFI_TEMPORARY_DIAGNOSTIC
    case WIFI_EVENT_STA_START: queueWifiObservation(WifiEventKind::Started, false); break;
#endif
    case WIFI_EVENT_STA_CONNECTED: queueWifiObservation(WifiEventKind::Associated, false); break;
    case WIFI_EVENT_STA_DISCONNECTED:
      queueWifiObservation(WifiEventKind::Disconnected, false,
                           static_cast<wifi_event_sta_disconnected_t*>(data)->reason);
      break;
    case WIFI_EVENT_STA_STOP: queueWifiObservation(WifiEventKind::Stopped, false); break;
#if WIFI_TEMPORARY_DIAGNOSTIC
    case WIFI_EVENT_SCAN_DONE: {
      const auto* scan = static_cast<wifi_event_sta_scan_done_t*>(data);
      queueWifiObservation(WifiEventKind::ScanDone, false, scan->status, scan->number);
      break;
    }
#endif
    default: break;
  }
}

#if WIFI_TEMPORARY_DIAGNOSTIC
void onWifiArduinoEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START: queueWifiObservation(WifiEventKind::Started, true); break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: queueWifiObservation(WifiEventKind::Associated, true); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      queueWifiObservation(WifiEventKind::Disconnected, true, info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP: queueWifiObservation(WifiEventKind::Stopped, true); break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: queueWifiObservation(WifiEventKind::GotIp, true); break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      // Arduino ya ha recogido los registros antes de llamar a este callback.
      queueWifiObservation(WifiEventKind::ScanDone, true,
                           info.wifi_scan_done.status, info.wifi_scan_done.number);
      break;
    default: break;
  }
}
#endif  // WIFI_TEMPORARY_DIAGNOSTIC

#if WIFI_TEMPORARY_TEST
// Estado exclusivo de connectionTask; los callbacks solo usan la cola existente.
// No copiar credenciales ni persistir datos de la prueba.
struct WifiTemporaryTest {
  bool retriesSuspended = false;
  bool disconnectConfirmed = false;
  bool recoveryRequested = false;
  bool ipReported = false;
  unsigned long disconnectRequestedMs = 0;
  unsigned long recoveryRequestedMs = 0;
  unsigned long ipRecoveryMs = 0;
  char command[32]{};
  size_t commandLength = 0;
  bool commandOverflow = false;
};
WifiTemporaryTest wifiTest;

void reportWifiTestIp(unsigned long timestampMs, const char* origin) {
  if (!wifiTest.recoveryRequested || wifiTest.retriesSuspended || wifiTest.ipReported ||
      static_cast<int32_t>(timestampMs - wifiTest.recoveryRequestedMs) < 0) return;
  wifiTest.ipRecoveryMs = timestampMs - wifiTest.recoveryRequestedMs;
  wifiTest.ipReported = true;
  Serial.printf("[%lu ms] WiFi PRUEBA: IP recuperada; desde solicitud=%lu ms; origen=%s\n",
                timestampMs, wifiTest.ipRecoveryMs, origin);
}

void reportWifiTestMqtt(unsigned long timestampMs) {
  if (!wifiTest.recoveryRequested || wifiTest.retriesSuspended) return;
  // Normalmente la métrica IP ya proviene de GOT_IP. Si faltó ese evento,
  // el cliente solo llega aquí tras comprobar WL_CONNECTED en el flujo normal.
  reportWifiTestIp(timestampMs, "WL_CONNECTED observado al aceptar MQTT");
  Serial.printf("[%lu ms] WiFi PRUEBA: MQTT recuperado; desde solicitud=%lu ms; "
                "desde IP=%lu ms\n", timestampMs,
                timestampMs - wifiTest.recoveryRequestedMs,
                timestampMs - wifiTest.recoveryRequestedMs - wifiTest.ipRecoveryMs);
  wifiTest.recoveryRequested = false;  // Una métrica por ciclo, sin reiniciar tiempos.
}

void handleWifiTestCommand(const char* command, bool& attemptPending, bool& associated) {
  if (std::strcmp(command, "wifi prueba desconectar") == 0) {
    if (wifiTest.retriesSuspended) {
      Serial.println("WiFi PRUEBA: ya desconectando/desconectado; reintentos suspendidos");
      return;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi PRUEBA: comando rechazado; esperar conexión WiFi e IP antes de desconectar");
      return;
    }
    const unsigned long requestedMs = millis();
    wifiTest.retriesSuspended = true;  // Antes de solicitar la desconexión.
    // Mantener STA, configuración en RAM y auto-reconexión desactivada.
    // Arduino no reintenta internamente el motivo voluntario ASSOC_LEAVE.
    const esp_err_t result = esp_wifi_disconnect();
    reportWifiSdkResult("esp_wifi_disconnect (prueba temporal)", result);
    if (result != ESP_OK) {
      wifiTest.retriesSuspended = false;
      Serial.println("WiFi PRUEBA: desconexión no aceptada; gestión normal conservada");
      return;
    }
    wifiTest.disconnectRequestedMs = requestedMs;
    wifiTest.disconnectConfirmed = false;
    wifiTest.recoveryRequested = false;
    wifiTest.ipReported = false;
    attemptPending = false;
    associated = false;
    Serial.printf("[%lu ms] WiFi PRUEBA: desconexión solicitada; reintentos suspendidos; "
                  "esperando confirmación; sensores continúan\n", requestedMs);
  } else if (std::strcmp(command, "wifi prueba reconectar") == 0) {
    if (!wifiTest.retriesSuspended) {
      Serial.println("WiFi PRUEBA: sin pausa activa; no se reinicia la conexión ni la medición");
      return;
    }
    if (!wifiTest.disconnectConfirmed && WiFi.status() != WL_CONNECTED) {
      // Respaldo para recuperar la prueba si se perdió el evento en la cola.
      wifi_ap_record_t ap{};
      const esp_err_t result = esp_wifi_sta_get_ap_info(&ap);
      reportWifiSdkResult("comprobación de desconexión para prueba", result);
      if (result == ESP_ERR_WIFI_NOT_CONNECT) {
        wifiTest.disconnectConfirmed = true;
        Serial.printf("[%lu ms] WiFi PRUEBA: desconexión confirmada por consulta del driver; "
                      "momento de observación\n", millis());
      }
    }
    if (!wifiTest.disconnectConfirmed || WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi PRUEBA: esperar confirmación de desconexión y repetir el comando");
      return;
    }
    wifiTest.recoveryRequestedMs = millis();
    wifiTest.ipReported = false;
    wifiTest.recoveryRequested = true;
    wifiTest.retriesSuspended = false;
    attemptPending = false;
    associated = false;
    // No llamar a begin/connect aquí ni reiniciar los intervalos: el mismo
    // Scheduler y las mismas llamadas de producción realizan la recuperación.
    Serial.printf("[%lu ms] WiFi PRUEBA: recuperación solicitada; reintentos normales reactivados\n",
                  wifiTest.recoveryRequestedMs);
  } else {
    // No repetir el texto recibido: podría contener información privada.
    Serial.println("WiFi PRUEBA: comando no reconocido; usar wifi prueba desconectar/reconectar");
  }
}

void pollWifiTestCommands(bool& attemptPending, bool& associated) {
  // Lectura incremental y acotada. Nunca readStringUntil(), esperas de línea
  // ni un bucle que pueda quedarse consumiendo Serial permanentemente.
  for (unsigned readCount = 0; readCount < 32 && Serial.available() > 0; ++readCount) {
    const int received = Serial.read();
    if (received < 0) break;
    const char character = static_cast<char>(received);
    if (character == '\r' || character == '\n') {
      if (wifiTest.commandOverflow) {
        Serial.println("WiFi PRUEBA: línea inválida o demasiado larga; descartada sin ejecutarla");
      } else if (wifiTest.commandLength != 0) {
        wifiTest.command[wifiTest.commandLength] = '\0';
        handleWifiTestCommand(wifiTest.command, attemptPending, associated);
      }
      std::memset(wifiTest.command, 0, sizeof(wifiTest.command));
      wifiTest.commandLength = 0;
      wifiTest.commandOverflow = false;
    } else if (!wifiTest.commandOverflow) {
      if (character == '\0' || wifiTest.commandLength >= sizeof(wifiTest.command) - 1) {
        wifiTest.commandOverflow = true;  // Descartar todo hasta fin de línea.
      } else {
        wifiTest.command[wifiTest.commandLength++] = character;
      }
    }
  }
}
#endif  // WIFI_TEMPORARY_TEST

#if WIFI_TEMPORARY_DIAGNOSTIC
enum class WifiScanDiagnosticState { Waiting, Reporting, Done };
WifiScanDiagnosticState wifiScanDiagnosticState = WifiScanDiagnosticState::Done;
unsigned long wifiScanStartedMs = 0;
int16_t wifiScanNetworkCount = 0;
int wifiScanNetworkIndex = 0;
bool configuredSsidDetected = false;
bool wifiScanResultsComplete = true;
bool wifiSdkScanDone = false;
bool wifiArduinoScanDone = false;
uint32_t wifiScanStatus = 0;
uint8_t wifiScanDriverCount = 0;
constexpr unsigned long WIFI_SCAN_DEADLINE_MS = 15000UL;

void reportWifiSsidBytes(const char* origin, const uint8_t* ssid, size_t length) {
  // Solo SSID. Nunca pasar contraseñas ni volcar wifi_config_t a esta función.
  // Acotar también el diagnóstico de una configuración de más de 32 bytes.
  constexpr size_t MAX_DIAGNOSTIC_BYTES = 64;
  char hex[MAX_DIAGNOSTIC_BYTES * 3 + 1]{};
  constexpr char digits[] = "0123456789ABCDEF";
  const size_t shown = length < MAX_DIAGNOSTIC_BYTES ? length : MAX_DIAGNOSTIC_BYTES;
  size_t cursor = 0;
  for (size_t index = 0; index < shown; ++index) {
    if (index != 0) hex[cursor++] = ' ';
    hex[cursor++] = digits[ssid[index] >> 4];
    hex[cursor++] = digits[ssid[index] & 0x0F];
  }
  Serial.printf("WiFi DIAG: SSID %s; longitud=%u bytes; HEX=%s%s\n", origin,
                static_cast<unsigned>(length), hex,
                shown < length ? " [TRUNCADO A 64 BYTES]" : "");
}

void reportWifiDriverSsid() {
  wifi_config_t actual{};
  const esp_err_t result = esp_wifi_get_config(WIFI_IF_STA, &actual);
  reportWifiSdkResult("esp_wifi_get_config (solo diagnóstico SSID)", result);
  if (result != ESP_OK) return;
  const size_t length = strnlen(reinterpret_cast<const char*>(actual.sta.ssid),
                                sizeof(actual.sta.ssid));
  reportWifiSsidBytes("driver STA", actual.sta.ssid, length);
  const bool same = length == std::strlen(wifi_ssid) &&
                    std::memcmp(actual.sta.ssid, wifi_ssid, length) == 0;
  Serial.printf("WiFi DIAG: SSID driver == SSID de config usado en begin: %s\n",
                same ? "SI" : "NO");
}

const char* wifiSecurityName(wifi_auth_mode_t security) {
  switch (security) {
    case WIFI_AUTH_OPEN: return "ABIERTA";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-PSK";
    case WIFI_AUTH_WAPI_PSK: return "WAPI-PSK";
    case WIFI_AUTH_WPA3_ENT_192: return "WPA3-Enterprise-192";
    default: return "DESCONOCIDA";
  }
}

const char* wifiDisconnectDescription(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND: return "AP no encontrado durante el intento";
    case WIFI_REASON_AUTH_FAIL: return "autenticación fallida; causa por confirmar";
    case WIFI_REASON_AUTH_EXPIRE: return "autenticación expirada";
    case WIFI_REASON_AUTH_LEAVE: return "salida de autenticación; posible reintento local";
    case WIFI_REASON_ASSOC_LEAVE: return "salida de asociación; posible desconexión local";
    case WIFI_REASON_ASSOC_FAIL: return "fallo de asociación con el AP";
    case WIFI_REASON_ASSOC_TOOMANY: return "AP rechaza asociación por exceso de estaciones";
    case WIFI_REASON_BEACON_TIMEOUT: return "se agotó la espera de balizas del AP";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "se agotó la negociación de claves";
    case WIFI_REASON_CONNECTION_FAIL: return "fallo de conexión informado por el driver";
    default: return "motivo del controlador; ver nombre simbólico del SDK";
  }
}

void startWifiDiagnostic() {
  Serial.println("WiFi DIAG: escaneo asíncrono único de 2,4 GHz; sin contraseñas");
  wifiScanStartedMs = millis();
  // Configuración completa, sin campos indeterminados ni timeout interno de
  // WiFi.scanNetworks(120): Arduino 2.x lo fija a 120 * 20 = 2400 ms.
  wifi_scan_config_t scan{};
  scan.show_hidden = true;
  scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  scan.scan_time.active.min = 100;
  scan.scan_time.active.max = 300;
  WiFi.scanDelete();
  const esp_err_t result = esp_wifi_scan_start(&scan, false);
  reportWifiSdkResult("esp_wifi_scan_start", result);
  wifiScanDiagnosticState = WifiScanDiagnosticState::Waiting;
  if (result != ESP_OK) {
    wifiScanDiagnosticState = WifiScanDiagnosticState::Done;
    Serial.println("WiFi DIAG: escaneo no iniciado; presencia del SSID desconocida; conexión continúa");
  }
}

bool wifiDiagnosticPending() {
  if (wifiScanDiagnosticState == WifiScanDiagnosticState::Done) return false;
  if (wifiScanDiagnosticState == WifiScanDiagnosticState::Waiting) {
    if (!wifiArduinoScanDone && millis() - wifiScanStartedMs < WIFI_SCAN_DEADLINE_MS) {
      return true;  // Volver al siguiente turno, sin esperar aquí.
    }
    const int16_t result = wifiArduinoScanDone ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
    if (!wifiArduinoScanDone || wifiScanStatus != 0 || result < 0 ||
        (wifiScanDriverCount > 0 && result == 0)) {
      Serial.printf("WiFi DIAG: escaneo sin resultados válidos; SDK_SCAN_DONE=%s; "
                    "ARDUINO_SCAN_DONE=%s; status=%lu; scanComplete=%d; "
                    "límite=%lu ms; presencia del SSID desconocida\n",
                    wifiSdkScanDone ? "SI" : "NO", wifiArduinoScanDone ? "SI" : "NO",
                    static_cast<unsigned long>(wifiScanStatus), static_cast<int>(result),
                    WIFI_SCAN_DEADLINE_MS);
      if (!wifiArduinoScanDone) reportWifiSdkResult("esp_wifi_scan_stop", esp_wifi_scan_stop());
      // Si el callback Arduino llega tarde, processWifiObservation libera la
      // lista. Evitar borrarla mientras el framework todavía la construye.
      if (wifiArduinoScanDone) WiFi.scanDelete();
      wifiScanDiagnosticState = WifiScanDiagnosticState::Done;
      Serial.println("WiFi DIAG: escaneo fallido/agotado; presencia del SSID desconocida; conexión continúa");
      return false;
    }
    wifiScanNetworkCount = result;
    wifiScanDiagnosticState = WifiScanDiagnosticState::Reporting;
    Serial.printf("WiFi DIAG: redes detectadas=%d\n", static_cast<int>(result));
  }
  if (wifiScanNetworkIndex < wifiScanNetworkCount) {
    // Acceso público del SDK Arduino con índice int; evita truncarlo a uint8_t
    // como hacen SSID(index)/RSSI(index) si hubiera más de 256 resultados.
    const auto* ap = static_cast<const wifi_ap_record_t*>(
        WiFi.getScanInfoByIndex(wifiScanNetworkIndex++));
    if (ap == nullptr) {
      wifiScanResultsComplete = false;
      Serial.println("WiFi DIAG: registro de red no disponible");
      return true;
    }
    const char* ssid = reinterpret_cast<const char*>(ap->ssid);
    const size_t detectedLength = strnlen(ssid, sizeof(ap->ssid));
    const size_t configuredLength = std::strlen(wifi_ssid);
    const bool validSsid = detectedLength <= 32;
    const bool matches = validSsid && detectedLength != 0 &&
                         detectedLength == configuredLength &&
                         std::memcmp(ap->ssid, wifi_ssid, detectedLength) == 0;
    if (!validSsid) wifiScanResultsComplete = false;
    configuredSsidDetected = configuredSsidDetected || matches;
    Serial.printf("WiFi DIAG: SSID=\"%.*s\"; canal=%u; RSSI=%d dBm; seguridad=%s (%u); objetivo=%s\n",
                  detectedLength == 0 ? 8 : static_cast<int>(detectedLength),
                  detectedLength == 0 ? "<oculto>" : ssid,
                  static_cast<unsigned>(ap->primary), static_cast<int>(ap->rssi),
                  wifiSecurityName(ap->authmode), static_cast<unsigned>(ap->authmode),
                  matches ? "SI" : "NO");
    reportWifiSsidBytes("detectado", ap->ssid, detectedLength);
    size_t firstDifference = 0;
    while (firstDifference < detectedLength && firstDifference < configuredLength &&
           ap->ssid[firstDifference] == static_cast<uint8_t>(wifi_ssid[firstDifference])) {
      ++firstDifference;
    }
    Serial.printf("WiFi DIAG: comparación SSID; longitud_config=%u; longitud_detectado=%u; "
                  "primera_diferencia=%d (índice desde 0; -1=iguales)\n",
                  static_cast<unsigned>(configuredLength), static_cast<unsigned>(detectedLength),
                  detectedLength == configuredLength && firstDifference == detectedLength
                      ? -1 : static_cast<int>(firstDifference));
    return true;  // Una red por turno para no emitir toda la lista de golpe.
  }
  Serial.printf("WiFi DIAG: SSID configurado %s; coincidencia exacta; escaneo puntual\n",
                configuredSsidDetected ? "DETECTADO"
                : wifiScanResultsComplete ? "NO DETECTADO" : "NO CONFIRMADO (lista incompleta)");
  Serial.println("WiFi DIAG: un SSID oculto puede no identificarse; fin de escaneo, conexión normal");
  WiFi.scanDelete();
  wifiScanDiagnosticState = WifiScanDiagnosticState::Done;
  return false;
}
#endif  // WIFI_TEMPORARY_DIAGNOSTIC

void processWifiObservation(bool& attemptPending, bool& associated) {
  WifiObservation event{};
  // Una entrada por turno, sin bloquear ni imprimir desde callbacks.
  if (wifiObservations == nullptr ||
      xQueueReceive(wifiObservations, &event, 0) != pdTRUE) return;
  if (!event.fromArduino) {
    if (event.kind == WifiEventKind::Associated || event.kind == WifiEventKind::GotIp) {
      associated = true;
    } else if (event.kind == WifiEventKind::Disconnected || event.kind == WifiEventKind::Stopped) {
      associated = false;
      attemptPending = false;
    }
  }
#if WIFI_TEMPORARY_TEST
  if (!event.fromArduino && event.kind == WifiEventKind::Disconnected &&
      wifiTest.retriesSuspended && !wifiTest.disconnectConfirmed &&
      static_cast<int32_t>(event.timestampMs - wifiTest.disconnectRequestedMs) >= 0) {
    wifiTest.disconnectConfirmed = true;
    Serial.printf("[%lu ms] WiFi PRUEBA: desconexión confirmada por SDK; "
                  "desde solicitud=%lu ms; reintentos suspendidos\n", event.timestampMs,
                  event.timestampMs - wifiTest.disconnectRequestedMs);
  }
  if (!event.fromArduino && event.kind == WifiEventKind::GotIp) {
    reportWifiTestIp(event.timestampMs, "GOT_IP SDK");
  }
#endif
#if WIFI_TEMPORARY_DIAGNOSTIC
  const char* source = event.fromArduino ? "Arduino" : "SDK";
  if (event.kind == WifiEventKind::ScanDone) {
    if (event.fromArduino) {
      wifiArduinoScanDone = true;
      // Si llegó tarde, liberar registros que Arduino acaba de recoger.
      if (wifiScanDiagnosticState == WifiScanDiagnosticState::Done) WiFi.scanDelete();
    } else {
      wifiSdkScanDone = true;
    }
    wifiScanDriverCount = event.count;
    wifiScanStatus = event.code;
    Serial.printf("[%lu ms] WiFi DIAG: evento %s SCAN_DONE; status=%lu; redes=%u\n",
                  event.timestampMs, source, static_cast<unsigned long>(event.code),
                  static_cast<unsigned>(event.count));
  } else if (event.kind == WifiEventKind::Disconnected) {
    Serial.printf("[%lu ms] WiFi DIAG: desconexión %s; código=%u; motivo=%s; %s\n",
                  event.timestampMs, source, static_cast<unsigned>(event.code),
                  WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(event.code)),
                  wifiDisconnectDescription(static_cast<uint8_t>(event.code)));
  } else {
    const char* stage = event.kind == WifiEventKind::Started ? "STA_START"
        : event.kind == WifiEventKind::Associated ? "STA_CONNECTED (asociado; espera IP)"
        : event.kind == WifiEventKind::GotIp ? "GOT_IP" : "STA_STOP";
    Serial.printf("[%lu ms] WiFi DIAG: evento %s %s\n", event.timestampMs, source, stage);
  }
#endif
}

static_assert(wifi_retry_interval_ms > 0 && wifi_retry_interval_ms < 30000,
              "El reintento WiFi debe ser positivo y menor de 30 segundos");
static_assert(mqtt_retry_interval_ms > 0 && mqtt_retry_interval_ms < 30000,
              "El reintento MQTT debe ser positivo y menor de 30 segundos");
static_assert(mqtt_tcp_timeout_ms > 0 && mqtt_tcp_timeout_ms <= 5000,
              "Limitar el timeout TCP a un máximo de 5 segundos");
static_assert(mqtt_socket_timeout_s > 0 && mqtt_socket_timeout_s <= 5 &&
              mqtt_keepalive_s > 0 && mqtt_keepalive_s <= 10,
              "MQTT requiere timeouts y keepalive cortos para detectar cortes");

// PubSubClient llama a connect(host/IP, port). En Arduino-ESP32 2.x ese método
// usa un timeout TCP distinto de setSocketTimeout(). Acotarlo en ambos casos.
class MqttTransport : public WiFiClient {
 public:
  using WiFiClient::connect;
  int connect(IPAddress ip, uint16_t port) override {
    return WiFiClient::connect(ip, port, mqtt_tcp_timeout_ms);
  }
  int connect(const char* host, uint16_t port) override {
    return WiFiClient::connect(host, port, mqtt_tcp_timeout_ms);
  }
};

void connectionTask(void*) {
  // Propiedad exclusiva de esta tarea: ni loop() ni callbacks usan el cliente.
  MqttTransport transport;
  PubSubClient mqtt(transport);
  connection_retry::Scheduler retries(wifi_retry_interval_ms,
                                      mqtt_retry_interval_ms);
  mqtt.setServer(mqtt_host, mqtt_port);
  mqtt.setSocketTimeout(mqtt_socket_timeout_s);
  mqtt.setKeepAlive(mqtt_keepalive_s);
  wifiObservations = xQueueCreate(16, sizeof(WifiObservation));
  if (wifiObservations == nullptr) {
    Serial.println("WiFi ERROR: sin memoria para eventos; se usará comprobación del driver");
  }
#if WIFI_TEMPORARY_DIAGNOSTIC
  WiFi.onEvent(onWifiArduinoEvent);
#endif
  WiFi.persistent(false);
  const bool stationEnabled = WiFi.mode(WIFI_STA);
  if (!stationEnabled || WIFI_TEMPORARY_DIAGNOSTIC) {
    Serial.printf("WiFi: WiFi.mode(WIFI_STA)=%s\n", stationEnabled ? "OK" : "FALLO");
  }
#if WIFI_TEMPORARY_DIAGNOSTIC
  wifi_mode_t driverMode = WIFI_MODE_NULL;
  const esp_err_t modeResult = esp_wifi_get_mode(&driverMode);
  reportWifiSdkResult("esp_wifi_get_mode", modeResult);
  if (modeResult == ESP_OK) {
    Serial.printf("WiFi: modo del driver=%u\n", static_cast<unsigned>(driverMode));
  }
#endif
  reportWifiSdkResult("registro eventos WIFI_EVENT",
                      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiSdkEvent, nullptr));
  reportWifiSdkResult("registro evento GOT_IP",
                      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onWifiSdkEvent, nullptr));
  // Arduino 2.x puede hacer su primer reintento interno; no añadir cancelaciones
  // periódicas mediante reconnect(). Desactivar sus reintentos posteriores.
  WiFi.setAutoReconnect(false);
  bool wifiStarted = false;
  bool wifiAttemptPending = false;
  bool wifiAssociated = false;
  bool wifiCancelSettling = false;
  unsigned long wifiAttemptCheckedMs = 0;
  bool mqttStarted = false;

#if WIFI_TEMPORARY_DIAGNOSTIC
  Serial.printf("WiFi DIAG: config=firmware/include/config.h; fuente=%s; compilación=%s %s\n",
                __FILE__, __DATE__, __TIME__);
  reportWifiSsidBytes("configurado", reinterpret_cast<const uint8_t*>(wifi_ssid),
                      std::strlen(wifi_ssid));
  if (std::strlen(wifi_ssid) == 0 || std::strlen(wifi_ssid) > 32) {
    Serial.println("WiFi DIAG: SSID configurado fuera de longitud válida (1–32 bytes)");
  }
  startWifiDiagnostic();
#endif

#if WIFI_TEMPORARY_TEST
  Serial.println("WiFi PRUEBA: comandos temporales habilitados; wifi prueba desconectar / wifi prueba reconectar");
#endif

  for (;;) {
#if WIFI_TEMPORARY_TEST
    pollWifiTestCommands(wifiAttemptPending, wifiAssociated);
#endif
    processWifiObservation(wifiAttemptPending, wifiAssociated);
#if WIFI_TEMPORARY_DIAGNOSTIC
    if (wifiDiagnosticPending()) {
      // Solo aplaza el primer intento WiFi; DHT22 sigue en loop(), otro core.
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
#endif
    const bool wifiSuspended =
#if WIFI_TEMPORARY_TEST
        wifiTest.retriesSuspended;
#else
        false;
#endif
    if (!wifiSuspended && WiFi.status() == WL_CONNECTED && mqtt.connected()) {
      mqtt.loop();  // Mantener PINGREQ/PINGRESP solo en la tarea de red.
    }
    // Reconsultar WiFi después de una posible espera de lectura MQTT.
    const bool wifiReady = !wifiSuspended && WiFi.status() == WL_CONNECTED;
    // Dar tiempo a entregar los eventos de una cancelación local antes de
    // iniciar otro intento, usando el intervalo de reintento ya configurado.
    if (wifiCancelSettling && millis() - wifiAttemptCheckedMs >= wifi_retry_interval_ms) {
      wifiCancelSettling = false;
    }
    if (wifiReady) {
      wifiAttemptPending = false;
      wifiAssociated = true;
    } else if (!wifiSuspended && (wifiAttemptPending || wifiAssociated) &&
               millis() - wifiAttemptCheckedMs >= WIFI_ATTEMPT_WATCHDOG_MS) {
      // Respaldo ante eventos ausentes/perdidos. No cortar una asociación válida
      // que todavía espera DHCP o cuya IP no se reflejó en el estado Arduino.
      wifi_ap_record_t ap{};
      const esp_err_t apResult = esp_wifi_sta_get_ap_info(&ap);
      reportWifiSdkResult("comprobación esp_wifi_sta_get_ap_info", apResult);
#if WIFI_TEMPORARY_DIAGNOSTIC
      Serial.printf("WiFi DIAG: estado Arduino=%d; asociado=%s; revisión tras %lu ms\n",
                    static_cast<int>(WiFi.status()), apResult == ESP_OK ? "SI" : "NO CONFIRMADO",
                    WIFI_ATTEMPT_WATCHDOG_MS);
#endif
      wifiAttemptCheckedMs = millis();
      if (apResult == ESP_OK) {
        wifiAssociated = true;
        Serial.println("WiFi: asociación conservada; esperando IP, sin reiniciar autenticación");
      } else if (apResult == ESP_ERR_WIFI_NOT_CONNECT) {
        Serial.println("WiFi: intento sin asociación tras 20 s; cancelación local, no motivo del AP");
        const esp_err_t stopResult = esp_wifi_disconnect();
        reportWifiSdkResult("esp_wifi_disconnect (watchdog local)", stopResult);
        if (stopResult == ESP_OK) {
          wifiAttemptPending = false;
          wifiAssociated = false;
          wifiCancelSettling = true;
        }
      }
    }
    const auto decision = retries.poll(millis(), wifiReady,
                                       wifiReady && mqtt.connected(),
                                       wifiAttemptPending || wifiAssociated ||
                                           wifiCancelSettling || wifiSuspended);
    if (decision.wifiDisconnected) {
      transport.stop();  // No conservar un socket de la conexión WiFi anterior.
#if WIFI_TEMPORARY_TEST
      if (wifiSuspended) {
        Serial.printf("[%lu ms] WiFi: desconectado; reintentos suspendidos por prueba\n", millis());
      } else
#endif
      {
        Serial.printf("[%lu ms] WiFi: desconectado; reconexión automática\n", millis());
      }
    }
    if (decision.mqttDisconnected) {
      if (wifiReady) {
        Serial.printf("[%lu ms] MQTT: desconectado; estado=%d; reconexión automática\n",
                      millis(), mqtt.state());
      } else {
        Serial.printf("[%lu ms] MQTT: desconectado por pérdida WiFi; sensores continúan\n",
                      millis());
      }
      transport.stop();
    }
    if (decision.wifiConnected) {
      Serial.printf("[%lu ms] WiFi: conectado; IP=%s\n", millis(),
                    WiFi.localIP().toString().c_str());
#if WIFI_TEMPORARY_TEST
      // Respaldo si el evento GOT_IP no se entregó a la cola: medir el momento
      // de observación de WL_CONNECTED, sin inventar el timestamp del evento.
      reportWifiTestIp(millis(), "WL_CONNECTED observado");
#endif
    }
    if (decision.mqttConnected) {
      Serial.printf("[%lu ms] MQTT: conectado al broker\n", millis());
    }
    if (decision.tryWifi) {
      Serial.printf("[%lu ms] WiFi: %s\n", millis(),
                    wifiStarted ? "intentando reconexión" : "iniciando conexión");
      // Cargar credenciales una vez, sin iniciar asociación desde begin().
      // El retorno del SDK al conectar queda disponible para el diagnóstico.
      if (!wifiStarted) {
#if WIFI_TEMPORARY_DIAGNOSTIC
        reportWifiSsidBytes("argumento WiFi.begin", reinterpret_cast<const uint8_t*>(wifi_ssid),
                            std::strlen(wifi_ssid));
#endif
        const wl_status_t configured = WiFi.begin(wifi_ssid, wifi_password, 0, nullptr, false);
        wifiStarted = configured != WL_CONNECT_FAILED;
        if (!wifiStarted || WIFI_TEMPORARY_DIAGNOSTIC) {
          Serial.printf("WiFi: configuración STA mediante begin(connect=false); estado=%d; %s\n",
                        static_cast<int>(configured), wifiStarted ? "aceptada" : "FALLO");
        }
#if WIFI_TEMPORARY_DIAGNOSTIC
        reportWifiDriverSsid();
#endif
      }
      if (wifiStarted) {
        const esp_err_t result = esp_wifi_connect();
        reportWifiSdkResult("esp_wifi_connect", result);
        wifiAttemptPending = result == ESP_OK;
        if (result == ESP_ERR_WIFI_CONN) {
          // Puede coincidir con el primer reintento interno de Arduino 2.x.
          // El código también representa otros errores del bloque de control:
          // no afirmar conexión ni repetir/cancelar inmediatamente el intento.
          wifiAttemptPending = true;
          Serial.println("WiFi: ESP_ERR_WIFI_CONN; estado interno por confirmar; "
                         "esperando evento o comprobación acotada del driver");
        }
        wifiAttemptCheckedMs = millis();
      }
    }
    if (decision.tryMqtt) {
      Serial.printf("[%lu ms] MQTT: %s\n", millis(),
                    mqttStarted ? "intentando reconexión" : "iniciando conexión");
      mqttStarted = true;
      // connect() y loop() de PubSubClient pueden esperar: solo en esta tarea,
      // nunca en el loop de sensores. No imprimir credenciales ni publicar topics.
      const char* user = mqtt_user[0] == '\0' ? nullptr : mqtt_user;
      const char* password = user == nullptr ? nullptr : mqtt_password;
      if (!mqtt.connect(mqtt_client_id, user, password)) {
        Serial.printf("[%lu ms] MQTT: intento fallido; estado=%d; reintento programado\n",
                      millis(), mqtt.state());
        transport.stop();
      }
#if WIFI_TEMPORARY_TEST
      else {
        reportWifiTestMqtt(millis());  // connect() acaba de aceptar CONNACK.
      }
#endif
    }
    // Ceder CPU entre turnos; no es una espera de conexión ni afecta al DHT22.
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

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
  // ESP32-WROOM-32: comunicación en core 0; loop() conserva sus lecturas.
  if (xTaskCreatePinnedToCore(connectionTask, "wifi-mqtt", 4096, nullptr, 1,
                            nullptr, 0) != pdPASS) {
    Serial.println("WiFi/MQTT ERROR: no se pudo crear la tarea; sensores continúan");
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
