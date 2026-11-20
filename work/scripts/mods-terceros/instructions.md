# mods/ — mods de terceros, con activar/desactivar

Cada mod tiene su subcarpeta, con el contenido del comprimido extraído (los comprimidos originales quedan al lado).
Nada de esta carpeta lo carga el juego: un mod está activo solo cuando su `toggle.sh` copió sus archivos al juego.
Cómo se instala cada uno, y de dónde salió esa instrucción: su `README`/`readme` o su `INSTALACION.md`.

| Carpeta | Qué hace | Interfiere con el coop | Estado |
|---------|----------|------------------------|--------|
| `NoHUD-crosshair/` | `UI.PAK` sin HUD (con mira) → `DATA/UI.PAK` | **sí**: el coop usa el HUD, los diálogos y el menú | `toggle.sh` (README) |
| `QuietXeno/` | sonidos del Alien más bajos → `DATA/SOUND/ALIEN.BNK` | no (solo audio) | `toggle.sh` (readme) |
| `SmartAI/` | IA del "Insane Mod" → `DATA/{ALIENCONFIGS,BEHAVIOR,BINARY_BEHAVIOR,CHR_INFO}` (121 archivos) | **sí, mucho** | `toggle.sh` (página del mod: `INSTALACION.md`) |
| `AIMovieFix/` | DLL del Cutscene Remaster → `xinput1_3.dll` en la raíz | solo con la variante `xinput1_3` del coop | `toggle.sh` (página del remaster: `INSTALACION.md`); los videos 4K ya estaban en `DATA/UI/MOVIES`, sin toggle |

## Scripts

Con el juego **cerrado** (si está abierto, no hacen nada):

```sh
./toggle-mod.sh --lista                  # todos los mods y su estado
./toggle-mod.sh NoHUD-crosshair          # alterna (activo ↔ inactivo)
./toggle-mod.sh QuietXeno on|off|status  # o explícito
./mods-desarrollo.sh desactivar          # apaga los que interfieren con el coop y anota cuáles
./mods-desarrollo.sh reactivar           # vuelve a prender solo esos
./mods-desarrollo.sh status
```

- `<mod>/toggle.sh [on|off|status|info]` hace el trabajo; `toggle-mod.sh` solo valida el nombre y lo llama. La lógica común
  está en `_lib/toggle-lib.sh`; cada `toggle.sh` declara qué archivo va adónde (`MOD_ARCHIVOS`) y si interfiere
  (`MOD_INTERFIERE`).
- **Backups:** al activar, cada archivo original del juego va a `<mod>/bak/orig/` y su hash a `<mod>/bak/manifiesto`
  (`bak/historial`: cuándo se activó y desactivó). Al desactivar se restaura, se verifica el hash y recién ahí se borra el
  backup.
- **A prueba de errores:** si una copia falla a mitad de camino, se deshace lo hecho. Si un archivo cambió por fuera
  (Steam → Verificar integridad, otro mod), no se toca nada y se avisa: el backup sigue en `bak/orig/`.
- **Desarrollo del coop:** `work/scripts/mode.sh` del repo AI_Multiplayer llama solo a `mods-desarrollo.sh`: `test`
  desactiva los que interfieren y `play` los reactiva.

## Agregar un mod

1. Extraerlo en una subcarpeta nueva y leer su README.
2. Copiar un `toggle.sh` existente y cambiar `MOD_NOMBRE`, `MOD_ARCHIVOS` (`"origen en la carpeta|destino en el juego"`),
   `MOD_INTERFIERE` (1 si toca UI, IA, scripts, DLLs o lógica del juego) y `MOD_MOTIVO`.
3. `./toggle-mod.sh <carpeta> status` para verificar.
