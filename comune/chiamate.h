/* Le chiamate di sistema: come un programma in ring 3 chiede qualcosa al kernel.
 *
 * Sul PC il programma mette in RAX il numero del servizio, in RDI, RSI, RDX, R10, R8 gli
 * argomenti, ed esegue "int 0x80". E' l'unica riga della IDT che il ring 3 puo' far
 * scattare: la CPU passa al ring 0 (sulla pila del kernel indicata dalla TSS), il kernel
 * fa il servizio, mette il risultato in RAX e la CPU torna al programma. Su ARM64 e'
 * "svc #0", con il numero in x8 e gli argomenti in x0-x4 (utente/libreria/libreria.h).
 *
 * Sta in comune/ perche' lo usano sia il kernel (src/) sia i programmi (utente/):
 * e' il contratto tra i due, i numeri devono coincidere. */
#ifndef CHIAMATE_H
#define CHIAMATE_H

#define INT_SISTEMA 0x80   /* la porta: come Linux */

/* I servizi. Fino a 5 argomenti; il risultato -1 vuol dire errore. */
#define SIS_SCRIVI 1       /* scrivi(testo, byte): scrive sullo schermo */
#define SIS_DORMI  2       /* dormi(millisecondi): non gira per quel tempo */
#define SIS_ESCI   3       /* esci(): il programma finisce */
#define SIS_APRI_EVENTI 4  /* apri_eventi(): un descrittore da cui leggere gli eventi
                            * (tasti, mouse) con ricevi. -1 se e' gia' aperto da un altro
                            * programma: gli eventi vanno a uno solo, il compositore, che li
                            * passera' alla finestra giusta. Come /dev/input in Linux */
#define SIS_AVVIA  5       /* avvia(percorso): fa partire il programma del disco, con lo
                            * stesso uid di chi lo chiede (serve il permesso di eseguirlo).
                            * Restituisce il suo numero di compito, o -1 */

#define SIS_APRI_SCHERMO 6 /* apri_schermo(): un descrittore dello schermo, che la console
                            * del kernel smette di usare; -1 se e' gia' aperto. Torna alla
                            * console quando nessuno lo tiene piu'. Come /dev/fb0 */

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
#define SIS_ASPETTA 13     /* aspetta(elenco, quanti, ms): dorme finche' almeno uno dei
                            * descrittori dell'elenco ha qualcosa da ricevere (un messaggio,
                            * un collegamento, un evento, o l'altro che ha chiuso), o finche'
                            * non passano ms millisecondi (-1: senza limite). Segna 'pronto'
                            * in ognuno e restituisce quanti sono pronti (0: tempo scaduto).
                            * Come poll */

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

#define SIS_TEMPO   17     /* tempo(): i millisecondi passati dall'avvio */
#define SIS_TERMINA 18     /* termina(d): chiude il programma che sta all'altro capo del
                            * collegamento d. Si puo' chiudere solo chi e' collegato a noi:
                            * il compositore le sue applicazioni, non un programma a caso */

#define SIS_ROTTURA 19     /* rottura(indirizzo): sposta il confine della memoria dinamica
                            * del programma (il "break" di Unix). Restituisce il confine
                            * dopo la chiamata: quello nuovo, o quello di prima se
                            * l'indirizzo non va bene; rottura(0) dice solo dov'e'. Le
                            * pagine sotto il confine arrivano la prima volta che si
                            * toccano. Di solito la usa solo malloc */

/* I FILE E LE CARTELLE: un unico albero, che comincia da "/" (il disco del sistema,
 * ext4, per ora in sola lettura). Un file aperto e' un descrittore come gli altri: si
 * legge con ricevi e si scrive con manda, a partire dalla sua POSIZIONE, che va avanti
 * da sola (come read e write in Unix). Una cartella aperta si legge una voce alla volta:
 * ogni ricevi da' la prossima struct voce_cartella, 0 quando sono finite.
 * I percorsi sono assoluti (cominciano con /); "." e ".." valgono come in Unix, e i
 * collegamenti simbolici si seguono. Se qualcosa va storto le chiamate dei file
 * restituiscono un ERRORE_... (negativo, qui sotto). */
#define SIS_APRI          20   /* apri(percorso, modo): un descrittore, o un errore. 'modo'
                                * e' una somma di APRI_... */
#define SIS_POSIZIONA     21   /* posiziona(d, byte): la prossima lettura o scrittura
                                * comincia da li' (non oltre la fine; per una cartella: la
                                * voce numero 'byte', 0 per ricominciare). La posizione */
#define SIS_INFO          22   /* info(percorso, &info): che cos'e', quanto e' grande... */
#define SIS_CREA_CARTELLA 23   /* crea_cartella(percorso) */
#define SIS_CANCELLA      24   /* cancella(percorso): un file, o una cartella vuota */
#define SIS_RINOMINA      25   /* rinomina(da, a): rinomina o sposta, anche in un'altra
                                * cartella. Un file gia' presente in 'a' si sostituisce
                                * (cosi' si salva in modo sicuro); una cartella no */

#define APRI_LEGGERE   1
#define APRI_SCRIVERE  2
#define APRI_CREARE    4       /* se non c'e', lo si crea vuoto */
#define APRI_SVUOTARE  8       /* lo si svuota prima di cominciare (serve anche SCRIVERE) */

#define TIPO_FILE         1
#define TIPO_CARTELLA     2
#define TIPO_COLLEGAMENTO 3    /* collegamento simbolico (le voci delle cartelle lo dicono) */
#define TIPO_ALTRO        4    /* dispositivi, code...: non si aprono */
#define MAX_NOME_FILE   256    /* byte di un nome (UTF-8), compreso lo 0 finale */

struct info_file {
    unsigned int tipo;               /* TIPO_... */
    unsigned int permessi;           /* come in Unix, in ottale: 0755, 0644... */
    unsigned int proprietario;       /* l'uid di chi l'ha creato */
    unsigned int collegamenti;       /* quanti nomi ha */
    unsigned long long dimensione;   /* byte */
    long long modificato;            /* l'ultima modifica: secondi dal 1/1/1970 (UTC) */
};

/* Gli errori delle chiamate dei file, sempre negativi: gli stessi numeri di VFS_ERR_ nel
 * kernel (src/fs/vfs.h). -1 lo danno anche le richieste sbagliate (un indirizzo fuori
 * dalla memoria del programma, un descrittore che non c'e'). */
#define ERRORE_DISCO          -1
#define ERRORE_FORMATO        -2    /* il disco e' rovinato */
#define ERRORE_NON_SUPPORTATO -3
#define ERRORE_NON_TROVATO    -4
#define ERRORE_ESISTE         -5
#define ERRORE_NON_CARTELLA   -6
#define ERRORE_E_CARTELLA     -7
#define ERRORE_NON_VUOTA      -8
#define ERRORE_PIENO          -9
#define ERRORE_NOME          -10    /* percorso scritto male, o nome troppo lungo */
#define ERRORE_PERMESSO      -11
#define ERRORE_SOLA_LETTURA  -12    /* il disco (o il formato) non si puo' scrivere */
#define ERRORE_MEMORIA       -13
#define ERRORE_TROPPI_LINK   -14

/* GLI UTENTI E I PERMESSI, come in Unix. Ogni programma ha un uid: 0 e' il SISTEMA (puo'
 * tutto: il kernel non gli controlla niente), 1000 le APPLICAZIONI. Un programma nuovo
 * prende l'uid di chi lo fa partire; solo il sistema puo' farne partire uno con un uid
 * diverso (avvia_come), e nessuno puo' salire a 0.
 * Ogni file ha un proprietario e i permessi: tre cifre ottali, ognuna la somma di
 * PERMESSO_LEGGERE, _SCRIVERE, _ESEGUIRE; la prima per il proprietario, l'ultima per
 * tutti gli altri (quella di mezzo, il gruppo, per ora non si usa). Per creare,
 * cancellare o rinominare qualcosa bisogna poter scrivere nella cartella che lo contiene.
 * Un file nuovo e' di chi lo crea, con 0644; una cartella nuova con 0755.
 *
 * /home e' la casa di chi chiede: per un programma con uid U e' /Utenti/U (il kernel
 * traduce il percorso). Le applicazioni ci trovano Apps/<Nome>, per i loro dati, e
 * Scrivania. */
#define UID_SISTEMA       0
#define UID_APPLICAZIONI  1000
#define PERMESSO_LEGGERE  4
#define PERMESSO_SCRIVERE 2
#define PERMESSO_ESEGUIRE 1

#define SIS_PERMESSI      26   /* permessi(percorso, proprietario, permessi): li cambia. Il
                                * sistema puo' tutto; gli altri solo i permessi dei loro
                                * file (il proprietario lo cambia solo il sistema) */
#define SIS_AVVIA_COME    27   /* avvia_come(percorso, uid): come avvia, ma il programma
                                * nuovo ha quell'uid. Solo per il sistema */
#define SIS_CHI           28   /* chi(d): l'uid del programma all'altro capo del
                                * collegamento d, o -1. Il kernel lo sa, non si puo'
                                * fingere: cosi' un servizio sa se parla con il sistema */

struct voce_cartella {
    unsigned int tipo;               /* TIPO_... (un collegamento non si segue) */
    unsigned int permessi;
    unsigned long long dimensione;   /* byte */
    long long modificato;            /* secondi dal 1/1/1970 (UTC) */
    char nome[MAX_NOME_FILE];
};

#define SIS_MAPPA_SCHERMO 29 /* mappa_schermo(d, &info): mette la memoria dello schermo
                              * del descrittore d nella memoria del programma, a
                              * SCHERMO_PROGRAMMA, e dice com'e' fatto. 0, o -1 */
#define SIS_AVVIA_CON     30 /* avvia_con(percorso, descrittori, quanti): come avvia, ma il
                              * programma nuovo trova gia' aperti quei descrittori, ai posti
                              * 0, 1, 2... (al massimo MAX_PASSATI). Cosi' init presta
                              * schermo, tastiera e il nome del compositore. Un collegamento
                              * (un capo di socket) non si passa */
#define SIS_PROCESSO      31 /* processo(numero): un descrittore del programma 'numero' (quello
                              * che danno avvia, avvia_come, avvia_con): ricevi aspetta che
                              * finisca e da' 0, e aspetta lo vede pronto quando e' finito.
                              * Come pidfd in Linux */
#define MAX_PASSATI 8
#define SIS_DATA_ORA      32 /* data_ora(&d): la data e l'ora, dall'orologio del PC */
#define SIS_PROGRAMMA_DI  33 /* programma_di(d, dove): il percorso del programma all'altro
                              * capo del collegamento d (per esempio
                              * "/Applicazioni/Contatore/Contatore"), in 'dove' (MAX_PERCORSO
                              * byte). Come chi, il kernel lo sa e non si puo' fingere. 0, o -1 */

/* I PROCESSI E LA MEMORIA, per chi vuole guardarli (il task manager): come /proc in
 * Linux. Chiunque puo' vedere l'elenco; terminare un processo invece segue la regola di
 * Unix: il sistema (uid 0) puo' terminare tutti, gli altri solo i processi del loro uid.
 * Init (il numero 1) non si termina: senza di lui non c'e' sistema. */
#define SIS_ELENCO_PROCESSI  34   /* elenco_processi(v, max): i processi in v (al massimo
                                   * max). Quanti sono */
#define SIS_TERMINA_PROCESSO 35   /* termina_processo(numero): 0, o -1 se non si puo' */
#define SIS_MEMORIA          36   /* stato_memoria(&m): com'e' usata la RAM */

#define STATO_PRONTO        0     /* aspetta il suo turno */
#define STATO_IN_ESECUZIONE 1     /* sta girando adesso */
#define STATO_DORME         2     /* dorme per un po' (dormi) */
#define STATO_IN_ATTESA     3     /* aspetta qualcosa: un messaggio, un evento */
#define STATO_FINITO        4

struct info_processo {
    int numero;
    unsigned int uid;
    unsigned int stato;            /* STATO_... */
    unsigned int memoria;          /* byte: le pagine che sono sue (non quelle condivise) */
    char nome[16];
};

struct info_memoria {              /* in byte, a 64 bit: la RAM puo' superare i 4 GB */
    unsigned long long totale;     /* byte di RAM */
    unsigned long long libera;     /* byte che nessuno usa */
    unsigned long long condivisa;  /* byte dei fogli condivisi: le immagini delle finestre */
};

struct data_ora {
    unsigned int anno, mese, giorno;   /* mese da 1 a 12 */
    unsigned int giorno_settimana;     /* 0 domenica, 1 lunedi'... */
    unsigned int ore, minuti, secondi;
};

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
#define EVENTO_PUNTATORE 3 /* una tavoletta: il puntatore e' in (dx, dy), da 0 a PUNTATORE_MAX
                            * su ogni asse (lo schermo intero), e questi sono i pulsanti */
#define PUNTATORE_MAX 32767
#define EVENTO_ROTELLA 4   /* la rotella del mouse ha girato di dy "scatti" (dx per quella
                            * orizzontale, se c'e'). Come gli spostamenti, dy positivo = verso
                            * il basso: la rotella girata verso di se', per vedere il contenuto
                            * che sta piu' sotto. Non dice dov'e' il puntatore: lo sa il
                            * compositore, dagli eventi di prima */

#define PULSANTE_SINISTRO 1   /* i bit di 'pulsanti': quelli tenuti premuti adesso */
#define PULSANTE_DESTRO   2
#define PULSANTE_CENTRALE 4

struct evento {
    unsigned int tipo;
    unsigned int tasto;      /* EVENTO_TASTO: il carattere (code page 437) */
    int dx, dy;              /* EVENTO_MOUSE: lo spostamento in pixel; dy positivo = in basso.
                              * EVENTO_PUNTATORE: la posizione, da 0 a PUNTATORE_MAX.
                              * EVENTO_ROTELLA: gli scatti; dy positivo = in basso */
    unsigned int pulsanti;   /* EVENTO_MOUSE, EVENTO_PUNTATORE, EVENTO_ROTELLA: PULSANTE_...
                              * tenuti premuti */
};

/* Dove vivono i programmi nella loro mappa. Gli indirizzi sono divisi in due meta', come
 * in Linux, Windows e macOS (la pianta intera e' in src/memoria/pagine.h):
 *   - la META' BASSA (da 0 a 128 TB) e' dei programmi: ogni mappa ha la sua;
 *   - la META' ALTA (gli indirizzi che cominciano con 0xFFFF...) e' del kernel, uguale in
 *     tutte le mappe, e la CPU non la lascia toccare ai programmi.
 * Cosi' un programma e il kernel non si pestano mai i piedi, comunque sia fatta la
 * macchina: la RAM e i registri delle schede il kernel li vede in alto, a indirizzi suoi,
 * qualunque sia il loro indirizzo fisico.
 * Dentro la meta' bassa i programmi usano i primi 2 GB: il codice e le variabili da 4 MB
 * (sotto non c'e' niente: un puntatore nullo, o quasi, fa errore), la memoria dinamica
 * dopo il codice, i fogli da 1,5 GB, lo schermo (se lo chiedono) da 1,75 GB, la pila in
 * cima, sotto i 2 GB. Restare sotto i 2 GB permette di compilarli nel modo normale (ogni
 * indirizzo sta in 32 bit dentro le istruzioni: vedi build.sh). Su tutte e due le CPU. */
#define PROGRAMMI_INIZIO    0x00400000ul   /* 4 MB */
#define FOGLI_INIZIO        0x60000000ul   /* 1,5 GB: la zona dei fogli, 16 posti da 16 MB */
#define FOGLIO_MAX          0x01000000u    /* il foglio piu' grande */
#define MAX_MAPPATI         16
#define SCHERMO_PROGRAMMA   0x70000000ul   /* 1,75 GB */
#define PROGRAMMI_FINE      0x80000000ul   /* 2 GB */
#define PILA_PROGRAMMA_CIMA PROGRAMMI_FINE

#endif
