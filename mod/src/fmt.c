/* printf mínimo sin CRT (snprintf de ntdll no soporta punto flotante). */
#include "aimp.h"

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} Out;

static void put(Out *o, char c)
{
    if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static void put_n(Out *o, const char *s, size_t n)
{
    while (n--)
        put(o, *s++);
}

static void pad(Out *o, char c, int n)
{
    while (n-- > 0)
        put(o, c);
}

enum { F_LEFT = 1, F_ZERO = 2, F_PLUS = 4, F_SPACE = 8, F_ALT = 16 };

/* Emite [signo][prefijo][ceros][dígitos] respetando ancho y flags. */
static void emit_num(Out *o, const char *sign, const char *digits, int ndig,
                     int zeros, int width, int flags)
{
    int slen = (int)aimp_strlen(sign);
    int total = slen + zeros + ndig;
    if (!(flags & F_LEFT) && !(flags & F_ZERO))
        pad(o, ' ', width - total);
    put_n(o, sign, (size_t)slen);
    if (!(flags & F_LEFT) && (flags & F_ZERO))
        pad(o, '0', width - total);
    pad(o, '0', zeros);
    put_n(o, digits, (size_t)ndig);
    if (flags & F_LEFT)
        pad(o, ' ', width - total);
}

static int u64_digits(char *tmp, uint64_t v, unsigned base, int upper)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char rev[24];
    int n = 0;
    do {
        rev[n++] = dig[v % base];
        v /= base;
    } while (v);
    for (int i = 0; i < n; i++)
        tmp[i] = rev[n - 1 - i];
    return n;
}

static const char *sign_for(int neg, int flags)
{
    if (neg)
        return "-";
    if (flags & F_PLUS)
        return "+";
    if (flags & F_SPACE)
        return " ";
    return "";
}

/* Punto flotante: fijo con prec decimales; notación científica si es enorme.
 * Redondeo al par en empates exactos (como el CRT). Para valores que en binario
 * caen justo en el borde (p. ej. 0.995) el último dígito puede diferir del CRT,
 * que convierte en forma exacta. Para un log alcanza. */
static void emit_double(Out *o, double v, int prec, int width, int flags, int sci)
{
    char tmp[64];
    int n = 0;
    union { double d; uint64_t u; } bits = { v };
    int neg = (int)(bits.u >> 63);
    if (v != v) {
        emit_num(o, "", "nan", 3, 0, width, flags & ~F_ZERO);
        return;
    }
    if (neg)
        v = -v;
    if (v > 1.7976931348623157e308) {
        emit_num(o, sign_for(neg, flags), "inf", 3, 0, width, flags & ~F_ZERO);
        return;
    }
    if (prec < 0)
        prec = 6;
    if (prec > 9)
        prec = 9;
    int exp10 = 0;
    if (sci || v >= 1e15) {
        sci = 1;
        if (v != 0.0) {
            while (v >= 10.0) {
                v /= 10.0;
                exp10++;
            }
            while (v < 1.0) {
                v *= 10.0;
                exp10--;
            }
        }
    }
    uint64_t scale = 1;
    for (int i = 0; i < prec; i++)
        scale *= 10;
    uint64_t whole = (uint64_t)v;
    double t = (v - (double)whole) * (double)scale;
    uint64_t f = (uint64_t)t;
    double rem = t - (double)f;
    int odd = prec > 0 ? (int)(f & 1) : (int)(whole & 1);
    if (rem > 0.5 || (rem == 0.5 && odd))
        f++;
    if (f >= scale) {
        whole++;
        f -= scale;
        if (sci && whole >= 10) {
            whole = 1;
            exp10++;
        }
    }
    n = u64_digits(tmp, whole, 10, 0);
    if (prec > 0) {
        char fd[24];
        int fn = u64_digits(fd, f, 10, 0);
        tmp[n++] = '.';
        for (int i = fn; i < prec; i++)
            tmp[n++] = '0';
        for (int i = 0; i < fn; i++)
            tmp[n++] = fd[i];
    }
    if (sci) {
        tmp[n++] = 'e';
        tmp[n++] = exp10 < 0 ? '-' : '+';
        int e = exp10 < 0 ? -exp10 : exp10;
        char ed[8];
        int en = u64_digits(ed, (uint64_t)e, 10, 0);
        if (en < 2)
            tmp[n++] = '0';
        for (int i = 0; i < en; i++)
            tmp[n++] = ed[i];
    }
    emit_num(o, sign_for(neg, flags), tmp, n, 0, width, flags);
}

int aimp_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    Out o = { buf, cap, 0 };
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            put(&o, *p);
            continue;
        }
        p++;
        int flags = 0, width = 0, prec = -1, lng = 0;
        for (;; p++) {
            if (*p == '-') flags |= F_LEFT;
            else if (*p == '0') flags |= F_ZERO;
            else if (*p == '+') flags |= F_PLUS;
            else if (*p == ' ') flags |= F_SPACE;
            else if (*p == '#') flags |= F_ALT;
            else break;
        }
        if (*p == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                flags |= F_LEFT;
                width = -width;
            }
            p++;
        } else {
            while (*p >= '0' && *p <= '9')
                width = width * 10 + (*p++ - '0');
        }
        if (*p == '.') {
            p++;
            prec = 0;
            if (*p == '*') {
                prec = va_arg(ap, int);
                p++;
            } else {
                while (*p >= '0' && *p <= '9')
                    prec = prec * 10 + (*p++ - '0');
            }
        }
        /* lng: 0 int, 1 long, 2 long long, -1 short, -2 char, 3 size_t */
        if (*p == 'h') {
            lng = -1;
            if (*++p == 'h') {
                lng = -2;
                p++;
            }
        } else if (*p == 'l') {
            lng = 1;
            if (*++p == 'l') {
                lng = 2;
                p++;
            }
        } else if (*p == 'z') {
            lng = 3;
            p++;
        }
        char tmp[32];
        switch (*p) {
        case 'd':
        case 'i': {
            int64_t v;
            if (lng == 2) v = va_arg(ap, long long);
            else if (lng == 1) v = va_arg(ap, long);
            else if (lng == 3) v = (int64_t)(intptr_t)va_arg(ap, size_t);
            else v = va_arg(ap, int);
            if (lng == -1) v = (short)v;
            if (lng == -2) v = (signed char)v;
            uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
            int n = (prec == 0 && u == 0) ? 0 : u64_digits(tmp, u, 10, 0);
            int zeros = prec > n ? prec - n : 0;
            emit_num(&o, sign_for(v < 0, flags), tmp, n, zeros, width,
                     prec >= 0 ? flags & ~F_ZERO : flags);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            uint64_t v;
            if (lng == 2) v = va_arg(ap, unsigned long long);
            else if (lng == 1) v = va_arg(ap, unsigned long);
            else if (lng == 3) v = va_arg(ap, size_t);
            else v = va_arg(ap, unsigned int);
            if (lng == -1) v = (unsigned short)v;
            if (lng == -2) v = (unsigned char)v;
            unsigned base = *p == 'u' ? 10 : 16;
            int n = (prec == 0 && v == 0) ? 0 : u64_digits(tmp, v, base, *p == 'X');
            int zeros = prec > n ? prec - n : 0;
            const char *pre = (flags & F_ALT) && v && base == 16 ? (*p == 'X' ? "0X" : "0x") : "";
            emit_num(&o, pre, tmp, n, zeros, width, prec >= 0 ? flags & ~F_ZERO : flags);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            int n = u64_digits(tmp, v, 16, 0);
            emit_num(&o, "0x", tmp, n, 8 - n > 0 ? 8 - n : 0, width, flags & ~F_ZERO);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            int n = (int)aimp_strlen(s);
            if (prec >= 0 && n > prec)
                n = prec;
            if (!(flags & F_LEFT))
                pad(&o, ' ', width - n);
            put_n(&o, s, (size_t)n);
            if (flags & F_LEFT)
                pad(&o, ' ', width - n);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!(flags & F_LEFT))
                pad(&o, ' ', width - 1);
            put(&o, c);
            if (flags & F_LEFT)
                pad(&o, ' ', width - 1);
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
            emit_double(&o, va_arg(ap, double), prec, width, flags, *p == 'e' || *p == 'E');
            break;
        case '%':
            put(&o, '%');
            break;
        case 0:
            p--; /* formato truncado: terminar */
            break;
        default:
            put(&o, '%');
            put(&o, *p);
            break;
        }
    }
    if (cap)
        buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}

int aimp_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = aimp_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
