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

#endif  // FIRMWARE_CONFIG_H
