# 03 - Entidad del jugador: posición, rotación y estado

- **Pregunta:** ¿Cómo se obtiene en runtime el personaje del jugador local, y dónde están su posición y su orientación?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | `GetPlayer()` (devuelve `Character*` o NULL; 180 call sites) | `0x0078BAF0` | `0x38BAF0` |
  | `g_root` (puntero) | `0x016F0C88` | `0x12F0C88` |
  | Array de jugadores: `[[g_root+0x08]+0x80]` (punteros), cantidad `[[g_root+0x08]+0x84]` | — | — |
  | vtable **final** de los personajes (clase derivada, sin RTTI) | `0x012F5D80` | `0xEF5D80` |
  | vtable de la clase base `Character` (la pisa el constructor derivado) | `0x012F4FC8` | `0xEF4FC8` |
  | Constructor derivado (llama al base y pone la vtable final) | `0x007CEC20` | `0x3CEC20` |
  | Constructor `Character` (`thiscall(this, a, b*, def*)`) | `0x007B2390` | `0x3B2390` |
  | Destructor (referencia la vtable) | `0x007B83B0` | `0x3B83B0` |
  | `Character::GetAimPosition(out, flag)` (thiscall; pos + (0, 1, 0) y casos especiales) | `0x007B8590` | `0x3B8590` |
  | `Character::SetTransform(pos*, yaw, ?, bool)` (thiscall; teleport) | `0x007C2710` | `0x3C2710` |
- **Evidencia:** `./work/evidence/h03/d1.txt` … `d31.txt` (salidas de `Dump.java`). Las claves son `d15.txt` (GetPlayer), `d7.txt` (GetAimPosition), `d26.txt` (SetTransform y constructor), `d31.txt` (lecturas de `+0x70/74/78/174` justo después de `GetPlayer`) y la lectura de constantes de `.rdata`.
- **Confianza:** alta para `GetPlayer`, la posición `+0x70/+0x74/+0x78` y el yaw `+0x174`. Media para los nombres de métodos y el significado de los demás campos.
- **Cómo verificar en runtime:** fase 2 del mod. Por frame loguear `GetPlayer()`, `+0x70/74/78` y `+0x174`. Caminar tiene que cambiar la posición (Y casi constante en el piso); girar tiene que cambiar `+0x174` en [−π, π].
- **Verificado en runtime:** **sí** (Corridas del 2026-11-05 con el mod de la fase 1+2 (Proton 9.0-203); logs en `work/evidence/runtime/run1-menu.log` y `run2-juego.log`, resumen en el hallazgo 11). `[[g_root+8]+0x80][0]` es el jugador (vtable final `0x12F5D80`, nombre "Character: 6"). La posición `+0x70/74/78` cambia al caminar (2,4 m/s) y Y es vertical (una escalera la sube de 14,5 a 18,6). El yaw `+0x174` cubre [−3,138, 2,969] al girar 360°. `+0x54/+0x58` = id de 8 bytes del personaje (p. ej. `b222f72f 72519ab8`), el mismo que va en el payload del RPC de interacción (hallazgo 11).
- **Preguntas abiertas:** al final.

## Cómo llegar al jugador

```c
// FUN_0078baf0 (decompilado)
Character* GetPlayer(void) {
  if (*(int*)(*(int*)(g_root + 0x08) + 0x84) == 0) return NULL;   // cantidad
  return **(Character***)(*(int*)(g_root + 0x08) + 0x80);          // array[0]
}
```

- Se llama `FUN_0078baf0(0)` (por ejemplo desde `PlayerTorch`, `FUN_004cd050`), pero el cuerpo **no usa el argumento**: siempre devuelve el elemento 0. Hipótesis: la firma original es `GetPlayer(int index)` y en la build de un solo jugador se simplificó. Igual, **el engine guarda un array de jugadores con contador**, lo que encaja con los nodos de script de multiplayer del hallazgo 01.
- No se encontró estáticamente quién llena el array (`+0x80`/`+0x84`) ni quién asigna `g_root+0x08`. Queda para el hallazgo 05 (creación de entidades).
- Con 180 call sites, es el acceso canónico. El mod debería hacer lo mismo: leer `[[[base+0x12F0C88]+8]+0x80]` con chequeos de NULL en cada paso, o llamar a `base+0x38BAF0`, que es una función cdecl sin efectos colaterales.

## Layout de `Character` (parcial)

| Offset | Tipo | Significado | Evidencia |
|--------|------|-------------|-----------|
| `+0x00` | ptr | vtable: en runtime `0x012F5D80`, la de la clase derivada. El constructor base pone `0x012F4FC8` y el derivado `FUN_007cec20` la reemplaza (corrección del 2026-11-05, ver hallazgo 05) | `FUN_007b2390`, `FUN_007cec20` |
| `+0x10` | char[64] | nombre de la instancia (`"Character: %d"` si viene vacío) | `RequestCharacter` `FUN_007d0470` |
| `+0x1120` | short | índice en la lista de personajes del gestor | `RequestCharacter` |
| `+0x2CB0`/`+0x2CB4` | ptr/int | interfaz de refcount / contador de referencias | `FUN_007cec20`, `RequestCharacter` |
| `+0x50` | dword | arg 2 del constructor | constructor |
| `+0x54`, `+0x58` | dword×2 | copiados de `*arg3` (¿id/handle de entidad?) | constructor |
| `+0x64` | ptr | definición/info del personaje (arg 4). `+0x04` = tipo (0, 1 = jugador según varios `if`, otro); `+0x1C` y `+0x1F` = flags que `PlayerTorch` exige para "es un jugador" | `FUN_004cd050`, `FUN_007caef0`, `FUN_007c2710` |
| **`+0x70`, `+0x74`, `+0x78`** | float×3 | **posición en el mundo (x, y, z), Y vertical** | se lee como vector después de `GetPlayer` (`0x80BCAC`); `GetAimPosition` devuelve `(x, y+1.0, z)`; un cast de física desde `y+0.6` (`FUN_007b8b90`, `character_playable.cpp`) |
| **`+0x174`** | float | **yaw en radianes, normalizado a [−π, π]** | `SetTransform` hace `yaw - round(yaw·(1/2π))·2π` con `DAT_011bb278 = 0.1591549` y `DAT_011bb274 = 6.2831855`, y lo guarda en `this[0x5D]` = `+0x174`; se lee después de `GetPlayer` (`0x80A52F`); se pasa a funciones tipo sin/cos (`FUN_007caef0`) |
| `+0x184`, `+0x1A0` | dword | se ponen en 0 al teletransportar | `SetTransform` |
| `+0x275`, `+0x2AF`, `+0x2B0`, `+0x2D9`, `+0x2FA` | byte | flags de estado (sin identificar) | varios |
| `+0x31C` | ptr | estado especial: si `[+0x31C]+0x48 != 0`, la posición "de puntería" sale de `[[g_root+0x58]+0x1DC]+0x3C` (¿escondite/locker o vehículo?) | `GetAimPosition` |
| `+0x36C` (`this[0xDB]`) | ptr | sub-objeto con flags en `+0x18` | `SetTransform` |
| `+0x6F0` | int | contador (`PlayerTorch` exige > 0; ¿baterías?) | `FUN_004cd050` |
| `+0x1358` | struct | sub-objeto muy usado como `this` (¿controlador de movimiento?) | `pre:` de `GetAimPosition` |

Otros objetos guardan un `Character*` en `+0x1640` (controladores en `0x7B9xxx`–`0x7D1xxx`, zona de `character_playable.cpp`/`look_at_manager.cpp`). Hipótesis: es el controlador del jugador o de la IA.

## Cómo leer el estado del jugador desde el mod (fase 2)

```c
uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
uintptr_t root = *(uintptr_t*)(base + 0x12F0C88);            // g_root
if (root) {
  uintptr_t pm  = *(uintptr_t*)(root + 0x08);
  if (pm && *(int*)(pm + 0x84) > 0) {
    uintptr_t chr = **(uintptr_t**)(pm + 0x80);
    if (chr && *(uintptr_t*)chr == base + 0xEF5D80) {       // vtable final esperada (clase derivada)
      float x = *(float*)(chr + 0x70), y = *(float*)(chr + 0x74), z = *(float*)(chr + 0x78);
      float yaw = *(float*)(chr + 0x174);
    }
  }
}
```

Hay que hacerlo en el thread principal (dentro del hook de `ModeManager::Run`, hallazgo 02) para evitar carreras.

## Preguntas abiertas

- ¿Quién llena `[[g_root+0x08]+0x80]`? ¿Qué pasa si se agrega un segundo `Character*` y se sube el contador? (hallazgo 05; potencialmente clave para el coop)
- ¿El valor de `+0x64 → +0x04` distingue jugador/NPC/Alien? (runtime)
- ¿Pitch de la cámara/cabeza? Probablemente esté en la cámara (hallazgo 04).
- ¿Velocidad/estado de locomoción (agachado, corriendo, escondido)? Para animar al "ghost" alcanza con posición + yaw al principio.
