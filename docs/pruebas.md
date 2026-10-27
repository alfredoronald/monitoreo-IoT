# Plan de pruebas de operación y comunicación (T-9.1)

**Sprint:** 3  
**Fecha de la tarea:** 25/10/2026  
**Issue padre:** [#139](https://github.com/alfredoronald/monitoreo-IoT/issues/139)

Este documento define cómo medir la persistencia, recuperación, latencia y
pérdida de mensajes del sistema completo: ESP32 → Mosquitto → backend →
PostgreSQL → dashboard. Los resultados se deben completar con datos reales; las
plantillas de este documento no constituyen evidencia de una ejecución.

## Criterios y límites

Se consideran seis métricas. La recuperación se desglosa en tres casos porque
cada componente se interrumpe y se mide por separado. QoS 0 y QoS 1 se ejecutan
como dos niveles de una misma métrica de transporte.

| ID | Métrica | Duración o muestra | Medición | Límite de aceptación |
|---|---|---:|---|---|
| M-01 | Lecturas guardadas en 24 h | 24 h | `guardadas / esperadas × 100` | **≥ 98 %** |
| M-02 | Recuperación tras cortar WiFi | 1 interrupción | `t_recuperación − t_corte` | **< 60 s** |
| M-03 | Recuperación tras reiniciar Mosquitto | 1 reinicio | `t_broker_disponible − t_reinicio` | **< 60 s** |
| M-04 | Recuperación tras reiniciar backend | 1 reinicio | `t_backend_listo − t_reinicio` | **< 60 s** |
| M-05 | Publicación hasta dashboard | 10 mensajes | promedio de `t_dashboard − t_publicación` | **< 20 s** |
| M-06 | Pérdida de transporte por QoS | 500 mensajes por nivel | `(enviados − recibidos) / enviados × 100`, por QoS 0 y QoS 1 | **Muestra obligatoria: 500 + 500**; la issue no define un porcentaje máximo de pérdida, por lo que se reporta el valor observado sin inventar un umbral |

En M-01, `esperadas` debe calcularse a partir del intervalo de publicación
configurado y del tiempo efectivo de la ventana, no a partir de filas que ya
existan en la base. En M-05 se debe usar el mismo identificador de prueba para
seguir cada mensaje y no confundirlo con una lectura anterior.

## Preparación común

1. Actualizar el código y anotar commit, fecha, zona horaria, versiones de
   Node.js, Mosquitto y PostgreSQL, además de la configuración del intervalo de
   publicación.
2. Iniciar PostgreSQL, Mosquitto, el backend y el dashboard. Confirmar que el
   backend está suscrito a `ambiente/#` y que el dashboard puede consultar
   `GET /api/lecturas/ultima`.
3. Detener publicaciones ajenas a la prueba. No borrar lecturas históricas;
   usar un identificador, rango de valores o límite de `id` exclusivo para cada
   ejecución.
4. Sincronizar el reloj del ESP32, del equipo que ejecuta el backend y del
   navegador. Registrar todos los tiempos en ISO 8601 con zona horaria.
5. Guardar el log MQTT, los `id` o valores publicados y las consultas SQL
   utilizadas. No incluir contraseñas ni archivos `.env` en la evidencia.

El firmware publica normalmente cada 15 s y el dashboard solicita las últimas
lecturas cada 15 s. El dashboard marca una lectura como antigua después de 60 s;
esa marca sirve como señal visual, pero la latencia se calcula con timestamps,
no con el momento de repintado de la pantalla.

## Procedimientos

### M-01 — Lecturas guardadas durante 24 h

**Duración:** 24 horas continuas, con el ESP32 y los servicios funcionando.

1. Registrar `inicio_utc` y `fin_utc` separados exactamente por 24 h.
2. Antes de iniciar, anotar el último `id` de `lecturas` y el contador de
   publicaciones esperado. Con un periodo `P`, calcular
   `esperadas = floor((fin − inicio) / P)` por cada variable publicada.
3. Mantener activos WiFi, broker, backend y PostgreSQL. Registrar cualquier
   reinicio, desconexión o intervalo de mantenimiento; no ocultarlo en el
   conteo.
4. Al terminar, consultar las filas del intervalo y del rango de `id` de la
   prueba. Contar solo lecturas válidas generadas durante esta ejecución.
5. Calcular el porcentaje por variable y el total. El caso pasa si el total y
   cada variable que se haya acordado medir cumplen ≥ 98 %.

Plantilla de resultado:

| Variable | Inicio UTC | Fin UTC | Periodo (s) | Esperadas | Guardadas | Porcentaje | Resultado |
|---|---|---|---:|---:|---:|---:|---|
| temperatura | `AAAA-MM-DDThh:mm:ssZ` | `...` | `15` | `...` | `...` | `... %` | Pendiente |
| humedad | `...` | `...` | `15` | `...` | `...` | `... %` | Pendiente |
| co2 | `...` | `...` | `15` | `...` | `...` | `... %` | Pendiente |
| **Total** | | | | `...` | `...` | `... %` | Pendiente |

### M-02, M-03 y M-04 — Recuperación en menos de un minuto

**Duración:** una interrupción controlada por caso; el cronómetro empieza al
confirmar la interrupción y termina al observar una lectura nueva y válida en la
base. No se debe detener el cronómetro mientras se revisan logs.

Para cada caso:

1. Dejar llegar una lectura normal y anotar su timestamp, `id` y valor de
   control.
2. Registrar `t_0` y provocar solo la interrupción indicada.
3. Registrar el instante en que el componente vuelve a estar disponible.
4. Esperar la primera publicación posterior a la recuperación y confirmar que
   el mensaje aparece en el log MQTT y se guarda en PostgreSQL.
5. Calcular `duración = t_lectura_recuperada − t_0`. Repetir el caso si hubo
   una acción accidental o más de un componente quedó interrumpido.

| Caso | Interrupción | Inicio del cronómetro | Fin del cronómetro |
|---|---|---|---|
| M-02 | Desactivar y volver a activar el WiFi del ESP32 o su punto de acceso | confirmación de pérdida de conexión | primera lectura nueva guardada después de reconectar |
| M-03 | Detener y volver a iniciar Mosquitto | confirmación de parada del broker | broker disponible y primera lectura nueva guardada |
| M-04 | Detener y volver a iniciar el proceso del backend | confirmación de cierre del proceso | HTTP listo, suscripción activa y primera lectura nueva guardada |

Plantilla de resultado:

| ID | Repetición | `t_0` UTC | `t_fin` UTC | Duración (s) | Lectura/id recuperado | Límite | Resultado | Evidencia |
|---|---:|---|---|---:|---|---:|---|---|
| M-02 | 1 | `...` | `...` | `...` | `...` | < 60 | Pendiente | `...` |
| M-03 | 1 | `...` | `...` | `...` | `...` | < 60 | Pendiente | `...` |
| M-04 | 1 | `...` | `...` | `...` | `...` | < 60 | Pendiente | `...` |

### M-05 — Latencia de publicación al dashboard

**Duración:** 10 publicaciones controladas; esperar la recepción de cada una
antes de enviar la siguiente para conservar la correspondencia.

1. Generar diez mensajes con un `test_id` único en un valor válido o en el
   mecanismo de correlación acordado. Registrar `t_publicación` inmediatamente
   antes de `mosquitto_pub` y el resultado del envío.
2. Para cada mensaje, registrar `t_dashboard` cuando el dashboard muestre ese
   mismo `test_id` o valor después de una respuesta HTTP nueva. No usar la hora
   de una lectura antigua conservada en pantalla.
3. Calcular `latencia_i = t_dashboard_i − t_publicación_i` y el promedio de las
   diez latencias. El promedio debe ser menor que 20 s.

Plantilla de resultado:

| # | Test ID | `t_publicación` UTC | `t_dashboard` UTC | Latencia (s) | Observación |
|---:|---|---|---|---:|---|
| 1 | `...` | `...` | `...` | `...` | |
| 2 | `...` | `...` | `...` | `...` | |
| 3 | `...` | `...` | `...` | `...` | |
| 4 | `...` | `...` | `...` | `...` | |
| 5 | `...` | `...` | `...` | `...` | |
| 6 | `...` | `...` | `...` | `...` | |
| 7 | `...` | `...` | `...` | `...` | |
| 8 | `...` | `...` | `...` | `...` | |
| 9 | `...` | `...` | `...` | `...` | |
| 10 | `...` | `...` | `...` | `...` | |
| **Promedio** | | | | **`...` s** | **< 20 s: Pendiente** |

### M-06 — Pérdidas con QoS 0 y QoS 1

**Duración:** dos rondas de 500 mensajes, una con QoS 0 y otra con QoS 1.
Usar topics y rangos de valores distintos por ronda, `retain=false`, y no
mezclar resultados con mensajes de otras pruebas.

1. Reiniciar el contador de la ronda y registrar exactamente 500 publicaciones
   confirmadas por `mosquitto_pub` para el nivel QoS en curso.
2. En el log del backend contar mensajes recibidos únicos por `test_id`; en la
   base contar los guardados correspondientes. Registrar también duplicados,
   porque QoS 1 puede entregar más de una vez.
3. Calcular pérdida de transporte con los recibidos únicos y pérdida de
   persistencia con los guardados. Repetir con el otro QoS.
4. La evidencia debe informar explícitamente los dos conteos y no convertir una
   entrega duplicada de QoS 1 en una pérdida.

Plantilla de resultado:

| QoS | Enviados | Recibidos únicos | Duplicados | Guardados | Pérdida transporte | Pérdida persistencia | Resultado |
|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 500 | `...` | `...` | `...` | `... %` | `... %` | Pendiente |
| 1 | 500 | `...` | `...` | `...` | `... %` | `... %` | Pendiente |

## Registro de ejecución y evidencia

Completar esta ficha junto con las tablas anteriores:

| Campo | Valor |
|---|---|
| Commit probado | `...` |
| Fecha y hora de inicio (UTC) | `...` |
| Equipo / sistema operativo | `...` |
| ESP32 / firmware | `...` |
| Node.js / pnpm | `...` |
| Mosquitto | `...` |
| PostgreSQL | `...` |
| Intervalo de publicación | `...` |
| Dashboard / navegador | `...` |
| Archivos de log, SQL y capturas | `...` |
| Incidencias o interrupciones durante la prueba | `...` |

La evidencia final debe incluir las tablas completadas, los logs o consultas que
permitan reproducir cada conteo y capturas del dashboard solo cuando sean
necesarias. Si una prueba falla o queda incompleta, conservar el resultado
parcial y explicar la causa; no reemplazarlo por un valor estimado.
