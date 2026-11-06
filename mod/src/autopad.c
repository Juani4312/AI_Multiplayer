/* Joystick virtual para pruebas automáticas (ver autopad.h). */
#include "autopad.h"
#include "log.h"
#include "probe.h"

#define AP_MAX_STEPS 64

/* XInput (no incluimos xinput.h: el mod no linkea xinput) */
typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} AGamepad;
typedef struct {
    DWORD dwPacketNumber;
    AGamepad Gamepad;
} AState;
typedef struct {
    BYTE Type, SubType;
    WORD Flags;
    AGamepad Gamepad;
    WORD wLeftMotorSpeed, wRightMotorSpeed;
} ACaps;

typedef DWORD(WINAPI *GetStateFn)(DWORD idx, AState *st);
typedef DWORD(WINAPI *GetCapsFn)(DWORD idx, DWORD flags, ACaps *caps);

typedef struct {
    int rel_player;
    int32_t t0, t1;
    uint16_t buttons;
    uint8_t lt, rt, mark;
    uint8_t has[4];
    int16_t thumb[4];
    int started;
    /* GO=x/z: ir caminando hasta (x, z) a lazo cerrado con la posición y el yaw del jugador 0 */
    uint8_t go, done;
    float gx, gz;
} Step;

static Step g_steps[AP_MAX_STEPS];
static int g_nsteps, g_on;
static GetStateFn g_get_state;
static GetCapsFn g_get_caps;
static volatile LONG g_packet;
static int64_t g_t_start, g_t_player;
static int64_t g_base_ms; /* los pasos sin 'P' cuentan desde acá (0 = el arranque; al leer el archivo, ese momento) */
static int g_had_player;
/* Jugador 0 (lo pasa la sonda en cada frame) y progreso del GO activo. */
static int g_have_pose;
static float g_px, g_pz, g_yaw;
static float g_go_best;
static float g_yaw_off; /* AIMP_AUTOPAD_YAWOFF en milirradianes (0 por defecto) */
static int64_t g_go_best_ms;
/* Archivo de comandos (AIMP_AUTOPAD_FILE): se relee cuando cambia su fecha de escritura. */
static wchar_t g_file[MAX_PATH];
static FILETIME g_file_time;
static int64_t g_next_file_check;
/* Estado actual (lo escribe autopad_frame en el thread principal; lo lee el hook). */
static volatile uint32_t g_cur_buttons_trig; /* buttons | lt << 16 | rt << 24 */
static volatile int32_t g_cur_thumbs[4];
static volatile LONG g_cur_has; /* bit i: thumb i forzado */
static volatile LONG g_n_state_calls;

static const struct {
    const char *name;
    uint16_t bit;
} k_buttons[] = {
    { "UP", 0x0001 },   { "DOWN", 0x0002 }, { "LEFT", 0x0004 }, { "RIGHT", 0x0008 }, { "START", 0x0010 },
    { "BACK", 0x0020 }, { "LS", 0x0040 },   { "RS", 0x0080 },   { "LB", 0x0100 },    { "RB", 0x0200 },
    { "A", 0x1000 },    { "B", 0x2000 },    { "X", 0x4000 },    { "Y", 0x8000 },
};

static int streq_n(const char *a, size_t n, const char *b)
{
    size_t m = aimp_strlen(b);
    if (m != n)
        return 0;
    for (size_t i = 0; i < n; i++)
        if ((a[i] & ~0x20) != (b[i] & ~0x20) && a[i] != b[i])
            return 0;
    return 1;
}

static const char *parse_int(const char *p, int32_t *out)
{
    int neg = 0;
    int64_t v = 0;
    if (*p == '-' || *p == '+')
        neg = *p++ == '-';
    if (*p < '0' || *p > '9')
        return NULL;
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    *out = (int32_t)(neg ? -v : v);
    return p;
}

static const char *parse_float(const char *p, float *out)
{
    int neg = 0, any = 0;
    float v = 0, f = 0.1f;
    if (*p == '-' || *p == '+')
        neg = *p++ == '-';
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (float)(*p++ - '0');
        any = 1;
    }
    if (*p == '.')
        for (p++; *p >= '0' && *p <= '9'; p++, f *= 0.1f) {
            v += (float)(*p - '0') * f;
            any = 1;
        }
    if (!any)
        return NULL;
    *out = neg ? -v : v;
    return p;
}

/* atan2 aproximado (error < 0,01 rad); el mod no tiene libm. */
static float atan2_approx(float y, float x)
{
    const float pi = 3.14159265f;
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    if (ax < 1e-9f && ay < 1e-9f)
        return 0;
    float a = ax > ay ? ay / ax : ax / ay;
    float r = a * (0.9998660f + a * a * (-0.3302995f + a * a * (0.1801410f + a * a * (-0.0851330f + a * a * 0.0208351f))));
    if (ay > ax)
        r = pi / 2 - r;
    if (x < 0)
        r = pi - r;
    return y < 0 ? -r : r;
}

static float wrap_pi(float a)
{
    const float pi = 3.14159265f;
    while (a > pi)
        a -= 2 * pi;
    while (a < -pi)
        a += 2 * pi;
    return a;
}

static int parse_action(Step *s, const char *a, size_t n)
{
    if (n > 5 && streq_n(a, 3, "GO=")) {
        const char *e = parse_float(a + 3, &s->gx);
        if (!e || *e != '/' || !(e = parse_float(e + 1, &s->gz)) || (size_t)(e - a) != n)
            return 0;
        s->go = 1;
        return 1;
    }
    for (size_t i = 0; i < sizeof k_buttons / sizeof k_buttons[0]; i++)
        if (streq_n(a, n, k_buttons[i].name)) {
            s->buttons |= k_buttons[i].bit;
            return 1;
        }
    if (streq_n(a, n, "LT")) {
        s->lt = 255;
        return 1;
    }
    if (streq_n(a, n, "RT")) {
        s->rt = 255;
        return 1;
    }
    if (streq_n(a, n, "MARK")) {
        s->mark = 1;
        return 1;
    }
    static const char *th[4] = { "LX", "LY", "RX", "RY" };
    for (int i = 0; i < 4; i++)
        if (n > 3 && streq_n(a, 2, th[i]) && a[2] == '=') {
            int32_t v;
            const char *e = parse_int(a + 3, &v);
            if (!e || (size_t)(e - a) != n)
                return 0;
            s->has[i] = 1;
            s->thumb[i] = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
            return 1;
        }
    return 0;
}

int autopad_parse(const char *p)
{
    g_nsteps = 0;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
            p++;
        if (!*p)
            break;
        if (g_nsteps >= AP_MAX_STEPS)
            return -1;
        Step *s = &g_steps[g_nsteps];
        aimp_memset(s, 0, sizeof *s);
        if (*p == 'P' || *p == 'p') {
            s->rel_player = 1;
            p++;
        }
        if (!(p = parse_int(p, &s->t0)) || *p++ != '-' || !(p = parse_int(p, &s->t1)) || *p++ != ':')
            return -1;
        for (;;) {
            const char *a = p;
            while (*p && *p != '+' && *p != ',' && *p != '\n' && *p != '\r' && *p != ' ')
                p++;
            if (p == a || !parse_action(s, a, (size_t)(p - a)))
                return -1;
            if (*p == '+') {
                p++;
                continue;
            }
            break;
        }
        g_nsteps++;
    }
    return g_nsteps;
}

static void compute(int64_t ms_start, int64_t ms_player, uint16_t *buttons, int16_t thumbs[4], uint8_t trig[2],
                    LONG *has, int fire_marks)
{
    *buttons = 0;
    trig[0] = trig[1] = 0;
    *has = 0;
    for (int i = 0; i < 4; i++)
        thumbs[i] = 0;
    for (int i = 0; i < g_nsteps; i++) {
        Step *s = &g_steps[i];
        int64_t t = s->rel_player ? ms_player : ms_start - g_base_ms;
        int active = t >= 0 && t >= s->t0 && t < s->t1;
        if (!active)
            continue;
        if (s->go) {
            /* encadenado: espera a que terminen los GO anteriores que ya empezaron */
            int wait = 0;
            for (int j = 0; j < i; j++) {
                Step *q = &g_steps[j];
                int64_t tq = q->rel_player ? ms_player : ms_start - g_base_ms;
                if (q->go && !q->done && tq >= q->t0 && tq < q->t1)
                    wait = 1;
            }
            if (wait || s->done || !g_have_pose)
                continue;
        }
        if (fire_marks && !s->started) {
            s->started = 1;
            log_printf("AUTOPAD paso %d activo (%s%ld-%ld ms): botones %04x, LT %u RT %u", i + 1, s->rel_player ? "P" : "",
                       (long)s->t0, (long)s->t1, s->buttons, s->lt, s->rt);
            if (s->mark)
                probe_mark("AUTOPAD");
        }
        if (s->go && fire_marks) {
            float dx = s->gx - g_px, dz = s->gz - g_pz, d2 = dx * dx + dz * dz;
            if (d2 < 0.36f) {
                s->done = 1;
                g_go_best_ms = 0;
                log_printf("AUTOPAD GO paso %d: llegó a (%.2f, %.2f) con el jugador en (%.2f, %.2f)", i + 1, (double)s->gx,
                           (double)s->gz, (double)g_px, (double)g_pz);
                if (s->mark)
                    probe_mark("AUTOPAD GO");
                continue;
            }
            if (d2 < g_go_best - 0.04f || g_go_best_ms == 0) {
                g_go_best = d2;
                g_go_best_ms = ms_start;
            } else if (ms_start - g_go_best_ms > 4000) {
                s->done = 1;
                log_printf("AUTOPAD GO paso %d: trabado a %.2f m^2 de (%.2f, %.2f) en (%.2f, %.2f); se saltea", i + 1, (double)d2,
                           (double)s->gx, (double)s->gz, (double)g_px, (double)g_pz);
                g_go_best_ms = 0;
                continue;
            }
            /* rumbo = atan2(dx, dz) = yaw (corrida 19, caminando recto: diferencia 0,000; la corrida 14
             * daba pi/4 porque el usuario caminaba en diagonal). RX > 0 aumenta el yaw (corrida 18). */
            float err = wrap_pi(atan2_approx(dx, dz) - (g_yaw + g_yaw_off));
            float rx = err * 30000.0f;
            thumbs[2] = (int16_t)(rx > 32767 ? 32767 : rx < -32767 ? -32767 : rx);
            float ae = err < 0 ? -err : err;
            thumbs[1] = ae < 0.35f ? 32767 : ae < 0.8f ? 16000 : 0;
            *has |= 1 << 1 | 1 << 2;
        }
        *buttons |= s->buttons;
        if (s->lt)
            trig[0] = s->lt;
        if (s->rt)
            trig[1] = s->rt;
        for (int k = 0; k < 4; k++)
            if (s->has[k]) {
                thumbs[k] = s->thumb[k];
                *has |= 1 << k;
            }
    }
}

void autopad_state_at(int64_t ms_start, int64_t ms_player, uint16_t *buttons, int16_t thumbs[4], uint8_t trig[2])
{
    LONG has;
    compute(ms_start, ms_player, buttons, thumbs, trig, &has, 0);
}

static DWORD WINAPI my_get_state(DWORD idx, AState *st)
{
    DWORD r = g_get_state ? g_get_state(idx, st) : ERROR_DEVICE_NOT_CONNECTED;
    if (idx != 0 || !st)
        return r;
    InterlockedIncrement(&g_n_state_calls);
    if (r != ERROR_SUCCESS)
        aimp_memset(st, 0, sizeof *st);
    uint32_t bt = g_cur_buttons_trig;
    LONG has = g_cur_has;
    st->Gamepad.wButtons |= (WORD)(bt & 0xFFFF);
    if ((bt >> 16) & 0xFF)
        st->Gamepad.bLeftTrigger = (BYTE)(bt >> 16);
    if (bt >> 24)
        st->Gamepad.bRightTrigger = (BYTE)(bt >> 24);
    SHORT *th[4] = { &st->Gamepad.sThumbLX, &st->Gamepad.sThumbLY, &st->Gamepad.sThumbRX, &st->Gamepad.sThumbRY };
    for (int k = 0; k < 4; k++)
        if (has & (1 << k))
            *th[k] = (SHORT)g_cur_thumbs[k];
    st->dwPacketNumber = (DWORD)g_packet;
    return ERROR_SUCCESS;
}

static DWORD WINAPI my_get_caps(DWORD idx, DWORD flags, ACaps *caps)
{
    DWORD r = g_get_caps ? g_get_caps(idx, flags, caps) : ERROR_DEVICE_NOT_CONNECTED;
    if (idx != 0 || !caps || r == ERROR_SUCCESS)
        return r;
    aimp_memset(caps, 0, sizeof *caps);
    caps->Type = 1;    /* XINPUT_DEVTYPE_GAMEPAD */
    caps->SubType = 1; /* XINPUT_DEVSUBTYPE_GAMEPAD */
    caps->Gamepad.wButtons = 0xF3FF;
    caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 255;
    caps->Gamepad.sThumbLX = caps->Gamepad.sThumbLY = caps->Gamepad.sThumbRX = caps->Gamepad.sThumbRY = (SHORT)0xFFC0;
    return ERROR_SUCCESS;
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if ((*a | 0x20) != (*b | 0x20))
            return 0;
    return *a == *b;
}

/* Reemplaza en la IAT del módulo los imports por ordinal 2 y 4 de XINPUT1_3.dll. */
int autopad_install_on(uintptr_t base)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(base + (uint32_t)dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd->VirtualAddress)
        return 0;
    int patched = 0;
    for (const IMAGE_IMPORT_DESCRIPTOR *d = (const IMAGE_IMPORT_DESCRIPTOR *)(base + dd->VirtualAddress); d->Name; d++) {
        if (!ieq((const char *)(base + d->Name), "XINPUT1_3.dll"))
            continue;
        const IMAGE_THUNK_DATA32 *ot = (const IMAGE_THUNK_DATA32 *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA32 *ft = (IMAGE_THUNK_DATA32 *)(base + d->FirstThunk);
        for (; ot->u1.Ordinal; ot++, ft++) {
            if (!IMAGE_SNAP_BY_ORDINAL32(ot->u1.Ordinal))
                continue;
            uint32_t ord = IMAGE_ORDINAL32(ot->u1.Ordinal);
            void *mine = ord == 2 ? (void *)my_get_state : ord == 4 ? (void *)my_get_caps : NULL;
            if (!mine)
                continue;
            if (ft->u1.Function == (DWORD)(uintptr_t)mine) { /* ya instalado: no encadenar con nosotros mismos */
                patched++;
                continue;
            }
            DWORD old;
            if (!VirtualProtect(&ft->u1.Function, 4, PAGE_READWRITE, &old))
                continue;
            if (ord == 2)
                g_get_state = (GetStateFn)(uintptr_t)ft->u1.Function;
            else
                g_get_caps = (GetCapsFn)(uintptr_t)ft->u1.Function;
            ft->u1.Function = (DWORD)(uintptr_t)mine;
            VirtualProtect(&ft->u1.Function, 4, old, &old);
            patched++;
        }
    }
    return patched;
}

int autopad_install(void)
{
    wchar_t ws[1024];
    char s[1024];
    int n = 0, have_file = aimp_env_str(L"AIMP_AUTOPAD_FILE", g_file, MAX_PATH);
    s[0] = 0;
    if (aimp_env_str(L"AIMP_AUTOPAD", ws, 1024)) {
        aimp_utf8(s, sizeof s, ws);
        n = autopad_parse(s);
        if (n < 0) {
            log_printf("AUTOPAD: guion inválido (\"%s\"); queda apagado", s);
            return 0;
        }
    }
    if (n == 0 && !have_file)
        return 0;
    int p = autopad_install_on(g_aimp_exe);
    if (p == 0) {
        log_printf("AUTOPAD: no se encontraron los imports de XINPUT1_3 (ordinales 2 y 4); queda apagado");
        return 0;
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    g_t_start = t.QuadPart;
    g_yaw_off = (float)aimp_env_int(L"AIMP_AUTOPAD_YAWOFF", 0) / 1000.0f;
    g_on = 1;
    char f8[MAX_PATH * 3];
    log_printf("AUTOPAD (pruebas): mando virtual 0 con %d pasos, %d slots de la IAT reemplazados: \"%s\"%s%s", n, p, s,
               have_file ? "; archivo de comandos: " : "", have_file ? aimp_utf8(f8, sizeof f8, g_file) : "");
    return 1;
}

/* Si el archivo de comandos cambió, reemplaza el guion; los pasos sin 'P' cuentan desde ahora. */
static void check_file(int64_t ms_start)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!g_file[0] || !GetFileAttributesExW(g_file, GetFileExInfoStandard, &fa))
        return;
    if (fa.ftLastWriteTime.dwLowDateTime == g_file_time.dwLowDateTime &&
        fa.ftLastWriteTime.dwHighDateTime == g_file_time.dwHighDateTime)
        return;
    g_file_time = fa.ftLastWriteTime;
    HANDLE h = CreateFileW(g_file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return;
    char buf[2048];
    DWORD got = 0;
    ReadFile(h, buf, sizeof buf - 1, &got, NULL);
    CloseHandle(h);
    buf[got] = 0;
    int n = autopad_parse(buf);
    g_base_ms = ms_start;
    g_go_best_ms = 0;
    if (n < 0) {
        g_nsteps = 0;
        log_printf("AUTOPAD: archivo de comandos inválido; guion vacío");
    } else {
        for (char *q = buf; *q; q++)
            if (*q == '\n' || *q == '\r')
                *q = ' ';
        log_printf("AUTOPAD: archivo de comandos leído (%d pasos, desde t=%ld ms): \"%s\"", n, (long)ms_start, buf);
    }
}

void autopad_frame(int64_t now, int64_t freq, int have_player, const float *pos, float yaw)
{
    if (!g_on)
        return;
    g_have_pose = have_player && pos;
    if (g_have_pose) {
        g_px = pos[0];
        g_pz = pos[2];
        g_yaw = yaw;
    }
    if (have_player && !g_had_player) {
        g_t_player = now;
        for (int i = 0; i < g_nsteps; i++)
            if (g_steps[i].rel_player)
                g_steps[i].started = 0;
        log_printf("AUTOPAD: apareció el jugador 0; los pasos 'P' cuentan desde acá");
    }
    g_had_player = have_player;
    int64_t ms_start = (now - g_t_start) * 1000 / freq;
    if (g_file[0] && now >= g_next_file_check) {
        g_next_file_check = now + freq / 4;
        check_file(ms_start);
    }
    int64_t ms_player = have_player ? (now - g_t_player) * 1000 / freq : -1;
    uint16_t b;
    int16_t th[4];
    uint8_t tr[2];
    LONG has;
    compute(ms_start, ms_player, &b, th, tr, &has, 1);
    uint32_t bt = b | (uint32_t)tr[0] << 16 | (uint32_t)tr[1] << 24;
    int changed = bt != g_cur_buttons_trig || has != g_cur_has;
    for (int k = 0; k < 4; k++) {
        changed |= g_cur_thumbs[k] != th[k];
        g_cur_thumbs[k] = th[k];
    }
    g_cur_has = has;
    g_cur_buttons_trig = bt;
    if (changed)
        InterlockedIncrement(&g_packet);
}
