# AI_Multiplayer — coop para Alien: Isolation (WIP)

Mod de investigación/interoperabilidad, uso personal, sobre copia comprada del juego.
No contiene ni redistribuye binarios, assets ni código del juego.

**Estado:** etapa 0 — identificación de build y setup de análisis. Nada jugable todavía.

## Build soportada

| Campo | Valor |
|-------|-------|
| Ejecutable | `AI.exe` (Windows, PE32 i386) — corrido en Linux vía Proton |
| SHA-256 | `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90` |
| Steam buildid | 4940658 |
| PE timestamp | 0x54E3199B (2015-02-17) |

Los dos jugadores **deben** tener exactamente este hash.

## Estructura

- `mod/` — código del mod (CMake, cross-compilado con mingw-w64, i686).
- `work/NOTES.md` — bitácora y preguntas abiertas.
- `work/findings/` — un `.md` por hallazgo de RE, con evidencia.
- `game/` y `work/evidence/` — locales, ignorados por git.

## Cómo cargar / hostear / unirse

Pendiente (fase 1 en adelante).
