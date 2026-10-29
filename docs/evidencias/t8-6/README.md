# T-8.6: comparación de tres escenarios

Resultado: **criterio de subida en cerrado/ocupado NO CUMPLIDO** en estas capturas. Tres ventanas reales de cinco minutos, sin modificar Ro ni normalizar valores.

| Escenario | Lecturas CO2 | Mínimo ppm | Máximo ppm | Media ppm |
|---|---:|---:|---:|---:|
| Exterior | 19 | 365.90 | 444.70 | 393.31 |
| Aula ventilada | 18 | 306.40 | 356.50 | 332.19 |
| Aula cerrada y ocupada | 20 | 287.50 | 332.20 | 303.50 |

La diferencia de medias cerrado menos ventilado es -28.69 ppm. No se observa la subida pedida por #145; la tendencia comparativa es contraria a la esperada. Esto no demuestra que el CO2 real haya descendido por cerrar el recinto.

Condiciones declaradas: escenario Exterior en la ventana, expuesto al aire y protegido; no se confirmó lado exterior. Recinto interior: habitación pequeña usada como aproximación de aula. Ventilado con puerta y ventana abiertas; cerrado con ambas cerradas y una persona. Las condiciones no fueron verificadas remotamente.

El rango numérico de la captura Exterior está en 350–500 ppm, pero la equivalencia de la ubicación con exterior abierto queda por validar. La aceptación del recinto como sustituto de aula también corresponde al equipo.

Limitaciones: pruebas cortas y secuenciales, cambios de temperatura/humedad, MQ-135 no selectivo y referencia experimental. No se atribuye una causa concreta sin comprobarla. Se conserva la evidencia completa, incluyendo valores bajos, timestamps y huecos. No declarar la issue aprobada con estos resultados.

No se han iniciado nuevas mediciones ni ampliado los cinco minutos acordados por escenario.

## Archivos

`evidencias.zip` reúne las tres capturas, resúmenes y condiciones declaradas; `sha256.json` permite comprobar cada archivo. `comparacion.csv` contiene la tabla. No contiene credenciales ni config.h.
