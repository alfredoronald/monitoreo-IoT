# Validación física reportada — Issue #140 / T-8.1

Fecha de redacción: 27/10/2026, America/La_Paz.
Rama revisada: `feat/140-mq135-calibration-mode`.
Destino previsto: `feature/firmware-esp32`.

## Evidencia textual disponible y capturas pendientes

El responsable aclaró que **no guardó imágenes del monitor serie durante
la validación física**. Conservó fragmentos de salida compartidos en ChatGPT;
este informe mantiene los resultados aportados como reportados por él.
No hay imágenes de este ensayo ni un log completo adjunto al repositorio.
No se reconstruyen líneas Serial a partir de los números del resumen.

La ruta prevista para futuras capturas es
`D:\telematicos\monitoreo-IoT\docs\evidencias\t8-1\capturas`.
Es una ubicación planificada, no evidencia de archivos existentes.
No se reutilizan imágenes de Sprint 1 ni se crean adjuntos vacíos.
Por indicación del responsable, **la ausencia de capturas se registra como
limitación documental y no bloquea por sí sola la revisión del commit**.

| Escenario solicitado | Captura auténtica disponible | Cobertura actual |
|---|---|---|
| Calibración activa | Pendiente; no se guardó imagen | CA-01: PASS reportado; sin corroboración visual |
| Detención y resumen | Pendiente; no se guardó imagen | CA-02: PASS reportado; 81/9 conservado como resumen de la ejecución anterior |
| Recuperación MQTT | Pendiente; no se guardó imagen | CA-03: PASS reportado; recuperación y publicaciones comunicadas, sin imagen |

Las capturas que se obtengan en el futuro deberán revisarse visualmente
para excluir contraseñas y credenciales antes de incorporarlas. No se
declara aprobada una inspección de imágenes que todavía no existen.

La revisión del código confirma que Rs conserva su etiqueta de estimada,
RL sigue nominal y `MQ135_RL_TOPOLOGY_VERIFIED` permanece en 0. Esta
confirmación del código no sustituye la lectura de las imágenes ausentes.

## Separación entre ejecuciones

El resumen de **81 muestras válidas, 9 rechazadas**, última Rs estimada
**26530.9026 Ω** y promedio **26093.8642 Ω** pertenece exclusivamente a la
ejecución anterior comunicada por el responsable. También pertenecen a ese
reporte los datos de publicación a 212039 y 227044 ms, con payload 401.9.

No hay resultados verificables disponibles de una ejecución nueva. Si las
capturas proceden de otra sesión, se documentarán por separado sus contadores,
resistencias, estados y tiempos realmente visibles. No se combinarán muestras,
promedios ni tiempos entre sesiones ni se exigirá que una nueva captura
reproduzca el resumen de 81/9.

## Origen y alcance de la evidencia

Este informe consolida los resultados físicos comunicados por el responsable
en la solicitud de cierre de T-8.1. La prueba fue realizada por el responsable;
esta revisión documental no volvió a conectar el ESP32, cargar firmware ni
ejecutar el ensayo.

Al redactar el informe, `docs/evidencias/t8-1/` contenía documentación,
resultados sintéticos y compilaciones, pero ninguna captura de pantalla,
fotografía ni log original de este ensayo físico. Los números siguientes
son datos reportados, no transcripciones verificadas contra un archivo Serial.
No se dispone de las 90 observaciones individuales ni se reconstruyen líneas
de monitor, fechas o mensajes ausentes.

La fecha y hora exactas del ensayo, el identificador del binario cargado,
sus flags de compilación y las versiones efectivamente usadas en la placa
no fueron aportados. La rama revisada no identifica por sí sola ese binario.

## Montaje y parámetros

| Elemento | Resultado o configuración reportada | Naturaleza |
|---|---|---|
| Placa | ESP32 conectado por COM3 | Observación comunicada |
| Monitor | 115200 baudios | Configuración comunicada |
| WiFi | Conectado; IP del ESP32 192.168.1.8 | Observación comunicada; no prueba reconexión ante fallos |
| MQTT | Conectado al broker local | Observación comunicada; sin log del broker adjunto |
| Modo | Calibración experimental iniciada mediante comando | Observación comunicada |
| RL interna | 10000 Ω | Nominal supuesta, no medida |
| Divisor externo | R1 = 10 kΩ y R2 = 10 kΩ, REV3 | Nominales comunicados y documentados en Sprint 1 |
| RL efectiva | 6666.67 Ω | Estimada por cálculo con la topología supuesta |
| Topología interna | Sin comprobación física | No verificada |

La configuración del código conserva `MQ135_EXPERIMENTAL_RS=1` y
`MQ135_RL_TOPOLOGY_VERIFIED=0`. Esto se verificó por lectura del código;
no constituye lectura de los parámetros del binario ejecutado.
No se incorpora `config.h`, SSID, contraseñas ni credenciales del broker.

La justificación del modelo y sus limitaciones se conserva en
[rs-estimada-experimental.md](rs-estimada-experimental.md):

```text
RL_efectiva = RL_interna || (R1 + R2)
           = 10000 || 20000 = 6666,666... Ω
Valor nominal redondeado utilizado = 6666.67 Ω
VAO = (adc_mV / 1000) / 0,50
Rs_estimada = RL_efectiva × (Vc / VAO − 1)
```

Se supone AO como nodo directo Rs/RL, sin buffer y con otras cargas
despreciables. Las mediciones previas de VCC, AO y GPIO34 están en el
[registro REV3 del Sprint 1](../t1-5/mq135-divisor-10k-rev3.md);
este ensayo no aporta mediciones nuevas de esas tensiones o resistencias.

## Resultados físicos comunicados

| Dato al detener | Valor reportado |
|---|---:|
| Muestras válidas | 81 |
| Muestras rechazadas | 9 |
| Última Rs estimada | 26530.9026 Ω |
| Promedio de Rs estimada | 26093.8642 Ω |
| RL efectiva nominal estimada | 6666.67 Ω |

Los contadores suman 90 observaciones contabilizadas: cálculo a partir del
resumen, no inventario de un log completo. Se desconocen la secuencia,
los motivos de los nueve rechazos, los extremos, la dispersión y la duración
total. No se atribuyen los rechazos a calentamiento, saturación o ruido sin
un registro que lo demuestre. El promedio no puede recalcularse a partir
de la última Rs y los contadores solamente.

Según lo comunicado por el responsable:

1. WiFi y MQTT estaban conectados antes de iniciar la captura experimental.
2. El comando de inicio activó la captura y se obtuvieron los contadores y
   las resistencias estimadas indicados arriba.
3. El DHT22 continuó registrando temperatura y humedad durante calibración.
   No se aportaron valores ni sellos temporales individuales del DHT22.
4. Durante calibración se omitió la publicación de CO₂.
5. El comando de detención desactivó la calibración y conservó el resumen.
6. Se recuperó la referencia provisional mediante tres muestras nuevas.
7. MQTT volvió a publicar `ambiente/temperatura`, `ambiente/humedad` y
   `ambiente/co2`.

Los nombres de comandos disponibles en el firmware son
`mq135 calibracion iniciar`, `mq135 calibracion estado` y
`mq135 calibracion detener`. Se citan como interfaz del programa; no son
una transcripción de órdenes recuperadas de un log físico.

## Publicación posterior a la detención

Los únicos datos numéricos de publicación aportados para este ensayo son:

| Tiempo reportado | Variable | Payload | QoS |
|---:|---|---|---:|
| 212039 ms | CO₂ provisional, tópico `ambiente/co2` | 401.9 | 0 |
| 227044 ms | CO₂ provisional, tópico `ambiente/co2` | 401.9 | 0 |

La separación calculada es `227044 − 212039 = 15005 ms` (15,005 s), compatible
con el intervalo nominal de 15 s en este único par. No demuestra cadencia
durante toda la sesión, estabilidad del sensor ni entrega garantizada.
No se dispone del texto literal de esas líneas ni de una captura de recepción
del suscriptor: la publicación reportada no se presenta como recepción
independientemente comprobada en el broker.

El payload 401.9 corresponde al modelo provisional recuperado. No proviene
de convertir este promedio de Rs en Ro ni constituye CO₂ calibrado o certificado.

## Criterios de aceptación de la #140

En esta matriz, **PASS reportado** significa comportamiento físico comunicado
por el responsable y compatible con la implementación y las pruebas existentes.
La evidencia primaria de pantalla sigue pendiente; no equivale a una
verificación independiente de cada lectura del ensayo.

| Criterio | Estado | Sustento y límite |
|---|---|---|
| Se imprime Rs en cada lectura | PASS reportado para Rs estimada | Captura experimental reportada y última Rs de 26530.9026 Ω. La implementación imprime `Rs_estimada_ohm`; los rechazos muestran `NO_DISPONIBLE`. Sin el log completo no se certifica la impresión de cada una de las 81 muestras válidas. |
| Se imprimen promedio y número acumulado | PASS reportado | Promedio final 26093.8642 Ω, 81 válidas y 9 rechazadas. Falta captura original del resumen para corroboración visual; no se recalcula la media desde muestras ausentes. |
| El modo normal de publicación sigue funcionando | PASS reportado | Detención, referencia nueva de tres muestras y retorno de los tres tópicos; dos publicaciones CO₂ con 15005 ms de separación. Falta evidencia visual de la transición y de los tres tópicos. |

El criterio de evidencia de la Issue pide una captura del monitor serie:
**ese adjunto queda pendiente**. La revisión previa al commit continúa con
los resultados textuales reportados por el responsable y esta limitación
explícita; no se declara cumplida la entrega de evidencia visual.

## Capturas y fotografía por incorporar

Los siguientes nombres son propuestas; **estos archivos no existen aún**
y no se crean imágenes, logs vacíos ni enlaces a adjuntos inexistentes.
Son evidencias pendientes de obtener, no requisitos bloqueantes para revisar
el commit con la evidencia textual autorizada. No se exige repetir mediciones
documentadas de Sprint 1.

| Prioridad | Evidencia necesaria | Qué debe verse | Nombre propuesto |
|---|---|---|---|
| Pendiente; no bloquea la revisión | Monitor durante captura activa | `[FISICO] [EXPERIMENTAL]`, Rs estimadas sucesivas, promedio, contador, carga nominal, topología supuesta; DHT22 y omisión CO₂ si caben en el fragmento | `CA01_Captura_Rs_Estimada.png` |
| Pendiente; no bloquea la revisión | Monitor al detener | Estado inactivo y resumen real de la sesión que se fotografíe; no atribuirle automáticamente los valores anteriores de 81/9 | `CA02_Resumen_Detenido.png` |
| Pendiente; no bloquea la revisión | Recuperación y publicación normal | Referencia provisional con tres muestras nuevas y retorno de los tres tópicos, con los tiempos y payloads de esa sesión | `CA03_Recuperacion_MQTT.png` |
| Complementaria | Conexión inicial | COM3, 115200 baudios, IP del ESP32 y conexión MQTT, sin secretos | `Conexion_ESP32_MQTT.png` |
| Complementaria | Foto actual de REV3 | ESP32, MQ-135 y divisor externo identificables | `Montaje_REV3.jpg` |

Pueden usarse menos capturas si muestran juntos los datos requeridos; para
omisión CO₂ conviene que el fragmento cubra al menos un ciclo MQTT durante
captura y muestre la omisión explícita. Una imagen aislada sin CO₂ no prueba
ausencia de publicaciones durante todo el ensayo.
Una foto del montaje no acredita por sí sola RL interna ni topología interna.
Si se realiza otro ensayo para obtener imágenes, identificarlo como sesión
distinta y conservar sus números reales, sin atribuirle el resumen de 81/9.

## Separación de evidencias y límites

- [resultados.md](resultados.md), los archivos `calibracion-*-sintetica.txt`
  y `calibracion-sintetica.txt`, los resúmenes `pruebas-escritorio*.txt` y
  los registros `compilacion-*.txt` conservan su carácter previo de software.
  No son logs del ESP32 de este ensayo y no fueron modificados.
- Las nueve suites anteriores aprobadas y las 159 comprobaciones de
  calibración en cada configuración siguen siendo evidencia sintética.
  Las compilaciones previas con plantilla no identifican el binario físico.
  En esta revisión documental no se volvieron a ejecutar pruebas ni builds.
- RL interna y RL efectiva siguen siendo nominales estimadas; la topología
  no está físicamente verificada. No se cuantifica exactitud ni incertidumbre.
- **No se obtuvo Ro definitivo. El promedio de Rs no es Ro. No se obtuvo
  CO₂ calibrado ni certificado.**
- Las 81 muestras válidas no acreditan por sí solas medición exterior en aire
  limpio ni estabilidad para #141. Esas condiciones no fueron aportadas.
- No se documentan aquí ensayos físicos de caída/reconexión de red, congestión
  Serial o todos los rechazos ADC. Las pruebas sintéticas no sustituyen esos
  ensayos, que no se declaran realizados.

## Dictamen para revisión

El flujo experimental de la #140 tiene los tres criterios funcionales
reportados como PASS, con las salvedades anteriores. **La revisión del commit
puede continuar con esta evidencia textual**, por indicación del responsable.
La falta de capturas queda registrada como evidencia pendiente, sin bloquear
por sí sola esa revisión ni presentar el paquete como evidencia visual completa.
No es necesario medir RL real para aceptar el modo estimado autorizado,
pero tampoco se puede elevar la bandera de topología verificada.

La revisión de alcance y el estado Git están en
[revision-precommit.md](revision-precommit.md). No se realizaron `git add`,
commit, push, PR ni cambios funcionales para preparar este informe.
