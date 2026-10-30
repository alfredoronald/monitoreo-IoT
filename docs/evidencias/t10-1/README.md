# T-10.1 / #153 — demostración de integración

Prueba física de ESP32 → Mosquitto → backend → PostgreSQL local → API → dashboard en la red local. Se utilizó el código integrado en `develop` (commit `1f5bc55`). El ESP32 publicó con la configuración normal, sin modo de simulación.

La configuración del broker exige autenticación y aplica el ACL del repositorio. La API respondió 200 con la clave configurada y 401 sin ella o con una clave incorrecta. La conexión MQTT anónima fue rechazada con código de salida 5. No se incluyen claves ni contraseñas en esta evidencia.

## Resultado de la ventana continua

El 30/10/2026, de 18:07:11 a 18:22:42 (hora de Bolivia), se registraron **930,673 s**, aproximadamente **15 min 31 s**. Se consultó PostgreSQL cada 30 s y se comprobó la API; la medición comprende 32 consultas, todas con 200 con clave y 401 sin clave.

| Variable | Filas guardadas | Mayor intervalo entre filas |
|---|---:|---:|
| Temperatura | 62 | 15,386 s |
| Humedad | 62 | 15,638 s |
| CO₂ estimado | 62 | 15,638 s |

Los conteos corresponden a esta ventana de 15 min 30,673 s; no son el resultado de una prueba de 24 horas. La captura final muestra los datos del dashboard al completar 15 minutos. El conteo demuestra filas en la base durante el intervalo; no mide por sí solo cada actualización visual del navegador.

Se replicó en PostgreSQL local el registro experimental aceptado de #142: Ro 49,486 kΩ, 60 lecturas y referencia exterior asumida de 420 ppm. El firmware se cargó con el mismo Ro; el monitor serie mostró `CO2 estimado con Ro configurado`. Este valor sigue sujeto a las limitaciones de la referencia experimental, alimentación aproximada y carga nominal documentadas en #141 y #142; no certifica precisión metrológica.

## Archivos

- `dashboard-inicial.png`: captura directa del dashboard con las tres variables.
- `dashboard-final.png`: captura directa del dashboard tomada al final de la ventana.
- `video-dashboard.zip`: video breve del dashboard obtenido con 22 capturas consecutivas, aproximadamente una por segundo. Corresponde al primer intento, antes de la interrupción; no representa los 15 minutos completos.
- `serie-esp32.txt`: extracto real filtrado del monitor serie físico, con lecturas y publicaciones MQTT. Corresponde al primer intento.
- `registro-continuo.jsonl`: muestras periódicas y tiempo de inicio y fin de la ventana continua, en UTC.
- `resumen-postgresql.json`: conteo y mayor intervalo entre filas por variable en la misma ventana, consultados directamente en PostgreSQL.

## Observaciones

Durante el primer intento se cambió la conexión de red de la computadora y hubo una interrupción posterior de la transmisión. Ese tramo no se cuenta como prueba continua. Se reiniciaron los servicios y se inició un nuevo registro independiente.

El pie del dashboard todavía indica que el CO₂ es provisional y requiere calibración, aunque el firmware ejecutado usa el Ro configurado. Se conserva esta observación para la revisión posterior; no se modificó el frontend dentro de esta tarea.
