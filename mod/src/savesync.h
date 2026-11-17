#ifndef AIMP_SAVESYNC_H
#define AIMP_SAVESYNC_H
#include "aimp.h"
#include "net.h"

/* Copia del save del host al cliente (2026-11-17, work/DESIGN_SESSION.md punto 1.2).
 *
 * El juego lee y escribe sus saves (*.ais: MSSVnn, CHSV00/01, PROGRESSION) con ISteamRemoteStorage v012
 * (FileRead vt+4, GetFileCount vt+0x3C, GetFileNameAndSize vt+0x40; thiscall; NOTES 2026-11-17). El mod usa la
 * misma interfaz, así que anda igual con Steam Cloud y con Steam sin conexión.
 *
 * Flujo (canal confiable propio, paquetes AIMP_PKT_XFER):
 *   1. el cliente (AIMP_NET=join con AIMP_SAVE_SYNC=1), en el menú principal, pide el manifiesto (REQ);
 *   2. el host manda nombre, tamaño y hash de cada .ais (MANIFEST);
 *   3. el cliente compara con los suyos y pide los distintos (WANT, máscara de bits);
 *   4. el host los lee, los comprime (LZ propio: el LZNT1 de Wine no comprime; si no achica, crudos) y los manda
 *      en fragmentos de 1 KB con ventana de 256, ACK con bitmap de la ventana y reenvío (DATA/ACK);
 *   5. el cliente verifica tamaño y hash, guarda un backup de sus archivos en
 *      %LOCALAPPDATA%\AI_Multiplayer\save_backup\<fecha>\ y escribe los del host (FileWrite); avisa (DONE).
 * Después, en el cliente, "Continue" carga la partida del host.
 *
 * El host siempre sirve (solo lee). El cliente solo escribe con AIMP_SAVE_SYNC=1 y en el menú (nivel 0). */

#define XFER_REQ 1
#define XFER_MANIFEST 2
#define XFER_WANT 3
#define XFER_DATA 4
#define XFER_ACK 5
#define XFER_DONE 6

#define XFER_MAX_FILES 32
#define XFER_NAME 16
#define XFER_CHUNK 1024 /* datos por paquete: con los encabezados queda bajo el MTU de Tailscale (1280) */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t type; /* AIMP_PKT_XFER */
    uint8_t op;   /* XFER_* */
    uint8_t n;    /* MANIFEST: cantidad de archivos */
    uint32_t xid; /* id de la transferencia (lo elige el host en cada MANIFEST) */
} XferHdr;        /* 12 bytes */

typedef struct {
    char name[XFER_NAME];
    uint32_t size;
    uint32_t hash; /* FNV-1a de 32 bits del contenido */
} XferEntry;

typedef struct {
    XferHdr h;
    uint32_t chunk, total, blob_len;
    uint16_t len;
    uint8_t data[XFER_CHUNK];
} XferData;
#pragma pack(pop)

/* Dónde están los saves: Steam (en el juego) o memoria (tests). */
typedef struct {
    void *ctx;
    int (*count)(void *ctx);
    /* nombre y tamaño del archivo i; 0 si no hay */
    int (*entry)(void *ctx, int i, char *name, int cap, int32_t *size);
    /* bytes leídos (<= 0 si no existe) */
    int32_t (*read)(void *ctx, const char *name, void *buf, int32_t cap);
    /* 1 si se escribió */
    int (*write)(void *ctx, const char *name, const void *buf, int32_t n);
} SaveStore;

typedef struct SaveSync SaveSync;

/* El del juego: lee la config (AIMP_SAVE_SYNC) y se engancha a la red. */
void savesync_install(AimpNet *net);
void savesync_frame(int64_t now, int64_t freq, uint32_t level);

/* Para tests: una instancia por extremo. want = el cliente pide el save; backup_dir puede ser NULL. */
SaveSync *savesync_new(AimpNet *net, const SaveStore *st, int want, const wchar_t *backup_dir);
void savesync_tick(SaveSync *s, int64_t now, int64_t freq, uint32_t level);
/* 0 nada, 1 esperando, 2 transfiriendo, 3 listo (copiado o ya igual), 4 error */
int savesync_state(const SaveSync *s);
uint32_t savesync_written_mask(const SaveSync *s);
/* Tests: descartar 1 de cada n paquetes de datos salientes (0 = ninguno). */
void savesync_test_drop(SaveSync *s, int every);

uint32_t savesync_fnv(const void *p, size_t n);
/* Compresión del canal (LZ voraz, formato de bloque de LZ4). 0 si no entra en cap o no achica. */
uint32_t savesync_lz_compress(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t cap);
int savesync_lz_decompress(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t raw);

#endif
