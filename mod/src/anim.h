#ifndef AIMP_ANIM_H
#define AIMP_ANIM_H
#include "aimp.h"

/* Comando PLAY_ANIMATION (tipo 7) para un personaje (hallazgo 20, 2026-11-17).
 *
 *   FUN_0067e3b0 thiscall(controlador de comandos, a[8]*, b[8]*, start_instantly, params[0x58]*), ret 0x10.
 *   Lo llama la entidad de script CMD_PlayAnimation (FUN_00530e60), que es también lo que usan las tareas de la
 *   IA (composites AYZ\ANIMATION\TASKS\...). Los parámetros los arma FUN_0052fa10; AnimationSet y Animation van
 *   como hash FNV-1a 32 del texto (+0x24 y +0x28).
 *
 * AIMP_TRACE_ANIM=1: hook de FUN_0067e3b0 que loguea cada PLAY_ANIMATION (personaje, set, animación, flags), con
 *   los nombres de la tabla de textos del juego (FUN_00a35b90) si están. Hasta AIMP_TRACE_ANIM_MAX (80).
 */
int anim_install(void);
/* ¿Se puede usar anim_play? (prólogo verificado) */
int anim_ready(void);
/* FNV-1a 32 como el engine (FUN_0052fa10: char con signo, 0 para el texto vacío). */
uint32_t anim_hash(const char *s);
/* Nombre de un hash en la tabla de textos del juego, o NULL. */
const char *anim_name(uint32_t hash);

typedef struct {
    uint32_t set, anim;   /* hashes */
    int32_t play_count;   /* 1 por defecto; muchas para sostener una pose */
    float speed;          /* 1,0 */
    float blend_in;       /* 0,3 */
    int gravity, collision, interruption, remove_motion, crouched_anim, no_ik, no_foot_ik, start_instantly;
} AnimReq;
void anim_req_default(AnimReq *r);
/* Da el comando al controlador (Character+0x3570 de scripts o +0x3820 de tareas) con los ids a y b (8 bytes). */
void anim_play(uint32_t ctrl, const uint8_t *a, const uint8_t *b, const AnimReq *r);
/* Tick del controlador de comandos (FUN_00679150 thiscall(ctrl, char), ret 4): si hay un comando pendiente y su
 * vt+0x48 lo deja arrancar (start_instantly), lo pasa a ejecución (FUN_00679000) y actualiza el que corre. Lo llama
 * ActionScript desde el árbol de comportamiento; con el avatar títere el árbol deja de correr (corrida 86). */
int anim_ctrl_tick(uint32_t ctrl);
/* ¿El comando cmd es un PLAY_ANIMATION con esos ids (en cualquier orden)? */
int anim_cmd_is(uint32_t cmd, const uint8_t *a, const uint8_t *b);

#endif
