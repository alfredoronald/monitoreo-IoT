# T-8.5: comparación de intervalos MQTT

La adquisición DHT22 y MQ-135 permanece cada 2000 ms. El parámetro comparado es
la publicación MQTT, no la frecuencia física de lectura ni el refresco del dashboard.

## Reproducción

Con el firmware T-8.4 y Ro de T-8.3 configurados, mantener ubicación y alimentación.
Para cada intervalo 10, 15 y 30 s:

1. Definir `MQTT_PUBLISH_INTERVAL_MS` en config.h privado como 10000UL, 15000UL o 30000UL.
2. Compilar y cargar el entorno físico: `pio run -d firmware -e esp32dev -t upload`.
3. Desde la raíz ejecutar, cambiando intervalo y directorio de salida:

```bash
node firmware/tools/compare-mqtt-interval.mjs --root . --interval 10 --duration 600 --settle 45 --output firmware/.local-mqtt/intervalos/10s
```

Requiere Node y dependencias de backend instaladas. Lee las credenciales desde
backend/.env sin imprimirlas. No modifica PostgreSQL ni inicia otro backend.
El broker y el suscriptor habitual deben permanecer activos. El cliente QA usa
un identificador propio y no sustituye al ESP32. Se excluyen mensajes retenidos.
Esperar 45 s tras suscribir permite el calentamiento del firmware después de cargar;
no sustituye el acondicionamiento físico del MQ-135.

Después de las tres pruebas, fijar 15000UL y cargar nuevamente. Esto reinicia el
ESP32 en cada carga; la BD conserva las lecturas recibidas y el histórico incluye
las pausas y los intervalos distintos. No se deben interpretar esos huecos como
pérdida de mensajes de un dispositivo funcionando continuamente a 15 s.

## Conteos esperados

| Publicación | Mensajes/h por variable | Total/h (3 variables) | Por variable en 10 min |
|---|---:|---:|---:|
| 10 s | 360 | 1080 | 60 |
| 15 s | 240 | 720 | 40 |
| 30 s | 120 | 360 | 20 |

El conteo de una ventana puede diferir en un mensaje por su fase y límites. Una
muestra inválida también se omite. El suscriptor no dispone de números de secuencia
ni ACK de almacenamiento: un conteo inferior no demuestra por sí solo pérdida de red.

## Estabilidad

Por variable se registra mínimo, máximo, media, desviación estándar y CV de los valores,
y la mediana de los intervalos y los huecos superiores/inferiores al intervalo ±1 s.
El margen de 1 s es un diagnóstico de cadencia, no una norma de calidad del aire.
La variación de valores puede proceder del ambiente: las pruebas secuenciales no
permiten atribuir causalmente cambios del CO2 al intervalo ni certificar exactitud.

## Decisión

Se conserva **15 s** como equilibrio operativo: respecto a 10 s reduce un tercio
las publicaciones (1080 a 720/h); respecto a 30 s reduce a la mitad la espera
nominal entre publicaciones. La adquisición sigue a 2 s y no se fuerza el DHT22.
La comparación real y cualquier incidencia se documentan en docs/evidencias/t8-5/.
Esta elección es de ingeniería del prototipo, no una exigencia atribuida a Red MoniCA.

HU-09 conserva su expectativa de 240 mensajes/h por variable, 720/h total; no hay
cambio del intervalo final de 15 s que requiera recalcular sus conteos.
