#ifndef AIMP_THROW_H
#define AIMP_THROW_H
#include "aimp.h"
#include "net.h"

/* Objetos arrojados y colocados por el jugador (hallazgo 17). Ver throw.c.
 *   AIMP_TRACE_THROW=1    loguea cada lanzamiento y cada colocación (solo mira).
 *   AIMP_SYNC_THROW=1     además los manda al otro (AIMP_EVT_THROW) y aplica los del otro.
 *   AIMP_THROW_ECHO=<ms>  prueba local sin red: repite cada lanzamiento propio <ms> después,
 *                         corrido AIMP_THROW_ECHO_CM cm (75 por defecto) al costado. */
int throw_install(AimpNet *net);
/* ¿Hay que despachar eventos AIMP_EVT_THROW? (sync_frame) */
int throw_sync_enabled(void);
/* Evento recibido (sync_frame, thread principal). */
void throw_remote_event(const AimpEvt *e, uint32_t level);
/* AIMP_EVT_THROW_REST recibido: si la réplica cayó lejos, se borra y se crea quieta donde cayó la del otro. */
void throw_rest_event(const AimpEvt *e, uint32_t level);
/* AIMP_EVT_PICKUP recibido: el otro recogió ese ítem; acá se lo saca del mundo (sin dárselo a nadie). */
void throw_pickup_event(const AimpEvt *e, uint32_t level);
/* Por frame (thread principal): ecos pendientes de AIMP_THROW_ECHO. */
void throw_frame(int64_t now, int64_t freq, uint32_t level);

/* Payload de AIMP_EVT_THROW (112 bytes). */
#define THROW_OP_THROW 1 /* FUN_007b5050: tirar ("spawn_explosive") */
#define THROW_OP_PLACE 2 /* FUN_007b5190: colocar ("place_item") */
#define THROW_NAME_MAX 40
#pragma pack(push, 1)
typedef struct {
    uint8_t op;
    uint8_t pad[3];
    float m[12];   /* matriz 3x4 por filas; la traslación está en m[3], m[7], m[11] (start_position) */
    float vel[3];  /* solo THROW_OP_THROW */
    uint32_t a3;   /* THROW_OP_THROW: 3er argumento (float 15.0 en el binario), bits tal cual */
    uint32_t t;    /* time_left (def+0x70), bits tal cual */
    char name[THROW_NAME_MAX]; /* plantilla ("Thrown\Flare"), terminada en 0 */
} ThrowEvt;
/* Payload de AIMP_EVT_THROW_REST (2026-11-15): el objeto que tiró el emisor se quedó quieto ahí.
 * id = ThrowEvt.pad[0] del lanzamiento (contador del emisor, 1..255). */
/* Payload de AIMP_EVT_PICKUP (2026-11-16): pickup recogido por el emisor. pid = ruta de composites (pins.c,
 * determinista entre máquinas); ids = los 8 bytes de FUN_008c3360 en el emisor (pueden ser dinámicos). */
typedef struct {
    uint32_t pid, eid, type;
    uint32_t ids[2];
    uint32_t depth;
} PickupEvt;
typedef struct {
    uint8_t id;
    uint8_t pad[3];
    float pos[3];
    uint32_t ms; /* desde el lanzamiento */
} ThrowRest;
#pragma pack(pop)

#endif
