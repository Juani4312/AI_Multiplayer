/* Configuración por archivo (aimp.cfg), para no depender de variables de entorno
 * (en Windows, Steam no permite VAR=valor en las opciones de lanzamiento).
 *
 * Se lee, en orden: %LOCALAPPDATA%\AI_Multiplayer\aimp.cfg y aimp.cfg al lado de la
 * DLL. Cada línea "AIMP_X=valor" define la variable de entorno del proceso si no
 * estaba definida (el entorno real tiene prioridad). '#' empieza un comentario.
 */
#include "aimp.h"

static char g_applied[512];
static size_t g_applied_len;

static void note(const char *s, size_t n)
{
    if (g_applied_len + n + 2 >= sizeof g_applied)
        return;
    if (g_applied_len)
        g_applied[g_applied_len++] = ' ';
    aimp_memcpy(g_applied + g_applied_len, s, n);
    g_applied_len += n;
    g_applied[g_applied_len] = 0;
}

static void apply_line(char *line, size_t n)
{
    while (n && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    size_t i = 0;
    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if (i >= n || line[i] == '#')
        return;
    size_t eq = i;
    while (eq < n && line[eq] != '=')
        eq++;
    if (eq >= n || eq == i)
        return;
    /* Solo variables del mod: no tocar el resto del entorno del juego. */
    if (n - i < 5 || aimp_memcmp(line + i, "AIMP_", 5) != 0)
        return;
    size_t kend = eq;
    while (kend > i && (line[kend - 1] == ' ' || line[kend - 1] == '\t'))
        kend--;
    size_t v = eq + 1;
    while (v < n && (line[v] == ' ' || line[v] == '\t'))
        v++;
    wchar_t key[64], val[MAX_PATH], cur[8];
    int kl = MultiByteToWideChar(CP_UTF8, 0, line + i, (int)(kend - i), key, 63);
    int vl = MultiByteToWideChar(CP_UTF8, 0, line + v, (int)(n - v), val, MAX_PATH - 1);
    if (kl <= 0 || vl < 0)
        return;
    key[kl] = 0;
    val[vl] = 0;
    if (GetEnvironmentVariableW(key, cur, 8) != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND)
        return; /* ya definida en el entorno: gana el entorno */
    if (SetEnvironmentVariableW(key, val))
        note(line + i, n - i);
}

static int load_file(const wchar_t *path)
{
    char buf[8192];
    DWORD got = 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    ReadFile(h, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    size_t start = 0;
    for (size_t k = 0; k <= got; k++) {
        if (k == got || buf[k] == '\n') {
            apply_line(buf + start, k - start);
            start = k + 1;
        }
    }
    return 1;
}

/* Devuelve cuántos archivos se leyeron; aplicadas() da el detalle para el log. */
int cfg_load(HMODULE self)
{
    wchar_t path[MAX_PATH + 32];
    int n = 0;
    if (aimp_env_str(L"LOCALAPPDATA", path, MAX_PATH)) {
        aimp_wcscat(path, MAX_PATH + 32, L"\\AI_Multiplayer\\aimp.cfg");
        n += load_file(path);
    }
    DWORD len = GetModuleFileNameW(self, path, MAX_PATH);
    if (len && len < MAX_PATH) {
        while (len && path[len - 1] != L'\\' && path[len - 1] != L'/')
            len--;
        path[len] = 0;
        aimp_wcscat(path, MAX_PATH + 32, L"aimp.cfg");
        n += load_file(path);
    }
    return n;
}

const char *cfg_applied(void) { return g_applied_len ? g_applied : "(ninguna)"; }
