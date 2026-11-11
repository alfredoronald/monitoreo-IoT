# Matriz de control de acceso - #134

## Alcance y entorno

- Tarea: #134. Historia padre: #137.
- Rama: `test/134-access-control-cases`.
- Base y destino de la PR: `feature/backend-mqtt-api`.
- Fecha de ejecución: 28/10/2026.
- Entorno: Windows, Mosquitto 2.1.2, PostgreSQL local y backend del commit
  `c84fb29` (incluye #157).

Las pruebas usaron únicamente las credenciales privadas ya configuradas en
`backend/.env` y `firmware/include/config.h`. No se imprimieron, adjuntaron ni
incluyeron contraseñas, claves de API o hashes de Mosquitto.

## Resultados de API

Se consultó `GET /api/lecturas/ultima` contra el backend reiniciado con la
configuración de la rama actual. La solicitud correcta devolvió lecturas reales
del circuito. El preflight se hizo desde el origen configurado del dashboard.

| Caso | Esperado | Obtenido | Resultado |
|---|---|---|---|
| Sin `X-API-Key` | HTTP 401 con `No autorizado.` | HTTP 401 con `No autorizado.` | Correcto |
| `X-API-Key` incorrecta | HTTP 401 con `No autorizado.` | HTTP 401 con `No autorizado.` | Correcto |
| `X-API-Key` correcta | HTTP 200 con lecturas | HTTP 200 con temperatura, humedad y CO2 | Correcto |
| Preflight `OPTIONS` con `X-API-Key` | No devuelve 401 y permite el encabezado | HTTP 204; `Access-Control-Allow-Headers: Content-Type,X-API-Key` | Correcto |

Como verificación de la cadena activa, dos consultas autenticadas separadas por
20 segundos devolvieron una lectura de CO2 nueva: 670 ppm a las
`12:51:50.393Z` y 642.5 ppm a las `12:52:05.098Z`.

## Resultados de broker

Se ejecutó `pnpm exec tsx scripts/accessControlMatrix.ts` desde `backend/`.
El comprobador toma los usuarios privados solo en memoria, usa MQTT 5 y QoS 1
para recibir los códigos de autorización, y publica un marcador único por
ejecución. La lectura del ESP32 se comprueba por ausencia de entrega: Mosquitto
puede aceptar el filtro de suscripción, pero su ACL impide entregar mensajes.

| Caso | Esperado | Obtenido | Resultado |
|---|---|---|---|
| Conexión anónima | Rechazada | Rechazada | Correcto |
| Usuario o contraseña incorrectos | Rechazados | Rechazados | Correcto |
| `esp32` publica en `ambiente/#` | Permitido y recibido por backend | Permitido y recibido | Correcto |
| `esp32` publica fuera de `ambiente/#` | Denegado | PUBACK 135 (`Not authorized`) | Correcto |
| `esp32` se suscribe a `ambiente/#` | No recibe mensajes | No recibió el marcador publicado | Correcto |
| `backend` publica | Denegado | PUBACK 135 (`Not authorized`) | Correcto |
| `backend` se suscribe a `ambiente/#` | Recibe publicaciones de `esp32` | Recibió el marcador publicado | Correcto |

La conexión física con las credenciales rotadas también quedó verificada en la
[evidencia de #133](133-acl-mosquitto.md). Esta ejecución confirma que el
backend actualizado sigue recibiendo publicaciones del circuito con la API ya
protegida por #157.

## Reproducción

1. Iniciar PostgreSQL, Mosquitto y el backend con las configuraciones privadas
   locales completas, incluida `API_KEY` en `backend/.env`.
2. Desde `backend/`, ejecutar `pnpm exec tsx scripts/accessControlMatrix.ts`.
3. La salida JSON debe mostrar todos los casos booleanos en `true` y los dos
   códigos de denegación MQTT en `135`.
4. Consultar la API sin clave, con una clave incorrecta y con la clave de
   `backend/.env`; comprobar además el preflight desde el origen del dashboard.
