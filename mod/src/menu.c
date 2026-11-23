/* Opciones del coop y menú "Coop" armado con Scaleform (ver menu.h y gfx.h). */
#include "menu.h"
#include "gfx.h"
#include "log.h"
#include "diff.h"
#include "inv.h"
#include "pins.h"
#include "sync.h"
#include "loot.h"

/* ---- opciones ---- */
typedef struct {
    const char *file;  /* clave en opciones.txt */
    int check;         /* 1 = casilla (sí/no) */
    int n;             /* cantidad de valores */
    int def;
    const wchar_t *name, *desc; /* la descripción entra en un renglón (corrida 170: ~75 caracteres) */
    const wchar_t *vals[5];
    const wchar_t *vdesc[5];    /* descripción de cada valor (selector), o NULL */
} OptDef;

static const OptDef OPTS[OPT_N] = {
    { "dificultad_coop", 0, 4, 0, L"Dificultad del coop", L"La decide el host",
      { L"Normal", L"Difícil", L"Pesadilla", L"Personalizada" },
      { L"La dificultad del juego, sin cambios. La decide el host",
        L"El Alien más encima (+1 escalón); al caer solo revive una terminal",
        L"El Alien todavía más encima (+2); al caer solo revive una terminal",
        L"Elegís cada cosa en Opciones avanzadas" } },
    { "escala_alien", 0, 4, 0, L"Escala del Alien", L"Escalones de agresividad del Alien sobre la dificultad del juego",
      { L"Como el juego", L"+1", L"+2", L"+3" }, { NULL } },
    { "escala_humanos", 0, 4, 0, L"Humanos y androides", L"Sentidos y daño de humanos y androides sobre la dificultad del juego",
      { L"Como el juego", L"+1", L"+2", L"+3" }, { NULL } },
    { "caer_y_revivir", 0, 2, 1, L"Caer y revivir", L"Qué pasa cuando cae uno de los dos",
      { L"Normal", L"Difícil" },
      { L"Te desangrás 90 s; tu compañero te levanta manteniendo R a tu lado",
        L"Al caer, solo te revive una terminal de guardado o un checkpoint" } },
    { "guardado_exigente", 1, 2, 0, L"Guardado exigente", L"Guarda el host, y con el compañero vivo cerca de la terminal", { NULL },
      { NULL } },
    { "inventario_compartido", 1, 2, 1, L"Inventario compartido", L"Lo que uno levanta, gasta o fabrica le pasa también al otro",
      { NULL }, { NULL } },
    { "cajas_compartidas", 1, 2, 1, L"Cajas compartidas", L"El primero que abre una caja se lleva el botín; al otro le queda vacía",
      { NULL }, { NULL } },
    { "botin", 0, 5, 2, L"Botín", L"Cuánto hay en las cajas, sobre lo que reparte el juego",
      { L"x0,5", L"x0,75", L"Normal", L"x1,5", L"x2" },
      { L"La mitad: algunas cajas salen vacías", L"Un cuarto menos", L"Lo que reparte el juego", L"La mitad más", L"El doble" } },
};
static const int LOOT_PCT[5] = { 50, 75, 100, 150, 200 };

static int g_val[OPT_N];
static AimpNet *g_net;
static int g_host[OPT_N], g_have_host; /* cliente: lo que eligió el host */
static int g_applied[OPT_N];
static int g_started, g_had_remote, g_sent_sig = -1;
static wchar_t g_path[MAX_PATH];
static int g_adv;
static int g_trace;

static int clampv(int id, int v) { return v < 0 ? 0 : v >= OPTS[id].n ? OPTS[id].n - 1 : v; }

/* las tres primeras avanzadas las fija la dificultad, salvo en Personalizada */
static int fixed_by_diff(int id) { return id == OPT_ALIEN || id == OPT_HUMANS || id == OPT_DOWNED; }

static int eff(const int *vals, int id)
{
    int d = vals[OPT_DIFF];
    if (fixed_by_diff(id) && d != OPT_DIFF_CUSTOM) {
        if (id == OPT_DOWNED)
            return d == 0 ? 0 : 1;
        return d; /* Normal 0, Difícil +1, Pesadilla +2 */
    }
    return vals[id];
}

static int from_host(void) { return g_net && g_net->mode == 2 && g_net->have_remote && g_have_host; }

int menu_opt(int id)
{
    if (id < 0 || id >= OPT_N)
        return 0;
    return eff(from_host() ? g_host : g_val, id);
}

int menu_adv_on(void) { return g_adv; }

static void save(void)
{
    if (!g_path[0])
        return;
    HANDLE h = CreateFileW(g_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char b[512];
    int n = 0;
    for (int i = 0; i < OPT_N; i++)
        n += aimp_snprintf(b + n, sizeof b - (size_t)n, "%s=%d\r\n", OPTS[i].file, g_val[i]);
    DWORD w = 0;
    WriteFile(h, b, (DWORD)n, &w, NULL);
    CloseHandle(h);
}

static int streq(const char *a, const char *b)
{
    for (; *a && *a == *b; a++, b++)
        ;
    return *a == *b;
}

static void load(void)
{
    HANDLE h = CreateFileW(g_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char b[1024];
    DWORD n = 0;
    ReadFile(h, b, sizeof b - 1, &n, NULL);
    CloseHandle(h);
    b[n] = 0;
    char *p = b;
    while (*p) {
        char *k = p, *eq = NULL;
        while (*p && *p != '\n') {
            if (*p == '=' && !eq)
                eq = p;
            p++;
        }
        if (*p)
            *p++ = 0;
        if (!eq)
            continue;
        *eq = 0;
        int v = 0, neg = 0;
        char *q = eq + 1;
        if (*q == '-')
            neg = 1, q++;
        while (*q >= '0' && *q <= '9')
            v = v * 10 + (*q++ - '0');
        for (int i = 0; i < OPT_N; i++)
            if (streq(k, OPTS[i].file))
                g_val[i] = clampv(i, neg ? -v : v);
    }
}

/* Lleva los valores efectivos a los módulos (solo con el menú armado: sin él mandan las variables de aimp.cfg). */
static void apply_effects(void)
{
    if (!g_adv || !g_started)
        return;
    for (int id = OPT_HUMANS; id < OPT_N; id++) {
        int v = menu_opt(id);
        if (v == g_applied[id] || id == OPT_DOWNED)
            continue;
        g_applied[id] = v;
        if (id == OPT_HUMANS)
            diff_set_humans(v);
        else if (id == OPT_STRICT)
            sync_set_save_rule(v);
        else if (id == OPT_INV)
            inv_set_shared(v);
        else if (id == OPT_BOXES)
            pins_set_boxes(v);
        else if (id == OPT_LOOT)
            loot_set_pct(LOOT_PCT[v]);
    }
}

static void apply(int id)
{
    if (id == OPT_DIFF || id == OPT_ALIEN)
        diff_set_local(eff(g_val, OPT_ALIEN)); /* diff.c ya usa la del host en el cliente (AIMP_EVT_OPTIONS tipo 1) */
    apply_effects();
}

void menu_start(AimpNet *net)
{
    g_net = net;
    g_started = 1;
    apply_effects();
}

static int sig_of(const int *vals)
{
    int s = 0;
    for (int i = 0; i < OPT_N; i++)
        s = s * 5 + vals[i];
    return s;
}

void menu_remote_event(const AimpEvt *e)
{
    MenuOptsEvt o;
    if (e->size < sizeof o)
        return;
    aimp_memcpy(&o, e->data, sizeof o);
    if (o.kind != 2 || !g_net || g_net->mode != 2)
        return;
    int nv[OPT_N];
    for (int i = 0; i < OPT_N; i++)
        nv[i] = i < (int)o.n && i < 16 ? clampv(i, o.v[i]) : OPTS[i].def;
    if (!g_have_host || sig_of(nv) != sig_of(g_host))
        log_printf("MENU: opciones del host: dificultad %d, Alien %d, humanos %d, caer %d, guardado exigente %d, inventario %d, cajas %d",
                   nv[OPT_DIFF], eff(nv, OPT_ALIEN), eff(nv, OPT_HUMANS), eff(nv, OPT_DOWNED), nv[OPT_STRICT], nv[OPT_INV], nv[OPT_BOXES]);
    aimp_memcpy(g_host, nv, sizeof g_host);
    g_have_host = 1;
    apply_effects();
}

/* host: manda sus opciones al conectarse el otro y cada vez que cambian; cliente: sin host vuelven las propias */
static void net_frame(void)
{
    if (!g_net || !g_net->mode || !g_adv)
        return;
    int rem = g_net->have_remote;
    if (g_net->mode == 1 && rem && (!g_had_remote || g_sent_sig != sig_of(g_val))) {
        MenuOptsEvt o;
        aimp_memset(&o, 0, sizeof o);
        o.kind = 2;
        o.n = OPT_N;
        for (int i = 0; i < OPT_N; i++)
            o.v[i] = (uint8_t)g_val[i];
        if (net_event(g_net, AIMP_EVT_OPTIONS, 0, &o, sizeof o)) {
            g_sent_sig = sig_of(g_val);
            log_printf("MENU: opciones del coop mandadas al cliente");
        }
    }
    if (g_net->mode == 2 && !rem && g_had_remote && g_have_host) {
        g_have_host = 0;
        log_printf("MENU: se fue el host: vuelven las opciones propias");
        apply_effects();
    }
    g_had_remote = rem;
}

void menu_set(int id, int v)
{
    if (id < 0 || id >= OPT_N)
        return;
    g_val[id] = clampv(id, v);
    save();
    apply(id);
    char a[64], b[64];
    log_printf("MENU: %s = %s (efectivo %d; opciones.txt)", aimp_utf8(a, sizeof a, OPTS[id].name),
               OPTS[id].check ? (g_val[id] ? "sí" : "no") : aimp_utf8(b, sizeof b, OPTS[id].vals[g_val[id]]), menu_opt(id));
}

/* ---- claves de texto ----
 * AI_AIMP_M<id> nombre, AI_AIMP_MD<id> descripción, AI_AIMP_MV<id>_<i> valor, AI_AIMP_MADV / AI_AIMP_MADVD el submenú. */
#define KEY_ADV "AI_AIMP_MADV"
#define KEY_ADV_DESC "AI_AIMP_MADVD"
static char g_kname[OPT_N][16], g_kdesc[OPT_N][16], g_kval[OPT_N][5][20], g_kvdesc[OPT_N][5][20];

static int starts(const char *s, const char *pre)
{
    for (; *pre && *s == *pre; s++, pre++)
        ;
    return !*pre;
}

const wchar_t *menu_text(const char *key)
{
    if (!key || !starts(key, "AI_AIMP_M"))
        return NULL;
    if (streq(key, KEY_ADV))
        return L"Opciones avanzadas";
    if (streq(key, KEY_ADV_DESC))
        return L"Alien, humanos, caer y revivir, y qué se comparte. Lo decide el host";
    for (int i = 0; i < OPT_N; i++) {
        if (streq(key, g_kname[i]))
            return OPTS[i].name;
        if (streq(key, g_kdesc[i]))
            return OPTS[i].desc;
        for (int j = 0; j < OPTS[i].n && !OPTS[i].check; j++) {
            if (streq(key, g_kval[i][j]))
                return OPTS[i].vals[j];
            if (streq(key, g_kvdesc[i][j]))
                return OPTS[i].vdesc[j];
        }
    }
    return NULL;
}

/* ---- armado con Scaleform ---- */
static uintptr_t g_scr;
static void *g_root_obj;
static int g_built, g_fail, g_tries;
static DWORD g_next;
static int g_shown[OPT_N]; /* lo que muestra el menú */

/* busca en un Vector.<MenuItem> el de nombre name (out queda gestionado: soltarlo) */
static int find_item(const GfxValue *items, const char *name, GfxValue *out, int *count)
{
    int n = 0;
    gfx_undef(out);
    if (!gfx_get_int(items, "length", &n))
        return 0;
    if (count)
        *count = n;
    for (int i = 0; i < n && i < 64; i++) {
        GfxValue it;
        char nm[64];
        if (!gfx_get_index(items, i, &it))
            continue;
        if (gfx_get_cstr(&it, "name", nm, sizeof nm) && streq(nm, name)) {
            *out = it;
            return 1;
        }
        gfx_release(&it);
    }
    return 0;
}

/* página de Extras ("Coop"): GameMenu.menu.rootPage.items -> AI_UI_FRONTEND_EXTRAS -> page */
static int coop_page(GfxCtx *c, GfxValue *page, int verbose)
{
    GfxValue menu, root, items, extras;
    int ok = 0, n = -1;
    gfx_undef(page);
    if (!gfx_get(c->root, "menu", &menu)) {
        if (verbose)
            log_printf("MENU: GameMenu.menu no se pudo leer");
        return 0;
    }
    if (gfx_get(&menu, "rootPage", &root)) {
        if (gfx_get(&root, "items", &items)) {
            if (find_item(&items, "AI_UI_FRONTEND_EXTRAS", &extras, &n)) {
                ok = gfx_get(&extras, "page", page) && (page->type & 0x8Fu) >= GFX_OBJECT;
                gfx_release(&extras);
            }
            gfx_release(&items);
        }
        gfx_release(&root);
    }
    if (verbose)
        log_printf("MENU: página de Extras %s (menú tipo %u, %d ítems en la raíz)", ok ? "encontrada" : "NO encontrada", menu.type, n);
    gfx_release(&menu);
    if (!ok)
        gfx_release(page);
    return ok;
}

static int mk_item(GfxCtx *c, int id, GfxValue *out)
{
    GfxValue a[4], items, labels, v;
    int ok;
    if (OPTS[id].check) {
        gfx_str(&a[0], g_kname[id]);
        gfx_bool(&a[1], 1);
        gfx_null(&a[2]);
        gfx_null(&a[3]);
        ok = gfx_new(c, "frontend.menu.MenuItemOptionCheckBox", a, 4, out);
        if (ok) {
            gfx_bool(&v, menu_opt(id));
            gfx_set(out, "selected", &v);
        }
    } else {
        if (!gfx_new(c, "Array", NULL, 0, &items))
            return 0;
        if (!gfx_new(c, "Array", NULL, 0, &labels)) {
            gfx_release(&items);
            return 0;
        }
        GfxValue e[5];
        for (int j = 0; j < OPTS[id].n; j++)
            gfx_int(&e[j], j);
        gfx_invoke(&items, "push", e, (unsigned)OPTS[id].n, NULL);
        for (int j = 0; j < OPTS[id].n; j++)
            gfx_str(&e[j], g_kval[id][j]);
        gfx_invoke(&labels, "push", e, (unsigned)OPTS[id].n, NULL);
        gfx_str(&a[0], g_kname[id]);
        a[1] = items; /* copia sin dueño: se suelta una sola vez abajo */
        a[2] = labels;
        gfx_null(&a[3]);
        ok = gfx_new(c, "frontend.menu.MenuItemOptionOptionStepper", a, 4, out);
        gfx_release(&items);
        gfx_release(&labels);
        if (ok) {
            gfx_int(&v, menu_opt(id));
            gfx_set(out, "selectedIndex", &v);
        }
    }
    if (ok) {
        gfx_str(&v, g_kdesc[id]);
        gfx_set(out, "description", &v);
        if (!OPTS[id].check && OPTS[id].vdesc[0]) { /* una descripción por valor (MenuItemOptionSequence.descriptions) */
            GfxValue d, e[5];
            if (gfx_new(c, "Array", NULL, 0, &d)) {
                for (int j = 0; j < OPTS[id].n; j++)
                    gfx_str(&e[j], g_kvdesc[id][j]);
                gfx_invoke(&d, "push", e, (unsigned)OPTS[id].n, NULL);
                gfx_set(out, "descriptions", &d);
                gfx_release(&d);
            }
        }
        if (fixed_by_diff(id)) {
            gfx_bool(&v, g_val[OPT_DIFF] == OPT_DIFF_CUSTOM);
            gfx_set(out, "enabled", &v);
        }
        g_shown[id] = menu_opt(id);
    }
    return ok;
}

/* 1 armado, 0 todavía no está la página (se reintenta), -1 falló al crear */
static int build(GfxCtx *c, int verbose)
{
    GfxValue page, pitems, have, credits, item, exp, sub, v;
    if (!coop_page(c, &page, verbose))
        return 0;
    int ok = -1;
    if (gfx_get(&page, "items", &pitems)) {
        if (find_item(&pitems, g_kname[OPT_DIFF], &have, NULL)) {
            log_printf("MENU: la página Coop ya tiene las opciones del mod");
            gfx_release(&have);
            ok = 1;
        } else if (mk_item(c, OPT_DIFF, &item)) {
            ok = gfx_invoke(&page, "addItem", &item, 1, NULL) ? 1 : -1;
            gfx_release(&item);
            GfxValue a[1];
            gfx_str(&a[0], KEY_ADV);
            if (ok > 0 && gfx_new(c, "frontend.menu.MenuItemExpandable", a, 1, &exp)) {
                gfx_str(&v, KEY_ADV_DESC);
                gfx_set(&exp, "description", &v);
                int nsub = 0;
                if (gfx_get(&exp, "page", &sub)) {
                    for (int id = OPT_ALIEN; id < OPT_N; id++)
                        if (mk_item(c, id, &item)) {
                            nsub += gfx_invoke(&sub, "addItem", &item, 1, NULL);
                            gfx_release(&item);
                        }
                    gfx_release(&sub);
                }
                ok = gfx_invoke(&page, "addItem", &exp, 1, NULL) ? 1 : -1;
                gfx_release(&exp);
                log_printf("MENU: submenú Opciones avanzadas con %d de %d opciones (%s)", nsub, OPT_N - 1, ok ? "agregado" : "addItem falló");
            } else
                log_printf("MENU: no se pudo crear el submenú (MenuItemExpandable)");
            /* el botón viejo (Credits = dificultad que cambia al apretarlo) se esconde */
            if (ok > 0 && find_item(&pitems, "AI_UI_FRONTEND_CREDITS", &credits, NULL)) {
                gfx_bool(&v, 0);
                gfx_set(&credits, "visible", &v);
                gfx_release(&credits);
            }
        } else
            log_printf("MENU: no se pudo crear el selector de dificultad (CreateObject de MenuItemOptionOptionStepper)");
        gfx_release(&pitems);
    }
    gfx_release(&page);
    return ok;
}

/* lee el valor que muestra cada opción; si el jugador lo cambió, lo toma */
static int read_item(const GfxValue *items, int id, int *out)
{
    GfxValue it;
    int ok = 0;
    if (find_item(items, g_kname[id], &it, NULL)) {
        ok = gfx_get_int(&it, OPTS[id].check ? "selected" : "selectedIndex", out);
        gfx_release(&it);
    }
    return ok;
}

static void show_item(const GfxValue *items, int id, int enabled)
{
    GfxValue it, v;
    if (!find_item(items, g_kname[id], &it, NULL))
        return;
    if (OPTS[id].check)
        gfx_bool(&v, menu_opt(id));
    else
        gfx_int(&v, menu_opt(id));
    gfx_set(&it, OPTS[id].check ? "selected" : "selectedIndex", &v);
    if (enabled >= 0) {
        gfx_bool(&v, enabled);
        gfx_set(&it, "enabled", &v);
    }
    gfx_release(&it);
    g_shown[id] = menu_opt(id);
}

/* Muestra en todas las opciones el valor efectivo; hm = cliente conectado (las del host, todas bloqueadas). */
static int g_ui_host, g_ui_sig = -2;
static void refresh_all(GfxCtx *c, int hm)
{
    GfxValue page, pitems, exp, sub, sitems;
    if (!coop_page(c, &page, 0))
        return;
    if (gfx_get(&page, "items", &pitems)) {
        show_item(&pitems, OPT_DIFF, !hm);
        if (find_item(&pitems, KEY_ADV, &exp, NULL)) {
            if (gfx_get(&exp, "page", &sub)) {
                if (gfx_get(&sub, "items", &sitems)) {
                    for (int id = OPT_ALIEN; id < OPT_N; id++)
                        show_item(&sitems, id, hm ? 0 : fixed_by_diff(id) ? g_val[OPT_DIFF] == OPT_DIFF_CUSTOM : 1);
                    gfx_release(&sitems);
                }
                gfx_release(&sub);
            }
            gfx_release(&exp);
        }
        gfx_release(&pitems);
    }
    gfx_release(&page);
}

static void poll(GfxCtx *c)
{
    GfxValue page, pitems, exp, sub, sitems;
    int hm = from_host();
    if (hm || g_ui_host) {
        int sig = hm ? sig_of(g_host) : -1;
        if (sig != g_ui_sig || hm != g_ui_host) {
            refresh_all(c, hm);
            log_printf("MENU: el menú muestra %s", hm ? "las opciones del host (bloqueadas)" : "otra vez las opciones propias");
        }
        g_ui_sig = sig;
        g_ui_host = hm;
        if (hm)
            return;
    }
    if (!coop_page(c, &page, 0))
        return;
    if (gfx_get(&page, "items", &pitems)) {
        int v;
        if (read_item(&pitems, OPT_DIFF, &v) && v != g_shown[OPT_DIFF]) {
            menu_set(OPT_DIFF, v);
            g_shown[OPT_DIFF] = v;
            if (find_item(&pitems, KEY_ADV, &exp, NULL)) {
                if (gfx_get(&exp, "page", &sub)) {
                    if (gfx_get(&sub, "items", &sitems)) {
                        for (int id = OPT_ALIEN; id < OPT_N; id++)
                            if (fixed_by_diff(id))
                                show_item(&sitems, id, v == OPT_DIFF_CUSTOM);
                        gfx_release(&sitems);
                    }
                    gfx_release(&sub);
                }
                gfx_release(&exp);
            }
        }
        if (find_item(&pitems, KEY_ADV, &exp, NULL)) {
            if (gfx_get(&exp, "page", &sub)) {
                if (gfx_get(&sub, "items", &sitems)) {
                    for (int id = OPT_ALIEN; id < OPT_N; id++) {
                        if (!read_item(&sitems, id, &v) || v == g_shown[id])
                            continue;
                        if (fixed_by_diff(id) && g_val[OPT_DIFF] != OPT_DIFF_CUSTOM) {
                            show_item(&sitems, id, 0); /* fijada por la dificultad: vuelve */
                            continue;
                        }
                        menu_set(id, v);
                        g_shown[id] = v;
                    }
                    gfx_release(&sitems);
                }
                gfx_release(&sub);
            }
            gfx_release(&exp);
        }
        gfx_release(&pitems);
    }
    gfx_release(&page);
}

void menu_frame(int have_player)
{
    net_frame();
    if (!g_adv || have_player || g_fail > 5)
        return;
    DWORD t = GetTickCount();
    if ((int)(t - g_next) < 0)
        return;
    g_next = t + 250;
    GfxCtx c;
    if (!gfx_begin("NEW_FRONTEND_MENU", &c)) {
        if (g_scr) {
            log_printf("MENU: se fue la pantalla del menú principal");
            g_scr = 0;
            g_built = 0;
        }
        return;
    }
    if (c.screen != g_scr || c.root->v.p != g_root_obj) {
        if (g_trace || !g_scr)
            log_printf("MENU: pantalla NEW_FRONTEND_MENU 0x%08x (película 0x%08x, MovieRoot 0x%08x, GameMenu tipo %u)", (unsigned)c.screen,
                       (unsigned)c.movie, (unsigned)c.mr, c.root->type);
        g_scr = c.screen;
        g_root_obj = c.root->v.p;
        g_built = 0;
        g_tries = 0;
    }
    if (!g_built) {
        int r = build(&c, g_tries++ % 40 == 0); /* el log de la búsqueda, cada 10 s */
        g_built = r > 0;
        if (g_built)
            g_ui_sig = -2, g_ui_host = from_host(); /* en el cliente conectado, el primer poll bloquea todo */
        if (r < 0 && ++g_fail > 5)
            log_printf("MENU: no se pudo armar el menú del mod después de 6 intentos: queda el botón de siempre");
    } else
        poll(&c);
    gfx_end(&c);
}

void menu_install(void)
{
    for (int i = 0; i < OPT_N; i++) {
        g_val[i] = OPTS[i].def;
        aimp_snprintf(g_kname[i], sizeof g_kname[i], "AI_AIMP_M%d", i);
        aimp_snprintf(g_kdesc[i], sizeof g_kdesc[i], "AI_AIMP_MD%d", i);
        g_applied[i] = -1;
        for (int j = 0; j < 5; j++)
        {
            aimp_snprintf(g_kval[i][j], sizeof g_kval[i][j], "AI_AIMP_MV%d_%d", i, j);
            aimp_snprintf(g_kvdesc[i][j], sizeof g_kvdesc[i][j], "AI_AIMP_MW%d_%d", i, j);
        }
    }
    wchar_t base[MAX_PATH];
    if (aimp_env_str(L"LOCALAPPDATA", base, MAX_PATH)) {
        aimp_wcscpy(g_path, MAX_PATH, base);
        aimp_wcscat(g_path, MAX_PATH, L"\\AI_Multiplayer\\opciones.txt");
        /* sin el valor en opciones.txt, lo que diga aimp.cfg (los presets traen AIMP_SYNC_INV=1) */
        g_val[OPT_INV] = aimp_env_int(L"AIMP_SYNC_INV", OPTS[OPT_INV].def) ? 1 : 0;
        g_val[OPT_STRICT] = aimp_env_int(L"AIMP_COOP_SAVE_RULE", OPTS[OPT_STRICT].def) ? 1 : 0;
        load();
    }
    g_adv = aimp_env_int(L"AIMP_MOD_MENU_ADV", 1) && aimp_env_int(L"AIMP_MOD_MENU", 1);
    g_trace = aimp_env_int(L"AIMP_TRACE_MENU", 0);
    apply(OPT_DIFF);
    log_printf("MENU: opciones del coop: dificultad %d, Alien %d, humanos %d, caer %d, guardado exigente %d, inventario %d, cajas %d%s",
               g_val[OPT_DIFF], menu_opt(OPT_ALIEN), menu_opt(OPT_HUMANS), menu_opt(OPT_DOWNED), g_val[OPT_STRICT], g_val[OPT_INV],
               g_val[OPT_BOXES], g_adv ? " (menú armado con Scaleform)" : "");
}
