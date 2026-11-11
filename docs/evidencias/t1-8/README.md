# T-1.8 — Verificación de publicaciones MQTT durante diez minutos

Issue: #14. Rama: `test/14-mqtt-publish-verification`.

## Procedimiento ejecutado

El responsable compiló y cargó el entorno físico `esp32dev` en COM3; ambas
operaciones finalizaron con `SUCCESS`. Mosquitto 2.1.2 recibió las publicaciones
del ESP32. Se ejecutó desde Git Bash, en la raíz del repositorio:

```bash
mosquitto_sub -o firmware/.local-mqtt/subscriber.conf -t 'ambiente/#' -R -F '%I %t %p' -W 600 | tee docs/evidencias/t1-8/recepcion-mqtt.txt
```

`-W 600` limita la sesión a 600 segundos; el responsable comunicó su finalización
con `Timed out` y el retorno del prompt. `-R` excluye mensajes retained.
El archivo de opciones contiene credenciales locales y no se versiona.
El intervalo de publicación del firmware es de 15 segundos.

## Resultados medidos

Fecha del ensayo: 12 de octubre de 2026, UTC−04:00.

| Topic | Esperados (600 / 15) | Recibidos | Diferencia / pérdidas observadas |
|---|---:|---:|---:|
| `ambiente/temperatura` | 40 | 40 | 0 |
| `ambiente/humedad` | 40 | 40 | 0 |
| `ambiente/co2` | 40 | 40 | 0 |
| **Total** | **120** | **120** | **0** |

El registro contiene exactamente 120 líneas válidas. Cada topic tiene su primera
llegada a las 17:55:28 y su última a las 18:05:13. Sus 39 intervalos consecutivos
son de 15 segundos según los sellos de recepción, con resolución de un segundo.
Los 120 payloads tienen formato numérico con exactamente un decimal.

La distancia entre la primera y última muestra es de 585 segundos: 39 intervalos
entre 40 muestras. La duración configurada de la sesión es de 600 segundos;
no se confunde la distancia entre muestras con la duración de la suscripción.
No se registraron las horas exactas de inicio y fin del proceso.

No se observan faltantes ni huecos de publicación en este ensayo. La comparación
de conteos no acredita una garantía general de entrega: el firmware utiliza
QoS 0 y el payload no contiene identificadores de secuencia. No se provocaron
cortes de red ni fallos de sensores. CO2 es una estimación provisional del MQ-135.

## Evidencias y aceptación

- [x] Tabla de esperados contra recibidos por topic.
- [x] Registro del número de pérdidas observadas: cero.
- [x] Registro completo de recepción: [recepcion-mqtt.txt](recepcion-mqtt.txt).
- [x] Captura de la terminal de recepción: [recepcion-10-minutos.png](recepcion-10-minutos.png).

La captura se verificó visualmente: muestra las últimas lecturas, el límite
`-W 600`, `Timed out` y el retorno del prompt. Los valores y horarios coinciden
con el registro textual. No muestra contraseñas; sí el usuario, equipo y ruta
local de la terminal. Los tres criterios de aceptación cuentan con evidencia.
La base del ensayo contiene la integración de T-1.7
(`5946cdf`); no se registró el hash del binario cargado.
