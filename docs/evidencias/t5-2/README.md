# T-5.2 — Cliente de la API y tipos TypeScript

Issue: #81. Rama local: `feat/81-api-client`.
Base y destino de la futura PR: `feature/frontend-dashboard`.

## Revisión previa del contrato

Referencia exacta: [PR #103, docs/api.md, revisión 7af6e70](https://github.com/alfredoronald/monitoreo-IoT/blob/7af6e70bc46c1af8723cf902dd3e6f0565ebccde/docs/api.md).

Se revisaron las cinco rutas, parámetros, respuestas vacías, errores y su
coherencia con `database/esquema.sql` de develop. Se verificó mediante JSON.parse
la sintaxis de los seis ejemplos JSON: todos son válidos. No se identificaron
problemas bloqueantes para implementar el cliente frontend.

La descripción de la PR marca a Maicol como aceptado, pero la tabla del archivo
lo mantiene pendiente. Se debe unificar ese registro y reunir las aceptaciones
requeridas en #73. Esta revisión local no publica aprobación ni comentario.
El contrato permanece propuesto; revisar cualquier cambio antes de fusionar #81.

## Implementación

| Función | Operación del contrato | Tipo de retorno |
|---|---|---|
| `getReadings` | GET /api/lecturas, con variable/desde/hasta | `HistoricalReading[]` |
| `getLatestReadings` | GET /api/lecturas/ultima | `LatestReading[]` |
| `getAirState` | GET /api/estado-aire | `AirState \| null` |
| `getAlerts` | GET /api/alertas, filtro atendida opcional | `AirAlert[]` |
| `markAlertAttended` | PATCH /api/alertas/:id, JSON atendida=true | `AirAlert` |

Los tipos conservan los nombres SQL, valores numéricos, fechas UTC y los IDs
como strings. Se aceptan arrays vacíos y estado ausente `null`. Se valida el
JSON recibido antes de entregarlo al consumidor. Los errores HTTP conservan
status y mensaje; red y cancelación no se convierten en respuestas vacías.

La instancia de navegador en `frontend/src/api/index.ts` lee
`import.meta.env.VITE_API_URL`. Plantilla y configuración local actualizadas a
`http://localhost:3000/api`; `.env` continúa excluido de Git. No se duplica `/api`
en las rutas ni se fijan direcciones del backend en el cliente.

## Verificación real ejecutada

Desde `frontend/`, con Node.js 24.20.0 y las dependencias existentes:

| Comando | Resultado |
|---|---|
| `npm test` | 16 pruebas aprobadas, 0 fallos |
| `npm run build` | TypeScript y build Vite correctos |
| `npm run lint` | Código de salida 0 |

Salida completa: [pruebas-cliente.txt](pruebas-cliente.txt).

Las pruebas cubren las cinco solicitudes, codificación del signo `+` en fechas,
BIGINT máximo, filtros booleanos, PATCH, respuestas vacías, errores 400/404/415/500,
respuestas no JSON, tipos inválidos, fechas imposibles, cancelación y errores de
red. Dos pruebas cargan la instancia mediante Vite con archivos `.env`
temporales: verifican que usa la URL configurada y rechaza configuración ausente.

Los resultados HTTP se simulan con fetch controlado: **no se ejecutaron los
cinco endpoints del backend real ni se utilizó el ESP32**. La pantalla inicial
no cambia en esta tarea; el dashboard integrará el cliente en tareas posteriores.

## Estado de aceptación y entrega

- [x] Una función fetch para cada endpoint del contrato.
- [x] Tipos y respuestas alineados con la revisión de contrato indicada.
- [x] URL leída de `VITE_API_URL` y verificada con Vite.
- [ ] Contrato final acordado en #73; comprobar si cambia respecto a 7af6e70.
- Evidencia requerida por la issue: commit de entrega de esta rama, publicado
  y consultable en la pestaña Commits de su PR hacia `feature/frontend-dashboard`.

El registro de pruebas es evidencia complementaria al commit que solicita #81.
La publicación de la rama y la apertura de PR fueron autorizadas posteriormente
por el responsable; la aceptación final del contrato permanece pendiente.
