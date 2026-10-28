# T-8.3 / #142 — registro de la calibración vigente

Base y destino: `feature/backend-mqtt-api`.
Estado inicial: implementación preparada; pendiente resultado aceptado de #141.
El candidato no se registra automáticamente como vigente.

## Actualizar el esquema sin borrar datos

Con PostgreSQL local activo y conectado como dueño de `calibraciones`, ejecutar
`database/001_add_calibration_sample_count.sql` con `psql -v ON_ERROR_STOP=1`.
La migración se puede repetir y conserva los datos. Las filas anteriores quedan
con `numero_lecturas = NULL` si no tenían cantidad documentada. El esquema para
instalaciones nuevas incluye la columna, pero `CREATE TABLE IF NOT EXISTS` no
actualiza una tabla existente: aplicar la migración en ese caso.
También amplía la precisión de `acondicionamiento_horas` para guardar 0.250 h
sin redondearlas a 0.3 h. Los valores históricos existentes se conservan; no se
intenta reconstruir una precisión que antes no se almacenó.

## Registrar un resultado aceptado

Usar `database/register_calibration.psql.sql`, con estas variables de psql:

| Variable | Valor que debe proceder de la captura aceptada |
|---|---|
| `sensor` | Identificador del mismo MQ-135, máximo 20 caracteres |
| `ro_kohm` | Ro en kiloohmios, no en ohmios; se almacena con tres decimales |
| `numero_lecturas` | Cantidad de muestras válidas, al menos 50 |
| `co2_referencia_ppm` | Referencia utilizada, indicando en observaciones si fue asumida |
| `acondicionamiento_horas` | Duración documentada; 15 min = 0.25 h según #141 actualizada |
| `fecha_hora` | Fecha real de la calibración en ISO 8601 con zona |
| `observaciones` | Enlace a revisión de #141, alimentación, fórmula y limitaciones |

Patrón de ejecución (reemplazar cada marcador antes de ejecutar):

```text
psql -h localhost -p 5433 -U app -d monitoreo_ambiental -W -v ON_ERROR_STOP=1 \
  -v sensor=MQ-135 -v ro_kohm=RO_ACEPTADO -v numero_lecturas=CANTIDAD \
  -v co2_referencia_ppm=REFERENCIA -v acondicionamiento_horas=HORAS \
  -v fecha_hora=FECHA_ISO -v observaciones=REVISION_Y_LIMITACIONES \
  -f database/register_calibration.psql.sql
```

Las variables se tratan como literales SQL mediante `:'variable'`. El script
valida entradas y cambia la vigente en una sola transacción, bloqueando cambios
concurrentes y preservando el índice único existente. Ante un error, detener la
ejecución; no continuar manualmente con solo parte de las sentencias.
Una ejecución válida repetida añade historial: no es un upsert ni elimina filas.

## Configuración privada y comprobación

Consultar en DBeaver o psql:

```sql
SELECT id, sensor, ro_kohm, numero_lecturas, co2_referencia_ppm,
       acondicionamiento_horas, fecha_hora, vigente, observaciones
FROM public.calibraciones ORDER BY fecha_hora DESC, id DESC;

SELECT sensor, count(*) FILTER (WHERE vigente) AS vigentes
FROM public.calibraciones GROUP BY sensor;
```

Copiar el valor persistido de `ro_kohm`, con sus tres decimales, a la macro
`MQ135_CALIBRATED_RO_KOHM` de `firmware/include/config.h`, antes de su `#endif`.
Ejemplo de estructura: `#define MQ135_CALIBRATED_RO_KOHM VALOR_PERSISTIDO`.
No publicar `config.h`, credenciales ni capturas que las incluyan. La macro
de ejemplo vale cero hasta completar este paso; #143 debe consumirla para
activar la conversión. Añadir la macro no demuestra que el firmware la utilice.

Evidencia de cierre: captura de la fila vigente, cantidad por sensor y registro
de comparación exacta entre la macro privada y el valor persistido. No declarar
estos pasos aprobados antes de ejecutarlos con datos aceptados.

## Prueba de implementación

Desde la raíz: `node --test backend/tests/calibrationRegistration.integration.test.mjs`.
Requiere las dependencias de backend y su `.env` local; crea y elimina un esquema
aislado `qa_calibration_*`. No inserta datos en las tablas reales. Comprueba
migración repetida, conservación del historial, activación por sensor, precisión
de 15 minutos, entradas inválidas y rollback si falla la inserción.
