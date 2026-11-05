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
