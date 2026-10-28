# Latencia publicación → dashboard — #151

Estado: **ensayo ejecutado el 28/10/2026; listo para revisión**.
Se usaron las versiones integradas de #132 y #157.

## Resultados observados

Se ejecutó Edge headless con el dashboard real, API Express autenticada, Mosquitto
y PostgreSQL local. Los diez mensajes son sintéticos, separados de las mediciones
físicas mediante broker y esquema exclusivos. No se interceptaron solicitudes ni
se aceleró el polling de 15 s. El reloj se tomó antes de publicar y al detectar el
valor en la tarjeta, esperando dos requestAnimationFrame para incluir el render.
Esto automatiza el cronómetro; no es una observación humana de pantalla.

| Medida | Valor sintético | Publicación UTC | Aparición observada UTC | Segundos |
|---:|---:|---|---|---:|
| 1 | 901 | 14:31:13.673 | 14:31:28.711 | 15,038 |
| 2 | 902 | 14:31:28.915 | 14:31:43.725 | 14,810 |
| 3 | 903 | 14:31:43.841 | 14:31:58.784 | 14,943 |
| 4 | 904 | 14:31:58.895 | 14:32:13.859 | 14,964 |
| 5 | 905 | 14:32:13.973 | 14:32:28.823 | 14,850 |
| 6 | 906 | 14:32:28.927 | 14:32:43.830 | 14,903 |
| 7 | 907 | 14:32:43.933 | 14:32:58.743 | 14,810 |
| 8 | 908 | 14:32:58.859 | 14:33:13.712 | 14,853 |
| 9 | 909 | 14:33:13.821 | 14:33:28.784 | 14,963 |
| 10 | 910 | 14:33:28.895 | 14:33:43.773 | 14,878 |
| **Promedio** | | | **Menor de 20 s** | **14,9012** |

- Backend: `ab382e3`; frontend: `9de8ba3`; Node: v24.18.0.
- Sin clave: HTTP 401; clave incorrecta: HTTP 401; clave correcta: HTTP 200.
- Todas las solicitudes API del dashboard incluyeron la clave correcta. El JSON
  registra solamente un booleano, nunca el valor de la clave.
- Cero excepciones JavaScript registradas.
- El esquema sintético se eliminó y las 15 lecturas físicas existentes quedaron
  intactas. Los valores 901–910 °C son identificadores de correlación irreales;
  no son lecturas del sensor. Las otras variables quedan vacías deliberadamente.
- La ausencia de históricos en las capturas corresponde a la ventana fija
  seleccionada antes de las publicaciones; no intervino en la medición de tarjetas.

`latencia-real.json` contiene los timestamps completos, métricas, comprobaciones
de autenticación y limpieza. `capturas-y-ejecutor.zip` incluye dos capturas reales
y el ejecutor exacto empleado. Sus rutas locales deben adaptarse al repetirlo;
requiere Playwright instalado aparte. El ejecutor genera credenciales temporales,
levanta servicios aislados y los detiene al terminar; no incluye secretos reales.

Los tiempos dependen del entorno y de la fase de polling: este resultado no
garantiza la misma latencia en otra red ni acredita el circuito del ESP32.

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

Prueba del cálculo (sintética): `pnpm exec tsx --test tests/latencyCheck.test.ts`.
