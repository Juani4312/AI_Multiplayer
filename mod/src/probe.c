/* Sonda de estado del juego. Solo lee memoria; nunca escribe en el juego.
 *
 * Por frame: cuenta frames y tiempo de Run, lee el tamaño de la cola A y mira la
 * tecla de volcado. Cada AIMP_SAMPLE_MS toma una foto completa y loguea los cambios
 * ("EV"). Cada AIMP_STATUS_MS escribe un resumen ("ST"). Con F9 (AIMP_DUMP_KEY)
 * vuelca las estructuras en crudo para validar offsets.
 */
#include "probe.h"
#include "game.h"
#include "log.h"
#include "autopad.h"
#include "pins.h"
#include "trace.h"
#include "net.h"
#include "ghost.h"
#include "sync.h"
#include <tlhelp32.h>

typedef struct {
    uint32_t frame;
    double dt, dts, gtime;
    uint32_t root;
    uint32_t mm, mm_mode, mm_index, mm_state, mode_vt, mode_level;
    uint32_t levels, level_count, level_cur;
    char level_name[LEVEL_NAME_STRIDE + 1];
    uint32_t cm, np, nn, npend, pl, pl_vt, pl_def, def_type;
    uint8_t def1c, def1f, ctrl_act;
    int pl_ok;
    float pos[3], yaw;
    uint32_t ctrl;
    uint32_t cmgr, cam;
    int cam_ok;
    float cq[4], cp[3];
    uint32_t netz, netz_tr, netz_m0, netz_m1;
    uint32_t sess_g, sess_r, sess_prov;
    uint8_t sess_2c, sess_2d;
    uint32_t qa, qa_sink, qa_size, qa_data;
    uint32_t qa_g;    /* *DAT_01be1bc8: tiene que ser == qa */
    uint32_t qb_size, qb_data; /* cola B = buffer de la sesión */
} Snap;

static Snap g_prev, g_cur;
static int g_have_prev;
static int64_t g_freq, g_sample_ticks, g_status_ticks, g_last_sample, g_last_status;
static uint32_t g_frames, g_frames_at_status, g_first_frames;
static int64_t g_run_acc, g_run_max;
static uint32_t g_frames_period;
static uint32_t g_qa_max, g_qa_frames_nz, g_mask_changes;
static uint64_t g_qa_sum;
static uint32_t g_qb_max, g_qb_frames_nz;
static uint64_t g_qb_sum;
static int64_t g_trace_ticks, g_last_trace;
static int g_warned_qa, g_rpc_listed;
static uint32_t g_status_frame_game;
static float g_status_pos[3];
static int g_status_pos_ok;
static int g_mark_key, g_mark_was_down;
static uint32_t g_marks;
static int g_dump_key, g_key_was_down, g_levels_listed, g_warned_mm, g_warned_sess;
static int g_dump_requested;
static uint32_t g_dump_at_frame;
static uint32_t g_fast_state, g_fast_mode;
static AimpNet g_net; /* estado/modo del ModeManager en el frame anterior */
static int64_t g_autodump_at; /* 0 = ninguno pendiente */
static int g_autodumps_done;
typedef SHORT(WINAPI *GetAsyncKeyStateFn)(int);
static GetAsyncKeyStateFn g_GetAsyncKeyState;

static double ticks_ms(int64_t t) { return g_freq ? (double)t * 1000.0 / (double)g_freq : 0.0; }

static double aimp_sqrt(double x)
{
    double r;
    if (x <= 0.0)
        return 0.0;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
}

static const char *state_name(uint32_t s)
{
    switch (s) {
    case 0: return "siguiente";
    case 1: return "preparar";
    case 2: return "init";
    case 3: return "cargar";
    case 4: return "esperar";
    case 5: return "corriendo";
    case 6: return "6";
    case 7: return "salir-modo";
    case 0xFFFFFFFFu: return "salir";
    default: return "?";
    }
}

static const char *vt_mode_name(uint32_t vt)
{
    if (!vt)
        return "-";
    return vt == g_aimp_exe + AIMP_RVA_VT_LEVEL_MODE ? "modo-de-nivel" : "otra";
}

static const char *vt_chr_name(uint32_t vt)
{
    if (!vt)
        return "-";
    return vt == g_aimp_exe + AIMP_RVA_VT_CHARACTER ? "Character(ok)" : "DISTINTA";
}

static uint32_t rva(uint32_t p) { return p ? p - (uint32_t)g_aimp_exe : 0; }

static void snap_take(Snap *s)
{
    const uintptr_t b = g_aimp_exe;
    aimp_memset(s, 0, sizeof *s);
    /* Globales de .data: lectura directa (la sección siempre está mapeada). */
    s->frame = *(volatile uint32_t *)(b + AIMP_RVA_FRAME_COUNTER);
    s->dt = *(volatile double *)(b + AIMP_RVA_DT_REAL);
    s->dts = *(volatile double *)(b + AIMP_RVA_DT_SCALED);
    s->gtime = *(volatile double *)(b + AIMP_RVA_GAME_TIME);
    s->root = *(volatile uint32_t *)(b + AIMP_RVA_G_ROOT);
    s->sess_g = *(volatile uint32_t *)(b + AIMP_RVA_NET_SESSION);
    s->qa_g = *(volatile uint32_t *)(b + AIMP_RVA_QUEUE_A);
    uint32_t llpp = *(volatile uint32_t *)(b + AIMP_RVA_LEVEL_LIST_PP);

    /* Heap: siempre con lecturas seguras. */
    if (llpp && mem_u32(llpp, &s->levels) && s->levels) {
        mem_u32(s->levels + LEVELS_COUNT, &s->level_count);
        mem_u32(s->levels + LEVELS_CURRENT, &s->level_cur);
        if (s->level_cur < LEVELS_MAX)
            mem_cstr(s->levels + s->level_cur * LEVEL_NAME_STRIDE, s->level_name, sizeof s->level_name);
    }
    if (s->sess_g) {
        mem_u32(s->sess_g + SESSION_PROVIDER, &s->sess_prov);
        mem_u8(s->sess_g + SESSION_FLAG_2C, &s->sess_2c);
        mem_u8(s->sess_g + SESSION_FLAG_2D, &s->sess_2d);
        mem_u32(s->sess_g + SESSION_QSIZE, &s->qb_size);
        mem_u32(s->sess_g + SESSION_QDATA, &s->qb_data);
    }
    if (!s->root)
        return;
    const uint32_t r = s->root;
    if (mem_u32(r + ROOT_MODEMGR, &s->mm) && s->mm) {
        mem_u32(s->mm + MM_MODE, &s->mm_mode);
        mem_u32(s->mm + MM_INDEX, &s->mm_index);
        mem_u32(s->mm + MM_STATE, &s->mm_state);
        if (s->mm_mode) {
            mem_u32(s->mm_mode, &s->mode_vt);
            mem_u32(s->mm_mode + MODE_LEVEL_INDEX, &s->mode_level);
        }
    }
    if (mem_u32(r + ROOT_CHARMGR, &s->cm) && s->cm) {
        uint32_t arr = 0;
        mem_u32(s->cm + CM_NPLAYERS, &s->np);
        mem_u32(s->cm + CM_NNPCS, &s->nn);
        mem_u32(s->cm + CM_NPENDING, &s->npend);
        if (s->np && mem_u32(s->cm + CM_PLAYERS, &arr) && arr && mem_u32(arr, &s->pl) && s->pl) {
            mem_u32(s->pl, &s->pl_vt);
            s->pl_ok = mem_read(s->pl + CHR_POS, s->pos, sizeof s->pos) && mem_f32(s->pl + CHR_YAW, &s->yaw);
            if (mem_u32(s->pl + CHR_DEF, &s->pl_def) && s->pl_def) {
                mem_u32(s->pl_def + DEF_TYPE, &s->def_type);
                mem_u8(s->pl_def + DEF_FLAG_1C, &s->def1c);
                mem_u8(s->pl_def + DEF_FLAG_1F, &s->def1f);
            }
            if (mem_u32(s->pl + CHR_CTRL, &s->ctrl) && s->ctrl)
                mem_u8(s->ctrl + CTRL_ACTIVE, &s->ctrl_act);
        }
    }
    if (mem_u32(r + ROOT_CAMMGR, &s->cmgr) && s->cmgr && mem_u32(s->cmgr + CAMMGR_ACTIVE, &s->cam) && s->cam)
        s->cam_ok = mem_read(s->cam + CAM_QUAT, s->cq, sizeof s->cq) && mem_read(s->cam + CAM_POS, s->cp, sizeof s->cp);
    if (mem_u32(r + ROOT_NETZ, &s->netz) && s->netz) {
        mem_u32(s->netz + NETZ_TRANSPORT, &s->netz_tr);
        mem_u32(s->netz + NETZ_MASK, &s->netz_m0);
        mem_u32(s->netz + NETZ_MASK + 4, &s->netz_m1);
    }
    mem_u32(r + ROOT_SESSION, &s->sess_r);
    if (mem_u32(r + ROOT_QUEUE_A, &s->qa) && s->qa) {
        mem_u32(s->qa + QA_SINK, &s->qa_sink);
        mem_u32(s->qa + QA_SIZE, &s->qa_size);
        mem_u32(s->qa + QA_DATA, &s->qa_data);
    }
}

static void list_levels(const Snap *s)
{
    char name[LEVEL_NAME_STRIDE + 1];
    uint32_t n = s->level_count < LEVELS_MAX ? s->level_count : LEVELS_MAX;
    log_printf("EV lista de niveles en 0x%08x: %u entradas (actual %u)", s->levels, s->level_count, s->level_cur);
    for (uint32_t i = 0; i < n; i++) {
        if (mem_cstr(s->levels + i * LEVEL_NAME_STRIDE, name, sizeof name))
            log_printf("    nivel %3u: %s", i, name);
    }
}

/* Experimento 3.5: recorre una lista estática de endpoints RPC (solo lectura). */
static void log_rpc_list(const char *name, uint32_t head_rva)
{
    uint32_t ep = *(volatile uint32_t *)(g_aimp_exe + head_rva), n = 0;
    log_printf("RPC lista %s (global rva 0x%x): cabeza 0x%08x", name, head_rva, ep);
    while (ep && n < 2048) {
        uint32_t next = 0, fn = 0, ctx = 0;
        uint16_t type = 0;
        if (!mem_u32(ep + EP_NEXT, &next) || !mem_u32(ep + EP_FN, &fn) || !mem_u32(ep + EP_CTX, &ctx) ||
            !mem_read(ep + EP_TYPE, &type, 2)) {
            log_printf("RPC   %s: endpoint ilegible en 0x%08x", name, ep);
            break;
        }
        log_printf("RPC   %s tipo 0x%04x handler rva 0x%x ctx 0x%08x endpoint rva 0x%x", name, type, rva(fn), ctx,
                   rva(ep));
        ep = next;
        n++;
    }
    log_printf("RPC lista %s: %u endpoints", name, n);
}

static void snap_events(const Snap *a, const Snap *b, uintptr_t mm_this)
{
#define CHG(f) (a->f != b->f)
    if (CHG(root))
        log_printf("EV g_root 0x%08x -> 0x%08x", a->root, b->root);
    if (CHG(mm))
        log_printf("EV ModeManager 0x%08x -> 0x%08x", a->mm, b->mm);
    /* Los cambios de estado se loguean por frame en probe_after_run. */
    if (CHG(mm_mode) || CHG(mm_index) || CHG(mode_vt))
        log_printf("EV modo 0x%08x (idx %d) -> 0x%08x (idx %d, vtable rva 0x%x = %s, nivel del modo %d)",
                   a->mm_mode, (int)a->mm_index, b->mm_mode, (int)b->mm_index, rva(b->mode_vt),
                   vt_mode_name(b->mode_vt), (int)b->mode_level);
    if (CHG(levels) || CHG(level_count))
        log_printf("EV lista de niveles 0x%08x (n=%u) -> 0x%08x (n=%u)", a->levels, a->level_count, b->levels,
                   b->level_count);
    if (CHG(level_cur) || aimp_memcmp(a->level_name, b->level_name, sizeof a->level_name) != 0)
        log_printf("EV nivel actual %d \"%s\" -> %d \"%s\"", (int)a->level_cur, a->level_name, (int)b->level_cur,
                   b->level_name);
    if (!g_rpc_listed && b->qa) {
        uint32_t na = 0, ns = 0;
        mem_u32(b->qa + QA_NHANDLERS, &na);
        if (b->sess_g)
            mem_u32(b->sess_g + SESSION_NHANDLERS, &ns);
        if (na) { /* RegisterAll ya corrió */
            g_rpc_listed = 1;
            log_printf("EV tablas de handlers: cola A %u, sesión %u", na, ns);
            log_rpc_list("A", AIMP_RVA_RPC_LIST_A);
            log_rpc_list("sesión", AIMP_RVA_RPC_LIST_S);
        }
    }
    if (!g_levels_listed && b->levels && b->level_count && b->level_count <= LEVELS_MAX) {
        g_levels_listed = 1;
        list_levels(b);
    }
    if (CHG(cm))
        log_printf("EV CharacterManager 0x%08x -> 0x%08x", a->cm, b->cm);
    if (CHG(np) || CHG(nn) || CHG(npend))
        log_printf("EV personajes: jugadores %u -> %u, NPCs %u -> %u, pendientes %u -> %u", a->np, b->np, a->nn,
                   b->nn, a->npend, b->npend);
    if (CHG(pl) || CHG(pl_vt))
        log_printf("EV jugador 0: 0x%08x -> 0x%08x (vtable rva 0x%x = %s; def 0x%08x tipo %u flags 1c=%u 1f=%u)",
                   a->pl, b->pl, rva(b->pl_vt), vt_chr_name(b->pl_vt), b->pl_def, b->def_type, b->def1c,
                   b->def1f);
    if (CHG(ctrl) || CHG(ctrl_act))
        log_printf("EV controlador de jugador 0x%08x (activo %u) -> 0x%08x (activo %u)", a->ctrl, a->ctrl_act,
                   b->ctrl, b->ctrl_act);
    if (CHG(cmgr) || CHG(cam))
        log_printf("EV cámara: gestor 0x%08x -> 0x%08x, activa 0x%08x -> 0x%08x", a->cmgr, b->cmgr, a->cam, b->cam);
    if (CHG(netz))
        log_printf("EV NETZ 0x%08x -> 0x%08x", a->netz, b->netz);
    if (CHG(netz_tr))
        log_printf("EV NETZ transporte 0x%08x -> 0x%08x%s", a->netz_tr, b->netz_tr,
                   b->netz_tr ? "  <-- INESPERADO: hay transporte" : "");
    if (CHG(netz_m0) || CHG(netz_m1))
        g_mask_changes++;
    if (CHG(sess_g) || CHG(sess_r))
        log_printf("EV sesión: *DAT_01be1bd4 0x%08x -> 0x%08x, g_root+0x54 0x%08x -> 0x%08x", a->sess_g, b->sess_g,
                   a->sess_r, b->sess_r);
    if (CHG(sess_prov))
        log_printf("EV sesión: proveedor 0x%08x -> 0x%08x%s", a->sess_prov, b->sess_prov,
                   b->sess_prov ? "  <-- INESPERADO: hay proveedor" : "");
    if (CHG(sess_2c) || CHG(sess_2d))
        log_printf("EV sesión: flags +2C %u -> %u, +2D %u -> %u", a->sess_2c, b->sess_2c, a->sess_2d, b->sess_2d);
    if (CHG(qa) || CHG(qa_sink))
        log_printf("EV cola A 0x%08x (sumidero 0x%08x) -> 0x%08x (sumidero 0x%08x)%s", a->qa, a->qa_sink, b->qa,
                   b->qa_sink, b->qa_sink ? "  <-- INESPERADO: hay sumidero" : "");
    if (!g_warned_qa && b->qa && b->qa_g && b->qa != b->qa_g) {
        g_warned_qa = 1;
        log_printf("EV OJO: g_root+0x50 (0x%08x) != *DAT_01be1bc8 (0x%08x): los productores usan otra cola", b->qa, b->qa_g);
    }
    if (!g_warned_sess && b->root && b->sess_r && b->sess_r != b->sess_g) {
        g_warned_sess = 1;
        log_printf("EV OJO: g_root+0x54 (0x%08x) != *DAT_01be1bd4 (0x%08x)", b->sess_r, b->sess_g);
    }
    if (!g_warned_mm && b->mm && mm_this && b->mm != mm_this) {
        g_warned_mm = 1;
        log_printf("EV OJO: ECX de ModeManager::Run (0x%08x) != [g_root+0x3C] (0x%08x)", (unsigned)mm_this, b->mm);
    }
#undef CHG
}

static void status(const Snap *s, int64_t period_ticks)
{
    double secs = (double)period_ticks / (double)g_freq;
    uint32_t df = g_frames - g_frames_at_status;
    double fps = secs > 0.0 ? (double)df / secs : 0.0;
    double avg = g_frames_period ? ticks_ms(g_run_acc) / (double)g_frames_period : 0.0;
    log_printf("ST frames +%u (contador del juego %u, +%d) fps=%.1f Run prom=%.2f ms max=%.2f ms dt=%.4f dts=%.4f t_juego=%.1f",
               df, s->frame, (int)(s->frame - g_status_frame_game), fps, avg, ticks_ms(g_run_max), s->dt, s->dts,
               s->gtime);
    log_printf("   modo: estado %d (%s) idx %d modo 0x%08x (%s, nivel %d) | nivel %d/%u \"%s\"", (int)s->mm_state,
               state_name(s->mm_state), (int)s->mm_index, s->mm_mode, vt_mode_name(s->mode_vt), (int)s->mode_level,
               (int)s->level_cur, s->level_count, s->level_name);
    log_printf("   personajes: jugadores %u, NPCs %u, pendientes %u | J0 0x%08x %s def 0x%08x tipo %u 1c=%u 1f=%u ctrl 0x%08x activo %u",
               s->np, s->nn, s->npend, s->pl, vt_chr_name(s->pl_vt), s->pl_def, s->def_type, s->def1c, s->def1f,
               s->ctrl, s->ctrl_act);
    if (s->pl_ok) {
        double d = 0.0;
        if (g_status_pos_ok) {
            double dx = s->pos[0] - g_status_pos[0], dy = s->pos[1] - g_status_pos[1], dz = s->pos[2] - g_status_pos[2];
            d = aimp_sqrt(dx * dx + dy * dy + dz * dz);
        }
        log_printf("   J0 pos=(%.3f, %.3f, %.3f) yaw=%.4f rad (%.1f grados) | se movió %.3f desde el ST anterior",
                   (double)s->pos[0], (double)s->pos[1], (double)s->pos[2], (double)s->yaw,
                   (double)s->yaw * 57.2957795, d);
    }
    if (s->cam_ok) {
        double qn = (double)s->cq[0] * s->cq[0] + (double)s->cq[1] * s->cq[1] + (double)s->cq[2] * s->cq[2] +
                    (double)s->cq[3] * s->cq[3];
        log_printf("   cam 0x%08x pos=(%.3f, %.3f, %.3f) q=(%.4f, %.4f, %.4f, %.4f) |q|^2=%.4f%s", s->cam,
                   (double)s->cp[0], (double)s->cp[1], (double)s->cp[2], (double)s->cq[0], (double)s->cq[1],
                   (double)s->cq[2], (double)s->cq[3], qn, "");
        if (s->pl_ok)
            log_printf("   cam - J0 = (%.3f, %.3f, %.3f)", (double)(s->cp[0] - s->pos[0]),
                       (double)(s->cp[1] - s->pos[1]), (double)(s->cp[2] - s->pos[2]));
    }
    log_printf("   red: NETZ 0x%08x transporte 0x%08x máscara %08x:%08x (%u cambios) | sesión 0x%08x proveedor 0x%08x +2C=%u +2D=%u",
               s->netz, s->netz_tr, s->netz_m1, s->netz_m0, g_mask_changes, s->sess_g, s->sess_prov, s->sess_2c,
               s->sess_2d);
    log_printf("   cola A 0x%08x (global 0x%08x) sumidero 0x%08x: tamaño tras Run máx %u, frames con datos %u/%u, suma %llu bytes",
               s->qa, s->qa_g, s->qa_sink, g_qa_max, g_qa_frames_nz, g_frames_period, (unsigned long long)g_qa_sum);
    log_printf("   cola B (sesión, se descarta sin proveedor): tamaño tras Run máx %u, frames con datos %u/%u, suma %llu bytes",
               g_qb_max, g_qb_frames_nz, g_frames_period, (unsigned long long)g_qb_sum);
    g_frames_at_status = g_frames;
    g_status_frame_game = s->frame;
    g_run_acc = g_run_max = 0;
    g_frames_period = 0;
    g_qa_max = g_qa_frames_nz = g_mask_changes = 0;
    g_qa_sum = 0;
    g_qb_max = g_qb_frames_nz = 0;
    g_qb_sum = 0;
    g_status_pos_ok = s->pl_ok;
    if (s->pl_ok)
        aimp_memcpy(g_status_pos, s->pos, sizeof g_status_pos);
}

static void dump_character_line(const char *tag, uint32_t i, uint32_t c)
{
    uint32_t vt = 0, def = 0, type = 0, ctrl = 0;
    uint8_t f1c = 0, f1f = 0;
    float pos[3] = { 0 }, yaw = 0;
    char name[65];
    mem_u32(c, &vt);
    mem_read(c + CHR_POS, pos, sizeof pos);
    mem_f32(c + CHR_YAW, &yaw);
    if (mem_u32(c + CHR_DEF, &def) && def) {
        mem_u32(def + DEF_TYPE, &type);
        mem_u8(def + DEF_FLAG_1C, &f1c);
        mem_u8(def + DEF_FLAG_1F, &f1f);
    }
    mem_u32(c + CHR_CTRL, &ctrl);
    if (!mem_cstr(c + 0x10, name, sizeof name)) /* +0x10: nombre de la instancia (hallazgo 03) */
        name[0] = 0;
    log_printf("    %s[%u] 0x%08x vt rva 0x%x %s def 0x%08x tipo %u 1c=%u 1f=%u ctrl 0x%08x pos=(%.2f, %.2f, %.2f) yaw=%.3f \"%s\"",
               tag, i, c, rva(vt), vt_chr_name(vt), def, type, f1c, f1f, ctrl, (double)pos[0], (double)pos[1],
               (double)pos[2], (double)yaw, name);
}

static void dump_list(const char *tag, uint32_t arr_field, uint32_t count, uint32_t max)
{
    uint32_t arr = 0;
    if (!mem_u32(arr_field, &arr) || !arr)
        return;
    for (uint32_t i = 0; i < count && i < max; i++) {
        uint32_t c = 0;
        if (mem_u32(arr + 4 * i, &c) && c)
            dump_character_line(tag, i, c);
    }
}

static void dump_detail(const Snap *s)
{
    log_printf("==== volcado: frame del juego %u ====", s->frame);
    if (s->root)
        log_dump32("g_root", s->root, 0xC0);
    if (s->mm)
        log_dump32("ModeManager", s->mm, 0x60);
    if (s->mm_mode)
        log_dump32("modo actual", s->mm_mode, 0x20);
    if (s->cm) {
        log_dump32("CharacterManager", s->cm, 0x180);
        log_printf("  jugadores (%u):", s->np);
        dump_list("J", s->cm + CM_PLAYERS, s->np, 8);
        log_printf("  NPCs (%u):", s->nn);
        dump_list("N", s->cm + CM_NPCS, s->nn, 64);
    }
    if (s->pl) {
        log_dump32("jugador 0 (Character)", s->pl, 0x380);
        if (s->pl_def)
            log_dump32("definición del jugador", s->pl_def, 0x40);
        if (s->ctrl)
            log_dump32("controlador de jugador", s->ctrl, 0x60);
    }
    if (s->cmgr)
        log_dump32("gestor de cámara", s->cmgr, 0x220);
    if (s->cam)
        log_dump32("cámara activa", s->cam, 0x60);
    if (s->netz) {
        log_dump32("NETZ +0x000", s->netz, 0x20);
        log_dump32("NETZ +0x690", s->netz + 0x690, 0x10);
    }
    if (s->qa) {
        log_dump32("cola A", s->qa, 0x40);
        if (s->qa_data && s->qa_size)
            log_hexdump("cola A: buffer", s->qa_data, s->qa_size < 0x80 ? s->qa_size : 0x80);
    }
    if (s->sess_g) {
        log_dump32("sesión de red", s->sess_g, 0x40);
        if (s->qb_data && s->qb_size)
            log_hexdump("cola B: buffer", s->qb_data, s->qb_size < 0x80 ? s->qb_size : 0x80);
    }
    trace_report_totals("volcado");
    pins_report_totals("volcado");
    log_printf("==== fin del volcado ====");
}

static void log_modules(void)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    MODULEENTRY32W me;
    char p8[MAX_PATH * 3];
    if (snap == INVALID_HANDLE_VALUE) {
        log_printf("módulos: CreateToolhelp32Snapshot falló (%u)", (unsigned)GetLastError());
        return;
    }
    me.dwSize = sizeof me;
    log_printf("módulos cargados:");
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me))
        log_printf("    0x%08x %8x %s", (unsigned)(uintptr_t)me.modBaseAddr, (unsigned)me.modBaseSize,
                   aimp_utf8(p8, sizeof p8, me.szExePath));
    CloseHandle(snap);
}

void probe_init(void)
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_freq = f.QuadPart ? f.QuadPart : 1;
    int sample_ms = aimp_env_int(L"AIMP_SAMPLE_MS", 250);
    int status_ms = aimp_env_int(L"AIMP_STATUS_MS", 1000);
    if (sample_ms < 16)
        sample_ms = 16;
    if (status_ms < sample_ms)
        status_ms = sample_ms;
    g_sample_ticks = g_freq * sample_ms / 1000;
    g_status_ticks = g_freq * status_ms / 1000;
    g_dump_key = aimp_env_int(L"AIMP_DUMP_KEY", VK_F9);
    g_mark_key = aimp_env_int(L"AIMP_MARK_KEY", VK_F8);
    g_first_frames = (uint32_t)aimp_env_int(L"AIMP_FIRST_FRAMES", 3);
    g_dump_at_frame = (uint32_t)aimp_env_int(L"AIMP_DUMP_AT_FRAME", 0);
    int trace_ms = aimp_env_int(L"AIMP_TRACE_MS", 5000);
    g_trace_ticks = g_freq * (trace_ms < 250 ? 250 : trace_ms) / 1000;
    net_init(&g_net, g_freq);
    log_printf("sonda: muestra cada %d ms, resumen cada %d ms, tecla de volcado 0x%02x, tecla de marca 0x%02x, primeros %u frames al detalle",
               sample_ms, status_ms, g_dump_key, g_mark_key, g_first_frames);
}

void probe_request_dump(void) { g_dump_requested = 1; }

AimpNet *probe_net(void) { return &g_net; }

/* MARCA n en el log con la posición del jugador 0 (tecla F8 o AUTOPAD). */
void probe_mark(const char *why)
{
    float pos[3] = { 0 }, yaw = 0;
    if (g_cur.pl) {
        mem_read(g_cur.pl + CHR_POS, pos, sizeof pos);
        mem_f32(g_cur.pl + CHR_YAW, &yaw);
    }
    log_printf("MARCA %u (frame %u, contador del juego %u) J0 pos=(%.2f, %.2f, %.2f) yaw=%.3f%s%s", ++g_marks, g_frames,
               *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_FRAME_COUNTER), (double)pos[0], (double)pos[1], (double)pos[2],
               (double)yaw, why ? " " : "", why ? why : "");
}

void probe_after_run(uintptr_t mm_this, uint32_t ret, int64_t run_ticks, int64_t now)
{
    g_frames++;
    g_frames_period++;
    g_run_acc += run_ticks;
    if (run_ticks > g_run_max)
        g_run_max = run_ticks;

    if (g_frames == 1) {
        log_printf("FASE 1: primer frame dentro de ModeManager::Run (this=0x%08x)", (unsigned)mm_this);
        log_modules();
        HMODULE u = GetModuleHandleW(L"user32.dll");
        if (u)
            g_GetAsyncKeyState = (GetAsyncKeyStateFn)(void *)GetProcAddress(u, "GetAsyncKeyState");
        g_last_sample = g_last_status = now - g_status_ticks; /* foto y resumen ya */
    }
    if (g_frames <= g_first_frames)
        log_printf("frame %u: Run=%.3f ms ret=%u contador del juego %u", g_frames, ticks_ms(run_ticks),
                   (unsigned)(ret & 0xFF), *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_FRAME_COUNTER));
    if (ticks_ms(run_ticks) > 500.0)
        log_printf("Run tardó %.2f s (¿carga?)", ticks_ms(run_ticks) / 1000.0);

    /* Cola A: tamaño pendiente después del frame (el flush corre dentro de Run). */
    if (g_cur.qa) {
        uint32_t sz = 0;
        if (mem_u32(g_cur.qa + QA_SIZE, &sz)) {
            if (sz > g_qa_max)
                g_qa_max = sz;
            if (sz)
                g_qa_frames_nz++;
            g_qa_sum += sz;
        }
    }
    if (g_cur.sess_g) {
        uint32_t sz = 0;
        if (mem_u32(g_cur.sess_g + SESSION_QSIZE, &sz)) {
            if (sz > g_qb_max)
                g_qb_max = sz;
            if (sz)
                g_qb_frames_nz++;
            g_qb_sum += sz;
        }
    }

    /* Por frame: estado y modo del ModeManager (this de Run). Las cargas pasan
     * adentro de una sola llamada a Run, así que acá se ven las transiciones que
     * quedan visibles entre frames. */
    uint32_t st = 0, md = 0;
    mem_u32(mm_this + MM_STATE, &st);
    mem_u32(mm_this + MM_MODE, &md);
    int state_changed = g_frames > 1 && (st != g_fast_state || md != g_fast_mode);
    if (state_changed)
        log_printf("EV frame %u: ModeManager estado %d (%s) -> %d (%s), modo 0x%08x -> 0x%08x", g_frames,
                   (int)g_fast_state, state_name(g_fast_state), (int)st, state_name(st), g_fast_mode, md);
    g_fast_state = st;
    g_fast_mode = md;

    int status_due = now - g_last_status >= g_status_ticks;
    int take = state_changed || status_due || now - g_last_sample >= g_sample_ticks;
    int dump = g_dump_requested || (g_dump_at_frame && g_frames == g_dump_at_frame);
    const char *why = g_dump_requested ? "pedido" : "AIMP_DUMP_AT_FRAME";
    if (g_GetAsyncKeyState && g_dump_key > 0) {
        int down = (g_GetAsyncKeyState(g_dump_key) & 0x8000) != 0;
        if (down && !g_key_was_down) {
            dump = 1;
            why = "tecla";
        }
        g_key_was_down = down;
    }
    if (g_GetAsyncKeyState && g_mark_key > 0) {
        int down = (g_GetAsyncKeyState(g_mark_key) & 0x8000) != 0;
        if (down && !g_mark_was_down) {
            probe_mark(NULL); /* marca liviana para cruzar acciones del usuario con el log */
        }
        g_mark_was_down = down;
    }
    if (g_autodump_at && now >= g_autodump_at) {
        dump = 1;
        why = "automático, 5 s después de aparecer el jugador";
        g_autodump_at = 0;
    }
    g_dump_requested = 0;
    if (take || dump) {
        g_prev = g_cur;
        snap_take(&g_cur);
        if (g_have_prev)
            snap_events(&g_prev, &g_cur, mm_this);
        else
            log_printf("EV primera foto: g_root 0x%08x ModeManager 0x%08x estado %d", g_cur.root, g_cur.mm,
                       (int)g_cur.mm_state);
        /* Volcados automáticos: cuando aparece el jugador 0 y 5 s después (2 veces por
         * sesión como mucho), para tener los offsets aunque nadie apriete la tecla. */
        if (g_cur.pl && g_cur.pl != g_prev.pl && g_cur.pl_ok && g_autodumps_done < 2) {
            g_autodumps_done++;
            dump = 1;
            why = "automático, apareció el jugador 0";
            g_autodump_at = now + g_freq * 5;
        }
        g_have_prev = 1;
        g_last_sample = now;
    }
    if (dump) {
        log_printf("volcado (%s)", why);
        dump_detail(&g_cur);
    }
    /* Fase 3: red (si está activada). El paquete se arma solo cuando toca enviar. */
    net_poll(&g_net, now);
    /* Fase 5 (experimental, apagada por defecto): usos remotos que llegaron y usos locales por mandar. */
    sync_frame(&g_net, g_cur.level_cur);
    if (net_due(&g_net, now)) {
        AimpPkt lp;
        aimp_memset(&lp, 0, sizeof lp);
        lp.level = g_cur.level_cur;
        lp.sim_frame = *(volatile uint32_t *)(g_aimp_exe + AIMP_RVA_FRAME_COUNTER);
        if (g_cur.pl && g_cur.pl_ok && mem_read(g_cur.pl + CHR_POS, lp.pos, sizeof lp.pos) &&
            mem_f32(g_cur.pl + CHR_YAW, &lp.yaw)) {
            mem_read(g_cur.pl + CHR_ID, lp.char_id, sizeof lp.char_id);
            lp.flags |= AIMP_FLAG_PLAYER;
        }
        if (g_cur.cam) {
            mem_read(g_cur.cam + CAM_QUAT, lp.cam_q, sizeof lp.cam_q);
            mem_read(g_cur.cam + CAM_POS, lp.cam_pos, sizeof lp.cam_pos);
        }
        net_send(&g_net, now, &lp);
    }
    /* Fase 4 (experimental, apagada por defecto): el avatar del otro jugador escribe en
     * el juego; vive en ghost.c para que esta sonda siga siendo de solo lectura. */
    ghost_frame(now, &g_net, g_cur.level_cur);
    /* Pruebas sin nadie al teclado (AIMP_AUTOPAD): mando virtual con guion. */
    autopad_frame(now, g_freq, g_cur.pl && g_cur.pl_ok);
    if (status_due) {
        status(&g_cur, now - g_last_status);
        net_status(&g_net, now);
        g_last_status = now;
    }
    if ((trace_enabled() || pins_enabled()) && now - g_last_trace >= g_trace_ticks) {
        trace_report_period();
        pins_report_period();
        g_last_trace = now;
    }
    if ((ret & 0xFF) == 0) {
        log_printf("ModeManager::Run devolvió 0: el juego sale del loop principal (frames vistos %u)", g_frames);
        trace_report_totals("salida");
        pins_report_totals("salida");
        net_close(&g_net);
    }
}
