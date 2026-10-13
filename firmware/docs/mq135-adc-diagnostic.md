# Issue #11 — T-1.5: diagnóstico ADC físico a 0 dB

## Configuración preparada

ESP32-WROOM-32, GPIO34 / ADC1_CH6, resolución de **12 bits** y atenuación
física de **0 dB** para el nivel bajo observado. MQ-135 desde VIN/5V,
VCC aproximadamente 4,89 V, GND común. R1=10 kΩ entre AO y GPIO34;
R2=10 kΩ entre GPIO34 y GND. Divisor instalado y comprobado por el responsable:
Al comprobar el divisor se midieron AO=0,24 V y nodo GPIO34=0,12 V:
relación nominal y medida k=0,50. En el contraste posterior a 0 dB, el nodo
fluctuó entre 0,17 y 0,20 V; ADC mediano=511 y conversión `EFUSE_VREF`=191 mV.

El ADC mide **VADC**, posterior al divisor. El modelo usa **VAO=VADC/0.5**.
La adaptación/reconstrucción está registrada en la
[evidencia REV3](../../docs/evidencias/t1-5/mq135-divisor-10k-rev3.md).
El firmware anterior y sus diagnósticos a 11 dB son antecedentes, no la
configuración de esta versión preparada.

Una selección común en `main.cpp` se utiliza para:

- `analogSetPinAttenuation(GPIO34, ADC_0db)`.
- La curva diagnóstica: `esp_adc_cal_characterize(..., ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, ...)`.
- La curva destinada al estimador: misma unidad, atenuación y resolución.
- La conversión de la **misma mediana**, sin adquirir otra lectura para los mV.

Se conservan 30 segundos, 2000 ms y mediana de 11 lecturas. No cambia DHT22,
Wi-Fi, MQTT ni el comportamiento del entorno simulado.

## Evidencia comunicada

| Dato | Resultado y configuración comunicados |
|---|---|
| eFuse TP | No disponible |
| eFuse VREF | Disponible |
| Vref de calibración | 1058 mV, procedente de eFuse |
| Método efectivo | `EFUSE_VREF` |
| AO directo anterior | ADC aprox. 308–343 → 381–408 mV |
| Contraste anterior sin divisor | Multímetro 370 mV y luego 380 mV; muestras cercanas 392/398/414 mV |
| Con divisor y **11 dB** | Mediana ADC=70 → **196 mV**; multímetro GPIO34–GND=**120 mV** |
| Diferencia del contraste antiguo a 11 dB | 76 mV; no se aportaron incertidumbres ni desfase temporal |
| Contraste nuevo REV3 / **0 dB** | Mediana ADC=511 → **191 mV**, `EFUSE_VREF`, Vref=1058 mV; multímetro GPIO34–GND fluctuante entre **170–200 mV** |

La coincidencia antigua alrededor de 380 mV no valida el nuevo nivel de
120 mV. No se considera que los 76 mV sean un offset constante ni se
restan del resultado. El contraste nuevo a 0 dB aporta coincidencia puntual:
191 mV está dentro de 170–200 mV. No se aportaron pares exactamente
sincronizados ni incertidumbres; no permite cuantificar exactitud o error absoluto.

La [guía ADC de Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html)
recomienda **100–950 mV para el ESP32 a 0 dB**. El guard de tensión usa ahora
ese rango operativo inclusive. El intervalo **170–200 mV** sigue siendo solo
el contraste comunicado con multímetro. La curva real `EFUSE_VREF` convierte
la mediana; no se sustituye por una escala nominal. La ampliación no acredita
un barrido, linealidad ni precisión en toda la escala.

## Cómo observar la nueva lectura

Desde PowerShell, manualmente:

```powershell
Set-Location D:\telematicos\monitoreo-IoT-Maicol\firmware
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -e esp32dev --baud 115200
```

Cerrar el monitor antes de cargar. Con varios puertos, añadir el COM real:
`--upload-port COMx` al upload y `--port COMx` al monitor. Abrir el monitor,
pulsar EN/RESET y esperar al menos 30 segundos. Enviar con Enter:

```text
mq135 adc diagnostico
```

Debe informar la configuración preparada:

```text
ADC DIAGNOSTICO [FISICO]: GPIO34 / ADC1_CH6; resolucion=12 bits; atenuacion=0 dB
ADC eFuse TP: ...
ADC eFuse VREF: ...
ADC metodo efectivo para mV diagnosticos: ...; Vref de curva=... mV
MQ-135 ADC FILTRADO [FISICO]: muestras_internas=11; mediana=CODIGO; calculado=VALOR mV; tiempo=TIEMPO ms; metodo=METODO; DENTRO DEL RANGO OPERATIVO ADC; NO METROLOGICO
```

Este bloque es un **formato**, no un resultado de prueba. El comando muestra
datos guardados del arranque y el último turno, sin adquirir muestras extra ni
cambiar la cadencia. La etiqueta final del ejemplo solo aparece con
conversión disponible y valor dentro de 100–950 mV; fuera de ese alcance se
indica `FUERA DEL RANGO OPERATIVO ADC O CONVERSION NO DISPONIBLE`.
El diagnóstico distingue el rango operativo del contraste puntual de 170–200 mV;
estar dentro del rango operativo no significa que esa tensión haya sido contrastada.
No se exige repetir exactamente 511/191 ni se reutiliza el código 70 a 11 dB.

Medir **GPIO34–GND**, manteniendo el divisor, y comparar con `calculado`.
Registrar tiempo/desfase, ADC, mV, método, Vref y VCC/AO disponibles en varios
turnos cercanos. El nuevo contraste comunicado es VADC=170–200 mV frente a
191 mV calculados. Estos últimos reconstruyen VAO=0,382 V mediante k=0,5;
no equivalen a una medición adicional de AO. No intercambiar los nodos ni
mezclar logs a 0 y 11 dB.

Para evaluar precisión fuera de 170–200 mV, registrar pares sincronizados de
multímetro GPIO34–GND y mV `EFUSE_VREF` en varios niveles bajos, intermedios y
altos dentro de 100–950 mV, con repeticiones. Anotar resolución/incertidumbre
del instrumento, tiempo/desfase, ADC mediano, Vref, método, VCC y condiciones
ambientales. Comparar también AO y el nodo del divisor para comprobar k cuando
esas mediciones estén disponibles. No se aportan esos pares en esta edición;
los códigos comunicados 763, 887, 1006 y 1171 no se convierten en mediciones
de mV inventadas ni se usan para fijar una referencia.

## Verificaciones separadas

`electricalVerified=true` refleja el divisor instalado y comprobado en el
montaje VIN/5V comunicado. No certifica transitorios, fallos ni otra fuente.
La atenuación y la eFuse no sustituyen la protección del GPIO.

`adcConversionVerified=true` refleja exclusivamente la coincidencia
experimental puntual 191 mV dentro de 170–200 mV, en REV3 y con el método y
atenuación correctos. El rango operativo es **100–950 mV**, con VAO mínima=0,200 V,
por recomendación técnica de Espressif, no por un nuevo contraste físico.
No se declara calibración metrológica, precisión cuantificada ni validación
de todos los puntos de ese intervalo o de toda la escala. ADC cero, saturado,
fuera de rango, tensiones y resultados matemáticos inválidos siguen rechazados.

TP y VREF se consultan independientemente. El método real lo devuelve
`esp_adc_cal_characterize`; disponer de una eFuse no prueba su uso si el
SDK la deshabilita. `EFUSE_VREF` utiliza la referencia de fábrica del chip;
los 1058 mV no son VCC ni se introducen como fallback externo medido.

El fallback nominal de 1100 mV, si el SDK lo usa, se identifica como
`VREF_NOMINAL_NO_MEDIDA` y sirve solo para diagnóstico. No habilita el
estimador. El diagnóstico no eleva una bandera de verificación.

Con esta configuración, el arranque físico solicita automáticamente una
referencia tras el calentamiento, captura tres muestras nuevas válidas y
habilita ppm solo si el lote es aceptado. El diagnóstico no interrumpe ese
flujo. Si falla, no hay referencia inventada, ppm ni reintento automático;
se conservan `mq135 referencia iniciar` y `mq135 ppm habilitar` para recuperación
manual. Ver [procedimiento CA-03](mq135-experimental.md#prueba-física-manual).
La calibración definitiva de CO₂ en HU-08 sigue pendiente y no es condición
de esa estimación provisional.

En la demo, el comando conserva su respuesta `inactivo [SIMULADO]` y no
consulta eFuse ni ADC físico. No se ejecutaron tests, build, upload, commit
ni push durante esta edición. El flujo manual físico fue validado según el
responsable; este inicio automático todavía requiere comprobación al reiniciar.
