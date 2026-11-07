/* La GDT: la tabella dei segmenti, che oggi serve a dire "kernel" (ring 0) o "programma" (ring 3). */
#ifndef GDT_H
#define GDT_H

/* I selettori: numero di riga * 8, piu' il ring negli ultimi 2 bit.
 * E' questo valore che va nei registri di segmento (CS, DS, SS...). */
#define SEL_KERNEL_CODICE 0x08   /* riga 1, ring 0 */
#define SEL_KERNEL_DATI   0x10   /* riga 2, ring 0 */
#define SEL_UTENTE_CODICE 0x1B   /* riga 3, ring 3: 0x18 | 3 */
#define SEL_UTENTE_DATI   0x23   /* riga 4, ring 3: 0x20 | 3 */

/* Prepara la tabella, la consegna alla CPU e ricarica i registri di segmento. */
void gdt_prepara(void);

/* Stampa la tabella e i registri di segmento attuali. */
void gdt_mostra(void);

#endif
