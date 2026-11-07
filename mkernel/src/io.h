/* Le porte di I/O: uno spazio di indirizzi separato dalla memoria, solo per i dispositivi.
 * Non ci si arriva con mov: servono le istruzioni in e out. */
#ifndef IO_H
#define IO_H

/* Scrive un byte sulla porta. */
static inline void outb(unsigned short porta, unsigned char valore) {
    __asm__ volatile("outb %0, %1" : : "a"(valore), "Nd"(porta));
}

/* Legge un byte dalla porta. */
static inline unsigned char inb(unsigned short porta) {
    unsigned char valore;
    __asm__ volatile("inb %1, %0" : "=a"(valore) : "Nd"(porta));
    return valore;
}

/* Come sopra, ma 2 byte alla volta (il disco passa i dati cosi'). */
static inline void outw(unsigned short porta, unsigned short valore) {
    __asm__ volatile("outw %0, %1" : : "a"(valore), "Nd"(porta));
}

static inline unsigned short inw(unsigned short porta) {
    unsigned short valore;
    __asm__ volatile("inw %1, %0" : "=a"(valore) : "Nd"(porta));
    return valore;
}

/* Una piccola pausa: i chip vecchi come il PIC vogliono un attimo tra un comando e l'altro.
 * Scrivere sulla porta 0x80 (non usata) richiede circa un microsecondo. */
static inline void io_attesa(void) {
    outb(0x80, 0);
}

#endif
