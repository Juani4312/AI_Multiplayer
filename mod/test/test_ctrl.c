/* Test de las trazas de control (ctrl.c), con wine. Imagen de AI.exe falsa: stub en la RVA real
 * de SwitchCommand (FUN_00678e20) con su prólogo (sub esp,0x100; push esi; mov esi,ecx) y `ret 4`,
 * tabla de nombres de comandos y un CharacterManager con un jugador y dos NPCs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "ctrl.h"
#include "game.h"
#include "log.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

static int g_fail;
#define CHECK(cond, ...)                          \
    do {                                          \
        int ok_ = (cond);                         \
        printf(ok_ ? "  ok    " : "  FALLA ");    \
        printf(__VA_ARGS__);                      \
        printf("\n");                             \
        if (!ok_)                                 \
            g_fail++;                             \
    } while (0)

static void put32(void *base, uint32_t off, uint32_t v) { memcpy((uint8_t *)base + off, &v, 4); }
static void putp(void *base, uint32_t off, void *p) { put32(base, off, (uint32_t)(uintptr_t)p); }

static int g_calls;
static uint32_t g_seen_ctrl, g_seen_cmd;
static void __attribute__((cdecl)) fake_switch(uint32_t ctrl, uint32_t cmd)
{
    g_calls++;
    g_seen_ctrl = ctrl;
    g_seen_cmd = cmd;
}

/* filtro de prueba: bloquea solo g_block_this */
static uint32_t g_block_this, g_cancel_this;
static int g_cancels;
static int block_filter(uint32_t ctrl, uint32_t cmd)
{
    (void)ctrl;
    return cmd != g_block_this;
}
static void __attribute__((fastcall)) fake_cancel(void *self, void *edx)
{
    (void)edx;
    g_cancels++;
    g_cancel_this = (uint32_t)(uintptr_t)self;
}
static void *fake_cancel_stub(void) { return (void *)fake_cancel; }

static void emit(uint8_t **p, const void *b, size_t n)
{
    memcpy(*p, b, n);
    *p += n;
}

static char *read_log(void)
{
    wchar_t path[MAX_PATH];
    if (!GetEnvironmentVariableW(L"AIMP_LOG", path, MAX_PATH))
        return NULL;
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    s[fread(s, 1, (size_t)n, f)] = 0;
    fclose(f);
    return s;
}
static void expect(const char *log, const char *needle, int should)
{
    int has = strstr(log, needle) != NULL;
    printf(has == should ? "  ok    " : "  FALLA ");
    printf("%s \"%s\"\n", should ? "está" : "no está", needle);
    if (has != should)
        g_fail++;
}

typedef void(__attribute__((fastcall)) * SwitchFn)(void *ctrl, void *edx, void *cmd);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    DWORD old;
    uint8_t *p = img + AIMP_RVA_SWITCH_COMMAND;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_SWITCH_COMMAND_PROLOGUE, sizeof AIMP_SWITCH_COMMAND_PROLOGUE); /* sub esp,0x100; push esi; mov esi,ecx */
    emit(&p, "\xFF\xB4\x24\x08\x01\x00\x00", 7);                                /* push [esp+0x108] (cmd) */
    emit(&p, "\x56", 1);                                                        /* push esi (ctrl) */
    uint32_t fn = (uint32_t)(uintptr_t)fake_switch;
    emit(&p, "\xB8", 1);
    emit(&p, &fn, 4);
    emit(&p, "\xFF\xD0\x83\xC4\x08", 5);                   /* call eax; add esp,8 */
    emit(&p, "\x5E\x81\xC4\x00\x01\x00\x00\xC2\x04\x00", 10); /* pop esi; add esp,0x100; ret 4 */

    static const char *names[COMMAND_TYPES] = { "NONE", "IDLE", "DIE", "GOTO", "GOTO_COVER", "MOVE_TOWARDS", "FOLLOW" };
    for (int i = 0; i < COMMAND_TYPES; i++)
        putp(img, AIMP_RVA_COMMAND_NAMES + 4 * i, (void *)(names[i] ? names[i] : "X"));
    /* personajes en regiones propias (VirtualAlloc reserva de a 64 KB): el dueño no queda ambiguo */
#define CHR_ALLOC() (uint8_t *)VirtualAlloc(NULL, 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
    uint8_t *root = calloc(1, 0x100), *cm = calloc(1, 0x200), *pl = CHR_ALLOC(), *n0 = CHR_ALLOC(), *n1 = CHR_ALLOC(),
            *pctrl = calloc(1, CTRL_SIZE), *act = calloc(1, 0x40), *blk = calloc(1, 0x40);
    void *parr[1] = { pl }, *narr[2] = { n0, n1 };
    putp(img, AIMP_RVA_G_ROOT, root);
    putp(root, ROOT_CHARMGR, cm);
    putp(cm, CM_PLAYERS, parr);
    put32(cm, CM_NPLAYERS, 1);
    putp(cm, CM_NPCS, narr);
    put32(cm, CM_NNPCS, 2);
    putp(pl, CHR_CTRL, pctrl);
    putp(pl, CHR_ACTION, act);
    putp(act, ACTION_BLOCK_B, blk);
    blk[0] = 6;
    blk[1] = 0x11;

    SetEnvironmentVariableW(L"AIMP_TRACE_CMD", L"1");
    SetEnvironmentVariableW(L"AIMP_TRACE_CTRL", L"1");
    SetEnvironmentVariableW(L"AIMP_TRACE_CTRL_MS", L"0");
    CHECK(ctrl_install() == 1, "ctrl_install con el hook de SwitchCommand");

    printf("[SwitchCommand: corre el original y se ubica al dueño]\n");
    uint8_t *ctrl = n1 + 0x1234, *cmd = calloc(1, 0x40);
    putp(ctrl, 0x34, n1); /* el controlador apunta a su personaje en +0x34 */
    ctrl[CMDCTRL_FLAGS] = 0;
    put32(cmd, CMD_TYPE, 5);
    uint32_t esp0, esp1;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    ((SwitchFn)(void *)(img + AIMP_RVA_SWITCH_COMMAND))(ctrl, NULL, cmd);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_calls == 1 && g_seen_ctrl == (uint32_t)(uintptr_t)ctrl && g_seen_cmd == (uint32_t)(uintptr_t)cmd,
          "el original recibió controlador y comando, stack balanceado");
    for (int i = 0; i < 5; i++)
        ((SwitchFn)(void *)(img + AIMP_RVA_SWITCH_COMMAND))(ctrl, NULL, cmd);
    CHECK(g_calls == 6, "seis llamadas pasan todas por el original");

    printf("[filtro: un cambio bloqueado no llega al original y el comando se cancela (vt+0x24)]\n");
    static uint8_t *s_blocked;
    ctrl_set_switch_filter(block_filter);
    uint8_t *bcmd = calloc(1, 0x40), *bvt = calloc(1, 0x40);
    putp(bcmd, 0, bvt);
    putp(bvt, CMD_VT_CANCEL, (void *)fake_cancel_stub());
    s_blocked = bcmd;
    g_block_this = (uint32_t)(uintptr_t)bcmd;
    int calls0 = g_calls;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    ((SwitchFn)(void *)(img + AIMP_RVA_SWITCH_COMMAND))(ctrl, NULL, bcmd);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1 && g_calls == calls0 && g_cancels == 1 && g_cancel_this == (uint32_t)(uintptr_t)bcmd,
          "bloqueado: original %d llamadas nuevas, cancelado %d vez con this correcto, stack balanceado", g_calls - calls0, g_cancels);
    ((SwitchFn)(void *)(img + AIMP_RVA_SWITCH_COMMAND))(ctrl, NULL, cmd);
    CHECK(g_calls == calls0 + 1, "otro comando pasa por el original (%d)", g_calls - calls0);
    ctrl_set_switch_filter(NULL);
    (void)s_blocked;

    printf("[controlador del jugador: se loguea solo cuando cambia]\n");
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    ctrl_frame(1000, f.QuadPart);
    ctrl_frame(2000, f.QuadPart); /* igual: no loguea */
    pctrl[8] = 0x7f;
    ctrl_frame(3000, f.QuadPart);

    log_close();
    char *log = read_log();
    if (!log) {
        printf("FALLA: no se pudo leer el log\n");
        return 1;
    }
    expect(log, "hook SwitchCommand (FUN_00678e20): instalado", 1);
    expect(log, "[SCRIPT] comando", 1);
    expect(log, "tipo MOVE_TOWARDS", 1);
    expect(log, "dueño: N1+0x1234 [+0x34]=N1", 1);
    expect(log, "CMD SwitchCommand 4:", 0); /* como mucho 3 por controlador */
    expect(log, "CTRL jugador 0 = ", 1);
    expect(log, "CTRL   acción id 0, bloque A 0 bytes, bloque B 6 bytes: 06110000 0000", 1);
    expect(log, "CTRL 00000000 00000000 7f000000", 1);
    int lines = 0;
    for (const char *q = log; (q = strstr(q, "CTRL 0")) != NULL; q++)
        lines++;
    CHECK(lines == 2, "dos volcados del controlador (el primero y el cambio), no tres (%d)", lines);
    free(log);
    printf(g_fail ? "RESULTADO: %d FALLAS\n" : "RESULTADO: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
