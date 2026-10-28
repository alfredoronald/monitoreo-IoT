#ifndef MONITOREO_IOT_MQTT_PUBLICATION_CONFIG_H
#define MONITOREO_IOT_MQTT_PUBLICATION_CONFIG_H

#include <cstdint>

// Prueba temporal CA-04: desactivada para la versión del PR.
#ifndef MQTT_TEMPORARY_TEST
#define MQTT_TEMPORARY_TEST 0
#endif
#if MQTT_TEMPORARY_TEST != 0 && MQTT_TEMPORARY_TEST != 1
#error "MQTT_TEMPORARY_TEST debe ser 0 o 1"
#endif

// Valor por defecto público; config.h privado puede definirlo antes de incluir este archivo.
// También admite -D MQTT_PUBLISH_INTERVAL_MS=10000 (hasta 30000).
#ifndef MQTT_PUBLISH_INTERVAL_MS
#define MQTT_PUBLISH_INTERVAL_MS 15000UL
#endif
static_assert(MQTT_PUBLISH_INTERVAL_MS >= 10000UL &&
              MQTT_PUBLISH_INTERVAL_MS <= 30000UL,
              "El intervalo MQTT debe estar entre 10000 y 30000 ms");

namespace mqtt_publication_config {
constexpr std::uint32_t INTERVAL_MS = MQTT_PUBLISH_INTERVAL_MS;
}  // namespace mqtt_publication_config

#endif  // MONITOREO_IOT_MQTT_PUBLICATION_CONFIG_H
