# T-8.1 / Issue #140 — Captura MQ-135 por monitor serie

Base auditada: `fb7fbcd`; rama `feat/140-mq135-calibration-mode`;
destino previsto `feature/firmware-esp32`. Fecha: 26/10/2026 (America/La_Paz).

Actualizacion 27/10/2026: se habilita por defecto la **captura experimental de
Rs estimada**, con RL interna nominal estimada de 10000 ohm y RL efectiva
calculada/redondeada de 6666,67 ohm. **La topologia sigue sin verificar.**
Formula, supuestos y limites: [rs-estimada-experimental.md](rs-estimada-experimental.md).

Esta entrega implementa captura y diagnostico. **No mide Rs fisica en esta
sesion, no calcula Ro, no calibra CO2 y no registra calibraciones en PostgreSQL.**
No se hizo upload ni se utilizo un ESP32 conectado. No se modificaron backend,
frontend, base de datos ni credenciales. La actualizacion amplia las pruebas
de calibracion existentes para cubrir el modo estimado y el modo estricto.

## Comandos y ciclo de captura

Monitor a **115200 baudios**. Acepta LF, CR o CRLF; un CRLF ejecuta una sola orden.

| Comando | Comportamiento |
|---|---|
| `mq135 calibracion iniciar` | Solicita omision de CO2, descarta referencia provisional y espera sin bloquear el ACK de la tarea de red antes de activar captura. |
| `mq135 calibracion estado` | Informa modo, ultima Rs estimada, promedio estimado, validas, rechazadas, q adimensional, carga nominal y topologia supuesta. No adquiere ADC. |
| `mq135 calibracion detener` | Detiene o cancela el inicio pendiente; conserva resumen y recupera el inicio normal con una referencia de tres muestras nuevas. |

Al arrancar esta desactivada. Repetir `iniciar` mientras esta pendiente/activa
no reinicia el acumulador. Repetir `detener` no altera el resumen ni vuelve a
reiniciar la recuperacion. Un nuevo `iniciar` despues de detener inicia otro
acumulador cuando recibe el ACK; no combina sesiones.

Se puede solicitar durante el calentamiento: los turnos anteriores a los
30 s se rechazan sin Rs ni ADC fisico. Los 30 s son una espera de software,
no una garantia de estabilizacion ni sustituto del acondicionamiento inicial.

Los comandos antiguos `mq135 estado`, `mq135 ayuda` y `mq135 adc diagnostico`
siguen disponibles. Las ordenes de referencia o ppm se rechazan durante el
inicio pendiente y la captura; primero usar `mq135 calibracion detener`.
El entorno `esp32dev-mq135-demo` rechaza los tres comandos como calibracion
fisica y conserva sus etiquetas `[SIMULADO]` y su funcionamiento previo.

Hay un unico lector Serial, con presupuesto de 32 caracteres por vuelta y
buffer de 64 bytes. Lineas largas o con NUL se descartan completas, sin eco.
Las pruebas WiFi temporales reciben ordenes desde este parser mediante una
cola FreeRTOS, sin competir por `Serial.read()`. MQTT temporal comparte el
mismo parser. La cola de salida MQ-135 existente evita esperar por Serial y
cuenta los mensajes omitidos por congestion; una captura con omisiones necesita
repetirse para obtener evidencia completa.

## Adquisicion, calculo y unidades

Se reutiliza el mismo turno fisico cada **2000 ms**, la mediana de **11**
lecturas, la conversion `esp_adc_cal_raw_to_voltage` y las validaciones
existentes. Las once lecturas internas representan **una muestra**, no once.
No hay lecturas adicionales para imprimir, consultar estado o detener.

```text
VADC_V = milivoltios_ADC / 1000
VAO_V  = VADC_V / k
q      = (Vc_V - VAO_V) / VAO_V               [adimensional]
Rs_ohm = RL_efectiva_ohm * q                  [ohmios]
media_n = media_(n-1) + (Rs_n - media_(n-1)) / n
```

El acumulador usa memoria constante, conserva solo contadores/media/ultimo
resultado y admite al menos 50 muestras (test de 512). Solo incrementa
`validas` si Rs y la nueva media son finitas y positivas. Las rechazadas
no modifican el promedio. Hay proteccion ante desbordamiento numerico y de
contadores; no se utiliza suma de todas las Rs. El promedio de Rs **NO es Ro**.

Se rechazan calentamiento incompleto, fuente no fisica, circuito/conversion
sin verificar, ADC cero/saturado/fuera de rango, ausencia de conversion,
tensiones fuera del rango operativo, Vc/VAO no positivas/no finitas,
VAO >= Vc, RL no positiva/no finita y revision/referencia documental RL incompatibles.
El modo experimental permite expresamente la topologia supuesta; no omite
las verificaciones de proteccion electrica ni conversion ADC.

## Configuracion explicita de RL efectiva

`firmware/include/mq135_calibration_config.h` admite estas macros en
`config.h` privado, incluido antes de las cabeceras del firmware, o en
`build_flags`. Los valores actuales habilitan **solo Rs estimada** al solicitar
captura por serie; no inician captura automaticamente ni verifican RL:

| Parametro | Predeterminado | Significado |
|---|---|---|
| `MQ135_EXPERIMENTAL_RS` | `1` | Autoriza exclusivamente una estimacion con carga nominal y topologia supuesta. |
| `MQ135_EFFECTIVE_RL_OHMS` | `6666.67` | Carga efectiva estimada, en ohmios; no medida. |
| `MQ135_RL_TOPOLOGY_VERIFIED` | `0` | Sigue sin verificar; no cambiar por adoptar valores nominales. |
| `MQ135_RL_CIRCUIT_REVISION` | `3` | Debe coincidir con REV3. |
| `MQ135_RL_EVIDENCE_ID` | `docs/evidencias/t8-1/rs-estimada-experimental.md` | Referencia a los supuestos, no evidencia de medicion; maximo 95 bytes. |

El monitor etiqueta `[FISICO] [EXPERIMENTAL]`, `Rs_estimada_ohm`,
`promedio_Rs_estimada_ohm`, `RL=NOMINAL_NO_MEDIDA` y
`topologia=SUPUESTA_NO_VERIFICADA`. FISICO identifica el origen del ADC;
no afirma que RL o Rs hayan sido medidas directamente.

Con `MQ135_EXPERIMENTAL_RS=0`, los predeterminados de carga/revision/evidencia
vuelven a `0.0`, `0` y `""`: Rs queda bloqueada hasta configurar una carga
fisicamente verificada. La API de calculo estricta tambien sigue bloqueando
la estimacion por defecto. Activar simultaneamente estimacion y bandera de
topologia verificada se rechaza. No existe un comando serie que acredite RL
o cambie los parametros de carga.

REV3 documenta Vc puntual de 4,89 V, k=0,5, ADC1_CH6/GPIO34 a 12 bits/0 dB,
rango operativo 100–950 mV y divisor R1=10 kohm / R2=10 kohm. La evidencia
es [el registro previo de REV3](../t1-5/mq135-divisor-10k-rev3.md).
Ese registro **no establece RL interna ni verifica la topologia necesaria
para obtener Rs absoluta**. No se utilizan las resistencias R1/R2 como RL.

## Procedimiento pendiente para verificar RL fisicamente

La captura estimada puede probarse con los supuestos autorizados. Para pasar
al modo estricto queda pendiente medir RL interna excluyendo caminos en
paralelo y comprobar su conexion al nodo AO, sin buffer u otra etapa que
invalide el modelo. Registrar esa evidencia y recalcular la carga efectiva.
Los valores R1/R2 y las mediciones VCC, AO y GPIO34 ya documentados en Sprint 1
no se solicitan de nuevo como condicion para iniciar la captura experimental.
Si cambia el montaje, revisar su revision, parametros y referencias.

## Serial y MQTT

Durante captura continuan adquisicion DHT22, WiFi, `mqtt.loop()` y reconexion.
Temperatura y humedad mantienen validacion independiente y publicaciones
validas cada 15 s por defecto. CO2 queda omitido sin cambiar topics, formato,
QoS 0, retained=false ni el intervalo MQTT existente.

La tarea de red confirma la supresion entre ciclos, tras completar cualquier
`publish` anterior. Hasta ese ACK aparece `INICIO_PENDIENTE`; `loop()` no
espera activamente y sigue atendiendo sensores/comandos. No requiere conexion
con el broker para confirmar. Si no pudo crearse la tarea de red, la captura
puede iniciarse sin ACK porque no existe un publicador concurrente.
Un paquete entregado al transporte antes de la orden puede llegar al broker
mas tarde: no se interpreta su hora de recepcion como una adquisicion nueva.

La entrada y salida invalidan la copia CO2 e incrementan su epoca. Ademas de
seleccionar muestras bajo supresion, se revalida la epoca justo antes de
publicar para rechazar ciclos copiados anteriormente. No se sostiene un mutex
durante operaciones de red.

Al detener, la referencia anterior no se restaura: el flujo automatico solicita
tres muestras nuevas tras el calentamiento y habilita de nuevo las ppm
**provisionales** solo si pasan los controles existentes. La captura de Rs
no alimenta esa referencia. Los ciclos MQTT mantienen su reloj: no hay
publicacion inmediata, backlog ni reutilizacion de CO2 anterior. Un fallo en
la recuperacion mantiene CO2 omitido y requiere la recuperacion manual ya
documentada en `firmware/docs/mq135-experimental.md`.

## Pruebas y compilacion

Resultados ejecutados y alcance: [resultados.md](resultados.md).
La suite nueva es `firmware/tests/mq135_calibration_test.cpp`; los fixtures
estan marcados SINTETICOS y nunca se incluyen en el firmware.

Desde la raiz, con las herramientas ya instaladas:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File firmware/tools/run-desktop-tests.ps1 -Compiler C:/msys64/ucrt64/bin/g++.exe
```

Compilacion normal, cuando exista configuracion privada real:

```powershell
platformio run -d firmware -e esp32dev -e esp32dev-mq135-demo
```

En este checkout falta `config.h`. Para comprobar compilacion sin crearlo ni
introducir secretos se utiliza exclusivamente la plantilla existente mediante
el override de PlatformIO `PLATFORMIO_BUILD_SRC_FLAGS=-include .../config.example.h`.
El helper rechaza ejecutarse si ya existe `config.h`, restaura las variables
del proceso y guarda binarios aparte en `firmware/.pio/example-build`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File firmware/tools/compile-with-example.ps1 -PlatformIO C:/Users/WIN/.platformio/penv/Scripts/platformio.exe
```

**Estos binarios solo prueban compilacion; no se cargan al ESP32 y no
acreditan conexiones reales.** No se crea `config.h`. Los ejecutables/logs
completos de escritorio quedan en `.pio/desktop-tests`; los resultados
relevantes se conservan junto a este README.

## Evidencia fisica pendiente y continuidad con #141/#142/#143

- Ejecutar la captura experimental identificando Rs y promedio como estimados.
  La medicion de RL/topologia sigue pendiente para pasar al modo estricto.
- Documentar acondicionamiento, calentamiento y estabilizacion real; mantener
  separadas las 24 h registradas en Sprint 1 de la espera software de 30 s.
- Capturar en exterior al menos 50 muestras validas, con registro completo,
  tiempos, parametros electricos, ambiente y rechazos. El software no impone
  ni inventa un umbral de estabilidad; #141 debe justificar la aceptacion del lote.
- Verificar continuidad DHT22 y MQTT durante captura, caidas de red y retorno
  automatico con referencia nueva, comandos repetidos y congestion serie.
- #141 calcula Ro con la curva/referencia acordadas; #142 conserva trazabilidad
  y carga ese Ro en configuracion. Acordar ohmios frente a `ro_kohm` (factor 1000)
  y el redondeo de la base. #143 cambia la conversion; esta entrega conserva
  el modelo provisional normal y sus advertencias.

No hay evidencia fisica nueva ni se adjuntan capturas fabricadas. El MQ-135
responde a varios gases y a condiciones ambientales; esta captura no lo
convierte en un medidor certificado de CO2.
