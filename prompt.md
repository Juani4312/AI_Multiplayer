# Prompt de trabajo: coop para Alien: Isolation usando REA

> Para Claude Code. Leé este archivo completo antes de ejecutar nada.
> Modelo recomendado: **Claude Opus 5.5 con esfuerzo alto** (elegilo con `/model`).
> Este archivo reemplaza a `ALIEN_COOP_MOD_BRIEF.md` si ambos están presentes.

---

## 0. Rutas

Rutas absolutas de la máquina del usuario. **Siempre entrecomilladas** en el shell: contienen una coma y un espacio.

```
AlienDir = "/mnt/1,8TB/SteamLibrary/steamapps/common/Alien Isolation"
READir   = "/home/juano/Documents/clones/rea"
```

Rutas de trabajo, relativas al directorio actual (`.`, donde vive este `prompt.md`):

```
./game/            # COPIA de los binarios a analizar (no se commitea)
./work/findings/   # un .md por hallazgo
./work/evidence/   # exports/snapshots de REA
./work/NOTES.md    # bitácora de la sesión
./mod/             # código del mod
```

**Reglas sobre `AlienDir`:**
- Es la instalación de Steam. **Solo lectura**: no escribir, no borrar, no renombrar nada ahí durante el análisis. Steam puede validar o pisar archivos.
- Para analizar, copiá el/los binarios a `./game/` y trabajá sobre la copia.
- Si más adelante hace falta colocar algo en `AlienDir` (por ejemplo una DLL proxy), pedí confirmación explícita antes.

`READir` es un clon del repo `https://github.com/morluto/rea`. No lo modifiques salvo que el usuario lo pida.

---

## 1. Objetivo

Hacer un mod de **Alien: Isolation** para que **dos jugadores jueguen a la vez en la misma partida**: uno hostea y el amigo se une. El juego es 100% single-player y su engine (Cathode) es cerrado, así que hay que agregar multiplayer por reverse engineering e inyección de código.

Se avanza por etapas, y cada una debe dejar algo jugable:

1. **Ghost**: cada uno en su campaña, viéndose como una entidad fantasma (MVP).
2. **Mundo compartido básico**: puertas, items y terminales sincronizados.
3. **Coop real**: ambos en la misma instancia, con la IA del Alien decidiendo entre dos objetivos. Es lo más difícil; no lo des por resuelto.

## 2. Reglas generales

- El usuario escribe en español rioplatense con términos técnicos en inglés. Respondé igual.
- El software es de uso personal, comprado por el usuario. Seguí el encuadre de `"$READir/DISCLAIMER.md"` (investigación, interoperabilidad, software que se posee). No redistribuyas assets, binarios ni código del juego. No toques DRM/licencias ni busques bypass de protecciones; si algo del binario lo complica, avisale al usuario.
- **Pedí confirmación antes de**: instalar software (Ghidra, JDK, herramientas), descargar archivos, cambiar configuración del agente o del sistema, ejecutar el juego, borrar algo.
- Si el usuario dice algo incorrecto o hay un hueco en su razonamiento, decíselo directo.
- Separá siempre **hipótesis** de **hecho verificado con evidencia**. Ningún offset sale de memoria o de suposiciones: todo sale de REA/Ghidra o de una verificación en runtime, con su evidencia registrada. Lo que no se pueda determinar va a `./work/NOTES.md` como pregunta abierta.

---

## 3. Primer paso obligatorio: identificar qué build es

La ruta de `AlienDir` está en Linux. Alien: Isolation tiene (a) una **build nativa para Linux** (portada por Feral) y (b) la **build de Windows** (`AI.exe`) que corre con Proton. Son **binarios distintos**: formato (ELF vs PE), offsets y forma de inyección cambian. No asumas cuál es.

1. Listá `"$AlienDir"` (solo lectura) y buscá el ejecutable principal. Corré `file` y `sha256sum` sobre los candidatos.
2. Registrá en `./work/NOTES.md`: nombre del binario, formato (ELF/PE), arquitectura, SHA-256, y si hay indicios de Proton (por ejemplo `AI.exe`, y un prefijo en `compatdata` de Steam).
3. **Preguntale al usuario**: ¿con qué build juega, y qué sistema operativo y build usa el amigo?

Consecuencias que tenés que explicarle al usuario antes de seguir:

| Caso | Análisis | Carga del mod |
|------|----------|---------------|
| Build nativa Linux (ELF) | REA con ELF; mitigaciones y símbolos según la herramienta | Objeto compartido `.so` por `LD_PRELOAD` (opción de lanzamiento de Steam) |
| Build Windows vía Proton (PE) | REA con PE (`AI.exe`) | DLL cross-compilada (mingw-w64) cargada como proxy (`dinput8.dll` o similar), con override de DLL de Wine en las opciones de lanzamiento |

**Punto crítico:** los dos jugadores deben usar **la misma build y el mismo hash**. Si el usuario juega en Linux nativo y el amigo en Windows, los binarios son distintos y habría que mantener offsets para ambos, o que los dos usen la build de Windows (el usuario vía Proton). Planteale esto como decisión de diseño y esperá su respuesta antes de invertir en un solo lado.

---

## 4. Setup de REA (usando el repo local)

REA es un servidor MCP + CLI de reverse engineering para agentes. Corre todo local y devuelve evidencia y limitaciones de cada conclusión.

### 4.1 Leer primero, en este orden

1. `"$READir/README.md"`
2. `"$READir/AGENTS.md"`
3. `"$READir/docs/installation.md"` (registro de agentes, proveedores, y **"Check readiness for your task"**)
4. `"$READir/docs/cli.md"` (comandos, selección de proveedor, snapshots, import/export)
5. `"$READir/docs/native-investigation.md"`
6. `"$READir/docs/binary-diagnostics.md"` y `"$READir/docs/pe-imports.md"` / `"$READir/docs/elf-notes.md"` según el formato del binario
7. `"$READir/docs/mcp-contracts.md"` (catálogo de tools y contratos de resultado)
8. `"$READir/DISCLAIMER.md"`

No inventes comandos de build ni de ejecución: leé `"$READir/package.json"` y la documentación, y proponé al usuario los comandos exactos antes de correrlos.

### 4.2 Requisitos (según el README)

- Node.js 22.x (>=22.23.4), 24.x (>=24.22.1) o 26.12.1+, más npm. Verificá con `node --version`.
- Un motor de análisis nativo: Hopper, Ghidra, IDA o Kuna. Para este proyecto lo razonable es **Ghidra** (gratis). REA no lo instala: pedile al usuario la ruta o confirmá antes de instalarlo (Ghidra requiere un JDK).
- En **Linux**, para Ghidra, JADX, firmware y análisis JS/Electron, REA necesita un `ps` compatible con procps en el `PATH` (chequeo de ownership de procesos). Verificalo; hay recuperación en `docs/installation.md`, sección "Linux owned-process prerequisite".
- Para binarios grandes, subí el timeout de arranque del proveedor: `REA_GHIDRA_STARTUP_TIMEOUT_MS` con un valor alto.

### 4.3 Registro como MCP

El README documenta `npx rea-agents setup`, y advierte que la adquisición del paquete se haga desde un **directorio de confianza fuera del proyecto objetivo**, y que se reinicie el agente después. Como acá se usa el **repo local**, mirá en `docs/installation.md` ("manual MCP registration") cómo apuntar el servidor MCP al entry script de `"$READir"`. Proponé el comando o la configuración exacta al usuario antes de aplicarla (el setup hace backup de la config existente). Después pedile que reinicie Claude Code y verificá que aparecen las tools de REA.

### 4.4 Checklist de readiness (registrar en `./work/NOTES.md`)

- [ ] Versión de Node OK
- [ ] `ps` compatible con procps disponible
- [ ] REA responde (`--help` o una tool MCP trivial)
- [ ] El proveedor (Ghidra) arranca y abre el binario copiado en `./game/`
- [ ] Hash del binario registrado

---

## 5. Workflow de análisis

Una pregunta concreta por investigación, con evidencia guardada. Cada hallazgo va en `./work/findings/NN-nombre.md`:

```
# NN - <título>
- Pregunta:
- Build y SHA-256 del binario:
- Dirección(es) / RVA / offset:
- Evidencia (pseudocódigo/asm/strings/xrefs; ruta en ./work/evidence/):
- Confianza: alta | media | baja
- Cómo verificar en runtime:
- Verificado en runtime: sí | no
- Preguntas abiertas:
```

### 5.1 Orden de investigación

| # | Objetivo | Para qué sirve |
|---|----------|----------------|
| 01 | Inventario del binario: formato, imports/librerías, strings relevantes, símbolos disponibles | Orientación; detectar protecciones |
| 02 | **Game loop / tick principal** | Punto estable para enganchar código por frame |
| 03 | **Entidad del jugador**: posición, rotación, estado | Leer y enviar el estado local |
| 04 | **Cámara y transform del mundo** | Ubicar al otro jugador correctamente |
| 05 | **Creación de entidades** y asignación de modelos/animaciones | Crear al "segundo jugador" |
| 06 | **Sistema de scripts del nivel** (puertas, terminales, triggers, comandos Cathode) | Sincronizar eventos |
| 07 | **Items e inventario** | Sincronizar pickups y crafting |
| 08 | **IA del Alien**: selección de objetivo, Director AI, menace gauge | Coop real con dos objetivos |
| 09 | **Guardado, checkpoints y carga de nivel** | Mantener a ambos en la misma etapa |

Para ubicar dónde mirar podés apoyarte en las herramientas de la comunidad (OpenCAGE) para formatos de assets y comandos de scripts, pero **nunca** como fuente de offsets sin verificar.

### 5.2 Cuándo parar

Pará y resumile al usuario el estado al terminar cada hallazgo, cuando REA devuelva resultados contradictorios o con límites importantes, o cuando una decisión de diseño cambie el alcance (por ejemplo, si el Alien resulta inseparable de un único objetivo).

---

## 6. Desarrollo del mod (`./mod`)

La arquitectura depende del resultado de la sección 3. Esqueleto general (validalo contra los hallazgos):

- **Red**: modelo **host-autoritativo** sobre UDP con **ENet** (o GameNetworkingSockets). El host expone IP:puerto. Para conectar a los dos sin port-forwarding ni problemas de CGNAT: **Tailscale** o ZeroTier.
- **Hooks**: siempre contra direcciones verificadas en la sección 5. La librería de hooking depende de la plataforma (evaluá las opciones según el caso de la tabla de la sección 3; no asumas MinHook en una build ELF).
- **Render del otro jugador**: entidad con interpolación de transform a partir de paquetes.
- **Build**: CMake. Si el caso es Proton, cross-compilación con mingw-w64.

Fases, sin saltear ninguna:

1. Código cargado y log por frame (prueba de que estamos dentro).
2. Leer posición/rotación del jugador y mostrarlas.
3. Cliente/servidor UDP mínimo con esa posición (local, y luego por Tailscale).
4. Representar al otro jugador en el mundo.
5. Sincronizar eventos básicos (puertas, items).
6. Recién entonces, la IA del Alien.

Cada fase termina con una forma concreta de probarla, registrada en `./work/NOTES.md`.

Higiene: `.gitignore` para `game/`, `work/evidence/` (si contiene material del juego) y binarios compilados. El README del mod debe indicar build soportada, hash esperado, cómo cargar el mod, cómo hostear y cómo unirse.

---

## 7. Arranque

Cuando termines de leer este archivo:

1. Ejecutá la sección 3 (identificar la build) y hacele al usuario las preguntas pendientes.
2. Leé la documentación de REA según 4.1 y proponé los comandos de setup.
3. Completá el checklist 4.4 y empezá con el hallazgo 01.

Si algo de este prompt contradice lo que ves en la documentación de REA, ganá la documentación y avisale al usuario.
