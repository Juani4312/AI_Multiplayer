# 13 - "Usar" compartido (fase 5 v0): replicar las interacciones del jugador

- **Pregunta:** ¿se puede hacer que lo que usa un jugador (lockers, botones, cajas, terminales, ítems) pase también en la otra máquina?
- **Build y SHA-256 del binario:** `AI.exe` PE32, SHA-256 `dde66521798aae8c39ed4e7a3400f561fcecfff83619d1426b9e0cedf5819a90`.
- **Dirección(es) / RVA / offset:** VAs con base `0x400000`; RVA = VA − `0x400000`.

  | Qué | VA | RVA |
  |-----|----|-----|
  | Al apretar "usar": arma el payload y llama al productor | `0x007A3110` | `0x3A3110` |
  | Arma `objetivo[8] \| personaje[8] \| flag` | `0x00772840` | `0x372840` |
  | Productor `thiscall(this, payload*, 0)`, `ret 8` (prólogo `55 8B EC 56 57 8B F9`) | `0x00795D00` | `0x395D00` |
  | Handler A del uso (`cdecl`): A 0x26 → B | `0x00418A10` | `0x18A10` |
  | Handler de sesión del uso (`cdecl`): `ctx->vf[1](copia)` | `0x00418980` | `0x18980` |
  | **Ejecución real del uso** (`vf[1]` del `ctx` de sesión, vtable `0xEF4EE4`) | `0x00793FB0` | `0x393FB0` |
  | id del RPC A de uso (u16, asignado en runtime) | `0x0163652C` (dato) | `0x123652C` |
- **Evidencia:**
  - `work/evidence/h12/f9.txt` … `f11.txt` (decompilados y asm);
  - `work/evidence/runtime/run10-usar-host.log` (gitignored);
  - el log y el reporte de la laptop, cuando los suba (`work/windows/`).
- **Confianza:** alta para el camino, que se observó en runtime en las dos máquinas. Media para el significado del flag.
- **Verificado en runtime:** **sí** (corridas 10 a 12, 2026-11-05: papito Proton en host ↔ laptop Windows 10 en join, por Tailscale). Con el avatar como actor (corrida 12), los botones funcionan en los dos sentidos sin mover al otro jugador.

## El camino de un uso (runtime, corrida 10)

```
jugador aprieta "usar" sobre un locker
  FUN_007a3110 → FUN_00772840(objetivo = usable+0x58, personaje = pj+0x54, flag)
  FUN_00795d00(this = objeto con vtable 0xEF4EEC)     // vf[1] = 0x74D3F0 = ret 4 (vacío)
      └─ encola A 0x26 (20 bytes)
  (mismo frame) handler A FUN_00418a10(ctx = el mismo objeto)   // vf[2] = 0x3806A0 = mov al,1; ret 4
      └─ encola B (sesión)
  (mismo frame) handler de sesión FUN_00418980(ctx = objeto + 4, vtable 0xEF4EE4)
      └─ ctx->vf[1] = FUN_00793FB0                              // acá se ejecuta el uso
```

- Es el mismo patrón que el spawn (hallazgo 12, parte 2): lo que se ejecuta en el momento está vacío, y lo real lo hace el handler de sesión. **En retail los mensajes de sesión se despachan en local**, en el mismo frame.
- Los ids de entidad coinciden entre máquinas: el mismo locker es `3e3ec35d bebfb3d2` en las dos.
  - Por la corrida 3, el primer dword parece indicar el tipo de objeto: `3e3ec35d` locker, `eed179bb` caja, `044106b1` botón del transit, `f7fb2113` ítem, `1e38b898` terminal.
- El flag: el último dword difiere entre máquinas (`007c5801` en papito, `013a5801` en la laptop). `FUN_00772840` recibe un bool (`~(usable+0x96 >> 4) & 1`). Seguramente solo el primer byte tiene significado y el resto es basura del stack. Sin verificar.

## Implementación (`mod/src/sync.c`, `AIMP_SYNC_USE=1`)

- **Captura:** hook en `FUN_00795d00`. Si el personaje del payload es el jugador local, el payload sale como evento de red `AIMP_EVT_USE` (`net.c`):
  - paquete de 76 bytes, mandado 3 veces;
  - el receptor descarta repetidos con una ventana de 64 bits.
- **Aplicación:** el receptor, si está en el mismo nivel, pone su propio jugador como personaje y encola un A 0x26 con `FUN_00a2a290`, usando el tipo que leyó de `[0x163652C]`. No hay eco, porque no pasa por el productor.
- **Traza:** los hooks en los dos handlers loguean el camino (`SYNC handler …`).
- Tests: `test_sync` (stubs con los prólogos reales) y `test_net` (protocolo de eventos).

## Resultados (corrida 10)

| Dirección | Log | En pantalla (usuario) |
|-----------|-----|-----------------------|
| papito → laptop | `SYNC uso local 1 … objetivo 3e3ec35dbebfb3d2 (jugador local)` → `mandado al otro jugador (evento 1, nivel 16)` | "el player de la laptop se metió conmigo al locker instantáneamente" |
| laptop → papito | `SYNC: aplicando uso remoto (evento 1): objetivo 3e3ec35dbebfb3d2` → handler A → handler de sesión, en 7 ms; 1 recibido, 2 repetidos descartados | "la PC se teletransporta dentro también" |

Sin excepciones.

Detalle del lado de la laptop (`work/windows/logs/aimp-usar-20261105-132155.log`, `work/windows/report.md` paso 7). Es su segunda sesión: aplicó el evento 2 de papito, de nuevo el locker.
- `SYNC: aplicando uso remoto (evento 2)` → handler A → handler de sesión (`ctx->vf[1]` = `0x393FB0`) en 17 ms.
- **El jugador local salta al anclaje del locker:** en ~3 s pasa de (1,76, 13,28, 34,80) a (−3,370, 13,236, 34,810), la misma posición que el jugador de papito, igual en los 3 decimales (los dos están en el mismo anclaje).
- La cámara queda fija adentro.
- 1 recibido, 2 repetidos descartados, 0 excepciones, salida limpia; 60 fps, 0 perdidos.

## Corrida 11: filtro y botón (los dos lados)

- Lockers y terminal de guardado: filtrados en las dos máquinas (no se mandan). El otro jugador no se mueve.
- Botón del transit (`044106b1 eb7fc7c0`): se replica y **el transit se llama en los dos juegos**. El jugador que recibe salta ~5,5 m al anclaje del botón (yaw −90° exacto) y queda trabado 2 a 4 s por la animación.
- Lo que se cambia adentro de una terminal no se replica: no genera RPCs.
- **El filtro por tipo no alcanza:** todo uso con animación lleva al que lo ejecuta hasta el anclaje.
- **Siguiente:** que el uso remoto lo haga el avatar (`AIMP_SYNC_USE_ACTOR=1`, `cba2b2e`). El avatar ya está donde está el otro jugador. Sin probar: hay que ver si el engine acepta que un NPC use el botón.

## Corrida 12: el avatar hace el uso remoto (`AIMP_SYNC_USE_ACTOR=1`, `cba2b2e`) — **funciona**

- Evidencia: `work/evidence/runtime/run12-avatar-actor-host.log` (gitignored), `work/windows/logs/aimp-usar-20261105-133822.log` y `work/windows/report.md` (paso 7c).
- **papito → laptop:** botón `044106b1 eb7fc7c0`.
  - La laptop lo aplicó con `personaje = el avatar (1c3460b37e03a5dc)`, el NPC "CV5".
  - **El jugador de la laptop no se movió:** quieto en (3,158, 13,278, 39,554) de 68 a 77 s.
- **laptop → papito:** otro botón del mismo tipo, `044106b1 071fbb9e`.
  - papito lo aplicó con el avatar. Su jugador quedó quieto en (6,556, 13,276, 40,089) de 210 a 226 s.
  - El avatar quedó en el objetivo exacto: la corrección del desvío (`8bc1f7e`) funciona, con ~50 teleports por segundo mientras la IA lo empuja.
- **El usuario:** "hice F8 y transit button en ambos y ambos funcionaron". **El transit viene en las dos máquinas y nadie se teletransporta:** el engine acepta que un NPC use el botón.
- Sin excepciones en las dos máquinas, salida limpia, 0 perdidos.
- Además: **disparar el lanzallamas no se replica**, porque no pasa por el "usar". En la máquina donde se dispara, el avatar (un civil) ve el fuego y su IA quiere huir; el teleport lo mantiene en el lugar. Es otro motivo para apagarle la IA al avatar.

## Limitaciones y siguiente paso

- **El que recibe hace la misma acción con su propio jugador.** Para el estado del mundo (cajas abiertas, botones, puertas, ítems) es lo que se busca. Para acciones del jugador (meterse en un locker, abrir la terminal de guardado) no: en un coop, solo se esconde el que se mete.
  - Opción 1: filtrar por tipo de objeto, con el primer dword del id: replicar cajas, botones e ítems, y no lockers ni terminales. **Implementada:** `AIMP_SYNC_USE_SKIP`, por defecto `3e3ec35d,1e38b898` (lockers y terminal de guardado). El uso se ejecuta igual en la máquina propia; solo no se manda. `test_sync` lo cubre. Falta probarlo en el juego con una caja o un botón.
  - Opción 2: aplicar el uso con el avatar (el id del NPC) como personaje. Hay que probar si un NPC puede usar esas cosas.
- **Ítems:** si los dos "usan" el mismo ítem del piso, ¿lo levantan los dos? Hay que probarlo (corrida 3: molotov `f7fb2113 d783f856`).
- Las puertas automáticas no generan mensajes (hallazgo 11): se abren solas por proximidad del jugador local. El avatar NPC probablemente no las abre.
