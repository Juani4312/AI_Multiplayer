/* Lecturas seguras de memoria del juego.
 *
 * Se usa ReadProcessMemory sobre el propio proceso: si la dirección no es legible
 * devuelve error en lugar de generar una excepción. Es más lento que leer directo
 * (bajo Wine pasa por el wineserver), por eso la sonda lee poco por frame y el
 * resto cada AIMP_SAMPLE_MS. */
#include "aimp.h"

int mem_read(uintptr_t addr, void *out, size_t n)
{
    SIZE_T got = 0;
    if (addr < 0x10000 || n == 0)
        return 0;
    if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)addr, out, n, &got))
        return 0;
    return got == n;
}

int mem_write(uintptr_t addr, const void *src, size_t n)
{
    SIZE_T put = 0;
    if (addr < 0x10000 || n == 0)
        return 0;
    if (!WriteProcessMemory(GetCurrentProcess(), (LPVOID)addr, src, n, &put))
        return 0;
    return put == n;
}

int mem_u32(uintptr_t addr, uint32_t *out)
{
    if (!mem_read(addr, out, 4)) {
        *out = 0;
        return 0;
    }
    return 1;
}

int mem_u8(uintptr_t addr, uint8_t *out)
{
    if (!mem_read(addr, out, 1)) {
        *out = 0;
        return 0;
    }
    return 1;
}

int mem_f32(uintptr_t addr, float *out)
{
    if (!mem_read(addr, out, 4)) {
        *out = 0.0f;
        return 0;
    }
    return 1;
}

int mem_cstr(uintptr_t addr, char *out, size_t cap)
{
    if (cap == 0)
        return 0;
    out[0] = 0;
    if (!mem_read(addr, out, cap - 1))
        return 0;
    out[cap - 1] = 0;
    for (size_t i = 0; i < cap - 1; i++) {
        if (out[i] == 0)
            break;
        /* No ensuciar el log con bytes de control. */
        if ((unsigned char)out[i] < 0x20)
            out[i] = '?';
    }
    return 1;
}

/* Compara código del juego con los bytes esperados (tomados del AI.exe con base 0x00400000). Con ASLR (Windows; la
 * laptop carga el exe en 0x00620000) el loader corrige las direcciones absolutas: un `push 0x0112FEC8` queda como
 * `push 0x0134FEC8`. Ante una diferencia, se acepta un valor de 4 bytes que en lo esperado cae dentro de la imagen y en
 * lo real vale lo esperado + (base real - 0x00400000). Prueba con dos máquinas, 2026-11-17: el prólogo de FUN_007c3980
 * "no coincide" en Windows por eso. Devuelve 0 si coincide (como memcmp). */
int aimp_code_cmp(const void *actual, const void *expected, size_t n)
{
    const uint8_t *a = actual, *e = expected;
    uint32_t delta = (uint32_t)g_aimp_exe - 0x00400000u;
    size_t i = 0;
    while (i < n) {
        if (a[i] == e[i]) {
            i++;
            continue;
        }
        int ok = 0;
        for (size_t j = i >= 3 ? i - 3 : 0; j <= i && j + 4 <= n && !ok; j++) {
            uint32_t ev, av;
            aimp_memcpy(&ev, e + j, 4);
            aimp_memcpy(&av, a + j, 4);
            if (delta && ev >= 0x00400000u && ev < 0x02400000u && av == ev + delta) {
                ok = 1;
                i = j + 4;
            }
        }
        if (!ok)
            return 1;
    }
    return 0;
}
