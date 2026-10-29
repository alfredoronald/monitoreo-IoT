# Historico compartido del 28-29 de octubre de 2026

Descargar **historico-2026-10-29.zip** y extraerlo completo. El ZIP contiene CSV, esquema de referencia, script de importacion, instrucciones y hashes. El script SQL visible aqui es una copia para revision; necesita lecturas.csv en la carpeta extraida.

- Total de BD exportado: **10.418 registros**, incluidos registros anteriores de pruebas.
- Sesion 28/10 16:32 a 29/10 07:40 (Bolivia): **10.381 registros**, unas **15 h 8 min** con huecos.
- Temperatura de la sesion: 3.464; humedad: 3.462; CO2: 3.455.
- No son 24 h continuas ni todos los registros son CO2 calibrado. Se preservan pruebas previas y cambios de intervalo, sin modificar fechas.

## Para los companeros

1. Tener la BD y permisos del proyecto instalados. El README dentro del ZIP explica el esquema y dependencias.
2. Abrir una terminal **en la carpeta extraida** y ejecutar, adaptando puerto/usuario locales:

```bash
psql -h localhost -p 5433 -U app -d monitoreo_ambiental -W -v ON_ERROR_STOP=1 -f importar-lecturas.sql
```

3. Introducir la contrasena local cuando la pida. No se incluye ninguna en el paquete.
4. Consultar el dashboard/API del proyecto con el rango original 28-29/10/2026. Importar el CSV no arranca servicios ni crea lecturas recientes.

La importacion genera IDs locales nuevos, conserva filas existentes y evita duplicados por fecha/variable/valor al repetir. Se probaron carga completa, segunda carga idempotente y fallo de CSV incompleto en esquema aislado: **3/3 PASS**. Referencias de calibracion y umbrales son informativas y no se aplican automaticamente a otro sensor.

Subida a feature/repo-guidelines por indicacion del responsable. Es un paquete de datos y documentacion; no sustituye la integracion del codigo de las ramas de area. La BD original se conserva en la laptop.
