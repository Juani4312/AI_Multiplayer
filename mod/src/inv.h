#ifndef AIMP_INV_H
#define AIMP_INV_H
#include "aimp.h"
#include "net.h"

/* Inventario compartido (decisión del usuario, 2026-11-17: "compartimos todo"; work/DESIGN_SESSION.md).
 *   AIMP_TRACE_INV=1  loguea cada cambio del inventario local (ítem, delta, cantidad).
 *   AIMP_SYNC_INV=1   además manda los cambios (AIMP_EVT_INV) y aplica los del otro (con AIMP_NET).
 * El inventario es [g_root+0x44]: 34 slots de 0x14 bytes desde +0x68 (+0 definición, +8 cantidad). La identidad del
 * ítem entre máquinas es el ShortGuid de su nombre (definición +0x14). Hallazgo 21. */
void inv_install(AimpNet *net);
/* Por frame (thread principal): detecta cambios locales. player = Character del jugador 0 o 0. */
void inv_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level);
int inv_sync_enabled(void);
/* AIMP_EVT_INV recibido (lo despacha sync_frame). */
void inv_remote_event(const AimpEvt *e);

/* Payload de AIMP_EVT_INV: n pares (ShortGuid del ítem, delta). */
#define INV_EVT_MAX 13
#pragma pack(push, 1)
typedef struct {
    uint32_t n;
    struct {
        uint32_t guid;
        int32_t delta;
    } d[INV_EVT_MAX];
} InvEvt; /* 108 bytes */
#pragma pack(pop)

#endif
