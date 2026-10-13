# Evidencias físicas — Issue #13 / T-1.7

Rama: `feat/13-publicacion-mqtt`. Carpeta real: `docs/evidencias/t1-7/`.
Los cuatro criterios fueron validados físicamente según el responsable; las
tres capturas existentes se revisaron para completar este registro.

Registro detallado de resultados, tiempos y limitaciones:
[validacion-fisica-mqtt.md](validacion-fisica-mqtt.md).
Procedimiento e implementación:
[publicación MQTT](../../../firmware/docs/mqtt-publication.md).

## Evidencias existentes

| Archivo real | Resultado documentado |
|---|---|
| [CA01_Publicacion_15s_PASS.png](CA01_Publicacion_15s_PASS.png) | CA-01: ciclos de recepción separados por 15 s. |
| [CA02_CA03_Topics_Payloads_PASS.png](CA02_CA03_Topics_Payloads_PASS.png) | CA-02/CA-03: tres topics exactos, payload numérico con un decimal. |
| [CA04_Omision_Independiente_PASS.png](CA04_Omision_Independiente_PASS.png) | CA-04: temperatura/humedad continúan sin CO2; CO2 reaparece al final. |

El nuevo registro Markdown describe estas capturas; no sustituye un log serie
ni un registro textual de recepción, que no están presentes en la carpeta.
CO2 sigue siendo una **estimación provisional**, no una medición calibrada.

## Revisión de privacidad

No se observan contraseñas no vacías en las tres capturas. Sin embargo, todas
muestran la dirección privada del broker y datos del equipo/usuario local;
CA02/CA03 también muestra un valor de `MQTT_USER` y la línea `MQTT_PASS` vacía.
Por ello **no se certifica ausencia de credenciales o información privada**.
Las imágenes originales se conservaron sin modificar: revisar y sanear esas
áreas antes de agregarlas al PR. Este registro no transcribe sus valores.

## Pendientes de trazabilidad y entrega

- Commit/revisión exacta del firmware cargado y versiones de las herramientas.
- Monitor serie: órdenes de omisión/restauración y muestras físicas posteriores.
- Registro textual completo de recepción; evidencia de calentamiento y
  referencia, cortes de red y recuperación sin ráfagas.
- Aclarar los horarios CA-01 comunicados por el responsable, distintos de la
  ventana visible en la captura; ambos se distinguen en el registro detallado.
- Revisar privacidad de las capturas y validar después la versión definitiva
  con `MQTT_TEMPORARY_TEST=0` cuando se autorice compilar/cargar.

No se ejecutaron tests, build, upload ni operaciones de escritura en Git al
completar esta documentación. `config.h` continúa privado y no se modificó.
