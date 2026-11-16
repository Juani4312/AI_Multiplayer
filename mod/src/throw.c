/* Objetos arrojados y colocados por el jugador (hallazgo 17).
 *
 * Tirar (bengala, noisemaker, molotov, pipe bomb, humo, flashbang...): el evento de animación
 * "spawn_projectile" (FUN_00666f90 -> FUN_0064c180 -> FUN_007d4830) llama a FUN_007b9500, que:
 *   1. arma 64 bytes con FUN_0079c910: matriz 3x4 de la mano (traslación en m[3], m[7], m[11]) y
 *      velocidad = normalizar(punto de impacto [equipo+0x90] - mano) * [0x163E964] (20), más 9.81;
 *   2. llama a FUN_007b5050(pool = [0x16F0DE4], &m, &m[12] (velocidad), 15.0, def+0x58, def+0x70).
 * FUN_007b5050 toma una entrada libre del pool y llama a FUN_007ade20, que instancia el composite
 * cuyo nombre termina con la plantilla (def+0x58 = "Thrown\%s", p. ej. REQUIRED_ASSETS\THROWN\FLARE;
 * búsqueda por sufijo sin mayúsculas en FUN_00920750), le pone start_position_matrix,
 * start_position, velocity, angular_velocity = 0, motion_duration = 10 y time_left, y le dispara
 * "spawn_explosive". El vuelo lo simula ProjectileMotion (FUN_0060b740) con esos parámetros y la
 * gravedad: no lee el estado del jugador.
 * Colocar (minas: evento "place_ied" -> FUN_007d4a00; soltar el ítem de la mano: FUN_007c7ef0):
 * FUN_007b5190(pool, &m, plantilla, time_left), que hace lo mismo con velocidad 0 y "place_item".
 *
 * Con AIMP_SYNC_THROW=1 y red, cada llamada del jugador local sale como AIMP_EVT_THROW y el otro
 * llama a la misma función (por el trampolín, así no vuelve a mandarse) con los mismos datos.
 * La plantilla es un char* que solo se usa para buscar el composite; igual se guarda en una tabla
 * estática, por si algo la retiene.
 * Pendiente: el lanzamiento local también avisa a la IA (FUN_0070f310 en FUN_007d4830); el
 * remoto no. Lo que haga el composite (ruido, luz, fuego) corre igual en las dos máquinas.
 */
#include "throw.h"
#include "game.h"
#include "hook.h"
#include "log.h"
#include "pins.h"

#define LOG_MAX 64
#define NAMES_MAX 16
#define ECHO_MAX 8

typedef void(__attribute__((fastcall)) * SpawnFn)(void *pool, void *edx, const float *m, const float *vel, uint32_t a3,
                                                  const char *name, uint32_t t);
typedef void(__attribute__((fastcall)) * PlaceFn)(void *pool, void *edx, const float *m, const char *name, uint32_t t);

static AimpHook g_hk_spawn, g_hk_place;
static SpawnFn g_spawn_orig;
static PlaceFn g_place_orig;
static AimpNet *g_net;
static int g_trace, g_sync, g_echo_ms, g_echo_cm = 75;
static volatile uint32_t g_level;
static volatile LONG g_n_local, g_n_remote;
/* AIMP_THROW_PROBE y el último lanzamiento (local o remoto) */
static int g_probe, g_fix_test;
static float g_last_start[3];
static int g_have_start;

/* Los detours corren en el thread del evento de animación (no se sabe si es el principal): solo
 * encolan, con una sección crítica. throw_frame (thread principal) loguea, manda y hace los ecos. */
typedef struct {
    ThrowEvt t;
    uint32_t level;
    uint32_t from; /* rva del caller */
    DWORD due_ms;  /* ecos */
} Item;
static CRITICAL_SECTION g_cs;
static int g_cs_ok;
static Item g_local[ECHO_MAX], g_echo[ECHO_MAX];
static int g_nlocal, g_necho;
static volatile LONG g_n_lost;

static char g_names[NAMES_MAX][THROW_NAME_MAX];

/* ---- Dónde cae (AIMP_THROW_FIX, 1 por defecto con AIMP_SYNC_THROW; pedido del usuario 2026-11-15) ----
 * La parábola sale igual en las dos máquinas (ProjectileMotion es analítico en t), pero el choque y los rebotes
 * (Havok) no. El que tira sigue su objeto (game.h: TemporaryEntity+0x80 -> +0xC -> +0x24) y, cuando queda quieto
 * 0,5 s, manda AIMP_EVT_THROW_REST. El otro, si su réplica cayó a más de AIMP_THROW_FIX_CM (15) de ahí, la libera
 * como lo hace el juego (FUN_007a10f0) y tira una quieta en ese punto: se ve el vuelo de referencia y después
 * queda donde cayó la del otro. */
typedef void(__attribute__((fastcall)) * ReleaseFn)(void *pool, void *edx, uint32_t lo, uint32_t hi);
typedef void *(__attribute__((fastcall)) * IdsFn)(void *obj, void *edx, uint32_t *out);
static ReleaseFn g_release;
static IdsFn g_ids;
static int g_fix, g_fix_cm = 15;
static uint8_t g_next_id;
#define TRK_MAX 16
typedef struct {
    int on;
    uint8_t id;
    uint32_t idx, obj, ent, off;
    DWORD t0, still;
    float last[3];
    ThrowEvt ev;
    /* ProjectileMotion de este lanzamiento (2026-11-16): estado, última posición y cuándo se actualizó */
    uint32_t pm;
    float pm_pos[3], pm_t, pm_dur;
    int pm_done;
    uint32_t rb, rboff; /* hkpRigidBody de la bengala y offset de su posición (2026-11-16) */
    int rb_tries;
} Track;
static Track g_trk_local[TRK_MAX], g_trk_rep[TRK_MAX];

/* ProjectileMotion (2026-11-16, estático): FUN_0060b6d0 thiscall(estado, entidad, x), ret 8, es su actualización por
 * frame. Calcula la posición analítica (FUN_0060b470 sobre estado-0x14) y la deja en la matriz de estado+0x40
 * (traslación en +0x4C, +0x5C, +0x6C); salida en +0x0C, velocidad en +0x18, duración en +0x24, tiempo en +0x28,
 * terminado en +0x2C (FUN_0060b740 los inicializa). Prueba conjunta 8c: el composite no se encontró en 0 de 5
 * lanzamientos reales (20 m/s contra paredes); si la actualización se corta antes de la duración, el punto es el
 * del choque. */
#define AIMP_RVA_PM_UPDATE 0x0020B6D0u
#define PM_START 0x0C
#define PM_VEL   0x18
#define PM_DUR   0x24
#define PM_T     0x28
#define PM_FIN   0x2C
#define PM_MX    0x4C
#define PM_MY    0x5C
#define PM_MZ    0x6C
#define PM_MAX   32
typedef uint32_t(__attribute__((fastcall)) * PmUpdFn)(void *st, void *edx, void *ent, uint32_t x);
typedef uint32_t(__attribute__((fastcall)) * PmUpd1Fn)(void *st, void *edx, void *ent);
static const uint8_t AIMP_PM_UPD_PROLOGUE[7] = { 0x53, 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x10 };
/* FUN_0060b980 thiscall(estado, entidad), ret 4: el otro que llama a FUN_0060b470 (slot +0x1C del vtable
 * 0x01299F80, el de FUN_0060b740). Corrida 70: FUN_0060b6d0 no se llamó para las bengalas. */
#define AIMP_RVA_PM_UPDATE2 0x0020B980u
static const uint8_t AIMP_PM_UPD2_PROLOGUE[7] = { 0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57 };
static AimpHook g_hk_pm, g_hk_pm2;
static PmUpdFn g_pm_orig;
static PmUpd1Fn g_pm2_orig;
static volatile LONG g_pm_calls;
typedef struct {
    volatile uint32_t st;
    volatile DWORD first, last;
    int logged;
} PmSlot;
static PmSlot g_pm[PM_MAX];
static int g_pm_on;

static void pm_seen(uint32_t s);
static uint32_t __attribute__((fastcall)) det_pm(void *st, void *edx, void *ent, uint32_t x)
{
    uint32_t r = g_pm_orig(st, edx, ent, x);
    pm_seen((uint32_t)st);
    return r;
}
static uint32_t __attribute__((fastcall)) det_pm2(void *st, void *edx, void *ent)
{
    uint32_t r = g_pm2_orig(st, edx, ent);
    pm_seen((uint32_t)st);
    return r;
}
static void pm_seen(uint32_t s)
{
    InterlockedIncrement(&g_pm_calls);
    DWORD now = GetTickCount();
    int i, fr = -1;
    for (i = 0; i < PM_MAX; i++) {
        if (g_pm[i].st == s)
            break;
        if (fr < 0 && (!g_pm[i].st || now - g_pm[i].last > 20000))
            fr = i;
    }
    if (i == PM_MAX) {
        if (fr < 0)
            return;
        i = fr;
        g_pm[i].st = s;
        g_pm[i].first = now;
        g_pm[i].logged = 0;
    }
    g_pm[i].last = now;
}

/* Sigue el ProjectileMotion del lanzamiento k. Devuelve 1 la vez que detecta que se detuvo (sin actualizaciones hace
 * 300 ms): pm_pos = última posición, pm_t = tiempo de vuelo, pm_dur = duración. */
static int pm_track(Track *k, const char *who)
{
    DWORD now = GetTickCount();
    if (!g_pm_on || k->pm_done)
        return 0;
    for (int i = 0; i < PM_MAX; i++) {
        /* diagnóstico (corrida 70): cada estado nuevo con su salida y velocidad */
        uint32_t s = g_pm[i].st;
        float w[6];
        if (s && !g_pm[i].logged && mem_read(s + PM_START, w, sizeof w)) {
            g_pm[i].logged = 1;
            log_printf("THROW pm: estado nuevo 0x%08x: salida (%.2f, %.2f, %.2f), vel (%.2f, %.2f, %.2f); %ld llamadas", s, (double)w[0],
                       (double)w[1], (double)w[2], (double)w[3], (double)w[4], (double)w[5], (long)g_pm_calls);
        }
    }
    if (!k->pm) {
        if (now - k->t0 > 1000)
            return 0;
        for (int i = 0; i < PM_MAX; i++) {
            uint32_t s = g_pm[i].st;
            float w[6];
            if (!s || now - g_pm[i].last > 200 || !mem_read(s + PM_START, w, sizeof w))
                continue;
            float dx = w[0] - k->ev.m[3], dz = w[2] - k->ev.m[11], vx = w[3] - k->ev.vel[0], vy = w[4] - k->ev.vel[1], vz = w[5] - k->ev.vel[2];
            if (dx * dx + dz * dz < 0.0025f && vx * vx + vy * vy + vz * vz < 0.01f) {
                k->pm = s;
                log_printf("THROW pm %u (%s): ProjectileMotion 0x%08x (salida y %.2f; el evento %.2f)", k->id, who, s, (double)w[1],
                           (double)k->ev.m[7]);
                break;
            }
        }
        if (!k->pm)
            return 0;
    }
    int i;
    for (i = 0; i < PM_MAX; i++)
        if (g_pm[i].st == k->pm)
            break;
    DWORD last = i < PM_MAX ? g_pm[i].last : 0;
    if (i < PM_MAX && now - last < 300) {
        float v[3], t = 0, d = 0;
        if (mem_f32(k->pm + PM_MX, &v[0]) && mem_f32(k->pm + PM_MY, &v[1]) && mem_f32(k->pm + PM_MZ, &v[2]) && mem_f32(k->pm + PM_T, &t) &&
            mem_f32(k->pm + PM_DUR, &d)) {
            aimp_memcpy(k->pm_pos, v, 12);
            k->pm_t = t;
            k->pm_dur = d;
        }
        return 0;
    }
    k->pm_done = 1;
    log_printf("THROW pm %u (%s): ProjectileMotion se detuvo a los %.2f s de %.2f en (%.2f, %.2f, %.2f)%s", k->id, who, (double)k->pm_t,
               (double)k->pm_dur, (double)k->pm_pos[0], (double)k->pm_pos[1], (double)k->pm_pos[2],
               k->pm_t < k->pm_dur - 0.05f ? " (antes de terminar: choque)" : "");
    return k->pm_t > 0.0f && k->pm_t < k->pm_dur - 0.05f;
}

static uint32_t pool_ptr(void) { return *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_THROW_POOL); }

static int pool_free_idx(uint32_t pool)
{
    for (uint32_t i = 0; i < 0x80; i++) {
        uint8_t f = 1;
        if (mem_u8(pool + 4 + i * 12, &f) && !f)
            return (int)i;
    }
    return -1;
}

/* objeto (TemporaryEntity) de una entrada ocupada del pool; 0 si está libre */
static uint32_t entry_obj(uint32_t pool, uint32_t idx)
{
    uint8_t f = 0;
    uint32_t hh = 0, obj = 0;
    if (!pool || idx >= 0x80 || !mem_u8(pool + 4 + idx * 12, &f) || !f || !mem_u32(pool + 4 + idx * 12 + 4, &hh) || !hh ||
        !mem_u32(hh + HANDLE_OBJ, &obj))
        return 0;
    return obj;
}

/* Entidad de posición del arrojable (2026-11-15, corridas 54-59): el CompositeInstance del arrojable (vt 0xFE1D2C) con el
 * mismo id (+0xC) que el TemporaryEntity del pool; su +0xA4 sigue la posición (corrida 38). Se busca recorriendo
 * punteros desde el objeto (hasta 5 niveles, 2048 nodos) y se acepta solo si coinciden la vtable y el id. El camino
 * "obj+0x80 -> +0xC" de la corrida 54 resultó casual (memoria vecina; +0x80 no es puntero en la run59). */
#define COMP_POS 0xA4u
/* offset de un triplete a menos de 1 m de p dentro de los primeros 0x600 bytes del composite; 0 si no hay */
static uint32_t comp_pos_off(uint32_t comp, const float *p)
{
    static uint8_t buf[0x600];
    if (!mem_read(comp, buf, sizeof buf))
        return 0;
    for (uint32_t off = 0x20; off + 12 <= sizeof buf; off += 4) {
        float v[3];
        aimp_memcpy(v, buf + off, 12);
        float dx = v[0] - p[0], dy = v[1] - p[1], dz = v[2] - p[2];
        if (dx * dx + dy * dy + dz * dz < 1.0f)
            return off;
    }
    return 0;
}

/* Con el composite seguro (camino fijo): el triplete más cercano a p a menos de 3 m, entre los parámetros
 * (+0x24 + 0x40*k: corridas 60-66), sin las copias quietas del punto de salida o (misma x y z, a partir de
 * 150 ms; son start_position y similares). Corrida 66: la bengala chocaba enseguida y a 1 m de la parábola no había nada. */
static uint32_t comp_pos_off_near(uint32_t comp, const float *p, const float *o, int skip_o)
{
    static uint8_t buf[0x600];
    if (!mem_read(comp, buf, sizeof buf))
        return 0;
    uint32_t best = 0;
    float bd = 9.0f;
    for (uint32_t off = 0x24; off + 12 <= sizeof buf; off += 0x40) {
        float v[3];
        aimp_memcpy(v, buf + off, 12);
        /* horizontal: el motor la pone 0,62 m más arriba que la matriz del evento (corridas 64-66) */
        float ox = v[0] - o[0], oz = v[2] - o[2];
        if (skip_o && ox * ox + oz * oz < 0.0001f)
            continue;
        float dx = v[0] - p[0], dy = v[1] - p[1], dz = v[2] - p[2], d = dx * dx + dy * dy + dz * dz;
        if (d < bd) {
            bd = d;
            best = off;
        }
    }
    return best;
}

#define FP_NODES 2048
#define FP_HASH 4096
static char g_fp_path[256];
#define FP_FIXED_A 0x24u
#define FP_FIXED_B 0x0Cu
static int g_fp_fixed_diag; /* 1: composite sin triplete cerca, 2: vtable o id distintos, 3: punteros ilegibles */
static uint32_t g_fp_fixed_c, g_fp_dumps;
static uint32_t find_pos_ent(uint32_t obj, const float *start, const float *origin, uint32_t age_ms, uint32_t *poff)
{
    static uint32_t q[FP_NODES], hs[FP_HASH];
    static uint8_t qd[FP_NODES], qo[FP_NODES];
    static uint16_t qp[FP_NODES];
    uint32_t id = 0, vtc = (uint32_t)g_aimp_exe + AIMP_RVA_VT_COMPOSITE, nq = 0, head = 0;
    if (!obj || !mem_u32(obj + OUT_ID, &id) || !id)
        return 0;
    {
        /* camino fijo (corrida 65: "obj +0x24 -> +0xc -> composite"); si no da, la búsqueda */
        uint32_t p1 = 0, c = 0, cw[4] = { 0, 0, 0, 0 };
        if (mem_u32(obj + FP_FIXED_A, &p1) && p1 && mem_u32(p1 + FP_FIXED_B, &c) && c && mem_read(c, cw, sizeof cw)) {
            if (cw[0] == vtc && cw[OUT_ID / 4] == id) {
                if ((*poff = comp_pos_off(c, start)) != 0 || (*poff = comp_pos_off_near(c, start, origin, age_ms > 150)) != 0) {
                    aimp_snprintf(g_fp_path, sizeof g_fp_path, "fijo obj +0x%x->0x%08x +0x%x->0x%08x", FP_FIXED_A, p1, FP_FIXED_B, c);
                    return c;
                }
                g_fp_fixed_diag = 1; /* el composite está, pero ningún triplete cerca de la parábola */
                if (g_fp_dumps < 12) {
                    /* corrida 67: 0 de 3; se vuelcan los parámetros (+0x24 + 0x40*k) para ver qué hay */
                    static uint8_t db[0x600];
                    char out[640];
                    size_t n = 0;
                    g_fp_dumps++;
                    if (mem_read(c, db, sizeof db))
                        for (uint32_t off = 0x24; off + 12 <= sizeof db && n + 48 < sizeof out; off += 0x40) {
                            float v[3];
                            aimp_memcpy(v, db + off, 12);
                            n += (size_t)aimp_snprintf(out + n, sizeof out - n, " +0x%x(%.2f,%.2f,%.2f)", off, (double)v[0], (double)v[1],
                                                       (double)v[2]);
                        }
                    out[n] = 0;
                    log_printf("THROW fijo diag: composite 0x%08x a los %u ms, parábola (%.2f, %.2f, %.2f):%s", c, age_ms, (double)start[0],
                               (double)start[1], (double)start[2], out);
                }
            } else {
                g_fp_fixed_diag = 2;
            }
            g_fp_fixed_c = c;
        } else {
            g_fp_fixed_diag = 3;
        }
    }
    aimp_memset(hs, 0, sizeof hs);
    g_fp_path[0] = 0;
    q[nq] = obj;
    qp[nq] = 0;
    qo[nq] = 0;
    qd[nq++] = 0;
    hs[(obj >> 2) & (FP_HASH - 1)] = obj;
    while (head < nq) {
        uint32_t o = q[head], d = qd[head++], w[64];
        if (!mem_read(o, w, sizeof w)) /* una sola lectura por nodo: vtable en w[0], id en w[3] (+0xC) */
            continue;
        if (o != obj && w[0] == vtc && w[OUT_ID / 4] == id && (*poff = comp_pos_off(o, start)) != 0) {
            /* camino desde el objeto (2026-11-16): para buscar uno fijo con varios lanzamientos */
            uint32_t chain[8], nc = 0, j = head - 1;
            while (j && nc < 8) {
                chain[nc++] = j;
                j = qp[j];
            }
            size_t n = (size_t)aimp_snprintf(g_fp_path, sizeof g_fp_path, "obj");
            while (nc--)
                n += (size_t)aimp_snprintf(g_fp_path + n, sizeof g_fp_path - n, " +0x%x->0x%08x", 4u * qo[chain[nc]], q[chain[nc]]);
            aimp_snprintf(g_fp_path + n, sizeof g_fp_path - n, " (%u nodos)", nq);
            return o;
        }
        if (d >= 5)
            continue;
        for (int i = 1; i < 64 && nq < FP_NODES; i++) {
            uint32_t v = w[i];
            if (v < 0x10000 || (v & 3) || v >= 0x7FFF0000u || (v >= (uint32_t)g_aimp_exe && v < (uint32_t)g_aimp_exe + AIMP_EXPECT_SIZEOFIMAGE))
                continue;
            uint32_t h = (v >> 2) & (FP_HASH - 1);
            int dup = 0;
            while (hs[h]) {
                if (hs[h] == v) {
                    dup = 1;
                    break;
                }
                h = (h + 1) & (FP_HASH - 1);
            }
            if (dup)
                continue;
            hs[h] = v;
            q[nq] = v;
            qp[nq] = (uint16_t)(head - 1);
            qo[nq] = (uint8_t)i;
            qd[nq++] = (uint8_t)(d + 1);
        }
    }
    return 0;
}

/* Cuerpo rígido de Havok (2026-11-16): corridas 70-71, la bengala no usa ProjectileMotion; vuela con la física.
 * RTTI: hkpRigidBody vtable VA 0x01413974 (TypeDescriptor 0x016652E8 -> COL -> vtable). Se busca desde el objeto
 * del pool (BFS por punteros, 4096 nodos, 6 niveles) un nodo con esa vtable, y adentro el triplete más cercano a
 * la parábola (< 1,5 m). Es un criterio firme: no depende del id de plantilla como el composite. */
#define AIMP_RVA_VT_HKP_RIGIDBODY 0x01013974u
#define RB_NODES 4096
#define RB_HASH 8192
static char g_rb_path[200];
static uint32_t rb_pos_off(uint32_t rb, const float *p)
{
    static uint8_t buf[0x400];
    if (!mem_read(rb, buf, sizeof buf))
        return 0;
    uint32_t best = 0;
    float bd = 2.25f;
    for (uint32_t off = 0x10; off + 12 <= sizeof buf; off += 4) {
        float v[3];
        aimp_memcpy(v, buf + off, 12);
        float dx = v[0] - p[0], dy = v[1] - p[1], dz = v[2] - p[2], d = dx * dx + dy * dy + dz * dz;
        if (d < bd) {
            bd = d;
            best = off;
        }
    }
    return best;
}
static uint32_t find_rigid(uint32_t obj, const float *p, uint32_t *poff, uint32_t *nfound)
{
    static uint32_t q[RB_NODES], hs[RB_HASH];
    static uint16_t qp[RB_NODES];
    static uint8_t qd[RB_NODES], qo[RB_NODES];
    uint32_t vtr = (uint32_t)g_aimp_exe + AIMP_RVA_VT_HKP_RIGIDBODY, nq = 0, head = 0, first = 0;
    *nfound = 0;
    g_rb_path[0] = 0;
    aimp_memset(hs, 0, sizeof hs);
    q[nq] = obj;
    qp[nq] = 0;
    qo[nq] = 0;
    qd[nq++] = 0;
    while (head < nq) {
        uint32_t o = q[head], d = qd[head++], w[64];
        if (!mem_read(o, w, sizeof w))
            continue;
        if (w[0] == vtr) {
            (*nfound)++;
            uint32_t off = rb_pos_off(o, p);
            if (!first || off) {
                uint32_t chain[8], nc = 0, j = head - 1;
                while (j && nc < 8) {
                    chain[nc++] = j;
                    j = qp[j];
                }
                size_t n = (size_t)aimp_snprintf(g_rb_path, sizeof g_rb_path, "obj");
                while (nc--)
                    n += (size_t)aimp_snprintf(g_rb_path + n, sizeof g_rb_path - n, " +0x%x", 4u * qo[chain[nc]]);
                first = o;
                *poff = off;
                if (off)
                    return o;
            }
            continue;
        }
        if (d >= 6)
            continue;
        for (int i = 1; i < 64 && nq < RB_NODES; i++) {
            uint32_t v = w[i];
            if (v < 0x10000 || (v & 3) || v >= 0x7FFF0000u || (v >= (uint32_t)g_aimp_exe && v < (uint32_t)g_aimp_exe + AIMP_EXPECT_SIZEOFIMAGE))
                continue;
            uint32_t h = (v >> 2) & (RB_HASH - 1);
            int dup = 0;
            while (hs[h]) {
                if (hs[h] == v) {
                    dup = 1;
                    break;
                }
                h = (h + 1) & (RB_HASH - 1);
            }
            if (dup)
                continue;
            hs[h] = v;
            q[nq] = v;
            qp[nq] = (uint16_t)(head - 1);
            qo[nq] = (uint8_t)i;
            qd[nq++] = (uint8_t)(d + 1);
        }
    }
    return first && *poff ? first : 0;
}
static int rb_pos(const Track *k, float out[3])
{
    uint32_t vt = 0;
    return k->rb && k->rboff && mem_u32(k->rb, &vt) && vt == (uint32_t)g_aimp_exe + AIMP_RVA_VT_HKP_RIGIDBODY &&
           mem_read(k->rb + k->rboff, out, 12);
}

static int ent_pos(const Track *k, float out[3])
{
    if (!k->ent && rb_pos(k, out))
        return 1;
    uint32_t vt = 0;
    return k->ent && mem_u32(k->ent, &vt) && vt == (uint32_t)g_aimp_exe + AIMP_RVA_VT_COMPOSITE && mem_read(k->ent + k->off, out, 12);
}

static Track *trk_add(Track *arr, uint8_t id, uint32_t idx, uint32_t obj, const ThrowEvt *ev)
{
    Track *t = &arr[0];
    for (int i = 0; i < TRK_MAX; i++) {
        if (!arr[i].on) {
            t = &arr[i];
            break;
        }
        if (arr[i].t0 - t->t0 > 0x80000000u) /* el más viejo */
            t = &arr[i];
    }
    aimp_memset(t, 0, sizeof *t);
    t->on = 1;
    t->id = id;
    t->idx = idx;
    t->obj = obj;
    t->t0 = GetTickCount();
    if (ev)
        t->ev = *ev;
    return t;
}

static uint32_t rva(uint32_t p) { return p >= (uint32_t)g_aimp_exe ? p - (uint32_t)g_aimp_exe : p; }

static float bits_f(uint32_t u)
{
    float f;
    aimp_memcpy(&f, &u, 4);
    return f;
}

static float sqrt_f(float x)
{
    float r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

/* ---- Pickups compartidos (2026-11-16, estático) ----
 * Al recoger un ítem, el composite del pickup dispara "pickup_collected"; el callback 0x7AFDC0 llama a
 * FUN_007a2b30 thiscall(gestor = [0x016F0DE8], Handle **h), ret 4: toma los ids (FUN_008c3360) de la
 * entidad (*h)+0xC y llama a FUN_00795ee0 thiscall(gestor+0x18, ids*, 0), ret 8, que lo aplica acá
 * (gestor+0x18 vt+4: lo saca del mundo) y encola el RPC A 0x23 (hallazgo 11; handler A 0x418D60 valida con
 * vt+8, handler de sesión 0x418CE0 aplica con vt+4). El otro hace lo mismo con FUN_00795ee0: el ítem
 * desaparece y nadie más lo recibe. La entidad se busca por pid (pins.c, determinista); si no está en el
 * índice, se usan los ids del otro (corrida 3: "5d9b3722 00000000", quizá dinámicos).
 *   AIMP_TRACE_PICKUP=1  loguea cada pickup recogido (solo mira).
 *   AIMP_SYNC_PICKUP=1   además lo manda (AIMP_EVT_PICKUP) y aplica los del otro (con AIMP_NET). */
#define AIMP_RVA_PICKUP_COLLECTED 0x003A2B30u
#define AIMP_RVA_PICKUP_APPLY     0x00395EE0u
#define AIMP_RVA_PICKUP_MGR       0x012F0DE8u
#define PICKUP_MGR_RPC            0x18u
static const uint8_t AIMP_PICKUP_PROLOGUE[9] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x00, 0x83, 0xEC, 0x08 };
static const uint8_t AIMP_PICKUP_APPLY_PROLOGUE[7] = { 0x55, 0x8B, 0xEC, 0x56, 0x57, 0x8B, 0xF9 };
typedef void(__attribute__((fastcall)) * PickupFn)(void *mgr, void *edx, void *h);
typedef void(__attribute__((fastcall)) * PickupApplyFn)(void *rpc, void *edx, const uint32_t *ids, uint32_t flag);
static AimpHook g_hk_pick;
static PickupFn g_pick_orig;
static PickupApplyFn g_pick_apply;
static int g_pick_trace, g_pick_sync;
static volatile LONG g_pick_n;
#define PICK_Q 8
static PickupEvt g_pick_q[PICK_Q];
static int g_npick_q;

static void __attribute__((fastcall)) det_pick(void *mgr, void *edx, void *h)
{
    uint32_t hh = 0, o = 0, ids[2] = { 0, 0 };
    PickupEvt pe;
    aimp_memset(&pe, 0, sizeof pe);
    if (h && mem_u32((uintptr_t)h, &hh) && hh && mem_u32(hh + HANDLE_OBJ, &o) && o) {
        mem_u32(o + 0xC, &pe.eid);
        mem_u32(o + 0x10, &pe.type);
        if (g_ids)
            g_ids((void *)o, NULL, ids);
        pe.pid = pins_path_id_of(o, &pe.depth);
    }
    pe.ids[0] = ids[0];
    pe.ids[1] = ids[1];
    LONG n = InterlockedIncrement(&g_pick_n);
    log_printf("PICKUP recogido %ld: gestor 0x%08x, entidad 0x%08x id %08x tipo %08x pid %08x (profundidad %u), ids %08x %08x", (long)n,
               (uint32_t)mgr, o, pe.eid, pe.type, pe.pid, pe.depth, ids[0], ids[1]);
    if (g_pick_sync && o && g_cs_ok) {
        EnterCriticalSection(&g_cs);
        if (g_npick_q < PICK_Q)
            g_pick_q[g_npick_q++] = pe;
        LeaveCriticalSection(&g_cs);
    }
    g_pick_orig(mgr, edx, h);
}

/* Creación de un pickup (para ubicarlos en las pruebas): FUN_007afe00 thiscall(this, desc, char), ret 8; desc+0x04..+0x30
 * es la matriz 3x4 (traslación en +0x10, +0x20, +0x30; la copia a locales), desc+0x48/+0x4C dos ids. Prólogo SEH:
 * 6A FF 68 <imm32 reubicable> 64 A1 00 00 00 00; se roban los 7 bytes de los dos push, tal como están en memoria. */
#define AIMP_RVA_PICKUP_SPAWN 0x003AFE00u
typedef void(__attribute__((fastcall)) * PickSpawnFn)(void *self, void *edx, void *desc, uint32_t flag);
static AimpHook g_hk_pspawn;
static PickSpawnFn g_pspawn_orig;
static volatile LONG g_pspawn_n;
static void __attribute__((fastcall)) det_pspawn(void *self, void *edx, void *desc, uint32_t flag)
{
    float m[12];
    uint32_t id2[2] = { 0, 0 };
    LONG n = InterlockedIncrement(&g_pspawn_n);
    if (n <= 400 && desc && mem_read((uintptr_t)desc + 4, m, sizeof m)) {
        mem_read((uintptr_t)desc + 0x48, id2, sizeof id2);
        log_printf("PICKUP creado %ld: en (%.2f, %.2f, %.2f), ids del descriptor %08x %08x, flag %u", (long)n, (double)m[3], (double)m[7],
                   (double)m[11], id2[0], id2[1], flag & 0xFF);
    }
    g_pspawn_orig(self, edx, desc, flag);
}

static void pickup_install(void)
{
    g_pick_trace = aimp_env_int(L"AIMP_TRACE_PICKUP", 0);
    g_pick_sync = aimp_env_int(L"AIMP_SYNC_PICKUP", 0);
    if (!g_pick_trace && !g_pick_sync)
        return;
    uint8_t a[sizeof AIMP_PICKUP_APPLY_PROLOGUE], b[sizeof AIMP_ENT_IDS_PROLOGUE];
    uintptr_t pa = g_aimp_exe + AIMP_RVA_PICKUP_APPLY, pi = g_aimp_exe + AIMP_RVA_ENT_IDS;
    if (mem_read(pi, b, sizeof b) && !aimp_memcmp(b, AIMP_ENT_IDS_PROLOGUE, sizeof b))
        g_ids = (IdsFn)(void *)pi;
    if (mem_read(pa, a, sizeof a) && !aimp_memcmp(a, AIMP_PICKUP_APPLY_PROLOGUE, sizeof a))
        g_pick_apply = (PickupApplyFn)(void *)pa;
    else if (g_pick_sync) {
        log_printf("PICKUP: el prólogo de FUN_00795ee0 no coincide; no se aplican los del otro");
        g_pick_sync = 0;
    }
    if (!g_cs_ok) {
        InitializeCriticalSection(&g_cs);
        g_cs_ok = 1;
    }
    int ok = hook_install(&g_hk_pick, "pickup recogido (FUN_007a2b30)", (void *)(g_aimp_exe + AIMP_RVA_PICKUP_COLLECTED),
                          AIMP_PICKUP_PROLOGUE, sizeof AIMP_PICKUP_PROLOGUE, sizeof AIMP_PICKUP_PROLOGUE, (void *)det_pick,
                          (void **)&g_pick_orig);
    if (!ok)
        g_pick_sync = g_pick_trace = 0;
    if (g_pick_trace) {
        uint8_t sp[13];
        uintptr_t ps = g_aimp_exe + AIMP_RVA_PICKUP_SPAWN;
        static const uint8_t tail[6] = { 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
        if (mem_read(ps, sp, sizeof sp) && sp[0] == 0x6A && sp[1] == 0xFF && sp[2] == 0x68 && !aimp_memcmp(sp + 7, tail, sizeof tail))
            hook_install(&g_hk_pspawn, "pickup creado (FUN_007afe00)", (void *)ps, sp, sizeof sp, 7, (void *)det_pspawn,
                         (void **)&g_pspawn_orig);
        else
            log_printf("PICKUP: el prólogo de FUN_007afe00 no coincide; sin log de creación");
    }
    log_printf("PICKUP: AIMP_TRACE_PICKUP=%d AIMP_SYNC_PICKUP=%d; hook %s; ids %s", g_pick_trace, g_pick_sync, ok ? "ok" : "NO",
               g_ids ? "ok" : "NO");
}

/* Por frame: manda los recogidos (no en el detour: net_event no es para otro thread). */
static void pickup_frame(void)
{
    PickupEvt q[PICK_Q];
    int n = 0;
    if (!g_pick_sync || !g_cs_ok)
        return;
    EnterCriticalSection(&g_cs);
    n = g_npick_q;
    aimp_memcpy(q, g_pick_q, sizeof(PickupEvt) * (size_t)n);
    g_npick_q = 0;
    LeaveCriticalSection(&g_cs);
    for (int i = 0; i < n; i++) {
        int sent = g_net && g_net->mode && net_event(g_net, AIMP_EVT_PICKUP, g_level, &q[i], sizeof q[i]);
        log_printf("PICKUP   pid %08x %s", q[i].pid, sent ? "mandado al otro jugador" : "no se pudo mandar (sin red o sin el otro)");
    }
}

void throw_pickup_event(const AimpEvt *e, uint32_t level)
{
    PickupEvt pe;
    if (!g_pick_sync || e->size != sizeof pe)
        return;
    aimp_memcpy(&pe, e->data, sizeof pe);
    if (e->level != level) {
        log_printf("PICKUP remoto: pid %08x del nivel %u; acá el nivel es %u: se descarta", pe.pid, e->level, level);
        return;
    }
    uint32_t mgr = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_PICKUP_MGR), ids[2] = { pe.ids[0], pe.ids[1] }, o = 0;
    const char *src = "los ids del otro";
    if (pe.pid && (o = pins_lookup_outer(pe.pid, 1)) != 0 && g_ids) {
        uint32_t loc[2] = { 0, 0 }, eid = 0;
        mem_u32(o + 0xC, &eid);
        g_ids((void *)o, NULL, loc);
        if (eid == pe.eid && (loc[0] || loc[1])) {
            ids[0] = loc[0];
            ids[1] = loc[1];
            src = "la entidad local con el mismo pid";
        }
    }
    if (!mgr || !g_pick_apply) {
        log_printf("PICKUP remoto: pid %08x sin gestor de pickups (0x%08x); no se aplica", pe.pid, mgr);
        return;
    }
    static uint32_t s_ids[4] __attribute__((aligned(16)));
    s_ids[0] = ids[0];
    s_ids[1] = ids[1];
    log_printf("PICKUP remoto: pid %08x id %08x, ids del otro %08x %08x, acá %08x %08x (%s; entidad 0x%08x): FUN_00795ee0", pe.pid, pe.eid,
               pe.ids[0], pe.ids[1], ids[0], ids[1], src, o);
    g_pick_apply((void *)(mgr + PICKUP_MGR_RPC), NULL, s_ids, 0);
}

int throw_sync_enabled(void) { return g_sync || g_pick_sync; }

static void log_evt(const char *what, LONG n, const ThrowEvt *t, uint32_t from)
{
    float v = sqrt_f(t->vel[0] * t->vel[0] + t->vel[1] * t->vel[1] + t->vel[2] * t->vel[2]);
    if (t->op == THROW_OP_THROW)
        log_printf("THROW %s %ld: tirar %s desde (%.2f, %.2f, %.2f), vel (%.2f, %.2f, %.2f) |%.2f|, a3 %.2f, t %.2f%s%x", what,
                   (long)n, t->name, (double)t->m[3], (double)t->m[7], (double)t->m[11], (double)t->vel[0], (double)t->vel[1],
                   (double)t->vel[2], (double)v, (double)bits_f(t->a3), (double)bits_f(t->t), from ? ", desde rva 0x" : "", from);
    else
        log_printf("THROW %s %ld: colocar %s en (%.2f, %.2f, %.2f), t %.2f%s%x", what, (long)n, t->name, (double)t->m[3],
                   (double)t->m[7], (double)t->m[11], (double)bits_f(t->t), from ? ", desde rva 0x" : "", from);
}

/* Lo que hizo el jugador local (en el detour): se encola para throw_frame. */
static void local_event(const ThrowEvt *t, uint32_t from)
{
    if (!g_cs_ok)
        return;
    EnterCriticalSection(&g_cs);
    if (g_nlocal < ECHO_MAX) {
        g_local[g_nlocal].t = *t;
        g_local[g_nlocal].level = g_level;
        g_local[g_nlocal].from = from;
        g_nlocal++;
    } else {
        InterlockedIncrement(&g_n_lost);
    }
    LeaveCriticalSection(&g_cs);
}

/* throw_frame: log, red y eco de un evento local. */
static void note_start(const ThrowEvt *t)
{
    g_last_start[0] = t->m[3];
    g_last_start[1] = t->m[7];
    g_last_start[2] = t->m[11];
    g_have_start = 1;
}

static void process_local(const Item *it)
{
    note_start(&it->t);
    LONG n = InterlockedIncrement(&g_n_local);
    if (n <= LOG_MAX)
        log_evt("local", n, &it->t, it->from);
    if (g_sync && g_net && g_net->mode) {
        if (net_event(g_net, AIMP_EVT_THROW, it->level, &it->t, sizeof it->t)) {
            if (n <= LOG_MAX)
                log_printf("THROW   mandado al otro jugador (evento %u, nivel %u)", g_net->evt_seq, it->level);
        } else if (n <= LOG_MAX) {
            log_printf("THROW   no se pudo mandar (sin par o cola llena)");
        }
    }
    if (g_echo_ms > 0 && it->t.op == THROW_OP_THROW && g_necho < ECHO_MAX) {
        g_echo[g_necho] = *it;
        g_echo[g_necho].due_ms = GetTickCount() + (DWORD)g_echo_ms;
        g_necho++;
    }
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_spawn(void *pool, void *edx, const float *m,
                                                                                 const float *vel, uint32_t a3,
                                                                                 const char *name, uint32_t t)
{
    ThrowEvt e;
    aimp_memset(&e, 0, sizeof e);
    e.op = THROW_OP_THROW;
    int idx = g_fix ? pool_free_idx((uint32_t)(uintptr_t)pool) : -1;
    if (m && vel && name && mem_read((uintptr_t)m, e.m, sizeof e.m) && mem_read((uintptr_t)vel, e.vel, sizeof e.vel) &&
        mem_cstr((uintptr_t)name, e.name, sizeof e.name)) {
        e.a3 = a3;
        e.t = t;
        if (g_fix) {
            if (!++g_next_id)
                g_next_id = 1;
            e.pad[0] = g_next_id;
        }
        local_event(&e, rva((uint32_t)(uintptr_t)__builtin_return_address(0)));
    }
    g_spawn_orig(pool, edx, m, vel, a3, name, t);
    if (g_fix && idx >= 0 && e.pad[0]) {
        uint32_t obj = entry_obj((uint32_t)(uintptr_t)pool, (uint32_t)idx);
        if (obj)
            trk_add(g_trk_local, e.pad[0], (uint32_t)idx, obj, &e);
    }
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_place(void *pool, void *edx, const float *m,
                                                                                 const char *name, uint32_t t)
{
    ThrowEvt e;
    aimp_memset(&e, 0, sizeof e);
    e.op = THROW_OP_PLACE;
    if (m && name && mem_read((uintptr_t)m, e.m, sizeof e.m) && mem_cstr((uintptr_t)name, e.name, sizeof e.name)) {
        e.t = t;
        local_event(&e, rva((uint32_t)(uintptr_t)__builtin_return_address(0)));
    }
    g_place_orig(pool, edx, m, name, t);
}

int throw_install(AimpNet *net)
{
    g_trace = aimp_env_int(L"AIMP_TRACE_THROW", 0);
    g_sync = aimp_env_int(L"AIMP_SYNC_THROW", 0);
    g_echo_ms = aimp_env_int(L"AIMP_THROW_ECHO", 0);
    g_echo_cm = aimp_env_int(L"AIMP_THROW_ECHO_CM", 75);
    g_probe = aimp_env_int(L"AIMP_THROW_PROBE", 0);
    /* experimento (2026-11-15): a los 3 s la sonda corre +1 m en x todos los candidatos que se movieron, para ver
     * si el objeto asentado se queda donde se lo escribe (corrección de la caída de la réplica) */
    g_fix_test = aimp_env_int(L"AIMP_THROW_FIX_TEST", 0);
    g_net = net;
    pickup_install();
    if (!g_trace && !g_sync && g_echo_ms <= 0 && !g_probe)
        return g_pick_trace || g_pick_sync;
    g_fix = g_sync ? aimp_env_int(L"AIMP_THROW_FIX", 1) : 0;
    g_fix_cm = aimp_env_int(L"AIMP_THROW_FIX_CM", 15);
    if (g_fix) {
        uint8_t pr[sizeof AIMP_ENT_IDS_PROLOGUE];
        uintptr_t pi = g_aimp_exe + AIMP_RVA_ENT_IDS, pr2 = g_aimp_exe + AIMP_RVA_THROW_RELEASE;
        uint8_t rp[sizeof AIMP_THROW_PROLOGUE];
        if (mem_read(pi, pr, sizeof pr) && !aimp_memcmp(pr, AIMP_ENT_IDS_PROLOGUE, sizeof pr) && mem_read(pr2, rp, sizeof rp) &&
            !aimp_memcmp(rp, AIMP_THROW_PROLOGUE, sizeof rp)) {
            g_ids = (IdsFn)(void *)pi;
            g_release = (ReleaseFn)(void *)pr2;
        } else {
            log_printf("THROW: el prólogo de FUN_008c3360 o FUN_007a10f0 no coincide; sin corrección de dónde cae");
            g_fix = 0;
        }
    }
    g_net = net;
    if (!g_cs_ok) {
        InitializeCriticalSection(&g_cs);
        g_cs_ok = 1;
    }
    int ok = hook_install(&g_hk_spawn, "tirar (FUN_007b5050)", (void *)(g_aimp_exe + AIMP_RVA_THROW_SPAWN), AIMP_THROW_PROLOGUE,
                          sizeof AIMP_THROW_PROLOGUE, AIMP_THROW_STEAL, (void *)det_spawn, (void **)&g_spawn_orig);
    int ok2 = hook_install(&g_hk_place, "colocar (FUN_007b5190)", (void *)(g_aimp_exe + AIMP_RVA_THROW_PLACE),
                           AIMP_THROW_PROLOGUE, sizeof AIMP_THROW_PROLOGUE, AIMP_THROW_STEAL, (void *)det_place,
                           (void **)&g_place_orig);
    if (g_fix && aimp_env_int(L"AIMP_THROW_PM", 0) /* corridas 70-71: las bengalas no pasan por acá */)
        g_pm_on = hook_install(&g_hk_pm, "ProjectileMotion (FUN_0060b6d0)", (void *)(g_aimp_exe + AIMP_RVA_PM_UPDATE),
                               AIMP_PM_UPD_PROLOGUE, sizeof AIMP_PM_UPD_PROLOGUE, sizeof AIMP_PM_UPD_PROLOGUE, (void *)det_pm,
                               (void **)&g_pm_orig);
    if (g_pm_on)
        g_pm_on = hook_install(&g_hk_pm2, "ProjectileMotion (FUN_0060b980)", (void *)(g_aimp_exe + AIMP_RVA_PM_UPDATE2),
                               AIMP_PM_UPD2_PROLOGUE, sizeof AIMP_PM_UPD2_PROLOGUE, sizeof AIMP_PM_UPD2_PROLOGUE, (void *)det_pm2,
                               (void **)&g_pm2_orig);
    if (!ok || !ok2) {
        g_sync = 0;
        g_echo_ms = 0;
    }
    log_printf("THROW: AIMP_TRACE_THROW=%d AIMP_SYNC_THROW=%d%s AIMP_THROW_ECHO=%d ms (%d cm) AIMP_THROW_PROBE=%d AIMP_THROW_FIX=%d (%d cm); hooks %s/%s",
               g_trace, g_sync, g_sync && !(net && net->mode) ? " (sin red: solo se loguea)" : "", g_echo_ms, g_echo_cm, g_probe,
               g_fix, g_fix_cm, ok ? "ok" : "NO", ok2 ? "ok" : "NO");
    return ok && ok2;
}

static const char *intern(const char *name)
{
    for (int i = 0; i < NAMES_MAX; i++) {
        if (!g_names[i][0]) {
            aimp_memcpy(g_names[i], name, THROW_NAME_MAX);
            g_names[i][THROW_NAME_MAX - 1] = 0;
            return g_names[i];
        }
        if (aimp_memcmp(g_names[i], name, aimp_strlen(g_names[i]) + 1) == 0)
            return g_names[i];
    }
    return NULL;
}

static uint32_t local_player(void)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0, pl = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm || !mem_u32(cm + CM_NPLAYERS, &n) || !n ||
        !mem_u32(cm + CM_PLAYERS, &arr) || !mem_u32(arr, &pl))
        return 0;
    return pl;
}

/* Llama a la función original (por el trampolín: no pasa por el detour). 1 si se llamó. */
static int apply(const ThrowEvt *t, const char *who)
{
    static float s_m[16] __attribute__((aligned(16)));
    static float s_v[4] __attribute__((aligned(16)));
    uint32_t pool = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_THROW_POOL);
    LONG n = InterlockedIncrement(&g_n_remote);
    char name[THROW_NAME_MAX];
    aimp_memcpy(name, t->name, sizeof name);
    name[sizeof name - 1] = 0;
    const char *nm = intern(name);
    if (!pool || !nm || !local_player() || (t->op == THROW_OP_THROW && !g_spawn_orig) ||
        (t->op == THROW_OP_PLACE && !g_place_orig) || (t->op != THROW_OP_THROW && t->op != THROW_OP_PLACE)) {
        if (n <= LOG_MAX)
            log_printf("THROW %s %ld: no se aplica (pool 0x%08x, plantilla %s, jugador %s, op %u)", who, (long)n, pool,
                       nm ? "ok" : "sin lugar", local_player() ? "sí" : "no", t->op);
        return 0;
    }
    if (n <= LOG_MAX)
        log_evt(who, n, t, 0);
    note_start(t);
    aimp_memcpy(s_m, t->m, sizeof t->m);
    aimp_memcpy(s_m + 12, t->vel, sizeof t->vel); /* como en FUN_007b9500: la velocidad sigue a la matriz */
    s_m[15] = 9.81f;
    aimp_memcpy(s_v, t->vel, sizeof t->vel);
    s_v[3] = 9.81f;
    int idx = pool_free_idx(pool);
    if (t->op == THROW_OP_THROW)
        g_spawn_orig((void *)(uintptr_t)pool, NULL, s_m, s_v, t->a3, nm, t->t);
    else
        g_place_orig((void *)(uintptr_t)pool, NULL, s_m, nm, t->t);
    if (g_fix && t->op == THROW_OP_THROW && t->pad[0] && idx >= 0) {
        uint32_t obj = entry_obj(pool, (uint32_t)idx);
        if (obj)
            trk_add(g_trk_rep, t->pad[0], (uint32_t)idx, obj, t);
    }
    return 1;
}

void throw_remote_event(const AimpEvt *e, uint32_t level)
{
    ThrowEvt t;
    if (e->size != sizeof t) {
        log_printf("THROW: evento remoto de %u bytes (se esperaban %u): se ignora", e->size, (unsigned)sizeof t);
        return;
    }
    aimp_memcpy(&t, e->data, sizeof t);
    if (e->level != level) {
        log_printf("THROW: lanzamiento remoto (evento %u, %s) del nivel %u; acá el nivel es %u: se descarta", e->seq,
                   t.name[0] ? "con plantilla" : "sin plantilla", e->level, level);
        return;
    }
    apply(&t, "remoto");
}


/* Por frame: los arrojables propios; cuando uno queda quieto 0,5 s, se manda dónde. */
/* diagnóstico: offsets del composite con tripletes a menos de 4 m del lanzamiento */
static void comp_diag(const Track *k, const char *when)
{
    static uint8_t buf[0x600];
    char out[600];
    int n = 0, c = 0;
    if (!mem_read(k->ent, buf, sizeof buf))
        return;
    for (uint32_t off = 0; off + 12 <= sizeof buf && c < 24; off += 4) {
        float v[3];
        aimp_memcpy(v, buf + off, 12);
        float dx = v[0] - k->ev.m[3], dy = v[1] - k->ev.m[7], dz = v[2] - k->ev.m[11];
        if (!(dx * dx + dy * dy + dz * dz < 16.0f))
            continue;
        n += aimp_snprintf(out + n, sizeof out - n, " +0x%x(%.2f,%.2f,%.2f)", off, (double)v[0], (double)v[1], (double)v[2]);
        c++;
    }
    log_printf("THROW fijo %u diag %s: composite 0x%08x:%s", k->id, when, k->ent, c ? out : " nada cerca");
}

static void track_find_ents(Track *arr, const char *who)
{
    uint32_t pool = pool_ptr();
    for (int i = 0; i < TRK_MAX; i++) {
        Track *k = &arr[i];
        DWORD age = GetTickCount() - k->t0;
        /* se busca cada 100 ms durante el primer segundo, contra la parábola prevista (ProjectileMotion es analítico) */
        if (!k->on || k->ent || age > 1000 || (k->still && age - k->still < 100) || entry_obj(pool, k->idx) != k->obj)
            continue;
        k->still = age ? age : 1; /* acá "still" guarda cuándo se buscó; se pone en 0 al encontrarlo */
        float t = (float)age / 1000.0f;
        float st[3] = { k->ev.m[3] + k->ev.vel[0] * t, k->ev.m[7] + k->ev.vel[1] * t - 4.905f * t * t, k->ev.m[11] + k->ev.vel[2] * t };
        if (!k->rb && k->rb_tries < 4) {
            uint32_t nf = 0, off = 0;
            k->rb_tries++;
            uint32_t rb = find_rigid(k->obj, st, &off, &nf);
            if (rb) {
                k->rb = rb;
                k->rboff = off;
                float v[3];
                mem_read(rb + off, v, 12);
                log_printf("THROW fijo %u (%s): hkpRigidBody 0x%08x, posición en +0x%x (%.2f, %.2f, %.2f), a los %lu ms; camino %s", k->id, who, rb,
                           off, (double)v[0], (double)v[1], (double)v[2], (unsigned long)age, g_rb_path);
            } else {
                log_printf("THROW fijo %u (%s): sin hkpRigidBody con posición cerca (%u con la vtable; camino del primero: %s), a los %lu ms",
                           k->id, who, nf, nf ? g_rb_path : "-", (unsigned long)age);
            }
        }
        float org[3] = { k->ev.m[3], k->ev.m[7], k->ev.m[11] };
        k->ent = find_pos_ent(k->obj, st, org, age, &k->off);
        if (k->ent) {
            k->still = 0;
            comp_diag(k, "al encontrarlo");
        }
        if (k->ent)
            log_printf("THROW fijo %u (%s): posición en el composite 0x%08x +0x%x, a los %lu ms; camino: %s", k->id, who, k->ent, k->off,
                       (unsigned long)(GetTickCount() - k->t0), g_fp_path);
        else if (age >= 900)
            log_printf("THROW fijo %u (%s): no se encontró el composite en el primer segundo (objeto 0x%08x; camino fijo: %s, 0x%08x)", k->id,
                       who, k->obj,
                       g_fp_fixed_diag == 1   ? "composite sin triplete a menos de 1 m"
                       : g_fp_fixed_diag == 2 ? "vtable o id distintos"
                                              : "punteros ilegibles",
                       g_fp_fixed_c);
    }
}

static void track_local_frame(void)
{
    uint32_t pool = pool_ptr();
    DWORD ms = GetTickCount();
    track_find_ents(g_trk_local, "propio");
    track_find_ents(g_trk_rep, "réplica");
    for (int i = 0; i < TRK_MAX; i++)
        if (g_trk_rep[i].on)
            pm_track(&g_trk_rep[i], "réplica"); /* solo para el log: comparar con el del otro */
    for (int i = 0; i < TRK_MAX; i++) {
        Track *k = &g_trk_local[i];
        if (!k->on)
            continue;
        DWORD age = ms - k->t0;
        float p[3];
        if (entry_obj(pool, k->idx) != k->obj) {
            log_printf("THROW fijo %u: el objeto propio desapareció antes de asentarse (%lu ms)", k->id, (unsigned long)age);
            k->on = 0;
            continue;
        }
        if (pm_track(k, "propio") && !k->ent) {
            /* sin composite: el punto del choque de ProjectileMotion (los rebotes de Havok después no se ven) */
            ThrowRest r;
            aimp_memset(&r, 0, sizeof r);
            r.id = k->id;
            aimp_memcpy(r.pos, k->pm_pos, 12);
            r.ms = age;
            int sent = g_net && g_net->mode && net_event(g_net, AIMP_EVT_THROW_REST, g_level, &r, sizeof r);
            log_printf("THROW fijo %u: sin composite; se usa el punto del choque (%.2f, %.2f, %.2f); %s", k->id, (double)r.pos[0],
                       (double)r.pos[1], (double)r.pos[2], sent ? "mandado" : "no se pudo mandar");
            k->on = 0;
            continue;
        }
        if (!ent_pos(k, p)) {
            if (age > 15000) {
                log_printf("THROW fijo %u: no se pudo leer la posición del objeto propio (vtable distinta); no se manda", k->id);
                k->on = 0;
            }
            continue;
        }
        float dx = p[0] - k->last[0], dy = p[1] - k->last[1], dz = p[2] - k->last[2];
        aimp_memcpy(k->last, p, 12);
        if (dx * dx + dy * dy + dz * dz > 0.0001f) {
            k->still = 0;
            if (age > 15000) {
                log_printf("THROW fijo %u: sigue moviéndose a los %lu ms; no se manda", k->id, (unsigned long)age);
                k->on = 0;
            }
            continue;
        }
        if (!k->still)
            k->still = ms;
        if (ms - k->still < 500 || age < 800)
            continue;
        ThrowRest r;
        aimp_memset(&r, 0, sizeof r);
        r.id = k->id;
        aimp_memcpy(r.pos, p, 12);
        r.ms = age;
        int sent = g_net && g_net->mode && net_event(g_net, AIMP_EVT_THROW_REST, g_level, &r, sizeof r);
        log_printf("THROW fijo %u: el propio se asentó en (%.2f, %.2f, %.2f) a los %lu ms; %s", k->id, (double)p[0], (double)p[1],
                   (double)p[2], (unsigned long)age, sent ? "mandado" : "no se pudo mandar");
        k->on = 0;
    }
}

void throw_rest_event(const AimpEvt *e, uint32_t level)
{
    ThrowRest r;
    if (!g_fix || e->size != sizeof r)
        return;
    aimp_memcpy(&r, e->data, sizeof r);
    if (e->level != level)
        return;
    Track *k = NULL;
    for (int i = 0; i < TRK_MAX; i++)
        if (g_trk_rep[i].on && g_trk_rep[i].id == r.id && (!k || g_trk_rep[i].t0 - k->t0 < 0x80000000u))
            k = &g_trk_rep[i];
    if (!k) {
        log_printf("THROW fijo %u: el otro dice que cayó en (%.2f, %.2f, %.2f), pero no hay réplica con ese id", r.id,
                   (double)r.pos[0], (double)r.pos[1], (double)r.pos[2]);
        return;
    }
    k->on = 0;
    uint32_t pool = pool_ptr();
    float p[3] = { 0, 0, 0 };
    int have = entry_obj(pool, k->idx) == k->obj;
    if (have && k->ent)
        comp_diag(k, "al asentarse");
    int hp = have && ent_pos(k, p);
    float dx = p[0] - r.pos[0], dy = p[1] - r.pos[1], dz = p[2] - r.pos[2];
    float d = sqrt_f(dx * dx + dy * dy + dz * dz);
    if (hp && d * 100.0f <= (float)g_fix_cm) {
        log_printf("THROW fijo %u: la réplica cayó a %.2f m de la del otro; se deja", r.id, (double)d);
        return;
    }
    if (!have) {
        /* ya no está (se apagó o explotó): no se crea otra */
        log_printf("THROW fijo %u: la réplica ya no estaba; no se crea otra", r.id);
        return;
    }
    uint32_t ids[2] = { 0, 0 };
    g_ids((void *)(uintptr_t)k->obj, NULL, ids);
    g_release((void *)(uintptr_t)pool, NULL, ids[0], ids[1]);
    int freed = entry_obj(pool, k->idx) != k->obj;
    /* una quieta, 5 cm arriba del punto, con el tiempo que le queda */
    static float s_m[16] __attribute__((aligned(16)));
    static float s_v[4] __attribute__((aligned(16)));
    aimp_memcpy(s_m, k->ev.m, sizeof k->ev.m);
    s_m[3] = r.pos[0];
    s_m[7] = r.pos[1] + 0.05f;
    s_m[11] = r.pos[2];
    s_m[12] = s_m[13] = s_m[14] = 0.0f;
    s_m[15] = 9.81f;
    s_v[0] = s_v[1] = s_v[2] = 0.0f;
    s_v[3] = 9.81f;
    float tl = bits_f(k->ev.t) - (float)(GetTickCount() - k->t0) / 1000.0f;
    if (tl < 1.0f)
        tl = 1.0f;
    uint32_t tlb;
    aimp_memcpy(&tlb, &tl, 4);
    char name[THROW_NAME_MAX];
    aimp_memcpy(name, k->ev.name, sizeof name);
    name[sizeof name - 1] = 0;
    const char *nm = intern(name);
    if (!freed || !nm || !local_player()) {
        log_printf("THROW fijo %u: no se pudo reemplazar la réplica (liberada %s, plantilla %s)", r.id, freed ? "sí" : "no",
                   nm ? "ok" : "sin lugar");
        return;
    }
    g_spawn_orig((void *)(uintptr_t)pool, NULL, s_m, s_v, k->ev.a3, nm, tlb);
    if (hp)
        log_printf("THROW fijo %u: la réplica cayó a %.2f m de la del otro: se borra y se pone en (%.2f, %.2f, %.2f), quedan %.1f s",
                   r.id, (double)d, (double)r.pos[0], (double)r.pos[1], (double)r.pos[2], (double)tl);
    else
        log_printf("THROW fijo %u: réplica sin posición legible, liberada; se pone en (%.2f, %.2f, %.2f), quedan %.1f s", r.id,
                   (double)r.pos[0], (double)r.pos[1], (double)r.pos[2], (double)tl);
}

/* ---- AIMP_THROW_PROBE (solo mira; prueba con dos máquinas 2026-11-15: el noisemaker cayó unos cm al costado) ----
 * ProjectileMotion (FUN_0060b740) integra el vuelo con el dt del frame, así que con fps distintos la trayectoria
 * se separa y los rebotes lo amplifican. Para corregir la réplica hace falta saber dónde vive la posición del
 * objeto. La sonda mira el pool cada frame; cuando una entrada pasa a "en uso", recorre su composite (hijos en
 * +0x2C, hallazgo 16), busca tripletes de float a menos de 8 m del lanzamiento y los sigue 9 s, logueando los que
 * se mueven (entidad, tipo, offset). */
#define PR_SLOTS 4
#define PR_CANDS 128
#define PR_NODES 768
typedef struct {
    uint32_t node, off;
    uint16_t qi, chained;
    float first[3], last[3];
} PCand;
typedef struct {
    int on, sample, fixed;
    uint32_t idx, obj;
    int64_t t0;
    uint32_t ncand;
    PCand c[PR_CANDS];
    /* camino de cada nodo del BFS: padre y cómo se llegó (offset del puntero, o 0x8000|i = hijo i del composite) */
    uint16_t par[PR_NODES], how[PR_NODES];
} Probe;
static Probe g_pr[PR_SLOTS];
static uint8_t g_pool_prev[0x80];
static const int k_pr_ms[] = { 100, 300, 600, 1000, 1500, 2000, 3000, 4000, 6000, 9000 };

static int f_ok(float f)
{
    uint32_t u;
    aimp_memcpy(&u, &f, 4);
    return ((u >> 23) & 0xFF) != 0xFF && f > -5000.0f && f < 5000.0f && (f > 0.001f || f < -0.001f);
}

static void probe_scan_node(Probe *p, uint32_t node, const float *ref, int nref, uint16_t qi)
{
    static uint8_t buf[0x800];
    uint32_t n = sizeof buf;
    while (n >= 0x100 && !mem_read(node, buf, n))
        n /= 2;
    if (n < 0x100)
        return;
    for (uint32_t off = 0; off + 12 <= n && p->ncand < PR_CANDS; off += 4) {
        float v[3];
        aimp_memcpy(v, buf + off, 12);
        if (!f_ok(v[0]) || !f_ok(v[1]) || !f_ok(v[2]))
            continue;
        int is_close = 0;
        for (int r = 0; r < nref; r++) {
            float dx = v[0] - ref[3 * r], dy = v[1] - ref[3 * r + 1], dz = v[2] - ref[3 * r + 2];
            is_close |= dx * dx + dy * dy + dz * dz < 64.0f;
        }
        if (!is_close)
            continue;
        PCand *c = &p->c[p->ncand++];
        c->node = node;
        c->off = off;
        c->qi = qi;
        c->chained = 0;
        aimp_memcpy(c->first, v, 12);
        aimp_memcpy(c->last, v, 12);
    }
}

static int probe_ptr(uint32_t v)
{
    return v >= 0x10000 && v < 0x7FFF0000u && !(v & 3) &&
           !(v >= (uint32_t)g_aimp_exe && v < (uint32_t)g_aimp_exe + AIMP_EXPECT_SIZEOFIMAGE);
}

static void probe_start(Probe *p)
{
    float ref[6];
    int nref = 0;
    uint32_t pl = local_player();
    if (pl && mem_read(pl + CHR_POS, ref, 12))
        nref++;
    if (g_have_start) {
        aimp_memcpy(ref + 3 * nref, g_last_start, 12);
        nref++;
    }
    uint32_t w[16];
    if (mem_read(p->obj, w, sizeof w))
        log_printf("THROW sonda: objeto 0x%08x (vt rva 0x%x): %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
                   p->obj, rva(w[0]), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]);
    /* BFS: hijos de los composites y punteros (primeros 0x100 bytes de cada nodo), hasta 3 niveles */
    static uint32_t q[PR_NODES];
    static uint8_t qd[PR_NODES];
    uint32_t nq = 0, head = 0, ncomp = 0;
    q[nq] = p->obj;
    p->par[nq] = 0xFFFF;
    p->how[nq] = 0;
    qd[nq++] = 0;
    while (head < nq) {
        uint32_t o = q[head], vt = 0;
        uint8_t d = qd[head++];
        probe_scan_node(p, o, ref, nref, (uint16_t)(head - 1));
        if (d >= 3)
            continue;
        uint32_t kids[64], nk = 0;
        uint16_t khow[64];
        if (mem_u32(o, &vt) && vt == (uint32_t)g_aimp_exe + AIMP_RVA_VT_COMPOSITE) {
            ncomp++;
            uint32_t arr = 0, cnt = 0, items = 0;
            if (mem_u32(o + COMP_CHILDREN, &arr) && arr && mem_u32(arr + 8, &cnt) && mem_u32(arr + 0xC, &items) && cnt <= 4096)
                for (uint32_t i = 0; i < cnt && nk < 64; i++) {
                    uint32_t hh = 0, ch = 0;
                    if (mem_u32(items + 4 * i, &hh) && hh && mem_u32(hh + HANDLE_OBJ, &ch) && ch) {
                        khow[nk] = (uint16_t)(0x8000 | i);
                        kids[nk++] = ch;
                    }
                }
        }
        uint32_t pw[64];
        if (mem_read(o, pw, sizeof pw))
            for (int i = 1; i < 64 && nk < 64; i++)
                if (probe_ptr(pw[i]) && pw[i] != o) {
                    khow[nk] = (uint16_t)(i * 4);
                    kids[nk++] = pw[i];
                }
        for (uint32_t k = 0; k < nk && nq < PR_NODES; k++) {
            int dup = 0;
            for (uint32_t j = 0; j < nq && !dup; j++)
                dup = q[j] == kids[k];
            if (dup)
                continue;
            uint32_t tmp;
            if (!mem_u32(kids[k], &tmp))
                continue;
            q[nq] = kids[k];
            p->par[nq] = (uint16_t)(head - 1);
            p->how[nq] = khow[k];
            qd[nq++] = (uint8_t)(d + 1);
        }
    }
    log_printf("THROW sonda: entrada %u del pool, objeto 0x%08x: %u nodos (%u composites), %u candidatos cerca de %d referencias",
               p->idx, p->obj, nq, ncomp, p->ncand, nref);
}

static void probe_frame(int64_t now, int64_t freq)
{
    uint32_t pool = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_THROW_POOL);
    if (!pool)
        return;
    uint8_t cur[0x80];
    for (uint32_t i = 0; i < 0x80; i++) {
        uint8_t f = 0;
        mem_u8(pool + 4 + i * 12, &f);
        cur[i] = f;
        if (f && !g_pool_prev[i]) {
            uint32_t hh = 0, obj = 0;
            mem_u32(pool + 4 + i * 12 + 4, &hh);
            if (hh)
                mem_u32(hh + HANDLE_OBJ, &obj);
            for (int s = 0; s < PR_SLOTS; s++) {
                if (g_pr[s].on)
                    continue;
                aimp_memset(&g_pr[s], 0, sizeof g_pr[s]);
                g_pr[s].on = 1;
                g_pr[s].idx = i;
                g_pr[s].obj = obj;
                g_pr[s].t0 = now;
                if (obj)
                    probe_start(&g_pr[s]);
                break;
            }
        }
    }
    aimp_memcpy(g_pool_prev, cur, sizeof cur);
    for (int s = 0; s < PR_SLOTS; s++) {
        Probe *p = &g_pr[s];
        if (!p->on)
            continue;
        int ms = (int)((now - p->t0) * 1000 / freq);
        if (p->sample >= (int)(sizeof k_pr_ms / sizeof k_pr_ms[0]) || !cur[p->idx]) {
            log_printf("THROW sonda: entrada %u terminada a los %d ms (%s)", p->idx, ms, cur[p->idx] ? "fin de la sonda" : "el pool la liberó");
            p->on = 0;
            continue;
        }
        if (ms < k_pr_ms[p->sample])
            continue;
        p->sample++;
        int shown = 0;
        for (uint32_t k = 0; k < p->ncand; k++) {
            PCand *c = &p->c[k];
            float v[3];
            if (!mem_read(c->node + c->off, v, 12))
                continue;
            float dx = v[0] - c->last[0], dy = v[1] - c->last[1], dz = v[2] - c->last[2];
            if (dx * dx + dy * dy + dz * dz < 0.0001f)
                continue;
            aimp_memcpy(c->last, v, 12);
            if (!c->chained) {
                /* camino desde el objeto del pool: [+off] = puntero en off, h<i> = hijo i del composite */
                char buf[160];
                int n = 0;
                uint16_t chain[8], nc = 0;
                for (uint16_t j = c->qi; j != 0xFFFF && nc < 8; j = p->par[j])
                    chain[nc++] = j;
                n += aimp_snprintf(buf + n, sizeof buf - n, "obj");
                for (int j = nc - 2; j >= 0 && n < (int)sizeof buf - 16; j--) {
                    uint16_t hw = p->how[chain[j]];
                    n += hw & 0x8000 ? aimp_snprintf(buf + n, sizeof buf - n, " h%u", hw & 0x7FFF)
                                     : aimp_snprintf(buf + n, sizeof buf - n, " [+0x%x]", hw);
                }
                log_printf("THROW sonda %u camino: %s +0x%x (nodo 0x%08x)", p->idx, buf, c->off, c->node);
                c->chained = 1;
            }
            if (shown++ < 24) {
                uint32_t id = 0, ty = 0, vt = 0;
                mem_u32(c->node, &vt);
                mem_u32(c->node + OUT_ID, &id);
                mem_u32(c->node + OUT_TYPE, &ty);
                log_printf("THROW sonda %u t=%d ms: nodo 0x%08x (vt rva 0x%x, id %08x, tipo %08x) +0x%x = (%.3f, %.3f, %.3f) [al inicio (%.3f, %.3f, %.3f)]",
                           p->idx, ms, c->node, rva(vt), id, ty, c->off, (double)v[0], (double)v[1], (double)v[2], (double)c->first[0],
                           (double)c->first[1], (double)c->first[2]);
            }
        }
        if (shown > 24)
            log_printf("THROW sonda %u t=%d ms: %d candidatos más se movieron", p->idx, ms, shown - 24);
        if (g_fix_test && !p->fixed && ms >= 3000) {
            int nw = 0;
            for (uint32_t k = 0; k < p->ncand; k++) {
                PCand *c = &p->c[k];
                float dx = c->last[0] - c->first[0], dy = c->last[1] - c->first[1], dz = c->last[2] - c->first[2];
                if (dx * dx + dy * dy + dz * dz < 0.0001f)
                    continue;
                float v[3];
                if (!mem_read(c->node + c->off, v, 12))
                    continue;
                v[0] += 1.0f;
                if (mem_write(c->node + c->off, v, 12)) {
                    aimp_memcpy(c->last, v, 12);
                    nw++;
                    log_printf("THROW prueba %u: nodo 0x%08x +0x%x -> (%.3f, %.3f, %.3f)", p->idx, c->node, c->off, (double)v[0],
                               (double)v[1], (double)v[2]);
                }
            }
            p->fixed = 1;
            log_printf("THROW prueba %u t=%d ms: %d campos corridos +1 m en x; las muestras siguientes dicen si se quedan", p->idx, ms, nw);
        }
    }
}

void throw_frame(int64_t now, int64_t freq, uint32_t level)
{
    g_level = level;
    if (!g_cs_ok)
        return;
    pickup_frame();
    if (g_probe)
        probe_frame(now, freq);
    Item loc[ECHO_MAX];
    int nl;
    EnterCriticalSection(&g_cs);
    nl = g_nlocal;
    aimp_memcpy(loc, g_local, sizeof(Item) * (size_t)nl);
    g_nlocal = 0;
    LeaveCriticalSection(&g_cs);
    for (int i = 0; i < nl; i++)
        process_local(&loc[i]);
    if (g_fix)
        track_local_frame();
    LONG lost = InterlockedExchange(&g_n_lost, 0);
    if (lost)
        log_printf("THROW: %ld eventos locales descartados en un frame (cola llena)", (long)lost);
    if (g_echo_ms <= 0)
        return;
    DWORD ms = GetTickCount();
    int w = 0, nd = 0;
    Item due[ECHO_MAX];
    for (int r = 0; r < g_necho; r++) {
        if ((LONG)(ms - g_echo[r].due_ms) >= 0)
            due[nd++] = g_echo[r];
        else
            g_echo[w++] = g_echo[r];
    }
    g_necho = w;
    for (int i = 0; i < nd; i++) {
        ThrowEvt t = due[i].t;
        if (due[i].level != level)
            continue;
        /* al costado de la dirección horizontal (y es la vertical); sin velocidad, en +x */
        float dx = 1.0f, dz = 0.0f, h = sqrt_f(t.vel[0] * t.vel[0] + t.vel[2] * t.vel[2]);
        if (h > 0.001f) {
            dx = -t.vel[2] / h;
            dz = t.vel[0] / h;
        }
        float d = (float)g_echo_cm / 100.0f;
        t.m[3] += dx * d;
        t.m[11] += dz * d;
        apply(&t, "eco");
    }
}
