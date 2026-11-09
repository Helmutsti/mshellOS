/* Le chiamate di sistema: come un programma in ring 3 chiede qualcosa al kernel.
 *
 * Il programma mette in EAX il numero del servizio, negli altri registri gli argomenti,
 * ed esegue "int 0x80". E' l'unica riga della IDT che il ring 3 puo' far scattare:
 * la CPU passa al ring 0 (sulla pila del kernel indicata dalla TSS), il kernel fa il
 * servizio, mette il risultato in EAX e la CPU torna al programma.
 *
 * Questo file lo includono sia il kernel sia i programmi: i numeri devono coincidere. */
#ifndef CHIAMATE_H
#define CHIAMATE_H

#define INT_SISTEMA 0x80   /* la porta: come Linux */

/* I servizi. Argomenti in EBX, ECX; risultato in EAX (-1 = errore). */
#define SIS_SCRIVI 1       /* scrivi(testo, byte): scrive sullo schermo */
#define SIS_DORMI  2       /* dormi(millisecondi): non gira per quel tempo */
#define SIS_ESCI   3       /* esci(): il programma finisce */

#endif
