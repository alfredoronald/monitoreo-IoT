# T-1.3 / T-1.5 — Pinout ESP32, DHT22 y MQ-135

## Montaje físico actual comunicado

ESP32-WROOM-32 y MQ-135 Flying-Fish. El responsable comunica un **divisor
instalado y comprobado de dos resistencias de 10 kΩ**, sustituyendo la
conexión AO directo de las lecturas anteriores. VCC desde VIN/5V,
aproximadamente 4,89 V; GND común; DO desconectado.

| Componente | Terminal | Conexión actual |
|---|---|---|
| ESP32 | Alimentación | USB, según el montaje comunicado |
| DHT22 | VCC / + | 3V3 del ESP32 |
| DHT22 | DATA | GPIO23 |
| MQ-135 | VCC | VIN/5V del ESP32 |
| MQ-135 | AO | R1=10 kΩ entre AO y el nodo ADC |
| Divisor | R2=10 kΩ | Entre el nodo ADC y GND |
| ESP32 | GPIO34 / ADC1_CH6 | Nodo común R1–R2 |
| ESP32 y sensores | GND | Compartida |
| MQ-135 | DO | Sin conectar |

La presencia/valor de pull-up del DHT22 no se especificó; esta edición no
modifica el DHT22 ni añade componentes a su circuito. Los GPIO son señales,
no posiciones del conector; verificar la serigrafía de la placa.

```text
USB ------------------------------- ESP32
ESP32 3V3 ------------------------- DHT22 VCC
DHT22 DATA ------------------------ GPIO23
ESP32 VIN/5V ---------------------- MQ-135 VCC

MQ-135 AO -- R1=10 kΩ --+----------- GPIO34 / ADC1_CH6
                       |
                    R2=10 kΩ
                       |
                      GND

GND ESP32, MQ-135 y DHT22: común
```

## Mediciones y reconstrucción

| Magnitud | Resultado comunicado |
|---|---|
| VCC del MQ-135 | Aproximadamente 4,89 V |
| AO–GND al comprobar el divisor | 0,24 V |
| GPIO34–GND al comprobar el divisor | 0,12 V |
| Relación nominal y medida | k=0,50 |
| Diagnóstico con divisor y 11 dB | Mediana ADC=70; `EFUSE_VREF`=196 mV |
| Contraste posterior REV3 a 0 dB | GPIO34–GND fluctuó entre 0,17 y 0,20 V; mediana ADC=511; `EFUSE_VREF`=191 mV |

`VADC=VAO×k`; el firmware reconstruye `VAO=VADC/0.5`. No utiliza la
medición puntual para establecer q de referencia. El divisor carga AO,
por lo que se necesitan tres muestras nuevas con este montaje definitivo.
La cancelación de carga efectiva del modelo presupone que se mantenga fija.

Antes del divisor se comunicaron 0,34 V con GPIO34 desconectado, 0,42 V
conectado, 0,37 V posterior sin conexión especificada y 0,38 V en GPIO34.
El intervalo 0,27–0,44 V observado anteriormente no establece un máximo
de AO. Las [fotografías históricas](../../docs/T-1.3-breadboard-circuit-evidence.md)
corresponden al antecedente de T-1.3; no prueban el montaje nuevo.

## Configuración de firmware preparada

GPIO34 es ADC1_CH6 y solo admite entrada; no sirve para DATA del DHT22.
Se mantiene ADC1 para evitar la dependencia de ADC2 con Wi-Fi. El DHT22
conserva GPIO23, lecturas y validaciones independientes.

Revisión preparada **3**: divisor 10 kΩ + 10 kΩ y ADC de **12 bits / 0 dB**.
La adquisición y ambas curvas `esp_adc_cal` utilizan la misma atenuación.
Se conservan calentamiento de 30 segundos, mediana de 11 lecturas y periodo
de 2000 ms. No se agrega un filtro ni se cambia Wi-Fi/MQTT.

La unidad comunicada dispone de VREF eFuse=1058 mV, TP no disponible.
El método efectivo se vuelve a consultar al arrancar; Vref no es VCC del
MQ-135. Se conserva rechazo de cero, saturación y valores inválidos.
El [diagnóstico](mq135-adc-diagnostic.md) muestra los detalles mediante comando.

## Alcance de la verificación

La instalación comprobada respalda `electricalVerified=true` para el
montaje VIN/5V y GND común comunicado. Bajo AO limitada por esa alimentación,
4,89×0,5=2,445 V es un límite nominal **calculado**, no medido. No se
certifican fallos de componentes, transitorios o uso de otras fuentes.
La atenuación ADC no protege eléctricamente GPIO34.

La discrepancia antigua a 11 dB fue 196−120=76 mV, sin incertidumbres/desfase
aportados. El nuevo contraste a **0 dB** es ADC=511 → **191 mV**, dentro del
intervalo **170–200 mV** observado en GPIO34–GND. Se marca
`adcConversionVerified=true` para este uso experimental puntual; esa evidencia
sigue limitada a 170–200 mV. El guard operativo admite 100–950 mV inclusive.
No se cuantifica precisión ni se declara calibración metrológica o validación
en toda la escala. Los 100–950 mV de la
[guía ADC de Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html)
son el rango operativo recomendado a 0 dB, no un rango contrastado en esta placa.
Saturación/rango ADC y límite eléctrico del GPIO son condiciones distintas.

La [evidencia REV3](../../docs/evidencias/t1-5/mq135-divisor-10k-rev3.md)
recoge montaje, mediciones y pendientes. La configuración actual permite
capturar referencia de tres muestras nuevas y después habilitar ppm
provisionales con 400 ppm supuestos. No exige HU-08 ni usa datos simulados.
El [entorno demo](mq135-simulated-demo.md) permanece separado y sin cambios.

## Referencias

- [ESP32-WROOM-32: pines y límites eléctricos](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html).
- [Espressif: ADC y calibración](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html).
- [Winsen: manual MQ135](https://www.winsen-sensor.com/d/files/manual/mq135.pdf).
