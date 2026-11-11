# Recuperación de WiFi, broker y backend — #149

Estado: **tres ensayos físicos aprobados el 29/10/2026; listos para revisión**.

## Resultado físico

Circuito ESP32 en COM5, ubicado en aula real según confirmación del responsable.
Firmware `7a0ac17`, Espressif32 7.1.3, publicación cada 15 s; backend `ab382e3`,
Mosquitto 2.1.2 y PostgreSQL real. Se usó la nueva red de 2,4 GHz configurada
privadamente. No se publicaron datos sintéticos ni se modificaron las lecturas.

| Falla | Interrupción confirmada UTC | Primera fila observada UTC | Segundos | Id | <60 s |
|---|---|---|---:|---|---|
| Broker | 15:25:40.941 | 15:26:00.577 | 19,636 | 47 | PASS |
| Backend | 15:26:02.839 | 15:26:15.635 | 12,796 | 50 | PASS |
| WiFi ESP32 | 15:26:15.854 | 15:26:59.442 | 43,588 | 53 | PASS |

Fecha de todas las horas: 29/10/2026 UTC. Los JSON conservan la fecha completa,
el timestamp SQL y el de observación, que pueden diferir hasta una consulta.

Se automatizó el procedimiento con el ejecutor adjunto: broker y backend se
cerraron como procesos y se confirmó su terminación. Tras 3 segundos se inició
únicamente el componente interrumpido. El firmware y el cliente MQTT realizaron
su reconexión habitual, sin reset del ESP32 ni reinserción manual. Esto no prueba
que exista un supervisor de reinicio automático de procesos.

Para WiFi se compiló `WIFI_TEMPORARY_TEST=1`: el comando serie existente solicita
`esp_wifi_disconnect`, el SDK confirma la desconexión y se suspenden los reintentos.
Después de 3 segundos, `wifi prueba reconectar` reactiva el planificador normal;
el comando no llama a connect/begin ni reinicia los sensores. El cronómetro incluye
esa pausa y llega hasta observar la primera fila nueva en PostgreSQL, no solo MQTT.

Evidencias completas en `fisico-2026-10-29/ensayo-1793287539275/`: tres JSON,
serie JSONL y logs del broker/backend. El registro `fisico-2026-10-29/recuperacion-broker.json`
conserva un intento previo sin corte ejecutado por fallo de detección del proceso;
no se presenta como medición física. `precondiciones.json` es una observación
histórica del 28/10, anterior a la configuración y a la carga correctas.

Se corrigió además el monitor interactivo: `ORDER BY lecturas.id` conserva orden
numérico aunque el SELECT convierta id a texto. `ORDER BY id` ordenaba por el alias
textual y podía tomar una fila antigua (por ejemplo, 9 antes que 28).

La estimación física de CO2 en esta sesión fue anormalmente baja (aproximadamente
50–60 ppm). Se conservaron valores y Ro experimental documentado; estas pruebas
acreditan transporte y recuperación, no precisión del CO2 ni recalibración.

## Procedimiento

1. Conectar el circuito y comprobar publicaciones reales recientes y su persistencia.
   Detener publicaciones sintéticas. Anotar commits, intervalo, reloj y entorno.
2. Desde `backend/`, ejecutar `pnpm exec tsx scripts/recoveryMonitor.ts wifi`.
3. Interrumpir solamente WiFi y confirmar con `FALLO CONFIRMADO`. El cronómetro
   empieza al confirmar; anotar cualquier retraso entre la falla y la confirmación.
   Restablecer la red sin reiniciar manualmente el ESP32 ni su cliente MQTT.
4. Repetir para `broker` y `backend`, cerrando e iniciando el proceso como falla
   controlada. El flujo posterior debe recuperarse sin reconexión o reinserción manual.
   Si se exige reinicio automático del proceso, configurar y registrar antes un supervisor.
5. El monitor consulta PostgreSQL cada 250 ms; mide hasta **observar una fila nueva**,
   no solo hasta que el servicio esté disponible. El resultado incluye el id y la fecha.
   La consulta añade hasta 250 ms más su duración y la confirmación humana tiene error.
6. Copiar los JSON de `backend/logs/` a esta carpeta, sin credenciales, y completar
   la tabla. Adjuntar logs MQTT para acreditar el origen y ausencia de intervención.

| Falla | Inicio UTC | Primera fila observada UTC | Segundos | Id | <60 s | Incidencias/corrección |
|---|---|---|---:|---|---|---|
| WiFi | 2026-10-29T15:26:15.854Z | 2026-10-29T15:26:59.442Z | 43,588 | 53 | PASS | Reconexión normal |
| Broker | 2026-10-29T15:25:40.941Z | 2026-10-29T15:26:00.577Z | 19,636 | 47 | PASS | Reinicio controlado del proceso |
| Backend | 2026-10-29T15:26:02.839Z | 2026-10-29T15:26:15.635Z | 12,796 | 50 | PASS | Reinicio controlado del proceso |

Una recuperación de 60 s o más **no pasa**. Conservar el resultado fallido y
repetir después de corregir la causa. No ejecutar durante la ventana de #150.

Prueba del cálculo: `pnpm exec tsx --test tests/recoveryMonitor.test.ts`.
La prueba de escritorio no acredita funcionamiento del circuito.
