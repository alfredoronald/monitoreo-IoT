# Validación física — Issue #12, T-1.6: WiFi y MQTT

Rama: `feat/12-wifi-mqtt-reconnect`.

## Objetivo

Registrar la conexión WiFi/MQTT del ESP32 y su recuperación sin bloquear las
lecturas DHT22. Los resultados siguientes provienen de las cuatro capturas
revisadas del monitor serie y de la comprobación del estado de `config.h` en
Git. En esta revisión documental no se ejecutaron tests, build ni upload.

## Configuración del prototipo

| Elemento | Configuración |
|---|---|
| Dispositivo | ESP32-WROOM-32; entorno PlatformIO `esp32dev`, framework Arduino |
| WiFi | Estación (`WIFI_STA`) en una red de 2,4 GHz; IP asignada `192.168.1.12` |
| Broker | Mosquitto accesible desde la red local; conexión mediante PubSubClient |
| Parámetros privados | SSID, contraseña WiFi, host/puerto MQTT, usuario, contraseña e identificador de cliente obtenidos de `firmware/include/config.h` |
| Gestión de conexiones | Tarea de red independiente de `loop()`, planificador mediante `millis()` y reintentos del flujo normal |
| Sensor | DHT22 en GPIO23, intervalo configurado de 2000 ms después de cada intento de lectura |
| Prueba WiFi | Comandos temporales con `WIFI_TEMPORARY_TEST=1`; desconexión voluntaria de la estación, con el router disponible |

Las capturas no documentan la versión de Mosquitto ni el host y puerto efectivos
del broker. No se incorporan credenciales privadas a este registro.

## Resultados y evidencias

### CA-01 — Conexión WiFi, IP y MQTT

El monitor muestra asociación WiFi, obtención de IP y conexión al broker:

- `4753 ms`: `WiFi: conectado; IP=192.168.1.12`.
- `4803 ms`: `MQTT: conectado al broker`.

Evidencia: [CA01_WiFi_MQTT_Conexion_PASS.png](CA01_WiFi_MQTT_Conexion_PASS.png).

### CA-02 — Reconexión MQTT

Con WiFi disponible, el firmware detectó la desconexión MQTT en `147476 ms`
(estado `-3`). Realizó un intento que falló (estado `-2`) y volvió a intentar
la conexión sin reiniciar el dispositivo. En `149645 ms` se registró
`MQTT: conectado al broker`.

Tiempo observado: **149645 − 147476 = 2169 ms**, menor de 30000 ms.
La base temporal es la **detección de la desconexión por el firmware**. La
captura no registra el instante externo en que el broker volvió a estar
disponible; este dato no debe confundirse con una medición desde ese instante.

Evidencia: [CA02_MQTT_Reconexion_PASS.png](CA02_MQTT_Reconexion_PASS.png).

### CA-02 — Reconexión WiFi controlada

Se ejecutó `wifi prueba desconectar`. La API devolvió `ESP_OK`, se suspendieron
los reintentos y el SDK confirmó la desconexión en `20385 ms`, con motivo
`ASSOC_LEAVE` (código `8`). El socket MQTT se cerró por pérdida de WiFi y
continuaron las lecturas DHT22 durante la pausa.

En `26968 ms` se aceptó `wifi prueba reconectar`, que reactivó el mismo
planificador y las llamadas normales de conexión WiFi/MQTT.

| Hito | Timestamp | Tiempo desde la solicitud |
|---|---:|---:|
| Solicitud de recuperación | 26968 ms | 0 ms |
| IP recuperada, origen `WL_CONNECTED observado` | 27148 ms | **180 ms** |
| MQTT recuperado | 27190 ms | **222 ms** |

Ambos tiempos son menores de 30000 ms. MQTT se recuperó **42 ms después de la
observación de IP**. La IP volvió a ser `192.168.1.12`. Los 180 ms corresponden
al momento en que el firmware observó `WL_CONNECTED`, como indica la captura,
y no al timestamp exacto del evento SDK `GOT_IP`. La pausa voluntaria previa
queda fuera de los tiempos de recuperación.

Evidencia: [CA02_WiFi_Reconexion_PASS.png](CA02_WiFi_Reconexion_PASS.png).

### CA-03 — Configuración privada mediante config.h

El firmware obtiene sus credenciales y parámetros de conexión de
`firmware/include/config.h`. La captura CA-01 muestra que el SSID del driver
coincide con el argumento procedente de la configuración utilizado en
`WiFi.begin()`; no muestra la contraseña.

Se comprobó mediante operaciones de lectura de Git:

- `git check-ignore -v -- firmware/include/config.h`: coincide con la regla
  `firmware/include/config.h` de `.gitignore`, línea 16.
- `git ls-files -- firmware/include/config.h`: no devuelve entradas; el archivo
  no está versionado en la rama revisada.

La plantilla `firmware/include/config.example.h` permite preparar la
configuración local sin publicar valores privados. No se modificó `config.h`.

### CA-04 — Continuidad del DHT22

Las lecturas válidas del DHT22 continúan durante la desconexión controlada y
después de recuperar WiFi/MQTT. Por ejemplo, la captura muestra lecturas en
`22091`, `24097`, `26103`, `28109` y `30115 ms`: separaciones de **2006 ms**,
aproximadamente cada 2 segundos. El intervalo nominal de 2000 ms se cuenta
después de cada intento; los timestamps incluyen el tiempo de lectura.

La captura de reconexión MQTT también muestra lecturas DHT22 durante los
intentos fallidos y la recuperación del broker. No se observa una pausa del
sensor causada por esos reintentos en los segmentos registrados.

Evidencia: [CA04_DHT22_Continuidad_PASS.png](CA04_DHT22_Continuidad_PASS.png),
complementada por las dos capturas de CA-02.

## Revisión visual de las capturas

Se verificó la existencia y se inspeccionó el contenido visible de los cuatro
archivos enlazados. **No se encontraron contraseñas, tokens ni otros secretos
visibles.** Las imágenes muestran datos de diagnóstico como SSID en HEX, IP,
identificador de cliente y rutas locales. No se modificó ninguna imagen.

## Limitaciones y alcance

- La prueba WiFi provoca una desconexión real pero **controlada de la estación**;
  no apaga el router ni reproduce una caída física del AP. Por ello no mide
  detección de pérdida de balizas ni recuperación después de reiniciar el router.
- Los tiempos corresponden a los ciclos capturados y a las bases temporales
  indicadas; no establecen un máximo universal para otras condiciones de red.
- La continuidad DHT22 se acredita en los segmentos visibles, sin afirmar una
  prueba prolongada adicional que no esté registrada.
- Las estimaciones de CO₂ en ppm no forman parte de esta Issue. La publicación
  MQTT de `ambiente/temperatura`, `ambiente/humedad` y `ambiente/co2` corresponde
  a la Issue #13; aquí se valida la conexión y reconexión al broker.

Procedimiento técnico y retirada de los comandos temporales:
[wifi-mqtt-reconnect.md](../../../firmware/docs/wifi-mqtt-reconnect.md).
