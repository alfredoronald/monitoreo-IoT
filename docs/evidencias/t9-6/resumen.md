# Resumen de transmisión — #152

Estado: cuatro grupos registrados; revisar criterios.

| Prueba | Enviados* | Recibidos* | Guardados | Métricas |
|---|---:|---:|---:|---|
| #148 2026-10-28T03:26:42.582Z-50c9e2e3f7b1 qos0 | 500 | 500 | 500 | {"publicacion_qos":0,"suscripcion_qos":0,"qos_efectivo":0,"recibidos_eventos":500,"duplicados":0,"periodo_publicacion_ms":20,"inicio":"2026-10-28T03:26:44.426Z","fin":"2026-10-28T03:27:00.202Z"} |
| #148 2026-10-28T03:26:42.582Z-50c9e2e3f7b1 qos1 | 500 | 500 | 500 | {"publicacion_qos":1,"suscripcion_qos":0,"qos_efectivo":0,"recibidos_eventos":500,"duplicados":0,"periodo_publicacion_ms":20,"inicio":"2026-10-28T03:27:00.202Z","fin":"2026-10-28T03:27:16.064Z"} |
| #149 2026-10-29T15:25:40.941Z-e9767fd08b85 broker | 0 | 0 | 1 | {"recuperacion_segundos":19.636,"aprobado":true,"lectura_id":"47","inicio":"2026-10-29T15:25:40.941Z","fin":"2026-10-29T15:26:00.577Z","conteos":"Enviados y recibidos no medidos (0); guardados=primera fila de recuperación."} |
| #149 2026-10-29T15:26:02.839Z-91d89ff4d73a backend | 0 | 0 | 1 | {"recuperacion_segundos":12.796,"aprobado":true,"lectura_id":"50","inicio":"2026-10-29T15:26:02.839Z","fin":"2026-10-29T15:26:15.635Z","conteos":"Enviados y recibidos no medidos (0); guardados=primera fila de recuperación."} |
| #149 2026-10-29T15:26:15.854Z-300ef7c1ed4f wifi | 0 | 0 | 1 | {"recuperacion_segundos":43.588,"aprobado":true,"lectura_id":"53","inicio":"2026-10-29T15:26:15.854Z","fin":"2026-10-29T15:26:59.442Z","conteos":"Enviados y recibidos no medidos (0); guardados=primera fila de recuperación."} |
| #150 1f8a80d8-d766-49af-913a-921d2f39cbb0 temperatura | 20 | 20 | 20 | {"esperadas":20,"porcentaje":100,"aprobado":true,"inicio":"2026-10-29T15:27:16.551Z","fin":"2026-10-29T15:32:16.551Z","incidencias":[],"conteos":"Enviados=esperadas teóricas; recibidos=mínimo confirmado por persistencia, no medición MQTT independiente."} |
| #150 1f8a80d8-d766-49af-913a-921d2f39cbb0 humedad | 20 | 20 | 20 | {"esperadas":20,"porcentaje":100,"aprobado":true,"inicio":"2026-10-29T15:27:16.551Z","fin":"2026-10-29T15:32:16.551Z","incidencias":[],"conteos":"Enviados=esperadas teóricas; recibidos=mínimo confirmado por persistencia, no medición MQTT independiente."} |
| #150 1f8a80d8-d766-49af-913a-921d2f39cbb0 co2 | 20 | 20 | 20 | {"esperadas":20,"porcentaje":100,"aprobado":true,"inicio":"2026-10-29T15:27:16.551Z","fin":"2026-10-29T15:32:16.551Z","incidencias":[],"conteos":"Enviados=esperadas teóricas; recibidos=mínimo confirmado por persistencia, no medición MQTT independiente."} |
| #151 2026-10-28T14:30:57.374Z-476e9a50998e latencia | 10 | 10 | 10 | {"latencias_segundos":[15.038,14.81,14.943,14.964,14.85,14.903,14.81,14.853,14.963,14.878],"promedio_segundos":14.9012,"aprobado":true,"conteos":"10 publicaciones, observaciones visuales y filas confirmadas; no conteo independiente de eventos MQTT.","inicio":"2026-10-28T14:30:57.374Z","fin":"2026-10-28T14:33:48.629Z","datos_sinteticos_eliminados":true} |

*Las definiciones de los conteos dependen del ensayo y figuran en métricas. 0 puede representar no medido; no inferir pérdidas de recuperación de esos campos.

Pendientes: consultar casos y criterios.
La presencia de filas no equivale a aprobar las pruebas; conservar y corregir los resultados fallidos.
