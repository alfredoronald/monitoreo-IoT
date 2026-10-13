# Verificación del histórico — #29

Dependencia: #28, integrada mediante la PR #63.
Rama: `test/29-verification-queries`. Destino: `feature/backend-mqtt-api`.
Consultas: `database/verification_queries.sql`.

## Preparación

1. Configurar `backend/.env` desde `.env.example` con los valores locales
   de `PG_*` y `MQTT_*`, sin versionar credenciales.
2. Iniciar PostgreSQL, Mosquitto y una única instancia del backend.
3. Desde `backend/`, ejecutar `pnpm install --frozen-lockfile`, `pnpm test`,
   `pnpm typecheck`, `pnpm build` y `pnpm dev`.
4. Confirmar conexión a la base y suscripción a `ambiente/#`.
5. Detener otras publicaciones (incluido el ESP32) durante esta prueba.

## Publicación controlada

Antes de publicar, ejecutar la consulta `limite_id` y anotar el límite inicial.
Publicar con el usuario MQTT `esp32`, QoS 0 y sin `retain`, estos contenidos:

| Topic | Contenidos válidos | Filas esperadas |
|---|---|---:|
| `ambiente/temperatura` | `21.11`, `22.22`, `23.33`, `24.44`, `25.55` | 5 |
| `ambiente/humedad` | `51.11`, `52.22`, `53.33`, `54.44`, `55.55` | 5 |
| `ambiente/co2` | `611`, `622`, `633`, `644`, `655` | 5 |

Publicar también `abc` en `ambiente/humedad`, `1` en `ambiente/status` y
`1013` en `ambiente/presion`: los tres deben descartarse.
Son 18 publicaciones, pero solo 15 lecturas válidas deben guardarse.

Ejemplo en PowerShell (credencial definida solo en la sesión local):

```powershell
& $env:MOSQUITTO_PUB_PATH -h localhost -p 1883 -u esp32 `
  -P $env:MQTT_PUBLISH_PASS -q 0 -t ambiente/temperatura -m 21.11
```

Repetir el comando para cada contenido de la tabla y los tres descartes.
Registrar los envíos que terminaron correctamente. Esperar las inserciones
asíncronas; si faltan filas, revisar el log y los errores de base de datos.
Ejecutar otra vez `limite_id` y anotar el límite final.

## Consultas y evidencia

1. Ejecutar conteo histórico y consultas de últimas lecturas.
2. Sustituir los dos `NULL` de `limits` por los límites de esta ejecución.
3. Ejecutar el conteo controlado: debe mostrar 5 registros y `COINCIDE`
   para cada variable (15 en total).
4. Confirmar que las últimas lecturas tienen los valores publicados.
5. Guardar capturas reales, legibles y sin credenciales:
   `conteo-lecturas-29.png` y `ultimas-lecturas-29.png`, en esta carpeta.

El conteo histórico incluye datos anteriores; solo el intervalo de IDs
permite comparar esta prueba con sus publicaciones. No borrar el histórico.

## Resultados

Verificaciones ejecutadas el 12 de octubre de 2026:

- `pnpm install --frozen-lockfile`: aprobado.
- `pnpm test`: 11 pruebas aprobadas, cero fallos.
- `pnpm typecheck`: aprobado.
- `pnpm build`: aprobado.
- Cambios en `schema.sql` y `esquema.sql`: solo comentarios; SQL sin cambios.

La prueba MQTT y la ejecución de las consultas siguen pendientes:
PostgreSQL está detenido y esta sesión no tiene permisos para iniciar su
servicio. Mosquitto no se encontró en el PATH ni en su ruta habitual.
Estas verificaciones del backend no demuestran todavía el conteo de #29.

Completar con límites inicial/final, conteos obtenidos, fecha de ejecución
y capturas reales antes de marcar los criterios de aceptación.
