/* Guardar la partida desde el mod (ver gamesave.h). */
#include "gamesave.h"
#include "game.h"
#include "hook.h"
#include "log.h"

#define AIMP_RVA_GAME_SAVE 0x003A90B0u /* FUN_007a90b0 */
#define ROOT_GAME_MGR 0x78
static const uint8_t GAME_SAVE_PROLOGUE[13] = { 0x6A, 0xFF, 0x68, 0x98, 0xE4, 0x12, 0x01, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };

typedef void(__attribute__((fastcall)) * SaveFn)(void *mgr, void *edx, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);
static SaveFn g_orig, g_direct;
static AimpHook g_hk;
static volatile LONG g_n;

__attribute__((fastcall, force_align_arg_pointer, noinline)) static void det_save(void *mgr, void *edx, uint32_t a1, uint32_t a2,
                                                                                    uint32_t a3, uint32_t a4)
{
    LONG n = InterlockedIncrement(&g_n);
    if (n <= 100)
        log_printf("SAVE: el juego guarda (#%ld): mgr %p, args %u %u %u %u, desde rva 0x%x", (long)n, mgr, a1, a2, a3, a4,
                   (unsigned)((uintptr_t)__builtin_return_address(0) - g_aimp_exe));
    g_orig(mgr, edx, a1, a2, a3, a4);
}

int gamesave_now(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    SaveFn f = g_orig ? g_orig : g_direct;
    uint32_t root = 0, mgr = 0;
    if (!f || !mem_u32(g_aimp_exe + AIMP_RVA_G_ROOT, &root) || !root || !mem_u32(root + ROOT_GAME_MGR, &mgr) || !mgr)
        return 0;
    log_printf("SAVE: el mod pide un guardado: mgr 0x%08x, args %u %u %u %u", mgr, a1, a2, a3, a4);
    f((void *)(uintptr_t)mgr, NULL, a1, a2, a3, a4);
    return 1;
}

void gamesave_install(void)
{
    uint8_t pr[sizeof GAME_SAVE_PROLOGUE];
    uintptr_t t = g_aimp_exe + AIMP_RVA_GAME_SAVE;
    if (!mem_read(t, pr, sizeof pr) || aimp_code_cmp(pr, GAME_SAVE_PROLOGUE, sizeof pr) != 0) {
        log_printf("SAVE: el prólogo de FUN_007a90b0 no coincide: no se puede guardar desde el mod");
        return;
    }
    g_direct = (SaveFn)(void *)t;
    if (aimp_env_int(L"AIMP_TRACE_SAVE", 0) &&
        hook_install(&g_hk, "guardado del juego (FUN_007a90b0)", (void *)t, GAME_SAVE_PROLOGUE, sizeof GAME_SAVE_PROLOGUE, 7, (void *)det_save,
                     (void **)&g_orig))
        log_printf("SAVE: traza de los guardados activa");
}
