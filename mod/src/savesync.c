/* Copia del save del host al cliente (ver savesync.h). */
#include "savesync.h"
#include "log.h"

#define WINDOW 256     /* fragmentos en vuelo */
#define BURST 24       /* fragmentos por frame (a 60 fps, ~1,4 MB/s) */
#define RTO_MS 250     /* reenvío si no llegó el ACK */
#define FAST_MS 40     /* reenvío de un hueco que el ACK ya mostró */
#define REQ_MS 1000    /* el cliente repite el pedido */
#define ACK_MS 30
#define GIVEUP_MS 30000 /* sin novedades de la transferencia: se abandona */

enum { ST_OFF, ST_WAIT, ST_XFER, ST_DONE, ST_ERR };

struct SaveSync {
    AimpNet *net;
    SaveStore st;
    int want; /* cliente: pedir el save */
    wchar_t backup_dir[MAX_PATH];
    int drop_every, drop_n;
    int state;
    int64_t freq, last_req, last_ack, last_news;
    uint32_t level;
    int warned_level;
    /* manifiesto (host: el propio; cliente: el del host) */
    uint32_t xid;
    int nfiles;
    XferEntry files[XFER_MAX_FILES];
    int64_t manifest_at;
    /* host: blob armado para la máscara pedida */
    uint32_t mask;
    uint8_t *blob;
    uint32_t blob_len, total;
    uint8_t *acked;     /* por fragmento */
    int64_t *sent_at;   /* por fragmento (0 = nunca) */
    uint32_t base;      /* primer fragmento sin ACK */
    uint32_t sends, resends;
    int64_t xfer_t0;
    /* cliente */
    uint8_t *got;
    uint32_t got_n;
    int ack_due;
    uint32_t written;
    int done_left;
    uint32_t done_status;
};

static SaveSync g_game;
static int g_game_on;

uint32_t savesync_fnv(const void *p, size_t n)
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 16777619u;
    }
    return h;
}

static void *xalloc(size_t n) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n); }
static void xfree(void *p)
{
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}
static int64_t ms_ticks(const SaveSync *s, int ms) { return s->freq * ms / 1000; }

/* ---- compresión LZ propia (formato de bloque de LZ4) ----
 * En Wine/Proton, RtlCompressBuffer con LZNT1 no comprime (guarda los bloques crudos: 1 MB -> 1,0005 MB;
 * probado con wine 11.22), y el host del usuario corre con Proton. Esto es un LZ77 voraz chico:
 * token (literales << 4 | largo-4), extensiones de 255, literales, distancia u16, extensión del largo.
 * Las corridas de ceros (los saves son de tamaño fijo con relleno) salen como un match con distancia 1. */
#define METHOD_RAW 0
#define METHOD_LZ 1
#define LZ_HASH_BITS 16

static uint32_t rd32(const uint8_t *p)
{
    uint32_t v;
    aimp_memcpy(&v, p, 4);
    return v;
}

static int lz_len(uint8_t **op, uint8_t *oend, uint32_t v)
{
    for (; v >= 255; v -= 255) {
        if (*op >= oend)
            return 0;
        *(*op)++ = 255;
    }
    if (*op >= oend)
        return 0;
    *(*op)++ = (uint8_t)v;
    return 1;
}

static int lz_seq(uint8_t **op, uint8_t *oend, const uint8_t *lit, uint32_t nlit, uint32_t off, uint32_t ml)
{
    if (*op >= oend)
        return 0;
    uint8_t *tok = (*op)++;
    *tok = (uint8_t)((nlit >= 15 ? 15 : nlit) << 4);
    if (nlit >= 15 && !lz_len(op, oend, nlit - 15))
        return 0;
    if ((uint32_t)(oend - *op) < nlit)
        return 0;
    aimp_memcpy(*op, lit, nlit);
    *op += nlit;
    if (!ml)
        return 1; /* última secuencia: solo literales */
    if (oend - *op < 2)
        return 0;
    *(*op)++ = (uint8_t)off;
    *(*op)++ = (uint8_t)(off >> 8);
    ml -= 4;
    *tok |= (uint8_t)(ml >= 15 ? 15 : ml);
    return ml < 15 || lz_len(op, oend, ml - 15);
}

/* Devuelve el largo comprimido, o 0 si no entra en cap o no conviene. */
uint32_t savesync_lz_compress(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap)
{
    uint32_t *tab = xalloc(sizeof(uint32_t) << LZ_HASH_BITS); /* posición + 1 (0 = vacío) */
    if (!tab)
        return 0;
    uint8_t *op = out, *oend = out + cap;
    uint32_t ip = 0, anchor = 0;
    int ok = 1;
    while (n >= 13 && ip < n - 12 && ok) {
        uint32_t seq = rd32(in + ip), h = (seq * 2654435761u) >> (32 - LZ_HASH_BITS), ref = tab[h];
        tab[h] = ip + 1;
        if (ref && ip - (ref - 1) <= 65535 && rd32(in + ref - 1) == seq) {
            ref--;
            uint32_t ml = 4;
            while (ip + ml < n - 5 && in[ref + ml] == in[ip + ml])
                ml++;
            ok = lz_seq(&op, oend, in + anchor, ip - anchor, ip - ref, ml);
            ip += ml;
            anchor = ip;
        } else {
            ip++;
        }
    }
    ok = ok && lz_seq(&op, oend, in + anchor, n - anchor, 0, 0);
    xfree(tab);
    return ok && (uint32_t)(op - out) < n ? (uint32_t)(op - out) : 0;
}

/* 1 si se descomprimió exactamente raw bytes. */
int savesync_lz_decompress(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t raw)
{
    const uint8_t *ip = in, *iend = in + n;
    uint32_t o = 0;
    while (ip < iend) {
        uint32_t tok = *ip++, nlit = tok >> 4, ml = tok & 15;
        if (nlit == 15) {
            uint8_t b;
            do {
                if (ip >= iend)
                    return 0;
                b = *ip++;
                nlit += b;
            } while (b == 255);
        }
        if ((uint32_t)(iend - ip) < nlit || raw - o < nlit)
            return 0;
        aimp_memcpy(out + o, ip, nlit);
        ip += nlit;
        o += nlit;
        if (ip == iend)
            break; /* última secuencia */
        if (iend - ip < 2)
            return 0;
        uint32_t off = ip[0] | ((uint32_t)ip[1] << 8);
        ip += 2;
        if (ml == 15) {
            uint8_t b;
            do {
                if (ip >= iend)
                    return 0;
                b = *ip++;
                ml += b;
            } while (b == 255);
        }
        ml += 4;
        if (!off || off > o || raw - o < ml)
            return 0;
        for (uint32_t i = 0; i < ml; i++, o++) /* de a un byte: el match puede solaparse */
            out[o] = out[o - off];
    }
    return o == raw;
}

/* ---- paquetes ---- */
static void hdr(XferHdr *h, uint8_t op, uint32_t xid, uint8_t n)
{
    aimp_memset(h, 0, sizeof *h);
    h->magic = AIMP_NET_MAGIC;
    h->version = AIMP_NET_VERSION;
    h->type = AIMP_PKT_XFER;
    h->op = op;
    h->n = n;
    h->xid = xid;
}

static void send_simple(SaveSync *s, uint8_t op, const void *extra, int extra_len)
{
    uint8_t b[sizeof(XferHdr) + 4 + WINDOW / 8];
    hdr((XferHdr *)b, op, s->xid, 0);
    if (extra_len)
        aimp_memcpy(b + sizeof(XferHdr), extra, (size_t)extra_len);
    net_send_raw(s->net, b, (int)sizeof(XferHdr) + extra_len);
}

static int is_save_name(const char *nm)
{
    size_t l = aimp_strlen(nm);
    return l >= 5 && l < XFER_NAME && (nm[l - 4] == '.') && (nm[l - 3] | 32) == 'a' && (nm[l - 2] | 32) == 'i' &&
           (nm[l - 1] | 32) == 's';
}

/* Hash del archivo local con ese nombre (0 si no existe). */
static uint32_t local_hash(SaveSync *s, const char *name, int32_t size, int32_t *got)
{
    *got = -1;
    if (size <= 0 || size > 16 * 1024 * 1024)
        return 0;
    uint8_t *b = xalloc((size_t)size);
    if (!b)
        return 0;
    int32_t r = s->st.read(s->st.ctx, name, b, size);
    *got = r;
    uint32_t h = r > 0 ? savesync_fnv(b, (size_t)r) : 0;
    xfree(b);
    return h;
}

/* ---- host ---- */
static void free_blob(SaveSync *s)
{
    xfree(s->blob);
    xfree(s->acked);
    xfree(s->sent_at);
    s->blob = s->acked = NULL;
    s->sent_at = NULL;
    s->blob_len = s->total = s->base = 0;
    s->mask = 0;
}

static void build_manifest(SaveSync *s, int64_t now)
{
    s->nfiles = 0;
    int c = s->st.count(s->st.ctx);
    for (int i = 0; i < c && s->nfiles < XFER_MAX_FILES; i++) {
        char nm[64];
        int32_t sz = 0;
        if (!s->st.entry(s->st.ctx, i, nm, sizeof nm, &sz) || !is_save_name(nm) || sz <= 0)
            continue;
        XferEntry *e = &s->files[s->nfiles];
        aimp_memset(e, 0, sizeof *e);
        aimp_memcpy(e->name, nm, aimp_strlen(nm));
        int32_t got;
        e->hash = local_hash(s, nm, sz, &got);
        if (got != sz)
            continue;
        e->size = (uint32_t)sz;
        s->nfiles++;
    }
    s->xid = (GetTickCount() << 4) | 1u;
    s->manifest_at = now;
    log_printf("SAVESYNC host: manifiesto %08x con %d saves", s->xid, s->nfiles);
}

static void send_manifest(SaveSync *s)
{
    uint8_t b[sizeof(XferHdr) + XFER_MAX_FILES * sizeof(XferEntry)];
    hdr((XferHdr *)b, XFER_MANIFEST, s->xid, (uint8_t)s->nfiles);
    aimp_memcpy(b + sizeof(XferHdr), s->files, (size_t)s->nfiles * sizeof(XferEntry));
    net_send_raw(s->net, b, (int)(sizeof(XferHdr) + (size_t)s->nfiles * sizeof(XferEntry)));
}

/* Arma el blob: por cada archivo pedido, {método u8, largo u32} + datos (método 0 crudo, 1 LZ). */
static int build_blob(SaveSync *s, uint32_t mask)
{
    free_blob(s);
    uint32_t cap = 0;
    for (int i = 0; i < s->nfiles; i++)
        if (mask & (1u << i))
            cap += 5 + s->files[i].size + s->files[i].size / 8 + 64;
    s->blob = xalloc(cap ? cap : 1);
    if (!s->blob)
        return 0;
    uint32_t off = 0, raw_total = 0;
    for (int i = 0; i < s->nfiles; i++) {
        if (!(mask & (1u << i)))
            continue;
        XferEntry *e = &s->files[i];
        uint8_t *raw = xalloc(e->size);
        if (!raw)
            return 0;
        int32_t r = s->st.read(s->st.ctx, e->name, raw, (int32_t)e->size);
        if (r != (int32_t)e->size || savesync_fnv(raw, e->size) != e->hash) {
            log_printf("SAVESYNC host: %s cambió desde el manifiesto (leídos %d de %u): se rehace", e->name, (int)r, e->size);
            xfree(raw);
            return 0;
        }
        uint32_t cl = savesync_lz_compress(raw, e->size, s->blob + off + 5, cap - off - 5);
        s->blob[off] = cl ? METHOD_LZ : METHOD_RAW;
        if (!cl) {
            cl = e->size;
            aimp_memcpy(s->blob + off + 5, raw, cl);
        }
        aimp_memcpy(s->blob + off + 1, &cl, 4);
        off += 5 + cl;
        raw_total += e->size;
        xfree(raw);
    }
    s->blob_len = off;
    s->total = (off + XFER_CHUNK - 1) / XFER_CHUNK;
    s->acked = xalloc(s->total ? s->total : 1);
    s->sent_at = xalloc(sizeof(int64_t) * (s->total ? s->total : 1));
    s->mask = mask;
    s->base = s->sends = s->resends = 0;
    log_printf("SAVESYNC host: %u bytes de saves -> %u comprimidos, %u fragmentos", raw_total, off, s->total);
    return s->acked && s->sent_at;
}

static void host_send(SaveSync *s, int64_t now)
{
    if (!s->blob || !s->total)
        return;
    if (now - s->last_news > ms_ticks(s, GIVEUP_MS)) {
        log_printf("SAVESYNC host: el cliente no confirma hace %d s: se abandona la transferencia", GIVEUP_MS / 1000);
        free_blob(s);
        s->xid = 0;
        return;
    }
    while (s->base < s->total && s->acked[s->base])
        s->base++;
    int sent = 0;
    for (uint32_t c = s->base; c < s->total && c < s->base + WINDOW && sent < BURST; c++) {
        if (s->acked[c] || (s->sent_at[c] && now - s->sent_at[c] < ms_ticks(s, RTO_MS)))
            continue;
        XferData d;
        hdr(&d.h, XFER_DATA, s->xid, 0);
        d.chunk = c;
        d.total = s->total;
        d.blob_len = s->blob_len;
        uint32_t off = c * XFER_CHUNK, len = s->blob_len - off < XFER_CHUNK ? s->blob_len - off : XFER_CHUNK;
        d.len = (uint16_t)len;
        aimp_memcpy(d.data, s->blob + off, len);
        if (s->sent_at[c])
            s->resends++;
        s->sent_at[c] = now;
        s->sends++;
        sent++;
        if (s->drop_every && ++s->drop_n % s->drop_every == 0)
            continue; /* tests: perdido */
        net_send_raw(s->net, &d, (int)(sizeof d - XFER_CHUNK + len));
    }
}

static void host_rx(SaveSync *s, const XferHdr *h, const uint8_t *p, int len, int64_t now)
{
    if (h->op == XFER_REQ) {
        /* el manifiesto se rehace si no hay una transferencia en curso y tiene más de 5 s */
        if (!s->blob && (!s->xid || now - s->manifest_at > s->freq * 5))
            build_manifest(s, now);
        send_manifest(s);
        return;
    }
    if (h->xid != s->xid)
        return;
    if (h->op == XFER_WANT && len >= (int)sizeof *h + 4) {
        uint32_t mask;
        aimp_memcpy(&mask, p + sizeof *h, 4);
        mask &= s->nfiles >= 32 ? 0xFFFFFFFFu : ((1u << s->nfiles) - 1);
        if (s->blob && s->mask == mask)
            return; /* repetido */
        if (!mask) {
            log_printf("SAVESYNC host: el cliente ya tiene los mismos saves");
            return;
        }
        char list[400];
        int n = 0;
        list[0] = 0;
        for (int i = 0; i < s->nfiles && n < (int)sizeof list - 20; i++)
            if (mask & (1u << i))
                n += aimp_snprintf(list + n, sizeof list - (size_t)n, " %s", s->files[i].name);
        log_printf("SAVESYNC host: el cliente pide%s", list);
        if (!build_blob(s, mask)) {
            free_blob(s);
            s->xid = 0; /* que el próximo REQ rehaga el manifiesto */
            return;
        }
        s->xfer_t0 = s->last_news = now;
        host_send(s, now);
        return;
    }
    if (h->op == XFER_ACK && s->blob && len >= (int)sizeof *h + 4 + WINDOW / 8) {
        uint32_t next, hi = 0;
        uint8_t bits[WINDOW / 8];
        aimp_memcpy(&next, p + sizeof *h, 4);
        aimp_memcpy(bits, p + sizeof *h + 4, sizeof bits);
        s->last_news = now;
        for (uint32_t c = s->base; c < next && c < s->total; c++)
            s->acked[c] = 1;
        for (uint32_t i = 0; i < WINDOW; i++)
            if ((bits[i / 8] >> (i % 8)) & 1 && next + 1 + i < s->total) {
                s->acked[next + 1 + i] = 1;
                hi = next + 1 + i;
            }
        /* reenvío rápido: los huecos antes del último confirmado, si salieron hace más de FAST_MS */
        for (uint32_t c = next; c < hi; c++)
            if (!s->acked[c] && s->sent_at[c] && now - s->sent_at[c] > ms_ticks(s, FAST_MS))
                s->sent_at[c] = now - ms_ticks(s, RTO_MS) - 1;
        return;
    }
    if (h->op == XFER_DONE && len >= (int)sizeof *h + 8) {
        uint32_t st, wr;
        aimp_memcpy(&st, p + sizeof *h, 4);
        aimp_memcpy(&wr, p + sizeof *h + 4, 4);
        if (s->blob) {
            double secs = (double)(now - s->xfer_t0) / (double)s->freq;
            log_printf("SAVESYNC host: el cliente %s (escritos %08x); %u fragmentos, %u envíos, %u reenvíos, %.1f s",
                       st == 0 ? "copió el save" : "NO pudo copiar el save", wr, s->total, s->sends, s->resends, secs);
            free_blob(s);
        }
        s->xid = 0; /* un WANT atrasado de esta transferencia ya no rearma nada; el próximo REQ hace otro manifiesto */
    }
}

/* ---- cliente ---- */
static void client_ack(SaveSync *s)
{
    uint32_t next = 0;
    while (next < s->total && s->got[next])
        next++;
    uint8_t e[4 + WINDOW / 8];
    aimp_memset(e, 0, sizeof e);
    aimp_memcpy(e, &next, 4);
    for (uint32_t i = 0; i < WINDOW; i++)
        if (next + 1 + i < s->total && s->got[next + 1 + i])
            e[4 + i / 8] |= (uint8_t)(1u << (i % 8));
    send_simple(s, XFER_ACK, e, (int)sizeof e);
}

static void backup_path(SaveSync *s, wchar_t *dir, size_t cap)
{
    dir[0] = 0;
    if (s->backup_dir[0]) {
        aimp_wcscpy(dir, cap, s->backup_dir);
    } else {
        wchar_t base[MAX_PATH];
        if (!aimp_env_str(L"LOCALAPPDATA", base, MAX_PATH))
            return;
        aimp_wcscpy(dir, cap, base);
        aimp_wcscat(dir, cap, L"\\AI_Multiplayer\\save_backup");
    }
    CreateDirectoryW(dir, NULL);
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[32];
    wchar_t ws[32];
    aimp_snprintf(stamp, sizeof stamp, "\\%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    for (int i = 0; i < 32; i++)
        if (!(ws[i] = (wchar_t)(uint8_t)stamp[i]))
            break;
    aimp_wcscat(dir, cap, ws);
    CreateDirectoryW(dir, NULL);
}

static int write_file(const wchar_t *dir, const char *name, const void *p, uint32_t n)
{
    wchar_t path[MAX_PATH + 32], wn[XFER_NAME + 1];
    int i = 0;
    for (; i < XFER_NAME && name[i]; i++)
        wn[i] = (wchar_t)(uint8_t)name[i];
    wn[i] = 0;
    aimp_wcscpy(path, MAX_PATH + 32, dir);
    aimp_wcscat(path, MAX_PATH + 32, L"\\");
    aimp_wcscat(path, MAX_PATH + 32, wn);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    DWORD w = 0;
    BOOL ok = WriteFile(f, p, n, &w, NULL);
    CloseHandle(f);
    return ok && w == n;
}

static void client_finish(SaveSync *s, uint32_t status)
{
    s->done_status = status;
    s->done_left = 3;
    s->state = status ? ST_ERR : ST_DONE;
    xfree(s->got);
    s->got = NULL;
    xfree(s->blob);
    s->blob = NULL;
}

/* Todo llegó: descomprimir y verificar todo antes de tocar nada; después backup y escritura. */
static void client_apply(SaveSync *s)
{
    uint8_t *raw[XFER_MAX_FILES] = { 0 };
    uint32_t off = 0, status = 0;
    for (int i = 0; i < s->nfiles && !status; i++) {
        if (!(s->mask & (1u << i)))
            continue;
        XferEntry *e = &s->files[i];
        uint32_t cl;
        if (off + 5 > s->blob_len) {
            status = 2;
            break;
        }
        uint8_t method = s->blob[off];
        aimp_memcpy(&cl, s->blob + off + 1, 4);
        if (off + 5 + cl > s->blob_len || !(raw[i] = xalloc(e->size))) {
            status = 2;
            break;
        }
        const uint8_t *src = s->blob + off + 5;
        if (method == METHOD_RAW && cl == e->size)
            aimp_memcpy(raw[i], src, cl);
        else if (method != METHOD_LZ || !savesync_lz_decompress(src, cl, raw[i], e->size))
            status = 3;
        if (!status && savesync_fnv(raw[i], e->size) != e->hash) {
            log_printf("SAVESYNC: %s llegó con otro hash: no se escribe nada", e->name);
            status = 4;
        }
        off += 5 + cl;
    }
    if (!status) {
        wchar_t dir[MAX_PATH];
        backup_path(s, dir, MAX_PATH);
        int nb = 0;
        for (int i = 0; i < s->nfiles; i++) {
            if (!(s->mask & (1u << i)))
                continue;
            XferEntry *e = &s->files[i];
            int32_t cap = 16 * 1024 * 1024;
            uint8_t *old = xalloc((size_t)cap);
            int32_t r = old ? s->st.read(s->st.ctx, e->name, old, cap) : -1;
            if (r > 0) {
                if (!dir[0] || !write_file(dir, e->name, old, (uint32_t)r)) {
                    log_printf("SAVESYNC: no se pudo hacer el backup de %s: no se escribe nada", e->name);
                    status = 5;
                }
                nb++;
            }
            xfree(old);
            if (status)
                break;
        }
        if (!status) {
            char d8[MAX_PATH * 3];
            log_printf("SAVESYNC: backup de %d saves propios en %s", nb, aimp_utf8(d8, sizeof d8, dir));
            for (int i = 0; i < s->nfiles; i++) {
                if (!(s->mask & (1u << i)))
                    continue;
                XferEntry *e = &s->files[i];
                if (s->st.write(s->st.ctx, e->name, raw[i], (int32_t)e->size)) {
                    s->written |= 1u << i;
                } else {
                    log_printf("SAVESYNC: FileWrite de %s falló", e->name);
                    status = 6;
                }
            }
        }
    }
    for (int i = 0; i < XFER_MAX_FILES; i++)
        xfree(raw[i]);
    if (status)
        log_printf("SAVESYNC: la copia del save del host FALLÓ (código %u)", status);
    else
        log_printf("SAVESYNC: save del host copiado (%08x). Ahora \"Continue\" carga la partida del host", s->written);
    client_finish(s, status);
}

static void client_rx(SaveSync *s, const XferHdr *h, const uint8_t *p, int len, int64_t now)
{
    if (h->op == XFER_MANIFEST && s->state == ST_WAIT) {
        if (h->n > XFER_MAX_FILES || len < (int)(sizeof *h + h->n * sizeof(XferEntry)))
            return;
        s->xid = h->xid;
        s->nfiles = h->n;
        aimp_memcpy(s->files, p + sizeof *h, (size_t)h->n * sizeof(XferEntry));
        uint32_t mask = 0;
        char list[400];
        int n = 0;
        list[0] = 0;
        for (int i = 0; i < s->nfiles; i++) {
            XferEntry *e = &s->files[i];
            e->name[XFER_NAME - 1] = 0;
            int32_t got;
            uint32_t lh = local_hash(s, e->name, (int32_t)e->size, &got);
            if (got != (int32_t)e->size || lh != e->hash) {
                mask |= 1u << i;
                if (n < (int)sizeof list - 20)
                    n += aimp_snprintf(list + n, sizeof list - (size_t)n, " %s", e->name);
            }
        }
        s->mask = mask;
        log_printf("SAVESYNC: manifiesto del host: %d saves; distintos:%s", s->nfiles, mask ? list : " ninguno");
        if (!mask) {
            log_printf("SAVESYNC: ya tenés los mismos saves que el host");
            client_finish(s, 0);
            return;
        }
        s->state = ST_XFER;
        s->last_news = now;
        s->last_req = 0; /* manda el WANT ya */
        return;
    }
    if (h->xid != s->xid || s->state != ST_XFER)
        return;
    if (h->op == XFER_DATA && len >= (int)(sizeof(XferData) - XFER_CHUNK)) {
        const XferData *d = (const XferData *)p;
        if (len < (int)(sizeof(XferData) - XFER_CHUNK + d->len) || d->len > XFER_CHUNK)
            return;
        if (!s->got) {
            if (!d->total || d->blob_len > 64u * 1024 * 1024 || d->total != (d->blob_len + XFER_CHUNK - 1) / XFER_CHUNK)
                return;
            s->total = d->total;
            s->blob_len = d->blob_len;
            s->got = xalloc(s->total);
            s->blob = xalloc(s->blob_len);
            s->got_n = 0;
            if (!s->got || !s->blob) {
                client_finish(s, 1);
                return;
            }
            log_printf("SAVESYNC: recibiendo %u bytes en %u fragmentos", s->blob_len, s->total);
        }
        if (d->chunk >= s->total || d->total != s->total)
            return;
        uint32_t off = d->chunk * XFER_CHUNK;
        uint32_t want_len = s->blob_len - off < XFER_CHUNK ? s->blob_len - off : XFER_CHUNK;
        if (d->len != want_len)
            return;
        s->last_news = now;
        s->ack_due = 1;
        if (s->got[d->chunk])
            return;
        aimp_memcpy(s->blob + off, d->data, d->len);
        s->got[d->chunk] = 1;
        if (++s->got_n == s->total) {
            client_ack(s);
            client_apply(s);
        }
    }
}

static void rx(void *ctx, const uint8_t *p, int len, int64_t now)
{
    SaveSync *s = ctx;
    if (len < (int)sizeof(XferHdr))
        return;
    const XferHdr *h = (const XferHdr *)p;
    if (s->net->mode == 1)
        host_rx(s, h, p, len, now);
    else if (s->want)
        client_rx(s, h, p, len, now);
}

void savesync_tick(SaveSync *s, int64_t now, int64_t freq, uint32_t level)
{
    s->freq = freq;
    s->level = level;
    AimpNet *n = s->net;
    if (!n || !n->mode || !n->have_peer)
        return;
    if (n->mode == 1) {
        host_send(s, now);
        return;
    }
    if (!s->want)
        return;
    if (s->done_left > 0) {
        uint32_t e[2] = { s->done_status, s->written };
        send_simple(s, XFER_DONE, e, 8);
        s->done_left--;
    }
    if (s->state == ST_OFF && n->have_remote) {
        if (level != 0) {
            if (!s->warned_level)
                log_printf("SAVESYNC: el save del host se copia en el menú principal: salí al menú para traerlo");
            s->warned_level = 1;
            return;
        }
        s->state = ST_WAIT;
        s->last_req = 0;
        log_printf("SAVESYNC: pidiendo el save del host");
    }
    if (s->state == ST_WAIT && now - s->last_req >= ms_ticks(s, REQ_MS)) {
        send_simple(s, XFER_REQ, NULL, 0);
        s->last_req = now;
    }
    if (s->state == ST_XFER) {
        if (!s->got && now - s->last_req >= ms_ticks(s, 500)) {
            send_simple(s, XFER_WANT, &s->mask, 4);
            s->last_req = now;
        }
        if (s->got && s->ack_due && now - s->last_ack >= ms_ticks(s, ACK_MS)) {
            client_ack(s);
            s->ack_due = 0;
            s->last_ack = now;
        }
        if (now - s->last_news > ms_ticks(s, GIVEUP_MS)) {
            log_printf("SAVESYNC: la transferencia se cortó (%u de %u fragmentos): se vuelve a pedir", s->got_n, s->total);
            xfree(s->got);
            xfree(s->blob);
            s->got = s->blob = NULL;
            s->state = ST_WAIT;
            s->last_req = 0;
        }
    }
}

SaveSync *savesync_new(AimpNet *net, const SaveStore *st, int want, const wchar_t *backup_dir)
{
    SaveSync *s = xalloc(sizeof *s);
    if (!s)
        return NULL;
    s->net = net;
    s->st = *st;
    s->want = want;
    if (backup_dir)
        aimp_wcscpy(s->backup_dir, MAX_PATH, backup_dir);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    s->freq = f.QuadPart;
    net->xfer_rx = rx;
    net->xfer_ctx = s;
    return s;
}

int savesync_state(const SaveSync *s) { return s->state; }
uint32_t savesync_written_mask(const SaveSync *s) { return s->written; }
void savesync_test_drop(SaveSync *s, int every) { s->drop_every = every; }

/* ---- Steam (ISteamRemoteStorage v012, thiscall) ---- */
typedef void *(__cdecl *SteamRsFn)(void);
typedef uint8_t(__attribute__((thiscall)) * RsWriteFn)(void *, const char *, const void *, int32_t);
typedef int32_t(__attribute__((thiscall)) * RsReadFn)(void *, const char *, void *, int32_t);
typedef int32_t(__attribute__((thiscall)) * RsCountFn)(void *);
typedef const char *(__attribute__((thiscall)) * RsNameFn)(void *, int, int32_t *);
#define RS_WRITE 0
#define RS_READ 1
#define RS_COUNT 15
#define RS_NAME 16

static void *rs_iface(void)
{
    HMODULE h = GetModuleHandleW(L"steam_api.dll");
    SteamRsFn f = h ? (SteamRsFn)(void *)GetProcAddress(h, "SteamRemoteStorage") : NULL;
    return f ? f() : NULL;
}
static void *rs_fn(int i)
{
    void *rs = rs_iface();
    void **vt = rs ? *(void ***)rs : NULL;
    return vt ? vt[i] : NULL;
}
static int st_count(void *ctx)
{
    void *rs = rs_iface();
    return rs ? ((RsCountFn)rs_fn(RS_COUNT))(rs) : 0;
}
static int st_entry(void *ctx, int i, char *name, int cap, int32_t *size)
{
    void *rs = rs_iface();
    if (!rs)
        return 0;
    const char *nm = ((RsNameFn)rs_fn(RS_NAME))(rs, i, size);
    return nm && mem_cstr((uintptr_t)nm, name, (size_t)cap);
}
static int32_t st_read(void *ctx, const char *name, void *buf, int32_t cap)
{
    void *rs = rs_iface();
    return rs ? ((RsReadFn)rs_fn(RS_READ))(rs, name, buf, cap) : -1;
}
static int st_write(void *ctx, const char *name, const void *buf, int32_t n)
{
    void *rs = rs_iface();
    return rs && ((RsWriteFn)rs_fn(RS_WRITE))(rs, name, buf, n) != 0;
}

void savesync_install(AimpNet *net)
{
    if (!net || !net->mode)
        return;
    SaveStore st = { NULL, st_count, st_entry, st_read, st_write };
    int want = net->mode == 2 && aimp_env_int(L"AIMP_SAVE_SYNC", 0);
    g_game.net = net;
    g_game.st = st;
    g_game.want = want;
    net->xfer_rx = rx;
    net->xfer_ctx = &g_game;
    g_game_on = 1;
    if (net->mode == 1)
        log_printf("SAVESYNC: host: le pasa sus saves al cliente que los pida (AIMP_SAVE_SYNC=1 en el cliente)");
    else if (want)
        log_printf("SAVESYNC: AIMP_SAVE_SYNC=1: en el menú principal se copian los saves del host (con backup de los propios)");
}

void savesync_frame(int64_t now, int64_t freq, uint32_t level)
{
    if (!g_game_on)
        return;
    if (!g_game.freq) {
        /* la interfaz recién existe con Steam inicializado: el primer frame, una prueba de lectura */
        int c = st_count(NULL);
        log_printf("SAVESYNC: Steam Cloud: %d archivos (%s)", c, rs_iface() ? "interfaz OK" : "sin interfaz");
    }
    savesync_tick(&g_game, now, freq, level);
}
