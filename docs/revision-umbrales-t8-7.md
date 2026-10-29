# T-8.7 / #146: revision de umbrales con el PO

Estado: **propuesta preparada; revision actual con el PO pendiente de constancia**.

## Valores vigentes consultados

| Nivel | Intervalo CO2 estimado (ppm) |
|---|---|
| Bueno | 0 <= valor < 800 |
| Moderado | 800 <= valor < 1200 |
| Malo | valor >= 1200 |

La consulta real de PostgreSQL local esta en docs/evidencias/t8-7/umbrales-consultados.json.
La clasificacion se obtiene de la tabla; no debe duplicarse en el frontend.

## Evidencia presentada a revision

Ro experimental registrado por T-8.3: 49.486 kOhm; conversion T-8.4, PR #175.
T-8.6, PR #177: exterior en ventana 365.90-444.70 ppm (media 393.31),
ventilada media 332.19, cerrada inicial 303.50 y repeticion 428.76 ppm.
La repeticion cambio cercania del ocupante; no permite atribuir la subida solo al
cierre del recinto. Habitacion con una persona usada como aproximacion de aula;
aceptacion del alcance y equivalencia exterior pendientes. Conservar estas reservas.

Todas las muestras de esos ensayos estan por debajo de 800 ppm. Por ello no validan
empiricamente los puntos de cambio de 800 y 1200 ni justifican bajarlos para obtener
alertas. Las comprobaciones SQL de fronteras validan la logica, no los umbrales fisicos.

## Propuesta para el PO

Conservar 0/800/1200 como rangos operativos del prototipo, sin afirmar que son limites
medicos, normativos o una calibracion metrologica del MQ-135. La propuesta prioriza
continuidad del indicador existente; los ensayos disponibles no aportan evidencia
para una nueva cifra. El PO puede rechazar, solicitar evidencia adicional o acordar
otros valores. La seleccion original de rangos no acredita esta revision posterior.

No se modifica la BD mientras no haya decision. Si conserva los rangos, no hace falta
una migracion que reescriba valores identicos. Si los cambia, preparar y revisar un SQL
versionado que preserve IDs y referencias de alertas, dentro de una transaccion;
actualizar tambien las expectativas de pruebas. No usar cambios manuales en DBeaver.

## Registro de revision (pendiente)

- PO: pendiente de identificar.
- Fecha y enlace a comentario de decision: pendientes.
- Evidencia de #177 y sus limitaciones revisadas: pendiente de confirmacion.
- Decision: propuesta CONSERVAR, todavia no aprobada en esta revision.
- Motivo y alcance aceptado: pendientes de constancia del PO.
- Nota provisional HU-06: **abierta** hasta esa constancia; no cerrada por este documento.

El responsable indico que cree que el PO ya reviso y que fue quien definio los rangos.
Se registra como antecedente comunicado, sin convertirlo en aprobacion actual.

## Pasos para cerrar despues de la decision

1. Adjuntar decision explicita del PO con nombre, fecha, rangos y alcance aceptado.
2. Resolver la dependencia #145 o registrar la aceptacion formal de su alcance reducido.
3. Si hay cambio, incluir y ejecutar el SQL versionado; consultar de nuevo los rangos.
4. Actualizar la nota provisional de database/esquema.sql y docs/api.md enlazando este
   registro ya completado. Conservar las limitaciones del sensor y evidencias historicas.
5. Adjuntar captura real de la consulta de umbrales (por ejemplo DBeaver), ademas del JSON.
6. Marcar lista para revision; no fusionar sin aprobacion del equipo.

## Reproduccion de comprobacion

Ejecutar database/review_co2_thresholds.sql completo mediante psql conectado a la BD.
Es una transaccion de solo lectura y no bloquea la ingesta con locks exclusivos.
Los casos 0, 799.99, 800, 1199.99, 1200 y 1500 verifican los rangos propuestos.
