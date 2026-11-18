#ifndef AIMP_DEATH_H
#define AIMP_DEATH_H
#include "aimp.h"
#include "net.h"

/* Muerte en coop (2026-11-17, en investigación).
 *   AIMP_TRACE_UI=1  hook de FUN_0081f8f0 (abre una pantalla de UI por nombre, thiscall(ui, nombre), ret 4): loguea el
 *                    nombre y los retornos a AI.exe que haya en la pila (para ubicar quién pide el game over). */
/*   AIMP_COOP_DOWNED=1  (experimental) el jugador local no muere: Character+0x4253 = 1 hace que la función de daño
 *                    (FUN_007caac0) recorte el golpe a salud-1. Cuando la salud llega a 1, queda "caído": invencible
 *                    (+0x2E8/+0x2EA |= 8, como CHR_SetInvincibility). AIMP_COOP_REVIVE_FILE (pruebas): si el archivo
 *                    cambia, revive (salud máxima, sin invencibilidad).
 *   Remates (corridas 134-137): con el modo caído se engancha matar (FUN_007d0c10); si matan al jugador local en una
 *                    acción que no es la locomoción (el agarre del Alien, vtable rva 0xEAE690), a los
 *                    AIMP_COOP_DEATH_ACT_MS (4000; -1 = nunca) se lo devuelve a la locomoción (FUN_00658080) y se borra el
 *                    fundido de pantalla (singleton 0x019D0970, +0x1B48): si no, la pantalla queda negra para siempre.
 *                    El atacante queda retenido en su mitad del remate mientras el jugador esté caído (suelto, lo vuelve
 *                    a rematar cada ~4,5 s) y se lo suelta al revivir (FUN_0065bdd0, acción por defecto).
 *                    AIMP_COOP_ALIEN_WITHDRAW=1 (experimental): además, el Alien se retira al soltarlo (NPC_WithdrawAlien).
 *   AIMP_TRACE_DMG=1 golpes sobre el jugador (FUN_007caac0: daño, atacante, flags, pila) y llamadas a matar (líneas DMG). */
void death_install(AimpNet *net);
/* Por red (AIMP_COOP_DOWNED=1 con AIMP_NET): AIMP_EVT_COOP avisa caído / revivido; si caen los dos, game over normal
 * (el último en caer muere de verdad); el que usa una terminal de guardado con el compañero caído lo revive a su lado. */
int death_coop_enabled(void);
/* 1 si el otro jugador avisó que está caído. */
int death_remote_is_downed(void);
/* Host: 1 si está caído o lo revivió el compañero hace menos de 2 min (savesync acepta entonces el save del cliente). */
int death_push_ok(void);
void death_remote_event(const AimpEvt *e);
/* La entidad Checkpoint de los scripts terminó de guardar (pins.c, cualquier thread): en el próximo frame, si el
 * compañero está caído, se lo revive como con la terminal de guardado. */
void death_note_checkpoint(void);
/* El jugador local usó una terminal de guardado (sync.c). */
void death_on_save_terminal(void);

#define COOP_DOWNED  1
#define COOP_REVIVED 2
#define COOP_REVIVE  3 /* pedido de revivir: pos/yaw = dónde está el que guardó */
#pragma pack(push, 1)
typedef struct {
    uint32_t kind;
    float pos[3];
    float yaw;
} CoopEvt;
#pragma pack(pop)
/* Por frame, en el thread principal (player = Character del jugador local o 0). */
void death_frame(int64_t now, int64_t freq, uint32_t player);
/* Devuelve al jugador a la locomoción (sale de una acción como la de muerte; FUN_00658080). 1 si se llamó. */
int death_player_to_loco(uint32_t player, const char *why);
/* Vuelve a la acción por defecto del personaje (FUN_0065bdd0). 1 si se llamó. */
int death_char_default_action(uint32_t chr, const char *why);
/* Manda al Alien (personaje de tipo 1) a retirarse, como NPC_WithdrawAlien. 1 si se pudo. */
int death_alien_withdraw(uint32_t alien, const char *why);
/* 1 si el jugador local está caído (para mandarlo por red más adelante). */
int death_is_downed(void);

#endif
