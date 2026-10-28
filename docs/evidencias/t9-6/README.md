# Consolidación de resultados — #152

Estado: **parcial**. Hay evidencia QoS de #148; faltan recuperación #149,
continuidad física #150 y diez latencias #151. Esta tarea no está completada.

## Avance ejecutado el 28/10/2026

Se aplicó la migración y se importaron los dos resultados QoS reales en
`monitoreo_ambiental.pruebas_transmision` (ids 1 y 2). Cada fila tiene
500 enviados, 500 recibidos únicos, 500 guardados y pérdida generada de 0,00 %.
Las 15 lecturas existentes permanecen intactas. Ver `persistencia-qos.json`,
`resumen.json` y `resumen.md`. La carga se probó dos veces en PostgreSQL
temporal: quedaron dos filas, sin duplicación. Esa prueba no acredita #149–#151.

La migración `database/001_extend_transmission_metrics.sql` amplía la tabla
existente `pruebas_transmision` con JSONB para métricas y una clave única de
ejecución. No crea una tabla paralela ni modifica lecturas. Puede repetirse.

Desde `backend/`, previsualizar los resultados disponibles:

```powershell
pnpm exec tsx scripts/importTransmissionResults.ts ../docs/evidencias/t9-2/resultados.json
```

Agregar `--apply` para aplicar migración y carga en una única transacción usando
las credenciales privadas de `PG_*`. Repetir el mismo archivo no duplica filas.
Para consolidar todos los resultados, pasar también los tres JSON de recuperación,
el checkpoint completo de continuidad y el JSON de las diez latencias.

El script registra hash SHA-256 de cada fuente. El JSON de cada intento debe ser
inmutable: conservar intentos fallidos como ejecuciones separadas. Los resultados
de #150 se identifican por runId y variable y reutilizan sus filas, si ya existen.
`perdidos_pct` sigue siendo generado por PostgreSQL; las métricas de recuperación
y latencia no se evalúan mediante ese campo. Los conteos no medidos y los conteos
teóricos se explican expresamente en `observaciones` y `metricas`.

Salidas: `backend/logs/transmission152/resumen.json` y `resumen.md`. Tras revisar,
copiar a `docs/` el resumen y evidencia SQL sin secretos para el informe. No marcar
esta issue como lista hasta registrar las cuatro pruebas y sus tres casos de recuperación.

Pruebas: `pnpm exec tsx --test tests/transmissionResults.test.ts`.
