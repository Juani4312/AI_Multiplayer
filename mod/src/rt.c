/* Runtime mínimo sin CRT. */
#include "aimp.h"

void *aimp_memcpy(void *dst, const void *src, size_t n) { return memcpy(dst, src, n); }
void *aimp_memset(void *dst, int c, size_t n) { return memset(dst, c, n); }
int aimp_memcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }

size_t aimp_strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

size_t aimp_wcslen(const wchar_t *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

size_t aimp_wcscpy(wchar_t *dst, size_t cap, const wchar_t *src)
{
    size_t n = 0;
    if (cap == 0)
        return 0;
    while (src[n] && n + 1 < cap) {
        dst[n] = src[n];
        n++;
    }
    dst[n] = 0;
    return n;
}

size_t aimp_wcscat(wchar_t *dst, size_t cap, const wchar_t *src)
{
    size_t n = aimp_wcslen(dst);
    if (n >= cap)
        return n;
    return n + aimp_wcscpy(dst + n, cap - n, src);
}

char *aimp_utf8(char *buf, size_t cap, const wchar_t *ws)
{
    if (cap == 0)
        return buf;
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf, (int)cap, NULL, NULL);
    if (n <= 0)
        buf[0] = 0;
    buf[cap - 1] = 0;
    return buf;
}

int aimp_env_str(const wchar_t *name, wchar_t *out, size_t cap)
{
    DWORD n = GetEnvironmentVariableW(name, out, (DWORD)cap);
    if (n == 0 || n >= cap) {
        if (cap)
            out[0] = 0;
        return 0;
    }
    return 1;
}

int aimp_env_int(const wchar_t *name, int def)
{
    wchar_t buf[32];
    if (!aimp_env_str(name, buf, 32))
        return def;
    int neg = 0, v = 0, any = 0;
    const wchar_t *p = buf;
    if (*p == L'-') {
        neg = 1;
        p++;
    }
    for (; *p >= L'0' && *p <= L'9'; p++) {
        v = v * 10 + (*p - L'0');
        any = 1;
    }
    return any ? (neg ? -v : v) : def;
}
