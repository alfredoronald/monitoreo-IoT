# T-1.5 / CA-03 — divisor comunicado y adaptación ADC REV3

Identificador: `docs/evidencias/t1-5/mq135-divisor-10k-rev3.md`.
Revisión preparada: **3**, divisor con ADC a 0 dB. Es un identificador de configuración,
no un resultado de prueba ni una afirmación sobre versiones cargadas antes.
Fuente: instalación comprobada y mediciones comunicadas por el responsable
en esta conversación. No se aportaron fecha de medición, fotografías,
tolerancias de las resistencias ni incertidumbre del instrumento.

| Elemento | Conexión / dato comunicado |
|---|---|
| MQ-135 VCC | VIN/5V del ESP32; aproximadamente 4,89 V |
| R1 | 10 kΩ entre AO y el nodo ADC |
| R2 | 10 kΩ entre el nodo ADC y GND |
| GPIO34 / ADC1_CH6 | Conectado al nodo R1–R2 |
| GND | Compartida entre ESP32 y MQ-135 |
| AO–GND al comprobar el divisor | 0,24 V con multímetro |
| GPIO34–GND al comprobar el divisor | 0,12 V con multímetro |
| Relación nominal | `10 / (10 + 10) = 0,50` |
| Relación observada | `0,12 / 0,24 = 0,50` |
| GPIO34–GND, contraste posterior a 0 dB | Fluctuación entre 0,17 y 0,20 V con multímetro |
| ADC posterior a 0 dB | Mediana física 511; `EFUSE_VREF`=191 mV; Vref eFuse=1058 mV |

La configuración utiliza `directAoConnection=false` y
`adcToAoVoltageRatio=0.5`. El modelo recibe `VAO=VADC/0.5`, reconstruida
desde la mediana ADC convertida a milivoltios; no recibe el voltaje atenuado
como si fuese AO.

## Alcance de la verificación

Se registra el divisor instalado y comprobado para el montaje VIN/5V y GND
común comunicado, sustituyendo AO directo. Bajo una salida AO limitada por
esa alimentación, `4,89 × 0,5 = 2,445 V` es un **límite calculado nominal**,
no una tensión máxima medida. El firmware marca `electricalVerified=true`
para este montaje; no certifica otras fuentes, fallos de componentes,
transitorios ni la operación con el ESP32 sin alimentación.

La instalación cambia la carga de AO. Ninguna referencia previa, sintética
o retrospectiva, se conserva: se necesitan tres muestras nuevas de REV3.
Los 0,24/0,12 V no fijan automáticamente `q_referencia`.

## Antecedentes de conversión ADC

El diagnóstico anterior de esta unidad informa TP no disponible, VREF eFuse
disponible, 1058 mV y método `EFUSE_VREF`. El contraste histórico entre
380 mV de multímetro y 392/398/414 mV del ESP32 era orientativo, con AO directo
y sin pares exactamente sincronizados. No acredita el nuevo tramo de 120 mV.

Con el divisor instalado y 11 dB se comunica **mediana ADC=70**, conversión
`EFUSE_VREF`=**196 mV** y multímetro GPIO34–GND=**120 mV**. La diferencia es
76 mV, orientativa sin incertidumbres/desfase aportados. No se resta ese valor
como corrección ni se declara su causa definitivamente comprobada.

La [guía ADC de Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html)
documenta aproximadamente 150–2450 mV para la atenuación máxima y **100–950 mV
para 0 dB**. Los 120 mV quedan dentro del segundo intervalo. Se adapta la
adquisición física a `ADC_0db` y ambas curvas a `ADC_ATTEN_DB_0` mediante
una selección común en `main.cpp`. No se modifica la resolución de 12 bits.

Antes del contraste nuevo, la conversión permanecía bloqueada. Los límites
100–950 mV preparados entonces describían el rango de desarrollo de la
atenuación, no un barrido validado en esta unidad.

## Contraste experimental puntual nuevo y decisión

Con **REV3**, divisor k=0,5, ADC1_CH6 de 12 bits, **0 dB** y mediana de 11
lecturas, el responsable comunica:

- Multímetro entre **GPIO34 y GND**: fluctuación de **170–200 mV**.
- Diagnóstico del mismo nodo: **mediana ADC=511**, **191 mV**, `EFUSE_VREF`.
- Vref eFuse: **1058 mV**. Sigue leyéndose del chip, sin fijarla como fallback.

**170 ≤ 191 ≤ 200 mV**: hay coincidencia cualitativa en el nivel de trabajo
con el montaje, nodo, atenuación y método correctos. Esto satisface el
contraste experimental puntual solicitado para el prototipo CA-03; se
marca `adcConversionVerified=true` con ese alcance exclusivamente.
No se establece una tolerancia de error ni se calcula error absoluto contra
un valor único de multímetro: este fluctuó y no se aportaron pares exactamente
sincronizados, desfase temporal conocido o incertidumbres. Ese registro más
preciso sigue pendiente para cuantificar exactitud, no para afirmarla ahora.
La decisión no equivale a calibración metrológica ni valida toda la escala.

El intervalo **170–200 mV** es el contraste puntual observado, no el rango
operativo. El guard existente admite ahora **100–950 mV inclusive**, rango
recomendado para el ESP32 a 0 dB por la
[guía ADC de Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html).
Esto permite observar variaciones experimentales sin afirmar que esos puntos
estén contrastados. El mínimo ADC=1 solo excluye cero; VAO mínima=**0,200 V**
se deriva de 100 mV / 0,5.
El dato 191 mV reconstruye **VAO=0,382 V**, cálculo y no medición adicional
de AO. Una lectura fuera de 100–950 mV se rechaza. Para cuantificar precisión
fuera del contraste puntual se necesitan pares sincronizados multímetro/ADC
en varios niveles del rango, repeticiones e incertidumbre del instrumento,
registrando tiempo, método, Vref, VCC y condiciones ambientales. No se han
aportado esos datos ni se añade un criterio de dispersión o estabilidad.

Identificador de conversión:
`EFUSE_VREF-ADC1_CH6-12bit-0dB-REV3-OPERATIVO-100-950mV`.
El firmware vuelve a consultar eFuse y el método efectivo; una conversión
nominal no medida continúa bloqueada para el estimador. Se conservan rechazo
de ADC cero/saturado, tensiones inválidas y resultados no finitos.
Las comprobaciones eléctricas del divisor son independientes de este contraste.
No se exige un patrón de CO₂ ni la calibración definitiva de HU-08.

## Resultado de esta edición

Configuración de adquisición preparada con verificación experimental puntual.
No queda un bloqueo de conversión documentada para lecturas válidas dentro
de 100–950 mV si el SDK mantiene `EFUSE_VREF`. Se conservan 30 s, mediana de
11 lecturas, periodo de 2000 ms, referencia de tres muestras, modelo GeorgK
y 400 ppm supuestos. El flujo actual captura automáticamente tres muestras
físicas nuevas tras el calentamiento y habilita emisión si se acepta el lote;
los comandos manuales siguen disponibles para recuperación. El contraste
511/191 no se reutiliza como referencia.
El responsable confirmó previamente referencia física REV3 aceptada y ppm
provisionales en el flujo manual. Esa evidencia no valida la precisión del
rango ampliado ni prueba este cambio. No se ejecutaron tests, build ni upload;
la ampliación y el inicio automático quedan pendientes de comprobación física.
