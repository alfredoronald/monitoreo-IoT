# Preparación de la prueba #32

- La aplicación probada corresponde a `develop`. Las evidencias se entregan mediante `test/32-end-to-end-integration` hacia `feature/repo-guidelines`, conforme al área de Gestión indicada en CONTRIBUTING.md.
- Versión de aplicación: `c9f2d2d`.
- Firmware físico compilado y cargado por PlatformIO, entorno `esp32dev`, puerto COM5; resultado `SUCCESS`.
- Hardware informado por esptool: ESP32-D0WD-V3, revisión v3.1, MAC `24:dc:c3:98:90:4c`.
- Se completó la configuración privada en `firmware/include/config.h`, excluida de Git.
- Se corrigió la configuración local de C/C++ en la raíz y se añadió un workspace con la carpeta de firmware como proyecto independiente. La configuración con rutas locales no se versiona.
- La evidencia del flujo se presenta mediante reportes PNG, registros e informe de comparación.

## Comprobación previa real

Una observación MQTT de 60 segundos mostró cuatro ciclos de tres variables.
La consulta inicial tenía como máximo ID 17; la final, ID 29.
Ejemplo observado el 2026-10-13:

| Etapa | Hora UTC | Variable | Valor |
|---|---|---|---:|
| Observador MQTT | 18:38:05.554 | temperatura | 30.8 |
| PostgreSQL, ID 27 | 18:38:05.557 | temperatura | 30.80 |
| Observador MQTT | 18:38:05.838 | humedad | 24.7 |
| PostgreSQL, ID 28 | 18:38:05.839 | humedad | 24.70 |
| Observador MQTT | 18:38:05.841 | co2 | 530.2 |
| PostgreSQL, ID 29 | 18:38:05.844 | co2 | 530.20 |

Esta comprobación verificó la recepción y el guardado antes de iniciar la prueba de integración de cinco minutos. Su conteo se registra por separado.

## Incidencia previa: desconexiones MQTT recuperadas

Durante la observación serial de 75 segundos posterior a la carga se obtuvieron:

```text
[80515 ms] MQTT: desconectado; estado=-4; reconexión automática
[80516 ms] MQTT: intentando reconexión
[81766 ms] MQTT: intento fallido; estado=-4; reintento programado
[82526 ms] MQTT: intentando reconexión
[82662 ms] MQTT: conectado al broker
[84712 ms] MQTT: desconectado; estado=-3; reconexión automática
[84713 ms] MQTT: intentando reconexión
[84822 ms] MQTT: conectado al broker
```

Son tiempos de actividad del ESP32, no horarios absolutos. La conexión se recuperó;
la causa del corte no está determinada. La prueba posterior registró la continuidad
de las conexiones. No se atribuye el fallo a WiFi, broker o firmware sin
evidencia adicional.

Seguimiento: [issue #100](https://github.com/alfredoronald/monitoreo-IoT/issues/100).
Durante la prueba de integración de cinco minutos no se repitieron estos cortes.
