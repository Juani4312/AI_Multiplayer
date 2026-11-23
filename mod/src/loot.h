#ifndef AIMP_LOOT_H
#define AIMP_LOOT_H
#include "aimp.h"

/* Botín de las cajas (2026-11-23, pedido del usuario: "multiplicador de botín").
 * El contenido de una caja sale del pool de ítems del nivel ([g_root+0xA0]) al abrirla por primera vez:
 *   - AllocateGCItemsFromPool (las cajas; vtable 0x1230448, manejador slot 0x284 = FUN_00576070, corrida 181):
 *     FUN_007a4a70(lista de ítems, bias, máx) recorre las ranuras y, en cada una vacía, sortea con FUN_00794960 (o
 *     directo, con cantidad fija) -> FUN_00786aa0 thiscall(pool, salida*, n), ret 8;
 *   - AllocateGCItemFromPoolBySubset (un ítem; manejador FUN_005761b0): FUN_007a4de0 thiscall(pool, ítems, float bias,
 *     char forzar, salida*), ret 0x10.
 * Las dos devuelven en AL si hubo ítem y usan la misma salida: +0 u32 ítem, +6 u16 cantidad (LCG propio del pool en
 * pool+0x5C18). El mod engancha las dos y escala la cantidad (redondeo al azar: en promedio da el porcentaje); si queda
 * en 0, ese sorteo falla (la ranura, y las que siguen en esa caja, quedan vacías). AIMP_TRACE_LOOT=1 loguea cada sorteo. */
void loot_install(void);
/* porcentaje de la cantidad (100 = el juego); menu.c le pasa el efectivo (el del host en el cliente conectado) */
void loot_set_pct(int pct);
/* para tests: la cantidad escalada con un azar dado (0..65535) */
int loot_scale(int qty, int pct, unsigned rnd16);

#endif
