/* Test de savesync (correr con wine): host y cliente en el mismo proceso por 127.0.0.1, con saves en memoria.
 * Cubre el manifiesto (solo viajan los distintos), la compresión LZ, pérdidas, el backup y la segunda conexión
 * (ya iguales: no viaja nada). */
#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aimp.h"
#include "log.h"
#include "net.h"
#include "savesync.h"

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

/* ---- saves en memoria ---- */
typedef struct {
    char name[32];
    uint8_t *data;
    int32_t size;
    int64_t stamp;
} MemFile;
static int64_t g_clock = 1000;
typedef struct {
    MemFile f[16];
    int n;
    int writes;
} MemStore;

static MemFile *find(MemStore *m, const char *name)
{
    for (int i = 0; i < m->n; i++)
        if (!strcmp(m->f[i].name, name))
            return &m->f[i];
    return NULL;
}
static int m_count(void *ctx) { return ((MemStore *)ctx)->n; }
static int m_entry(void *ctx, int i, char *name, int cap, int32_t *size)
{
    MemStore *m = ctx;
    if (i < 0 || i >= m->n)
        return 0;
    snprintf(name, (size_t)cap, "%s", m->f[i].name);
    *size = m->f[i].size;
    return 1;
}
static int32_t m_read(void *ctx, const char *name, void *buf, int32_t cap)
{
    MemFile *f = find(ctx, name);
    if (!f)
        return -1;
    int32_t n = f->size < cap ? f->size : cap;
    memcpy(buf, f->data, (size_t)n);
    return n;
}
static int m_write(void *ctx, const char *name, const void *buf, int32_t n)
{
    MemStore *m = ctx;
    MemFile *f = find(m, name);
    if (!f) {
        f = &m->f[m->n++];
        snprintf(f->name, sizeof f->name, "%s", name);
        f->data = NULL;
    }
    free(f->data);
    f->data = malloc((size_t)n);
    memcpy(f->data, buf, (size_t)n);
    f->size = n;
    f->stamp = ++g_clock;
    m->writes++;
    return 1;
}
static int64_t m_stamp(void *ctx, const char *name)
{
    MemFile *f = find(ctx, name);
    return f ? f->stamp : 0;
}
static void put(MemStore *m, const char *name, int32_t size, uint32_t seed, int32_t used)
{
    uint8_t *b = calloc(1, (size_t)size);
    uint32_t x = seed;
    for (int32_t i = 0; i < used; i++) {
        /* mitad patrones repetidos, mitad ruido: algo que se comprime a medias, como un save real */
        if ((i / 512) % 2) {
            x = x * 1103515245u + 12345u;
            b[i] = (uint8_t)(x >> 16);
        } else {
            b[i] = (uint8_t)((i * 7 + seed) & 0x3F);
        }
    }
    m_write(m, name, b, size);
    free(b);
    m->writes = 0;
}
static int same(MemStore *a, MemStore *b, const char *name)
{
    MemFile *x = find(a, name), *y = find(b, name);
    return x && y && x->size == y->size && !memcmp(x->data, y->data, (size_t)x->size);
}

static void open_pair(AimpNet *h, AimpNet *c, int64_t freq, const wchar_t *port, const wchar_t *peer)
{
    SetEnvironmentVariableW(L"AIMP_NET", L"host");
    SetEnvironmentVariableW(L"AIMP_NET_PORT", port);
    SetEnvironmentVariableW(L"AIMP_NET_HZ", L"60");
    CHECK(net_init(h, freq), "host abierto");
    SetEnvironmentVariableW(L"AIMP_NET", L"join");
    SetEnvironmentVariableW(L"AIMP_NET_PEER", peer);
    CHECK(net_init(c, freq), "cliente abierto");
}

/* Corre los dos extremos hasta que el cliente termine (o 30 s). Devuelve los ms que tardó. */
static int run(AimpNet *h, AimpNet *c, SaveSync *sh, SaveSync *sc, int64_t freq, uint32_t client_level)
{
    LARGE_INTEGER t0, now;
    QueryPerformanceCounter(&t0);
    AimpPkt st;
    memset(&st, 0, sizeof st);
    for (;;) {
        QueryPerformanceCounter(&now);
        net_poll(c, now.QuadPart);
        net_poll(h, now.QuadPart);
        net_send(c, now.QuadPart, &st);
        net_send(h, now.QuadPart, &st);
        savesync_tick(sh, now.QuadPart, freq, 10);
        savesync_tick(sc, now.QuadPart, freq, client_level);
        int s = savesync_state(sc);
        int ms = (int)((now.QuadPart - t0.QuadPart) * 1000 / freq);
        if (s >= 3 || ms > 30000) {
            /* unos frames más para que el DONE llegue al host */
            for (int i = 0; i < 10; i++) {
                QueryPerformanceCounter(&now);
                net_poll(h, now.QuadPart);
                net_poll(c, now.QuadPart);
                savesync_tick(sh, now.QuadPart, freq, 10);
                savesync_tick(sc, now.QuadPart, freq, client_level);
                Sleep(2);
            }
            return ms;
        }
        Sleep(2);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    g_aimp_main_tid = GetCurrentThreadId();
    log_open();
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);

    printf("[hash]\n");
    CHECK(savesync_fnv("", 0) == 2166136261u && savesync_fnv("a", 1) == 0xe40c292cu, "FNV-1a de 32 bits");

    printf("[compresión LZ: ida y vuelta]\n");
    {
        static uint8_t in[300000], out[340000], back[300000];
        uint32_t sizes[] = { 0, 1, 5, 12, 13, 17, 64, 4096, 65536 + 300, 300000 };
        int ok = 1;
        for (int k = 0; k < 4 && ok; k++) {
            uint32_t x = 99u + (uint32_t)k;
            for (uint32_t i = 0; i < sizeof in; i++) {
                x = x * 1103515245u + 12345u;
                in[i] = k == 0 ? 0 : k == 1 ? (uint8_t)(x >> 16) : k == 2 ? (uint8_t)(i % 37) : (i / 300) % 3 ? 0 : (uint8_t)(x >> 16);
            }
            for (unsigned j = 0; j < sizeof sizes / sizeof sizes[0] && ok; j++) {
                uint32_t n = sizes[j];
                uint32_t c = savesync_lz_compress(in, n, out, sizeof out);
                memset(back, 0xAB, sizeof back);
                if (c && !(savesync_lz_decompress(out, c, back, n) && !memcmp(in, back, n))) {
                    printf("    falla: patrón %d, %u bytes, comprimido %u\n", k, n, c);
                    ok = 0;
                }
            }
            uint32_t c = savesync_lz_compress(in, sizeof in, out, sizeof out);
            printf("    patrón %d: 300000 -> %u\n", k, c);
        }
        CHECK(ok, "ida y vuelta exacta en 4 patrones y 10 tamaños");
        CHECK(savesync_lz_compress(in, 300000, out, 10) == 0, "sin lugar: devuelve 0");
        uint32_t c = savesync_lz_compress(in, 300000, out, sizeof out);
        out[c / 2] ^= 0x55;
        int r1 = savesync_lz_decompress(out, c, back, 300000), r2 = savesync_lz_decompress(out, c / 2, back, 300000);
        CHECK(!r2, "datos rotos o cortados: no crashea (rotos %d, cortados %d; el hash del archivo los rechaza igual)", r1, r2);
    }
    {
        /* saves reales (opcional): AIMP_TEST_SAVE_DIR=Z:\ruta\a\los\.ais */
        wchar_t dir[MAX_PATH], pat[MAX_PATH];
        if (GetEnvironmentVariableW(L"AIMP_TEST_SAVE_DIR", dir, MAX_PATH)) {
            printf("[compresión LZ: saves reales]\n");
            wcscpy(pat, dir);
            wcscat(pat, L"\\*.ais");
            WIN32_FIND_DATAW fd;
            HANDLE fh = FindFirstFileW(pat, &fd);
            uint64_t tin = 0, tout = 0;
            int ok = 1;
            if (fh != INVALID_HANDLE_VALUE) {
                do {
                    wchar_t path[MAX_PATH];
                    wcscpy(path, dir);
                    wcscat(path, L"\\");
                    wcscat(path, fd.cFileName);
                    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                    DWORD n = GetFileSize(h, NULL), r = 0;
                    uint8_t *b = malloc(n), *o = malloc(n + n / 8 + 64), *bk = malloc(n);
                    ReadFile(h, b, n, &r, NULL);
                    CloseHandle(h);
                    LARGE_INTEGER t0, t1, fq;
                    QueryPerformanceFrequency(&fq);
                    QueryPerformanceCounter(&t0);
                    uint32_t c = savesync_lz_compress(b, n, o, n + n / 8 + 64);
                    QueryPerformanceCounter(&t1);
                    int rt = c && savesync_lz_decompress(o, c, bk, n) && !memcmp(b, bk, n);
                    ok &= rt;
                    printf("    %-16ls %8lu -> %8u (%4.1f %%) %5.1f ms %s\n", fd.cFileName, n, c, 100.0 * c / n,
                           (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)fq.QuadPart, rt ? "" : "FALLA");
                    tin += n;
                    tout += c;
                    free(b), free(o), free(bk);
                } while (FindNextFileW(fh, &fd));
                FindClose(fh);
            }
            CHECK(ok && tin, "saves reales: %llu -> %llu bytes", (unsigned long long)tin, (unsigned long long)tout);
        }
    }

    MemStore hs = { 0 }, cs = { 0 };
    put(&hs, "CHSV00.ais", 2 * 1024 * 1024, 1, 900000);
    put(&hs, "CHSV01.ais", 2 * 1024 * 1024, 2, 700000);
    put(&hs, "MSSV01.ais", 1024 * 1024, 3, 180000);
    put(&hs, "PROGRESSION.ais", 10240, 4, 2670);
    put(&hs, "input.cfg", 764, 5, 764);
    put(&cs, "CHSV00.ais", 2 * 1024 * 1024, 1, 900000); /* igual */
    put(&cs, "CHSV01.ais", 2 * 1024 * 1024, 9, 650000); /* distinto */
    put(&cs, "PROGRESSION.ais", 10240, 8, 2600);       /* distinto */
    put(&cs, "input.cfg", 764, 7, 764);                /* no es un save: no se toca */
    /* MSSV01 falta en el cliente */

    wchar_t bdir[MAX_PATH];
    GetTempPathW(MAX_PATH, bdir);
    wcscat(bdir, L"aimp_savesync_test");
    CreateDirectoryW(bdir, NULL);

    printf("[primera conexión, con pérdidas (1 de cada 7 fragmentos)]\n");
    {
        AimpNet h, c;
        open_pair(&h, &c, f.QuadPart, L"47881", L"127.0.0.1:47881");
        SaveStore sth = { &hs, m_count, m_entry, m_read, m_write, m_stamp }, stc = { &cs, m_count, m_entry, m_read, m_write, m_stamp };
        SaveSync *sh = savesync_new(&h, &sth, 0, NULL), *sc = savesync_new(&c, &stc, 1, bdir);
        savesync_test_drop(sh, 7);
        int ms = run(&h, &c, sh, sc, f.QuadPart, 0);
        CHECK(savesync_state(sc) == 3, "el cliente terminó OK (estado %d) en %d ms", savesync_state(sc), ms);
        CHECK(savesync_written_mask(sc) == 0xE, "escribió CHSV01, MSSV01 y PROGRESSION (máscara %08x)", savesync_written_mask(sc));
        CHECK(same(&hs, &cs, "CHSV00.ais") && same(&hs, &cs, "CHSV01.ais") && same(&hs, &cs, "MSSV01.ais") &&
                  same(&hs, &cs, "PROGRESSION.ais"),
              "los saves del cliente quedaron iguales a los del host");
        CHECK(!same(&hs, &cs, "input.cfg"), "input.cfg no se copió");
        CHECK(cs.writes == 3, "3 escrituras (%d)", cs.writes);
        net_close(&c);
        net_close(&h);
    }

    printf("[backup de los saves propios]\n");
    {
        wchar_t pat[MAX_PATH];
        wcscpy(pat, bdir);
        wcscat(pat, L"\\*");
        WIN32_FIND_DATAW fd;
        HANDLE fh = FindFirstFileW(pat, &fd);
        int found = 0;
        wchar_t sub[MAX_PATH] = L"";
        if (fh != INVALID_HANDLE_VALUE) {
            do
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') {
                    wcscpy(sub, bdir);
                    wcscat(sub, L"\\");
                    wcscat(sub, fd.cFileName);
                    found++;
                }
            while (FindNextFileW(fh, &fd));
            FindClose(fh);
        }
        wchar_t p1[MAX_PATH], p2[MAX_PATH];
        wcscpy(p1, sub);
        wcscat(p1, L"\\CHSV01.ais");
        wcscpy(p2, sub);
        wcscat(p2, L"\\PROGRESSION.ais");
        WIN32_FILE_ATTRIBUTE_DATA a1, a2;
        int ok1 = GetFileAttributesExW(p1, GetFileExInfoStandard, &a1), ok2 = GetFileAttributesExW(p2, GetFileExInfoStandard, &a2);
        CHECK(found >= 1 && ok1 && ok2 && a1.nFileSizeLow == 2 * 1024 * 1024 && a2.nFileSizeLow == 10240,
              "backup con CHSV01 y PROGRESSION viejos (%d carpetas)", found);
        DeleteFileW(p1);
        DeleteFileW(p2);
        RemoveDirectoryW(sub);
    }

    printf("[segunda conexión: ya iguales, no viaja nada]\n");
    {
        AimpNet h, c;
        open_pair(&h, &c, f.QuadPart, L"47882", L"127.0.0.1:47882");
        SaveStore sth = { &hs, m_count, m_entry, m_read, m_write, m_stamp }, stc = { &cs, m_count, m_entry, m_read, m_write, m_stamp };
        cs.writes = 0;
        SaveSync *sh = savesync_new(&h, &sth, 0, NULL), *sc = savesync_new(&c, &stc, 1, bdir);
        int ms = run(&h, &c, sh, sc, f.QuadPart, 0);
        CHECK(savesync_state(sc) == 3 && savesync_written_mask(sc) == 0 && cs.writes == 0,
              "terminó sin escribir nada (estado %d, %d ms)", savesync_state(sc), ms);
        net_close(&c);
        net_close(&h);
    }

    printf("[cliente dentro de un nivel: no pide]\n");
    {
        AimpNet h, c;
        open_pair(&h, &c, f.QuadPart, L"47883", L"127.0.0.1:47883");
        SaveStore sth = { &hs, m_count, m_entry, m_read, m_write, m_stamp }, stc = { &cs, m_count, m_entry, m_read, m_write, m_stamp };
        SaveSync *sh = savesync_new(&h, &sth, 0, NULL), *sc = savesync_new(&c, &stc, 1, bdir);
        LARGE_INTEGER now;
        AimpPkt st;
        memset(&st, 0, sizeof st);
        for (int i = 0; i < 100; i++) {
            QueryPerformanceCounter(&now);
            net_poll(&c, now.QuadPart);
            net_poll(&h, now.QuadPart);
            net_send(&c, now.QuadPart, &st);
            net_send(&h, now.QuadPart, &st);
            savesync_tick(sh, now.QuadPart, f.QuadPart, 10);
            savesync_tick(sc, now.QuadPart, f.QuadPart, 10);
            Sleep(2);
        }
        CHECK(savesync_state(sc) == 0, "en el nivel 10 el cliente espera (estado %d)", savesync_state(sc));
        net_close(&c);
        net_close(&h);
    }
    printf("[guardar juntos: el host guarda y el cliente, dentro del nivel, se copia el save nuevo]\n");
    {
        AimpNet h, c;
        open_pair(&h, &c, f.QuadPart, L"47884", L"127.0.0.1:47884");
        SaveStore sth = { &hs, m_count, m_entry, m_read, m_write, m_stamp }, stc = { &cs, m_count, m_entry, m_read, m_write, m_stamp };
        SaveSync *sh = savesync_new(&h, &sth, 0, NULL), *sc = savesync_new(&c, &stc, 1, bdir);
        savesync_test_poll(sh, 50, 150);
        savesync_test_poll(sc, 50, 150);
        int ms = run(&h, &c, sh, sc, f.QuadPart, 0);
        CHECK(savesync_state(sc) == 3 && savesync_written_mask(sc) == 0, "al conectar, ya iguales (%d ms)", ms);
        put(&hs, "CHSV00.ais", 2 * 1024 * 1024, 77, 950000); /* el host guarda */
        LARGE_INTEGER t0, now;
        QueryPerformanceCounter(&t0);
        AimpPkt st;
        memset(&st, 0, sizeof st);
        int done = 0;
        for (;;) {
            QueryPerformanceCounter(&now);
            net_poll(&c, now.QuadPart);
            net_poll(&h, now.QuadPart);
            net_send(&c, now.QuadPart, &st);
            net_send(&h, now.QuadPart, &st);
            savesync_tick(sh, now.QuadPart, f.QuadPart, 10);
            savesync_tick(sc, now.QuadPart, f.QuadPart, 10); /* el cliente está jugando */
            if (same(&hs, &cs, "CHSV00.ais") && savesync_state(sc) == 3) {
                done = 1;
                break;
            }
            if ((now.QuadPart - t0.QuadPart) * 1000 / f.QuadPart > 10000)
                break;
            Sleep(2);
        }
        CHECK(done && savesync_written_mask(sc) == 1, "el cliente copió el CHSV00 nuevo dentro del nivel en %d ms (máscara %08x)",
              (int)((now.QuadPart - t0.QuadPart) * 1000 / f.QuadPart), savesync_written_mask(sc));
        /* el juego del cliente reescribe su CHSV00 (como al morir): el mod lo vuelve a igualar al del host */
        put(&cs, "CHSV00.ais", 2 * 1024 * 1024, 55, 940000);
        QueryPerformanceCounter(&t0);
        done = 0;
        int writes0 = cs.writes;
        for (;;) {
            QueryPerformanceCounter(&now);
            net_poll(&c, now.QuadPart);
            net_poll(&h, now.QuadPart);
            net_send(&c, now.QuadPart, &st);
            net_send(&h, now.QuadPart, &st);
            savesync_tick(sh, now.QuadPart, f.QuadPart, 10);
            savesync_tick(sc, now.QuadPart, f.QuadPart, 10);
            if (same(&hs, &cs, "CHSV00.ais") && savesync_state(sc) == 3) {
                done = 1;
                break;
            }
            if ((now.QuadPart - t0.QuadPart) * 1000 / f.QuadPart > 10000)
                break;
            Sleep(2);
        }
        /* y no entra en un bucle: con todo igual, no vuelve a escribir */
        for (int i = 0; i < 300; i++) {
            QueryPerformanceCounter(&now);
            net_poll(&c, now.QuadPart);
            net_poll(&h, now.QuadPart);
            savesync_tick(sh, now.QuadPart, f.QuadPart, 10);
            savesync_tick(sc, now.QuadPart, f.QuadPart, 10);
            Sleep(2);
        }
        CHECK(done && cs.writes == writes0 + 1, "el juego del cliente pisó su CHSV00 y el mod lo volvió a igualar al del host, sin bucle (escrituras %d)",
              cs.writes - writes0);
        net_close(&c);
        net_close(&h);
    }
    RemoveDirectoryW(bdir);

    printf(g_fail ? "HAY %d FALLAS\n" : "todo OK\n", g_fail);
    return g_fail ? 1 : 0;
}
