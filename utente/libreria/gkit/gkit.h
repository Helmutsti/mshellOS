/* GKit, la base della grafica (vedi LIBRERIE.md): come la parte bassa di Quartz sul Mac o
 * libwayland su Linux. Questo e' il suo header principale: un programma scrive
 *     #include "gkit/gkit.h"
 * e ha la FINESTRA: la apre, riceve una tela libera su cui disegnare, la fa mostrare dal
 * compositore, riceve gli eventi (mouse, tastiera). Gli strumenti per disegnare su una tela
 * stanno accanto, in gkit_disegno.h. Un programma che disegna da solo pixel per pixel (Doom)
 * usa solo GKit.
 *
 * GKit non sa niente di stile, temi, font e icone del sistema, componenti: GKit sa COME si
 * disegna e si mostra, UIKit (utente/libreria/uikit/) sa COSA.
 *
 * LE CARTELLE DI GKIT (utente/libreria/gkit/). Tre livelli, come CoreKit (corekit.h):
 *
 *   gkit/            PUBBLICO: quello che un'applicazione puo' usare. gkit.h (questo: la
 *                    finestra, il lato dell'applicazione) e gkit_disegno.h (gli strumenti di
 *                    disegno: il pennello, le forme, il testo, le icone, le immagini, e i
 *                    materiali letti da un file).
 *   gkit/sistema/    DEL SISTEMA: quello che usano solo i programmi del sistema. protocollo.h
 *                    (i messaggi tra la finestra e il compositore, e le superfici speciali
 *                    della scrivania: pannelli e sfondo), schermo.h (lo schermo, il lato del
 *                    compositore: il DRM di Linux), pixel.h (i conti di ogni pixel: mettere un
 *                    colore sopra un altro, quanto un pixel sta dentro una forma arrotondata;
 *                    li usa il compositore per comporre le finestre). Sono header come gli
 *                    altri, ma un'app non li deve includere: possono cambiare senza avviso.
 *   gkit/interno/    INTERNO: il codice (i .c) e interno.h, i passi di ogni disegno che i .c
 *                    si dicono tra loro. Nessun programma li include.
 *
 * build.sh compila i .c di interno/ (con stb_image, la libreria di altri che legge le PNG e
 * che usa solo GKit) in una libreria statica, libgkit.a, e la lega dentro ogni programma: ne
 * entra solo quello che il programma usa. Nel comando di lega viene dopo UIKit e il programma
 * (che usano le sue funzioni) e prima di libcorekit.a (che usa lei).
 *
 * Negli header pubblici ci sono solo dichiarazioni, tipi, costanti e i commenti per chi li
 * usa: il codice e' tutto nei .c. Le uniche funzioni scritte negli header sono i conti per
 * pixel di sistema/pixel.h e interno/interno.h, che si copiano dentro chi le chiama per non
 * pagare una chiamata a ogni pixel.
 *
 * GLI ERRORI. Le funzioni che possono non riuscire danno -1 (o 0 quelle che danno un
 * indirizzo o dicono "e' arrivato qualcosa"): ognuna lo dice nel suo commento. */
#ifndef GKIT_H
#define GKIT_H

#include "corekit/corekit.h"

#define MAX_TITOLO 40                     /* "Terminale - super amministratore" ne ha 32 piu' la fine */
#define RAGGIO_PREDEFINITO 12             /* gli angoli di una finestra aperta con finestra_apri */
#define RUOLO_FINESTRA  0                 /* una finestra normale (per finestra_apri_come) */

/* =====================================================================================
 * LE ZONE CAMBIATE
 * =====================================================================================
 *
 * Quando l'applicazione fa mostrare la finestra dice anche DOVE l'immagine nuova e' diversa
 * da quella che il compositore mostrava prima: al massimo MAX_ZONE_MOSTRA rettangoli, in
 * pixel della finestra (0 rettangoli: tutta la finestra, come la prima volta). Il compositore
 * ricompone solo li' (come wl_surface.damage di Wayland): se una riga cambia colore sotto il
 * puntatore, non si rifa' tutta la finestra con la sua ombra e il suo vetro smerigliato.
 * Fuori dalle zone l'immagine nuova DEVE essere uguale a quella di prima: e' l'applicazione
 * a garantirlo (finestra_arretrate, qui sotto, la aiuta con le due immagini). */
#define MAX_ZONE_MOSTRA 8

struct zona_finestra {
    int x, y, l, a;                    /* x <= ... < x + l, y <= ... < y + a */
};

/* I rettangoli: un elenco di zone che non si sovrappongono troppo. Lo usano l'applicazione
 * (le zone da ridisegnare e da mandare) e il compositore. */

/* 1 se la zona e' vuota (larga o alta 0 o meno), 0 se no. */
int zona_vuota(struct zona_finestra z);

/* Il rettangolo piu' piccolo che contiene 'p' e 'q'. */
struct zona_finestra zona_unione(struct zona_finestra p, struct zona_finestra q);

/* La parte comune di 'p' e 'q' (una zona vuota, 0 0 0 0, se non si toccano). */
struct zona_finestra zona_dentro(struct zona_finestra p, struct zona_finestra q);

/* Aggiunge 'z' all'elenco 'zone', che ne ha '*quante' e ne puo' tenere 'massimo' (una zona
 * vuota non si aggiunge). Se 'z' tocca una zona gia' nell'elenco, o se unirle costa poco in
 * piu' (meno di un quarto della loro area), le due diventano il rettangolo che le contiene, e
 * si ricontrolla; se l'elenco e' pieno, tutto diventa un solo rettangolo. Due zone che si
 * sovrappongono vorrebbero dire disegnare due volte gli stessi pixel. Aggiorna '*quante'. */
void zone_aggiungi(struct zona_finestra *zone, int *quante, int massimo, struct zona_finestra z);

/* =====================================================================================
 * LE NOTIZIE: quello che il compositore dice alla finestra
 * =====================================================================================
 *
 * Ogni notizia e' una struct notizia; 'tipo' dice cos'e' e quali campi contano. Le coordinate
 * del puntatore sono INTERNE alla finestra: l'applicazione non sa dov'e' sullo schermo e non
 * vede le altre finestre. I numeri che qui mancano (1, 6-10) sono le notizie che la libreria
 * sbriga da sola o che arrivano solo ai pannelli del sistema (sistema/protocollo.h). */

#define PUNTATORE  2   /* il puntatore e' in x, y (dentro la finestra), con 'pulsanti'
                        * premuti (PULSANTE_... di chiamate.h) */
#define USCITO     3   /* il puntatore e' uscito dalla finestra */
#define TASTO      4   /* e' stato premuto il tasto 'tasto' (code page 437) */
#define FUOCO      5   /* 'fuoco' = 1: i tasti ora vengono a te; 0: non piu' */
#define ROTELLA   11   /* la rotella del mouse ha girato di 'scatti_y' scatti (positivo: in
                        * basso, il contenuto va su), e 'scatti_x' quella orizzontale
                        * (positivo: a destra), con il puntatore in x, y. Va alla finestra
                        * sotto il puntatore, anche se non ha il fuoco: come sul Mac e in
                        * Wayland, si scorre quello che si guarda */
#define TASTO_FISICO 12  /* (solo a chi l'ha chiesto con finestra_tasti_fisici) il tasto
                          * Linux 'tasto' (KEY_...) e' stato premuto ('premuto' = 1) o
                          * lasciato (0). Senza ripetizioni. Arriva in piu' di TASTO. */
#define TEMA_CAMBIATO 13  /* (a tutte le finestre) il tema del sistema (Chiaro o Scuro) e'
                          * cambiato: rileggilo dalle preferenze e ridisegnati. Le app di
                          * UIKit lo fanno da sole */

struct notizia {
    unsigned int tipo;
    unsigned int immagine;   /* (della libreria) */
    int x, y;                /* PUNTATORE, ROTELLA */
    unsigned int pulsanti;   /* PUNTATORE */
    unsigned int tasto;      /* TASTO, TASTO_FISICO */
    unsigned int fuoco;      /* FUOCO */
    unsigned int numero;     /* (della libreria e dei pannelli del sistema) */
    char testo[MAX_PERCORSO_PROGRAMMA];   /* (dei pannelli del sistema) */
    int scatti_x, scatti_y;  /* ROTELLA */
    unsigned int premuto;    /* TASTO_FISICO */
};

/* =====================================================================================
 * LA FINESTRA, il lato dell'applicazione
 * =====================================================================================
 *
 * Apre la finestra, da' una tela libera su cui disegnare, la fa mostrare, consegna gli
 * eventi. COME disegnare (rettangoli, testo...) non e' affar suo: e' di gkit_disegno.h.
 *
 *     struct finestra f;
 *     finestra_apri(&f, 300, 200, "Ciao");
 *     unsigned int *tela = finestra_tela(&f);   ... si disegna: tela[y * 300 + x] = 0xAARRGGBB
 *     finestra_mostra(&f);
 *     struct notizia n;
 *     while (finestra_notizia(&f, &n)) { ... }
 *
 * LA FINESTRA E' TUTTA DELL'APPLICAZIONE, barra del titolo e bordo compresi: e' lei a
 * decidere il suo aspetto (come su Mac e in GNOME, le "decorazioni lato client"); se usa
 * UIKit, sara' lui a disegnarle con lo stile del sistema. A spostarla pero' e' il compositore
 * (finestra_trascinami). I pixel della tela sono 0xAARRGGBB PREMOLTIPLICATI (vedi
 * gkit_disegno.h), riga dopo riga: il compositore mescola ogni pixel con quello che c'e'
 * sotto, quindi una finestra puo' essere di vetro e i suoi angoli arrotondati sono
 * trasparenti.
 *
 * LE DUE IMMAGINI. Il compositore mostra un'immagine mentre l'applicazione disegna
 * nell'altra: non si vede mai una finestra disegnata a meta'. finestra_tela da' sempre
 * un'immagine che il compositore non sta usando: se sono tutte e due sue, aspetta che ne
 * restituisca una. Le notizie arrivate nel frattempo (un clic, un tasto) si mettono da parte,
 * e finestra_notizia le consegnera' dopo, in ordine.
 *
 * RIDISEGNARE SOLO UN PEZZO. Chi cambia solo una parte della finestra la ridisegna e la
 * mostra con finestra_mostra_zone. Ma l'immagine che riceve da finestra_tela e' quella di DUE
 * fotogrammi fa (l'altra e' del compositore): le mancano anche le zone cambiate nel
 * fotogramma di mezzo. Gliele dice finestra_arretrate (e' l'"eta' del buffer" di EGL e
 * Wayland): chi disegna rifa' le sue zone PIU' quelle arretrate, e l'immagine torna giusta
 * dappertutto. La prima volta (e dopo un finestra_mostra intero) e' tutta da rifare.
 *
 * "CI SEI?". Ogni tanto il compositore chiede alla finestra se c'e', e la libreria risponde
 * da sola ogni volta che l'applicazione aspetta le sue notizie. Se l'applicazione resta
 * bloccata altrove la risposta non parte, e il compositore ne ridisegna la barra in rosso,
 * con il pulsante di chiusura d'emergenza (come GNOME e il Mac). */

#define DA_PARTE 16

/* Una finestra aperta. La riempie finestra_apri: i campi sono della libreria, l'applicazione
 * non li tocca. */
struct finestra {
    int collegamento, foglio;
    int larghezza, altezza;
    unsigned int *immagini[2];
    int libera[2];             /* 1 se l'immagine e' nostra, 0 se e' del compositore */
    int in_uso;                /* l'immagine data dall'ultima finestra_tela */
    struct notizia messe_da_parte[DA_PARTE];
    int quante;
    /* le zone che mancano a ogni immagine rispetto all'ultima mostrata (-1: tutta) */
    struct zona_finestra arretrate[2][MAX_ZONE_MOSTRA];
    int quante_arretrate[2];
};

/* Quanto e' grande lo schermo: lo chiede al compositore (se non e' ancora pronto riprova per
 * qualche secondo) e lo scrive in *larghezza e *altezza. 0, o -1 se il compositore non c'e'
 * o non risponde. Si puo' chiedere prima di aprire una finestra. */
int finestra_schermo(int *larghezza, int *altezza);

/* Si collega al compositore (se non e' ancora pronto riprova per qualche secondo), crea
 * il foglio con le due immagini, larghe 'larghezza' e alte 'altezza' (tutta la finestra,
 * barra compresa), e chiede una finestra normale con il titolo 'titolo' (al massimo
 * MAX_TITOLO - 1 caratteri: il resto si taglia) e gli angoli di RAGGIO_PREDEFINITO. 0 se
 * fatto, -1 se il compositore non c'e' o il foglio non si crea. */
int finestra_apri(struct finestra *f, int larghezza, int altezza, const char *titolo);

/* Come finestra_apri, con il ruolo, il posto sullo schermo (x, y) e il raggio degli angoli
 * (il compositore lo usa per l'ombra e il vetro smerigliato sotto). Per un'applicazione il
 * ruolo e' sempre RUOLO_FINESTRA, e x e y non contano (la finestra la mette il compositore);
 * gli altri ruoli (sistema/protocollo.h) sono solo del sistema: da un'applicazione, il
 * compositore chiude il collegamento. 0 se fatto, -1 se no. */
int finestra_apri_come(struct finestra *f, int larghezza, int altezza, const char *titolo,
                       unsigned int ruolo, int x, int y, unsigned int raggio);

/* Chiude la finestra: il compositore la toglie dallo schermo (se ne accorge dal collegamento
 * chiuso) e la sua memoria si restituisce. Per riaverla si riapre. */
void finestra_chiudi(struct finestra *f);

/* Riceve un messaggio dal compositore (dorme finche' non arriva). Le immagini restituite si
 * segnano qui, e al "ci sei?" si risponde qui. 1 se e' arrivato qualcosa (anche una notizia
 * di servizio: vedi finestra_di_servizio), 0 se il compositore ha chiuso. Chi non ha bisogno
 * di aspettare altro usa finestra_notizia, che salta le notizie di servizio. */
int finestra_ricevi(struct finestra *f, struct notizia *n);

/* 1 se 'n' e' una notizia che la libreria sbriga da sola (un'immagine restituita, il "ci
 * sei?"): l'applicazione la ignora. 0 se e' per l'applicazione. */
int finestra_di_servizio(const struct notizia *n);

/* Un'immagine libera, su cui disegnare: larghezza x altezza pixel. Se il compositore le ha
 * tutte e due, aspetta che ne restituisca una. 0 se il compositore ha chiuso. */
unsigned int *finestra_tela(struct finestra *f);

/* 1 se c'e' un'immagine libera senza aspettare (per non bloccarsi su finestra_tela mentre
 * arrivano altre notizie), 0 se no. */
int finestra_libera(const struct finestra *f);

/* Le zone che mancano all'immagine data dall'ultima finestra_tela (vedi "ridisegnare solo un
 * pezzo"): le copia in 'zone' (al massimo MAX_ZONE_MOSTRA) e dice quante sono; -1 se
 * l'immagine e' tutta da rifare. */
int finestra_arretrate(const struct finestra *f, struct zona_finestra *zone);

/* L'immagine data dall'ultima finestra_tela e' pronta: il compositore la mostra. E' cambiata
 * solo nelle 'quante' zone (in pixel della finestra; 0: tutta). Da adesso l'altra immagine,
 * quando tornera' libera, sara' indietro anche di queste zone. 0 se il messaggio e' partito,
 * -1 se il compositore ha chiuso. */
int finestra_mostra_zone(struct finestra *f, const struct zona_finestra *zone, int quante);

/* Tutta l'immagine e' nuova (chi ridisegna sempre tutto, come Doom). 0, o -1. */
int finestra_mostra(struct finestra *f);

/* Da chiamare quando si preme sulla propria barra del titolo: il compositore sposta la
 * finestra seguendo il mouse, finche' il pulsante resta premuto (se e' gia' stato lasciato,
 * non succede niente). 0, o -1 se il compositore ha chiuso. */
int finestra_trascinami(struct finestra *f);

/* Chiede di ricevere anche le notizie TASTO_FISICO (tasto premuto/lasciato, codici KEY_ di
 * Linux): per i giochi. Le notizie TASTO continuano ad arrivare. 0, o -1. */
int finestra_tasti_fisici(struct finestra *f);

/* Chiede al compositore di nascondere la finestra (il pulsante "riduci"). Tornera'
 * quando la scrivania la fa tornare. 0, o -1. */
int finestra_riducimi(struct finestra *f);

/* Le FORME DEL PUNTATORE (come il disegno, "Puntatore"). Il compositore disegna il puntatore e
 * mostra la forma che vuole la finestra su cui sta; di partenza e' FORMA_NORMALE. */
#define FORMA_NORMALE         0   /* la freccia */
#define FORMA_COLLEGAMENTO    1   /* la mano: su un collegamento */
#define FORMA_TESTO           2   /* la barra a I: su un testo che si scrive o si seleziona */
#define FORMA_NON_DISPONIBILE 3   /* la freccia col divieto: l'azione non e' consentita */
#define FORMA_ATTESA          4   /* l'anello che gira: sto lavorando, non rispondo subito */

/* Dice al compositore quale forma mostrare mentre il puntatore e' su questa finestra (vale
 * finche' non la si cambia; il compositore la mostra anche mentre il puntatore e' fermo). Se la
 * forma non e' una di FORMA_..., il compositore chiude il collegamento. UIKit la sceglie da
 * solo (uikit.h, finestra_tk_forma). 0, o -1 se il compositore ha chiuso. */
int finestra_forma_puntatore(struct finestra *f, unsigned int forma);

/* Chiede al compositore di dire a tutte le finestre che il tema e' cambiato (TEMA_CAMBIATO):
 * il tema nuovo va scritto PRIMA nelle preferenze. Lo puo' chiedere solo il sistema o un'app
 * con il permesso "settings.system" nel manifesto; per gli altri non succede niente.
 * 0 se il messaggio e' partito, -1 se no. */
int finestra_cambia_tema(struct finestra *f);

/* La prossima notizia per l'applicazione (un clic, un tasto...): prima quelle messe da
 * parte, poi dorme finche' ne arriva una. 1, o 0 se il compositore ha chiuso. */
int finestra_notizia(struct finestra *f, struct notizia *n);

#endif
