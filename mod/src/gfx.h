#ifndef AIMP_GFX_H
#define AIMP_GFX_H
#include "aimp.h"

/* Capa mínima sobre Scaleform GFx 4 (AS3) para leer y armar objetos de ActionScript de una pantalla del juego
 * (work/DESIGN_CONFIG_MENU.md, "Submenú Opciones avanzadas"). Todo verificado en AI.exe (2026-11-23, Ghidra):
 *   - GFx::Value = 0x18 bytes: +0 ObjectInterface*, +4 tipo (0x8F = máscara; 0x40 = gestionado), +8 dato (8 bytes),
 *     +0x10 aux. Tipos: 0 undefined, 1 null, 2 bool, 3 int, 4 uint, 5 number, 6 string, 8 objeto, 9 arreglo,
 *     0xA objeto de pantalla. FUN_0081c1a0 arma así los argumentos (tipo 6 = texto) y calcula isdobj = (tipo&0x8F)==0xA.
 *   - vtable del ObjectInterface de AS3 = VA 0x1485500 (la escribe FUN_00ecf3f0): +0x08 ObjectRelease(Value*, obj)
 *     ret 8 (FUN_00ea60a0), +0x10 GetMember(obj, nombre, Value* sal, isdobj) ret 0x10 (FUN_00ee68a0), +0x14 SetMember
 *     ret 0x10 (FUN_00ee14f0: "SetMember aborted"), +0x18 Invoke(obj, Value* res, nombre, args, n, isdobj) ret 0x18
 *     (FUN_00ee6ba0). El juego llama Invoke así en FUN_0081c1a0.
 *   - MovieRoot de AS3 = [[OI+4]+0xC] (GetMember e Invoke lo leen así; su +0x28 es la VM); vtable VA 0x14872E8
 *     (runtime, corrida 170; Ghidra la ve como continuación del bloque de 0x14872CC). Su entrada +0xB8 es
 *     CreateObject = FUN_00ee1e60 thiscall(mr, Value* res, const char* clase, const Value* args, unsigned n), ret 0x10;
 *     +0xBC CreateArray (FUN_00ee21b0).
 *   - Pantallas: FUN_00810420 thiscall([VA 0x174A78C], nombre) (lista en +0x38/+0x3C, nombre en pantalla+0x34);
 *     pantalla+8 = GFx::Movie, pantalla+0x68 = Value* del objeto AS de la pantalla. Antes de tocar la película el
 *     juego (FUN_0082b450) toma el lock [0x174A78C]+4, llama Movie vt+0x6C(GetCurrentThreadId()) y le asigna el
 *     heap de Movie vt+0xC0() al hilo actual (FUN_00ce5b30). gfx_begin hace lo mismo.
 * Nombre de la pantalla del menú principal: "NEW_FRONTEND_MENU" (FUN_00813410). */

typedef struct GfxValue {
    void *oi;
    uint32_t type;
    union {
        double n;
        int32_t i;
        uint32_t u;
        const char *s;
        void *p;
        uint8_t b;
    } v;
    uint32_t aux;
    uint32_t pad;
} GfxValue;

typedef struct {
    uintptr_t mgr, screen, movie, mr;
    GfxValue *root; /* objeto AS de la pantalla */
} GfxCtx;

#define GFX_UNDEF 0u
#define GFX_NULL 1u
#define GFX_BOOL 2u
#define GFX_INT 3u
#define GFX_UINT 4u
#define GFX_NUMBER 5u
#define GFX_STRING 6u
#define GFX_OBJECT 8u
#define GFX_ARRAY 9u
#define GFX_DOBJ 10u

/* 1 si las funciones del juego coinciden con la build (una vez). */
int gfx_ok(void);
/* Toma el lock y prepara la película de la pantalla; 0 si no está (en ese caso no hace falta gfx_end). */
int gfx_begin(const char *screen, GfxCtx *c);
void gfx_end(GfxCtx *c);

void gfx_undef(GfxValue *v);
void gfx_str(GfxValue *v, const char *s); /* el texto tiene que vivir hasta que se use */
void gfx_bool(GfxValue *v, int b);
void gfx_int(GfxValue *v, int i);
void gfx_null(GfxValue *v);
/* Suelta un valor gestionado (los que devuelve el juego) y lo deja undefined. */
void gfx_release(GfxValue *v);

int gfx_get(const GfxValue *obj, const char *name, GfxValue *out);
int gfx_set(const GfxValue *obj, const char *name, const GfxValue *val);
int gfx_invoke(const GfxValue *obj, const char *name, const GfxValue *args, unsigned n, GfxValue *out /* o NULL */);
int gfx_new(GfxCtx *c, const char *cls, const GfxValue *args, unsigned n, GfxValue *out);

/* Conversión de lo leído: entero (int/uint/number/bool) y texto (string gestionado o no). */
int gfx_to_int(const GfxValue *v, int *out);
int gfx_to_cstr(const GfxValue *v, char *out, size_t cap);
/* Atajos: miembro entero / texto de un objeto. */
int gfx_get_int(const GfxValue *obj, const char *name, int *out);
int gfx_get_cstr(const GfxValue *obj, const char *name, char *out, size_t cap);
/* Elemento i de un Vector/Array por nombre ("0", "1"...). */
int gfx_get_index(const GfxValue *vec, int i, GfxValue *out);

#endif
