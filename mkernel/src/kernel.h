/* Cio' che i file del kernel si dicono tra loro. */
#ifndef KERNEL_H
#define KERNEL_H

/* console.c */
void console_pulisci(void);
unsigned char console_colore(unsigned char colore);   /* 0xSF: S sfondo, F primo piano. Restituisce il vecchio */
void console_putc(char c);
void console_scrivi(const char *s);
void console_dove(int *riga, int *colonna);           /* dove si trova il cursore */
void console_vai(int riga, int colonna);              /* sposta il cursore */

/* kprintf.c */
void kprintf(const char *fmt, ...);

/* kernel.c: errore senza rimedio, scrive il messaggio in rosso e ferma la CPU */
void panico(const char *fmt, ...) __attribute__((noreturn));

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
