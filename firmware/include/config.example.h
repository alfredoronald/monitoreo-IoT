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

// T-8.5: publicar cada 15 s; lectura DHT22/MQ-135 permanece cada 2 s.
#define MQTT_PUBLISH_INTERVAL_MS 15000UL

#endif  // FIRMWARE_CONFIG_H
