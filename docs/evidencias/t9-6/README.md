# Consolidación de resultados — #152

Estado: **cuatro grupos registrados; listo para revisión el 29/10/2026**.
QoS #148, recuperación #149 (WiFi, broker y backend), continuidad física de
5 minutos #150 y diez latencias #151 están consolidados en PostgreSQL y docs/.

## Consolidación final del 29/10/2026

| Grupo | Resultado |
|---|---|
| QoS #148 | Dos rondas 500/500/500; sin pérdida ni duplicados; entrega efectiva QoS 0 |
| Recuperación #149 | Broker 19,636 s; backend 12,796 s; WiFi ESP32 43,588 s; todos <60 s |
| Continuidad #150 | 300 s reales; 20/20 por variable, 60/60 total; 100%; sin incidencias registradas |
| Latencia #151 | Diez mediciones; promedio 14,9012 s con API key activa |

`persistencia-final.json` registra **nueve filas**: dos QoS, tres recuperaciones,
tres variables de continuidad y una latencia. La importación completa se ejecutó
dos veces y permanecieron nueve filas: se reutilizaron las tres filas de #150.
Las 15 lecturas anteriores a la sesión permanecen intactas. Los resultados físicos
nuevos se conservan en `lecturas`; no se borran para volver al conteo histórico.

Las fuentes JSON físicas y la de latencia están copiadas sin alteración en
`fuentes/`, para reproducir el resumen sin depender de worktrees locales. Los
hashes, claves de ejecución, ids y métricas figuran en `resumen.json`.

La continuidad acredita **cinco minutos**, por cambio de alcance autorizado,
no 24 horas. El máximo intervalo observado entre lecturas fue 23,417 s con
publicación configurada a 15 s: el conteo no implica una cadencia perfecta.
El CO2 físico estimado fue anormalmente bajo (~50–60 ppm): estas pruebas no
acreditan exactitud del sensor ni recalibración. La ubicación en aula fue
confirmada por el operador. QoS/latencia usaron publicaciones sintéticas aisladas;
recuperación/continuidad usaron el circuito real, sin inyección sintética.

Repetir la carga final desde `backend/`:

```powershell
pnpm exec tsx scripts/importTransmissionResults.ts --apply ../docs/evidencias/t9-2/resultados.json ../docs/evidencias/t9-6/fuentes/149-broker.json ../docs/evidencias/t9-6/fuentes/149-backend.json ../docs/evidencias/t9-6/fuentes/149-wifi.json ../docs/evidencias/t9-6/fuentes/150-continuidad.json ../docs/evidencias/t9-6/fuentes/151-latencia.json
```

## Avance ejecutado el 28/10/2026

Se aplicó la migración y se importaron los dos resultados QoS reales en
`monitoreo_ambiental.pruebas_transmision` (ids 1 y 2). Cada fila tiene
500 enviados, 500 recibidos únicos, 500 guardados y pérdida generada de 0,00 %.
Las 15 lecturas existentes permanecen intactas. Ver `persistencia-qos.json`,
`resumen.json` y `resumen.md`. La carga se probó dos veces en PostgreSQL
temporal: quedaron dos filas, sin duplicación. Esa prueba no acredita #149–#151.

Se incorporó también el resultado de #151: diez publicaciones y visualizaciones
con promedio de 14,9012 s, con API key activa y datos sintéticos aislados. La fila
de latencia y los hashes de ambas fuentes están en el resumen actualizado.

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
esta issue como aprobada sin revisión del equipo; los cuatro grupos ya están registrados.

Pruebas: `pnpm exec tsx --test tests/transmissionResults.test.ts`.
