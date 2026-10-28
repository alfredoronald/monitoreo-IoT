# T-8.4 / #143: evidencia real

## Resultado

- Conversión con Ro configurado: implementada y cargada al ESP32 en COM3.
- Ro privado: 49.486 kΩ, mismo valor registrado por T-8.3. VCC de trabajo aproximado: 4.54 V.
- 10 suites C++ de escritorio: PASS; incluyen unidades kΩ/Ω, fórmula, valores inválidos, calentamiento, bloqueo durante captura y exclusión de demo.
- Compilación física y demo: SUCCESS. Carga física: SUCCESS.
- Serie física: muestra «CO2 estimado con Ro configurado», sin marca provisional en la ruta configurada.
- **Criterio exterior 350–500 ppm: NO CUMPLIDO en esta captura.** 35 muestras: 269.40–338.16 ppm, media 307.78 ppm. No se recortaron ni normalizaron para aprobar.

Ubicación comunicada: junto a ventana abierta con viento. No se acredita condición exterior ni aire limpio certificado. El Ro conserva las limitaciones de #141 (referencia 420 ppm asumida, carga nominal, VCC aproximado). La aceptación documental de una referencia no garantiza que la validación física siguiente pase.

## Archivos

- `monitor-serie.txt`: captura real COM3/115200, con fecha por línea; incluye calentamiento, DHT, ADC y ppm. Filtrado por tipo de mensaje para excluir datos de conexión; no se excluyeron lecturas fuera de rango.
- `monitor-serie.svg`: representación legible de un extracto del mismo registro, no captura de una aplicación.
- `resultado-fisico.json`: resumen de todas las ppm capturadas.
- `pruebas-escritorio.txt`: resultado de suites con entradas sintéticas, separadas de la evidencia física.
- `compilacion-y-carga.json`: resultado de compilación, carga y hash del binario físico.
- `recepcion-postgres.json`: consulta real de las últimas lecturas; confirma continuidad de recepción tras el reinicio.

## Pendiente para cerrar #143

Repetir en exterior seco y protegido, con estabilización y condiciones identificadas. Si sigue fuera de rango, revisar alimentación, circuito y referencia con T-8.2/T-8.3; no alterar constantes para forzar 350–500. La integración de #142 sigue su PR #174. No cerrar esta tarea con esta evidencia como si el criterio exterior hubiera pasado.

## Aclaración y repetición exterior

El responsable aclaró después de la primera captura que el sensor ya estaba afuera y confirmó su ubicación exterior para repetir. La descripción anterior «junto a ventana» conserva lo entendido en ese momento; no contradice ni verifica remotamente esta aclaración. La repetición no cambia Ro, alimentación configurada ni fórmula y no reinicia el ESP32. Véanse los archivos de `repeticion-exterior/`.

Resultado posterior: el tramo final de 15 lecturas consecutivas de la repetición exterior quedó en 353.37–419.47 ppm. El rango se observó sin cambiar la calibración; revisar [informe completo](repeticion-exterior/README.md). El apartado pendiente anterior describe el resultado de la primera captura.
