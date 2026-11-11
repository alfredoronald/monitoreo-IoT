# T-8.5: comparación real de publicación MQTT

Tres ventanas de 600 s reales, después de 45 s de asentamiento tras cada carga. Un ESP32, mismo Ro 49.486 kΩ, VCC configurado aproximado 4.54 V; ubicación exterior confirmada por el responsable. Fuente de firmware: PR #175 (8.4), integrada antes de publicar esta PR. Adquisición DHT/MQ conservada a 2 s.

| Intervalo | Teóricos/h por variable | Teóricos/h total | Esperados/variable en 10 min | Recibidos T/HR/CO2 | Mediana CO2 (s) | Mayor hueco (s) | CV CO2 (%) |
|---|---:|---:|---:|---|---:|---:|---:|
| 10 s | 360 | 1080 | 60 | 43/43/43 | 10.11 | 94.36 | 5.62 |
| 15 s | 240 | 720 | 40 | 31/31/31 | 15.04 | 56.86 | 4.84 |
| 30 s | 120 | 360 | 20 | 19/19/19 | 30.09 | 59.48 | 3.94 |

## Variación de valores por variable

| Intervalo | Variable | Mínimo | Máximo | Media | Desv. estándar | CV (%) | Huecos fuera de intervalo ±1 s |
|---|---|---:|---:|---:|---:|---:|---:|
| 10 s | temperatura | 20.30 | 20.40 | 20.31 | 0.035 | 0.17 | 11 |
| 10 s | humedad | 61.70 | 62.80 | 62.31 | 0.325 | 0.52 | 12 |
| 10 s | co2 | 320.60 | 402.10 | 357.12 | 20.076 | 5.62 | 12 |
| 15 s | temperatura | 20.20 | 20.30 | 20.27 | 0.045 | 0.22 | 4 |
| 15 s | humedad | 63.00 | 64.50 | 63.67 | 0.419 | 0.66 | 4 |
| 15 s | co2 | 338.20 | 419.50 | 373.14 | 18.047 | 4.84 | 4 |
| 30 s | temperatura | 20.30 | 20.40 | 20.31 | 0.031 | 0.15 | 3 |
| 30 s | humedad | 64.50 | 65.00 | 64.83 | 0.126 | 0.19 | 3 |
| 30 s | co2 | 353.40 | 402.10 | 380.37 | 14.996 | 3.94 | 3 |

## Interpretación y decisión

Se fija **15 s** en config.h privado y se vuelve a cargar al terminar. La plantilla versionada también lo indica. Frente a 10 s reduce 33.3% la carga teórica (1080 a 720 mensajes/h); frente a 30 s reduce a la mitad la espera nominal entre publicaciones. Las medianas observadas deben corresponder a cada intervalo, pero las ventanas presentan huecos y no demuestran entrega perfecta. No se declara que uno de los intervalos elimine esas incidencias.

Los conteos son mediciones de diez minutos; las columnas por hora son expectativas matemáticas, no una prueba de una hora. El margen diagnóstico de ±1 s no procede de una norma. El CV describe variación durante pruebas secuenciales en ambiente cambiante: no demuestra precisión ni causalidad por cambiar el intervalo. Los conteos inferiores no permiten distinguir desconexión del ESP32, omisión de publicación, transporte o pausa del receptor sin instrumentación adicional; las reconexiones registradas por el cliente QA no cubren las del ESP32.

La elección de 15 s es un compromiso operativo del prototipo, no una exigencia atribuida a Red MoniCA. HU-09 mantiene 240 mensajes/h por variable y 720/h totales. No se modifica su intervalo esperado ni se requiere aviso por cambio a otro intervalo. Los huecos observados deben considerarse al evaluar la prueba continua.

## Evidencias y reproducción

`comparacion.csv` contiene las nueve filas de métricas. `evidencias.zip` contiene mensajes MQTT reales con timestamps, resúmenes, estado de ejecución y logs de compilación/carga, incluida la restauración final a 15 s. No incluye config.h, .env ni el binario con credenciales. `sha256.json` permite comprobar los archivos del ZIP.

Herramienta: firmware/tools/compare-mqtt-interval.mjs. Instrucciones: firmware/docs/intervalos-t8-5.md. No se simularon mensajes ni se borraron muestras para mejorar los resultados.

## Comprobación final

`verificacion-final.json` confirma el valor privado 15000 ms y nuevas lecturas de las tres variables en PostgreSQL después de la carga final. No se cambió el Ro ni se incluyeron credenciales.
