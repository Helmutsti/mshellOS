/* kprintf: la printf del kernel. Senza libreria standard va scritta a mano.
 * Supporta: %c %s %d %i %u %x %X %p %% con larghezza e zeri (%08x, %5d)
 * e il prefisso l per i long. */
#include <stdarg.h>
#include "kernel.h"

/* Scrive un numero in una base (2..16), riempiendo fino a 'larghezza'. */
static void numero(unsigned long v, int base, int maiusc, int negativo,
                   int larghezza, char riempi) {
    const char *cifre = maiusc ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[32];
    int n = 0;

    do { buf[n++] = cifre[v % base]; v /= base; } while (v);
    if (negativo) buf[n++] = '-';

    /* con gli zeri il segno va davanti: -0042 e non 00-42 */
    if (negativo && riempi == '0') {
        console_putc('-');
        n--;
        larghezza--;
    }
    for (int i = n; i < larghezza; i++) console_putc(riempi);
    while (n) console_putc(buf[--n]);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') { console_putc(*fmt); continue; }
        fmt++;

        char riempi = ' ';
        int larghezza = 0, lungo = 0;
        if (*fmt == '0') { riempi = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') larghezza = larghezza * 10 + (*fmt++ - '0');
        if (*fmt == 'l') { lungo = 1; fmt++; }

        switch (*fmt) {
        case 'c': console_putc((char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            console_scrivi(s ? s : "(null)");
            break;
        }
        case 'd': case 'i': {
            long v = lungo ? va_arg(ap, long) : va_arg(ap, int);
            int neg = v < 0;
            numero(neg ? -(unsigned long)v : (unsigned long)v, 10, 0, neg, larghezza, riempi);
            break;
        }
        case 'u': numero(lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, 0, 0, larghezza, riempi); break;
        case 'x': numero(lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 0, 0, larghezza, riempi); break;
        case 'X': numero(lungo ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 1, 0, larghezza, riempi); break;
        case 'p':
            console_scrivi("0x");
            numero((unsigned long)va_arg(ap, void *), 16, 0, 0, 8, '0');
            break;
        case '%': console_putc('%'); break;
        case '\0': va_end(ap); return;
        default:  console_putc('%'); console_putc(*fmt); break;
        }
    }
    va_end(ap);
}
