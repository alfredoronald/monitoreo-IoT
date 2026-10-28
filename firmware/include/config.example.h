#ifndef FIRMWARE_CONFIG_H
#define FIRMWARE_CONFIG_H

#include <stdint.h>

// Copiar a config.h y reemplazar los valores de ejemplo por los locales.
// config.h contiene credenciales privadas y no debe versionarse.
constexpr char wifi_ssid[] = "example_wifi";
constexpr char wifi_password[] = "replace_with_wifi_password";

constexpr char mqtt_host[] = "broker.example.invalid";
constexpr uint16_t mqtt_port = 1883;
constexpr char mqtt_user[] = "example_mqtt_user";
constexpr char mqtt_password[] = "replace_with_mqtt_password";
// Identificador único por ESP32; no usar el mismo en varios dispositivos.
constexpr char mqtt_client_id[] = "esp32-aula-01";

// Reintentos programados mediante millis(), no esperas en loop().
constexpr uint32_t wifi_retry_interval_ms = 5000;
constexpr uint32_t mqtt_retry_interval_ms = 2000;
// Tiempo TCP en milisegundos; CONNACK/lectura MQTT y keepalive en segundos.
constexpr int32_t mqtt_tcp_timeout_ms = 1000;
constexpr uint16_t mqtt_socket_timeout_s = 1;
constexpr uint16_t mqtt_keepalive_s = 5;

// Completar tras aceptar #141 y registrar #142; kOhm con precisión de PostgreSQL.
#define MQ135_CALIBRATED_RO_KOHM 0.0
#define MQ135_CALIBRATION_ACCEPTED 0
// Alimentación del sensor documentada; distinta del Vref interno del ADC.
// Actualizarla junto al registro de la calibración si cambia el montaje.
#define MQ135_SUPPLY_V 4.89

#endif  // FIRMWARE_CONFIG_H
