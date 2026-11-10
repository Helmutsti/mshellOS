/* Le quattro funzioni sulla memoria che il compilatore puo' chiamare da solo
 * (per esempio quando copia una struttura grande con '=') anche se noi non le usiamo.
 * Senza libreria C le dobbiamo scrivere noi: se mancano, il linker si ferma.
 *
 * Sono le stesse del kernel (src/lib/stringhe.c), ma un programma non puo' usare quelle:
 * ne ha una copia sua, unita a ogni programma come avvio.c. */
#include "libreria.h"

void *memset(void *dest, int valore, unsigned int n) {
    unsigned char *d = dest;
    while (n--) *d++ = (unsigned char)valore;
    return dest;
}

void *memcpy(void *dest, const void *src, unsigned int n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

/* come memcpy, ma funziona anche se le due zone si sovrappongono */
void *memmove(void *dest, const void *src, unsigned int n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dest;
}

int memcmp(const void *a, const void *b, unsigned int n) {
    const unsigned char *x = a, *y = b;
    for (; n--; x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}
