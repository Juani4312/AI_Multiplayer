/* Log a archivo, seguro entre threads. WriteFile sin buffer propio: si el juego
 * crashea, lo escrito ya está en el sistema operativo. */
#include "log.h"

static CRITICAL_SECTION g_cs;
static HANDLE g_file = INVALID_HANDLE_VALUE;
static LARGE_INTEGER g_t0, g_freq;
static int g_ready;
static char g_path_utf8[MAX_PATH * 3];

static int ensure_dir(const wchar_t *dir)
{
    if (CreateDirectoryW(dir, NULL))
        return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

static int pick_path(wchar_t *out, size_t cap)
{
    wchar_t base[MAX_PATH];
    if (aimp_env_str(L"AIMP_LOG", out, cap))
        return 1;
    if (aimp_env_str(L"LOCALAPPDATA", base, MAX_PATH)) {
        aimp_wcscpy(out, cap, base);
        aimp_wcscat(out, cap, L"\\AI_Multiplayer");
        if (ensure_dir(out)) {
            aimp_wcscat(out, cap, L"\\aimp.log");
            return 1;
        }
    }
    if (GetTempPathW(MAX_PATH, base)) {
        aimp_wcscpy(out, cap, base);
        aimp_wcscat(out, cap, L"aimp.log");
        return 1;
    }
    return 0;
}

void log_open(void)
{
    wchar_t path[MAX_PATH + 16], prev[MAX_PATH + 32];
    if (g_ready)
        return;
    InitializeCriticalSection(&g_cs);
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_t0);
    g_ready = 1;
    if (!pick_path(path, MAX_PATH))
        return;
    aimp_wcscpy(prev, MAX_PATH + 32, path);
    aimp_wcscat(prev, MAX_PATH + 32, L".prev");
    MoveFileExW(path, prev, MOVEFILE_REPLACE_EXISTING);
    g_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    aimp_utf8(g_path_utf8, sizeof g_path_utf8, path);
}

void log_close(void)
{
    if (!g_ready)
        return;
    EnterCriticalSection(&g_cs);
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    LeaveCriticalSection(&g_cs);
}

const char *log_path_utf8(void) { return g_path_utf8; }

double log_now(void)
{
    LARGE_INTEGER t;
    if (!g_ready || g_freq.QuadPart == 0)
        return 0.0;
    QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_t0.QuadPart) / (double)g_freq.QuadPart;
}

static void write_raw(const char *s, size_t n)
{
    DWORD w;
    if (g_file != INVALID_HANDLE_VALUE)
        WriteFile(g_file, s, (DWORD)n, &w, NULL);
}

void log_printf(const char *fmt, ...)
{
    static char line[2048];
    if (!g_ready)
        return;
    EnterCriticalSection(&g_cs);
    DWORD tid = GetCurrentThreadId();
    int n;
    if (tid == g_aimp_main_tid || g_aimp_main_tid == 0)
        n = aimp_snprintf(line, sizeof line, "[%9.3f] ", log_now());
    else
        n = aimp_snprintf(line, sizeof line, "[%9.3f T%u] ", log_now(), (unsigned)tid);
    va_list ap;
    va_start(ap, fmt);
    n += aimp_vsnprintf(line + n, sizeof line - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof line - 2)
        n = (int)sizeof line - 2;
    line[n++] = '\n';
    write_raw(line, (size_t)n);
    LeaveCriticalSection(&g_cs);
}

void log_hexdump(const char *title, uintptr_t addr, size_t len)
{
    char row[160];
    log_printf("%s @ 0x%08x (0x%x bytes)", title, (unsigned)addr, (unsigned)len);
    for (size_t off = 0; off < len; off += 16) {
        uint8_t b[16];
        size_t k = len - off < 16 ? len - off : 16;
        int n = aimp_snprintf(row, sizeof row, "    +%03x:", (unsigned)off);
        if (!mem_read(addr + off, b, k)) {
            aimp_snprintf(row + n, sizeof row - (size_t)n, " (ilegible)");
            log_printf("%s", row);
            continue;
        }
        for (size_t i = 0; i < 16; i++)
            n += i < k ? aimp_snprintf(row + n, sizeof row - (size_t)n, " %02x", b[i])
                       : aimp_snprintf(row + n, sizeof row - (size_t)n, "   ");
        n += aimp_snprintf(row + n, sizeof row - (size_t)n, "  |");
        for (size_t i = 0; i < k; i++)
            row[n++] = (b[i] >= 0x20 && b[i] < 0x7F) ? (char)b[i] : '.';
        row[n++] = '|';
        row[n] = 0;
        log_printf("%s", row);
    }
}

/* ¿Parece un float "razonable" (no denormal, no enorme)? */
static int plausible_float(uint32_t u)
{
    uint32_t e = (u >> 23) & 0xFF;
    if (u == 0 || u == 0x80000000u)
        return 1;
    return e >= 127 - 20 && e <= 127 + 24;
}

void log_dump32(const char *title, uintptr_t addr, size_t len)
{
    char row[200];
    log_printf("%s @ 0x%08x (0x%x bytes)", title, (unsigned)addr, (unsigned)len);
    for (size_t off = 0; off < len; off += 16) {
        uint32_t d[4];
        int n = aimp_snprintf(row, sizeof row, "    +%03x:", (unsigned)off);
        if (!mem_read(addr + off, d, 16)) {
            aimp_snprintf(row + n, sizeof row - (size_t)n, " (ilegible)");
            log_printf("%s", row);
            continue;
        }
        for (int i = 0; i < 4; i++)
            n += aimp_snprintf(row + n, sizeof row - (size_t)n, " %08x", d[i]);
        n += aimp_snprintf(row + n, sizeof row - (size_t)n, "  |");
        for (int i = 0; i < 4; i++) {
            union { uint32_t u; float f; } c = { d[i] };
            if (plausible_float(d[i]))
                n += aimp_snprintf(row + n, sizeof row - (size_t)n, " %10.4f", (double)c.f);
            else
                n += aimp_snprintf(row + n, sizeof row - (size_t)n, " %10s", ".");
        }
        log_printf("%s", row);
    }
}
