# T-1.5 / Issue #11: estimación provisional física del MQ-135

La captura de Rs de T-8.1 tiene comandos `mq135 calibracion iniciar/estado/detener`.
Su [procedimiento y evidencia](../../docs/evidencias/t8-1/README.md) describen
el bloqueo estricto por RL efectiva sin verificar, la captura de Rs estimada
con carga nominal y topologia supuesta, la omisión temporal de CO2 y la
recuperación de la referencia provisional al detener. El promedio Rs no es Ro.

## Alcance actual de CA-03

Rama: `feature/11-lectura-mq135`. Entorno físico: `esp32dev`.

El objetivo es emitir ppm provisionales desde lecturas **físicas nuevas**.
Se mantienen GPIO34 / ADC1_CH6, 12 bits, **0 dB** para la señal baja, mediana de 11 lecturas,
calentamiento de software de 30000 ms y adquisición cada 2000 ms.
El DHT22, Wi-Fi, MQTT y el entorno `esp32dev-mq135-demo` no cambian.

La referencia usa **3 turnos físicos válidos**, sin exigir 20 muestras ni un
límite del 5 %. Esta ventana breve es una elección de implementación, no un
criterio original de aceptación ni una calibración. No se implementa HU-08.

**El responsable confirmó CA-03 físico con referencia REV3 y ppm reales en
el flujo manual. El nuevo inicio automático queda pendiente de comprobar
al reiniciar el ESP32.**
El divisor 10 kΩ + 10 kΩ instalado y comprobado sustituye AO directo.
El contraste nuevo de 191 mV frente a 170–200 mV de multímetro respalda una
verificación experimental puntual de conversión a 0 dB. El rango operativo
es 100–950 mV según Espressif, distinto del intervalo contrastado de 170–200 mV;
cada arranque debe capturar una referencia nueva. No se utiliza
la demo ni un log retrospectivo para completar una referencia física.

## Mediciones y antecedentes comunicados

Montaje actual: ESP32-WROOM-32, MQ-135 Flying-Fish, VCC desde VIN/5V,
GND común, R1=10 kΩ entre AO y GPIO34 y R2=10 kΩ entre GPIO34 y GND.
Relación nominal y medida k=0,50; DO sigue desconectado.
Estas mediciones fueron aportadas por el responsable; esta edición no ejecuta
firmware ni inventa fechas, incertidumbres o mediciones nuevas.

| Dato | Resultado y condiciones comunicadas |
|---|---|
| VCC–GND | 4,89 V; medición puntual, no máximo garantizado de alimentación |
| Divisor actual REV3 | R1=10 kΩ AO–nodo; R2=10 kΩ nodo–GND; GPIO34 en el nodo |
| AO–GND al comprobar el divisor | 0,24 V con multímetro |
| GPIO34–GND al comprobar el divisor | 0,12 V con multímetro |
| Relación del divisor | k=0,50 nominal y medida; GND compartida |
| GPIO34–GND, contraste nuevo a 0 dB | Fluctuación de 0,17–0,20 V con multímetro |
| Diagnóstico nuevo a 0 dB | Mediana física 511 → 191 mV, `EFUSE_VREF`; Vref=1058 mV |
| AO–GND con GPIO34 desconectado | 0,34 V |
| AO–GND con GPIO34 conectado | 0,42 V, medición anterior |
| AO–GND posterior | 0,37 V; conexión de GPIO34 en ese instante no especificada |
| GPIO34–GND antes del divisor | 0,38 V con multímetro |
| mV cercanos a esa medición | 392, 398 y 414 mV; sin pares exactamente sincronizados |
| Intervalo histórico observado de AO directo | 0,27–0,44 V; no establece su máximo posible |
| ADC anterior | Aproximadamente 308–343, convertido a 381–408 mV |
| eFuse | TP no disponible; VREF disponible; Vref=1058 mV |
| Método de conversión observado | `EFUSE_VREF`; los 1058 mV no son VCC ni un fallback externo medido |

La comparación con el multímetro coincide aproximadamente en el tramo
observado, pero no valida toda la escala ni mide el error de conversión:
faltan pares sincronizados e incertidumbres. Los voltajes puntuales no
constituyen una referencia estable. El contraste antiguo no acredita la
conversión del nivel de 120 mV posterior al divisor. El contraste nuevo a
0 dB se documenta abajo con alcance puntual; no valida retrospectivamente
esa lectura antigua.

Antes del filtro se comunicaron pares ADC→mV `330→398`, `384→440`,
`324→393`, `305→378`, `389→443`, `320→390` y `318→388`, incluso
después de más de 8 minutos. El lote histórico de 20 muestras tuvo dispersión
aproximada de q del 20 %. Ese antecedente explica el filtro, **no impone un
umbral de aceptación actual** y no se reutiliza como referencia.

Se comunicaron 215 comprobaciones sintéticas aprobadas y build de
`esp32dev` exitoso antes de estas modificaciones. No se atribuyen esos
resultados a la versión simplificada. En esta edición no se ejecutan tests,
lint, build, upload, commit ni push.

## Mediana en la adquisición física

Cada turno posterior al calentamiento toma 11 `analogRead(GPIO34)`
consecutivos, sin `delay`. [mq135_adc_filter.h](../include/mq135_adc_filter.h)
valida todos los códigos, ordena una copia y devuelve el sexto elemento.
No promedia tensiones ni conserva una ventana entre turnos.

Una lectura interna cero, saturada en 4095, fuera de rango o inferior al
mínimo útil aplicable invalida el turno completo. No se reutiliza la mediana
anterior. Las 11 lecturas internas constituyen **una sola muestra**.

La mediana representativa se convierte mediante `esp_adc_cal_raw_to_voltage`.
Diagnóstico, captura y estimación utilizan el mismo código. Las curvas
diagnóstica y destinada al estimador mantienen sus bloqueos separados:
un fallback nominal diagnóstico nunca habilita el estimador.
La unidad comunicada utiliza `EFUSE_VREF`; no se fija 1058 mV por código.

El filtro puede reducir picos breves, pero no garantiza precisión, estabilidad
térmica o compatibilidad eléctrica. La espera de 30 segundos es software,
no sustituye el acondicionamiento físico del sensor descrito por el fabricante.

## Referencia provisional: tres muestras nuevas

Después de 30 segundos y con configuración eléctrica/conversión acreditadas:

1. El firmware solicita automáticamente la referencia al finalizar el primer
   turno posterior al calentamiento. En recuperación manual, enviar
   `mq135 referencia iniciar`. No se reutiliza la muestra previa a la solicitud.
2. Recoger las próximas **3 medianas físicas válidas**, separadas por 2000 ms.
   Nominalmente hay 4000 ms entre primera y tercera muestra; desde la solicitud
   puede tardar hasta unos 6000 ms, más los retrasos reales del loop.
3. Convertir cada mediana a VADC y reconstruir VAO según el montaje documentado.
4. Calcular `q_i = (Vc - VAO_i) / VAO_i`.
5. Fijar `q_referencia = (q_1 + q_2 + q_3) / 3`.
6. Solo si el lote automático es aceptado, habilitar emisión mediante la misma
   API protegida que usa `mq135 ppm habilitar`. Las ppm comienzan en el siguiente
   turno programado; no se adquiere una muestra adicional para publicarlas.

El arranque realiza **un único intento automático**. Con cadencia ideal, la
solicitud ocurre alrededor de 30 s, las tres muestras a 32/34/36 s y la primera
salida de ppm a 38 s; los tiempos reales dependen del loop. No hay ppm antes
del calentamiento ni durante la captura.

Una muestra inválida aborta la captura, sin rellenarla con valores anteriores;
otro intento exige una orden manual nueva o reinicio. No se inventa referencia
ni se habilitan ppm tras un fallo. Se mantienen validaciones de ADC, tensión
útil, contexto, procedencia y resultados finitos. No se exige estabilidad
definitiva, un ambiente con CO₂ medido ni la calibración de HU-08.

`NO_SPREAD_LIMIT=-1` indica explícitamente ausencia de criterio de dispersión;
no se aceptan NaN/infinito como parámetros. La dispersión del registro puede
consultarse como dato **informativo**, sin rechazar por superar el 5 %.
La API matemática conserva las comprobaciones de límites explícitos usadas
por las pruebas históricas; el firmware CA-03 selecciona sin límite.

La referencia queda fija en RAM durante la sesión. Las lecturas posteriores
no la recalculan. Reinicio, invalidación explícita o reaplicación de
configuración exigen otra captura. Al reiniciar se repite el inicio automático;
tras invalidación manual se requiere recuperación manual. Se conservan ADC, VAO, Vc disponible,
tiempos y condiciones DHT disponibles en el registro existente.
La [plantilla CSV](mq135-reference-capture-template.csv) usa tres filas;
no hay exportación automática ni persistencia NVS.

## Modelo y salida

Se conserva el modelo GeorgK/MQ135 normalizado:

```text
q = (Vc - VAO) / VAO
CO2 provisional = 400 * (q / q_referencia)^(-2.769034857)
```

A=116.6020682 se cancela en la normalización. La cancelación de la carga
efectiva presupone que no cambia entre referencia y lecturas posteriores.
**400 ppm es un supuesto de desarrollo**, nunca una concentración medida.
T-1.5 demuestra una estimación provisional; selectividad, precisión,
compensación ambiental y calibración definitiva corresponden a HU-08.

Con referencia aceptada y emisión habilitada, la salida MQ-135 cada 2000 ms es:

```text
MQ-135 ADC: XXX
CO2 estimado provisional: XXX.XX ppm
```

Sin requisitos satisfechos se muestra ADC y un motivo breve de bloqueo,
sin ppm numéricas. Durante calentamiento solo se muestra el estado.
La salida del DHT22 conserva su funcionamiento independiente.

`mq135 adc diagnostico` muestra TP, VREF, método efectivo, configuración ADC
y el **último** turno filtrado con mediana, mV, tiempo y estado.
No adquiere una muestra extra ni modifica la cadencia. No se repiten estos
detalles en cada turno físico. `mq135 estado` conserva el resumen de captura
y referencia fija. Ver [diagnóstico](mq135-adc-diagnostic.md).

## Divisor físico y ADC a 0 dB — REV3

La evidencia se registra en
[mq135-divisor-10k-rev3.md](../../docs/evidencias/t1-5/mq135-divisor-10k-rev3.md),
con origen, mediciones, alcance y datos pendientes. Revisión de circuito **3**;
identificador `docs/evidencias/t1-5/mq135-divisor-10k-rev3.md`.

```text
AO -- R1=10 kΩ --+-- GPIO34 / ADC1_CH6
                |
             R2=10 kΩ
                |
               GND común

k = 10 / (10 + 10) = 0,50
VAO = (mV del ADC / 1000) / 0,50
```

AO=0,24 V y GPIO34=0,12 V respaldan la relación instalada. No se usan para
fijar q retrospectivamente. Las tres muestras de referencia deben ser nuevas
y posteriores al calentamiento con REV3.

En `electricalConfiguration()` se configura:

| Campo | Valor / alcance |
|---|---|
| `directAoConnection`, `adcToAoVoltageRatio` | `false`, `0.5` |
| `measuredVcV` | 4,89 V comunicado; contrastar para la sesión |
| `electricalVerified` | `true`, divisor instalado y comprobado para este montaje VIN/5V y GND común |
| `electrical.circuitRevision` | 3; cualquier cambio exige otra revisión y referencia |
| `verificationEvidenceId` | Documento REV3 indicado arriba |
| `electrical.circuitDescription` | Conexiones y valores reales del divisor |
| `electrical.adcConversionId` | `EFUSE_VREF-ADC1_CH6-12bit-0dB-REV3-OPERATIVO-100-950mV` |
| `adcConversionVerified` | `true`; contraste experimental puntual 511 → 191 mV dentro de 170–200 mV, no metrológico |
| `minimumAdcMilliVolts`, `maximumAdcMilliVolts` | 100, 950 mV, inclusivos: rango operativo recomendado para ESP32 a 0 dB, no intervalo contrastado en esta placa |
| `electrical.minimumUsableAdcCode` | 1; excluye cero, sin certificar precisión |
| `electrical.minimumUsefulVaoV` | 0,200 V, derivado de 100 mV / 0,5 |
| Referencia | Tres muestras nuevas, `NO_SPREAD_LIMIT`, media de q fija |

Bajo una AO limitada por la alimentación del montaje, 4,89×0,5=2,445 V es
un límite **calculado nominal**, no un máximo medido. El divisor protege la
entrada en el montaje comunicado; no se verificaron fallos, transitorios,
tolerancias extremas ni otra fuente. La atenuación ADC no es protección.

El diagnóstico anterior utilizó eFuse VREF=1058 mV y sigue consultándola al
arranque; no se fija por código ni se usa VCC como Vref. El parámetro
`measuredAdcVrefMilliVolts=0` permanece así porque no hay fallback externo
medido. El [diagnóstico](mq135-adc-diagnostic.md) convierte la misma mediana
incluso sin referencia; la consulta no inicia captura ni habilita ppm.

### Conversión adaptada y contraste experimental puntual

Con el divisor y 11 dB se comunicó mediana ADC=70, conversión `EFUSE_VREF`=196 mV
y multímetro GPIO34–GND=120 mV. Diferencia: 76 mV; sin incertidumbres/desfase
aportados, no acredita un offset constante ni su causa. No se resta 76 mV.

La [guía ADC de Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html)
indica aproximadamente **100–950 mV a 0 dB**; los 120 mV quedan dentro de ese
intervalo. La lectura física usa `ADC_0db` y ambas llamadas a
`esp_adc_cal_characterize` utilizan su equivalente `ADC_ATTEN_DB_0`.
Una selección común y una comprobación de compilación en `main.cpp` conservan
la coherencia; no cambia la resolución de 12 bits ni se recompone una escala
lineal nominal del ADC.

El responsable aporta ahora **mediana ADC=511 → 191 mV** con `EFUSE_VREF`,
Vref=1058 mV y multímetro **GPIO34–GND=170–200 mV** fluctuantes en REV3 a 0 dB.
191 mV está dentro de ese intervalo: satisface el contraste cualitativo
puntual solicitado para CA-03, con el montaje/nodo/atenuación/método correctos.
Se conserva `adcConversionVerified=true` como contraste experimental puntual,
sin ampliar el alcance de esa evidencia ni cambiar la verificación eléctrica.
El intervalo 170–200 mV describe las mediciones comunicadas, no el rango operativo
del ADC. No demuestra linealidad en todos sus puntos.

No se aportaron pares exactamente sincronizados, desfase conocido o
incertidumbres. No se afirma un error de conversión cuantificado ni una
calibración metrológica. Esos datos siguen pendientes para evaluar precisión;
HU-08 y un patrón de CO₂ no son requisitos de la estimación provisional.
191 mV reconstruye VAO=0,382 V; es un cálculo, no una medición nueva de AO.
El diagnóstico 511/191 no fija ni completa retrospectivamente q_referencia.

El guard existente admite **100–950 mV inclusive**, rango recomendado por
Espressif para el ESP32 a 0 dB; rechaza valores inferiores o superiores.
Se compara la conversión real `esp_adc_cal_raw_to_voltage` de la mediana,
no una aproximación nominal del código ADC. VAO se reconstruye con k=0,5:
este rango corresponde a 0,200–1,900 V de AO calculadas, no medidas nuevas.
La ampliación permite observar variaciones provisionales; no acredita precisión
fuera del contraste puntual. Para evaluarla se requieren pares sincronizados
GPIO34–GND / mV calculados en varios niveles bajos, intermedios y altos del rango,
con repeticiones, resolución/incertidumbre del multímetro, tiempo, ADC mediano,
método, Vref, VCC y condiciones ambientales registrados.
Saturación/rango ADC y límite eléctrico del GPIO siguen siendo independientes.
No se cambia automáticamente la atenuación ni se añade otro filtro.

El inicio automático utiliza `startReference()` y `enableEmission()` sin
modificar sus verificaciones. Se conservan `mq135 referencia iniciar` y
`mq135 ppm habilitar` para recuperación manual, sin permiso de compilación redundante.
La demo no proporciona evidencia ni referencia a este montaje.

## Prueba física manual

Con el divisor REV3 instalado y la configuración actual, ejecutar manualmente
estos comandos; no se ejecutaron durante esta edición:

```powershell
Set-Location D:\telematicos\monitoreo-IoT-Maicol\firmware
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev --baud 115200
```

Cerrar el monitor antes de cargar; si hay varios puertos, usar el COM real con
`--upload-port COMx` y `--port COMx`. Con el monitor abierto, pulsar EN/RESET,
**sin enviar comandos de control**. Comprobar calentamiento durante 30 s,
captura automática de tres muestras y emisión provisional cada 2000 ms una
vez completada la referencia. Esperar nominalmente unos 38 s desde el arranque.
La referencia debe permanecer fija; no se calcula de nuevo en cada lectura.

Si se desea consultar el diagnóstico, enviar con Enter:

```text
mq135 adc diagnostico
```

La consulta es opcional: debe indicar **0 dB**, método `EFUSE_VREF`, Vref
leída del chip y el último ADC mediano/mV. No requiere reproducir exactamente
511/191. El estimador solo admitirá 100–950 mV; si se usa fallback nominal,
falta conversión válida o el ADC/tensión es inválido, seguirá bloqueado.
Para nuevos contrastes, comparar los mV con GPIO34–GND, no con AO.

### Recuperación manual

Si el intento automático falla, no se reintenta ni se publican ppm. Consultar
`mq135 estado` o `mq135 adc diagnostico` y resolver el motivo de rechazo.
Después del calentamiento, iniciar otra captura manual:

```text
mq135 referencia iniciar
```

Esperar a completar tres muestras físicas (nominalmente hasta unos 6 segundos
desde el comando). Con referencia aceptada, enviar:

```text
mq135 ppm habilitar
```

Esas dos órdenes solo son necesarias para recuperación manual. `mq135 estado`,
`mq135 adc diagnostico` y `mq135 ayuda` son consultas y no interrumpen el inicio
automático. Las órdenes de iniciar/cancelar/invalidar referencia o de
habilitar/deshabilitar ppm toman el control manual y detienen la automatización
del arranque, incluso si la orden resulta rechazada; un reinicio vuelve a activarla.
Si se solicita emisión antes de completar la referencia, se rechaza sin
activar una habilitación pendiente; repetir el comando tras aceptarse el lote.

Verificar salida física cada 2000 ms, referencia fija y ausencia de ppm ante
ADC/tensión/cálculo inválidos. Desactivar con `mq135 ppm deshabilitar`;
para otra referencia usar `mq135 referencia invalidar` y volver a iniciar.
Con la configuración actual no queda bloqueo de conversión pendiente en el
rango operativo documentado. Antes de la captura siguen activas la
referencia ausente y la emisión deshabilitada; el arranque automático o las
dos órdenes manuales resuelven esas condiciones cuando las muestras son válidas. Fuera de
100–950 mV o sin conversión válida, no se emiten ppm. No hay una orden que
certifique hardware o eluda las protecciones.

**CA-03 PASS requiere una captura física aceptada y emisión real de ppm
provisionales documentadas.** La demo, las mediciones retrospectivas y las
pruebas sintéticas no sustituyen esa evidencia.

Las suites existentes se conservan; las pruebas de adquisición comprueban
100 y 950 mV inclusivos, rechazo fuera de rango y de valores no finitos,
ADC cero/saturado y GeorgK con referencia fija fuera del contraste puntual.
No se ejecutaron. Al finalizar solo se comprueban `git diff --check` y
`git status --short`.
