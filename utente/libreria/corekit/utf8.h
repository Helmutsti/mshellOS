/* CoreKit: UTF-8. Tutto il sistema scrive i testi in UTF-8 (i font, la tastiera, i campi di
 * testo, la shell, il terminale): un carattere sono da 1 a 4 byte, e quelli ASCII restano un
 * byte solo, identici a prima. Qui ci sono le operazioni che servono a chi lavora sui testi:
 * leggere un carattere, scriverlo, e muoversi tra un carattere e l'altro senza mai fermarsi
 * a meta' di uno. Solo funzioni inline: niente da collegare, le puo' includere chiunque
 * (anche GKit, che non conosce il resto di CoreKit).
 *
 * Un testo rovinato (un byte che non comincia niente, una sequenza interrotta, una forma
 * piu' lunga del necessario, un surrogato, un numero oltre U+10FFFF) non si rifiuta: ogni
 * byte sbagliato conta come UN carattere, U+FFFD, il "carattere sostitutivo". Cosi' un testo
 * sbagliato si vede, ma non rompe niente. */
#ifndef UTF8_H
#define UTF8_H

#define UTF8_SOSTITUTO 0xFFFDu   /* il carattere sostitutivo (U+FFFD) */

/* Legge il carattere che comincia in 's': ne mette il numero Unicode in *cp e restituisce
 * quanti byte occupa (da 1 a 4). Alla fine del testo (uno 0) restituisce 0 e *cp = 0. Un
 * byte sbagliato da' 1 byte e U+FFFD. */
static inline int utf8_leggi(const char *s, unsigned int *cp) {
    const unsigned char *u = (const unsigned char *)s;
    unsigned int c = u[0], minimo;
    int n;
    if (c == 0) { *cp = 0; return 0; }
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xC2 && c <= 0xDF)      { c &= 0x1F; n = 1; minimo = 0x80; }
    else if ((c & 0xF0) == 0xE0)     { c &= 0x0F; n = 2; minimo = 0x800; }
    else if (c >= 0xF0 && c <= 0xF4) { c &= 0x07; n = 3; minimo = 0x10000; }
    else { *cp = UTF8_SOSTITUTO; return 1; }
    for (int i = 1; i <= n; i++) {
        if ((u[i] & 0xC0) != 0x80) { *cp = UTF8_SOSTITUTO; return 1; }
        c = (c << 6) | (u[i] & 0x3F);
    }
    if (c < minimo || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) { *cp = UTF8_SOSTITUTO; return 1; }
    *cp = c;
    return n + 1;
}

/* Scrive il carattere 'cp' in 'dove' (servono 4 byte) e restituisce quanti byte ha scritto.
 * Un numero che non e' un carattere (surrogato, oltre U+10FFFF) diventa U+FFFD. Non scrive
 * lo 0 finale. */
static inline int utf8_scrivi(char *dove, unsigned int cp) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = UTF8_SOSTITUTO;
    if (cp < 0x80) { dove[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        dove[0] = (char)(0xC0 | cp >> 6);
        dove[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        dove[0] = (char)(0xE0 | cp >> 12);
        dove[1] = (char)(0x80 | (cp >> 6 & 0x3F));
        dove[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    dove[0] = (char)(0xF0 | cp >> 18);
    dove[1] = (char)(0x80 | (cp >> 12 & 0x3F));
    dove[2] = (char)(0x80 | (cp >> 6 & 0x3F));
    dove[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Dove finisce il carattere che comincia al byte 'i' di 's' (cioe' dove comincia il
 * prossimo). Alla fine del testo resta dov'e'. */
static inline int utf8_dopo(const char *s, int i) {
    unsigned int cp;
    return i + utf8_leggi(s + i, &cp);
}

/* Dove comincia il carattere che finisce al byte 'fine' di 's' (cioe' il carattere prima di
 * 'fine'). Con 'fine' = 0 restituisce 0. Serve a cancellare un carattere intero. Se i byte
 * prima di 'fine' non formano un carattere giusto, e' un byte rovinato e conta per uno. */
static inline int utf8_prima(const char *s, int fine) {
    int i = fine;
    while (i > 0 && fine - i < 4) {
        i--;
        if (((unsigned char)s[i] & 0xC0) != 0x80) break;   /* un byte di testa: il carattere comincia qui */
    }
    unsigned int cp;
    if (i < fine && utf8_leggi(s + i, &cp) != fine - i) return fine - 1;
    return i;
}

/* Quanti byte di 'da' (UTF-8) si possono copiare in al massimo 'massimo' byte senza tagliare un
 * carattere a meta': tutto 'da' se ci sta, se no fino all'ultimo carattere intero. Serve a chi
 * copia un testo in un posto di misura fissa. */
static inline int utf8_entra(const char *da, int massimo) {
    int i = 0;
    while (i < massimo && da[i]) i++;
    if (da[i])                                              /* tagliato: non a meta' di un carattere */
        while (i > 0 && ((unsigned char)da[i] & 0xC0) == 0x80) i--;
    return i;
}

#endif
