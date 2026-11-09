/* Le chiamate di sistema: come un programma in ring 3 chiede qualcosa al kernel.
 *
 * Il programma mette in EAX il numero del servizio, negli altri registri gli argomenti,
 * ed esegue "int 0x80". E' l'unica riga della IDT che il ring 3 puo' far scattare:
 * la CPU passa al ring 0 (sulla pila del kernel indicata dalla TSS), il kernel fa il
 * servizio, mette il risultato in EAX e la CPU torna al programma.
 *
 * Sta in comune/ perche' lo usano sia il kernel (src/) sia i programmi (utente/):
 * e' il contratto tra i due, i numeri devono coincidere. */
#ifndef CHIAMATE_H
#define CHIAMATE_H

#define INT_SISTEMA 0x80   /* la porta: come Linux */

/* I servizi. Argomenti in EBX, ECX; risultato in EAX (-1 = errore). */
#define SIS_SCRIVI 1       /* scrivi(testo, byte): scrive sullo schermo */
#define SIS_DORMI  2       /* dormi(millisecondi): non gira per quel tempo */
#define SIS_ESCI   3       /* esci(): il programma finisce */
#define SIS_LEGGI  4       /* leggi(): aspetta un tasto e lo restituisce (-1: la tastiera
                            * e' gia' di un altro programma che sta aspettando) */
#define SIS_AVVIA  5       /* avvia(percorso): fa partire il programma del disco.
                            * Restituisce il suo numero di compito, o -1 */

#define MAX_PERCORSO 64    /* lettere di un percorso passato al kernel, compreso lo 0 finale */

/* Dove vivono i programmi nella loro mappa: tra 1 e 2 GB. Il codice e le variabili
 * dal basso, la pila appena sotto i 2 GB. Il resto della mappa e' del kernel
 * (invisibile al ring 3): la RAM sotto 1 GB, la memoria dello schermo sopra i 2 GB. */
#define PROGRAMMI_INIZIO    0x40000000u
#define PROGRAMMI_FINE      0x80000000u
#define PILA_PROGRAMMA_CIMA PROGRAMMI_FINE

#endif
