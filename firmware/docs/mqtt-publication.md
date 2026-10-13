# Issue #13 — T-1.7: publicación periódica MQTT

Rama de trabajo: `feat/13-publicacion-mqtt`. Base del futuro PR:
`feature/firmware-esp32`. La implementación inicial pasó las pruebas de software
y compiló en `esp32dev`. El responsable confirmó la validación física de
CA-01 a CA-04 y la restauración de CO2. Las tres capturas existentes y sus
limitaciones se registran en
[validacion-fisica-mqtt.md](../../docs/evidencias/t1-7/validacion-fisica-mqtt.md).
La versión definitiva deja `MQTT_TEMPORARY_TEST=0`; no se ejecutaron nuevos
tests, build ni upload al documentar el ensayo y desactivar la prueba.

## Criterios de aceptación

| Criterio | Comportamiento |
|---|---|
| CA-01 | `MQTT_PUBLISH_INTERVAL_MS=15000UL` inicialmente, configurable entre 10000 y 30000 ms, inclusive. |
| CA-02 | Solo `ambiente/temperatura`, `ambiente/humedad` y `ambiente/co2`, cada uno con su propia selección. |
| CA-03 | Texto numérico con exactamente un decimal y punto: `28.5`, `50.2`, `415.7`; sin JSON, unidades ni salto de línea. Los ejemplos describen el formato, no son lecturas del prototipo. |
| CA-04 | Una variable inválida, descartada, no disponible, antigua o sin muestra nueva omite solamente su topic. Un fallo de envío tampoco cancela los intentos de las demás variables. |

El periodo se configura en `firmware/include/mqtt_publication_config.h`, sin
editar el archivo privado `config.h`. Como alternativa, usar una definición
de compilación `-D MQTT_PUBLISH_INTERVAL_MS=10000` (hasta `30000`) en el entorno
físico. La comprobación `static_assert` rechaza valores fuera del rango.
La configuración requiere recompilación; no se añade un comando de ajuste en
tiempo de ejecución. `WIFI_TEMPORARY_DIAGNOSTIC` y `WIFI_TEMPORARY_TEST` conservan
su valor predeterminado `0`.

## Adquisición, validez y concurrencia

No se añaden sensores ni lecturas ADC/DHT. `loop()` sigue adquiriendo DHT22 cada
2000 ms después de cada intento y MQ-135 en sus turnos de 2000 ms. Alimenta un
snapshot con el resultado del último intento de cada variable, su tiempo y su
revisión. Una lectura fallida reemplaza el valor anterior con un estado de
error: no se vuelve a publicar un dato viejo para cubrir una falla.

Estados de adquisición:

- **Válida:** valor finito y dentro de las validaciones existentes.
- **Inválida:** NaN, infinito o fallo numérico del estimador.
- **Descartada:** valor DHT fuera de rango o lectura MQ-135 rechazada por ADC.
- **No disponible:** sin intento, calentamiento, referencia/conversión pendiente,
  emisión deshabilitada o procedencia simulada del CO2.

Al seleccionar el ciclo, además se omiten muestras de más de **5000 ms** o con
una revisión ya consumida. La edad máxima es independiente del periodo de
publicación: la adquisición sigue cada 2 s. Valores numéricos iguales en dos
adquisiciones distintas sí pueden publicarse; el control usa revisiones, no
compara números. La resta unsigned mantiene los intervalos durante el
desbordamiento de `millis()`.

Temperatura y humedad usan los mismos límites inclusivos del DHT22: 5–35 °C y
10–90 % HR. Su selección MQTT es independiente para cumplir CA-04. La
validación original de la pareja, sus logs y el snapshot ambiental que consume
la referencia MQ-135 permanecen intactos. Si la pareja DHT no es válida mientras
se captura la referencia, el MQ-135 conserva su bloqueo; MQTT no lo elude.

Se usa un `portMUX_TYPE` con `portENTER_CRITICAL`/`portEXIT_CRITICAL` para
actualizar o copiar el snapshot entre los dos cores. Dentro de la sección
crítica solo se actualizan/copían campos y se clasifican valores; no hay
Serial, operaciones de red, esperas ni acceso a sensores. La tarea de red usa
una copia coherente para seleccionar el ciclo y enviar después de liberar la
protección. Los objetos DHT, Controller MQ-135 y PubSubClient conservan sus
propietarios originales. No se crea otra tarea ni se cambia el planificador de
reconexión.

MQTT no publica CO2 antes de los 30 s, una referencia física válida de 3
muestras y la habilitación existente de emisión. El controlador sigue
validando la mediana de 11 lecturas y el rango ADC **100–950 mV**, además de la
procedencia, conversión, circuito y condiciones matemáticas. Solo una
`Observation` física con estimación válida pasa a publicación. La demo
simulada siempre se omite en `ambiente/co2`. Invalidar la referencia,
deshabilitar ppm o reconfigurar el MQ-135 invalida también su copia compartida;
no espera al siguiente turno ADC para borrar el valor anterior.

El payload CO2 es una **estimación provisional**, con referencia de 400 ppm
asumidos: no acredita una medición de CO2 calibrada. Esta condición queda en
documentación y monitor serie, manteniendo el payload estrictamente numérico.

## Tiempo de publicación y fallos de red

La primera ventana se abre un intervalo completo después de que la tarea
observe MQTT conectado. Tras una desconexión WiFi/broker, el siguiente ciclo
espera de nuevo un intervalo completo desde la reconexión observada. Los
ciclos perdidos se descartan, sin cola de backlog ni ráfagas de recuperación.
Un retraso de la tarea conectado produce como máximo un ciclo y el plazo se
cuenta de nuevo desde ese ciclo efectivo.

Las tres publicaciones de un ciclo se intentan consecutivamente con QoS 0 y
`retained=false`. Las muestras se consumen al seleccionar el ciclo, también
si el envío falla; se usarán adquisiciones nuevas en el siguiente. Los
timeouts/keepalive y reintentos existentes siguen en la tarea de red. `loop()`
no espera a que conecte el broker ni publica directamente.

El monitor distingue omisiones y resultado de cada intento, sin credenciales:
`MQTT omitido: <topic>; <motivo>` o
`MQTT publicacion: <topic>; payload=<numero>; enviado (QoS 0)`.
`publish()` exitoso indica aceptación del envío por el cliente, **no** un
acuse del broker ni confirmación de recepción; esto se verifica con el
suscriptor. Los tres topics tampoco forman una transacción atómica.

## Prueba temporal controlada CA-04 — ejecutada y desactivada

Resultado físico comunicado y respaldado por la captura de omisión/restauración
en el registro enlazado arriba. El procedimiento siguiente permite reproducir
la prueba; no se mantiene habilitada en la configuración definitiva.

`MQTT_TEMPORARY_TEST` está definido en
`firmware/include/mqtt_publication_config.h`, con valor predeterminado **0**.
Al habilitarlo en **1** se incorporan exclusivamente estos comandos del monitor:

```text
mqtt prueba co2 omitir
mqtt prueba co2 restaurar
```

Se reutiliza el lector serie incremental existente de `loop()`, con su límite
de 32 caracteres por vuelta y buffer de 64. Enviar una línea terminada en LF
o CRLF. No se crea otra tarea ni otro lector. Mantener
`WIFI_TEMPORARY_TEST=0` y `WIFI_TEMPORARY_DIAGNOSTIC=0`; una comprobación de
compilación impide combinar los dos lectores de pruebas MQTT/WiFi.

**Omitir:** marca inmediatamente la copia compartida de CO2 como no disponible
y borra su valor anterior bajo el mismo `portMUX`. Mientras está activa la
omisión, cada adquisición posterior mantiene esa copia no publicable. El
MQ-135 sigue leyendo ADC, calculando ppm provisionales y mostrando resultados
físicos por Serial. No se cambia su referencia, emisión, rango ADC ni modelo.
Temperatura/humedad y WiFi/MQTT continúan por sus flujos normales.

**Restaurar:** vuelve a invalidar la copia y desactiva la inhibición. No recupera
el valor anterior ni lee sensores desde el comando: solo una adquisición
física nueva, posterior a la orden y válida según los bloqueos normales, podrá
habilitar CO2 para el siguiente ciclo programado. No modifica el periodo de
15000 ms, los topics, el formato ni el planificador de conexión/publicación.

Mensajes de confirmación:

```text
MQTT PRUEBA: omision CO2 activada; sensores continúan
MQTT PRUEBA: CO2 restaurado; esperando nueva muestra fisica valida
```

Los mensajes usan la cola serie acotada existente. Un ciclo que ya hubiese
copiado sus muestras antes de aceptar la orden puede terminar su envío; evaluar
los ciclos seleccionados después de la confirmación. La inhibición se pierde
al reiniciar: no se guarda estado de prueba ni credenciales.

### Procedimiento físico y evidencia CA-04

1. Cambiar temporalmente el valor público a `MQTT_TEMPORARY_TEST=1`, o añadir
   `-D MQTT_TEMPORARY_TEST=1` a los flags de compilación del entorno **esp32dev**.
   Mantener `MQTT_PUBLISH_INTERVAL_MS=15000UL` y los dos indicadores WiFi en 0.
   Compilar/cargar posteriormente cuando se autorice; estas operaciones no se
   ejecutaron al preparar este cambio. No editar `config.h` ni el broker.
2. Usar el suscriptor de los tres topics indicado abajo, con autenticación
   privada fuera del repositorio. Esperar calentamiento, referencia automática
   válida y al menos un ciclo con los tres topics recibidos.
3. Enviar `mqtt prueba co2 omitir` con terminación de línea y guardar la
   confirmación `MQTT PRUEBA`. Observar al menos **dos ciclos completos de 15 s**
   posteriores: deben recibirse `ambiente/temperatura` y `ambiente/humedad`, y
   omitirse `ambiente/co2`. Correlacionar con
   `MQTT omitido: ambiente/co2; no disponible` y comprobar que el monitor sigue
   mostrando lecturas físicas DHT22, ADC MQ-135 y ppm provisionales.
4. Enviar `mqtt prueba co2 restaurar`. Guardar la confirmación y comprobar una
   **nueva** muestra física válida posterior a la orden en el monitor. CO2 debe
   volver en una ventana periódica posterior, sin mensaje inmediato ni ráfaga;
   temperatura/humedad mantienen su cadencia. Un valor numérico igual al previo
   es admisible si procede de otra adquisición. Si ADC/referencia no son
   válidos, CO2 debe seguir omitido: la restauración no fuerza un valor.
5. Guardar recepción y monitor saneado en `docs/evidencias/t1-7/`, junto con
   capturas de omisión y restauración, indicando tiempos
   de las órdenes y de los ciclos. Registrar el resultado solo después del
   ensayo. Esta prueba demuestra omisión individual de publicabilidad, no una
   falla física del ADC ni una calibración del MQ-135.
6. Antes del PR, devolver `MQTT_TEMPORARY_TEST` a **0** y retirar cualquier
   `-D MQTT_TEMPORARY_TEST=1` usado localmente. Con valor 0 se excluyen estado,
   comandos y mensajes `MQTT PRUEBA`; permanece el flujo normal de publicación.
   Revalidar después de recompilar/cargar la versión definitiva.

## Pruebas de software de la implementación inicial

La suite de publicación inicial pasó 85 comprobaciones, sin fallos. También
pasaron las regresiones disponibles y la compilación física `esp32dev` antes
de añadir el interruptor temporal. Estos resultados no validan este nuevo
cambio ni sustituyen el ensayo CA-04.

`firmware/tests/mqtt_publication_test.cpp` cubre intervalo configurable y
fronteras, primer ciclo, atrasos, rollover, formato/redondeo, topics exactos,
omisiones individuales, ausencia de datos, edad máxima, revisiones consumidas,
reconexión, fallo individual de envío y bloqueos CO2. Usa el mismo módulo de
selección/transporte que producción y el Controller MQ-135 con entradas
explícitamente sintéticas. No ejercita concurrencia real, WiFi, Mosquitto ni
hardware. Se conservan las suites existentes DHT22, conexión y MQ-135.

Cuando se autorice, desde MSYS2 UCRT64 con `g++` instalado, ejecutar desde la
raíz del repositorio:

```bash
mkdir -p /tmp/monitoreo-t17
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/mqtt_publication_test.cpp -o /tmp/monitoreo-t17/mqtt_publication_test.exe
/tmp/monitoreo-t17/mqtt_publication_test.exe
```

Repetir posteriormente las suites existentes y compilar `esp32dev`; no
interpretar esta guía como evidencia de que ya se ejecutaron.

## Procedimiento de validación física con Mosquitto

1. Mantener el montaje físico verificado de Issue #11 y los parámetros locales
   privados de Issue #12. Usar el entorno **`esp32dev`**, con el periodo público
   inicial de 15000 ms. Build/upload se realizarán cuando se autoricen.
2. En el equipo suscriptor, guardar autenticación y host/puerto en el archivo
   privado predeterminado de Mosquitto, fuera del repositorio. En Windows:
   `%USERPROFILE%\mosquitto_sub`; en sistemas POSIX:
   `$XDG_CONFIG_HOME/mosquitto_sub` o `$HOME/.config/mosquitto_sub`. No incluir
   contraseñas en comandos, capturas ni evidencias. El broker debe permitir al
   ESP32 publicar y al cliente de validación suscribirse a los tres topics.
3. Desde la raíz del repositorio, abrir el suscriptor **antes de reiniciar**
   el ESP32. En PowerShell, para guardar las llegadas con tiempo y topic:

   ```powershell
   mosquitto_sub -V mqttv311 -q 0 -R -t 'ambiente/temperatura' -t 'ambiente/humedad' -t 'ambiente/co2' -F '%I %t %p' | Tee-Object -FilePath 'docs/evidencias/t1-7/recepcion-mqtt-local.txt'
   ```

   `-R` omite mensajes retained anteriores y `-F` añade metadatos solo en la
   salida del suscriptor; el payload enviado por el ESP32 sigue siendo el
   número. No usar `-c`, que habilita sesión persistente. Detener con Ctrl+C
   después de al menos cinco ciclos con las tres variables disponibles.
4. **CA-01:** comparar ciclos consecutivos del mismo topic y los tiempos de
   ciclo del monitor serie: aproximadamente 15 s en condiciones estables.
   Registrar retrasos y tolerancia observada, sin prometer tiempo real estricto
   ni comparar mensajes de distintos topics dentro del mismo ciclo. Luego,
   si se autoriza, repetir con 10000 y 30000 ms mediante la configuración
   pública y recompilación, y restaurar 15000 ms.
5. **CA-02/CA-03:** comprobar las tres rutas exactas y que todo payload cumpla
   `^-?[0-9]+\.[0-9]$`. No basta con ver el monitor: conservar recepción del
   broker. No exigir valores coincidentes entre sensores ni CO2 fijo en 400.
6. **Calentamiento/referencia:** después del reinicio observar la omisión de
   CO2 durante los primeros 30 s y hasta terminar las tres muestras válidas y
   habilitar emisión automática. DHT22 puede publicar antes. Correlacionar
   recepción CO2 con el monitor físico y conservar la advertencia provisional.
7. **CA-04 sin modificar sensores:** usar el procedimiento temporal
   `mqtt prueba co2 omitir/restaurar` descrito arriba. Para esa prueba no enviar
   comandos de deshabilitación de ppm ni invalidación de referencia, pues se
   quiere conservar la adquisición y el estimador físico funcionando.
   Para probar fallas DHT, apagar antes de cambiar conexiones y provocar solo
   una falla segura de su línea de datos. Tras reiniciar, verificar omisiones
   sin recuperar lecturas previas. Si no hay referencia válida, CO2 también se
   omite: no marcar esa prueba como aislamiento individual de CO2. Los casos
   de temperatura/humedad individual se cubren sintéticamente cuando el
   hardware no permite producir esas fallas por separado; registrar esa
   limitación, sin forzar calor/humedad o tensiones fuera del montaje seguro.
8. **Reconexión:** registrar un corte del broker manteniendo el ESP32
   encendido, y luego un corte/restauración WiFi. Conservar logs serie que
   muestren DHT22 y MQ-135 funcionando durante ambos cortes. Al reconectar,
   observar un intervalo completo antes de nuevas publicaciones y ausencia
   de ráfagas; los siguientes ciclos contienen solo muestras recientes.

Guardar evidencias en la carpeta real y consultar el inventario de
[`docs/evidencias/t1-7/README.md`](../../docs/evidencias/t1-7/README.md). Las tres
capturas actuales requieren revisión/saneamiento de información privada antes
de versionarlas; no se certificaron como libres de credenciales. El log textual
del comando anterior es una salida propuesta para futuras sesiones, no un
archivo existente. No se crean capturas ni resultados PASS ficticios.
`config.h` continúa ignorado; no agregar `firmware/.vscode/`.

## Referencias de las herramientas

- [API de PubSubClient 2.8](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.h)
  y [publicación QoS 0 / retained](https://github.com/knolleary/pubsubclient/blob/v2.8/src/PubSubClient.cpp).
- [Manual oficial de mosquitto_sub](https://mosquitto.org/man/mosquitto_sub-1.html):
  configuración privada de autenticación, múltiples `-t`, `-R` y `-F`.
