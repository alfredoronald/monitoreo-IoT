# Resultados ejecutados — T-8.1

El registro inicial del 26/10 se conserva a continuacion. La actualizacion
del 27/10 al final documenta el modo de Rs estimada; sus parametros actuales
sustituyen el estado anterior de carga sin configurar, sin acreditar RL real.

Fecha: 26/10/2026 (America/La_Paz). HEAD base `fb7fbcd`; codigo T-8.1
sin commit en el arbol de trabajo. Todos los resultados siguientes son de
escritorio/compilacion, **no ensayos fisicos**.

## Escritorio

Compilador instalado: MSYS2 UCRT64 GCC **16.2.0**. Opciones:
`-std=c++11 -Wall -Wextra -Werror -pedantic`.

| Suite | Comprobaciones/casos reportados | Fallos | Compilacion / ejecucion |
|---|---:|---:|---|
| connection_retry_test | 42 | 0 | 0 / 0 |
| dht_validation_test | 12 | 0 | 0 / 0 |
| mq135_acquisition_test | 97 | 0 | 0 / 0 |
| mq135_adc_filter_test | 89 | 0 | 0 / 0 |
| mq135_calibration_test (nueva) | 103 | 0 | 0 / 0 |
| mq135_experimental_test | 131 | 0 | 0 / 0 |
| mq135_historical_demo_test | No imprime conteo | 0 | 0 / 0 |
| mq135_simulated_demo_test | 64 | 0 | 0 / 0 |
| mqtt_publication_test | 85 | 0 | 0 / 0 |

**9 suites aprobadas.** Las ocho que imprimen conteo suman 623
comprobaciones/casos; la retrospectiva adicional termina con cero fallos.
No se modificaron ni eliminaron tests anteriores.

Evidencia: [resumen de las nueve suites](pruebas-escritorio.txt) y
[salida completa de la suite nueva](calibracion-sintetica.txt).
Los logs completos restantes se generaron en `firmware/.pio/desktop-tests`.

La suite nueva verifica formula/unidades, carga/topologia/revision/evidencia,
NaN/infinitos, overflow/underflow, configuracion por defecto bloqueada,
acumuladores de 1/2/50/512 muestras, media sin suma que desborde double,
reinicio y rechazo sin contaminar media. Tambien verifica calentamiento,
mediana de once como una muestra, ADC/voltajes invalidos, exclusividad del
turno, rollover, rechazo de demo, bloqueo de referencia/ppm durante captura,
recuperacion con tres muestras nuevas, parser CR/LF/CRLF, comandos existentes,
lineas largas/NUL, supresion CO2, ACK, invalidacion de ciclos antiguos,
continuidad DHT22, cadencia y reconexion sin backlog.

Estas pruebas ejercitan las cabeceras usadas por el firmware. No ejecutan
FreeRTOS, Serial real, eFuse, sensores ni sockets; la concurrencia fisica y
los tiempos reales de reconexion requieren ESP32/broker.

## PlatformIO

Herramientas ya instaladas:

- PlatformIO Core **6.2.0**, plataforma Espressif32 **7.1.3**.
- Arduino-ESP32 **2.0.17** (paquete `4.20017.260907+sha.dcc1105b`).
- Xtensa GCC **8.4.0+2021r2-patch5**.
- DHT sensor library **1.4.7**, Adafruit Unified Sensor **1.1.15**,
  PubSubClient **2.8.0**, WiFi **2.0.0**.

El primer intento normal `platformio run -d firmware -e esp32dev` termino
con **exit 1**: este checkout no contiene `firmware/include/config.h` y faltan
los simbolos de configuracion WiFi/MQTT, por ejemplo `wifi_retry_interval_ms`
y `mqtt_host`. No se invento una configuracion privada para resolverlo.

Se compilaron los dos entornos con el helper `compile-with-example.ps1`,
que preincluye **solo la plantilla existente** para los fuentes del proyecto,
sin crear `config.h` ni modificar `platformio.ini`:

| Entorno | Resultado | Duracion final | RAM | Flash |
|---|---|---:|---:|---:|
| esp32dev | SUCCESS | 12,829 s | 49 084 / 327 680 bytes | 781 977 / 1 310 720 bytes |
| esp32dev-mq135-demo | SUCCESS | 5,596 s | 48 772 / 327 680 bytes | 767 485 / 1 310 720 bytes |

Codigo de salida global: **0**. Log final completo:
[compilacion con plantilla](compilacion-ejemplo.txt).

Tambien se compilo `esp32dev` con ambos flags de pruebas temporales:
`WIFI_TEMPORARY_TEST=1` y `MQTT_TEMPORARY_TEST=1`, mediante
`-AdditionalSourceFlags '-D WIFI_TEMPORARY_TEST=1 -D MQTT_TEMPORARY_TEST=1'`.
Resultado **SUCCESS**, exit **0**, **42,690 s**; RAM **49 108 bytes**,
Flash **784 741 bytes**. Esto comprueba compilacion de la ruta de cola WiFi
y parser compartido, no su funcionamiento fisico.
[Log de compilacion con flags temporales](compilacion-comandos-temporales.txt).

Se reutilizaron los paquetes y bibliotecas instalados; para el entorno demo
se copio la cache local de bibliotecas del entorno fisico dentro de `.pio`.
No se instalaron dependencias nuevas. PlatformIO requirio autorizacion para
escribir sus archivos de bloqueo/cache fuera del repositorio.

Los binarios quedan en `.pio/example-build`, separados de los normales y
**no se cargaron a ninguna placa**. El ultimo build de `esp32dev` en esa
carpeta tiene los flags temporales de prueba: no usarlo como firmware de
operacion. Para operar, compilar de nuevo con configuracion real y flags
temporales desactivados. La plantilla no acredita WiFi/MQTT reales.

## Limites y verificacion final

- Sigue faltando `config.h`; no se comprobo compilacion con configuracion
  privada ni conexion con una red/broker real.
- RL efectiva sigue sin configurar/verificar. El valor predeterminado bloquea
  Rs fisica; no hay medicion real ni Ro calculado en esta evidencia.
- Estan pendientes las verificaciones fisicas y la captura exterior indicadas
  en [README](README.md). La aceptacion de estabilidad pertenece a #141.
- `git diff --check` sin incidencias; sin cambios en backend, frontend,
  base de datos, tests existentes ni archivos de credenciales.
- No se ejecuto `git add`, commit, push, cambio de rama ni creacion de PR.

## Actualizacion 27/10/2026 — captura experimental de Rs estimada

Se adopta RL interna **nominal estimada, no medida**, de 10000 ohm y carga
efectiva calculada/redondeada de 6666,67 ohm. La opcion
`MQ135_EXPERIMENTAL_RS=1` habilita la estimacion durante captura;
`MQ135_RL_TOPOLOGY_VERIFIED=0` permanece sin cambios.
La topologia de AO sigue supuesta. Formula, procedencia y limitaciones:
[rs-estimada-experimental.md](rs-estimada-experimental.md).

Se ejecutaron las nueve suites con GCC y
`-std=c++11 -Wall -Wextra -Werror -pedantic`. Todas compilaron y terminaron
con codigo **0**. Las ocho suites que imprimen conteo suman **679**
comprobaciones/casos; la retrospectiva adicional tambien paso.
La suite de calibracion ampliada reporta **159 comprobaciones, 0 fallos**.

Tambien se compilo y ejecuto esa suite con
`-D MQ135_EXPERIMENTAL_RS=0`: **159 comprobaciones, 0 fallos**, codigo **0**.
Esto verifica la desactivacion de los predeterminados nominales y el bloqueo
estricto sin carga verificada, ademas de los fixtures de ambos modos.

Evidencia de esta ejecucion, separada de los logs iniciales:

- [Resumen de nueve suites](pruebas-escritorio-experimental.txt).
- [Calibracion con opcion experimental habilitada](calibracion-estimada-sintetica.txt).
- [Calibracion con opcion experimental deshabilitada](calibracion-estricta-sintetica.txt).

Tambien se compilaron `esp32dev` y `esp32dev-mq135-demo` con la plantilla
existente mediante `compile-with-example.ps1`: ambos **SUCCESS**, salida **0**.
El entorno fisico uso 49092 bytes de RAM y 782409 bytes de flash; la demo,
48780 y 767785 bytes, respectivamente. Duraciones: 41,579 s y 24,491 s.
[Comando y extracto de resultados](compilacion-estimada-resumen.txt).
El intento inicial restringido fallo al escribir `platforms.lock`; se repitio
con permiso para la cache de PlatformIO. Los binarios de ejemplo permanecen
en `.pio/example-build`, sin upload y sin acreditar conexiones reales.

Se verifican etiquetas estimadas en Serial y en el resumen detenido, promedio
incremental y contador con 512 muestras, rechazo sin contaminar media,
revision y referencia documental obligatorias, API estricta cerrada a
estimaciones y rechazo de banderas experimental/verificada incompatibles.
Los controles de calentamiento, ADC/voltajes, conversion, cadencia,
exclusividad, rollover y recuperacion se ejercitan en ambos modos.
Las suites de reconexion, parser y MQTT mantienen sus comprobaciones de
supresion CO2/ACK, continuidad DHT22 y ausencia de backlog.

Son pruebas sinteticas de escritorio: no ejecutan el ADC, Serial, FreeRTOS,
WiFi ni broker reales. No se calcula Ro definitivo, no se declaran ppm
calibradas y no se acredita una medicion fisica de RL ni de Rs.
No se modificaron backend, frontend o credenciales. No se cargo firmware
ni se realizaron operaciones Git, commit, push o PR en esta actualizacion.
