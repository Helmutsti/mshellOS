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

#define SIS_SCHERMO 6      /* prendi_schermo(&info): lo schermo diventa di questo programma.
                            * Il kernel mette la memoria dello schermo nella sua mappa, a
                            * SCHERMO_PROGRAMMA, riempie 'info' e smette di scriverci.
                            * -1 se lo schermo e' gia' di un altro programma. Quando il
                            * programma finisce, lo schermo torna alla console del kernel. */

#define MAX_PERCORSO 64    /* lettere di un percorso passato al kernel, compreso lo 0 finale */

/* Com'e' fatto lo schermo, per chi lo riceve con SIS_SCHERMO. Ogni pixel e' di 4 byte;
 * rosso, verde e blu dicono a che bit sta ogni colore dentro il pixel. */
struct info_schermo {
    unsigned int passo;                /* byte da una riga di pixel alla successiva */
    unsigned int larghezza, altezza;   /* in pixel */
    unsigned int rosso, verde, blu;
};

/* Dove vivono i programmi nella loro mappa: tra 1 e 2 GB. Il codice e le variabili
 * dal basso, lo schermo (se lo chiedono) a 1,75 GB, la pila appena sotto i 2 GB.
 * Il resto della mappa e' del kernel (invisibile al ring 3): la RAM sotto 1 GB, la
 * memoria dello schermo sopra i 2 GB. */
#define PROGRAMMI_INIZIO    0x40000000u
#define SCHERMO_PROGRAMMA   0x70000000u
#define PROGRAMMI_FINE      0x80000000u
#define PILA_PROGRAMMA_CIMA PROGRAMMI_FINE

#endif
