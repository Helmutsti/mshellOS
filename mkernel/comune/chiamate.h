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

/* I servizi. Argomenti in EBX, ECX, EDX, ESI, EDI; risultato in EAX (-1 = errore). */
#define SIS_SCRIVI 1       /* scrivi(testo, byte): scrive sullo schermo */
#define SIS_DORMI  2       /* dormi(millisecondi): non gira per quel tempo */
#define SIS_ESCI   3       /* esci(): il programma finisce */
#define SIS_APRI_EVENTI 4  /* apri_eventi(): un descrittore da cui leggere gli eventi
                            * (tasti, mouse) con ricevi. -1 se e' gia' aperto da un altro
                            * programma: gli eventi vanno a uno solo, il compositore, che li
                            * passera' alla finestra giusta. Come /dev/input in Linux */
#define SIS_AVVIA  5       /* avvia(percorso): fa partire il programma del disco.
                            * Restituisce il suo numero di compito, o -1 */

#define SIS_SCHERMO 6      /* prendi_schermo(&info): lo schermo diventa di questo programma.
                            * Il kernel mette la memoria dello schermo nella sua mappa, a
                            * SCHERMO_PROGRAMMA, riempie 'info' e smette di scriverci.
                            * -1 se lo schermo e' gia' di un altro programma. Quando il
                            * programma finisce, lo schermo torna alla console del kernel. */

/* I SOCKET: i tubi tra programmi, come i socket locali di Linux "a pacchetti"
 * (SOCK_SEQPACKET). Ogni messaggio arriva intero e separato dagli altri.
 * Chi offre un servizio si mette in ascolto con un nome; chi lo vuole si collega a quel
 * nome. Per ogni collegamento accettato, chi ascolta riceve un descrittore suo: cosi' sa
 * sempre da chi arriva un messaggio (il kernel non lascia fingere di essere un altro).
 * Il nome vive solo nel kernel (come i nomi "astratti" di Linux): sparisce quando chi
 * ascolta chiude. */
#define SIS_ASCOLTA 7      /* ascolta(nome): un descrittore su cui arrivano i collegamenti.
                            * -1 se il nome e' gia' preso */
#define SIS_COLLEGA 8      /* collega(nome): un descrittore collegato a chi ascolta su
                            * 'nome'. -1 se nessuno ascolta con quel nome */
#define SIS_ACCETTA 9      /* accetta(d): il prossimo collegamento arrivato su d (dorme se
                            * non ce n'e'). Un descrittore nuovo, per parlare con lui */
#define SIS_MANDA   10     /* manda(d, dati, byte, allegato, modo): un messaggio (al massimo
                            * MAX_MESSAGGIO byte). Dorme se l'altro ha la casella piena,
                            * tranne con modo MANDA_SUBITO: allora -1. -1 anche se l'altro
                            * ha chiuso. 'allegato' e' il descrittore di un foglio da
                            * passare con il messaggio, o -1 */
#define SIS_RICEVI  11     /* ricevi(d, dove, byte, &allegato): il prossimo messaggio (dorme
                            * se non ce n'e'). Quanti byte, o 0 se l'altro ha chiuso. Se il
                            * messaggio e' piu' lungo di 'byte', il resto si perde. Se aveva
                            * un foglio allegato, in 'allegato' il descrittore NUOVO con cui
                            * questo programma lo tiene in mano; se no -1 */
#define SIS_CHIUDI  12     /* chiudi(d): il descrittore non serve piu' */
#define SIS_ASPETTA 13     /* aspetta(elenco, quanti): dorme finche' almeno uno dei
                            * descrittori dell'elenco ha qualcosa da ricevere (un messaggio,
                            * un collegamento, un evento, o l'altro che ha chiuso). Segna
                            * 'pronto' in ognuno e restituisce quanti sono pronti. Come poll */

/* I FOGLI: memoria condivisa tra programmi, come memfd di Linux. Un foglio e' un gruppo
 * di pagine che il kernel tiene per conto di chi l'ha creato. Si tiene in mano con un
 * descrittore, si mette nella propria memoria con mappa, e si passa a un altro
 * programma allegandolo a un messaggio: dopo, tutti e due scrivono e leggono LE STESSE
 * pagine. E' cosi' che un'applicazione da' al compositore il disegno della sua finestra.
 * Il foglio sparisce quando nessuno lo tiene piu' (ne' descrittori ne' mappe). */
#define SIS_FOGLIO  14     /* foglio(byte): un foglio nuovo, pieno di zeri. Il descrittore */
#define SIS_MAPPA   15     /* mappa(d, &byte): mette il foglio nella memoria del programma,
                            * nella zona dei fogli. L'indirizzo, o -1. In 'byte' (se non e'
                            * 0) quanto e' grande: chi riceve un foglio da un altro lo deve
                            * sapere, per non leggere oltre la fine */
#define SIS_TOGLI   16     /* togli(indirizzo): leva il foglio messo li' da mappa */

#define MANDA_SUBITO 1     /* il modo di manda che non aspetta mai */

#define MAX_NOME       32  /* lettere di un nome di socket, compreso lo 0 finale */
#define MAX_MESSAGGIO 512  /* byte di un messaggio */
#define MAX_GUARDATI   16  /* descrittori in un elenco di aspetta */

struct da_guardare {
    int descrittore;
    int pronto;            /* lo scrive il kernel: 1 se c'e' qualcosa da ricevere */
};

#define MAX_PERCORSO 64    /* lettere di un percorso passato al kernel, compreso lo 0 finale */

/* Com'e' fatto lo schermo, per chi lo riceve con SIS_SCHERMO. Ogni pixel e' di 4 byte;
 * rosso, verde e blu dicono a che bit sta ogni colore dentro il pixel. */
struct info_schermo {
    unsigned int passo;                /* byte da una riga di pixel alla successiva */
    unsigned int larghezza, altezza;   /* in pixel */
    unsigned int rosso, verde, blu;
};

/* Un evento: qualcosa che e' successo alla tastiera o al mouse. Come gli eventi di
 * Linux, il kernel dice solo COSA e' successo, non a quale finestra va. Si legge con
 * ricevi dal descrittore di apri_eventi, un evento per volta. */
#define EVENTO_TASTO 1     /* e' stato premuto un tasto: il carattere e' in 'tasto' */
#define EVENTO_MOUSE 2     /* il mouse si e' spostato di dx, dy, o un pulsante e' cambiato */

#define PULSANTE_SINISTRO 1   /* i bit di 'pulsanti': quelli tenuti premuti adesso */
#define PULSANTE_DESTRO   2
#define PULSANTE_CENTRALE 4

struct evento {
    unsigned int tipo;
    unsigned int tasto;      /* EVENTO_TASTO: il carattere (code page 437) */
    int dx, dy;              /* EVENTO_MOUSE: lo spostamento in pixel; dy positivo = in basso */
    unsigned int pulsanti;   /* EVENTO_MOUSE: PULSANTE_... tenuti premuti */
};

/* Dove vivono i programmi nella loro mappa: tra 1 e 2 GB. Il codice e le variabili
 * dal basso, i fogli da 1,5 GB, lo schermo (se lo chiedono) a 1,75 GB, la pila appena sotto i 2 GB.
 * Il resto della mappa e' del kernel (invisibile al ring 3): la RAM sotto 1 GB, la
 * memoria dello schermo sopra i 2 GB. */
#define PROGRAMMI_INIZIO    0x40000000u
#define FOGLI_INIZIO        0x60000000u   /* la zona dei fogli: 16 posti da 16 MB */
#define FOGLIO_MAX          0x01000000u   /* il foglio piu' grande */
#define MAX_MAPPATI         16
#define SCHERMO_PROGRAMMA   0x70000000u
#define PROGRAMMI_FINE      0x80000000u
#define PILA_PROGRAMMA_CIMA PROGRAMMI_FINE

#endif
