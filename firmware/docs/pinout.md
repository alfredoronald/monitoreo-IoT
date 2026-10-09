# T-1.3 — Pinout del circuito ESP32, DHT22 y MQ-135

## 1. Alcance y origen de los datos

- **Proyecto:** monitoreo-IoT.
- **Issue:** [#9 — Circuito en protoboard](https://github.com/alfredoronald/monitoreo-IoT/issues/9).
- **Rama:** `docs/9-circuito-esp32`.
- **Montaje:** completado según la confirmación del responsable comunicada para esta tarea.
- **Informe:** [Evidencia de T-1.3](../../docs/T-1.3-breadboard-circuit-evidence.md).

Los GPIO indicados a continuación corresponden al montaje comunicado. El responsable confirmó el cumplimiento de los cuatro criterios de aceptación, incluida la medición con multímetro de una tensión no superior a 3,3 V en la entrada analógica. También confirmó la alimentación y la instalación de un divisor en AO. No se comunicaron el valor numérico medido ni los valores de sus resistencias.

Las recomendaciones se distinguen de las conexiones confirmadas por el responsable y de las observaciones de las fotografías reales. Los valores propuestos anteriormente para un divisor no se presentan como valores instalados.

## 2. Conexiones reales comunicadas

| Componente | Terminal | Conexión / función | Información disponible |
|---|---|---|---|
| DHT22 | DATA | GPIO23 del ESP32 | GPIO indicado para el montaje. Comunicación digital bidireccional. |
| MQ-135 | AO | Hacia GPIO34 del ESP32, ADC1_CH6, mediante divisor según confirmación del responsable | Los valores de las resistencias no fueron comunicados; el divisor externo no se distingue en las fotografías. |
| ESP32 | Alimentación | USB | Confirmado por el responsable; cable USB visible en la foto general. |
| DHT22 | VCC / + | 3V3 del ESP32 | Confirmado por el responsable. |
| MQ-135 | VCC | VIN/5V del ESP32, alimentado por USB | Confirmado por el responsable. VIN/5V se utiliza como alimentación del sensor, no como entrada de otra fuente. |
| ESP32 y sensores | GND | Referencia común necesaria para las señales | La distribución física de tierra y su comprobación de continuidad no fueron detalladas. |
| DHT22 | Pull-up de DATA | Presencia y valor no especificados | No se afirma que exista una resistencia externa o integrada. |
| MQ-135 | Protección de AO | Divisor de voltaje confirmado por el responsable | Valores, ubicación y disposición de las resistencias no detallados. La fotografía no permite verificarlo. |
| MQ-135 | DO | Sin cable visible en la fotografía del módulo | No se utiliza para la adquisición analógica documentada. |

Los nombres GPIO23 y GPIO34 identifican señales del ESP32; no son números de posición del conector ni coordenadas de la protoboard. La posición física se debe identificar con la serigrafía y el pinout de la placa utilizada.

## 3. Esquema de señales comunicado

```text
USB -------------------------------- ESP32
ESP32 3V3 --------------------------- DHT22 VCC / +
ESP32 VIN/5V ------------------------ MQ-135 VCC
DHT22 DATA -------------------------- GPIO23

MQ-135 AO -- [divisor confirmado;
              resistencias sin
              valores comunicados] -- GPIO34 / ADC1_CH6

GND ESP32 y GND sensores: referencia común requerida.
```

El divisor se incluye por confirmación del responsable. En las fotografías se aprecia el cable de AO hacia GPIO34 sin un divisor externo identificable; por ello no se atribuyen a las imágenes la verificación de sus resistencias ni de su ubicación. Este esquema no es una instrucción para conectar AO directamente al ESP32.

## 4. Justificación de los GPIO

- **GPIO23 / DHT22:** admite entrada y salida, necesarias para iniciar la comunicación con el sensor y recibir su respuesta.
- **GPIO34 / MQ-135:** corresponde a **ADC1_CH6** en el ESP32 clásico. Solo admite entrada y no dispone de pull-up/pull-down internos habilitables por software. Es adecuado para adquirir una señal analógica acondicionada, pero no para DATA del DHT22.
- **ADC1:** evita la limitación del ADC2, compartido con WiFi en el ESP32 clásico. Esta elección permite conservar la adquisición del MQ-135 cuando se implemente la comunicación WiFi/MQTT.

En la revisión estática, `firmware/src/main.cpp` todavía contiene únicamente el arranque serie; no implementa lecturas de sensores. Este documento no modifica ni demuestra la implementación de los GPIO en firmware.

## 5. Alimentación y protección: recomendaciones

Estas indicaciones son recomendaciones para revisar el circuito real; no describen componentes instalados sin confirmación.

| Elemento | Recomendación |
|---|---|
| ESP32 | Usar una ruta de alimentación admitida por la placa. No unir fuentes externas a VIN/5V simultáneamente con USB sin verificar el circuito de alimentación. |
| DHT22 | Para un sensor compatible con 3,3 V, alimentar desde 3V3 y mantener DATA referida a 3,3 V. Identificar los terminales del modelo real. |
| Pull-up DHT22 | Verificar si el módulo incorpora pull-up. Si necesita una externa, usar aproximadamente 4,7–5,1 kΩ entre DATA y 3V3, con cables cortos. |
| MQ-135 | Alimentar el calefactor a 5 V según el fabricante; no alimentarlo desde un GPIO ni desde 3V3. Verificar capacidad de fuente y cableado. |
| Tierra | Conectar GND del ESP32, GND del DHT22 y GND del MQ-135 a una referencia común; comprobar cortes de rieles en la protoboard. |
| AO / GPIO34 | Si AO puede superar 3,3 V, incorporar un divisor o acondicionamiento adecuado antes del GPIO. Una lectura puntual segura no garantiza el máximo de AO en otras condiciones. |
| DO del MQ-135 | No se necesita para esta adquisición analógica. No conectarlo al ESP32 sin comprobar sus niveles eléctricos. |

Un divisor recomendado se diseña con una resistencia superior entre AO y el nodo ADC y una resistencia inferior entre ese nodo y GND:

```text
AO -- Rsuperior --+-- GPIO34
                 |
              Rinferior
                 |
                GND
```

`VADC = VAO × Rinferior / (Rsuperior + Rinferior)`.

Los valores deben dimensionarse considerando el máximo de AO, las tolerancias y el rango de adquisición del ADC. Para T-1.5 deberán registrarse los valores efectivamente instalados y su efecto en la conversión y calibración. La instalación del divisor fue confirmada, pero no se asignan valores a sus resistencias porque no fueron comunicados.

## 6. Comprobación eléctrica comunicada

| Dato | Registro |
|---|---|
| Magnitud | Tensión en la entrada analógica utilizada por el MQ-135. |
| Punto | GPIO34 respecto de GND. |
| Instrumento exigido por la Issue | Multímetro. |
| Resultado comunicado | Criterio de tensión máxima de 3,3 V cumplido según confirmación del responsable. |
| Valor numérico | No comunicado; no se inventa una lectura. |
| Condiciones y registro fotográfico de la medición | No comunicados. Las tres fotografías disponibles muestran el montaje, no una lectura de multímetro. |

La confirmación se registra como resultado del responsable; no como una medición ejecutada durante esta edición documental. Tampoco certifica por sí sola la protección ante todos los valores futuros de AO. Las [fotografías reales del circuito](../../docs/T-1.3-breadboard-circuit-evidence.md#5-evidencias-fotográficas) complementan el registro del montaje.

## 7. Referencias técnicas

- [ESP32-WROOM-32: funciones de los pines](https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html).
- [Espressif: ADC1, ADC2 y limitaciones con WiFi](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-reference/peripherals/adc.html).
- [Aosong: manual AM2302, alimentación e interfaz](https://www.aosong.com/uploadfiles/2025/04/20250417105409216.pdf).
- [Winsen: manual MQ135](https://www.winsen-sensor.com/d/files/manual/mq135.pdf).
