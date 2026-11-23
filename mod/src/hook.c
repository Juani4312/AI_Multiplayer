/* Detour mínimo para x86: JMP rel32 + trampolín con los bytes robados. */
#include "hook.h"
#include "log.h"

static int32_t rel32(uintptr_t from_next, uintptr_t to)
{
    return (int32_t)(to - from_next);
}

static void hex_bytes(char *out, size_t cap, const uint8_t *b, uint32_t n)
{
    size_t k = 0;
    out[0] = 0;
    for (uint32_t i = 0; i < n && k + 4 < cap; i++)
        k += (size_t)aimp_snprintf(out + k, cap - k, i ? " %02x" : "%02x", b[i]);
}

int hook_install(AimpHook *h, const char *name, void *target, const uint8_t *expect,
                 uint32_t expect_len, uint32_t steal, void *detour, void **tramp_out)
{
    uint8_t cur[32];
    char hb[128], he[128];
    DWORD old, tmp;

    aimp_memset(h, 0, sizeof *h);
    h->name = name;
    if (steal < 5 || steal > sizeof h->orig || expect_len < steal || expect_len > sizeof cur) {
        log_printf("hook %s: parámetros inválidos (steal=%u expect_len=%u)", name, steal, expect_len);
        return 0;
    }
    if (!mem_read((uintptr_t)target, cur, expect_len)) {
        log_printf("hook %s: no se puede leer 0x%08x", name, (unsigned)(uintptr_t)target);
        return 0;
    }
    if (aimp_code_cmp(cur, expect, expect_len) != 0) {
        hex_bytes(hb, sizeof hb, cur, expect_len);
        hex_bytes(he, sizeof he, expect, expect_len);
        log_printf("hook %s: bytes distintos en 0x%08x: hay [%s], se esperaba [%s]. No se instala.",
                   name, (unsigned)(uintptr_t)target, hb, he);
        return 0;
    }

    uint8_t *tr = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) {
        log_printf("hook %s: VirtualAlloc falló (%u)", name, (unsigned)GetLastError());
        return 0;
    }
    aimp_memcpy(tr, cur, steal);
    tr[steal] = 0xE9;
    int32_t back = rel32((uintptr_t)tr + steal + 5, (uintptr_t)target + steal);
    aimp_memcpy(tr + steal + 1, &back, 4);
    VirtualProtect(tr, 64, PAGE_EXECUTE_READ, &tmp);
    FlushInstructionCache(GetCurrentProcess(), tr, 64);

    uint8_t patch[16];
    patch[0] = 0xE9;
    int32_t go = rel32((uintptr_t)target + 5, (uintptr_t)detour);
    aimp_memcpy(patch + 1, &go, 4);
    for (uint32_t i = 5; i < steal; i++)
        patch[i] = 0x90; /* NOP: el resto de la instrucción partida */

    if (!VirtualProtect(target, steal, PAGE_EXECUTE_READWRITE, &old)) {
        log_printf("hook %s: VirtualProtect falló (%u)", name, (unsigned)GetLastError());
        VirtualFree(tr, 0, MEM_RELEASE);
        return 0;
    }
    aimp_memcpy(target, patch, steal);
    VirtualProtect(target, steal, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), target, steal);

    aimp_memcpy(h->orig, cur, steal);
    h->target = target;
    h->tramp = tr;
    h->steal = steal;
    h->installed = 1;
    *tramp_out = tr;
    hex_bytes(hb, sizeof hb, cur, steal);
    log_printf("hook %s: instalado en 0x%08x (bytes robados [%s], trampolín 0x%08x, detour 0x%08x)",
               name, (unsigned)(uintptr_t)target, hb, (unsigned)(uintptr_t)tr,
               (unsigned)(uintptr_t)detour);
    return 1;
}

int hook_remove(AimpHook *h)
{
    DWORD old, tmp;
    if (!h->installed)
        return 0;
    if (!VirtualProtect(h->target, h->steal, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    aimp_memcpy(h->target, h->orig, h->steal);
    VirtualProtect(h->target, h->steal, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), h->target, h->steal);
    h->installed = 0;
    log_printf("hook %s: removido", h->name);
    return 1;
}

static int ieq_ascii(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

static int eq_ascii(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;
    return *a == *b;
}

void *hook_iat_name(uintptr_t base, const char *dll, const char *fn, void *mine)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(base + (uint32_t)dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd->VirtualAddress)
        return NULL;
    for (const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(base + dd->VirtualAddress); d->Name; d++) {
        if (!ieq_ascii((const char *)(base + d->Name), dll) || !d->OriginalFirstThunk)
            continue;
        const IMAGE_THUNK_DATA32 *ot = (const IMAGE_THUNK_DATA32 *)(base + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA32 *ft = (IMAGE_THUNK_DATA32 *)(base + d->FirstThunk);
        for (; ot->u1.AddressOfData; ot++, ft++) {
            if (IMAGE_SNAP_BY_ORDINAL32(ot->u1.Ordinal))
                continue;
            const IMAGE_IMPORT_BY_NAME *ibn = (const IMAGE_IMPORT_BY_NAME *)(base + ot->u1.AddressOfData);
            if (!eq_ascii((const char *)ibn->Name, fn))
                continue;
            void *old_fn = (void *)(uintptr_t)ft->u1.Function;
            if (old_fn == mine)
                return NULL;
            DWORD old;
            if (!VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old))
                return NULL;
            ft->u1.Function = (DWORD)(uintptr_t)mine;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            return old_fn;
        }
    }
    return NULL;
}
