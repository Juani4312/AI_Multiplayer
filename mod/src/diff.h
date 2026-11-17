#ifndef AIMP_DIFF_H
#define AIMP_DIFF_H
#include "aimp.h"

/* Dificultad del coop (work/DESIGN_DIFFICULTY.md). Paso 1: ver cuándo carga el juego su dificultad y con qué valores.
 * FUN_006f8b00 (método virtual, vtable 0x12F1300; ret 0xC) parsea DIFFICULTYSETTINGS/<dif>.BML en la estructura que
 * recibe como tercer argumento:
 *   +0x00..+0x64  bloque del Alien (modificadores de barrido, radios de acecho, medidor de amenaza, conductos);
 *   +0x68..+0x70  vector de punteros a un bloque de 0x40 por plantilla de NPC (índice por nombre con FUN_006d8d90 sobre
 *                 [g_root+0x98]): sentidos +0x00..+0x20, daño hecho +0x24, daño recibido +0x28, ritmo de ataque
 *                 +0x2C/+0x30, cobertura +0x34, tiempo entre disparos +0x38, ítem sospechoso +0x3C.
 * AIMP_TRACE_DIFF=1: loguea cada carga. */
void diff_install(void);

#endif
