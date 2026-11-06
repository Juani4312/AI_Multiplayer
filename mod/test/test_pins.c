/* Test de la fase 6, paso 1 (pins.c), con wine. Imagen de AI.exe falsa con un stub en la RVA real de
 * FUN_008be0d0 (disparar pin de salida): prólogo real (push ebp; mov ebp,esp; push -1; push imm32),
 * llama a una función de prueba en C con (this, ctx, pin) y termina con el `ret 0x18` real.
 * Entidades y tablas de links falsas con el layout del hallazgo 15. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimp.h"
#include "game.h"
#include "log.h"
#include "pins.h"

HMODULE g_aimp_self;
uintptr_t g_aimp_exe;
DWORD g_aimp_main_tid;

static int g_fail;
#define CHECK(cond, ...)                          \
    do {                                          \
        int ok_ = (cond);                         \
        printf(ok_ ? "  ok    " : "  FALLA ");    \
        printf(__VA_ARGS__);                      \
        printf("\n");                             \
        if (!ok_)                                 \
            g_fail++;                             \
    } while (0)

static void *obj(size_t n) { return calloc(1, n); }
static void put32(void *base, uint32_t off, uint32_t v) { memcpy((uint8_t *)base + off, &v, 4); }
static void putp(void *base, uint32_t off, void *p) { put32(base, off, (uint32_t)(uintptr_t)p); }

typedef void(__attribute__((fastcall)) * FireFn)(void *self, void *edx, void *ctx, const uint32_t *pin, void *msg,
                                                 double delay, uint32_t flag);

static uint32_t g_calls, g_last_this, g_last_ctx, g_last_pin;
static FireFn g_fire;
static void *g_nested_ent, *g_nested_ctx;
static uint32_t g_nested_pin = 0x71b80569; /* on_finished */

static void __attribute__((cdecl)) fake_fire(uint32_t self, uint32_t ctx, const uint32_t *pin)
{
    g_calls++;
    g_last_this = self;
    g_last_ctx = ctx;
    g_last_pin = *pin;
    if (g_nested_ent) { /* un pin que dispara a otro directamente (como FUN_008be0d0 con un link inmediato) */
        void *e = g_nested_ent;
        g_nested_ent = NULL;
        g_fire(e, NULL, g_nested_ctx, &g_nested_pin, NULL, 0.0, 0);
    }
}

static void emit(uint8_t **p, const void *bytes, size_t n)
{
    memcpy(*p, bytes, n);
    *p += n;
}

static void build_stub(uint8_t *img)
{
    DWORD old;
    uint8_t *p = img + AIMP_RVA_FIRE_PIN;
    VirtualProtect(p, 0x100, PAGE_EXECUTE_READWRITE, &old);
    emit(&p, AIMP_FIRE_PIN_PROLOGUE, sizeof AIMP_FIRE_PIN_PROLOGUE); /* push ebp; mov ebp,esp; push -1; push ... */
    emit(&p, "\xA0\x4F\x13\x01", 4);                                 /* ... imm32 (relocado en el juego) */
    emit(&p, "\x83\xC4\x08", 3);                                     /* add esp,8 */
    emit(&p, "\xFF\x75\x0C\xFF\x75\x08\x51", 7);                     /* push pin; push ctx; push this */
    uint8_t mov = 0xB8;
    uint32_t a = (uint32_t)(uintptr_t)fake_fire;
    emit(&p, &mov, 1);
    emit(&p, &a, 4);
    emit(&p, "\xFF\xD0", 2);                 /* call eax */
    emit(&p, "\x83\xC4\x0C\x5D\xC2\x18\x00", 7); /* add esp,0xC; pop ebp; ret 0x18 */
}

static char *read_log(void)
{
    wchar_t path[MAX_PATH];
    if (!GetEnvironmentVariableW(L"AIMP_LOG", path, MAX_PATH))
        return NULL;
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    s[fread(s, 1, (size_t)n, f)] = 0;
    fclose(f);
    return s;
}

static int count(const char *log, const char *needle)
{
    int n = 0;
    for (const char *q = log; (q = strstr(q, needle)); q++)
        n++;
    return n;
}

static void expect_n(const char *log, const char *needle, int want)
{
    int n = count(log, needle);
    printf(n == want ? "  ok    " : "  FALLA ");
    printf("%d vez/veces \"%s\" (se esperaban %d)\n", n, needle, want);
    if (n != want)
        g_fail++;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    uint8_t *img = VirtualAlloc(NULL, AIMP_EXPECT_SIZEOFIMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    g_aimp_exe = (uintptr_t)img;
    build_stub(img);
    put32(img, AIMP_RVA_FRAME_COUNTER, 77);
    g_fire = (FireFn)(void *)(img + AIMP_RVA_FIRE_PIN);

    /* Entidad: vtable concreta de TriggerSimple (RVA 0x00E61EB8), índice 0x123, flags 0x01.
     * Pin "trigger" (d9d0ca29) con 2 links; "on_triggered" (19d97b56) sin links. */
    uint8_t *outer = obj(0x100), *ent = outer + 0x14;
    putp(ent, 0, img + 0x00E61EB8);
    uint8_t *tab = obj(0x20), *arr = obj(36), *l1 = obj(0x10), *l2 = obj(0x10), *t1 = obj(0x10);
    putp(ent, ENT_LINKS, tab);
    put32(ent, ENT_INDEX, 0x01000123);
    put32(tab, 8, 3);
    putp(tab, 0xC, arr);
    put32(arr, 0, 0x19d97b56);
    put32(arr, 12, 0xd9d0ca29);
    putp(arr, 12 + 8, l1);
    put32(arr, 24, 0x71b80569);
    putp(l1, 8, t1);
    putp(l1, 0xC, l2);
    putp(l2, 8, t1);
    /* ctx = puntero a un handle refcontado cuyo +0xC es la propia entidad */
    uint8_t *h = obj(0x10);
    putp(h, 0xC, ent);
    void *ctx = obj(4);
    putp(ctx, 0, h);

    SetEnvironmentVariableW(L"AIMP_TRACE_PINS", L"1");
    CHECK(pins_install_at(img + AIMP_RVA_FIRE_PIN) == 1, "hook instalado sobre el prólogo real");

    printf("[disparos]\n");
    static const uint32_t trig = 0xd9d0ca29, ontrig = 0x19d97b56;
    uint32_t esp0, esp1;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    for (int i = 0; i < 3; i++)
        g_fire(ent, NULL, ctx, &trig, NULL, 0.0, 0);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1, "stack balanceado después de 3 disparos (ret 0x18)");
    CHECK(g_calls == 3 && g_last_this == (uint32_t)(uintptr_t)ent && g_last_ctx == (uint32_t)(uintptr_t)ctx &&
              g_last_pin == trig,
          "el original recibió this, ctx y pin intactos (%u llamadas)", g_calls);
    g_fire(ent, NULL, ctx, &ontrig, NULL, 1.5, 0);
    CHECK(g_calls == 4, "pin sin links también llega al original");

    printf("[anidado]\n");
    g_nested_ent = ent;
    g_nested_ctx = ctx;
    g_fire(ent, NULL, ctx, &ontrig, NULL, 0.0, 0);
    CHECK(g_calls == 6 && g_last_pin == 0x71b80569, "disparo anidado pasa por el hook y por el original");

    pins_report_totals("test");
    char *log = read_log();
    CHECK(log != NULL, "log leído");
    if (log) {
        expect_n(log, "pin d9d0ca29 n1 | links 2 tgt 0x", 1);
        expect_n(log, "pin d9d0ca29 n2 | links 2", 1);
        expect_n(log, "pin d9d0ca29 n3", 0);
        expect_n(log, "pin 19d97b56 n1 | links 0", 1);
        expect_n(log, "demora 3ff8000000000000", 1);
        expect_n(log, "vt e61eb8 idx 291 fl 01", 5);
        expect_n(log, "(ent+0)", 5);
        expect_n(log, "f77 ", 5);
        expect_n(log, "pin 71b80569 n1 | links 0", 1);
        expect_n(log, "(anidado)", 1);
        expect_n(log, "PIN primera instancia de la vtable", 1);
        expect_n(log, "PIN totales (test): 6 disparos, 3 (entidad, pin) distintos", 1);
        expect_n(log, "anidamiento máx. 2", 1);
        expect_n(log, "PIN        3 x  ent", 1);
    }
    printf(g_fail ? "test_pins: %d FALLAS\n" : "test_pins: todo OK\n", g_fail);
    return g_fail ? 1 : 0;
}
