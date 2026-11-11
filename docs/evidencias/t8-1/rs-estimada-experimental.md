# Issue #140 — Rs estimada con carga nominal

Fecha: 27/10/2026, America/La_Paz. Revision electrica: REV3.
Esta decision autoriza captura experimental; **no registra una medicion nueva**.

## Parametros y procedencia

| Parametro | Valor | Clasificacion |
|---|---:|---|
| RL interna del MQ-135 Flying-Fish | 10000 ohm | Nominal estimado adoptado provisionalmente; no medido ni identificado fisicamente |
| R1, AO a GPIO34 | 10000 ohm | Nominal instalado comunicado en Sprint 1 |
| R2, GPIO34 a GND | 10000 ohm | Nominal instalado comunicado en Sprint 1 |
| RL efectiva | 6666,67 ohm | Calculada y redondeada a partir de esos nominales y la topologia supuesta |
| k = VGPIO34 / VAO | 0,50 | Nominal y corroborado puntualmente por 0,12/0,24 V |
| Vc del sensor | 4,89 V | Medicion puntual previa comunicada; no demuestra estabilidad de la fuente |
| Topologia interna | AO en el nodo directo Rs/RL, sin buffer | Supuesta, sin comprobacion fisica |

Antecedentes: [registro REV3](../t1-5/mq135-divisor-10k-rev3.md).
Las tensiones VCC, AO y GPIO34 y la comprobacion del divisor ya estan
documentadas; no se exige repetirlas para habilitar esta estimacion.
Las constantes nominales quedan identificadas en
`firmware/include/mq135_calibration_config.h`.

## Circuito supuesto y calculo

Se supone Rs entre Vc y AO, RL interna entre AO y GND, y el divisor externo
R1 + R2 como otra rama desde AO a GND. La entrada ADC y las otras cargas
se consideran despreciables para este modelo. No se ha comprobado que el
modulo concreto cumpla esas condiciones.

```text
Rdivisor = R1 + R2 = 10000 + 10000 = 20000 ohm
RL_efectiva = RL_interna || Rdivisor
           = (10000 * 20000) / (10000 + 20000)
           = 6666,666... ohm
Carga utilizada por el firmware = 6666.67 ohm

VADC_V = adc_mV / 1000
VAO_V = VADC_V / 0.50
q = 4.89 / VAO_V - 1
Rs_estimada_ohm = 6666.67 * q
promedio_n = promedio_(n-1) + (Rs_estimada_n - promedio_(n-1)) / n
```

Reconstruir VAO compensa la atenuacion; usar el paralelo compensa la carga
externa. El redondeo de RL efectiva no representa precision de medicion.
La salida esta en ohmios, no en kiloohmios.

## Activacion y salida

`MQ135_EXPERIMENTAL_RS=1` esta habilitado por defecto para este ensayo;
la captura comienza solo al recibir `mq135 calibracion iniciar` y el ACK
existente de supresion CO2. `MQ135_RL_TOPOLOGY_VERIFIED` permanece en **0**.
La revision es 3 y `MQ135_RL_EVIDENCE_ID` apunta a este documento de supuestos.

Monitor a 115200 baudios:

```text
mq135 calibracion iniciar
mq135 calibracion estado
mq135 calibracion detener
```

Los campos de salida son `[FISICO] [EXPERIMENTAL]`, `Rs_estimada_ohm`,
`promedio_Rs_estimada_ohm`, `validas`, `rechazadas`, `q_adimensional`,
`RL=NOMINAL_NO_MEDIDA`, `RL_efectiva_ohm=6666.67`,
`topologia=SUPUESTA_NO_VERIFICADA` y `promedio Rs NO es Ro`.
La etiqueta FISICO describe el origen ADC, no la certeza de los parametros.
Una lectura rechazada muestra Rs no disponible y conserva el promedio previo.
Detener conserva el resumen; iniciar otra captura reinicia los contadores.

## Protecciones y limites

- Se conservan ADC1/GPIO34, 12 bits, 0 dB, mediana de 11 lecturas por muestra,
  intervalo de 2000 ms y espera de software de 30000 ms.
- Siguen activos los rechazos por ADC cero/saturado, rango 100–950 mV,
  conversion no disponible, configuracion electrica incompatible, tensiones
  invalidas, AO >= Vc, NaN/infinito y resultados numericos invalidos.
- La opcion experimental solo autoriza la carga supuesta; no verifica
  proteccion electrica ni conversion ADC. La demo sintetica no puede iniciar
  una captura fisica.
- Se exige revision coincidente, referencia documental no vacia y carga
  positiva finita. Estimacion y bandera de topologia verificada simultaneas
  se rechazan. La API estricta sigue rechazando cargas estimadas.
- DHT22, WiFi y MQTT mantienen sus protecciones, reconexion y cadencia.
  CO2 se omite durante captura con el ACK existente. Al detener se requiere
  referencia provisional nueva; las Rs estimadas no alimentan esa referencia.
- No se calcula Ro definitivo ni ppm calibradas. Las ppm del modo normal
  conservan su caracter provisional y los 400 ppm supuestos.
- No se cuantifica exactitud: se desconocen RL real, tolerancias y cargas
  internas; el contraste ADC previo es puntual, no una calibracion de escala.

Para volver al modo estricto, definir `MQ135_EXPERIMENTAL_RS=0`. Sin overrides
sus valores de carga/revision/evidencia vuelven a 0/0/vacio y bloquean Rs.
Solo despues de medir RL interna y comprobar la topologia se podra configurar
una carga verificada, con revision y evidencia fisica correspondientes.

Los resultados de escritorio estan en [resultados.md](resultados.md).
Esta actualizacion no carga firmware ni acredita ejecucion en ESP32.
