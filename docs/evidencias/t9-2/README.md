# Comparación de QoS 0 y QoS 1 — Issue #148 (T-9.2)

**Sprint:** Sprint 3 · **Issue:** #148 · **Rama:** `test/148-qos-comparison`
**Dependencia:** #147 (`docs/pruebas.md`, métrica M-06) — integrada en la base de la rama.
**Ejecución:** 2026-10-28T03:26:42Z → 2026-10-28T03:27:16Z (UTC).

Prueba sintética **sin ESP32**: el script `backend/scripts/qosComparison.ts`
publica directamente contra Mosquitto con el cliente `mqtt` del backend como
suscriptor real, y persiste mediante `insertReading` sobre PostgreSQL.

## Entorno

| Componente | Versión / detalle |
|---|---|
| Node.js | v24.18.0 |
| PostgreSQL | 18.0 (instancia temporal aislada, puerto 55489) |
| Mosquitto | 2.1.2 (broker temporal autenticado, puerto 18848) |
| Backend | commit `8df7281` de `feature/backend-mqtt-api` |
| Topic | `ambiente/temperatura` |
| Intervalo entre publicaciones | 20 ms, `retain=false` |

## Aislamiento y limpieza

- Broker **temporal propio** con `password_file` y `acl_file` generados en el
  ensayo; no se modificó `infra/mosquitto/mosquitto.conf` ni el broker habitual
  (1883), que permanece intacto.
- PostgreSQL **temporal propio**; las credenciales de `backend/.env` no se usaron.
- Esquema desechable `qos148_*` creado con `esquema.sql` y **eliminado al
  finalizar** (`schemaDropped: true`).
- Inventario y conteos de las tablas públicas comparados **antes y después**:
  sin cambios (`publicBefore == publicAfter`).
- Credenciales temporales y archivos del broker borrados
  (`temporaryFilesRemoved: true`); broker detenido (`brokerStopped: true`).
- El log `backend/logs/mqtt-messages.jsonl` habitual no se utilizó: el ensayo
  inyecta su propio registrador de eventos.

**Los 1.000 registros sintéticos nunca tocaron la base `monitoreo_ambiental`.**

## Tabla de resultados (evidencia requerida)

| QoS publicación | QoS suscripción (pedido → concedido) | QoS efectivo | Enviados | Recibidos por backend | Únicos | Duplicados | Guardados en `lecturas` | Pérdida transporte | Pérdida persistencia |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 0 → 0 | 0 | 500 | 500 | 500 | 0 | 500 | 0 % | 0 % |
| 1 | 0 → 0 | 0 | 500 | 500 | 500 | 0 | 500 | 0 % | 0 % |
| **Total** | | | **1000** | **1000** | **1000** | **0** | **1000** | **0 %** | **0 %** |

- **QoS de suscripción:** el backend se suscribe a `ambiente/#` con `qos: 0`
  (`backend/src/mqtt/mqttSubscriber.ts`); el broker concedió QoS 0 en el SUBACK.
- **QoS efectivo:** menor entre publicación y suscripción → **0 en ambas
  rondas**. La ronda con publicación QoS 1 se publicó con QoS 1 (espera PUBACK)
  pero el broker entrega al suscriptor con QoS 0.
- **QoS observado en la entrega:** los paquetes PUBLISH recibidos informaron
  `qos=0` en ambas rondas (`deliveredQos: [0]`).
- **Enviados** = publicaciones sin error de callback; QoS 1 espera PUBACK.
- **Recibidos** = eventos `received` del suscriptor; **únicos** = valores
  distintos; **guardados** = filas nuevas con `valor` dentro del rango de la ronda.
- Rangos sintéticos separados: `10000–10499` (QoS 0) y `11000–11499` (QoS 1);
  conteos antes de cada ronda: `savedBefore = 0`.

## Limitación declarada

La suscripción actual del backend es QoS 0, por lo que **esta ejecución mide
transporte y persistencia con QoS efectivo 0 en ambos casos**. El valor de
publicación QoS 1 sí agrega confirmación PUBACK hasta el broker. Si el proyecto
requiere medir QoS efectivo 1 de extremo a extremo, deberá cambiarse primero el
`qos` de la suscripción del backend y repetir esta misma evidencia.

La issue no fija un porcentaje máximo de pérdida; ambos niveles observaron
**0 %** de pérdida en esta ejecución.

## Archivos

- `resultados.json` — resultado completo del ensayo (conteos, QoS, aislamiento,
  errores y `passed: true`).
- `recepciones-backend.jsonl` — 1.000 eventos de recepción con timestamp,
  topic y contenido.
- `backend/scripts/qosComparison.ts` — script reproducible (`pnpm test:qos`).

## Criterios de aceptación

- [x] Se enviaron 500 mensajes QoS 0 y 500 QoS 1.
- [x] Se registran enviados, recibidos y guardados por nivel.
- [x] Se indica el QoS de suscripción (pedido y concedido: 0).
- [x] Los datos de prueba no quedan mezclados con los reales (esquema temporal
      eliminado; base pública sin cambios).
