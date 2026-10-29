# Contrato de la API ambiental — Sprint 2

Issue: #73. Estado: **propuesto, pendiente de aceptación de HU-05 y HU-06**.
Este documento define el comportamiento que implementarán #74–#77 y #89.
Las cinco rutas todavía no están implementadas en esta rama. Los ejemplos
son ilustrativos; no son evidencia de respuestas ejecutadas.

## 1. Convenciones comunes

- Base local: `http://localhost:3000/api`. En otra instalación cambia el host
  y el puerto; se mantienen las rutas. El cliente configura `VITE_API_URL`
  como `http://localhost:3000/api`, sin barra final, y agrega `/lecturas`, etc.
- Respuestas JSON con `Content-Type: application/json`. PATCH requiere ese
  mismo encabezado en la solicitud. GET no lleva cuerpo.
- Nombres de campos coherentes con la base: `tipo_variable`, `valor`,
  `fecha_hora`, `atendida`. Variables: `temperatura`, `humedad`, `co2`.
- `valor` es un número JSON finito, aunque PostgreSQL devuelva NUMERIC como
  texto. Unidades: temperatura en °C, humedad en % HR y CO₂ **estimado** en ppm.
- Identificadores JSON como cadenas de dígitos positivos, incluidos los de
  umbrales. Así se conserva la precisión de los BIGINT de PostgreSQL.
- Fechas de entrada: ISO 8601 con fecha, hora, segundos y zona explícita,
  por ejemplo `2026-10-15T08:00:00-04:00` o `2026-10-15T12:00:00Z`.
  Se admiten fracciones de segundo. Fechas imposibles, solo fecha o sin zona
  responden 400. La salida siempre usa UTC con milisegundos y `Z`.
- Los parámetros de consulta son escalares. Valores vacíos, repetidos,
  parámetros desconocidos o cuerpos no permitidos responden 400. En URLs,
  codificar `+` como `%2B` si se utiliza una zona horaria positiva.
- Las listas se devuelven como arrays directos, sin envoltorio `data` ni
  paginación implícita. Lista sin coincidencias: **200 con `[]`**, nunca 404.
- Las rutas /api requieren `X-API-Key`; sin clave o con clave incorrecta responden 401. No enviar credenciales MQTT ni PostgreSQL al
  navegador. CORS permite el origen configurado en `CORS_ORIGIN` (#77).
- Errores no incluyen SQL, trazas, contraseñas ni contenido de `.env`.

## 2. Resumen de rutas

| Método | Ruta | Respuesta correcta |
|---|---|---|
| GET | `/api/lecturas` | 200, lista histórica de una variable |
| GET | `/api/lecturas/ultima` | 200, última lectura disponible por variable |
| GET | `/api/estado-aire` | 200, estado derivado del último CO₂ o `null` |
| GET | `/api/alertas?atendida=false` | 200, lista de alertas pendientes |
| PATCH | `/api/alertas/:id` | 200, alerta actualizada |

## 3. GET /api/lecturas

Consulta el histórico para gráficos (#74, #75, #84 y #85).

| Parámetro query | Tipo | Obligatorio | Regla |
|---|---|---|---|
| `variable` | string | Sí | `temperatura`, `humedad` o `co2` |
| `desde` | fecha ISO 8601 | Sí | Inicio del rango |
| `hasta` | fecha ISO 8601 | Sí | Fin del rango; debe ser igual o posterior a `desde` |

Los dos extremos son inclusivos (`desde <= fecha_hora <= hasta`). Comparar
instantes, no textos de fechas. Se devuelven **todas** las coincidencias en
orden `fecha_hora ASC, id ASC`. El parámetro `limite` del README inicial
queda fuera de este contrato de Sprint 2: no truncar resultados de 24 h
(aproximadamente 5 760 lecturas por variable). Cualquier límite o paginación
futuros requiere acordar una actualización del contrato.

Ejemplo de solicitud:

```http
GET /api/lecturas?variable=temperatura&desde=2026-10-15T12:00:00Z&hasta=2026-10-15T13:00:00Z
```

Respuesta **200**:

```json
[
  { "id": "101", "tipo_variable": "temperatura", "valor": 24.5, "fecha_hora": "2026-10-15T12:00:00.000Z" },
  { "id": "104", "tipo_variable": "temperatura", "valor": 24.7, "fecha_hora": "2026-10-15T12:00:15.000Z" }
]
```

Sin lecturas en ese rango: **200** con `[]`. Falta cualquier parámetro,
variable desconocida, fecha inválida o `desde > hasta`: **400**, con el
formato de la sección 8. No se aplican rangos predeterminados silenciosos.

## 4. GET /api/lecturas/ultima

Sin parámetros de ruta, query ni cuerpo. Usa `v_ultima_lectura` (#76).
Devuelve como máximo una lectura por variable, ordenada por
`tipo_variable ASC`: `co2`, `humedad`, `temperatura`.
Cada elemento contiene `tipo_variable`, `valor` y `fecha_hora`; no lleva
`id` porque la vista no lo expone.

```http
GET /api/lecturas/ultima
```

Respuesta **200**:

```json
[
  { "tipo_variable": "co2", "valor": 900, "fecha_hora": "2026-10-15T12:00:15.000Z" },
  { "tipo_variable": "humedad", "valor": 55, "fecha_hora": "2026-10-15T12:00:14.000Z" },
  { "tipo_variable": "temperatura", "valor": 24.7, "fecha_hora": "2026-10-15T12:00:15.000Z" }
]
```

Sin lecturas: **200** con `[]`. Si falta una variable, se omite su elemento;
el frontend muestra «Sin datos» en su tarjeta, sin inventar un cero.
La vista actual no desempata lecturas con igual timestamp; la selección
entre ellas no está garantizada por este contrato.

## 5. GET /api/estado-aire

Sin parámetros de ruta, query ni cuerpo. Devuelve **un objeto** con la fila
de `v_estado_aire` correspondiente a `tipo_variable = 'co2'` (#89, #91).
El indicador representa CO₂ estimado; no combina temperatura y humedad.

```http
GET /api/estado-aire
```

Respuesta **200**:

```json
{
  "tipo_variable": "co2",
  "valor": 900,
  "fecha_hora": "2026-10-15T12:00:15.000Z",
  "nivel": "moderado",
  "etiqueta": "Aire moderado",
  "recomendacion": "Conviene ventilar pronto."
}
```

- `nivel`: `bueno`, `moderado` o `malo`. `etiqueta`: string.
  `recomendacion`: string o `null`, como permite la tabla `umbrales`.
- Sin CO₂ o sin un umbral aplicable: **200** con el literal JSON `null`.
  El frontend muestra «Sin datos para clasificar», no «Aire bueno».
- La clasificación sale de la tabla `umbrales` mediante la vista: se elige
  el mayor `valor_desde` que no supere la lectura. No duplicar rangos en React.
- Rangos iniciales provisionales de #88: bueno `0 <= valor < 800`, moderado
  `800 <= valor < 1200`, malo `valor >= 1200`. No son límites médicos ni
  definitivos; el PO debe revisarlos. Los textos se leen de la base.
  Seguimiento de T-8.7: [revision con el PO](revision-umbrales-t8-7.md);
  la nota provisional permanece abierta hasta registrar su decision.

Para #83, #86 y #91: consultar valores actuales y estado cada **15 s**.
Una lectura con más de **60 s** de antigüedad se muestra como «Sin datos
recientes». La API conserva su valor y fecha; no la convierte en cero,
no oculta el histórico ni garantiza que el ESP32 esté conectado.

## 6. GET /api/alertas

| Parámetro query | Tipo | Obligatorio | Regla |
|---|---|---|---|
| `atendida` | boolean en URL | No | Solo los textos `true` o `false`; omitido devuelve todas |

`atendida=false` devuelve exclusivamente pendientes. `atendida=true`
devuelve atendidas. Valores como `0`, `1`, `False` o vacío responden 400.
Sin cuerpo ni parámetros de ruta. Orden: `fecha_hora DESC, id DESC`.

```http
GET /api/alertas?atendida=false
```

Respuesta **200**:

```json
[
  {
    "id": "7",
    "lectura_id": "102",
    "umbral_id": "2",
    "mensaje": "El CO2 estimado pasó a nivel moderado.",
    "fecha_hora": "2026-10-15T12:00:15.000Z",
    "atendida": false
  }
]
```

Los campos corresponden a la tabla `alertas`; `mensaje` es texto de la
alerta almacenada, no una cadena fija. Sin coincidencias: **200** con `[]`.
Consultar no genera alertas ni las marca como atendidas. La generación al
cambiar a moderado o malo corresponde a #90.

## 7. PATCH /api/alertas/:id

Marca como atendida una alerta (#89, #92). `id` es una cadena de dígitos
positivos sin signo ni decimales, entre `1` y `9223372036854775807` (BIGINT).
Sin parámetros query. Cuerpo obligatorio: objeto JSON con exactamente
`atendida: true`. No admite reabrir alertas con `false` en Sprint 2.

```http
PATCH /api/alertas/7
Content-Type: application/json

{ "atendida": true }
```

Respuesta **200** (objeto completo con el mismo formato que en GET):

```json
{
  "id": "7",
  "lectura_id": "102",
  "umbral_id": "2",
  "mensaje": "El CO2 estimado pasó a nivel moderado.",
  "fecha_hora": "2026-10-15T12:00:15.000Z",
  "atendida": true
}
```

Repetir la operación devuelve **200** con la alerta ya atendida; no crea
otra alerta ni cambia su `fecha_hora`. Después del éxito, el dashboard
vuelve a consultar pendientes para actualizar el aviso.

- ID inválido, JSON mal formado, cuerpo ausente, array, campos adicionales,
  `"true"` como string o `false`: **400** con mensaje.
- Falta `Content-Type: application/json` o es otro tipo: **415** con mensaje.
- ID válido pero alerta inexistente: **404** con mensaje.

## 8. Formato uniforme de errores

Todos los errores usan un objeto con un único campo obligatorio `mensaje`
(string explicativo en español). El cliente comprueba el código HTTP;
no depende del texto exacto para tomar decisiones.

Ejemplo **400** por variable desconocida:

```json
{ "mensaje": "variable debe ser temperatura, humedad o co2." }
```

| Código | Situación | Ejemplo de `mensaje` |
|---|---|---|
| 400 | Falta un parámetro obligatorio | `El parámetro desde es obligatorio.` |
| 400 | Fecha inválida o sin zona horaria | `desde debe ser una fecha ISO 8601 con zona horaria.` |
| 400 | Rango invertido | `desde no puede ser posterior a hasta.` |
| 400 | ID inválido | `id debe ser un entero positivo dentro del rango BIGINT.` |
| 400 | Cuerpo inválido | `El cuerpo debe contener únicamente atendida con valor true.` |
| 400 | JSON mal formado | `El cuerpo no contiene JSON válido.` |
| 400 | Query no admitida | `Parámetro de consulta no permitido: limite.` |
| 404 | Alerta inexistente | `No se encontró la alerta solicitada.` |
| 415 | Tipo de contenido incorrecto | `Se requiere Content-Type: application/json.` |
| 500 | Fallo interno o de base de datos | `No se pudo completar la solicitud.` |

Una consulta correcta sin resultados no es un error. Mantiene 200 con `[]`
para listas, y 200 con `null` para el objeto de estado del aire ausente.

## 9. Revisión y aceptación

Referencias: #73 (contrato), #74–#77 (backend), #81 (cliente tipado),
#83–#86 (dashboard), #88–#92 (umbrales, indicador y alertas).
Base de datos de referencia: `database/esquema.sql`, en particular
`v_ultima_lectura`, `v_estado_aire`, `umbrales` y `alertas`.

Antes de aceptar, confirmar especialmente: arrays directos, IDs como strings,
parámetros históricos obligatorios, estado singular de CO₂, ausencia como
`null`, atención sin reapertura y criterio de antigüedad de 60 s.
Las decisiones son propuestas hasta recibir las cuatro aceptaciones.

| Persona | Área | Estado | Evidencia de aceptación |
|---|---|---|---|
| Erick (@ErickAr-Dev) | HU-05 | Pendiente | — |
| Maicol (@maicolismael) | HU-05 | Pendiente | — |
| Alfredo (@alfredoronald) | HU-06 | Pendiente | — |
| Marcela (@MarcelaChalcoSoliz) | HU-06 | Pendiente | — |

Registrar el enlace al comentario o revisión de cada persona en el PR de
`docs/73-api-contract` hacia `feature/backend-mqtt-api`, correspondiente a
la versión final del contrato. Si cambia el formato después de la revisión,
solicitar confirmación de los cambios a las áreas afectadas. No marcar la
issue como completada mientras falte alguna aceptación.
