#ifndef AIMP_HOOK_H
#define AIMP_HOOK_H
#include "aimp.h"

typedef struct {
    const char *name;
    uint8_t *target;
    uint8_t *tramp;
    uint8_t orig[16];
    uint32_t steal;
    int installed;
} AimpHook;

/* Instala un detour (JMP rel32) al principio de `target`.
 * - expect/expect_len: bytes exactos que tiene que haber en target. Si no
 *   coinciden, no se toca nada y devuelve 0.
 * - steal (5..16): bytes que se pisan. Se copian a un trampolín, así que tienen
 *   que ser instrucciones completas y sin direccionamiento relativo al EIP. Lo
 *   garantiza el llamador con expect (los bytes se verificaron en el binario).
 * - *tramp_out: dirección para llamar a la función original. */
int hook_install(AimpHook *h, const char *name, void *target, const uint8_t *expect,
                 uint32_t expect_len, uint32_t steal, void *detour, void **tramp_out);
/* Restaura los bytes originales. El trampolín no se libera (podría haber un thread
 * adentro). */
int hook_remove(AimpHook *h);

#endif
