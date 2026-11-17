/* Mensajes del coop en el menú del juego (ver ui.h). */
#include "ui.h"
#include "hook.h"
#include "log.h"
#include "savesync.h"

#define AIMP_RVA_TEXT_LOOKUP 0x00590B60u /* FUN_00990b60 */
static const uint8_t TEXT_LOOKUP_PROLOGUE[11] = { 0x51, 0x8B, 0x54, 0x24, 0x08, 0x8D, 0x44, 0x24, 0x03, 0x50, 0x52 };

typedef const wchar_t *(__attribute__((fastcall)) * LookupFn)(void *db, void *edx, const char *key);
static LookupFn g_orig;
static AimpHook g_hk;
static volatile LONG g_logged;

/* Corrida 121: el título ("Press any key") y la descripción del menú principal se piden una sola vez, al armar el
 * frontend (antes de que haya red): quedarían viejos. Estos dos se piden cada vez que se entra al submenú o se
 * resalta la opción, así que muestran el estado del momento. */
enum { K_CAMPAIGN_DESC, K_CONTINUE, K_N };
static const char *const g_keys[K_N] = { "AI_UI_FRONTEND_CAMPAIGN_DESC", "AI_UI_FRONTEND_CONTINUE_GAME" };
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

void ui_install(void)
{
    if (!aimp_env_int(L"AIMP_UI_TEXT", 1) || !aimp_env_str(L"AIMP_NET", g_buf[0], 8)) {
        g_buf[0][0] = 0;
        return;
    }
    g_buf[0][0] = 0;
    if (hook_install(&g_hk, "textos de la UI (FUN_00990b60)", (void *)(g_aimp_exe + AIMP_RVA_TEXT_LOOKUP), TEXT_LOOKUP_PROLOGUE,
                     sizeof TEXT_LOOKUP_PROLOGUE, 5, (void *)det_lookup, (void **)&g_orig))
        log_printf("UI: mensajes del coop en el menú (pantalla de título, descripciones y Continue Game)");
}
