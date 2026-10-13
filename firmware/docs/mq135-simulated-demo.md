# Issue #11 — T-1.5 / CA-03: demostración provisional en ESP32

La implementación permite mostrar `CO2 estimado provisional: XXX ppm
[SIMULADO]` después de los 30000 ms de espera del firmware. Todos los números
MQ-135 de este modo son **sintéticos**, sin adquirir AO ni convertir el ADC
físico. No son concentraciones de CO₂ medidas ni calibradas. El DHT22 continúa
con sus lecturas físicas, validación e intervalo de 2000 ms.

Se entrega el código y las pruebas para verificación manual. **No se han
ejecutado compilaciones, pruebas ni cargas en esta intervención.** Las 187
comprobaciones sintéticas aprobadas previamente (131 del modelo y 56 de la
integración, según los registros existentes) son antecedentes; no acreditan
la nueva demostración ni el hardware. No se declara validado físicamente CA-03.

## Selección y estados

`platformio.ini` mantiene `esp32dev` como entorno por defecto. El entorno
explícito `esp32dev-mq135-demo` hereda su configuración y dependencias y añade
`MQ135_SIMULATED_DEMO=1`. Sin esa selección, la macro vale cero. La demo no
puede activarse con comandos serie ni cambia las verificaciones eléctricas físicas.

En la compilación demo se excluyen las llamadas MQ-135 a `analogReadResolution`,
`analogSetPinAttenuation`, `analogRead` y a la caracterización/conversión ADC.
El ESP32 puede demostrar el flujo con AO desconectado. No se agrega ninguna
biblioteca ni se modifica Wi-Fi, MQTT o `.vscode/`.

| Momento / condición | Estado y comportamiento MQ-135 |
|---|---|
| Antes de 30000 ms desde `begin` | `CALENTANDO [SIMULADO]`; ninguna estimación ni muestra sintética consumida. |
| Espera completa, siguiente turno de 2000 ms | Validación del fixture, modelo normalizado y `ESTIMACION_DISPONIBLE [SIMULADO]`. |
| Muestra o resultado inválido | Sin ppm numéricas; motivo de bloqueo y etiqueta `[SIMULADO]`. |
| Referencia demo invalidada | `REFERENCIA_PENDIENTE`; no se reconstruye automáticamente. |
| Regreso a la configuración normal actual | `VERIFICACION_PENDIENTE`; ADC bruto físico y ppm bloqueadas. |

La transición a 30000 ms no introduce una adquisición fuera de intervalo.
Si el bucle se retrasa, se usa el siguiente turno disponible sin recuperar
muestras en ráfaga. Se conservan el manejo de rollover de `millis()`, la cola
serie acotada y los intervalos existentes. Los 30 segundos demuestran una
espera de software; no prueban estabilización térmica ni acondicionamiento físico.

## Referencia y muestras inventadas

[mq135_simulated_demo.h](../include/mq135_simulated_demo.h) define una referencia
demostrativa fija **q = 4**, exclusivamente en memoria y separada de
`Reference`/`ReferenceLog`. Nunca fabrica una revisión eléctrica, evidencia ni
`VerifiedContext`. Entrar en demo descarta cualquier referencia física anterior;
volver a la configuración física descarta la demo y deshabilita la emisión.
No hay persistencia NVS ni importación de referencias históricas.

| Orden repetido | ADC sintético | Vc sintética (V) | VAO sintética (V) |
|---|---:|---:|---:|
| 1 | 1200 | 5,0 | 1,0 |
| 2 | 1400 | 5,0 | 1,2 |
| 3 | 1600 | 5,0 | 1,4 |
| 4 | 1400 | 5,0 | 1,2 |

Los códigos y voltajes son pares de prueba inventados, **no una transferencia
ADC a voltaje** ni una reconstrucción de AO real. Los mínimos demostrativos
ADC=10 y VAO=0,15 V solo ejercitan validaciones; no establecen un rango útil
para la placa. La secuencia vuelve al primer elemento cada cuatro turnos sin
recalcular q de referencia.

Se reutilizan las validaciones ADC de 12 bits (fuera de rango, cero, saturación,
señal insuficiente), tensiones positivas y finitas y controles de resultados
no finitos, overflow y underflow del modelo GeorgK/MQ135 normalizado:

```text
q = (Vc - VAO) / VAO
ppm = 400 * (q / 4)^(-2.769034857)
```

A = 116.6020682 se cancela por normalización. **400 ppm es un supuesto de
desarrollo**, provisional hasta HU-08. El primer fixture tiene q=4 y por ello
produce 400,00 ppm simulados; los siguientes cambian con VAO y conservan la
misma referencia. No se aplica compensación ambiental con el DHT22.

## Compilar, cargar y verificar manualmente

Con el equipo apagado, dejar **AO del MQ-135 desconectado de GPIO34** para esta
demostración. El MQ-135 no necesita estar conectado para ejecutar la demo;
mantener el DHT22 funcional. Conectar el ESP32 por USB y abrir una terminal
con PlatformIO disponible. Desde PowerShell:

```powershell
Set-Location D:\telematicos\monitoreo-IoT-Maicol\firmware
pio run -e esp32dev-mq135-demo
pio run -e esp32dev-mq135-demo -t upload
pio device monitor -e esp32dev-mq135-demo --baud 115200
```

Si hay varios puertos, consultar `pio device list` y añadir `--upload-port COMx`
a la carga y `--port COMx` al monitor, sustituyendo COMx por el puerto real.
Cerrar el monitor con Ctrl+C antes de volver a cargar.

Con el monitor abierto, pulsar EN/RESET. Verificar:

1. Arranque marcado `MQ-135 [SIMULADO]`, sin adquisición ADC física.
2. Durante la espera, `MQ-135: CALENTANDO [SIMULADO]` y mensajes DHT22 con su
   validación habitual; **ninguna línea `CO2 estimado provisional:`**.
3. Después de al menos 30000 ms relativos al inicio MQ-135, primera salida
   como el siguiente ejemplo ilustrativo (no es evidencia capturada):

   ```text
   [30014 ms] MQ-135 ADC sintetico: 1200 | ESTIMACION_DISPONIBLE [SIMULADO]
   CO2 estimado provisional: 400.00 ppm [SIMULADO]
   Referencia demostrativa fija: 400 ppm asumidos, no medidos [SIMULADO]
   Pendiente de calibración y validación en HU-08 [SIMULADO]
   ```

4. Aproximadamente cada 2000 ms aparecen las siguientes muestras. Todas las
   líneas MQ-135 con valores sintéticos llevan `[SIMULADO]`; el DHT22 sigue
   siendo físico. El prefijo temporal permite comprobar la primera emisión.
5. `mq135 estado` muestra estado y referencia **demostrativa**, sin referencia
   física; `mq135 ayuda` indica cómo regresar. Los comandos de captura,
   invalidación y habilitación física se rechazan en el monitor demo.
6. Reiniciar vuelve a imponer los 30 segundos y repite la secuencia desde su
   primer fixture. Guardar el log real para evidenciar CA-03 en modo simulado.

`mq135 adc diagnostico` informa `inactivo [SIMULADO]` en este entorno; no
consulta eFuse ni adquiere ADC. Para observar la calibración del chip se debe
cargar el entorno normal; ver [diagnóstico eFuse/ADC](mq135-adc-diagnostic.md).

Los entornos y su herencia se configuran según la
[documentación de PlatformIO](https://docs.platformio.org/en/latest/projectconf/sections/env/options/advanced/extends.html).

## Volver al funcionamiento normal

Cerrar el monitor, compilar y **volver a cargar** el entorno normal; cambiar
solo el entorno del monitor no cambia el firmware instalado:

```powershell
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev --baud 115200
```

Tras EN/RESET, comprobar el arranque `[FISICO]`, los 30 segundos de espera,
el DHT22 y el ADC filtrado en sus turnos normales. El montaje físico REV3
actual usa divisor k=0,5 y `EFUSE_VREF` a 0 dB, con rango operativo 100–950 mV
y contraste puntual de 170–200 mV; ver [procedimiento físico](mq135-experimental.md#prueba-física-manual).
El arranque captura automáticamente tres muestras físicas nuevas y habilita
ppm si se acepta el lote; los comandos quedan para recuperación manual.
La demo no aporta esa referencia ni autoriza AO
directo. No hay que editar el entorno demo, intervalos ni `.vscode/`.

## Pruebas nuevas, pendientes de ejecución

[mq135_simulated_demo_test.cpp](../tests/mq135_simulated_demo_test.cpp) comprueba:

- Ausencia de ppm en cada instante anterior a 30000 ms, frontera exacta,
  inicio distinto de cero, jitter, rollover y turnos de 2000 ms sin duplicados.
- Primera muestra sin consumir fixtures durante calentamiento, repetición y
  referencia fija frente a los cambios posteriores.
- Etiquetas del calentamiento, muestras, ppm y errores; rechazo de procedencia
  desconocida, salida inconsistente antes de 30 segundos y formatos truncados.
- Aislamiento de APIs y referencias: AO físico rechazado en demo, demo rechazada
  en modo físico, ausencia de evidencia ficticia y bloqueo al regresar/resetear.
- ADC inválido, saturado, cero y señal pequeña; tensiones/referencias inválidas,
  NaN, infinito, overflow y underflow sin salida de ppm.

Ejecución opcional manual desde MSYS2 UCRT64 con g++ ya disponible (no instala
bibliotecas, no usa hardware y no se ha ejecutado aquí):

```bash
cd /d/telematicos/monitoreo-IoT-Maicol
mkdir -p /tmp/mq135-ca03
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/mq135_simulated_demo_test.cpp -o /tmp/mq135-ca03/demo-test.exe
/tmp/mq135-ca03/demo-test.exe
g++ -std=c++11 -Wall -Wextra -pedantic -DMQ135_SIMULATED_DEMO=1 firmware/tests/mq135_simulated_demo_test.cpp -o /tmp/mq135-ca03/demo-env-test.exe
/tmp/mq135-ca03/demo-env-test.exe
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/mq135_acquisition_test.cpp -o /tmp/mq135-ca03/acquisition-test.exe
/tmp/mq135-ca03/acquisition-test.exe
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/mq135_experimental_test.cpp -o /tmp/mq135-ca03/model-test.exe
/tmp/mq135-ca03/model-test.exe
```

## Hardware pendiente

Datos puntuales comunicados: VCC=4,89 V; AO–GND=0,34 V con GPIO34 desconectado;
AO–GND=0,42 V con GPIO34 conectado, medidos anteriormente. Alimentación desde
VIN/5V, sin divisor externo. Se registra VCC en la configuración física, pero
**no se usan 0,34 V ni 0,42 V como referencia ni como conversión ADC**. La
diferencia entre condiciones no identifica por sí sola su causa o una carga
estable. Tampoco prueba el máximo de AO ni la compatibilidad de GPIO34.

Quedan pendientes la verificación eléctrica completa y su evidencia,
compatibilidad/protección de AO, estabilidad de alimentación/carga efectiva,
conversión ADC contrastada, rango y mínimos útiles justificados,
acondicionamiento físico del sensor y un lote nuevo estable con procedencia.
Solo después se podrá fijar una referencia física válida y habilitar
deliberadamente ppm provisionales. HU-08 debe validar/calibrar frente a una
referencia real; 400 ppm asumidos no sustituyen esa referencia.

La demo permite verificar el flujo **espera → validación → estimador → salida
serie** en el ESP32 sin AO. La compilación, carga y observación manual del nuevo
modo aún deben aportar evidencia de ejecución; la validación física de CA-03
permanece pendiente.
