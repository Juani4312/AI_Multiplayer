/* Lector de memoria por archivo, para investigar en runtime sin recompilar (2026-11-17).
 *
 * AIMP_MEMSRV_FILE=<ruta>: cada 200 ms, si el archivo cambió, el mod ejecuta sus líneas y escribe las respuestas en
 * <ruta>.out (primero en <ruta>.tmp y después lo renombra, así el que lee nunca ve un archivo a medias). Solo lee.
 *   B                 base del EXE en runtime ("B 0x00400000")
 *   C                 personajes: "P <i> 0x<chr>" (jugadores) y "N <i> 0x<chr>" (NPCs)
 *   R <hex> <len>     "R 0x<addr> <len> <hex...>" o "... ERR" (len hasta 0x10000, en hex o decimal)
 *   H <hex>           nombre de un hash en la tabla de textos del juego ("H 0x<hash> <texto>" o "?")
 *   KILL <hex>        (solo con AIMP_MEMSRV_TEST=1) mata al personaje como lo hace el juego: FUN_007d0c10 thiscall(chr,
 *                     atacante = 0), ret 4; daño igual a su salud (Character+0x33C). La llama ACT_PLAYER_DEATH (FUN_00668120).
 * La primera línea de la respuesta es "# <seq>": el número de pedido (sube en cada lectura del archivo).
 * Desde Linux: work/scripts/memsrv.py. */
#include "memsrv.h"
#include "anim.h"
#include "game.h"
#include "log.h"

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
        } else if (p[0] == 'C')
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
            int ok = g_test && chr && is_char(chr) && mem_read(f, pr, sizeof pr) && aimp_memcmp(pr, KILL_PROLOGUE, sizeof pr) == 0 &&
                     mem_read(chr + 0x33C, &hp, 2);
            aimp_snprintf(line, sizeof line, "KILL 0x%08x salud %u %s\n", chr, hp, ok ? "ok" : "NO (AIMP_MEMSRV_TEST o prólogo)");
            put(line);
            if (ok) {
                log_printf("MEMSRV: KILL 0x%08x (salud %u)", chr, hp);
                ((KillFn)(void *)f)((void *)chr, NULL, 0);
            }
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
