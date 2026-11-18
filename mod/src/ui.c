/* Mensajes del coop en el menú del juego (ver ui.h). */
#include "ui.h"
#include "hook.h"
#include "log.h"
#include "savesync.h"
#include "diff.h"

#define AIMP_RVA_TEXT_LOOKUP 0x00590B60u /* FUN_00990b60 */
static const uint8_t TEXT_LOOKUP_PROLOGUE[11] = { 0x51, 0x8B, 0x54, 0x24, 0x08, 0x8D, 0x44, 0x24, 0x03, 0x50, 0x52 };

typedef const wchar_t *(__attribute__((fastcall)) * LookupFn)(void *db, void *edx, const char *key);
static LookupFn g_orig;
static AimpHook g_hk;
static volatile LONG g_logged;

/* Corrida 121: el título ("Press any key") y la descripción del menú principal se piden una sola vez, al armar el
 * frontend (antes de que haya red): quedarían viejos. Estos dos se piden cada vez que se entra al submenú o se
 * resalta la opción, así que muestran el estado del momento. */
enum { K_CAMPAIGN_DESC, K_CONTINUE, K_EXTRAS, K_EXTRAS_DESC, K_CREDITS, K_CREDITS_DESC, K_N };
static const char *const g_keys[K_N] = { "AI_UI_FRONTEND_CAMPAIGN_DESC", "AI_UI_FRONTEND_CONTINUE_GAME", "AI_UI_FRONTEND_EXTRAS",
                                         "AI_UI_FRONTEND_EXTRAS_DESC",   "AI_UI_FRONTEND_CREDITS",       "AI_UI_FRONTEND_CREDITS_DESC" };

/* ---- menú del mod (DESIGN_CONFIG_MENU, decisión del usuario 2026-11-18: "reemplazar Extras y que abra la config") ----
 * "Extras" pasa a ser "Coop"; su único botón en PC ("Credits") es la dificultad del coop, que cambia al apretarlo
 * (se intercepta show_credits_screen: FUN_0082aa60, cdecl(params*) -> FUN_00823310). Valor en opciones.txt. */
static int g_menu_on;
static int g_coop_diff; /* 0 Normal, 1 Difícil, 2 Pesadilla */
static const wchar_t *const DIFF_NAMES[3] = { L"Normal", L"Difícil", L"Pesadilla" };
static wchar_t g_opt_path[MAX_PATH];

int ui_coop_difficulty(void) { return g_coop_diff; }

static void opt_load(void)
{
    HANDLE h = CreateFileW(g_opt_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char b[128];
    DWORD n = 0;
    ReadFile(h, b, sizeof b - 1, &n, NULL);
    CloseHandle(h);
    b[n] = 0;
    for (char *p = b; *p; p++)
        if (p[0] == '=' && p[1] >= '0' && p[1] <= '2') {
            g_coop_diff = p[1] - '0';
            break;
        }
}

static void opt_save(void)
{
    HANDLE h = CreateFileW(g_opt_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char b[64];
    int n = aimp_snprintf(b, sizeof b, "dificultad_coop=%d\r\n", g_coop_diff);
    DWORD w = 0;
    WriteFile(h, b, (DWORD)n, &w, NULL);
    CloseHandle(h);
}

#define AIMP_RVA_UI_CREDITS 0x0042AA60u /* show_credits_screen */
static const uint8_t UI_CREDITS_PROLOGUE[6] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x08 };
typedef void(__attribute__((cdecl)) * UiActFn)(void *params);
static UiActFn g_credits_orig;
static AimpHook g_hk_credits;

__attribute__((cdecl, force_align_arg_pointer, noinline)) static void det_credits(void *params)
{
    if (!g_menu_on) {
        g_credits_orig(params);
        return;
    }
    g_coop_diff = (g_coop_diff + 1) % 3;
    opt_save();
    diff_set_local(g_coop_diff);
    char n8[32];
    log_printf("UI: menú del mod: dificultad del coop = %s (opciones.txt)", aimp_utf8(n8, sizeof n8, DIFF_NAMES[g_coop_diff]));
}
static wchar_t g_buf[K_N][256]; /* la UI copia el texto; uno por clave para que dos pedidos seguidos no se pisen */

static int streq(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;
    return *a == *b;
}

static const wchar_t *status_text(int st)
{
    switch (st) {
    case SSUI_HOST_WAITING:
        return L"Coop (host): esperando al otro jugador";
    case SSUI_HOST_CONNECTED:
        return L"Coop (host): el otro jugador está conectado";
    case SSUI_CLI_WAITING:
        return L"Coop: esperando al host... no entres todavía";
    case SSUI_CLI_CONNECTED:
        return L"Coop: conectado al host";
    case SSUI_CLI_COPYING:
        return L"Coop: copiando la partida del host... no entres todavía";
    case SSUI_CLI_COPIED:
        return L"Coop: partida del host copiada. Ya podés entrar (Continue Game)";
    case SSUI_CLI_SAME:
        return L"Coop: ya tenés la partida del host. Ya podés entrar (Continue Game)";
    case SSUI_CLI_ERROR:
        return L"Coop: no se pudo copiar la partida del host (ver aimp.log)";
    default:
        return NULL;
    }
}

/* out = a + sep + b (truncado) */
static const wchar_t *join(wchar_t *out, size_t cap, const wchar_t *a, const wchar_t *sep, const wchar_t *b)
{
    out[0] = 0;
    if (a)
        aimp_wcscat(out, cap, a);
    if (a && b && sep)
        aimp_wcscat(out, cap, sep);
    if (b)
        aimp_wcscat(out, cap, b);
    return out;
}

__attribute__((fastcall, force_align_arg_pointer, noinline)) static const wchar_t *det_lookup(void *db, void *edx, const char *key)
{
    const wchar_t *orig = g_orig(db, edx, key);
    if (!key || key[0] != 'A' || key[1] != 'I' || key[2] != '_')
        return orig;
    int k = -1;
    for (int i = 0; i < K_N; i++)
        if (streq(key, g_keys[i])) {
            k = i;
            break;
        }
    if (k < 0)
        return orig;
    if (k >= K_EXTRAS) {
        if (!g_menu_on)
            return orig;
        switch (k) {
        case K_EXTRAS:
            return L"Coop";
        case K_EXTRAS_DESC:
            return L"Opciones del mod coop";
        case K_CREDITS:
            if (diff_from_host())
            {
                static wchar_t tmp[64];
                return join(g_buf[k], 256, L"Dificultad del coop: ", NULL, join(tmp, 64, DIFF_NAMES[diff_effective()], NULL, L" (la eligió el host)"));
            }
            return join(g_buf[k], 256, L"Dificultad del coop: ", NULL, DIFF_NAMES[g_coop_diff]);
        default:
            return L"Apretá para cambiar. Normal: la del juego. Lo decide el host";
        }
    }
    int st = savesync_ui();
    const wchar_t *msg = status_text(st);
    if (!msg)
        return orig;
    if (InterlockedIncrement(&g_logged) <= 30) {
        char m8[300];
        log_printf("UI: texto \"%s\" con el estado del coop (%d): %s", key, st, aimp_utf8(m8, sizeof m8, msg));
    }
    if (k == K_CONTINUE) {
        if (st == SSUI_CLI_WAITING)
            return join(g_buf[k], 256, orig, L" - ", L"esperá al host");
        if (st == SSUI_CLI_COPYING)
            return join(g_buf[k], 256, orig, L" - ", L"esperá, copiando");
        if (st == SSUI_CLI_COPIED || st == SSUI_CLI_SAME)
            return join(g_buf[k], 256, orig, L" - ", L"partida del host");
        if (st == SSUI_CLI_ERROR)
            return join(g_buf[k], 256, orig, L" - ", L"falló la copia del save del host");
        return orig;
    }
    return join(g_buf[k], 256, msg, NULL, NULL);
}

/* ---- traza de las llamadas de la UI al juego (DESIGN_CONFIG_MENU, paso 1) ----
 * Manejadores cdecl(params*): params+8 = arreglo de valores Scaleform de 0x18 bytes; en cada valor, +4 son los flags
 * (bit 6: el valor es un puntero a puntero) y +8 el valor (texto o entero). Registro en 0x8410xx. */
#define AIMP_RVA_UI_GENERIC 0x0040D080u     /* GenericCallback */
#define AIMP_RVA_UI_GENERIC_INT 0x0040D0A0u /* GenericCallbackInt */
#define AIMP_RVA_UI_ACTIVATE 0x0042CD60u    /* ActivateLayer */
static const uint8_t UI_GENERIC_PROLOGUE[7] = { 0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x41, 0x08 };
static const uint8_t UI_GENERIC_INT_PROLOGUE[7] = { 0x8B, 0x54, 0x24, 0x04, 0x8B, 0x42, 0x08 };
static const uint8_t UI_ACTIVATE_PROLOGUE[7] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x40, 0x08 };
typedef void(__attribute__((cdecl)) * UiCbFn)(void *params);
static UiCbFn g_cb_generic, g_cb_int, g_cb_act;
static AimpHook g_hk_generic, g_hk_int, g_hk_act;

static void ui_arg(void *params, int i, char *out, size_t cap, uint32_t *ival)
{
    uint32_t args = 0, fl = 0, v = 0;
    out[0] = 0;
    *ival = 0;
    if (!mem_u32((uintptr_t)params + 8, &args) || !args || !mem_u32(args + 0x18 * (uint32_t)i + 4, &fl) ||
        !mem_u32(args + 0x18 * (uint32_t)i + 8, &v))
        return;
    *ival = v;
    if ((fl >> 6) & 1)
        mem_u32(v, &v);
    if (!mem_cstr(v, out, cap))
        out[0] = 0;
}

static void ui_log(const char *what, void *params)
{
    char a0[96], a1[96];
    uint32_t i0, i1;
    ui_arg(params, 0, a0, sizeof a0, &i0);
    ui_arg(params, 1, a1, sizeof a1, &i1);
    log_printf("UI: %s(\"%s\", \"%s\" / %u)", what, a0, a1, i1);
}

__attribute__((cdecl, force_align_arg_pointer, noinline)) static void det_generic(void *params)
{
    ui_log("GenericCallback", params);
    g_cb_generic(params);
}
__attribute__((cdecl, force_align_arg_pointer, noinline)) static void det_int(void *params)
{
    ui_log("GenericCallbackInt", params);
    g_cb_int(params);
}
__attribute__((cdecl, force_align_arg_pointer, noinline)) static void det_act(void *params)
{
    ui_log("ActivateLayer", params);
    g_cb_act(params);
}

void ui_install(void)
{
    if (aimp_env_int(L"AIMP_TRACE_UI_CALLBACKS", 0)) {
        hook_install(&g_hk_generic, "UI GenericCallback (FUN_0080d080)", (void *)(g_aimp_exe + AIMP_RVA_UI_GENERIC), UI_GENERIC_PROLOGUE,
                     sizeof UI_GENERIC_PROLOGUE, 7, (void *)det_generic, (void **)&g_cb_generic);
        hook_install(&g_hk_int, "UI GenericCallbackInt (FUN_0080d0a0)", (void *)(g_aimp_exe + AIMP_RVA_UI_GENERIC_INT),
                     UI_GENERIC_INT_PROLOGUE, sizeof UI_GENERIC_INT_PROLOGUE, 7, (void *)det_int, (void **)&g_cb_int);
        hook_install(&g_hk_act, "UI ActivateLayer (FUN_0082cd60)", (void *)(g_aimp_exe + AIMP_RVA_UI_ACTIVATE), UI_ACTIVATE_PROLOGUE,
                     sizeof UI_ACTIVATE_PROLOGUE, 7, (void *)det_act, (void **)&g_cb_act);
        log_printf("UI: traza de GenericCallback/GenericCallbackInt/ActivateLayer activa");
    }
    g_menu_on = aimp_env_int(L"AIMP_MOD_MENU", 1);
    {
        wchar_t base[MAX_PATH];
        if (aimp_env_str(L"LOCALAPPDATA", base, MAX_PATH)) {
            aimp_wcscpy(g_opt_path, MAX_PATH, base);
            aimp_wcscat(g_opt_path, MAX_PATH, L"\\AI_Multiplayer\\opciones.txt");
            opt_load();
        }
    }
    diff_set_local(g_coop_diff);
    if (g_menu_on &&
        hook_install(&g_hk_credits, "botón Credits de Extras (FUN_0082aa60)", (void *)(g_aimp_exe + AIMP_RVA_UI_CREDITS), UI_CREDITS_PROLOGUE,
                     sizeof UI_CREDITS_PROLOGUE, 6, (void *)det_credits, (void **)&g_credits_orig)) {
        char n8[32];
        log_printf("UI: menú del mod en Extras (\"Coop\"): dificultad del coop = %s", aimp_utf8(n8, sizeof n8, DIFF_NAMES[g_coop_diff]));
    } else {
        g_menu_on = 0;
    }
    if (!aimp_env_int(L"AIMP_UI_TEXT", 1))
        return;
    if (hook_install(&g_hk, "textos de la UI (FUN_00990b60)", (void *)(g_aimp_exe + AIMP_RVA_TEXT_LOOKUP), TEXT_LOOKUP_PROLOGUE,
                     sizeof TEXT_LOOKUP_PROLOGUE, 5, (void *)det_lookup, (void **)&g_orig))
        log_printf("UI: mensajes del coop en el menú (pantalla de título, descripciones y Continue Game)");
}
