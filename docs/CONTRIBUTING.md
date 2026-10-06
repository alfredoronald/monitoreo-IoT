# Convenciones del repositorio — monitoreo-IoT

Grupo 22 · Servicios Telemáticos · UMSS

Léelo completo una vez antes de crear tu primera rama. Después vuelve solo a la tabla de la sección 1.

**Regla de oro del idioma:** lo que lee el equipo y el docente va en **español** (issues, tareas, documentación, textos de la interfaz). Lo que lee Git y el código va en **inglés** (ramas, commits, pull requests, nombres de variables y funciones). **Sin emojis** en ningún nombre, mensaje ni descripción.

---

## 1. Tabla rápida de formatos

| Qué | Idioma | Formato | Ejemplo |
|---|---|---|---|
| Issue de HU (padre) | Español | `[HU-0X] Título de la historia` | `[HU-02] Almacenar lecturas en PostgreSQL` |
| Sub-issue de tarea | Español | `T-X.Y Título de la tarea` | `T-2.4 Crear el proyecto Node.js/Express en TypeScript` |
| Rama | Inglés | `tipo/<nro-issue>-<descripcion-corta>` | `feat/23-express-typescript-setup` |
| Commit | Inglés | `tipo(alcance): resumen` | `feat(backend): add lecturas endpoint` |
| Título de PR | Inglés | `tipo(alcance): resumen (#nro-issue)` | `feat(backend): add lecturas endpoint (#23)` |
| Código: variables, funciones, clases | Inglés | según carpeta (ver §6) | `getReadingsByVariable()` |
| Textos de la interfaz, informe, `docs/` | Español | — | "Temperatura promedio" |
| Tablas y columnas de la base de datos | Español | `snake_case` (ya definidas) | `lecturas(fecha_hora, tipo_variable, valor)` |

---

## 2. Ramas

`main` es la **única rama permanente** y está protegida: nadie trabaja ni sube directo a ella. Todo cambio entra por pull request.

**Formato:** `tipo/<nro-issue>-<descripcion-corta-en-ingles>`

```
feat/23-express-typescript-setup
fix/31-mqtt-reconnect-loop
docs/12-sprint1-evidence
```

Reglas:

1. El **número del issue** es el que GitHub le asignó al sub-issue de tu tarea (el `#23` que aparece en el título).
2. La descripción va en **inglés**, en minúsculas, separada por guiones, de 2 a 5 palabras. Sin tildes, sin `ñ`, sin espacios, sin puntos.
3. El `tipo` es el mismo de la tabla de commits (§3).
4. Una rama por tarea. Si la tarea es tuya y de tu pareja, trabajan sobre la misma rama.
5. Crea la rama siempre desde `main` actualizado:

```bash
git switch main
git pull
git switch -c feat/23-express-typescript-setup
```

6. Después de que el PR se fusione, borra la rama (GitHub ofrece el botón "Delete branch").

> Si el issue trae el campo "Rama sugerida" con otro formato, manda este manual: el manual es la referencia válida.

---

## 3. Commits

**Formato:** `tipo(alcance): resumen en inglés`

```
feat(backend): add GET /api/lecturas with date filters
fix(firmware): keep MQ-135 on ADC1 to avoid wifi conflict
docs(repo): add contributing guidelines
```

### Tipos

| Tipo | Cuándo se usa |
|---|---|
| `feat` | Funcionalidad nueva |
| `fix` | Corrección de un error |
| `refactor` | Reorganizar código sin cambiar su comportamiento |
| `docs` | Documentación, informe, diagramas, evidencias |
| `test` | Pruebas nuevas o corregidas |
| `chore` | Configuración, dependencias, scripts, `.gitignore` |
| `style` | Formato, indentación, nombres (sin cambio de lógica) |

### Alcances (`scope`)

Son las carpetas del proyecto: `firmware`, `backend`, `frontend`, `db`, `infra`, `docs`, `repo`.

### Reglas del mensaje

1. **Inglés**, en **imperativo**: `add`, `fix`, `remove`, `update`. No `added`, no `adding`.
2. Todo en **minúsculas** y **sin punto final**.
3. Máximo **72 caracteres** en la primera línea.
4. Un commit = un cambio con sentido propio. No mezcles "arreglo el endpoint" con "cambio colores del dashboard".
5. Si necesitas explicar el *por qué*, deja una línea en blanco y escribe el cuerpo (también en inglés):

```
fix(firmware): add voltage divider reading for MQ-135

The sensor outputs up to 5 V and the ESP32 ADC tolerates 3.3 V.
Refs #18
```

6. Para vincular con el tablero, al final: `Refs #18` (relaciona) o `Closes #18` (cierra el issue al fusionar).

---

## 4. Pull requests

**Título:** igual que un commit, más el número del issue entre paréntesis.

```
feat(backend): add lecturas endpoint (#23)
```

**Descripción:** en inglés, con esta plantilla. Copia y rellena:

```markdown
## What
Short description of the change.

## Why
Closes #23

## How to test
1. Step one
2. Step two

## Checklist
- [ ] Acceptance criteria of the issue are met
- [ ] No secrets or .env files committed
- [ ] Tested locally
```

Reglas:

1. **Una PR por tarea.** PRs pequeñas: si pasa de unos 400 cambios de línea, pártela.
2. Abre la PR en cuanto tengas algo que mostrar, con el prefijo `Draft:` si aún no está lista. Así nadie duplica tu trabajo.
3. **Mínimo una aprobación** antes de fusionar: la de tu pareja de la HU o la del Scrum Master.
4. **Nadie fusiona su propia PR sin aprobación.**
5. Método de fusión: **Squash and merge** (el historial de `main` queda con un commit limpio por tarea).
6. Si hay conflictos, los resuelve quien abrió la PR, actualizando su rama:

```bash
git switch main && git pull
git switch feat/23-express-typescript-setup
git merge main
```

7. Al fusionar: mueve la tarjeta a **Terminado** en el tablero y marca los criterios de aceptación del issue.

### Cómo revisar (para quien aprueba)

- Comprueba los **criterios de aceptación** del issue, no el estilo personal.
- Comenta con sugerencias concretas, no con juicios. Si algo es opcional, escríbelo como `nit:`.
- Responde en menos de 24 horas: una PR parada bloquea el sprint.

---

## 5. Issues y tablero

Las tarjetas ya se crean con este formato; respétalo al editarlas o al crear nuevas:

- **Issue padre** = una HU completa. **Sub-issues** = sus tareas.
- Cuerpo del sub-issue, en español: `Descripción técnica`, `Task: ... · Sprint 1 · DX → DY`, `Cubre`, `Rama sugerida`, `Criterios de aceptación` (checklist), `Dependencias`.
- Campos obligatorios de la barra lateral: **Assignees**, **Labels**, **Status**, **Sprint**, **Priority**, **Estimate** y **Milestone**.
- Estados: `Por hacer` → `En progreso` → `En revisión` → `Terminado`. Mueve la tarjeta **el mismo día** en que cambia.
- Si descubres trabajo que no está en el tablero, crea el issue. Nada se trabaja sin tarjeta.

---

## 6. Nombres de archivos y de código

Identificadores (variables, funciones, clases, componentes) **siempre en inglés**. Los textos que ve el usuario, en español.

| Carpeta | Convención | Ejemplo |
|---|---|---|
| `backend/` | archivos y funciones `camelCase`, clases `PascalCase` | `readingService.ts`, `getReadings()` |
| `frontend/` | componentes `PascalCase.tsx`, hooks `useAlgo.ts`, utilidades `camelCase.ts` | `ReadingsChart.tsx`, `useReadings.ts` |
| `firmware/` | archivos y funciones `snake_case` | `sensor_dht22.cpp`, `read_temperature()` |
| `database/` | migraciones numeradas `NNN_accion_objeto.sql` | `001_create_lecturas.sql` |
| `docs/` | `kebab-case` en español | `informe-sprint-1.md`, `diagrama-arquitectura.png` |
| `infra/` | todo en minúsculas | `mosquitto.conf` |

Las tablas y columnas de la base de datos se quedan como están definidas en la planificación (`lecturas`, `fecha_hora`, `tipo_variable`, `valor`): español, `snake_case`, singular en columnas y plural en tablas.

---

## 7. Lo que nunca se sube al repo

- Archivos `.env`, `firmware/include/config.h`, `infra/mosquitto/passwd`: contienen contraseñas y credenciales de WiFi. Si subes uno por error, avisa de inmediato: la contraseña queda en el historial y hay que cambiarla.
- `node_modules/`, `dist/`, `.pio/`, `coverage/`, `.tsbuildinfo`: se generan con un comando, no se versionan.
- Capturas o binarios pesados sueltos: las evidencias van en `docs/evidencias/` y comprimidas.

Cuando agregues una variable de configuración nueva, **añádela también a `.env.example`** con un valor de ejemplo, no con el real.

---

## 8. Tu día a día, paso a paso

1. Toma tu tarjeta del tablero y pásala a **En progreso**.
2. `git switch main && git pull`
3. Crea tu rama con el formato de §2.
4. Trabaja en commits pequeños, con el formato de §3.
5. `git push -u origin <tu-rama>`
6. Abre la PR con el formato de §4 y pide revisión a tu pareja. Pasa la tarjeta a **En revisión**.
7. Atiende los comentarios con commits nuevos en la misma rama.
8. Con la aprobación, **Squash and merge**, borra la rama y pasa la tarjeta a **Terminado**.

---

## 9. Antes de pedir revisión: revisa esto

- [ ] El proyecto compila y arranca (`npm run dev`, o compila el firmware).
- [ ] Cumplí **todos** los criterios de aceptación del issue.
- [ ] No subí `.env`, credenciales ni carpetas generadas.
- [ ] Mis commits están en inglés y con el formato `tipo(alcance): resumen`.
- [ ] La PR enlaza su issue (`Closes #NN`).
- [ ] Si agregué configuración, actualicé `.env.example` y el `README.md`.

---

## 10. Dudas sobre las convenciones

Si algo no está cubierto aquí, decide lo más parecido a lo que ya existe en el repo y coméntalo en el canal del equipo. 
