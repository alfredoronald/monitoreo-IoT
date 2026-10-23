# Propuesta de reducción de evidencias — Issue #87 / T-5.8

Fecha de auditoría: 22/10/2026, America/La_Paz. Rama comprobada: `test/87-responsive-check`. Estado: **sólo inventario y propuesta; reducción no ejecutada**. No se borró, movió, renombró, recomprimió ni modificó ningún archivo original. Tampoco se modificó código. Los resultados originales permanecen intactos: **146 PASS, 0 FAIL finales; 10 FAIL históricos**. No se repitieron pruebas en esta tarea.

## Inventario de partida

Los 209 archivos originales suman **87.282.391 bytes = 87,28 MB decimales = 83,24 MiB**. Esta cifra excluye los documentos nuevos de esta revisión. MiB = 1.048.576 bytes; la diferencia con aproximadamente 84 MB procede de las unidades/redondeo.

| Categoría | Archivos | Bytes | MiB |
|---|---:|---:|---:|
| Logs | 4 | 5031 | 0,005 |
| Metadatos y manifiestos | 4 | 103227 | 0,098 |
| ZIP diagnóstico | 1 | 52031383 | 49,621 |
| Documentación | 3 | 28851 | 0,028 |
| Informes resultados | 11 | 7885496 | 7,520 |
| PNG cierre | 176 | 25688444 | 24,498 |
| PNG FAIL históricos | 10 | 1539959 | 1,469 |

- [Inventario completo CSV](inventario-originales.csv) y [JSON](inventario-originales.json): ruta de cada uno de los 209 archivos, bytes, SHA-256, propósito, categoría y destino propuesto.
- [Conservar en el repositorio](conservar-en-repositorio.csv): lista exacta de 67 archivos originales, incluidos 45 PNG.
- [Archivar fuera de Git](archivar-fuera-de-git.csv): lista exacta de 142 archivos originales; 141 PNG no seleccionados y el ZIP actual.
- [Contenido del ZIP](contenido-zip.csv): 393 entradas, tamaños comprimidos/sin comprimir y SHA-256 comprobados contra el manifiesto original. No se extrajo ni modificó el ZIP.

## Duplicados y redundancia

Se calcularon hashes de los 209 archivos. Hay **9 grupos de duplicados exactos sueltos**, con 2.745.868 bytes de contenido repetido (2,619 MiB):

- Dos pares de informes: `resultados-chrome.json` = `resultados-chrome-cierre.json`, y el par equivalente de Edge. Se recomienda conservar los cuatro, aun ocupando 1.867.646 bytes duplicados, para preservar nombres, enlaces y bytes originales de los informes completos.
- `lint.txt` = `lint-cierre.txt`: 37 bytes idénticos. Conservar ambos; los comandos y fechas diferenciados están en validacion.json.
- Seis pares de PNG de cierre: `01-dashboard-ultima-hora.png` y `02-tooltips-tres-graficos.png` son idénticos para cada combinación de navegador/viewport. Conservar el dashboard; archivar la captura genérica 02. Los PNG individuales con tooltip activo son distintos y se seleccionan como evidencia.

Además, los diez PNG sueltos de FAIL históricos están dentro del ZIP con el mismo SHA-256: 1.539.959 bytes sueltos duplicados respecto de su contenido archivado. Dentro del ZIP hay siete pares adicionales de imágenes idénticas. Estos ahorros potenciales no se suman al ahorro de la propuesta: algunos se solapan y los informes duplicados se conservan.

Listados: [duplicados sueltos](duplicados-exactos.csv), [duplicados internos del ZIP](duplicados-dentro-zip.csv). Muchas otras capturas son redundantes para la entrega aunque sus bytes difieren por timestamps, hover o resolución; no se etiquetan como duplicados exactos.

El ZIP actual pesa **52.031.383 bytes (49,621 MiB)** y contiene 57.236.669 bytes de PNG. La compresión sólo ahorra aproximadamente un 9,1 % frente a su contenido; recomprimir PNG/ZIP no es la vía principal de reducción. No se propone convertir, redimensionar ni recortar las capturas originales.

## Qué conservar en Git

Conservar **los 22 archivos originales que no son PNG ni ZIP**, sin reescribirlos en esta fase: los once informes resultados-*.json (incluidos todos los intentos y sus alias), resumen.json, entorno.json, validacion.json, el manifiesto original, cuatro logs y README.md/matriz-pruebas.md/defectos.md. Suman 8.022.605 bytes (7,651 MiB).

Añadir los documentos de esta revisión y las **45 capturas** que siguen. Se mantienen sus nombres y rutas; no se duplican en una nueva carpeta de seleccionadas. La tabla concreta y los hashes permiten revisar el conjunto antes de cualquier traslado.

| Captura original que se conservaría | Bytes | Propósito |
|---|---:|---|
| [capturas/chrome/1366x768/cierre/01-dashboard-ultima-hora.png](../capturas/chrome/1366x768/cierre/01-dashboard-ultima-hora.png) | 139767 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |
| [capturas/chrome/1366x768/cierre/02-tooltip-co2.png](../capturas/chrome/1366x768/cierre/02-tooltip-co2.png) | 146701 | Tooltip activo de cada variable en Chrome laptop |
| [capturas/chrome/1366x768/cierre/02-tooltip-humedad.png](../capturas/chrome/1366x768/cierre/02-tooltip-humedad.png) | 148238 | Tooltip activo de cada variable en Chrome laptop |
| [capturas/chrome/1366x768/cierre/02-tooltip-temperatura.png](../capturas/chrome/1366x768/cierre/02-tooltip-temperatura.png) | 147342 | Tooltip activo de cada variable en Chrome laptop |
| [capturas/chrome/1366x768/cierre/03-24-horas-5760-puntos.png](../capturas/chrome/1366x768/cierre/03-24-horas-5760-puntos.png) | 141252 | Histórico de 24 horas con 5760 puntos por variable |
| [capturas/chrome/1366x768/cierre/04-personalizado-valido-teclado.png](../capturas/chrome/1366x768/cierre/04-personalizado-valido-teclado.png) | 150554 | Rango personalizado y foco visible; controles móviles y laptop |
| [capturas/chrome/360x800/antes/17-valores-numericos-largos.png](../capturas/chrome/360x800/antes/17-valores-numericos-largos.png) | 170422 | Antes de corregir D87-01/D87-02: evidencia visual original del desbordamiento |
| [capturas/chrome/360x800/antes/18-textos-aire-sin-espacios.png](../capturas/chrome/360x800/antes/18-textos-aire-sin-espacios.png) | 207595 | Antes de corregir D87-01/D87-02: evidencia visual original del desbordamiento |
| [capturas/chrome/360x800/cierre/01-dashboard-ultima-hora.png](../capturas/chrome/360x800/cierre/01-dashboard-ultima-hora.png) | 143374 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |
| [capturas/chrome/360x800/cierre/04-personalizado-valido-teclado.png](../capturas/chrome/360x800/cierre/04-personalizado-valido-teclado.png) | 155875 | Rango personalizado y foco visible; controles móviles y laptop |
| [capturas/chrome/360x800/cierre/09-cargando.png](../capturas/chrome/360x800/cierre/09-cargando.png) | 89011 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/chrome/360x800/cierre/10-error-inicial.png](../capturas/chrome/360x800/cierre/10-error-inicial.png) | 102735 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/chrome/360x800/cierre/11-sin-datos.png](../capturas/chrome/360x800/cierre/11-sin-datos.png) | 88209 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/chrome/360x800/cierre/12-datos-antiguos.png](../capturas/chrome/360x800/cierre/12-datos-antiguos.png) | 151008 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/chrome/360x800/cierre/14-aire-bueno.png](../capturas/chrome/360x800/cierre/14-aire-bueno.png) | 141945 | Clasificación y color del aire; moderado ya visible en el dashboard representativo |
| [capturas/chrome/360x800/cierre/14-aire-malo.png](../capturas/chrome/360x800/cierre/14-aire-malo.png) | 141383 | Clasificación y color del aire; moderado ya visible en el dashboard representativo |
| [capturas/chrome/360x800/cierre/15-alertas-patch-fallido-reintento.png](../capturas/chrome/360x800/cierre/15-alertas-patch-fallido-reintento.png) | 116270 | Alertas: error de PATCH visible y desaparición del banner después del reintento |
| [capturas/chrome/360x800/cierre/15-patch-fallido.png](../capturas/chrome/360x800/cierre/15-patch-fallido.png) | 145478 | Alertas: error de PATCH visible y desaparición del banner después del reintento |
| [capturas/chrome/360x800/cierre/17-valores-numericos-largos.png](../capturas/chrome/360x800/cierre/17-valores-numericos-largos.png) | 165175 | Después de corregir D87-01/D87-02; prueba visual en ambos navegadores |
| [capturas/chrome/360x800/cierre/18-textos-aire-sin-espacios.png](../capturas/chrome/360x800/cierre/18-textos-aire-sin-espacios.png) | 154760 | Después de corregir D87-01/D87-02; prueba visual en ambos navegadores |
| [capturas/chrome/360x800/cierre/19-polling-error-conserva-recuperacion.png](../capturas/chrome/360x800/cierre/19-polling-error-conserva-recuperacion.png) | 142949 | Recuperación tras error de actualización con polling real |
| [capturas/chrome/768x1024/cierre/01-dashboard-ultima-hora.png](../capturas/chrome/768x1024/cierre/01-dashboard-ultima-hora.png) | 155921 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |
| [capturas/chrome/901x900/cierre/20-breakpoint.png](../capturas/chrome/901x900/cierre/20-breakpoint.png) | 141803 | Breakpoint de 900px: tarjetas y personalizados en tres columnas |
| [capturas/edge/1366x768/cierre/01-dashboard-ultima-hora.png](../capturas/edge/1366x768/cierre/01-dashboard-ultima-hora.png) | 138923 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |
| [capturas/edge/1366x768/cierre/04-personalizado-valido-teclado.png](../capturas/edge/1366x768/cierre/04-personalizado-valido-teclado.png) | 150074 | Rango personalizado y foco visible; controles móviles y laptop |
| [capturas/edge/360x800/cierre/01-dashboard-ultima-hora.png](../capturas/edge/360x800/cierre/01-dashboard-ultima-hora.png) | 143869 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |
| [capturas/edge/360x800/cierre/02-tooltip-co2.png](../capturas/edge/360x800/cierre/02-tooltip-co2.png) | 149459 | Tooltip activo de cada variable en Edge móvil emulado |
| [capturas/edge/360x800/cierre/02-tooltip-humedad.png](../capturas/edge/360x800/cierre/02-tooltip-humedad.png) | 149380 | Tooltip activo de cada variable en Edge móvil emulado |
| [capturas/edge/360x800/cierre/02-tooltip-temperatura.png](../capturas/edge/360x800/cierre/02-tooltip-temperatura.png) | 148742 | Tooltip activo de cada variable en Edge móvil emulado |
| [capturas/edge/360x800/cierre/04-personalizado-valido-teclado.png](../capturas/edge/360x800/cierre/04-personalizado-valido-teclado.png) | 156549 | Rango personalizado y foco visible; controles móviles y laptop |
| [capturas/edge/360x800/cierre/07-fechas-iguales-un-punto.png](../capturas/edge/360x800/cierre/07-fechas-iguales-un-punto.png) | 151441 | Extremos iguales y un único punto por gráfico |
| [capturas/edge/360x800/cierre/09-cargando.png](../capturas/edge/360x800/cierre/09-cargando.png) | 88905 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/edge/360x800/cierre/10-error-inicial.png](../capturas/edge/360x800/cierre/10-error-inicial.png) | 102868 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/edge/360x800/cierre/11-sin-datos.png](../capturas/edge/360x800/cierre/11-sin-datos.png) | 88371 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/edge/360x800/cierre/12-datos-antiguos.png](../capturas/edge/360x800/cierre/12-datos-antiguos.png) | 151503 | Estado importante en móvil; el informe conserva el resultado en las otras resoluciones |
| [capturas/edge/360x800/cierre/14-aire-bueno.png](../capturas/edge/360x800/cierre/14-aire-bueno.png) | 142277 | Clasificación y color del aire; moderado ya visible en el dashboard representativo |
| [capturas/edge/360x800/cierre/14-aire-malo.png](../capturas/edge/360x800/cierre/14-aire-malo.png) | 140970 | Clasificación y color del aire; moderado ya visible en el dashboard representativo |
| [capturas/edge/360x800/cierre/15-alertas-patch-fallido-reintento.png](../capturas/edge/360x800/cierre/15-alertas-patch-fallido-reintento.png) | 116398 | Alertas: error de PATCH visible y desaparición del banner después del reintento |
| [capturas/edge/360x800/cierre/15-patch-fallido.png](../capturas/edge/360x800/cierre/15-patch-fallido.png) | 145588 | Alertas: error de PATCH visible y desaparición del banner después del reintento |
| [capturas/edge/360x800/cierre/17-valores-numericos-largos.png](../capturas/edge/360x800/cierre/17-valores-numericos-largos.png) | 165680 | Después de corregir D87-01/D87-02; prueba visual en ambos navegadores |
| [capturas/edge/360x800/cierre/18-textos-aire-sin-espacios.png](../capturas/edge/360x800/cierre/18-textos-aire-sin-espacios.png) | 155187 | Después de corregir D87-01/D87-02; prueba visual en ambos navegadores |
| [capturas/edge/360x800/cierre/19-polling-error-conserva-recuperacion.png](../capturas/edge/360x800/cierre/19-polling-error-conserva-recuperacion.png) | 143710 | Recuperación tras error de actualización con polling real |
| [capturas/edge/600x900/cierre/20-breakpoint.png](../capturas/edge/600x900/cierre/20-breakpoint.png) | 163540 | Breakpoint de 600px: banner apilado |
| [capturas/edge/621x900/cierre/20-breakpoint.png](../capturas/edge/621x900/cierre/20-breakpoint.png) | 169493 | Breakpoint de 620px: controles y botón después de la transición |
| [capturas/edge/768x1024/cierre/01-dashboard-ultima-hora.png](../capturas/edge/768x1024/cierre/01-dashboard-ultima-hora.png) | 156331 | Dashboard completo: tarjetas, cero válido, tres gráficos, rango de una hora, aire moderado y banner; cobertura de la combinación navegador/viewport |

Cobertura de esta selección: seis dashboards (Chrome/Edge × 360/768/1366), cuatro personalizados con foco (móvil/laptop de cada navegador), ocho estados de carga/error/vacío/antigüedad, cuatro clasificaciones bueno/malo (moderado aparece en los dashboards), seis tooltips activos, dos casos de históricos (24 horas/5760 puntos y un solo punto), cuatro capturas de PATCH fallido/reintento completado, seis capturas de defectos (dos antes y cuatro después), tres breakpoints y dos recuperaciones del polling. Cada captura corresponde a una ejecución ya registrada; seleccionar menos PNG no elimina resultados ni cambia escenarios ejecutados.

## Qué archivar y dónde

Sacar del conjunto destinado a Git, en una fase posterior, los **142 archivos** de archivar-fuera-de-git.csv: el ZIP de diagnóstico (49,621 MiB), 133 PNG adicionales de cierre y ocho PNG sueltos de errores de la herramienta. Los dos PNG históricos de los defectos del producto permanecen visibles en Git. Los ocho FAIL de la herramienta siguen íntegros en sus JSON originales y en el archivo visual externo.

Destino propuesto: un archivo de entrega **externo a Git**, denominado `t5-8-originales-2026-10-22`, con copia íntegra de los 209 originales y el inventario de hashes. Puede ser un directorio de respaldo o un artefacto comprimido; no se creó ninguno ni se presupone un enlace de descarga. Debe conservar el ZIP actual tal cual y todas las rutas relativas. Ubicación, enlace estable y checksum del paquete completo: pendientes de definir y verificar antes de reducir el árbol de entrega. Mantener el mismo ZIP pesado en otra carpeta versionada del repositorio no reduce su tamaño.

Esta propuesta no elimina evidencia: los originales completos se conservan en el archivo externo. El tamaño de 13,76 MiB corresponde al conjunto original seleccionado para Git, no al respaldo completo.

## Referencias de los JSON y cambios de documentación posteriores

Los once informes originales deben conservarse **byte por byte**, sin cambiar sus rutas screenshot ni sus cifras. Se encontraron **569 rutas de captura distintas**: 186 tienen PNG suelto y 393 están en el ZIP; diez rutas existen en ambos. La unión es 569, no 579. La propuesta deja 45 rutas directamente en Git y **524 rutas de consulta mediante archivo externo**. Seis de estas últimas también tienen un PNG seleccionado idéntico disponible como equivalente local.

El [catálogo por referencia](referencias-capturas.csv) identifica cada ruta, los informes que la citan, su disponibilidad actual y el destino propuesto. No considera una ruta archivada como enlace directo válido dentro de Git. Para la fase de reducción se requiere:

1. Conservar los JSON sin modificaciones y registrar en un catálogo nuevo la ubicación real del archivo externo, su checksum y la ruta interna de cada PNG. Completar el destino real antes de retirar originales del árbol de entrega.
2. Actualizar README.md para distinguir capturas seleccionadas y archivadas, enlazar el paquete real, su manifiesto y las instrucciones de recuperación. Mantener las cifras y explicar que resumen.json es el registro original previo a la reducción, no el contador vigente de PNG sueltos.
3. En matriz-pruebas.md, conservar cada PASS/FAIL y sustituir sólo los enlaces visuales no seleccionados por enlaces al catálogo con la etiqueta «captura archivada». Los seis dashboards, y los resultados completos, siguen disponibles en Git. No enlazar una imagen distinta como si fuera la captura original del escenario.
4. En defectos.md, los enlaces antes/después de los dos defectos mantienen sus rutas porque los seis PNG están seleccionados. Añadir únicamente la ubicación del archivo externo para los errores de preparación de la herramienta.
5. Para recuperar evidencias antiguas: obtener la copia completa, verificar hashes, localizar los PNG sueltos por la ruta del catálogo y, para las capturas históricas que ya estaban comprimidas, abrir el ZIP de diagnóstico conservando sus rutas internas. Verificar contra contenido-zip.csv y el manifiesto original.

**No se editaron README.md, matriz-pruebas.md ni defectos.md ahora**, porque no se cambió ninguna ruta ni se ejecutó la reducción. Los enlaces existentes siguen en su estado original (PNG suelto o archivo ZIP expresamente documentado).

## Tamaño previsto

| Conjunto | Archivos originales | Bytes | MiB |
|---|---:|---:|---:|
| Estado original | 209 | 87.282.391 | 83,239 |
| Propuesta para Git, sin documentos nuevos | 67 | 14.429.630 | 13,761 |
| Fuera del conjunto de Git | 142 | 72.852.761 | 69,478 |

La selección de 45 PNG pesa 6.407.025 bytes (6,110 MiB). La reducción sobre originales sería **83,47 %**. Incluyendo los once archivos nuevos de inventario, catálogos y propuesta, el cálculo actual es **aproximadamente 14,21 MiB**; reservar **14,3 MiB** para la entrega reducida tras completar enlaces y documentación. El resultado exacto dependerá de esos cambios cuando se concrete el archivo externo. No se ha reducido todavía el tamaño real de la carpeta.

## Riesgos y controles

- Sin un archivo externo accesible y verificado se perdería acceso visual a 524 rutas, aunque los resultados completos sigan en Git. Por eso la ubicación y recuperación deben quedar resueltas antes de aplicar el plan.
- Algunas capturas de estados en resoluciones no seleccionadas y los errores de validación/cancelación quedarían fuera de Git. Su resultado y geometría se conservan en JSON, y el PNG original en el archivo completo. La selección es evidencia representativa; no debe presentarse como un nuevo conjunto de pruebas.
- Los ocho FAIL de preparación de la herramienta no se reclasifican ni desaparecen de los registros. Siguen siendo diez FAIL históricos contando los dos del producto. Los 146 PASS y cero FAIL de cierre tampoco cambian.
- Un archivo externo dentro de una carpeta local ignorada no es por sí solo un respaldo compartido durable. Debe poder recuperarlo otra persona; no basta con escribir un nombre de archivo o un checksum sin acceso al contenido.
- Convertir o reducir resolución de imágenes puede ocultar defectos o foco. No se propone hacerlo; la reducción se basa en selección y archivo sin alterar bytes.
- No se obtuvo evidencia nueva en teléfono físico. Esa limitación original permanece.

## Verificación de esta tarea

Consultar [verificacion-originales.json](verificacion-originales.json): comparación posterior de tamaños y SHA-256 de los 209 originales contra el inventario inicial, y estado de los resultados. No se ejecutaron git add, commit, push ni PR. Los únicos archivos nuevos están en revision-entrega/.
