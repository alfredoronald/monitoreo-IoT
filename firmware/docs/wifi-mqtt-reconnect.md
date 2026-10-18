# Issue #12 — T-1.6: conexión WiFi y MQTT con reconexión

## Alcance y estado

Implementación preparada en `feat/12-wifi-mqtt-reconnect`, con base para el PR
`feature/firmware-esp32`. No se ejecutaron pruebas, compilación ni upload en
esta revisión final. La validación física de CA-01 a CA-04, incluida la
recuperación WiFi controlada, está registrada en
[validacion-fisica-wifi-mqtt.md](../../docs/evidencias/T1-6/validacion-fisica-wifi-mqtt.md).
Ese registro precisa las bases temporales y las limitaciones de las pruebas.
No se declaran resultados nuevos a partir de la limpieza del código.

La versión final establece `WIFI_TEMPORARY_DIAGNOSTIC=0` y
`WIFI_TEMPORARY_TEST=0`. Excluye escaneo temporal, volcado de SSID, callback
adicional de comparación de eventos Arduino, comandos, buffer y métricas de
prueba. Conserva la cola y los observadores SDK necesarios para gestionar el
intento normal, la tarea FreeRTOS, los intervalos y las lecturas DHT22.
El monitor mantiene conexiones WiFi con IP, conexión MQTT, desconexiones,
reintentos y errores; los retornos SDK exitosos y trazas detalladas no se
imprimen en la configuración final. No se publica ningún topic MQTT.

La rama inspeccionada contiene T-1.4 (DHT22) y la plantilla de configuración.
No contiene todavía el firmware MQ-135 de `feature/11-lectura-mq135`; esta tarea
no copia ni modifica esa rama. Se conserva íntegra la adquisición y validación
DHT22: GPIO23, intervalo de 2000 ms tras cada intento y descarte de muestras
inválidas. No se publican ni se suscriben topics; `ambiente/temperatura`,
`ambiente/humedad` y `ambiente/co2` corresponden a la Issue #13.

## Implementación

Se utilizan las dependencias ya declaradas: WiFi de Arduino-ESP32, FreeRTOS
incluido en el framework y PubSubClient 2.8. No cambia `platformio.ini` ni se
añaden bibliotecas. `connection_retry.h` decide intentos y transiciones mediante
resta unsigned de `millis()`, compatible con su desbordamiento. Cada turno
ordena como máximo un intento; no hay bucles de espera de conexión.

Una única tarea `wifi-mqtt` en core 0 es propietaria de WiFi y de los clientes
TCP/MQTT. `setup()` crea esa tarea y retorna; `loop()` conserva exclusivamente
su trabajo de sensores. La tarea consulta estados, mantiene `mqtt.loop()` cuando
hay conexión y cede CPU 20 ms entre turnos. Esa pausa no ocurre en `loop()`.
Si no se puede crear la tarea se registra el error y los sensores continúan.

**PubSubClient no es un cliente asíncrono**: `connect()` espera TCP/CONNACK y
`mqtt.loop()` puede esperar lecturas. Mover solo los reintentos a `millis()` no
evitaría esas esperas dentro de `loop()`. Por eso esas llamadas se aíslan en la
tarea y se limitan sus timeouts. El adaptador `MqttTransport` aplica el timeout
TCP a ambos métodos `connect(host/IP, port)`; `setSocketTimeout()` de PubSubClient
controla otra espera, en segundos. La resolución DNS de un nombre puede añadir
latencia independiente del timeout TCP; para la prueba de menos de 30 s se
recomienda la IPv4 LAN del broker, o verificar también la disponibilidad del DNS.

Se imprime la IP al conectar WiFi, tanto al arrancar como al recuperar la red.
Una pérdida de WiFi invalida el estado MQTT y cierra el socket anterior. MQTT
solo se intenta con WiFi conectado; al recuperarse WiFi se intenta inmediatamente.
Si solo cae el broker, se reintenta MQTT sin reiniciar WiFi. El keepalive permite
detectar un broker que deja de responder aunque el socket parezca abierto.
No se imprimen contraseñas ni otros secretos.

Los defaults son reintento WiFi **5000 ms**, MQTT **2000 ms**, TCP **1000 ms**,
espera MQTT **1 s** y keepalive **5 s**. Los reintentos se cuentan desde el inicio
del intento, pero el vencimiento del intervalo WiFi no cancela una asociación,
autenticación o espera de IP en curso. Se carga la configuración una vez con
`WiFi.begin(..., connect=false)` y se inicia cada intento con
`esp_wifi_connect()`, comprobando su retorno. No se llama periódicamente a
`WiFi.reconnect()`, que en Arduino-ESP32 2.x fuerza primero una desconexión.
Las operaciones de red solo consumen tiempo de la tarea de conexión.
Estos parámetros permiten intentar la recuperación con margen respecto de 30 s,
pero el tiempo real de asociación, DHCP, DNS y aceptación del broker se mide
en hardware. No se afirma una garantía temporal sin esa prueba.

Referencias técnicas:

- [PubSubClient 2.8: conexión y keepalive](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.cpp).
- [Arduino-ESP32 2.0.17: WiFiClient y timeout TCP](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiClient.cpp).
- [Espressif: tareas FreeRTOS y afinidad de núcleo](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/system/freertos.html).

## Diagnóstico WiFi temporal sin acceso al router

El diagnóstico usado durante la investigación se conserva como opción,
**desactivada en la versión final**. Solo si se habilita deliberadamente
`WIFI_TEMPORARY_DIAGNOSTIC=1`, al reiniciar
se ejecuta **un solo escaneo asíncrono**, antes del primer `WiFi.begin()`.
El driver puede rechazar un escaneo durante la asociación; por eso no se
repite junto con los intentos de conexión. Se utiliza `esp_wifi_scan_start()`
con configuración inicializada a cero, todos los canales disponibles, redes
ocultas incluidas y escaneo activo de 100–300 ms por canal. Se imprime el retorno
exacto (`esp_err_to_name` y código hexadecimal), sin perderlo en un `-2` genérico.
La espera se consulta por turnos y se abandona ante fallo o timeout propio de
**15000 ms**. Ese límite incluye la entrega de eventos y no equivale al tiempo
por canal. Después se muestra una red por turno,
se liberan los resultados y continúa el flujo de conexión existente.
El diagnóstico puede demorar brevemente el primer intento WiFi; no espera
en `loop()` ni cambia las lecturas DHT22 o el cliente MQTT. Deshabilitarlo
antes de medir el comportamiento definitivo de arranque de T-1.6.

Por cada red detectada se imprimen SSID público, canal, RSSI en dBm, nombre
y código del modo de seguridad, y `objetivo=SI/NO` según coincidencia exacta
con `wifi_ssid`. La conclusión indica `DETECTADO`, `NO DETECTADO` o presencia
desconocida si falló el escaneo; si faltan registros se informa lista incompleta.
Las redes ocultas pueden aparecer como `<oculto>` sin revelar su SSID, por lo
que un resultado negativo no demuestra ausencia física del AP. El escaneo
refleja los canales de 2,4 GHz disponibles para el ESP32 y su dominio regulatorio.

Se observan por separado eventos del SDK (`WIFI_EVENT` y `IP_EVENT_STA_GOT_IP`)
y callbacks Arduino: inicio/parada de STA, asociación, IP, desconexión y fin de
escaneo. Los callbacks copian únicamente tiempo, origen, tipo y códigos a una
cola de 16 entradas, sin esperar, imprimir ni conectar. La tarea imprime el
motivo numérico, nombre del SDK y descripción; también funciona para intentos
iniciales fallidos que nunca llegaron a `WL_CONNECTED`. La cola es acotada y
puede omitir eventos si se satura. Se comprueba y registra la inicialización
`WiFi.mode(WIFI_STA)`, el modo real del driver y el registro de observadores.
No se imprime la contraseña WiFi, credenciales MQTT ni el contenido completo
de `config.h`. Temporalmente sí se muestran longitud y bytes HEX del SSID
configurado, de cada SSID detectado, del argumento de `WiFi.begin()` y del SSID
STA obtenido por `esp_wifi_get_config()`. De esa estructura solo se imprime el
campo SSID; nunca la contraseña. El volcado se limita a 64 bytes e identifica
el truncamiento si una configuración inválida excediera ese límite.

`main.cpp` incluye explícitamente `../include/config.h`, evitando seleccionar
un encabezado homónimo de otra carpeta. El diagnóstico identifica esa ruta,
el archivo fuente y fecha/hora de compilación para distinguir una imagen antigua.
En la revisión se encontró un único `config.h` de aplicación y una definición
de `wifi_ssid` en él. `config.example.h` contiene su propia definición como
plantilla, pero no se incluye en el firmware. La dependencia existente
`.pio/build/esp32dev/src/main.cpp.d` señala `include/config.h`; se leyó este
registro anterior, sin ejecutar una compilación nueva. El encabezado privado
local todavía contenía el SSID de ejemplo: editar la plantilla u otra copia no
cambia el argumento utilizado por `WiFi.begin()`. No se modificó ese valor.

La comparación conserva coincidencia exacta, sin recortar espacios ni normalizar
Unicode: exige igual longitud en bytes y `memcmp()` del SSID completo. Esto es
equivalente a la comparación anterior con `strcmp()` para SSID terminados en
NUL válidos, pero acota la lectura del registro detectado. Se informa el índice
del primer byte diferente (desde 0; `-1` si son iguales). El SSID válido tiene
1–32 bytes, que pueden diferir del número de caracteres Unicode. Revisar `20`
(espacio), `09` (tabulación), `0D`/`0A`, `C2 A0` (espacio no separable), `E2 80 8B`
(espacio de ancho cero) y `EF BB BF` (BOM dentro del literal). Un BOM al principio
del archivo fuente no implica que forme parte del SSID. No se cambia la red
elegida ni se corrigen credenciales automáticamente.

El evento Arduino `SCAN_DONE` llega después de que el framework recoge los
registros. Solo entonces se usa `WiFi.scanComplete()` y la lista existente;
no se recuperan los registros dos veces desde el SDK. Se muestran `status`
(0: éxito; otro valor: fallo) y número de redes del evento. Si faltan eventos,
se informa cuáles llegaron (`SDK_SCAN_DONE`, `ARDUINO_SCAN_DONE`), el estado
de la lista y el retorno de `esp_wifi_scan_stop()`. Una lista perdida/inválida
no se interpreta como prueba de ausencia del SSID.

Si un intento no termina, cada **20000 ms** se consulta
`esp_wifi_sta_get_ap_info()`. Si hay asociación se conserva y se espera la IP,
sin reiniciar autenticación. Solo `ESP_ERR_WIFI_NOT_CONNECT` autoriza cancelar
el intento sin asociación; se imprime que la cancelación es local y su retorno,
y se espera el intervalo WiFi configurado antes del siguiente intento. Otros
errores se registran sin asumir que el AP rechazó la conexión. Los eventos del
SDK de desconexión/parada permiten terminar normalmente el intento pendiente.
Arduino 2.x puede hacer un primer reintento interno aun con auto-reconexión
desactivada; el firmware no añade cancelaciones periódicas sobre él.
Si `esp_wifi_connect()` devuelve `ESP_ERR_WIFI_CONN`, el firmware anterior
ponía el intento pendiente a falso y permitía nuevos intentos cada 5 s.
Ahora conserva la espera acotada hasta un evento o la comprobación del driver.
Puede coincidir con el primer reintento interno del framework, pero el código
de error no demuestra por sí solo que exista un intento activo: Espressif lo
define como error interno del bloque de control de STA/AP. No se trata como
`ESP_OK` ni se informa conexión aceptada. Referencia:
[Espressif, retornos de esp_wifi_connect](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/network/esp_wifi.html#_CPPv416esp_wifi_connectv).

La revisión del framework instalado comprobó dos defectos del flujo anterior:
`WiFi.scanNetworks(..., 120)` fija internamente `_scanTimeout = 120 * 20`, es
decir **2400 ms**, compatible con el primer intento observado a los 2545 ms;
y `WiFi.reconnect()` desconecta primero aunque la asociación anterior siga en
curso. Los retornos de `mode()` y `begin()` tampoco se comprobaban. Esto explica
el corte local y el riesgo de interrumpir los intentos cada 5 s; no demuestra
por qué no llegó `SCAN_DONE` o un evento de desconexión, ni la causa final de
falta de conexión. No hay evidencia para atribuirla al router.
Fuentes del framework: [WiFiScan.cpp, timeout y entrega de resultados](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiScan.cpp)
y [WiFiSTA.cpp, begin y reconnect](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiSTA.cpp).

Después de cargar manualmente cuando se autorice, abrir el monitor serie a
115200 y pulsar EN/RESET. Buscar `WiFi DIAG:` y guardar el resumen del escaneo
y los motivos posteriores, junto con los timestamps DHT22. Estos son formatos,
no resultados obtenidos:

```text
WiFi DIAG: config=firmware/include/config.h; fuente=FUENTE; compilación=FECHA HORA
WiFi DIAG: SSID configurado; longitud=N bytes; HEX=BYTES
WiFi: WiFi.mode(WIFI_STA)=OK
[TIEMPO ms] WiFi: esp_wifi_scan_start; resultado=ESP_OK (0x0)
[TIEMPO ms] WiFi DIAG: evento SDK SCAN_DONE; status=0; redes=CANTIDAD
[TIEMPO ms] WiFi DIAG: evento Arduino SCAN_DONE; status=0; redes=CANTIDAD
WiFi DIAG: SSID="RED_DETECTADA"; canal=CANAL; RSSI=VALOR dBm; seguridad=MODO (CODIGO); objetivo=SI
WiFi DIAG: SSID detectado; longitud=N bytes; HEX=BYTES
WiFi DIAG: comparación SSID; longitud_config=N; longitud_detectado=N; primera_diferencia=-1 (índice desde 0; -1=iguales)
WiFi DIAG: SSID configurado DETECTADO; coincidencia exacta; escaneo puntual
WiFi DIAG: SSID argumento WiFi.begin; longitud=N bytes; HEX=BYTES
WiFi DIAG: SSID driver STA; longitud=N bytes; HEX=BYTES
WiFi DIAG: SSID driver == SSID de config usado en begin: SI
[TIEMPO ms] WiFi: esp_wifi_connect; resultado=ESP_OK (0x0)
[TIEMPO ms] WiFi DIAG: desconexión SDK; código=CODIGO; motivo=NOMBRE_SDK; DESCRIPCION
```

Guardar el log completo desde RESET al menos 60 s. Si aparecen eventos SDK
pero no Arduino, investigar la entrega de eventos del framework; si no aparece
ninguno, conservar retornos de inicialización, registro, escaneo y conexión y
la consulta de asociación. `STA_CONNECTED` sin `GOT_IP` separa asociación de
obtención de dirección. Si ambos llegan, debe aparecer la IP normal. Comparar
también los timestamps DHT22 para comprobar que el diagnóstico sigue cediendo
CPU. Ninguno de estos mensajes se presenta como resultado obtenido en esta
intervención.

Para resolver la discrepancia del SSID, revisar **localmente** el valor en
`firmware/include/config.h`, no solo la plantilla. En la siguiente carga manual,
confirmar primero fecha/hora de compilación y después que los HEX configurado,
detectado, argumento de `begin()` y driver coinciden. Si el archivo se corrigió
pero el monitor muestra otros bytes, todavía debe comprobarse la imagen cargada,
el proyecto/entorno y el dispositivo/puerto seleccionado. Corregir el valor en
disco no cambia un firmware que ya está ejecutándose. El escaneo debe indicar
`objetivo=SI`; eso acredita coincidencia del SSID, no autenticación ni DHCP.

Interpretación orientativa:

- `NO_AP_FOUND` (201): revisar nombre exacto, banda de 2,4 GHz, cobertura y
  posible red oculta; el escaneo no prueba que el AP acepte la conexión.
- `AUTH_FAIL` (202): falló autenticación; no demuestra por sí solo que la
  contraseña esté mal. Revisar localmente credenciales y seguridad observada.
- `HANDSHAKE_TIMEOUT` (204) o `4WAY_HANDSHAKE_TIMEOUT` (15): negociación de
  claves sin completar; no permite asignar una causa única.
- `BEACON_TIMEOUT` (200): se agotó la espera de balizas del AP.
- `AUTH_LEAVE`/`ASSOC_LEAVE` pueden acompañar una desconexión o reintento
  iniciado por el firmware, no necesariamente un rechazo del router.

El modo de seguridad detectado no acredita compatibilidad ni una autenticación
exitosa. Referencia: [Espressif, escaneo y motivos de desconexión](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-guides/wifi.html).
El diagnóstico ya está retirado de la compilación normal mediante
`WIFI_TEMPORARY_DIAGNOSTIC=0` en `main.cpp` (también puede definirse
`-D WIFI_TEMPORARY_DIAGNOSTIC=0` al
compilar posteriormente). Esto excluye escaneo y trazas detalladas; la cola y
observación de eventos permanecen para gestionar el intento en curso.
Se conserva la conexión/reconexión normal. No se ejecutaron tests, build ni
upload para este diagnóstico y no se modificó la configuración privada.

## Prueba temporal de reconexión WiFi sin apagar el router

La prueba documentada se ejecutó con `WIFI_TEMPORARY_TEST=1`, independiente de
`WIFI_TEMPORARY_DIAGNOSTIC`. La versión final utiliza **`WIFI_TEMPORARY_TEST=0`**
y no acepta estos comandos. Si se reactiva expresamente para otra prueba,
los comandos se procesan en la misma tarea
`wifi-mqtt`, leyendo como máximo 32 bytes por turno y sin esperar una línea.
Se aceptan CR, LF o CRLF; el buffer admite 31 caracteres. Las líneas demasiado
largas o con NUL se descartan completas. Un comando desconocido no se imprime.
No se modifica `loop()`, el intervalo DHT22 ni `config.h`; no se copian ni
persisten credenciales para la prueba. El buffer se limpia tras cada línea.

Después de compilar y cargar manualmente cuando se autorice, desde `firmware/`
abrir el monitor y esperar WiFi con IP y MQTT conectado:

```powershell
pio device monitor -e esp32dev --baud 115200
```

Enviar cada comando exactamente como aparece y terminar con Enter:

```text
wifi prueba desconectar
```

El firmware suspende los intentos antes de llamar a `esp_wifi_disconnect()`;
mantiene STA y la configuración existente, sin apagar la radio ni borrar
credenciales. Comprueba el retorno y registra el momento de la solicitud.
Si la API falla, informa el error y conserva la gestión normal. Si se acepta,
el evento SDK registra por separado el momento de desconexión confirmada.
El flujo de pérdida WiFi existente cierra el socket MQTT. Mientras la prueba
está pausada no se inicia WiFi ni MQTT, ni actúa el watchdog de asociación.
Arduino 2.x no reintenta la desconexión voluntaria `ASSOC_LEAVE`, y sus
reintentos posteriores ya están desactivados con `setAutoReconnect(false)`.

Esperar la confirmación y mantener la pausa **al menos 10 segundos** (dos
intervalos WiFi por defecto) para observar ausencia de reintentos. Para
registrar también continuidad prolongada del DHT22, mantenerla **60 segundos**.
Después enviar:

```text
wifi prueba reconectar
```

El comando registra el comienzo de la medición y retira únicamente la pausa.
No llama a `WiFi.begin()`, `esp_wifi_connect()` ni al cliente MQTT: el mismo
`Scheduler`, los mismos intervalos y las mismas llamadas del flujo normal
realizan la recuperación. No se reinician los temporizadores del planificador
para mejorar artificialmente los tiempos. Si la desconexión aún no se confirmó
o Arduino sigue indicando conexión, se pide esperar y repetir el comando.
Si se perdió el evento, una consulta al driver puede confirmar falta de
asociación; se distingue ese momento de observación del timestamp del evento.

Se informa una vez el tiempo hasta IP (evento SDK `GOT_IP`, o momento de
observación de `WL_CONNECTED` si el evento todavía no se entregó). El mensaje
indica el origen. MQTT se mide inmediatamente después del retorno exitoso del
`mqtt.connect()` normal, que acepta CONNACK. No se publica una métrica de éxito
si la operación falla; permanecen los mensajes de error y reintento existentes.
Repetir `desconectar` durante la pausa o `reconectar` sin una pausa activa no
reinicia la medición.
Para otra repetición, esperar ambas conexiones y comenzar otro ciclo.

Formatos esperados, **sin resultados físicos nuevos ni tiempos inventados**:

```text
[T0 ms] WiFi PRUEBA: desconexión solicitada; reintentos suspendidos; esperando confirmación; sensores continúan
[T1 ms] WiFi PRUEBA: desconexión confirmada por SDK; desde solicitud=DELTA ms; reintentos suspendidos
[T2 ms] DHT22 OK: T=VALOR C; HR=VALOR %
[T3 ms] WiFi PRUEBA: recuperación solicitada; reintentos normales reactivados
[T4 ms] WiFi PRUEBA: IP recuperada; desde solicitud=TIEMPO_WIFI ms; origen=GOT_IP SDK
[T5 ms] WiFi PRUEBA: MQTT recuperado; desde solicitud=TIEMPO_TOTAL ms; desde IP=TIEMPO_MQTT ms
```

`TIEMPO_WIFI = T4 − T3` y `TIEMPO_TOTAL = T5 − T3` deben ser **menores de
30000 ms** con AP, DHCP y broker disponibles durante toda la recuperación.
`TIEMPO_MQTT = T5 − T4` separa la recuperación del broker tras obtener IP.
La pausa voluntaria `T3 − T1` no forma parte del tiempo de recuperación.
Las restas unsigned conservan la medición al desbordar `millis()`. Los tiempos
corresponden a observaciones del firmware; si se usa `WL_CONNECTED`, incluyen
el tiempo hasta el siguiente turno de la tarea. Guardar el log completo y
repetir varios ciclos; comprobar mensajes DHT22 antes, durante y después.
Si transcurren 30 s sin IP o MQTT, ese objetivo no se cumple para el ciclo;
no inferir éxito por haber enviado el comando.

Esta prueba verifica una desconexión voluntaria real de la estación y su
recuperación con los servicios disponibles. No mide detección de pérdida de
balizas, recuperación tras apagar el AP ni disponibilidad real tras un reinicio
del router. La prueba independiente de reinicio del broker sigue utilizando el
procedimiento normal de CA-02.

Los comandos y métricas ya están retirados mediante `WIFI_TEMPORARY_TEST=0` en
`main.cpp`; también puede definirse `-D WIFI_TEMPORARY_TEST=0` al compilar posteriormente.
Esto excluye también buffer y estado temporal; conserva la estrategia normal.
No se modificó `platformio.ini`. La evidencia física de esta prueba está
enlazada al comienzo del documento. No se ejecutaron tests, build ni upload
durante la preparación de la configuración final.

## Configuración privada

Desde `firmware/`, crear la copia local solo si no existe:

```powershell
if (-not (Test-Path include/config.h)) {
    Copy-Item include/config.example.h include/config.h
}
```

Completar `include/config.h`; si ya existía, incorporar los parámetros nuevos
desde la plantilla sin sobrescribir credenciales. No se creó ni modificó ese
archivo privado; en la revisión del SSID se examinó su definición sin exponer
contraseñas. Sin `config.h` el firmware no
compila deliberadamente: no hay fallback con credenciales ficticias en producción.
`.gitignore` ya excluye `firmware/include/config.h`; no añadirlo a Git ni incluir
credenciales en capturas, comandos compartidos o registros de QA.

| Parámetro en `config.h` | Dato necesario |
|---|---|
| `wifi_ssid`, `wifi_password` | SSID y contraseña de la red **2,4 GHz** accesible al ESP32-WROOM-32 |
| `mqtt_host`, `mqtt_port` | IPv4 LAN o nombre resoluble del equipo Mosquitto, puerto TCP; plantilla 1883 |
| `mqtt_user`, `mqtt_password` | Usuario y contraseña aceptados por Mosquitto; usuario vacío solo si el broker permite conexión anónima expresamente |
| `mqtt_client_id` | Identificador único por dispositivo para evitar expulsiones entre clientes |
| `wifi_retry_interval_ms` | 5000 por defecto; positivo y menor de 30000 |
| `mqtt_retry_interval_ms` | 2000 por defecto; positivo y menor de 30000 |
| `mqtt_tcp_timeout_ms` | 1000 por defecto; de 1 a 5000 ms |
| `mqtt_socket_timeout_s` | 1 por defecto; de 1 a 5 s |
| `mqtt_keepalive_s` | 5 por defecto; de 1 a 10 s |

No usar `localhost` como host del broker: desde el ESP32 sería el propio ESP32.
Mosquitto debe escuchar en la interfaz LAN, aceptar las credenciales y tener
el puerto accesible en el firewall; evitar aislamiento de clientes en el AP.
Esta rama no incluye una configuración operativa de Mosquitto: `infra/mosquitto/`
solo contiene `.gitkeep`. Configurar el broker en el equipo de laboratorio sin
versionar su archivo de contraseñas. La plantilla no contiene secretos reales.

## Pruebas de software preparadas, no ejecutadas

`tests/connection_retry_test.cpp` usa el mismo planificador que `main.cpp` y
cubre inicio inmediato, orden WiFi→MQTT, intervalos inclusivos, ausencia de
intentos duplicados, eventos sin mensajes repetidos, caída WiFi con estado de
socket MQTT obsoleto, caída independiente del broker, recuperación, caídas
prolongadas, parametrización y desbordamiento de `millis()`.
Incluye casos nuevos de asociación pendiente más allá de 5000 ms, bloqueo de
MQTT hasta obtener IP, recuperación tras terminar el intento y el mismo control
durante el desbordamiento del reloj. Estos casos están preparados, no ejecutados.

Estas pruebas no conectan servicios ni acreditan la concurrencia de FreeRTOS,
los timeouts del SDK o el plazo físico de 30 s. Se conserva sin cambios la suite
DHT22 para comprobar regresiones cuando se autorice ejecutarla. Instrucciones
para una futura ejecución desde MSYS2 UCRT64 con g++ ya instalado:

```bash
cd /d/telematicos/monitoreo-IoT-Maicol
mkdir -p /tmp/monitoreo-t16
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/connection_retry_test.cpp -o /tmp/monitoreo-t16/connection_retry_test.exe
/tmp/monitoreo-t16/connection_retry_test.exe
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/dht_validation_test.cpp -o /tmp/monitoreo-t16/dht_validation_test.exe
/tmp/monitoreo-t16/dht_validation_test.exe
```

## Validación física de CA-01 a CA-04

Preparar WiFi de 2,4 GHz y Mosquitto con conexión comprobable desde otro equipo.
Después de completar `config.h`, ejecutar manualmente cuando se autorice:

```powershell
Set-Location D:\telematicos\monitoreo-IoT-Maicol\firmware
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev --baud 115200
```

Cerrar el monitor antes de cargar; con varios puertos usar el COM real mediante
`--upload-port COMx` y `--port COMx`. Abrir el monitor y pulsar EN/RESET. Los
mensajes tienen tiempo desde el arranque en ms; los siguientes son formatos,
no resultados obtenidos:

```text
Firmware ESP32 iniciado
[TIEMPO ms] WiFi: iniciando conexión
[TIEMPO ms] WiFi: conectado; IP=DIRECCION_REAL
[TIEMPO ms] MQTT: iniciando conexión
[TIEMPO ms] MQTT: conectado al broker
[TIEMPO ms] DHT22 OK: T=VALOR C; HR=VALOR %
```

| Criterio | Procedimiento y evidencia requerida |
|---|---|
| CA-01 | Arranque en frío y EN/RESET con servicios disponibles. Guardar el log de conexión WiFi con una IP LAN real y confirmar conexión MQTT. |
| CA-02: pérdida WiFi | Apagar temporalmente el AP o su red de 2,4 GHz. Observar desconexión, cierre MQTT, intentos WiFi y lecturas DHT22. Restaurar AP y DHCP; registrar el instante real en que vuelven a estar disponibles. Medir hasta `WiFi: conectado; IP=...` y hasta `MQTT: conectado al broker`: ambos deben ser menores de 30000 ms. |
| CA-02: reinicio Mosquitto | Mantener WiFi disponible y detener/reiniciar Mosquitto. Observar pérdida MQTT y reintentos sin reiniciar WiFi. Registrar cuándo el listener vuelve a aceptar conexiones válidas y medir hasta `MQTT: conectado al broker`: menos de 30000 ms. Repetir con un corte sin cierre TCP limpio para comprobar keepalive. |
| CA-03 | Revisar localmente que SSID, credenciales, host, puerto, ID y tiempos provienen de `config.h`. Confirmar que el archivo sigue ignorado por Git. No publicar capturas de su contenido. |
| CA-04 | Mantener cada servicio caído al menos 60 s, también desde el arranque. Guardar timestamps DHT22 durante toda la caída y recuperación; deben mantener el intervalo existente, incluyendo sus tiempos de lectura, sin pausas correspondientes a los reintentos. Los mensajes de descarte/error del DHT22 también acreditan ejecución de cada intento. |

Medir desde la **disponibilidad efectiva** del servicio, no desde el clic de
reinicio. Correlacionar el reloj del equipo de prueba con los timestamps del
ESP32 y registrar el desfase; una secuencia de mensajes sin ese instante no
demuestra CA-02. Usar credenciales correctas y un ID único; un rechazo de
autenticación no se corrige reintentando. Conservar logs completos de caída y
recuperación, configuración no sensible, tiempos máximos y resultado por CA.

También probar credenciales erróneas, puerto cerrado y host inaccesible para
comprobar que se registran fallos y el DHT22 sigue funcionando. No se modifica
la validación del sensor para obtener lecturas aceptadas.
