/* Muerte en coop: investigación del flujo del game over (2026-11-17). Ver death.h. */
#include "death.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define AIMP_RVA_UI_OPEN 0x0041F8F0u
static const uint8_t UI_OPEN_PROLOGUE[7] = { 0x6A, 0xFF, 0x68, 0x48, 0x2E, 0x13, 0x01 };

typedef void(__attribute__((fastcall)) * UiOpenFn)(void *ui, void *edx, const char *name);
static UiOpenFn g_ui_orig;
static AimpHook g_hk_ui;
static volatile LONG g_n_ui;
static uint32_t g_text_lo, g_text_hi;

static void stack_line(const char *what, uint32_t sp)
{
    uint32_t st[192];
    if (!mem_read(sp, st, sizeof st))
        return;
    char line[900];
    int n = aimp_snprintf(line, sizeof line, "   %s: retornos posibles (offset:rva):", what);
    int k = 0;
    for (int i = 0; i < 192 && k < 28 && n > 0 && (size_t)n < sizeof line - 24; i++)
        if (st[i] >= g_text_lo && st[i] < g_text_hi) {
            n += aimp_snprintf(line + n, sizeof line - (size_t)n, " +%x:%x", (unsigned)(i * 4), (unsigned)(st[i] - g_aimp_exe));
            k++;
        }
    log_printf("%s", line);
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_ui_open(void *ui, void *edx, const char *name)
{
    LONG n = InterlockedIncrement(&g_n_ui);
    if (n <= 200) {
        char nm[64];
        if (!name || !mem_cstr((uintptr_t)name, nm, sizeof nm))
            aimp_snprintf(nm, sizeof nm, "?");
        log_printf("UI: abre \"%s\" (ui 0x%08x, #%ld)", nm, (unsigned)(uintptr_t)ui, (long)n);
        stack_line("pila", (uint32_t)(uintptr_t)__builtin_frame_address(0) + 4);
    }
    g_ui_orig(ui, edx, name);
}

/* FUN_007caac0 (daño sobre el componente de salud, Character+0x320): si Character+0x4253 != 0 y el golpe es >= la
 * salud, lo recorta a salud-1 (también cuando el golpe no es "letal", arg 5 en 0). Salud u16 en Character+0x33C,
 * máxima en +0x33E (NOTES, 2026-11-15). */
#define CHR_NO_DEATH   0x4253
#define CHR_HEALTH     0x33C
#define CHR_HEALTH_MAX 0x33E
#define CHR_INVINC_A   0x2E8
#define CHR_INVINC_B   0x2EA

static int g_downed_mode, g_downed;
static uint32_t g_last_player;
static int64_t g_last_chk;
static wchar_t g_revive_file[MAX_PATH];
static FILETIME g_revive_time;

int death_is_downed(void) { return g_downed; }

static void set_flags(uint32_t chr, int downed)
{
    uint8_t a = 0, b = 0;
    mem_u8(chr + CHR_INVINC_A, &a);
    mem_u8(chr + CHR_INVINC_B, &b);
    a = downed ? (uint8_t)(a | 8) : (uint8_t)(a & ~8);
    b = downed ? (uint8_t)(b | 8) : (uint8_t)(b & ~8);
    mem_write(chr + CHR_INVINC_A, &a, 1);
    mem_write(chr + CHR_INVINC_B, &b, 1);
}

void death_frame(int64_t now, int64_t freq, uint32_t player)
{
    if (!g_downed_mode || !player || now - g_last_chk < freq / 10)
        return;
    g_last_chk = now;
    if (player != g_last_player) {
        g_last_player = player;
        g_downed = 0;
    }
    uint8_t nd = 0;
    if (mem_u8(player + CHR_NO_DEATH, &nd) && !nd) {
        uint8_t one = 1;
        mem_write(player + CHR_NO_DEATH, &one, 1);
        log_printf("DEATH: jugador 0x%08x: no puede morir (+0x4253 = 1)", player);
    }
    uint16_t hp = 0, mx = 0;
    if (!mem_read(player + CHR_HEALTH, &hp, 2))
        return;
    mem_read(player + CHR_HEALTH_MAX, &mx, 2);
    if (!g_downed && hp <= 1) {
        g_downed = 1;
        set_flags(player, 1);
        log_printf("DEATH: el jugador quedó caído (salud %u de %u): invencible; en coop sigue el compañero", hp, mx);
    }
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (g_downed && g_revive_file[0] && GetFileAttributesExW(g_revive_file, GetFileExInfoStandard, &fa) &&
        (fa.ftLastWriteTime.dwLowDateTime != g_revive_time.dwLowDateTime ||
         fa.ftLastWriteTime.dwHighDateTime != g_revive_time.dwHighDateTime)) {
        g_revive_time = fa.ftLastWriteTime;
        g_downed = 0;
        set_flags(player, 0);
        if (mx)
            mem_write(player + CHR_HEALTH, &mx, 2);
        log_printf("DEATH: el jugador revive (salud %u)", mx);
    }
}

void death_install(void)
{
    g_downed_mode = aimp_env_int(L"AIMP_COOP_DOWNED", 0);
    if (aimp_env_str(L"AIMP_COOP_REVIVE_FILE", g_revive_file, MAX_PATH)) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(g_revive_file, GetFileExInfoStandard, &fa))
            g_revive_time = fa.ftLastWriteTime;
    }
    if (g_downed_mode)
        log_printf("DEATH: AIMP_COOP_DOWNED=1: el jugador local no muere, queda caído");
    /* .text de AI.exe: RVA 0x1000, 0xD9D13B bytes (objdump -h) */
    g_text_lo = (uint32_t)g_aimp_exe + 0x1000;
    g_text_hi = g_text_lo + 0x00D9D13Bu;
    if (aimp_env_int(L"AIMP_TRACE_UI", 0)) {
        if (hook_install(&g_hk_ui, "abrir pantalla de UI (FUN_0081f8f0)", (void *)(g_aimp_exe + AIMP_RVA_UI_OPEN), UI_OPEN_PROLOGUE,
                         sizeof UI_OPEN_PROLOGUE, 7, (void *)det_ui_open, (void **)&g_ui_orig))
            log_printf("DEATH: traza de pantallas de UI activa");
    }
}
