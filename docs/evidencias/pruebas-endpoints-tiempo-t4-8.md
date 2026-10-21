# Pruebas de los endpoints y consulta de 24 h (T-4.8)

Issue: #78. Ejecución: 2026-10-21T01:58:36.918Z (UTC). Entorno local: Node.js v24.14.0, PostgreSQL 18.3; API HTTP en `127.0.0.1` con puerto asignado durante la prueba.

## Método

La prueba se ejecutó sobre `feature/backend-mqtt-api` (commit `cd6885b0`) con la corrección mínima de #75 (commit `cf29d97`) aplicada temporalmente y el test de #78. No se integró `develop` para esta medición.

La prueba inicia la aplicación Express real y envía solicitudes HTTP con `fetch` de Node.js. Usa el esquema de `database/esquema.sql` y datos sintéticos dentro de una transacción PostgreSQL que se revierte al terminar. Así se prueba el comportamiento de los endpoints sin conservar lecturas de prueba. Se incluyen respuestas vacías, solicitudes válidas, parámetros inválidos y la política CORS.

Desde `backend/`, con PostgreSQL en marcha y la configuración local en `.env`:

```bash
pnpm exec tsx --test tests/apiEndpointsEvidence.integration.test.ts
```

La salida incluye `EVIDENCIA_T4_8=` seguido del resultado completo en JSON. El script falla si un caso no cumple lo esperado o si alguna medición de 24 h llega a 1 000 ms.

## Resultados

| ID | Caso | Esperado | Observado | Tiempo (ms) | Resultado |
|---|---|---|---|---:|---|
| H-01 | Histórico sin lecturas | 200; [] | HTTP 200; 0 lecturas | 61.033 | Aprobado |
| U-01 | Últimas lecturas sin datos | 200; [] | HTTP 200; 0 lecturas | 4.490 | Aprobado |
| H-02 | Histórico válido con extremos incluidos | 200; IDs 101 y 102 en orden | HTTP 200; 2 lecturas, orden y formato correctos | 4.434 | Aprobado |
| H-03 | Rango válido sin coincidencias | 200; [] | HTTP 200; 0 lecturas | 3.700 | Aprobado |
| H-04 | Zona horaria equivalente | 200; 2 lecturas | HTTP 200; 2 lecturas | 3.694 | Aprobado |
| U-02 | Última lectura de cada variable | 200; co2, humedad y temperatura | HTTP 200; 3 variables, fecha y valor válidos | 3.178 | Aprobado |
| H-05 | Variable desconocida | 400; mensaje en español | HTTP 400; mensaje: variable debe ser temperatura, humedad o co2. | 1.999 | Aprobado |
| H-06 | Falta desde | 400; mensaje en español | HTTP 400; mensaje: El parámetro desde es obligatorio. | 2.477 | Aprobado |
| H-07 | Fecha imposible | 400; mensaje en español | HTTP 400; mensaje: desde debe ser una fecha ISO 8601 válida con hora, segundos y zona horaria. | 2.662 | Aprobado |
| H-08 | Rango invertido | 400; mensaje en español | HTTP 400; mensaje: desde no puede ser posterior a hasta. | 2.343 | Aprobado |
| H-09 | Parámetro duplicado | 400; mensaje en español | HTTP 400; mensaje: El parámetro variable debe ser un texto no vacío y aparecer una sola vez. | 2.038 | Aprobado |
| H-10 | Parámetro desconocido | 400; mensaje en español | HTTP 400; mensaje: Parámetro de consulta no permitido: limite. | 1.324 | Aprobado |
| U-03 | Parámetros no admitidos en última | 400; mensaje en español | HTTP 400; mensaje: Esta ruta no admite parámetros de consulta. | 1.767 | Aprobado |
| C-01 | Origen autorizado por CORS | 200; Access-Control-Allow-Origin exacto | HTTP 200; origen permitido | 3.238 | Aprobado |
| C-02 | Origen rechazado por CORS | 403; sin cabecera de permiso | HTTP 403; mensaje: Origen no permitido por CORS. | 1.388 | Aprobado |
| P-00 | Preparación de la consulta de 24 h | 200; 5760 lecturas | HTTP 200; 5760 lecturas de temperatura, ventana de 24 h | 42.489 | Aprobado |
| P-01 | Consulta de 24 h, medición 1 | 200; 5760 lecturas; <1000 ms | HTTP 200; 5760 lecturas de temperatura, ventana de 24 h | 41.926 | Aprobado |
| P-02 | Consulta de 24 h, medición 2 | 200; 5760 lecturas; <1000 ms | HTTP 200; 5760 lecturas de temperatura, ventana de 24 h | 29.022 | Aprobado |
| P-03 | Consulta de 24 h, medición 3 | 200; 5760 lecturas; <1000 ms | HTTP 200; 5760 lecturas de temperatura, ventana de 24 h | 35.409 | Aprobado |

Total: **19 aprobados y 0 fallidos**. Casos fallidos: **ninguno**.

## Consulta de 24 h

Se insertaron **5760 lecturas de temperatura** separadas por 15 segundos para el intervalo `2026-10-19T00:00:00Z` a `2026-10-20T00:00:00Z`. La primera respuesta verificó el conjunto y sirvió de calentamiento; se hicieron tres mediciones adicionales. El tiempo va desde el envío de la solicitud HTTP hasta la lectura y conversión del cuerpo JSON completo en el mismo equipo.

| Medición | Tiempo (ms) | Límite | Resultado |
|---|---:|---:|---|
| 1 | 41.926 | < 1 000 ms | Aprobado |
| 2 | 29.022 | < 1 000 ms | Aprobado |
| 3 | 35.409 | < 1 000 ms | Aprobado |

Promedio: **35.452 ms**. Máximo observado: **41.926 ms**. Los tres tiempos medidos cumplen el criterio de respuesta menor a 1 segundo. La transacción se revirtió al terminar la prueba.

## Alcance de la medición

Los tiempos corresponden al equipo local y a PostgreSQL local, con un cliente HTTP automatizado. Son una medición reproducible de este entorno; la latencia en otra red o equipo puede ser distinta. La inserción de datos y el arranque del servidor quedan fuera del cronómetro.
