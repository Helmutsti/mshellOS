/* Cio' che i file del kernel si dicono tra loro. */
#ifndef KERNEL_H
#define KERNEL_H

#include <stdarg.h>

/* La CPU: indirizzo_t, le interruzioni (spegni, ripristina...) e il resto: arch.h. */
#include "arch.h"

/* Lo schermo a pixel, come ce lo descrive GRUB. */
struct schermo {
    indirizzo_t indirizzo;                 /* dove sta la memoria dello schermo */
    unsigned int passo;                    /* byte da una riga di pixel alla successiva */
    unsigned int larghezza, altezza, bpp;  /* pixel, pixel, bit per pixel */
    unsigned int rosso, verde, blu;        /* dove sta ogni colore dentro il pixel (bit) */
};

/* console.c */
int console_prepara(const struct schermo *s);         /* pulisce, cursore in alto. 0 se non sa usarlo */
unsigned char console_colore(unsigned char colore);   /* 0xSF: S sfondo, F primo piano. Restituisce il vecchio */
void console_putc(char c);
const struct schermo *console_schermo(void);          /* com'e' fatto lo schermo (0: non c'e') */
/* Presta lo schermo a 'chi' (un compito): la console smette di disegnare.
 * 0 se e' gia' prestato a qualcun altro. */
int console_presta(const void *chi);
/* Lo schermo torna alla console, che ridisegna tutto. Solo se e' prestato a 'chi';
 * con chi = 0 se lo riprende comunque (il panico). */
void console_riprendi(const void *chi);

/* kprintf.c */
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list ap);

/* stringhe.c (le funzioni di string.h che il compilatore puo' chiamare da solo) */
void *memset(void *dest, int valore, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
void *memmove(void *dest, const void *src, unsigned int n);
int memcmp(const void *a, const void *b, unsigned int n);

/* kernel.c: errore senza rimedio, scrive il messaggio in rosso e ferma la CPU */
void panico(const char *fmt, ...) __attribute__((noreturn));
void titolo(const char *t);   /* un titolo giallo sulla console, per le fasi dell'avvio */

/* Il kernel neutro (avvio/kernel.c): lo chiama l'avvio della CPU quando ha acceso lo
 * schermo, preparato le sue tabelle e dato la RAM al magazziniere. Non torna. */
void kernel_avvia(const struct schermo *s) __attribute__((noreturn));

#define NERO       0x0
#define BLU        0x1
#define VERDE      0x2
#define CIANO      0x3
#define ROSSO      0x4
#define GIALLO     0xE
#define BIANCO     0xF
#define GRIGIO     0x7

#endif
