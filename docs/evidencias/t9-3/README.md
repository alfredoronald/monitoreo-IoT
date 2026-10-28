# Recuperación de WiFi, broker y backend — #149

Estado: **preparación; no se ejecutó la prueba física**. No se detectó puerto
serie del ESP32 al preparar esta tarea. Esta documentación no acredita sus criterios.

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
| WiFi | Pendiente | Pendiente | — | — | Pendiente | — |
| Broker | Pendiente | Pendiente | — | — | Pendiente | — |
| Backend | Pendiente | Pendiente | — | — | Pendiente | — |

Una recuperación de 60 s o más **no pasa**. Conservar el resultado fallido y
repetir después de corregir la causa. No ejecutar durante la ventana de #150.

Prueba del cálculo: `pnpm exec tsx --test tests/recoveryMonitor.test.ts`.
La prueba de escritorio no acredita funcionamiento del circuito.
