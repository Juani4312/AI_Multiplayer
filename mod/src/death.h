#ifndef AIMP_DEATH_H
#define AIMP_DEATH_H
#include "aimp.h"

/* Muerte en coop (2026-11-17, en investigación).
 *   AIMP_TRACE_UI=1  hook de FUN_0081f8f0 (abre una pantalla de UI por nombre, thiscall(ui, nombre), ret 4): loguea el
 *                    nombre y los retornos a AI.exe que haya en la pila (para ubicar quién pide el game over). */
/*   AIMP_COOP_DOWNED=1  (experimental) el jugador local no muere: Character+0x4253 = 1 hace que la función de daño
 *                    (FUN_007caac0) recorte el golpe a salud-1. Cuando la salud llega a 1, queda "caído": invencible
 *                    (+0x2E8/+0x2EA |= 8, como CHR_SetInvincibility). AIMP_COOP_REVIVE_FILE (pruebas): si el archivo
 *                    cambia, revive (salud máxima, sin invencibilidad). */
void death_install(void);
/* Por frame, en el thread principal (player = Character del jugador local o 0). */
void death_frame(int64_t now, int64_t freq, uint32_t player);
/* 1 si el jugador local está caído (para mandarlo por red más adelante). */
int death_is_downed(void);

#endif
