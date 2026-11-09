/* La GDT: la tabella dei segmenti, che oggi serve a dire "kernel" (ring 0) o "programma" (ring 3). */
#ifndef GDT_H
#define GDT_H

/* I selettori: numero di riga * 8, piu' il ring negli ultimi 2 bit.
 * E' questo valore che va nei registri di segmento (CS, DS, SS...). */
#define SEL_KERNEL_CODICE 0x08   /* riga 1, ring 0 */
#define SEL_KERNEL_DATI   0x10   /* riga 2, ring 0 */
#define SEL_UTENTE_CODICE 0x1B   /* riga 3, ring 3: 0x18 | 3 */
#define SEL_UTENTE_DATI   0x23   /* riga 4, ring 3: 0x20 | 3 */
#define SEL_TSS           0x28   /* riga 5: la TSS (solo il kernel la carica, ring 0) */

/* Prepara la tabella, la consegna alla CPU, ricarica i registri di segmento
 * e dice alla CPU dov'e' la TSS. */
void gdt_prepara(void);

/* Scrive nella TSS la cima della pila del kernel da usare alla prossima
 * interruzione che arriva mentre gira codice in ring 3. */
void tss_pila_kernel(unsigned int cima);

/* Stampa dove sta la GDT e i registri di segmento; con dettagli=1 anche le righe. */
void gdt_mostra(int dettagli);

#endif
