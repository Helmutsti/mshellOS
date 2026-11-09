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

/* Dove vivono i programmi nella loro mappa: da 1 GB in su il codice e le variabili,
 * la pila appena sotto i 2 GB. Sotto 1 GB c'e' il kernel (invisibile al ring 3). */
#define PROGRAMMI_INIZIO    0x40000000u
#define PILA_PROGRAMMA_CIMA 0x80000000u

#endif
