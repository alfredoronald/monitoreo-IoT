// ============================================================
// Monitoreo IoT - Firmware ESP32
// T-1.4: lectura del DHT22 con validación del rango del aula.
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
#include "../include/config.h"  // Privado; ruta explícita para evitar otro config.h.
#include "connection_retry.h"
#include "dht_validation.h"

// Versión final: diagnóstico temporal excluido; activar solo para investigar.
#ifndef WIFI_TEMPORARY_DIAGNOSTIC
#define WIFI_TEMPORARY_DIAGNOSTIC 0
#endif

// Versión final: comandos y métricas de prueba temporal excluidos.
#ifndef WIFI_TEMPORARY_TEST
#define WIFI_TEMPORARY_TEST 0
#endif

#if WIFI_TEMPORARY_DIAGNOSTIC || WIFI_TEMPORARY_TEST
#include <cstring>
#endif

namespace {
constexpr uint8_t DHT_PIN = 23;
constexpr unsigned long DHT_READ_INTERVAL_MS = 2000UL;
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
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("Firmware ESP32 iniciado");
  dht.begin();
  lastAttemptFinishedMs = millis();
  // ESP32-WROOM-32: comunicación en core 0; loop() conserva sus lecturas.
  if (xTaskCreatePinnedToCore(connectionTask, "wifi-mqtt", 4096, nullptr, 1,
                            nullptr, 0) != pdPASS) {
    Serial.println("WiFi/MQTT ERROR: no se pudo crear la tarea; sensores continúan");
  }
}

void loop() {
  const unsigned long attemptTimestampMs = millis();
  // La resta unsigned mantiene el intervalo cuando millis() desborda.
  if (attemptTimestampMs - lastAttemptFinishedMs < DHT_READ_INTERVAL_MS) {
    return;
  }

  const float humidity = dht.readHumidity();
  const float temperature = dht.readTemperature();  // Celsius por defecto.
  // Esperar el intervalo completo tras cada intento, incluso si falla.
  lastAttemptFinishedMs = millis();

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

  Serial.printf("[%lu ms] DHT22 OK: T=%.1f C; HR=%.1f %%\n",
                attemptTimestampMs, temperature, humidity);
}
