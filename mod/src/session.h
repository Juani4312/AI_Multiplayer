#ifndef AIMP_SESSION_H
#define AIMP_SESSION_H
#include "aimp.h"
#include "net.h"

/* ¿Las dos máquinas cargaron la misma partida? (2026-11-17, work/DESIGN_SESSION.md punto 1)
 * Huella = nivel + posición y yaw del jugador cuando aparece después de cargar (con el mismo save coinciden al cm).
 * Se manda (AIMP_EVT_SESSION) al conectarse el otro y después de cada carga; si no coincide, el log avisa
 * "PARTIDAS DISTINTAS". Siempre activo con AIMP_NET. */
void session_install(AimpNet *net);
void session_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level);
void session_remote_event(const AimpEvt *e);
/* -1 sin datos, 0 distintas, 1 iguales */
int session_match(void);

#pragma pack(push, 1)
typedef struct {
    uint32_t level;
    float pos[3];
    float yaw;
} SessionEvt;
#pragma pack(pop)

#endif
