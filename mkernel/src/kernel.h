/* Cio' che i file del kernel si dicono tra loro. */
#ifndef KERNEL_H
#define KERNEL_H

#include <stdarg.h>

/* console.c
 * La riga 0 dello schermo e' la BARRA DI STATO (orologio, contatori): non scorre.
 * Il testo normale usa le righe 1-24. */
void console_prepara(void);                           /* pulisce tutto, barra compresa */
void console_pulisci(void);                           /* pulisce il testo, non la barra */
unsigned char console_colore(unsigned char colore);   /* 0xSF: S sfondo, F primo piano. Restituisce il vecchio */
void console_putc(char c);
void console_scrivi(const char *s);
void console_dove(int *riga, int *colonna);           /* dove si trova il cursore */
void console_vai(int riga, int colonna);              /* sposta il cursore */
void console_stato(int colonna, unsigned char colore, const char *testo);  /* scrive nella barra */

/* kprintf.c */
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list ap);
int ksnprintf(char *buf, unsigned int max, const char *fmt, ...);   /* come kprintf, ma in buf */

/* memoria.c */
void *memset(void *dest, int valore, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int n);
void *memmove(void *dest, const void *src, unsigned int n);
int memcmp(const void *a, const void *b, unsigned int n);

/* kernel.c: errore senza rimedio, scrive il messaggio in rosso e ferma la CPU */
void panico(const char *fmt, ...) __attribute__((noreturn));

/* Le SEZIONI CRITICHE: pezzi di codice che devono girare tutti d'un fiato.
 * Con una sola CPU basta spegnere le interruzioni: niente tick, niente cambio di compito.
 *     unsigned int s = interruzioni_spegni();
 *     ... pezzo delicato, brevissimo ...
 *     interruzioni_ripristina(s);
 * Si rimette lo stato DI PRIMA (non si accende a prescindere): se erano gia' spente,
 * restano spente. Cosi' le sezioni critiche si possono annidare. */
static inline unsigned int interruzioni_spegni(void) {
    unsigned int eflags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(eflags) : : "memory");
    return eflags;
}

static inline void interruzioni_ripristina(unsigned int eflags) {
    if (eflags & 0x200)                   /* il bit IF: erano accese? */
        __asm__ volatile("sti" : : : "memory");
}

/* colori VGA (primo piano) */
#define NERO       0x0
#define BLU        0x1
#define VERDE      0x2
#define CIANO      0x3
#define ROSSO      0x4
#define GIALLO     0xE
#define BIANCO     0xF
#define GRIGIO     0x7

#endif
