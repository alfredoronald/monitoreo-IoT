# Referencias de diseño de T-5.3

Se revisaron ejemplos públicos y documentación de interfaces de monitoreo ambiental:

- [Airthings: dashboard](https://www.airthings.com/business/dashboard). Referencia para tarjetas con una lectura principal, identificación del espacio y separación entre resumen e histórico.
- [AirGradient: mediciones e interpretación del dashboard](https://www.airgradient.com/documentation/kb/airgradient-dashboard-measurement-and-interpretation). Referencia para nombres de variables, unidades explícitas y contexto de última actualización.

## Aplicación al proyecto

- Tres tarjetas: temperatura, humedad relativa y CO₂ estimado, con unidades visibles e iconos propios en SVG.
- Histórico separado, con el mismo color de identificación por variable.
- Espacio para la última lectura y mensajes explícitos cuando no hay datos.
- Distribución en tres columnas en escritorio y una en celular; tipografía secundaria de al menos 12 px.
- Se conserva el aviso de estimación provisional del MQ-135. Los colores identifican variables: no representan niveles de riesgo ni un índice AQI.

Se adaptó la organización visual al alcance del proyecto. No se incorporaron imágenes, logotipos ni código de los productos consultados. Las lecturas reales y las gráficas corresponden a las siguientes tareas; esta entrega es el layout.
