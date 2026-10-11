# Validación física — Issue #13 / T-1.7: publicación MQTT

Rama de trabajo: `feat/13-publicacion-mqtt`.
Base del futuro PR: `feature/firmware-esp32`.

## Objetivo y alcance

Documentar la recepción física de las lecturas DHT22 y las estimaciones
provisionales MQ-135 por Mosquitto: periodo de 15 s, tres topics exactos,
payload numérico de un decimal y omisión independiente de CO2.

El responsable comunicó la validación física de CA-01 a CA-04 y la restauración
de CO2. Se inspeccionaron las tres capturas reales de esta carpeta, sin crear
imágenes ni logs adicionales. Este documento distingue lo visible en ellas,
lo comunicado y los datos pendientes. No se realizaron nuevas pruebas, build
ni upload al preparar el registro.

## Configuración y trazabilidad

| Elemento | Información comprobada / alcance |
|---|---|
| Firmware físico | Entorno `esp32dev` en la configuración del repositorio; revisión exacta cargada no registrada en las capturas. |
| Adquisición | Firmware integrado: DHT22 GPIO23 y MQ-135 GPIO34; conservación física confirmada por el responsable, sin monitor serie adjunto. |
| Red | Mosquitto local y suscriptor `mosquitto_sub.exe` visibles; conexión/reconexión mediante la tarea FreeRTOS existente en el código. |
| Puerto del suscriptor | 1883, visible en las capturas. Dirección privada del broker no transcrita aquí. |
| Suscripción | Filtro `ambiente/#`; salida con `-v` en CA02/CA03 y formato `-F '%I %t %p'` en CA01 y CA04. |
| Periodo | Inicial `MQTT_PUBLISH_INTERVAL_MS=15000UL`; rango admitido por código 10000–30000 ms. |
| Prueba CA-04 | Omisión temporal de publicabilidad de CO2; comandos `mqtt prueba co2 omitir` y `mqtt prueba co2 restaurar` informados por el responsable. |
| Fecha y offset visibles | CA01 y CA04 muestran `2026-10-11` y `-0400`; CA02/CA03 no tiene sello temporal de recepción. No se atribuye una zona geográfica al equipo. |
| Identificación del firmware | Commit, hash del binario y hora del upload pendientes; no se deducen del HEAD actual. |
| Versiones de herramientas | Versiones del broker, suscriptor y herramientas usadas en el ensayo no aparecen en las capturas. |

La configuración definitiva deja `MQTT_TEMPORARY_TEST=0`, conserva el periodo
15000 ms, los topics y las validaciones. El indicador habilitado durante la
prueba no constituye una modificación física ni un cambio del estimador.
No se modificaron credenciales ni `firmware/include/config.h`.

## Resultados CA-01 a CA-04

### CA-01 — Periodo de 15 segundos

Evidencia existente:
[CA01_Publicacion_15s_PASS.png](CA01_Publicacion_15s_PASS.png).

Una ventana completa visible en la captura contiene estos ciclos, con los
tres topics en cada sello temporal:

| Hora de recepción visible (2026-10-11, UTC−04:00) | Diferencia respecto al ciclo anterior |
|---|---|
| 11:27:23 | Inicio de la ventana seleccionada |
| 11:27:38 | 15 s |
| 11:27:53 | 15 s |
| 11:28:08 | 15 s |

La captura continúa con ciclos separados por 15 s hasta 11:29:23.
Resultado: **PASS para el periodo físico de 15000 ms en la ventana visible**.
Los sellos del suscriptor tienen resolución de segundos; no acreditan ausencia
de jitter subsegundo ni tiempo real estricto.

El responsable informó otra secuencia: **11:25:38, 11:25:53 y 11:26:08**, también
con diferencias de 15 s. Esos sellos no están en la captura CA01 existente;
quedan registrados como información comunicada, sin atribuirlos a esa imagen.
No hay evidencia física adjunta de los extremos 10000 y 30000 ms; el rango está
implementado y se verificó en las pruebas de software iniciales, según el
registro de trabajo previo.

### CA-02 — Tres topics exactos

Evidencia existente:
[CA02_CA03_Topics_Payloads_PASS.png](CA02_CA03_Topics_Payloads_PASS.png).

El suscriptor muestra recepción de:

- `ambiente/temperatura`
- `ambiente/humedad`
- `ambiente/co2`

También aparecen los tres en CA01. Resultado: **PASS**. La recepción observada
acredita entrega al suscriptor en esos instantes; no se infiere garantía general
de entrega a partir del retorno de `publish()`.

### CA-03 — Payload numérico con exactamente un decimal

La misma captura CA02/CA03 muestra, entre otros valores reales recibidos:

| Topic | Ejemplo visible |
|---|---|
| `ambiente/temperatura` | `30.5` |
| `ambiente/humedad` | `47.9` |
| `ambiente/co2` | `372.2` |

Los payloads visibles son números con punto y un decimal, sin JSON ni unidades.
El topic y la fecha, cuando aparecen, son metadatos añadidos por el suscriptor,
no parte del payload. Resultado: **PASS en los mensajes visibles**. No existe
un registro textual completo para comprobar automáticamente toda la sesión.

### CA-04 — Omisión independiente de CO2 y restauración

Evidencia existente:
[CA04_Omision_Independiente_PASS.png](CA04_Omision_Independiente_PASS.png).

La captura muestra ciclos con temperatura y humedad, sin `ambiente/co2`,
desde **12:25:13** y en los ciclos siguientes de 15 s. En **12:28:13** siguen
apareciendo solo temperatura (`33.0`) y humedad (`48.6`). En el último ciclo,
**12:28:28**, aparecen temperatura (`33.0`), humedad (`56.6`) y CO2 (`336.4`).
Los sellos visibles llevan fecha `2026-10-11` y offset `-0400`.

Resultado: **PASS para la omisión individual y regreso de CO2 observables**.
Temperatura/humedad siguen recibiéndose mientras CO2 está omitido.
El responsable confirma que el regreso ocurrió después del comando temporal
de restauración y que las lecturas físicas DHT22/MQ-135 se conservaron.

Las órdenes, sus mensajes `MQTT PRUEBA` y la muestra ADC posterior a restaurar
no aparecen en esta captura del suscriptor. Sus horas exactas y la correlación
directa con la nueva muestra física quedan pendientes de un monitor serie.
El código restaura invalidando la copia previa y solo vuelve a habilitar CO2
al adquirir una nueva observación física válida; esa regla se revisó en el
firmware, pero la captura por sí sola no identifica cada adquisición.

La prueba inhibe únicamente la publicabilidad: no demuestra una falla física
del ADC ni ensaya por separado temperatura inválida y humedad válida o viceversa.

## Privacidad de capturas y registros

Se revisó visualmente el contenido de las tres imágenes completas. No se
observan contraseñas no vacías ni un comando de suscripción con contraseña.
No obstante:

- Las tres muestran la dirección privada del broker y datos del equipo/usuario
  local en la terminal.
- CA02/CA03 muestra un valor de `MQTT_USER` y una línea `MQTT_PASS` vacía en el
  editor visible sobre la terminal.
- No hay logs textuales adicionales en esta carpeta que puedan revisarse.

Por tanto, **no se puede confirmar ausencia de credenciales o información
privada**. Se conservaron las imágenes originales, sin reproducir esos valores
en este documento. Revisar y sanear las áreas indicadas antes de agregarlas al
PR, conservando los sellos, topics y payloads que respaldan los resultados.

`config.h` permanece ignorado por `.gitignore` y fuera de los archivos
versionados; su contenido no se incluyó en el registro ni se modificó.

## Limitaciones y pendientes antes de la entrega

- CO2 es una **estimación provisional**, con referencia de desarrollo; no una
  medición calibrada ni evidencia de calibración metrológica HU-08.
- Faltan commit/revisión del firmware ensayado, versiones de herramientas y
  hora del reinicio/upload. No se completan con datos del equipo actual.
- Faltan log serie y registro textual de recepción para trazar órdenes,
  calentamiento, referencia y muestra nueva después de restauración.
- No se adjunta evidencia de cortes WiFi/broker, recuperación sin ráfagas ni
  ensayos físicos con intervalos de 10000/30000 ms. Estas imágenes no acreditan
  esos escenarios adicionales.
- Las capturas pendientes de saneamiento impiden certificar la revisión de
  privacidad como aprobada.
- La compilación y pruebas previas corresponden a la implementación inicial;
  falta verificar después la versión definitiva con el interruptor en 0.
  No se ejecutan ahora tests, build ni upload por instrucción expresa.

## Archivos realmente disponibles

- [README.md](README.md)
- [CA01_Publicacion_15s_PASS.png](CA01_Publicacion_15s_PASS.png)
- [CA02_CA03_Topics_Payloads_PASS.png](CA02_CA03_Topics_Payloads_PASS.png)
- [CA04_Omision_Independiente_PASS.png](CA04_Omision_Independiente_PASS.png)

Este registro se creó a partir de esas fuentes y de los resultados comunicados.
No se inventaron capturas, fechas, commits ni logs.
