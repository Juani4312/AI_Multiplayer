/* Capa mínima sobre Scaleform GFx 4 (ver gfx.h, con la evidencia de cada dirección). */
#include "gfx.h"
#include "log.h"

_Static_assert(sizeof(GfxValue) == 0x18, "GFx::Value mide 0x18");

#define RVA_SCREENS 0x0134A78Cu      /* VA 0x174A78C: administrador de pantallas */
#define RVA_FIND_SCREEN 0x00410420u  /* FUN_00810420 */
#define RVA_HEAP_ASSIGN 0x008E5B30u  /* FUN_00ce5b30: heap->hilo actual (+0x14) */
#define RVA_CREATE_OBJECT 0x00AE1E60u /* FUN_00ee1e60: AS3 MovieRoot::CreateObject */
#define RVA_OI_VTABLE 0x01085500u    /* VA 0x1485500 */
#define RVA_MR_VTABLE 0x010872E8u    /* VA 0x14872E8 (runtime, corrida 170: el objeto de [[OI+4]+0xC]); +0xB8 = CreateObject */

static const uint8_t FIND_SCREEN_PRO[9] = { 0x83, 0xEC, 0x08, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x57 };
static const uint8_t HEAP_ASSIGN_PRO[14] = { 0x56, 0x8B, 0xF1, 0xFF, 0x15, 0x3C, 0xF0, 0x19, 0x01, 0x89, 0x46, 0x14, 0x5E, 0xC3 };
static const uint8_t CREATE_OBJECT_PRO[16] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x30, 0x01, 0x00, 0x00, 0x53, 0x8B, 0x5D, 0x14, 0x56, 0x57, 0x8B };

typedef void *(__attribute__((fastcall)) * FindScreenFn)(void *mgr, void *edx, const char *name);
typedef void(__attribute__((fastcall)) * HeapAssignFn)(void *heap, void *edx);
typedef void(__attribute__((fastcall)) * SetCaptureFn)(void *movie, void *edx, DWORD tid);
typedef void *(__attribute__((fastcall)) * GetHeapFn)(void *movie, void *edx);
typedef void(__attribute__((fastcall)) * CreateObjectFn)(void *mr, void *edx, GfxValue *out, const char *cls, const GfxValue *args, unsigned n);
typedef void(__attribute__((fastcall)) * ReleaseFn)(void *oi, void *edx, GfxValue *v, void *obj);
typedef uint8_t(__attribute__((fastcall)) * GetMemberFn)(void *oi, void *edx, void *obj, const char *name, GfxValue *out, uint32_t isdobj);
typedef uint8_t(__attribute__((fastcall)) * SetMemberFn)(void *oi, void *edx, void *obj, const char *name, const GfxValue *val, uint32_t isdobj);
typedef uint8_t(__attribute__((fastcall)) * InvokeFn)(void *oi, void *edx, void *obj, GfxValue *res, const char *name, const GfxValue *args,
                                                      unsigned n, uint32_t isdobj);

static int g_ok = -1;

int gfx_ok(void)
{
    if (g_ok >= 0)
        return g_ok;
    uint8_t a[16];
    g_ok = mem_read(g_aimp_exe + RVA_FIND_SCREEN, a, sizeof FIND_SCREEN_PRO) && !aimp_code_cmp(a, FIND_SCREEN_PRO, sizeof FIND_SCREEN_PRO) &&
           mem_read(g_aimp_exe + RVA_HEAP_ASSIGN, a, sizeof HEAP_ASSIGN_PRO) && !aimp_code_cmp(a, HEAP_ASSIGN_PRO, sizeof HEAP_ASSIGN_PRO) &&
           mem_read(g_aimp_exe + RVA_CREATE_OBJECT, a, sizeof CREATE_OBJECT_PRO) &&
           !aimp_code_cmp(a, CREATE_OBJECT_PRO, sizeof CREATE_OBJECT_PRO);
    if (!g_ok)
        log_printf("GFX: los prólogos de FUN_00810420/FUN_00ce5b30/FUN_00ee1e60 no coinciden: sin menú armado por el mod");
    return g_ok;
}

static int oi_ok(const GfxValue *v)
{
    uint32_t vt = 0;
    return v && v->oi && mem_u32((uintptr_t)v->oi, &vt) && vt == g_aimp_exe + RVA_OI_VTABLE;
}

static uint32_t isdobj(const GfxValue *v) { return (v->type & 0x8Fu) == GFX_DOBJ; }

/* diagnóstico: el motivo de la última falla de gfx_begin, logueado una vez por cambio */
static int g_last_why = -1;
static void why(int w, const char *msg, uint32_t a, uint32_t b)
{
    if (w == g_last_why)
        return;
    g_last_why = w;
    log_printf("GFX: %s (0x%08x 0x%08x)", msg, a, b);
}

static void list_screens(uint32_t mgr)
{
    uint32_t arr = 0, n = 0, s = 0, nm = 0;
    char buf[48];
    if (!mem_u32(mgr + 0x38, &arr) || !mem_u32(mgr + 0x3C, &n))
        return;
    log_printf("GFX: %u pantallas cargadas:", n);
    for (uint32_t i = 0; i < n && i < 64; i++)
        if (mem_u32(arr + 4 * i, &s) && mem_u32(s + 0x34, &nm) && mem_cstr(nm, buf, sizeof buf))
            log_printf("GFX:   %2u 0x%08x %s", i, s, buf);
}

int gfx_begin(const char *screen, GfxCtx *c)
{
    aimp_memset(c, 0, sizeof *c);
    uint32_t mgr = 0, root = 0, movie = 0, vt = 0, f = 0, fh = 0, heap = 0, oivt = 0;
    if (!gfx_ok() || !mem_u32(g_aimp_exe + RVA_SCREENS, &mgr) || !mgr)
        return 0;
    EnterCriticalSection((LPCRITICAL_SECTION)(uintptr_t)(mgr + 4));
    void *scr = ((FindScreenFn)(void *)(g_aimp_exe + RVA_FIND_SCREEN))((void *)(uintptr_t)mgr, NULL, screen);
    int w = 0;
    if (!scr)
        w = 1;
    else if (!mem_u32((uintptr_t)scr + 8, &movie) || !movie)
        w = 2;
    else if (!mem_u32((uintptr_t)scr + 0x68, &root) || !root)
        w = 3;
    else if (!mem_u32(root, &oivt) || !oivt || !mem_u32(oivt, &vt) || vt != g_aimp_exe + RVA_OI_VTABLE)
        w = 4;
    else if (!mem_u32(movie, &vt) || !mem_u32(vt + 0x6C, &f) || !mem_u32(vt + 0xC0, &fh))
        w = 5;
    if (w) {
        if (w == 1 && g_last_why != 1)
            list_screens(mgr);
        why(w, w == 1 ? "no está la pantalla pedida" : w == 2 ? "pantalla sin película (+8)" : w == 3 ? "pantalla sin objeto AS (+0x68)"
               : w == 4 ? "el objeto AS no tiene el ObjectInterface de AS3 (vtable, esperada)" : "película sin vtable",
            w == 4 ? vt : (uint32_t)(uintptr_t)scr, w == 4 ? (uint32_t)(g_aimp_exe + RVA_OI_VTABLE) : root);
        LeaveCriticalSection((LPCRITICAL_SECTION)(uintptr_t)(mgr + 4));
        return 0;
    }
    /* MovieRoot de AS3: [[OI+4]+0xC], con su vtable */
    uint32_t mi = 0, mr = 0, mrvt = 0;
    GfxValue *rv = (GfxValue *)(uintptr_t)root;
    uint32_t co = 0;
    if (!mem_u32((uintptr_t)rv->oi + 4, &mi) || !mi || !mem_u32(mi + 0xC, &mr) || !mr || !mem_u32(mr, &mrvt) ||
        mrvt != g_aimp_exe + RVA_MR_VTABLE || !mem_u32(mrvt + 0xB8, &co) || co != g_aimp_exe + RVA_CREATE_OBJECT) {
        why(6, "MovieRoot de AS3 no encontrado en [[OI+4]+0xC] (MovieImpl, vtable)", mi, mrvt);
        LeaveCriticalSection((LPCRITICAL_SECTION)(uintptr_t)(mgr + 4));
        return 0;
    }
    if (g_last_why != 0)
        why(0, "pantalla lista (película, MovieRoot)", movie, mr);
    ((SetCaptureFn)(uintptr_t)f)((void *)(uintptr_t)movie, NULL, GetCurrentThreadId());
    heap = (uint32_t)(uintptr_t)((GetHeapFn)(uintptr_t)fh)((void *)(uintptr_t)movie, NULL);
    if (heap)
        ((HeapAssignFn)(void *)(g_aimp_exe + RVA_HEAP_ASSIGN))((void *)(uintptr_t)heap, NULL);
    c->mgr = mgr;
    c->screen = (uintptr_t)scr;
    c->movie = movie;
    c->mr = mr;
    c->root = rv;
    return 1;
}

void gfx_end(GfxCtx *c)
{
    if (c->mgr)
        LeaveCriticalSection((LPCRITICAL_SECTION)(c->mgr + 4));
    c->mgr = 0;
}

void gfx_undef(GfxValue *v) { aimp_memset(v, 0, sizeof *v); }
void gfx_null(GfxValue *v)
{
    gfx_undef(v);
    v->type = GFX_NULL;
}
void gfx_str(GfxValue *v, const char *s)
{
    gfx_undef(v);
    v->type = GFX_STRING;
    v->v.s = s;
}
void gfx_bool(GfxValue *v, int b)
{
    gfx_undef(v);
    v->type = GFX_BOOL;
    v->v.b = b ? 1 : 0;
}
void gfx_int(GfxValue *v, int i)
{
    gfx_undef(v);
    v->type = GFX_INT;
    v->v.i = i;
}

void gfx_release(GfxValue *v)
{
    uint32_t vt = 0, f = 0;
    if ((v->type & 0x40u) && v->oi && mem_u32((uintptr_t)v->oi, &vt) && mem_u32(vt + 8, &f) && f)
        ((ReleaseFn)(uintptr_t)f)(v->oi, NULL, v, v->v.p);
    gfx_undef(v);
}

static uint32_t vfn(const GfxValue *obj, uint32_t off)
{
    uint32_t vt = 0, f = 0;
    if (!oi_ok(obj) || !mem_u32((uintptr_t)obj->oi, &vt) || !mem_u32(vt + off, &f))
        return 0;
    return f;
}

int gfx_get(const GfxValue *obj, const char *name, GfxValue *out)
{
    gfx_undef(out);
    uint32_t f = vfn(obj, 0x10);
    return f && ((GetMemberFn)(uintptr_t)f)(obj->oi, NULL, obj->v.p, name, out, isdobj(obj)) != 0;
}

int gfx_set(const GfxValue *obj, const char *name, const GfxValue *val)
{
    uint32_t f = vfn(obj, 0x14);
    return f && ((SetMemberFn)(uintptr_t)f)(obj->oi, NULL, obj->v.p, name, val, isdobj(obj)) != 0;
}

int gfx_invoke(const GfxValue *obj, const char *name, const GfxValue *args, unsigned n, GfxValue *out)
{
    GfxValue tmp;
    GfxValue *res = out ? out : &tmp;
    gfx_undef(res);
    uint32_t f = vfn(obj, 0x18);
    int ok = f && ((InvokeFn)(uintptr_t)f)(obj->oi, NULL, obj->v.p, res, name, args, n, isdobj(obj)) != 0;
    if (!out)
        gfx_release(&tmp);
    return ok;
}

int gfx_new(GfxCtx *c, const char *cls, const GfxValue *args, unsigned n, GfxValue *out)
{
    gfx_undef(out);
    if (!c->mr)
        return 0;
    ((CreateObjectFn)(void *)(g_aimp_exe + RVA_CREATE_OBJECT))((void *)c->mr, NULL, out, cls, args, n);
    return oi_ok(out) && (out->type & 0x8Fu) >= GFX_OBJECT;
}

int gfx_to_int(const GfxValue *v, int *out)
{
    switch (v->type & 0x8Fu) {
    case GFX_BOOL:
        *out = v->v.b ? 1 : 0;
        return 1;
    case GFX_INT:
        *out = v->v.i;
        return 1;
    case GFX_UINT:
        *out = (int)v->v.u;
        return 1;
    case GFX_NUMBER:
        *out = (int)v->v.n;
        return 1;
    default:
        return 0;
    }
}

int gfx_to_cstr(const GfxValue *v, char *out, size_t cap)
{
    out[0] = 0;
    if ((v->type & 0x8Fu) != GFX_STRING)
        return 0;
    uint32_t p = (uint32_t)(uintptr_t)v->v.s;
    if ((v->type & 0x40u) && !mem_u32(p, &p)) /* gestionado: puntero a puntero (como ui_arg) */
        return 0;
    return p && mem_cstr(p, out, cap);
}

int gfx_get_int(const GfxValue *obj, const char *name, int *out)
{
    GfxValue v;
    int ok = gfx_get(obj, name, &v) && gfx_to_int(&v, out);
    gfx_release(&v);
    return ok;
}

int gfx_get_cstr(const GfxValue *obj, const char *name, char *out, size_t cap)
{
    GfxValue v;
    int ok = gfx_get(obj, name, &v) && gfx_to_cstr(&v, out, cap);
    gfx_release(&v);
    return ok;
}

int gfx_get_index(const GfxValue *vec, int i, GfxValue *out)
{
    char n[12];
    aimp_snprintf(n, sizeof n, "%d", i);
    return gfx_get(vec, n, out) && (out->type & 0x8Fu) >= GFX_OBJECT;
}
