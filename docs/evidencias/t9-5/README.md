# Latencia publicación → dashboard — #151

Estado: **preparación**. #132 y #157 no estaban integradas al preparar la rama.
No hay diez mediciones reales ni aprobación de su criterio de promedio.

## Ejecución

1. Integrar #157 y #132 en sus respectivas ramas de área. Levantar backend y
   dashboard compatibles con `X-API-Key`. Registrar commits y navegador.
2. Crear un entorno exclusivo (broker y esquema/base de ensayo); conectar a él
   backend, dashboard y publicador. Detener ESP32 y otras publicaciones en este
   entorno. Los valores 901–910 °C son datos de correlación sintéticos, no mediciones.
3. Configurar en privado `PG_*`, `MQTT_URL`, `MQTT_PUBLISH_USER`,
   `MQTT_PUBLISH_PASS`, `API_KEY`, `LATENCY_API_URL` (URL terminada en `/api/`).
   La API, el backend y el script deben usar exactamente la misma base de ensayo.
4. En Network del navegador confirmar que el frontend envía la clave y mantiene
   el refresco normal cada 15 s. No pulsar recargar ni acelerar el polling.
5. Desde `backend/`, en PowerShell:

```powershell
$env:LATENCY_TEST_DATABASE='nombre_de_la_base_de_ensayo'
$env:LATENCY_SYNTHETIC_ISOLATED='yes'
pnpm exec tsx scripts/latencyCheck.ts logs/latencia-151.json
```

La herramienta exige 401 sin clave y éxito con clave. Por cada publicación,
confirmar el número SOLO cuando se vea en la tarjeta; cronometra hasta esa
confirmación visual, incluyendo reacción y tecleo humanos. La incertidumbre
debe declararse; no equivale a una captura automática del instante de render.
No sustituir por el momento en que la API responde. Contrasta el valor con API
y SQL y elimina únicamente su rango sintético nuevo al finalizar.

6. Copiar el JSON sin secretos y completar diez filas con publicación, aparición
   observada y segundos. Adjuntar captura o clip del ensayo y un promedio <20 s.
   Conservar fallos e intentos incompletos. Verificar que `cleaned=true`.

| Medida | Valor | Publicación UTC | Confirmación visual UTC | Segundos |
|---:|---:|---|---|---:|
| 1–10 | Pendiente | Pendiente | Pendiente | — |
| Promedio | | | Pendiente | — |

Prueba del cálculo (sintética): `pnpm exec tsx --test tests/latencyCheck.test.ts`.
