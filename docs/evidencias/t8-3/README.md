# T-8.3: registro y comprobación local

Resultado: **PASS**, verificado el 28 de octubre de 2026.

La referencia experimental de la PR #173, fusionada para #141, se registró en PostgreSQL local. La consulta real y la comparación con la macro privada están en `verificacion-local.json`. No contiene credenciales ni una copia de config.h.

- Ro persistido: **49.486 kΩ**; macro privada: **49.486 kΩ**.
- Sensor MQ-135, 60 muestras, referencia asumida 420 ppm, acondicionamiento documentado 0.250 h.
- Una única calibración vigente para el sensor. Se conserva el historial.
- Migración y registro ejecutados con los scripts de esta PR; respaldo anterior conservado solo en local.

## Alcance

El valor conserva las limitaciones experimentales de #141: VCC aproximado 4.54 V, RL nominal y CO₂ de referencia asumido. Registrar este resultado no añade una certificación metrológica.

No se cargó el firmware ni se demostró aquí la conversión a ppm: corresponde a #143 / T-8.4. La configuración privada no se versiona.

## Integración

Git no replica los registros locales. En la BD de destino se deben ejecutar la migración y el registro parametrizado según `docs/calibracion-vigente-t8-3.md`, usando estos datos y conservando sus observaciones. Después se repite la consulta de vigencia y la comparación con la configuración privada del ESP32 de destino.
