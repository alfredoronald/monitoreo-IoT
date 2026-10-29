# Revisión antes del commit — Issue #140

Fecha: 27/10/2026, America/La_Paz.
Rama activa comprobada: `feat/140-mq135-calibration-mode`.
Destino solicitado: `feature/firmware-esp32`.

## Actualización: revisión con evidencia textual autorizada

El responsable aclaró que no guardó capturas del monitor durante el ensayo;
conservó fragmentos de salida compartidos en ChatGPT. Los resultados de
`validacion-fisica.md` continúan identificados como reportados por él.
No se afirma disponer de imágenes ni de un log completo del ESP32.

La carpeta `D:\telematicos\monitoreo-IoT\docs\evidencias\t8-1\capturas`
es una ubicación prevista para imágenes futuras. No hay originales que
sincronizar ahora ni se reutilizan imágenes históricas de Sprint 1.

En esta intervención se modifican únicamente `validacion-fisica.md` y este
informe para registrar esa limitación y separar las sesiones. Los apartados
siguientes conservan los hallazgos de la revisión inicial. Continúan siete
archivos versionados modificados y diecinueve archivos sin seguimiento,
ninguno preparado en el índice. No se agregaron imágenes ni archivos vacíos.

Las 81 muestras válidas y nueve rechazadas, con última Rs 26530.9026 Ω y
promedio 26093.8642 Ω, siguen atribuidas a la ejecución anterior reportada.
No se asignan a una posible ejecución nueva ni se completan datos ausentes.
Las futuras capturas deberán revisarse por posibles secretos al incorporarlas;
no se declara realizada ni aprobada una inspección visual inexistente.

Se confirmó en el código la salida `Rs_estimada_ohm`, el promedio estimado,
`RL=NOMINAL_NO_MEDIDA`, la topología supuesta y la bandera de verificación
en 0. No se obtiene Ro definitivo ni se declara CO₂ calibrado.

La revisión de rutas y diferencias conserva el alcance de #140: captura,
parser, protección de publicación CO₂, pruebas, helpers y documentación.
No se detectaron cambios pendientes en backend o frontend. Se verificó que
`config.h` sigue ignorado; no se copió su contenido ni se modificó firmware.

**Lista para continuar la revisión previa al commit con evidencia textual.**
Por indicación expresa del responsable, la ausencia de capturas es una
limitación pendiente y no bloquea por sí sola esa revisión. No se declara
completa la evidencia visual ni se exige repetir mediciones de Sprint 1.
No se ejecutan `git add`, commit, push ni creación de PR.

## Resultado de la revisión inicial

Se creó [validacion-fisica.md](validacion-fisica.md) con los resultados
reportados por el responsable, la matriz de aceptación y los adjuntos
pendientes. Este archivo registra la revisión de alcance y Git.
Son los únicos dos archivos creados en esta intervención; no se modificaron
archivos existentes, firmware funcional, resultados históricos, backend,
frontend ni configuración privada.

No se identificó un defecto funcional concreto adicional en la revisión
estática de los cambios de T-8.1. Esto no sustituye una revisión de otro
integrante ni certifica todos los casos de concurrencia en el dispositivo.
No se ejecutaron pruebas nuevas, builds, upload ni otro ensayo físico.

## Alcance de los cambios pendientes frente a HEAD

Se revisaron las siete diferencias de archivos ya versionados y los
diecisiete archivos sin seguimiento que existían al iniciar esta revisión.
Con los dos informes nuevos quedan diecinueve archivos sin seguimiento.
El índice no tiene cambios preparados.

| Archivos existentes modificados | Relación con #140 |
|---|---|
| `firmware/include/mq135_acquisition.h` | Captura, estado, promedio/contadores, etiquetas estimadas y separación de la referencia provisional |
| `firmware/include/mq135_experimental.h` | Estados de carga no verificada y captura activa |
| `firmware/include/mq135_site_config.h` | Asociación de la carga configurada a la adquisición |
| `firmware/include/mqtt_publication.h` | Supresión CO₂, confirmación y descarte de publicaciones de épocas anteriores |
| `firmware/src/main.cpp` | Comandos, salida Serial, coordinación de la captura y recuperación normal |
| `firmware/docs/mq135-experimental.md` | Referencia a captura estimada, bloqueo estricto y recuperación |
| `firmware/docs/mqtt-publication.md` | Explicación del parser compartido para calibración y comandos temporales |

| Archivos nuevos pendientes | Relación con #140 |
|---|---|
| `firmware/include/mq135_calibration.h` | Fórmula y acumulador con carga estricta o estimada explícita |
| `firmware/include/mq135_calibration_config.h` | Parámetros nominales experimentales y topología sin verificar |
| `firmware/include/serial_line_parser.h` | Único parser incremental de comandos |
| `firmware/tests/mq135_calibration_test.cpp` | Pruebas sintéticas de modos, acumulación, protecciones y continuidad |
| `firmware/tools/run-desktop-tests.ps1` | Ejecución reproducible de pruebas de escritorio |
| `firmware/tools/compile-with-example.ps1` | Comprobación de compilación con plantilla, sin upload |
| `docs/evidencias/t8-1/` | Once archivos previos de documentación/software y dos informes de esta revisión |

El cambio del parser WiFi temporal por una cola y la actualización MQTT se
relacionan con compartir Serial y omitir CO₂ durante captura; no se detectan
como cambios ajenos a la Issue. Los helpers apoyan la evidencia reproducible.
No aparecen cambios pendientes en backend, frontend, contratos de tópicos,
base de datos o archivos de credenciales. No se incorporan binarios `.pio`.

## Rama destino local y referencia remota guardada

Hay una diferencia importante al elegir la base de comparación:

| Referencia local inspeccionada | Identificador observado |
|---|---|
| `HEAD` / `feat/140-mq135-calibration-mode` | `fb7fbcd` |
| `feature/firmware-esp32` | `0ab1e5a` |
| `origin/feature/firmware-esp32` | `fb7fbcd` |

La rama destino **local** está once commits detrás de HEAD (conteo 0/11).
Compararla directamente con HEAD muestra 60 archivos históricos del Sprint 1,
8177 inserciones y 101 eliminaciones; no son cambios introducidos ahora por
#140. Se revisaron sus nombres y el historial: incluyen DHT22, MQ-135,
WiFi/MQTT y las evidencias de Sprint 1.

La referencia remota **guardada localmente** `origin/feature/firmware-esp32`
coincide con HEAD (conteo 0/0). Frente a ella, los cambios de archivos
versionados pendientes son los siete indicados arriba, con 252 inserciones
y 60 eliminaciones; los archivos sin seguimiento se enumeran aparte y no
aparecen en `git diff --stat`.

No se ejecutó fetch ni se consultó el estado actual del servidor. Antes del
PR conviene confirmar la base remota vigente para no presentar diferencias
históricas como parte de #140. No se cambió de rama ni se actualizó ninguna
referencia para preparar estos informes.

## Comprobaciones de Git y exclusiones

`git diff --check` se ejecutó sin salida y con código **0**.
Esta orden comprueba el diff de archivos versionados; no cubre los nuevos
archivos sin seguimiento. Los informes nuevos se verifican además por
formato y enlaces locales sin añadirlos al índice.

`git diff --cached --quiet` terminó con código **0**: índice sin cambios.
Las consultas de cambios de backend y frontend no devolvieron rutas.

`git check-ignore -v firmware/include/config.h` confirmó la regla de
`.gitignore:16`. `config.h` no está versionado, no aparece en el estado ni
se incorporó a los informes. Su contenido no se copió. Los registros de
T-8.1 revisados no contienen coincidencias de los patrones de asignación
de credenciales examinados; esto es una comprobación acotada, no una
certificación universal de ausencia de secretos.

Salida de `git status --short` (Git agrupa los archivos nuevos de T-8.1):

```text
 M firmware/docs/mq135-experimental.md
 M firmware/docs/mqtt-publication.md
 M firmware/include/mq135_acquisition.h
 M firmware/include/mq135_experimental.h
 M firmware/include/mq135_site_config.h
 M firmware/include/mqtt_publication.h
 M firmware/src/main.cpp
?? docs/evidencias/t8-1/
?? firmware/include/mq135_calibration.h
?? firmware/include/mq135_calibration_config.h
?? firmware/include/serial_line_parser.h
?? firmware/tests/mq135_calibration_test.cpp
?? firmware/tools/compile-with-example.ps1
?? firmware/tools/run-desktop-tests.ps1
```

No se ejecutaron `git add`, commit, push ni creación de PR.

## Recomendación vigente

**Lista para revisión humana previa al commit.** Los tres criterios tienen
PASS físico reportado para el modo estimado autorizado. RL sigue nominal,
la topología sin verificar y el promedio no es Ro definitivo.

Continuar la revisión del commit con los resultados textuales registrados y
su procedencia explícita. Las capturas que detalla el
[informe físico](validacion-fisica.md#capturas-y-fotografía-por-incorporar)
quedan pendientes y no bloquean por sí solas esa revisión, conforme a la
indicación del responsable. No se afirma disponer de un log completo,
imágenes del ensayo ni evidencia visual revisada. No se pide repetir las
mediciones previas de Sprint 1 ni se realiza el commit en esta intervención.
