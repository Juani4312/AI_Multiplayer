#ifndef AIMP_WEAPONS_H
#define AIMP_WEAPONS_H
#include "aimp.h"
#include "net.h"

/* Armas de a una (AIMP_SYNC_WEAPONS=1; ver weapons.c). */
void weapons_install(AimpNet *net);
void weapons_frame(int64_t now, int64_t freq, uint32_t player, uint32_t level);
int weapons_enabled(void);
void weapons_remote_event(const AimpEvt *e);

#define WEAPON_EQUIP 1
#define WEAPON_UNEQUIP 2
#pragma pack(push, 1)
typedef struct {
    uint32_t kind;
    uint32_t guid; /* ShortGuid del ítem del arma */
} WeaponEvt;
#pragma pack(pop)

#endif
