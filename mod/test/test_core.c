/* Tests del núcleo del mod (correr con wine):
 *  1. hook.c sobre una función con exactamente el prólogo de ModeManager::Run;
 *  2. el formateador fmt.c contra el snprintf del CRT. */
#include <stdio.h>
#include <string.h>
#include "aimp.h"
#include "hook.h"

static int g_fail;
#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (cond) {                                       \
            printf("  ok   ");                            \
        } else {                                          \
            printf("  FALLA ");                           \
            g_fail++;                                     \
        }                                                 \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
    } while (0)

/* ---- 1. Hook ----
 * fake_run: mismos 10 primeros bytes que ModeManager::Run (0x7A8460):
 *   51 push ecx | 53 push ebx | 55 push ebp | 56 push esi | 8B F1 mov esi,ecx | 83 7E 4C FF cmp [esi+0x4C],-1
 * thiscall, sin argumentos de stack. Si [this+0x4C] != -1 incrementa [this+0x50] y
 * devuelve 0x12345601 (AL = 1); si no, 0x12345600 (AL = 0). Pisa EDX como haría
 * cualquier función MSVC. */
__asm__(".text\n"
        ".globl _fake_run\n"
        "_fake_run:\n"
        "  .byte 0x51, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x4C, 0xFF\n"
        "  push %edi\n"
        "  mov $0xDEADBEEF, %edx\n"
        "  je 1f\n"
        "  incl 0x50(%esi)\n"
        "  mov $0x12345601, %eax\n"
        "  jmp 2f\n"
        "1:\n"
        "  mov $0x12345600, %eax\n"
        "2:\n"
        "  pop %edi\n"
        "  pop %esi\n"
        "  pop %ebp\n"
        "  pop %ebx\n"
        "  pop %ecx\n"
        "  ret\n");
extern void fake_run(void);

/* Llama f(this) como lo hace el loop del juego, con el stack desalineado a 4 bytes
 * y registros callee-saved con valores conocidos. Verifica que se preserven y que
 * ESP quede igual. cdecl: uint32_t call_like_game(void *f, void *self). */
uint32_t g_regs_ok, g_esp_ok, g_esp_before;
__asm__(".text\n"
        ".globl _call_like_game\n"
        "_call_like_game:\n"
        "  push %ebp\n  push %ebx\n  push %esi\n  push %edi\n"
        "  mov 20(%esp), %eax\n"
        "  mov 24(%esp), %ecx\n"
        "  sub $4, %esp\n"
        "  mov $0x11111111, %ebx\n  mov $0x22222222, %esi\n"
        "  mov $0x33333333, %edi\n  mov $0x44444444, %ebp\n"
        "  mov %esp, _g_esp_before\n"
        "  call *%eax\n"
        "  movl $0, _g_esp_ok\n"
        "  cmp _g_esp_before, %esp\n  jne 1f\n"
        "  movl $1, _g_esp_ok\n"
        "1:\n"
        "  movl $0, _g_regs_ok\n"
        "  cmp $0x11111111, %ebx\n  jne 2f\n"
        "  cmp $0x22222222, %esi\n  jne 2f\n"
        "  cmp $0x33333333, %edi\n  jne 2f\n"
        "  cmp $0x44444444, %ebp\n  jne 2f\n"
        "  movl $1, _g_regs_ok\n"
        "2:\n"
        "  add $4, %esp\n"
        "  pop %edi\n  pop %esi\n  pop %ebx\n  pop %ebp\n"
        "  ret\n");
uint32_t call_like_game(void *f, void *self);

typedef uint32_t(__attribute__((thiscall)) * RunFn)(void *self);
static RunFn g_orig;
static int g_detour_calls;
static void *g_detour_self;

__attribute__((fastcall, force_align_arg_pointer)) static uint32_t detour(void *self, void *edx)
{
    volatile double x = 1.5; /* usar FPU/stack dentro del detour */
    g_detour_calls++;
    g_detour_self = self;
    uint32_t r = g_orig(self);
    x *= 2.0;
    (void)x;
    return r;
}

static void test_hook(void)
{
    static const uint8_t prologue[10] = { 0x51, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x4C, 0xFF };
    static const uint8_t wrong[10] = { 0x51, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x83, 0x7E, 0x4C, 0x00 };
    uint32_t obj[0x60 / 4];
    AimpHook h, bad;
    void *tr = NULL;
    printf("[hook]\n");
    memset(obj, 0, sizeof obj);

    uint32_t r = call_like_game((void *)fake_run, obj);
    CHECK(r == 0x12345601 && obj[0x50 / 4] == 1 && g_regs_ok && g_esp_ok, "sin hook: ret=%08x cuerpo=%u regs=%u esp=%u",
          r, obj[0x50 / 4], g_regs_ok, g_esp_ok);

    CHECK(!hook_install(&bad, "malo", (void *)fake_run, wrong, 10, 6, (void *)detour, &tr),
          "bytes esperados distintos: no instala");
    CHECK(memcmp((void *)fake_run, prologue, 10) == 0, "función intacta después del rechazo");

    CHECK(hook_install(&h, "fake_run", (void *)fake_run, prologue, 10, 6, (void *)detour, (void **)&g_orig),
          "instala con los bytes correctos");
    const uint8_t *p = (const uint8_t *)fake_run;
    CHECK(p[0] == 0xE9 && p[5] == 0x90 && p[6] == 0x83, "parche: E9 rel32 + NOP, resto intacto (%02x .. %02x %02x)",
          p[0], p[5], p[6]);

    r = call_like_game((void *)fake_run, obj);
    CHECK(r == 0x12345601 && obj[0x50 / 4] == 2 && g_detour_calls == 1 && g_detour_self == obj,
          "con hook: ret=%08x cuerpo=%u detour=%d this ok=%d", r, obj[0x50 / 4], g_detour_calls, g_detour_self == obj);
    CHECK(g_regs_ok && g_esp_ok, "con hook: registros callee-saved y ESP preservados (stack desalineado)");

    obj[0x4C / 4] = 0xFFFFFFFFu;
    r = call_like_game((void *)fake_run, obj);
    CHECK((r & 0xFF) == 0 && r == 0x12345600 && obj[0x50 / 4] == 2 && g_detour_calls == 2,
          "rama de salida: ret=%08x (AL=0) cuerpo sin cambios", r);

    CHECK(hook_remove(&h) && memcmp((void *)fake_run, prologue, 10) == 0, "remove restaura los bytes");
    obj[0x4C / 4] = 0;
    r = call_like_game((void *)fake_run, obj);
    CHECK(r == 0x12345601 && obj[0x50 / 4] == 3 && g_detour_calls == 2, "después de remove ya no pasa por el detour");
}

/* ---- 2. Formateador ---- */
static void fmt_case(const char *fmt, ...)
{
    char a[256], b[256];
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int na = aimp_vsnprintf(a, sizeof a, fmt, ap);
    int nb = vsnprintf(b, sizeof b, fmt, ap2);
    va_end(ap2);
    va_end(ap);
    CHECK(na == nb && strcmp(a, b) == 0, "fmt \"%s\" -> \"%s\" (crt \"%s\")", fmt, a, b);
}

static void test_fmt(void)
{
    char small[8];
    printf("[fmt]\n");
    fmt_case("%d %i %u", -12345, 0, 4000000000u);
    fmt_case("%5d|%-5d|%05d|%+d|% d", 42, 42, 42, 42, 42);
    fmt_case("%x %X %08x %#x", 0xbeefu, 0xbeefu, 0x1234u, 0u);
    fmt_case("%lld %llu %llx", -9000000000ll, 18000000000000000000ull, 0x123456789abcull);
    fmt_case("%s|%10s|%-10s|%.3s", "hola", "hola", "hola", "holanda");
    fmt_case("%c%c", 'o', 'k');
    fmt_case("%.3f %.0f %.0f %.1f %.4f", 3.14159, 2.5, 3.5, -0.04, 1.0 / 3.0);
    fmt_case("%9.3f|%-9.3f|%f", 12.3456, -1.5, 0.1);
    fmt_case("%.3f %.3f", 123456.789, -98765.4321);
    fmt_case("%.2f %.2f", 0.996, 0.125);
    fmt_case("%10.4f %10.4f", 1.0, -3.14159265);
    fmt_case("%hu %zu", 70000, (size_t)12345);
    /* %hhu: C99 convierte a unsigned char (300 -> 44). El ucrtbase de Wine imprime 300,
     * así que acá se compara contra el valor del estándar y no contra el CRT. */
    char hh[16];
    aimp_snprintf(hh, sizeof hh, "%hhu %hhd", 300, 200);
    CHECK(strcmp(hh, "44 -56") == 0, "fmt \"%%hhu %%hhd\" -> \"%s\" (estándar \"44 -56\")", hh);
    int n = aimp_snprintf(small, sizeof small, "%s", "truncado");
    CHECK(n == 8 && strcmp(small, "truncad") == 0, "truncado: n=%d \"%s\"", n, small);
    char e[64];
    aimp_snprintf(e, sizeof e, "%.3e|%.2f", 12345.678, 1e300);
    CHECK(strcmp(e, "1.235e+04|1.00e+300") == 0, "científica: \"%s\"", e);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_hook();
    test_fmt();
    printf("%s (%d fallas)\n", g_fail ? "TESTS CON FALLAS" : "TODOS LOS TESTS OK", g_fail);
    return g_fail ? 1 : 0;
}
