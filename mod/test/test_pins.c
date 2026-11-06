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
#include "net.h"
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
static uint32_t get32(void *base, uint32_t off)
{
    uint32_t v;
    memcpy(&v, (uint8_t *)base + off, 4);
    return v;
}

typedef void(__attribute__((fastcall)) * FireFn)(void *self, void *edx, void *ctx, const uint32_t *pin, void *msg,
                                                 double delay, uint32_t flag);

static uint32_t g_calls, g_last_this, g_last_ctx, g_last_pin, g_last_outer;
static FireFn g_fire;
static void *g_nested_ent, *g_nested_ctx;
static uint32_t g_nested_pin = 0x71b80569; /* on_finished */

/* Igual que pins.c (el pid se calcula igual en las dos máquinas). */
static uint32_t hash2(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

static void __attribute__((cdecl)) fake_fire(uint32_t self, uint32_t ctx, const uint32_t *pin)
{
    g_calls++;
    g_last_this = self;
    g_last_ctx = ctx;
    g_last_pin = *pin;
    g_last_outer = 0;
    if (ctx)
        memcpy(&g_last_outer, (uint8_t *)(uintptr_t)(*(uint32_t *)(uintptr_t)ctx) + 0xC, 4);
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
    /* ctx = puntero a un handle refcontado cuyo +0xC es el objeto completo (outer = ent - 0x14),
     * con el id de la entidad en +0xC y el tipo ("TriggerSimple") en +0x10. */
    uint8_t *h = obj(0x10);
    put32(h, 4, 3);
    putp(h, 0xC, outer);
    put32(outer, 0xC, 0x73da3779);
    put32(outer, 0x10, 0xdd5a65d6);
    /* composite padre: outer+8 = handle -> CompositeInstance con id 11111111 y sin padre */
    uint8_t *parent = obj(0x60), *hp = obj(0x10);
    put32(parent, 0xC, 0x11111111);
    putp(hp, 0xC, parent);
    putp(outer, 8, hp);
    /* El padre es un CompositeInstance (vtable primaria real) con dos hijos: la entidad de arriba y un
     * PlayerTriggerBox que nunca disparó (EntityInterface en +0x18). Las dos vtables tienen
     * FUN_008be0d0 en +0x394, como en el juego. */
    putp(parent, 0, img + AIMP_RVA_VT_COMPOSITE);
    putp(img, 0x00E61EB8 + ENT_VT_FIRE, img + AIMP_RVA_FIRE_PIN);
    putp(img, 0x00E39EF8 + ENT_VT_FIRE, img + AIMP_RVA_FIRE_PIN);
    uint8_t *outer2 = obj(0x100), *ent2 = outer2 + 0x18, *h2 = obj(0x10);
    putp(ent2, 0, img + 0x00E39EF8);
    put32(outer2, 0xC, 0xfa26b87f);
    put32(outer2, 0x10, 0x2f3efad3);
    putp(outer2, 8, hp);
    put32(h2, 4, 2);
    putp(h2, 0xC, outer2);
    uint8_t *kids = obj(0x10), *kid_items = obj(8);
    putp(parent, COMP_CHILDREN, kids);
    put32(kids, 8, 2);
    putp(kids, 0xC, kid_items);
    putp(kid_items, 0, h);
    putp(kid_items, 4, h2);
    uint32_t pid1 = hash2(hash2(0x811C9DC5u, 0x73da3779), 0x11111111);
    uint32_t pid2 = hash2(hash2(0x811C9DC5u, 0xfa26b87f), 0x11111111);
    void *ctx = obj(4);
    putp(ctx, 0, h);
    /* msg = puntero a un objeto de 0x28 bytes del pool (refcount +0x24, flags +0x20) */
    uint8_t *mo = obj(0x28);
    put32(mo, 0x20, 0x01000000);
    put32(mo, 0x24, 1);
    void *msg = obj(4);
    putp(msg, 0, mo);

    SetEnvironmentVariableW(L"AIMP_TRACE_PINS", L"1");
    SetEnvironmentVariableW(L"AIMP_SYNC_PINS", L"1"); /* cliente: reglas por defecto (zona) */
    AimpNet net;
    memset(&net, 0, sizeof net);
    net.mode = 2;
    CHECK(pins_install_at(img + AIMP_RVA_FIRE_PIN, &net) == 1, "hook instalado sobre el prólogo real");
    CHECK(pins_sync_enabled(), "sincronización de pines activada (cliente)");

    printf("[disparos]\n");
    static const uint32_t trig = 0xd9d0ca29, ontrig = 0x19d97b56;
    uint32_t esp0, esp1;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp0));
    for (int i = 0; i < 3; i++)
        g_fire(ent, NULL, ctx, &trig, msg, 0.0, 0);
    __asm__ volatile("mov %%esp, %0" : "=r"(esp1));
    CHECK(esp0 == esp1, "stack balanceado después de 3 disparos (ret 0x18)");
    CHECK(g_calls == 3 && g_last_this == (uint32_t)(uintptr_t)ent && g_last_ctx == (uint32_t)(uintptr_t)ctx &&
              g_last_pin == trig,
          "el original recibió this, ctx y pin intactos (%u llamadas)", g_calls);
    g_fire(ent, NULL, ctx, &ontrig, msg, 1.5, 0);
    CHECK(g_calls == 4, "pin sin links también llega al original");

    printf("[anidado]\n");
    g_nested_ent = ent;
    g_nested_ctx = ctx;
    g_fire(ent, NULL, ctx, &ontrig, msg, 0.0, 0);
    CHECK(g_calls == 6 && g_last_pin == 0x71b80569, "disparo anidado pasa por el hook y por el original");

    printf("[reinyección]\n");
    CHECK(pins_fire(0x73da3779, 0x12345678, 0) == 1 && g_calls == 7 && g_last_pin == 0x12345678 &&
              g_last_this == (uint32_t)(uintptr_t)ent && g_last_outer == (uint32_t)(uintptr_t)outer,
          "FIRE con id de entidad: el original recibe la entidad, el pin pedido y un handle que apunta al objeto completo");
    CHECK(pins_fire(0xdeadbeef, 0x12345678, 0) == 0 && g_calls == 7, "id desconocido: no dispara");
    putp(h, 0xC, NULL); /* la entidad "murió": su handle ya no apunta al objeto */
    CHECK(pins_fire(0x73da3779, trig, 0) == 0 && g_calls == 7, "entidad muerta: no dispara");
    putp(h, 0xC, outer);
    CHECK(get32(h, 4) == 3 && get32(mo, 0x24) == 1, "el handle y el mensaje originales no se tocaron");

    printf("[cambio de nivel]\n");
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    pins_frame(now.QuadPart, 16);
    CHECK(pins_fire(0x73da3779, trig, 0) == 1, "las claves recientes sobreviven al cambio de nivel");

    printf("[índice]\n");
    CHECK(pins_index_build("test") == 3, "índice: el composite y sus dos hijos");
    static const uint32_t on_entered = 0xaf66ad73;
    uint32_t c0 = g_calls;
    CHECK(pins_fire(pid2, on_entered, 0x01000000) == 1 && g_calls == c0 + 1 && g_last_this == (uint32_t)(uintptr_t)ent2 &&
              g_last_outer == (uint32_t)(uintptr_t)outer2 && g_last_pin == on_entered,
          "FIRE por pid de una entidad que nunca disparó: el original recibe su EntityInterface (+0x18) y su objeto");
    CHECK(pins_fire(pid1, trig, 0) == 1 && g_last_this == (uint32_t)(uintptr_t)ent, "FIRE por pid de una entidad vista");
    CHECK(get32(h2, 4) == 2, "el handle del índice no se tocó (se usa una copia)");

    printf("[sincronización, cliente]\n");
    void *ctx2 = obj(4), *msg2 = obj(4);
    uint8_t *mo2 = obj(0x28);
    putp(ctx2, 0, h2);
    put32(mo2, 0x20, 0x11000000);
    put32(mo2, 0x24, 1);
    putp(msg2, 0, mo2);
    c0 = g_calls;
    g_fire(ent2, NULL, ctx2, &on_entered, msg2, 0.0, 0);
    CHECK(g_calls == c0, "un on_entered propio del PlayerTriggerBox se bloquea (lo decide el host)");
    g_fire(ent, NULL, ctx, &trig, msg, 0.0, 0);
    CHECK(g_calls == c0 + 1, "un pin que no está en las reglas pasa");
    AimpEvt ev;
    memset(&ev, 0, sizeof ev);
    ev.kind = AIMP_EVT_PIN;
    ev.seq = 7;
    ev.level = 16;
    ev.size = 20;
    uint32_t data[5] = { pid2, on_entered, 0x2f3efad3, 0xfa26b87f, 0x12000000 };
    memcpy(ev.data, data, sizeof data);
    pins_remote_event(&ev);
    ev.seq = 8;
    ev.level = 99;
    pins_remote_event(&ev);
    c0 = g_calls;
    g_nested_ent = ent2; /* durante la inyección, el mismo pin vuelve a dispararse en cadena: se bloquea */
    g_nested_ctx = ctx2;
    g_nested_pin = on_entered;
    QueryPerformanceCounter(&now);
    pins_frame(now.QuadPart, 16);
    CHECK(g_calls == c0 + 1 && g_last_this == (uint32_t)(uintptr_t)ent2 && g_last_pin == on_entered,
          "el pin del host se aplica en el cliente (una llamada: el anidado se bloqueó)");
    pins_frame(now.QuadPart, 16);
    CHECK(g_calls == c0 + 1, "el evento de otro nivel queda pendiente");

    printf("[sincronización, host]\n");
    pins_test_role(1);
    net.mode = 1;
    net.have_peer = 1;
    c0 = g_calls;
    g_fire(ent2, NULL, ctx2, &on_entered, msg2, 0.0, 0);
    g_fire(ent, NULL, ctx, &trig, msg, 0.0, 0);
    CHECK(g_calls == c0 + 2, "en el host nada se bloquea");
    pins_frame(now.QuadPart, 16);
    int nev = 0, okev = 0;
    for (int i = 0; i < AIMP_EVT_QUEUE; i++) {
        if (!net.out_left[i])
            continue;
        nev++;
        uint32_t d[5];
        memcpy(d, net.out[i].data, sizeof d);
        okev = net.out[i].kind == AIMP_EVT_PIN && net.out[i].size == 20 && net.out[i].level == 16 && d[0] == pid2 &&
               d[1] == on_entered && d[2] == 0x2f3efad3 && d[3] == 0xfa26b87f && d[4] == 0x11000000;
    }
    CHECK(nev == 1 && okev, "sale un solo evento AIMP_EVT_PIN (el de la regla) con pid, pin, tipo, id y flags del mensaje");

    pins_report_totals("test");
    char *log = read_log();
    CHECK(log != NULL, "log leído");
    if (log) {
        expect_n(log, "pin d9d0ca29 n1 | links 2 tgt 0x", 1);
        expect_n(log, "pin d9d0ca29 n2 | links 2", 1);
        expect_n(log, "pin d9d0ca29 n3", 0);
        expect_n(log, "pin 19d97b56 n1 | links 0", 1);
        expect_n(log, "demora 3ff8000000000000", 1);
        expect_n(log, "vt e61eb8 idx 291 fl 01", 10); /* 4 naturales detalladas, 3 inyecciones, 3 en la ventana */
        expect_n(log, "| id 73da3779 tipo dd5a65d6 pid ", 10);
        expect_n(log, "o -20 |", 10);
        expect_n(log, "o -24 |", 5); /* el PlayerTriggerBox (EntityInterface en +0x18) */
        expect_n(log, " d2 | pin", 15);
        expect_n(log, "(INYECTADO)", 5);
        expect_n(log, "mfl 01000000 mref 1", 6); /* el anidado no tiene msg */
        expect_n(log, "renovada (3 claves", 0);
        expect_n(log, "claves de los últimos 3 s conservadas", 1);
        expect_n(log, "f77 ", 15);
        expect_n(log, "pin 71b80569 n1 | links 0", 1);
        expect_n(log, "(anidado)", 2);
        expect_n(log, "PIN primera instancia de la vtable", 2);
        expect_n(log, "PIN totales (test): 16 disparos, 5 (entidad, pin) distintos", 1);
        expect_n(log, "3 entidades (3 nodos, 1 composites, 1 raíces, prof. máx. 1)", 2);
        expect_n(log, "contra lo visto: 4 ok, 0 con otro subobjeto, 0 faltan", 2);
        expect_n(log, "por índice), mensaje vacío, mfl 01000000 -> 01000000", 1);
        expect_n(log, "(BLOQUEADO)", 2);
        expect_n(log, "PINSYNC bloqueado (lo decide el host): tipo 2f3efad3", 2);
        expect_n(log, "PINSYNC aplicado (evento 7)", 1);
        expect_n(log, "mfl 11000000 -> 12000000", 1); /* clase del mensaje del host (salió) sobre la plantilla vista */
        expect_n(log, "PINSYNC pendiente (evento 8", 1);
        expect_n(log, "PINSYNC al cliente: tipo 2f3efad3 id fa26b87f", 1);
        expect_n(log, "1 mandados, 2 bloqueados, 1 aplicados, 0 vencidos, 1 pendientes", 1);
        expect_n(log, "PIN mensaje del primer", 3); /* d9d0ca29, 19d97b56 y el on_entered natural; los inyectados no */
        expect_n(log, "PIN mensaje del primer 2f3efad3.af66ad73 (mfl 11000000)", 1);
        expect_n(log, "anidamiento máx. 2", 2); /* resumen del cambio de nivel y el final */
        expect_n(log, "PIN        7 x  ent", 1);
    }
    printf(g_fail ? "test_pins: %d FALLAS\n" : "test_pins: todo OK\n", g_fail);
    return g_fail ? 1 : 0;
}
