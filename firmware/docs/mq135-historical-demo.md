# Issue #11 — demostración retrospectiva del algoritmo MQ-135

**Los números de esta demostración NO son concentraciones reales de CO₂.**
Se utilizan exclusivamente diez códigos ADC históricos aportados del monitor
serie. No se conectan sensores ni se abre un puerto serie ni se carga firmware.
El análisis de escritorio no modifica ni importa referencias a `main.cpp` o
`mq135_site_config.h`. La configuración física REV3 y su contraste puntual
actual se documentan aparte en [mq135-experimental.md](mq135-experimental.md);
esta demostración no completa una referencia física ni habilita ppm.
El modelo es provisional hasta HU-08.

## Datos y selección de referencia

La transcripción íntegra está en
[mq135_historical_demo.h](../tools/mq135_historical_demo.h).
Se eligen **las tres primeras muestras por orden temporal**, sin buscar un
resultado favorable ni ajustar su selección según las siete posteriores:

| Uso | Tiempo desde arranque (ms) | ADC |
|---|---:|---:|
| Referencia demostrativa | 30114 | 64 |
| Referencia demostrativa | 32120 | 80 |
| Referencia demostrativa | 34126 | 89 |
| Evaluación | 36132 | 120 |
| Evaluación | 38138 | 87 |
| Evaluación | 40144 | 112 |
| Evaluación | 42150 | 97 |
| Evaluación | 44156 | 86 |
| Evaluación | 46162 | 80 |
| Evaluación | 48168 | 87 |

Referencia: 3 muestras, promedio **77,666666667 códigos ADC**, duración
primera–última **4012 ms** e intervalos observados **2006 ms**. El conjunto
completo dura **18054 ms** y todos sus intervalos son 2006 ms. Esos tiempos
históricos no cambian el intervalo de 2000 ms configurado en el firmware.
No están disponibles fecha/hora civil, condiciones del DHT22 ni voltajes
medidos; se registran como ausentes, sin inventar datos.

Los tiempos superan los 30000 ms de espera del firmware. Esto **no acredita el
precalentamiento/acondicionamiento físico ni la estabilidad térmica del sensor**.
Tampoco consta verificación eléctrica anterior a estas muestras. Por ello,
este conjunto **no puede registrarse como referencia válida del firmware**.

## Hipótesis explícitas y fórmula

Montaje histórico comunicado: ESP32-WROOM-32, GPIO34, ADC de 12 bits con
atenuación de 11 dB; MQ-135 Flying-Fish alimentado nominalmente por VIN/5V,
AO directo a GPIO34, **sin divisor externo** y DO sin conectar.
**La tensión AO y su compatibilidad eléctrica siguen sin verificar.**
Un código ADC bajo no demuestra seguridad eléctrica.

Para ilustrar el cálculo se define el escenario
`RETROSPECTIVA_LINEAL_NO_VERIFICADA_V1`:

- **Fondo de escala lineal hipotético: 3,3 V.** Es una elección numérica para
  esta demostración; no se obtiene de la atenuación de 11 dB ni representa una
  calibración oficial o medida del ESP32. Se desconocen la transferencia real,
  offsets, linealidad, ruido y rango útil de estos códigos bajos.
- **Vc asumida: 5,0 V**, a partir de la alimentación nominal documentada,
  sin sustituir una medición de Vc ni demostrar su estabilidad.
- `VAO_aprox = 3,3 * (ADC / 4095)`. En este escenario se identifica la tensión
  hipotética del pin con AO por la conexión directa documentada. Esto no
  reconstruye una tensión físicamente validada. No hay divisor que compensar.
- **400 ppm es un supuesto de desarrollo, no una concentración medida**.
  Coeficientes GeorgK/MQ135: A = 116.6020682, B = 2.769034857; provisional
  hasta HU-08, sin compensación ambiental ni selectividad demostrada al CO₂.

Se reutilizan `validateAdc12Bit`, `normalizedResistance` y
`estimateExperimentalPpm` de `mq135_experimental.h`:

```text
q_i = (Vc_asumida - VAO_aprox_i) / VAO_aprox_i
    = Vc_asumida / VAO_aprox_i - 1
q_referencia_demostrativa = promedio(q_1, q_2, q_3)
ppm_experimentales_i = 400 * (q_i / q_referencia_demostrativa)^(-B)
```

Se promedian los **q individuales**, no se calcula q a partir del ADC promedio:
esas operaciones difieren por la transformación no lineal. La referencia
demostrativa se fija una vez antes de evaluar y permanece constante. Las tres
muestras de referencia tienen **ppm vacíos** en la tabla; solo se estiman las
siete posteriores. La evaluación muestra comportamiento matemático; no mide
error o precisión frente a una concentración conocida.

A se cancela al normalizar el modelo contra 400 ppm. La interpretación ideal
`q = Rs/RL` y la cancelación de RL exigen una carga resistiva efectiva constante
entre referencia y evaluación. Cambios de circuito, carga/divisor o alimentación
invalidan esa comparación física; el registro histórico no demuestra dichas
condiciones. La derivación y los controles del sistema real están en
[mq135-experimental.md](mq135-experimental.md).

## Tabla algebraica de ejemplo

[mq135-historical-demo-example.csv](mq135-historical-demo-example.csv) guarda
los diez tiempos y ADC, el uso de cada muestra, VAO **aproximada no verificada**,
q y ppm **experimentales no medidos**, con ambos voltajes supuestos y
`conversion_verificada=false` en cada fila. Se preparó mediante cálculo
algebraico para el escenario 3,3 V / 5,0 V; **no es la salida de una ejecución
comprobada del programa ni evidencia de pruebas aprobadas**.

En ese escenario:

- `q_referencia_demostrativa ≈ 80,405611593463`.
- Dispersión relativa observada `(max(q) - min(q))/promedio(q)`:
  **0,338683187856**, aproximadamente **33,87 %**. No hay criterio físico
  justificado de aceptación; **no se declara el lote físicamente estable**.
- Los voltajes hipotéticos abarcan aproximadamente 0,051575–0,096703 V. No
  están acreditados como voltajes medidos o señales útiles para estimación.
- Por ejemplo, ADC 120 a 36132 ms produce aproximadamente **1433,936938 ppm
  experimentales**; es consecuencia de las hipótesis, no una lectura real de CO₂.

La herramienta guarda además un informe con fuente, muestras exactas, tiempos,
promedios, dispersión, hipótesis, identificador de escenario, coeficientes,
400 ppm asumidos y datos ausentes. Ese identificador describe un escenario
numérico; **no es una revisión eléctrica verificada** ni un registro persistible
mediante `Reference`/`ReferenceLog`. No se construye un `VerifiedContext`
ficticio para evitar los bloqueos de esas clases.

## Rechazos y límites

Se descartan ADC fuera de 0–4095, ADC 4095 saturado y ADC cero. Se rechazan
hipótesis no positivas o no finitas, `VAO_aprox <= 0`, `VAO_aprox >= Vc`, q
no finito y resultados ppm no finitos o nulos por underflow. Las muestras deben
mantener orden temporal y estar después de la espera. Si falla una muestra de
referencia, se invalida el lote completo y se bloquean las estimaciones; no se
elimina esa muestra para fabricar una referencia aceptable. Una evaluación
inválida conserva la referencia y deja su ppm vacío con motivo de rechazo.

El mínimo ADC 1 **solo excluye cero**. No se inventa un umbral de señal confiable
ni se interpreta `OK_SOLO_MATEMATICO` como validez física. La falta de rango útil,
conversión contrastada y precalentamiento físico es una limitación explícita de
todos los números. Ni los filtros de software ni esta demostración protegen GPIO34.

## Ejecución manual desde MSYS2 UCRT64

El código y las comprobaciones están **preparados, sin compilar ni ejecutar**
en esta intervención. Estos comandos son instrucciones para revisión manual;
no usan PlatformIO ni acceden al ESP32. Requieren `g++` disponible en UCRT64.

```bash
cd /d/telematicos/monitoreo-IoT-Maicol
mkdir -p /tmp/mq135-demo
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tools/mq135_historical_demo.cpp -o /tmp/mq135-demo/mq135_historical_demo.exe
/tmp/mq135-demo/mq135_historical_demo.exe --solo-retrospectiva 3.3 5.0 /tmp/mq135-demo/tabla.csv /tmp/mq135-demo/informe.txt
```

Los argumentos 3.3 y 5.0 usan punto decimal y aceptan expresamente las hipótesis
del ejemplo; el programa no elige voltajes por defecto. Guarda CSV e informe
UTF-8 con punto decimal, imprime el informe y devuelve error ante entradas,
cálculos o escritura inválidos. Una salida correcta solo significa validez
matemática del escenario. Rechaza sobrescribir archivos existentes: para repetir,
usar nombres de salida nuevos. Los resultados y el ejecutable quedan en `/tmp`,
separados del firmware y del CSV de ejemplo del repositorio.

Opcionalmente, revisar de forma manual las comprobaciones adicionales:

```bash
g++ -std=c++11 -Wall -Wextra -pedantic firmware/tests/mq135_historical_demo_test.cpp -o /tmp/mq135-demo/mq135_historical_demo_test.exe
/tmp/mq135-demo/mq135_historical_demo_test.exe
```

Cubren transcripción, partición de muestras, promedio de q, dispersión reportada,
referencia fija ante cambios posteriores, resultados algebraicos del ejemplo,
ADC cero/fuera de rango/saturado, tensiones inválidas/no finitas, underflow,
overflow de q/ppm, bloqueo sin referencia, tiempos inválidos y normalización ante
un escalado común **sintético**. No se modifican las pruebas existentes para
obtener aprobaciones. Comparar la salida manual con el CSV de ejemplo usando
tolerancia numérica, por ejemplo 1e-8, en lugar de identidad textual de decimales.

## Pendiente para CA-03

Esta preparación aporta una demostración adicional del algoritmo condicionada a
supuestos. **No completa por sí sola la validación física del CA-03**. Faltan
verificación/protección del circuito y AO, Vc medida, transferencia y rango útil
ADC contrastados, acondicionamiento físico del sensor, criterio de estabilidad
justificado y un lote nuevo adquirido después de verificar el circuito. La
referencia histórica demostrativa no sustituye ese lote ni permite activar ppm.
La compilación y ejecución manual del programa/pruebas también están pendientes.
HU-08 realizará la calibración y validación definitiva frente a datos físicos
de referencia, sin tratar 400 ppm supuestos como concentración medida.
