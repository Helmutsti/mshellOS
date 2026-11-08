/* kprintf: la printf del kernel. Senza libreria standard va scritta a mano.
 * Supporta: %c %s %d %i %u %x %X %p %% con larghezza e zeri (%08x, %5d)
 * e il prefisso l per i long.
 *
 * Il lavoro vero lo fa formatta(), che non sa DOVE va il testo: per ogni carattere
 * chiama una funzione 'metti'. Cosi' la stessa formattazione serve sia per lo
 * schermo (kprintf) sia per un testo in memoria (ksnprintf). */
#include <stdarg.h>
#include "kernel.h"

typedef void (*metti_carattere)(char c, void *dove);

/* Scrive un numero in una base (2..16), riempiendo fino a 'larghezza'. */
static void numero(metti_carattere metti, void *dove, unsigned long v, int base,
                   int maiusc, int negativo, int larghezza, char riempi) {
    const char *cifre = maiusc ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int n = 0;

    do { buf[n++] = cifre[v % base]; v /= base; } while (v);
    if (negativo) buf[n++] = '-';

    /* con gli zeri il segno va davanti: -0042 e non 00-42 */
    if (negativo && riempi == '0') {
        metti('-', dove);
        n--;
        larghezza--;
    }
    for (int i = n; i < larghezza; i++) metti(riempi, dove);
    while (n) metti(buf[--n], dove);
}

static void formatta(metti_carattere metti, void *dove, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') { metti(*fmt, dove); continue; }
        fmt++;

        char riempi = ' ';
        int larghezza = 0, lungo = 0;
        if (*fmt == '0') { riempi = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') larghezza = larghezza * 10 + (*fmt++ - '0');
        if (*fmt == 'l') { lungo = 1; fmt++; }

        switch (*fmt) {
        case 'c': metti((char)va_arg(ap, int), dove); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            for (s = s ? s : "(null)"; *s; s++) metti(*s, dove);
            break;
        }
        case 'd': case 'i': {
            long v = lungo ? va_arg(ap, long) : va_arg(ap, int);
            int neg = v < 0;
            numero(metti, dove, neg ? -(unsigned long)v : (unsigned long)v, 10, 0, neg, larghezza, riempi);
            break;
        }
        case 'u': numero(metti, dove, lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, 0, 0, larghezza, riempi); break;
        case 'x': numero(metti, dove, lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 0, 0, larghezza, riempi); break;
        case 'X': numero(metti, dove, lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 1, 0, larghezza, riempi); break;
        case 'p':
            metti('0', dove); metti('x', dove);
            numero(metti, dove, (unsigned long)va_arg(ap, void *), 16, 0, 0, 8, '0');
            break;
        case '%': metti('%', dove); break;
        case '\0': return;
        default:  metti('%', dove); metti(*fmt, dove); break;
        }
    }
}

/* --- sullo schermo --- */

static void sullo_schermo(char c, void *dove) {
    (void)dove;
    console_putc(c);
}

void vkprintf(const char *fmt, va_list ap) {
    formatta(sullo_schermo, 0, fmt, ap);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vkprintf(fmt, ap);
    va_end(ap);
}

/* --- in un testo in memoria --- */

struct testo { char *buf; unsigned int pos, max; };

static void nel_testo(char c, void *dove) {
    struct testo *t = dove;
    if (t->pos + 1 < t->max) t->buf[t->pos++] = c;   /* lascia posto per lo 0 finale */
}

int ksnprintf(char *buf, unsigned int max, const char *fmt, ...) {
    struct testo t = { buf, 0, max };
    va_list ap;
    va_start(ap, fmt);
    formatta(nel_testo, &t, fmt, ap);
    va_end(ap);
    if (max) buf[t.pos] = 0;
    return t.pos;
}
