/* CoreKit, pubblico: i servizi del sistema visti dai programmi. I numeri, i tipi e le
 * regole che usano corekit.h (che lo include) e i programmi.
 *
 * Sotto c'e' il kernel Linux. Un programma gli chiede le cose con le CHIAMATE DI SISTEMA
 * di Linux (open, read, socket, mmap...), attraverso la libreria C musl. I programmi pero'
 * non le usano direttamente: usano le funzioni di corekit.h (apri, manda, ricevi,
 * foglio...), che le fanno per loro. Le funzioni sono le stesse di quando sotto c'era
 * mkernel, il kernel didattico (che vive nel ramo "mykernel"): e' cambiato solo il "come",
 * dentro la libreria (corekit/interno/: corekit.c, file.c, comunicazione.c; per lo schermo
 * gkit/interno/schermo.c). Qui sotto c'e' il "cosa": il contratto tra la libreria e i programmi. */
#ifndef CHIAMATE_H
#define CHIAMATE_H

/* I SOCKET: i tubi tra programmi. Sono i socket locali di Linux "a pacchetti"
 * (AF_UNIX, SOCK_SEQPACKET): ogni messaggio arriva intero e separato dagli altri.
 * Chi offre un servizio si mette in ascolto con un nome; chi lo vuole si collega a quel
 * nome. Per ogni collegamento accettato, chi ascolta riceve un descrittore suo: cosi' sa
 * sempre da chi arriva un messaggio (il kernel non lascia fingere di essere un altro).
 * Il nome e' un file in /run/mshell/ (per esempio /run/mshell/compositore), che tutti
 * possono usare per collegarsi.
 *
 *   ascolta(nome)      un descrittore su cui arrivano i collegamenti; -1 se il nome e'
 *                      gia' preso da un altro
 *   collega(nome)      un descrittore collegato a chi ascolta su 'nome'; -1 se nessuno
 *                      ascolta con quel nome
 *   accetta(d)         il prossimo collegamento arrivato su d (dorme se non ce n'e')
 *   coppia(d)          due descrittori gia' collegati tra loro, d[0] e d[1]: un tubo
 *                      senza nome, da prestare a due programmi (socketpair)
 *   manda(d, ...)      un messaggio (al massimo MAX_MESSAGGIO byte). Dorme se l'altro ha
 *                      la casella piena, tranne con manda_subito: allora -1. -1 anche se
 *                      l'altro ha chiuso. Con manda_foglio si allega un foglio
 *   ricevi(d, ...)     il prossimo messaggio (dorme se non ce n'e'): quanti byte, o 0 se
 *                      l'altro ha chiuso. Se il messaggio e' piu' lungo, il resto si perde
 *   aspetta(...)       dorme finche' almeno uno dei descrittori dell'elenco ha qualcosa da
 *                      ricevere (un messaggio, un collegamento, l'altro che ha chiuso, un
 *                      processo finito), o finche' non passano ms millisecondi (poll)
 *
 * I FOGLI: memoria condivisa tra programmi (memfd di Linux). Un foglio e' un pezzo di
 * memoria che il kernel tiene per conto di chi l'ha creato. Si tiene in mano con un
 * descrittore, si mette nella propria memoria con mappa, e si passa a un altro programma
 * allegandolo a un messaggio (SCM_RIGHTS): dopo, tutti e due scrivono e leggono LE STESSE
 * pagine. E' cosi' che un'applicazione da' al compositore il disegno della sua finestra.
 * Un foglio non cambia mai misura (la libreria lo "sigilla"): chi lo riceve puo' fidarsi
 * di quella che legge. Sparisce quando nessuno lo tiene piu' (ne' descrittori ne' mappe). */
#define FOGLIO_MAX   0x01000000u   /* il foglio piu' grande: 16 MB */
#define MAX_MAPPATI  64            /* fogli messi nella memoria di un programma, insieme */
#define MANDA_SUBITO 1

#define MAX_NOME       32  /* lettere di un nome di socket, compreso lo 0 finale */
#define MAX_MESSAGGIO 5120 /* byte di un messaggio (basta per un percorso lungo MAX_PERCORSO e il resto) */
#define MAX_GUARDATI   16  /* descrittori in un elenco di aspetta */

struct da_guardare {
    int descrittore;
    int pronto;            /* lo scrive aspetta: 1 se c'e' qualcosa da ricevere */
};

/* I FILE E LE CARTELLE: un unico albero, che comincia da "/" (il disco del sistema, ext4).
 * Un file aperto e' un descrittore come gli altri: si legge con leggi (o ricevi) e si
 * scrive con scrivi_in (o manda), a partire dalla sua POSIZIONE, che va avanti da sola
 * (come read e write). Una cartella aperta si legge una voce alla volta: ogni leggi da'
 * la prossima struct voce_cartella, in ordine alfabetico, 0 quando sono finite ("." e
 * ".." non ci sono).
 * I percorsi sono assoluti (cominciano con /) e lunghi al massimo MAX_PERCORSO; "." e ".."
 * valgono come in Unix, e i collegamenti simbolici si seguono. Se qualcosa va storto le
 * chiamate dei file restituiscono un ERRORE_... (negativo, qui sotto). */
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
    long long modificato;            /* l'ultima modifica: secondi dal 1/1/1970 */
};

/* Gli errori delle chiamate dei file, sempre negativi (la libreria traduce gli "errno"
 * di Linux in questi). -1 lo danno anche le richieste sbagliate (un descrittore che non
 * c'e'). */
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
#define ERRORE_SOLA_LETTURA  -12    /* il disco non si puo' scrivere */
#define ERRORE_MEMORIA       -13
#define ERRORE_TROPPI_LINK   -14

/* GLI UTENTI E I PERMESSI, quelli di Unix, che controlla il kernel. Ogni programma ha un
 * uid: 0 e' il SISTEMA (puo' tutto), 1000 le APPLICAZIONI. Un programma nuovo prende
 * l'uid di chi lo fa partire; solo il sistema puo' farne partire uno con un uid diverso
 * (avvia_come, in sistema/privilegi.h), e nessuno puo' salire a 0.
 * Ogni file ha un proprietario e i permessi: tre cifre ottali, ognuna la somma di
 * PERMESSO_LEGGERE, _SCRIVERE, _ESEGUIRE; la prima per il proprietario, l'ultima per
 * tutti gli altri (quella di mezzo, il gruppo, per ora non si usa). Per creare,
 * cancellare o rinominare qualcosa bisogna poter scrivere nella cartella che lo contiene.
 * Un file nuovo e' di chi lo crea, con 0644; una cartella nuova con 0755.
 *
 * I POSTI DEL DISCO (build.sh). "/" e' la partizione system, in sola lettura: dentro,
 * /system con i programmi del sistema e le applicazioni (/system/apps/<app>). /data e' la
 * partizione data, che si scrive: per la persona con uid U c'e' /data/users/U, sua (700),
 * con dentro home (i suoi file), bin, appsdata/<app> (i dati di ogni applicazione) e
 * preferences. /home e' la casa di chi chiede: per un programma con uid U e'
 * /data/users/U/home (la libreria traduce il percorso); dentro c'e' Scrivania. */
#define UID_SISTEMA       0
#define UID_APPLICAZIONI  1000
#define PERMESSO_LEGGERE  4
#define PERMESSO_SCRIVERE 2
#define PERMESSO_ESEGUIRE 1

struct voce_cartella {
    unsigned int tipo;               /* TIPO_... (un collegamento non si segue) */
    unsigned int permessi;
    unsigned long long dimensione;   /* byte */
    long long modificato;            /* secondi dal 1/1/1970 */
    char nome[MAX_NOME_FILE];
};

/* I PROGRAMMI. avvia, avvia_come e avvia_con fanno partire un programma del disco (fork
 * ed execve) e danno il suo numero (il PID di Linux). Con avvia_con il programma nuovo
 * trova gia' aperti i descrittori dell'elenco, ai posti 0, 1, 2... (al massimo
 * MAX_PASSATI): cosi' init presta schermo, tastiera e il nome del compositore. Gli altri
 * descrittori di chi lo fa partire non li vede.
 * processo(numero) da' un descrittore del processo (pidfd): aspetta lo vede pronto
 * quando il processo e' finito, e ricevi aspetta che finisca. */
#define MAX_PASSATI 8

/* I PROCESSI E LA MEMORIA, per chi vuole guardarli (il task manager): la libreria li
 * legge da /proc. Chiunque puo' vedere l'elenco; terminare un processo invece segue la
 * regola di Unix, che fa rispettare il kernel: il sistema (uid 0) puo' terminare tutti,
 * gli altri solo i processi del loro uid. Init (il numero 1) non si termina: senza di lui
 * non c'e' sistema. Nell'elenco ci sono i programmi, non i lavori interni del kernel
 * (i "kernel thread" di Linux). */
#define STATO_PRONTO        0     /* aspetta il suo turno */
#define STATO_IN_ESECUZIONE 1     /* sta girando adesso: chi ha chiesto l'elenco */
#define STATO_DORME         2     /* dorme per un po' (dormi) */
#define STATO_IN_ATTESA     3     /* aspetta qualcosa: un messaggio, un evento */
#define STATO_FINITO        4

struct info_processo {
    int numero;
    int padre;                     /* il numero di chi l'ha lanciato (0: nessuno) */
    unsigned int uid;
    unsigned int stato;            /* STATO_... */
    unsigned int memoria;          /* byte: le pagine che sono sue (non quelle condivise) */
    char nome[16];
};

struct info_memoria {              /* in byte, a 64 bit: la RAM puo' superare i 4 GB */
    unsigned long long totale;     /* byte di RAM che il kernel puo' usare */
    unsigned long long libera;     /* byte che nessuno usa */
    unsigned long long condivisa;  /* byte dei fogli condivisi: le immagini delle finestre */
};

struct info_disco {                /* una partizione, in byte */
    unsigned long long totale;     /* quanto e' grande il suo file system */
    unsigned long long libero;     /* quanto non e' usato (anche la riserva del sistema) */
};

struct data_ora {
    unsigned int anno, mese, giorno;   /* mese da 1 a 12 */
    unsigned int giorno_settimana;     /* 0 domenica, 1 lunedi'... */
    unsigned int ore, minuti, secondi;
};

#define MAX_PERCORSO 4096  /* lettere di un percorso, compreso lo 0 finale (come PATH_MAX di Linux) */
#define MAX_PERCORSO_PROGRAMMA 64   /* il percorso del programma di un'app (/system/apps/<id>/code): corto, viaggia nei messaggi */

/* LO SCHERMO (il DRM di Linux, /dev/dri/card0). apri_schermo da' un descrittore dello
 * schermo a un solo programma alla volta (il "padrone" dello schermo, il DRM master);
 * mappa_schermo mette nella memoria del programma l'immagine che si vede, e dice com'e'
 * fatta. Ogni pixel e' di 4 byte; rosso, verde e blu dicono a che bit sta ogni colore
 * dentro il pixel. Dopo averci scritto, schermo_cambiato dice al kernel quali rettangoli
 * sono cambiati: alcune schede video (quelle virtuali, per esempio) li devono copiare. */
struct info_schermo {
    unsigned int passo;                /* byte da una riga di pixel alla successiva */
    unsigned int larghezza, altezza;   /* in pixel */
    unsigned int rosso, verde, blu;
};

struct zona_schermo { int x0, y0, x1, y1; };   /* x0 <= x < x1, y0 <= y < y1 */
#define MAX_ZONE_SCHERMO 32

/* GLI EVENTI: qualcosa che e' successo alla tastiera o al mouse. Linux li da' in
 * /dev/input, un file per dispositivo, nel suo formato; il servizio eventi
 * (utente/programmi/eventi.c) li legge tutti, li traduce (la tastiera italiana) e li
 * mette in un unico tubo, un evento per messaggio. Dice solo COSA e' successo, non a
 * quale finestra va: lo decide il compositore, che legge dall'altro capo del tubo. */
#define EVENTO_TASTO 1     /* e' stato premuto un tasto: il carattere e' in 'tasto' */
#define EVENTO_MOUSE 2     /* il mouse si e' spostato di dx, dy, o un pulsante e' cambiato */
#define EVENTO_PUNTATORE 3 /* una tavoletta: il puntatore e' in (dx, dy), da 0 a PUNTATORE_MAX
                            * su ogni asse (lo schermo intero), e questi sono i pulsanti */
#define EVENTO_TASTO_FISICO 5 /* un tasto della tastiera e' stato premuto o lasciato, senza
                            * tradurlo in un carattere: 'tasto' e' il codice di Linux (KEY_...,
                            * linux/input-event-codes.h), 'pulsanti' vale 1 (premuto) o 0
                            * (lasciato). Le ripetizioni automatiche non si mandano. */
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

#endif
