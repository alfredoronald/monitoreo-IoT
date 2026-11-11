# T-8.7 / #146: revision de umbrales con el PO

Estado: **rangos operativos confirmados por el PO; nota provisional de HU-06 cerrada en este alcance**. La validación física de #145 permanece pendiente.

## Valores operativos confirmados

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

## Decisión del PO

Alfredo confirma conservar 0/800/1200 ppm tras revisar la evidencia de #145 y la propuesta de #146. La declaración fue transmitida por el responsable en esta conversación el 28 de octubre de 2026 (America/La_Paz). Texto íntegro y procedencia: [decisión](evidencias/t8-7/decision-po.md).

Esta decisión define el indicador del prototipo: no acredita precisión metrológica ni completa la validación física de #145. No se interpreta como aceptación de la habitación como aula o de la cercanía como ocupación equivalente. Los rangos podrán revisarse con mediciones comparables y referencia independiente.

## Aplicación y cierre de nota HU-06

Se conservan los valores y textos vigentes; no hay UPDATE ni migración numérica que aplicar. La captura de datos y los seis casos SQL coinciden con la decisión. Si una revisión futura modifica los rangos, deberá incluir SQL versionado transaccional que preserve IDs y referencias de alertas, y actualizar sus pruebas; no usar cambios manuales.

La nota provisional de selección de rangos de HU-06 queda cerrada por esta decisión operativa. database/esquema.sql y docs/api.md enlazan este registro. Se mantienen las limitaciones del MQ-135 y la evidencia histórica; no se reescriben resultados anteriores ni se cierra #145 o toda HU-08.

La revisión y fusión de esta PR siguen el flujo habitual del equipo. La declaración recibida del PO no se suplanta como una review de GitHub. La dependencia #145 aporta evidencia exploratoria y permanece con validación física pendiente, tal como la decisión reconoce.

## Reproduccion de comprobacion

Ejecutar database/review_co2_thresholds.sql completo mediante psql conectado a la BD.
Es una transaccion de solo lectura y no bloquea la ingesta con locks exclusivos.
Los casos 0, 799.99, 800, 1199.99, 1200 y 1500 verifican los rangos propuestos.
