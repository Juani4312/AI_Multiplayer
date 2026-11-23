#ifndef AIMP_DIFF_H
#define AIMP_DIFF_H
#include "aimp.h"
#include "net.h"

/* Dificultad del coop (work/DESIGN_DIFFICULTY.md).
 * FUN_006f8b00 (método virtual, vtable 0x12F1300; ret 0xC) parsea DIFFICULTYSETTINGS/<dif>.BML en la estructura que
 * recibe como tercer argumento; el juego carga las 5 al arrancar (Easy, Medium, Hard, Nightmare, Novice) y usa la del save:
 *   +0x00..+0x64  bloque del Alien: modificadores ADITIVOS sobre ALIENCONFIGS (radios de acecho, medidor de amenaza,
 *                 conductos, barrido de backstage). Hard = 0; Nightmare = un escalón más duro.
 *   +0x68..+0x70  vector de bloques de 0x40 por plantilla de NPC (sentidos +0x00..+0x20, daño +0x24/+0x28, …).
 * La dificultad del coop (menú "Coop", ui.c) suma N escalones más sobre las 5 estructuras (N = 0 Normal, 1 Difícil,
 * 2 Pesadilla; 3 con la escala del Alien +3 de Personalizada, menu.c), partiendo siempre de una copia de los valores base: no se acumula y la dificultad del juego sigue
 * mandando como base. El host manda la suya al cliente (AIMP_EVT_OPTIONS).
 * AIMP_TRACE_DIFF=1: loguea cada carga y cada aplicación. */
void diff_install(AimpNet *net);
void diff_frame(int64_t now, int64_t freq);
/* elección local (menú) */
void diff_set_local(int level);
/* la que se aplica (la del host si este es el cliente conectado) */
int diff_effective(void);
/* 1 si la decide el host (cliente conectado) */
int diff_from_host(void);
void diff_remote_event(const AimpEvt *e);

/* Para tests: aplicar a una estructura falsa (bloque del Alien de 26 floats). */
void diff_apply_block(float *dst, const float *base, int level);

#pragma pack(push, 1)
typedef struct {
    uint32_t kind; /* 1 = dificultad */
    uint32_t level;
} OptionsEvt;
#pragma pack(pop)

#endif
