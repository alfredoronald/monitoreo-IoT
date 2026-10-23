# Evidencias archivadas localmente — T-5.8

Reducción aplicada el **22/10/2026**, después de copiar y verificar por SHA-256 los **209 archivos originales**. Se retiraron del conjunto destinado a Git exactamente los **142 archivos** de la lista auditada. No se alteraron informes de resultados ni logs: **146 PASS y 0 FAIL finales; diez FAIL históricos**. No se ejecutaron pruebas nuevas durante la reducción.

## Disponible en Git

Permanecen los 67 archivos originales seleccionados: **45 PNG representativos**, los once informes completos, cuatro logs, cuatro archivos de metadatos/manifiestos y tres documentos actualizados. Se conservan además los once archivos de auditoría previa de `revision-entrega/`, sin cambios; describen el estado anterior a esta reducción y no son contadores actuales.

Listas originales: [conservar](revision-entrega/conservar-en-repositorio.csv) y [retirar de Git](revision-entrega/archivar-fuera-de-git.csv). La [verificación de reducción](reduccion-verificacion.json) documenta el respaldo y los controles realizados. El [catálogo actual](catalogo-capturas.csv) distingue cada captura disponible de las archivadas.

## Capturas archivadas

Ubicación local verificada del respaldo completo:

```text
D:\telematicos\respaldos-IoT\t5-8-completo\
```

**Esta carpeta está fuera del repositorio y no estará disponible automáticamente para otros colaboradores.** No hay un enlace público ni un artefacto compartido publicado. Para consultar una captura archivada, debe solicitarse una copia al responsable del respaldo. Su ubicación local documenta dónde se preservaron los originales; no sustituye un respaldo compartido durable.

Los informes JSON conservan sus rutas `screenshot` originales sin reescritura. De las **569 rutas distintas**, 45 tienen PNG directamente en Git y **524 requieren el respaldo local**: 141 están como PNG sueltos y 383 existen únicamente dentro del ZIP de diagnóstico. El catálogo registra la ruta exacta, disponibilidad (`GIT` o `RESPALDO_LOCAL`) y contenedor (`PNG` o `ZIP`); los tamaños y SHA-256 se conservan en los inventarios originales enlazados a continuación. Algunas imágenes tienen un equivalente local idéntico, indicado expresamente; ese equivalente no se presenta como el original de otro escenario.

Los enlaces «captura archivada» de la matriz llevan a esta explicación, no a un PNG ausente. Para localizar una imagen concreta, buscar el navegador, resolución e identificador del escenario en [catalogo-capturas.csv](catalogo-capturas.csv) o utilizar la ruta del informe JSON correspondiente.

## Recuperación e integridad

1. Obtener acceso al respaldo local o una copia entregada por su responsable.
2. Para contenedor `PNG`, localizar `path` dentro del respaldo; comparar tamaño y SHA-256 contra [inventario-originales.csv](revision-entrega/inventario-originales.csv).
3. Para contenedor `ZIP`, abrir `capturas-diagnostico.zip` dentro del respaldo y recuperar la entrada cuyo nombre es `path` en una carpeta de trabajo separada, conservando la ruta relativa.
4. Comparar tamaño y SHA-256 de esa entrada contra [contenido-zip.csv](revision-entrega/contenido-zip.csv) o el [manifiesto original](capturas-diagnostico-manifiesto.json). Los inventarios conservan los 209 originales y las 393 entradas del ZIP sin duplicar sus datos en el catálogo nuevo.

El ZIP respaldado conserva el SHA-256 `9046b380f43a6a878c84a8ac3d0cd4c55f8a27bade0b52853074d942e650f620`. No se transformó ninguna imagen. Las versiones originales de README, matriz y defectos también permanecen en el respaldo; sus versiones de Git sólo se actualizan para reflejar la disponibilidad de evidencias.

`resumen.json`, los informes y los once archivos de auditoría son registros originales de las fases anteriores. Sus cantidades históricas de PNG y sus campos de propuesta no se reescriben. El estado actual de la entrega se registra en `reduccion-verificacion.json` y este catálogo.

## Limitaciones

La selección cubre ambos navegadores, los tres viewports y los dos defectos corregidos. La cobertura automatizada sigue documentada por todos los resultados completos, aunque no se conserve en Git un PNG por cada ejecución. El respaldo local debe compartirse mediante un medio acordado si otro colaborador necesita las capturas archivadas. Permanece la limitación original: viewports Windows headless emulados, sin prueba en teléfono físico ni integración real con backend en esta matriz.
