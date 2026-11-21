/* Lector de memoria por archivo, para investigar en runtime sin recompilar (2026-11-17).
 *
 * AIMP_MEMSRV_FILE=<ruta>: cada 200 ms, si el archivo cambió, el mod ejecuta sus líneas y escribe las respuestas en
 * <ruta>.out (primero en <ruta>.tmp y después lo renombra, así el que lee nunca ve un archivo a medias). Solo lee.
 *   B                 base del EXE en runtime ("B 0x00400000")
 *   C                 personajes: "P <i> 0x<chr>" (jugadores) y "N <i> 0x<chr>" (NPCs)
 *   R <hex> <len>     "R 0x<addr> <len> <hex...>" o "... ERR" (len hasta 0x10000, en hex o decimal)
 *   H <hex>           nombre de un hash en la tabla de textos del juego ("H 0x<hash> <texto>" o "?")
 *   LOADF <hex> <ruta> (solo con AIMP_MEMSRV_TEST=1) copia el archivo a esa dirección si el rango es escribible
 *   LOADN <hex> <largo> <ruta>  ídem, solo los primeros <largo> bytes
 *   TP <x> <y> <z> <yaw>  (solo con AIMP_MEMSRV_TEST=1) teletransporta al jugador 0 (FUN_007c6330)
 *   W <hex> <hexbytes> (solo con AIMP_MEMSRV_TEST=1) escribe hasta 256 bytes en memoria escribible
 *   SAVE <a1> <a2> <a3> <a4>  (solo con AIMP_MEMSRV_TEST=1) llama al guardado del juego (FUN_007a90b0)
 *   PUSH              (solo con AIMP_MEMSRV_TEST=1) cliente: arma el envío del próximo guardado al host (savesync inverso)
 *   V <rva>           objetos con esa vtable: direcciones (alineadas a 4) que contienen exe+rva ("V 0x<addr>", "V fin <n>")
 *   F <hexbytes>      dónde está esa secuencia de bytes (4 a 64) en la memoria escribible ("F 0x<addr>", "F fin <n>")
 *   MSG <texto>       (solo con AIMP_MEMSRV_TEST=1) aviso en pantalla con el diálogo "Warning" del juego
 *   TIP <ms> <texto>  (solo con AIMP_MEMSRV_TEST=1) aviso sin pausa con el cartel de consejos del HUD
 *   CALL <rva> <ecx> <n> [args]  (solo con AIMP_MEMSRV_TEST=1) thiscall de prueba con n (0..4) argumentos
 *   ACT <hex>         (solo con AIMP_MEMSRV_TEST=1) el personaje vuelve a su acción por defecto (FUN_0065bdd0)
 *   WD <hex>          (solo con AIMP_MEMSRV_TEST=1) el Alien se retira (como NPC_WithdrawAlien)
 *   LOCO              (solo con AIMP_MEMSRV_TEST=1) el jugador 0 vuelve a la locomoción (FUN_00658080; sale del remate)
 *   KILL <hex>        (solo con AIMP_MEMSRV_TEST=1) mata al personaje como lo hace el juego: FUN_007d0c10 thiscall(chr,
 *                     atacante = 0), ret 4; daño igual a su salud (Character+0x33C). La llama ACT_PLAYER_DEATH (FUN_00668120).
 * La primera línea de la respuesta es "# <seq>": el número de pedido (sube en cada lectura del archivo).
 * Desde Linux: work/scripts/memsrv.py. */
#include "memsrv.h"
#include "anim.h"
#include "game.h"
#include "log.h"
#include "gamesave.h"
#include "savesync.h"
#include "death.h"
#include "ui.h"

#define IN_MAX  (64 * 1024)
#define OUT_MAX (4 * 1024 * 1024)
#define R_MAX   0x10000

static wchar_t g_in[MAX_PATH], g_out[MAX_PATH], g_tmp[MAX_PATH];
static FILETIME g_time;
static int64_t g_last;
static char *g_buf, *g_ob;
static uint8_t *g_rb;
static uint32_t g_seq;
static int g_test;
typedef void(__attribute__((fastcall)) * KillFn)(void *chr, void *edx, uint32_t attacker);
static const uint8_t KILL_PROLOGUE[11] = { 0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x64, 0x80, 0x79, 0x1F, 0x00 };
#define AIMP_RVA_CHR_KILL 0x003D0C10u
static size_t g_on;

static void put(const char *s)
{
    while (*s && g_on < OUT_MAX - 1)
        g_ob[g_on++] = *s++;
}

static void puthex(const uint8_t *p, uint32_t n)
{
    static const char hx[] = "0123456789abcdef";
    for (uint32_t i = 0; i < n && g_on < OUT_MAX - 3; i++) {
        g_ob[g_on++] = hx[p[i] >> 4];
        g_ob[g_on++] = hx[p[i] & 15];
    }
}

static uint32_t num(const char **pp)
{
    const char *p = *pp;
    while (*p == ' ' || *p == '\t')
        p++;
    uint32_t v = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        for (;; p++) {
            int d = *p >= '0' && *p <= '9' ? *p - '0' : (*p | 32) >= 'a' && (*p | 32) <= 'f' ? (*p | 32) - 'a' + 10 : -1;
            if (d < 0)
                break;
            v = v * 16 + (uint32_t)d;
        }
    } else
        for (; *p >= '0' && *p <= '9'; p++)
            v = v * 10 + (uint32_t)(*p - '0');
    *pp = p;
    return v;
}

static void chars(void)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0, c = 0;
    char line[64];
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm) {
        put("C sin CharacterManager\n");
        return;
    }
    if (mem_u32(cm + CM_NPLAYERS, &n) && n < 8 && mem_u32(cm + CM_PLAYERS, &arr) && arr)
        for (uint32_t i = 0; i < n; i++)
            if (mem_u32(arr + 4 * i, &c)) {
                aimp_snprintf(line, sizeof line, "P %u 0x%08x\n", i, c);
                put(line);
            }
    if (mem_u32(cm + CM_NNPCS, &n) && n < 256 && mem_u32(cm + CM_NPCS, &arr) && arr)
        for (uint32_t i = 0; i < n; i++)
            if (mem_u32(arr + 4 * i, &c)) {
                aimp_snprintf(line, sizeof line, "N %u 0x%08x\n", i, c);
                put(line);
            }
}

/* Jugador 0 del CharacterManager (o 0). */
static uint32_t player0(void)
{
    uint32_t pl = 0, root = 0, cm = 0, arr = 0, n = 0;
    if (mem_u32(g_aimp_exe + AIMP_RVA_G_ROOT, &root) && root && mem_u32(root + ROOT_CHARMGR, &cm) && cm &&
        mem_u32(cm + CM_NPLAYERS, &n) && n && mem_u32(cm + CM_PLAYERS, &arr) && arr)
        mem_u32(arr, &pl);
    return pl;
}

/* ¿Está en las listas del CharacterManager? (KILL solo sobre personajes vivos de esta partida) */
static int is_char(uint32_t chr)
{
    uint32_t root = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_G_ROOT), cm = 0, arr = 0, n = 0, c = 0;
    if (!root || !mem_u32(root + ROOT_CHARMGR, &cm) || !cm)
        return 0;
    if (mem_u32(cm + CM_NPLAYERS, &n) && n < 8 && mem_u32(cm + CM_PLAYERS, &arr) && arr)
        for (uint32_t i = 0; i < n; i++)
            if (mem_u32(arr + 4 * i, &c) && c == chr)
                return 1;
    if (mem_u32(cm + CM_NNPCS, &n) && n < 256 && mem_u32(cm + CM_NPCS, &arr) && arr)
        for (uint32_t i = 0; i < n; i++)
            if (mem_u32(arr + 4 * i, &c) && c == chr)
                return 1;
    return 0;
}

/* número con decimales y signo ("-91.17") */
static float numf(const char **pp)
{
    const char *p = *pp;
    while (*p == ' ' || *p == '\t')
        p++;
    int neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    float v = 0.0f, sc = 1.0f;
    for (; *p >= '0' && *p <= '9'; p++)
        v = v * 10.0f + (float)(*p - '0');
    if (*p == '.')
        for (p++; *p >= '0' && *p <= '9'; p++) {
            sc *= 0.1f;
            v += (float)(*p - '0') * sc;
        }
    *pp = p;
    return neg ? -v : v;
}

typedef void(__attribute__((fastcall)) * TpFn)(void *chr, void *edx, const float *pos, float yaw, uint32_t p3);

/* V/F (2026-11-17): búsqueda en la memoria escribible del proceso (páginas comprometidas, sin guarda), alineada a 4.
 * Lectura directa: VirtualQuery dice que la página es legible. Hasta 32 resultados. */
static int hexbyte(const char **q, uint8_t *out)
{
    int v = 0;
    for (int k = 0; k < 2; k++) {
        char c = **q;
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0)
            return 0;
        v = v * 16 + d;
        (*q)++;
    }
    *out = (uint8_t)v;
    return 1;
}

static void scan(const uint8_t *pat, uint32_t n, char tag)
{
    char line[96];
    uint32_t hits = 0;
    MEMORY_BASIC_INFORMATION mi;
    for (uintptr_t a = 0x10000; a < 0x7FFF0000u && hits < 32;) {
        if (!VirtualQuery((void *)a, &mi, sizeof mi))
            break;
        uintptr_t end = (uintptr_t)mi.BaseAddress + mi.RegionSize;
        DWORD pr = mi.Protect & 0xFF;
        if (mi.State == MEM_COMMIT && !(mi.Protect & PAGE_GUARD) &&
            (pr == PAGE_READWRITE || pr == PAGE_EXECUTE_READWRITE || pr == PAGE_WRITECOPY)) {
            const uint8_t *b = (const uint8_t *)mi.BaseAddress;
            for (size_t i = 0; i + n <= mi.RegionSize && hits < 32; i += 4)
                if (b[i] == pat[0] && aimp_memcmp(b + i, pat, n) == 0) {
                    aimp_snprintf(line, sizeof line, "%c 0x%08x\n", tag, (unsigned)(uintptr_t)(b + i));
                    put(line);
                    hits++;
                }
        }
        if (end <= a)
            break;
        a = end;
    }
    aimp_snprintf(line, sizeof line, "%c fin %u\n", tag, hits);
    put(line);
}

static void run(char *txt)
{
    char line[96];
    g_on = 0;
    aimp_snprintf(line, sizeof line, "# %u\n", ++g_seq);
    put(line);
    for (char *p = txt; p && *p;) {
        char *e = p;
        while (*e && *e != '\n' && *e != '\r')
            e++;
        char c0 = *e;
        *e = 0;
        const char *q = p + 1;
        if (p[0] == 'B') {
            aimp_snprintf(line, sizeof line, "B 0x%08x\n", (unsigned)g_aimp_exe);
            put(line);
        } else if (p[0] == 'C' && (p[1] == 0 || p[1] == ' ' || p[1] == '\r'))
            chars();
        else if (p[0] == 'R') {
            uint32_t a = num(&q), n = num(&q);
            if (n > R_MAX)
                n = R_MAX;
            aimp_snprintf(line, sizeof line, "R 0x%08x %u ", a, n);
            put(line);
            if (n && mem_read(a, g_rb, n))
                puthex(g_rb, n);
            else
                put("ERR");
            put("\n");
        } else if (p[0] == 'K' && p[1] == 'I' && p[2] == 'L' && p[3] == 'L') {
            q = p + 4;
            uint32_t chr = num(&q);
            uint8_t pr[sizeof KILL_PROLOGUE];
            uintptr_t f = g_aimp_exe + AIMP_RVA_CHR_KILL;
            uint16_t hp = 0;
            int ok = g_test && chr && is_char(chr) &&
                     ((mem_read(f, pr, sizeof pr) && aimp_code_cmp(pr, KILL_PROLOGUE, sizeof pr) == 0) || death_kill_entry() == f) &&
                     mem_read(chr + 0x33C, &hp, 2);
            aimp_snprintf(line, sizeof line, "KILL 0x%08x salud %u %s\n", chr, hp, ok ? "ok" : "NO (AIMP_MEMSRV_TEST o prólogo)");
            put(line);
            if (ok) {
                log_printf("MEMSRV: KILL 0x%08x (salud %u)", chr, hp);
                ((KillFn)(void *)f)((void *)chr, NULL, 0);
            }
        } else if (p[0] == 'T' && p[1] == 'I' && p[2] == 'P' && p[3] == ' ') {
            /* TIP <ms> <texto>: (solo AIMP_MEMSRV_TEST=1) aviso sin pausa con el cartel de consejos (ui_toast) */
            const char *q = p + 4;
            int ms = 0;
            while (*q >= '0' && *q <= '9')
                ms = ms * 10 + (*q++ - '0');
            while (*q == ' ')
                q++;
            if (g_test)
                ui_toast(q, ms);
            put(g_test ? "TIP ok\n" : "TIP NO\n");
        } else if (p[0] == 'M' && p[1] == 'S' && p[2] == 'G' && p[3] == ' ') {
            /* MSG <texto>: (solo AIMP_MEMSRV_TEST=1) aviso en pantalla con el diálogo del juego (ui_message) */
            if (g_test)
                ui_message(p + 4);
            put(g_test ? "MSG ok\n" : "MSG NO\n");
        } else if (p[0] == 'C' && p[1] == 'A' && p[2] == 'L' && p[3] == 'L' && p[4] == ' ') {
            /* CALL <rva> <ecx> <n> [a1..a4]: (solo AIMP_MEMSRV_TEST=1) thiscall a AI.exe+rva con ecx y n argumentos de 32
             * bits (n = 0..4; el llamado limpia n*4 bytes: n tiene que ser el de su ret). Para experimentos en la partida
             * de prueba; un n o una dirección equivocados hacen caer el juego. */
            q = p + 5;
            uint32_t rva = num(&q), ecx = num(&q), n = num(&q), a[4] = { 0 };
            for (uint32_t i = 0; i < n && i < 4; i++)
                a[i] = num(&q);
            uint32_t r = 0;
            int ok = g_test && rva > 0x1000 && rva < 0x00D9E13Bu && n <= 4;
            if (ok) {
                uintptr_t f = g_aimp_exe + rva;
                void *c = (void *)(uintptr_t)ecx;
                log_printf("MEMSRV: CALL rva 0x%x ecx 0x%08x n %u (%x %x %x %x)", rva, ecx, n, a[0], a[1], a[2], a[3]);
                switch (n) {
                case 0: r = ((uint32_t(__attribute__((fastcall)) *)(void *, void *))f)(c, NULL); break;
                case 1: r = ((uint32_t(__attribute__((fastcall)) *)(void *, void *, uint32_t))f)(c, NULL, a[0]); break;
                case 2: r = ((uint32_t(__attribute__((fastcall)) *)(void *, void *, uint32_t, uint32_t))f)(c, NULL, a[0], a[1]); break;
                case 3: r = ((uint32_t(__attribute__((fastcall)) *)(void *, void *, uint32_t, uint32_t, uint32_t))f)(c, NULL, a[0], a[1], a[2]); break;
                default: r = ((uint32_t(__attribute__((fastcall)) *)(void *, void *, uint32_t, uint32_t, uint32_t, uint32_t))f)(c, NULL, a[0], a[1], a[2], a[3]); break;
                }
            }
            aimp_snprintf(line, sizeof line, "CALL %s eax 0x%08x\n", ok ? "ok" : "NO", r);
            put(line);
        } else if (p[0] == 'A' && p[1] == 'C' && p[2] == 'T' && p[3] == ' ') {
            /* ACT <hex>: (solo AIMP_MEMSRV_TEST=1) el personaje vuelve a su acción por defecto (FUN_0065bdd0) */
            q = p + 4;
            uint32_t chr = num(&q);
            int ok = g_test && chr && is_char(chr) && death_char_default_action(chr, "memsrv ACT");
            put(ok ? "ACT ok\n" : "ACT NO\n");
        } else if (p[0] == 'W' && p[1] == 'D' && p[2] == ' ') {
            /* WD <hex>: (solo AIMP_MEMSRV_TEST=1) el Alien se retira (death_alien_withdraw) */
            q = p + 3;
            uint32_t chr = num(&q);
            int ok = g_test && chr && is_char(chr) && death_alien_withdraw(chr, "memsrv WD");
            put(ok ? "WD ok\n" : "WD NO\n");
        } else if (p[0] == 'L' && p[1] == 'O' && p[2] == 'C' && p[3] == 'O') {
            /* LOCO: (solo AIMP_MEMSRV_TEST=1) el jugador 0 vuelve a la locomoción (sale de la acción de muerte) */
            uint32_t pl = player0();
            int ok = g_test && pl && death_player_to_loco(pl, "memsrv LOCO");
            put(ok ? "LOCO ok\n" : "LOCO NO\n");
        } else if (p[0] == 'L' && p[1] == 'O' && p[2] == 'A' && p[3] == 'D' && (p[4] == 'F' || p[4] == 'N')) {
            /* LOADF <hex> <ruta> / LOADN <hex> <largo> <ruta>: (solo AIMP_MEMSRV_TEST=1) copia el archivo (o sus primeros
             * <largo> bytes) a esa dirección, si todo el rango es memoria escribible. Experimento de la copia en memoria
             * del save (corridas 110-112). */
            int limited = p[4] == 'N';
            q = p + 5;
            uint32_t a = num(&q);
            uint32_t lim = limited ? num(&q) : 0;
            while (*q == ' ')
                q++;
            wchar_t wp[MAX_PATH];
            int k = 0;
            for (; q[k] && k < MAX_PATH - 1; k++)
                wp[k] = (wchar_t)(uint8_t)q[k];
            wp[k] = 0;
            int ok = 0;
            DWORD got = 0, sz = 0;
            HANDLE fh = g_test ? CreateFileW(wp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL) : INVALID_HANDLE_VALUE;
            if (fh != INVALID_HANDLE_VALUE) {
                sz = GetFileSize(fh, NULL);
                if (limited && lim < sz)
                    sz = lim;
                MEMORY_BASIC_INFORMATION mi;
                int writable = sz > 0 && sz < 64u * 1024 * 1024;
                for (uintptr_t x = a; writable && x < a + sz;) {
                    if (!VirtualQuery((void *)x, &mi, sizeof mi) || mi.State != MEM_COMMIT || (mi.Protect & PAGE_GUARD) ||
                        ((mi.Protect & 0xFF) != PAGE_READWRITE && (mi.Protect & 0xFF) != PAGE_EXECUTE_READWRITE))
                        writable = 0;
                    else
                        x = (uintptr_t)mi.BaseAddress + mi.RegionSize;
                }
                if (writable && ReadFile(fh, (void *)(uintptr_t)a, sz, &got, NULL) && got == sz)
                    ok = 1;
                CloseHandle(fh);
            }
            aimp_snprintf(line, sizeof line, "LOADF 0x%08x %u %s\n", a, (unsigned)got, ok ? "ok" : "NO");
            put(line);
            if (ok)
                log_printf("MEMSRV: LOADF 0x%08x: %u bytes", a, (unsigned)got);
        } else if (p[0] == 'T' && p[1] == 'P' && p[2] == ' ') {
            /* TP <x> <y> <z> <yaw>: (solo AIMP_MEMSRV_TEST=1) teletransporta al jugador 0 con FUN_007c6330 (la del
             * avatar y de revivir). Para ir a donde el otro usó algo (prueba de a dos, 2026-11-17). */
            q = p + 3;
            static float pos[4] __attribute__((aligned(16)));
            pos[0] = numf(&q);
            pos[1] = numf(&q);
            pos[2] = numf(&q);
            float yaw = numf(&q);
            uint32_t pl = 0, root = 0, cm = 0, arr = 0, n = 0;
            if (mem_u32(g_aimp_exe + AIMP_RVA_G_ROOT, &root) && root && mem_u32(root + ROOT_CHARMGR, &cm) && cm &&
                mem_u32(cm + CM_NPLAYERS, &n) && n && mem_u32(cm + CM_PLAYERS, &arr) && arr)
                mem_u32(arr, &pl);
            uint8_t pr[sizeof AIMP_CHR_TELEPORT_PROLOGUE];
            uintptr_t f = g_aimp_exe + AIMP_RVA_CHR_TELEPORT;
            int ok = g_test && pl && mem_read(f, pr, sizeof pr) && aimp_code_cmp(pr, AIMP_CHR_TELEPORT_PROLOGUE, sizeof pr) == 0 &&
                     mem_f32(pl + CHR_POS + 12, &pos[3]);
            aimp_snprintf(line, sizeof line, "TP 0x%08x (%.2f, %.2f, %.2f) yaw %.3f %s\n", pl, (double)pos[0], (double)pos[1],
                          (double)pos[2], (double)yaw, ok ? "ok" : "NO (AIMP_MEMSRV_TEST, jugador o prólogo)");
            put(line);
            if (ok) {
                log_printf("MEMSRV: TP del jugador 0x%08x a (%.2f, %.2f, %.2f) yaw %.3f", pl, (double)pos[0], (double)pos[1], (double)pos[2],
                           (double)yaw);
                ((TpFn)(void *)f)((void *)pl, NULL, pos, yaw, 0);
            }
        } else if (p[0] == 'W' && p[1] == ' ') {
            /* W <hex> <hexbytes>: (solo AIMP_MEMSRV_TEST=1) escribe hasta 256 bytes en memoria escribible. Para probar
             * valores de la dificultad (DESIGN_DIFFICULTY). */
            uint32_t a = num(&q);
            while (*q == ' ')
                q++;
            uint8_t buf[256];
            uint32_t n = 0;
            while (n < sizeof buf && hexbyte(&q, &buf[n]))
                n++;
            MEMORY_BASIC_INFORMATION mi;
            int ok = g_test && n && VirtualQuery((void *)(uintptr_t)a, &mi, sizeof mi) && mi.State == MEM_COMMIT &&
                     !(mi.Protect & PAGE_GUARD) && ((mi.Protect & 0xFF) == PAGE_READWRITE || (mi.Protect & 0xFF) == PAGE_EXECUTE_READWRITE ||
                                                                (mi.Protect & 0xFF) == PAGE_WRITECOPY || (mi.Protect & 0xFF) == PAGE_EXECUTE_WRITECOPY) &&
                     (uintptr_t)a + n <= (uintptr_t)mi.BaseAddress + mi.RegionSize && mem_write(a, buf, n);
            aimp_snprintf(line, sizeof line, "W 0x%08x %u %s\n", a, n, ok ? "ok" : "NO");
            put(line);
            if (ok)
                log_printf("MEMSRV: W 0x%08x (%u bytes)", a, n);
        } else if (p[0] == 'P' && p[1] == 'U' && p[2] == 'S' && p[3] == 'H') {
            /* PUSH: (solo AIMP_MEMSRV_TEST=1) como la terminal de guardado del cliente con el host caído (savesync) */
            if (g_test)
                savesync_client_saving();
            put(g_test ? "PUSH ok\n" : "PUSH NO (AIMP_MEMSRV_TEST)\n");
        } else if (p[0] == 'S' && p[1] == 'A' && p[2] == 'V' && p[3] == 'E' && p[4] == ' ') {
            /* SAVE <a1> <a2> <a3> <a4>: (solo AIMP_MEMSRV_TEST=1) llama al guardado del juego (gamesave.c) */
            q = p + 5;
            uint32_t a1 = num(&q), a2 = num(&q), a3 = num(&q), a4 = num(&q);
            int ok = g_test && gamesave_now(a1, a2, a3, a4);
            aimp_snprintf(line, sizeof line, "SAVE %u %u %u %u %s\n", a1, a2, a3, a4, ok ? "ok" : "NO");
            put(line);
        } else if (p[0] == 'V' && p[1] == ' ') {
            uint32_t v = (uint32_t)g_aimp_exe + num(&q);
            scan((const uint8_t *)&v, 4, 'V');
        } else if (p[0] == 'F' && p[1] == ' ') {
            uint8_t pat[64];
            uint32_t n = 0;
            q = p + 2;
            while (n < sizeof pat && hexbyte(&q, &pat[n]))
                n++;
            if (n >= 4)
                scan(pat, n, 'F');
            else
                put("F ERR (al menos 4 bytes en hex)\n");
        } else if (p[0] == 'H') {
            uint32_t h = num(&q);
            const char *s = anim_name(h);
            char nm[128];
            if (!s || !mem_cstr((uintptr_t)s, nm, sizeof nm))
                aimp_snprintf(nm, sizeof nm, "?");
            aimp_snprintf(line, sizeof line, "H 0x%08x ", h);
            put(line);
            put(nm);
            put("\n");
        }
        p = c0 ? e + 1 : NULL;
    }
    HANDLE h = CreateFileW(g_tmp, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD wr = 0;
    WriteFile(h, g_ob, (DWORD)g_on, &wr, NULL);
    CloseHandle(h);
    MoveFileExW(g_tmp, g_out, MOVEFILE_REPLACE_EXISTING);
}

void memsrv_install(void)
{
    if (!aimp_env_str(L"AIMP_MEMSRV_FILE", g_in, MAX_PATH - 8) || !g_in[0])
        return;
    int n = 0;
    while (g_in[n])
        n++;
    for (int i = 0; i <= n; i++)
        g_out[i] = g_tmp[i] = g_in[i];
    const wchar_t *so = L".out", *st = L".tmp";
    for (int i = 0; i < 5; i++) {
        g_out[n + i] = so[i];
        g_tmp[n + i] = st[i];
    }
    g_buf = VirtualAlloc(NULL, IN_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_ob = VirtualAlloc(NULL, OUT_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_rb = VirtualAlloc(NULL, R_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_buf || !g_ob || !g_rb) {
        g_in[0] = 0;
        return;
    }
    g_test = aimp_env_int(L"AIMP_MEMSRV_TEST", 0);
    /* corrida 91: lo que quedó de una corrida anterior no se ejecuta (un KILL viejo hizo caer el juego) */
    {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_in, GetFileExInfoStandard, &fa))
            g_time = fa.ftLastWriteTime;
    }
    log_printf("MEMSRV: lector de memoria por archivo activo (%s)", g_test ? "con comandos de prueba: KILL" : "solo lee");
}

void memsrv_frame(int64_t now, int64_t freq)
{
    if (!g_in[0] || now - g_last < freq / 5)
        return;
    g_last = now;
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(g_in, GetFileExInfoStandard, &fa))
        return;
    if (fa.ftLastWriteTime.dwLowDateTime == g_time.dwLowDateTime && fa.ftLastWriteTime.dwHighDateTime == g_time.dwHighDateTime)
        return;
    g_time = fa.ftLastWriteTime;
    HANDLE h = CreateFileW(g_in, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD got = 0;
    ReadFile(h, g_buf, IN_MAX - 1, &got, NULL);
    CloseHandle(h);
    g_buf[got] = 0;
    run(g_buf);
}
