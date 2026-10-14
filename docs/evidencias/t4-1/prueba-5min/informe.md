# T-4.1 / #32 — Prueba de integración de extremo a extremo (5 minutos)

Se ejecutó durante cinco minutos el flujo físico ESP32 → Mosquitto → backend → PostgreSQL. Se registraron 60 mensajes publicados, recibidos y almacenados, con diferencia cero y coincidencia de valores y secuencias por variable. Las evidencias PNG muestran el inicio, el punto intermedio y el final de la ejecución.

- Commit probado: `c9f2d2d2db56311a9160b0294fc0ecfa81b69f6e`; hash de firmware/src/main.cpp: `49a39c34f12a5b7d81930d9cb0e2df52442a25c0`.
- Dispositivo físico: ESP32-D0WD-V3, MAC 24:dc:c3:98:90:4c, COM5 / 115200 baudios.
- Inicio UTC: 2026-10-13T18:46:51.340Z.
- Fin de la ventana UTC: 2026-10-13T18:51:51.532Z.
- Duración de la ventana: 300.191 segundos; objetivo: 300 segundos.
- Base: monitoreo_ambiental; ID inicial: 131.
- Un solo ESP32 publica. La herramienta observa: no publica mensajes sintéticos ni inserta filas.
- Los PNG son reportes generados desde datos reales, identificados como tales; los JSONL/CSV conservan los registros.

## Comparación

| Variable | ESP32 enviados | MQTT recibidos | Backend recibidos | BD guardadas | MQTT − BD |
|---|---:|---:|---:|---:|---:|
| temperatura | 20 | 20 | 20 | 20 | 0 |
| humedad | 20 | 20 | 20 | 20 | 0 |
| co2 | 20 | 20 | 20 | 20 | 0 |

## Método y límites

- Se cuentan únicamente temperatura, humedad y co2; se excluyen status y mensajes retained.
- Ventana delimitada por tiempo y corte inicial de ID; se conservan mensajes anteriores/posteriores como guardas en los registros.
- Tras cerrar la ventana se esperan 5 segundos para los guardados pendientes.
- Se comparan cantidades y multiconjuntos de valores (incluyendo repeticiones) entre ESP32, broker, backend y BD; además, secuencias MQTT/BD por variable.
- Payload físico: un decimal; BD NUMERIC(8,2). Se normaliza a dos decimales.
- El contrato no tiene identificador de mensaje. No se atribuyen correspondencias únicas a valores repetidos ni se usa un conteo teórico.
- Salud: API, PostgreSQL y PIDs/fechas de inicio de broker/backend/BD, más monitor serial y conexión del observador MQTT.

## Resultado

- Comparación final: Cantidades, valores y secuencias coinciden en las cuatro etapas.
- Estabilidad estricta (sin eventos registrados, ventana de 300 s): CUMPLE.
- Cantidades y valores con datos para las tres variables: CUMPLE.
- Eventos registrados: 0.
- Auditoría independiente de los registros: 20 publicaciones, recepciones MQTT, recepciones backend y filas por variable; valores y secuencias MQTT/BD coincidentes.
- El ID inicial fue 131 y la última fila del intervalo fue 194. El rango por ID aislado incluye tres filas fuera del horario de la prueba; la comparación utiliza ambos filtros (ID y fecha), con 60 filas incluidas.

## Cumplimiento de criterios de aceptación

| Criterio | Evidencia | Resultado |
|---|---|---|
| El flujo funciona durante cinco minutos sin caídas | Ventana de 300,191 s; registros de salud y monitor serial | Cumple: sin reinicios, desconexiones ni errores registrados dentro de la ventana |
| Se comparan cantidades y valores y se anota la diferencia | Comparación de ESP32, MQTT, backend y PostgreSQL | Cumple: 60 mensajes y 60 filas; diferencia cero; valores y secuencias coincidentes |
| Hay evidencias PNG del inicio, punto intermedio y final | inicio.png, intermedio.png y final.png, respaldados por registros.zip | Cumple: reportes generados desde los datos observados |
| Los fallos encontrados se registran como issues | Incidencia de preparación documentada en #100 | Cumple: las desconexiones previas tienen seguimiento independiente |

## Evidencias

- [Inicio](inicio.png)
- [Punto intermedio](intermedio.png)
- [Final](final.png)
- [Comparación detallada](final.json)
- [Registros y herramienta utilizada, comprimidos](registros.zip)

## Incidencia encontrada durante la preparación

Se observaron dos desconexiones MQTT recuperadas antes de esta ventana. Se documentan en [preparación](../preparacion.md) y en [issue #100](https://github.com/alfredoronald/monitoreo-IoT/issues/100). Durante los cinco minutos de esta prueba no se repitieron.

El flujo cumple los criterios de aceptación de la prueba de integración de cinco minutos, según los registros y la comparación documentados.
