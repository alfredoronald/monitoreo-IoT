# T-8.4 / #143 — conversión usando Ro configurado

Estado: implementación y carga física realizadas; validación exterior pendiente (captura fuera de rango).
No retirar advertencias de los informes históricos ni llamar calibrado a un
candidato pendiente. La ruta configurada no se etiqueta como provisional; la
ruta anterior sigue identificada como tal hasta que se complete la transición.

En `config.h` privado, después de #142:
- `MQ135_CALIBRATED_RO_KOHM`: valor exacto persistido en `ro_kohm`, en kOhm.
- `MQ135_CALIBRATION_ACCEPTED=1`: solo cuando la revisión acepte el resultado.
- `MQ135_SUPPLY_V`: alimentación del montaje documentada, coherente con el cálculo.
No poner un Ro real por defecto en la plantilla ni publicar credenciales.

Se usa Rs=RL*(Vc/VAO-1), ratio=Rs/(1000*Ro_kOhm), ppm=A*ratio^(-B), con
A=116.6020682 y B=2.769034857. Se conserva el divisor, calentamiento, límites ADC,
procedencia física y supresión durante captura. El inicio con Ro no toma tres
muestras ambientales como referencia de 400 ppm. Un Ro configurado pero no
aceptado bloquea la publicación de CO2 en vez de regresar silenciosamente al
modelo anterior. DHT y reconexión siguen funcionando.

Esta implementación no vuelve selectivo ni preciso al MQ-135. RL nominal y
topología supuesta conservan sus limitaciones aunque se acepte un ensayo
experimental. No confundir aprobación del código con calibración validada.

Registro local T-8.3 completado con Ro 49.486 kΩ y PR #174. Firmware físico cargado y captura real realizada: 35 muestras, 269.40–338.16 ppm junto a ventana abierta; no cumple 350–500 ppm. PostgreSQL recibe las nuevas lecturas. Ver [evidencias](../../docs/evidencias/t8-4/README.md). Falta repetir y superar la validación exterior antes del cierre. Los registros históricos no cambian de valor.
