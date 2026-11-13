/* UIKit, l'interfaccia (vedi LIBRERIE.md): come AppKit sul Mac o GTK su Linux, costruita
 * sopra GKit. Da' alle applicazioni dei pezzi pronti, i WIDGET, tutti con l'aspetto del
 * sistema, e fa da solo le cose noiose: la barra del titolo, il "trascinami", il fuoco, la
 * tastiera, il ciclo degli eventi, il ridisegno. Un'applicazione include solo questo file:
 * dentro c'e' tutto quello che le serve.
 *
 * I PEZZI DI UIKIT:
 *   - lo stile e i temi (stile.h, tema.h, dal file Figma "mshell - interfaccia"): colori
 *     semantici Chiaro/Scuro, misure, raggi, ombre;
 *   - i materiali del sistema (materiali.h; i dati in file su disco, /system/resources,
 *     caricati la prima volta che servono): il font, le icone Lucide, le icone di file e
 *     cartelle;
 *   - la finestra dell'applicazione come la decide il design, i componenti e il ciclo degli
 *     eventi: qui sotto (e l'elemento di un file in elemento_file.h; la userbar e il vassoio,
 *     della scrivania, in userbar.h);
 *   - quello che serve al compositore del design (sistema/uikit_compositore.h).
 * Sotto, GKit (utente/libreria/gkit/): la finestra (gkit.h) e gli strumenti di disegno
 * (gkit_disegno.h).
 *
 * LE CARTELLE DI UIKIT (utente/libreria/uikit/). Tre livelli, come CoreKit e GKit (corekit.h,
 * gkit.h):
 *
 *   uikit/            PUBBLICO: quello che un'applicazione puo' usare. uikit.h (questo:
 *                     include tutto quello che serve a un'app), stile.h (colori, misure,
 *                     font e ombre del design), tema.h (i due temi e quello in uso),
 *                     materiali.h e materiali_elenco.h (i font, le icone, gli oggetti del
 *                     sistema; l'elenco e' generato da strumenti/grafica/genera.py), tasti.h
 *                     (i codici dei tasti e gli ascoltatori delle scorciatoie), testo437.h (da
 *                     UTF-8 ai caratteri dei font del sistema), e gli header dei componenti.
 *   uikit/sistema/    DEL SISTEMA: quello che usano solo i programmi del sistema.
 *                     uikit_compositore.h (le poche cose del design che disegna il
 *                     compositore). Un'app non lo deve includere: puo' cambiare senza avviso.
 *   uikit/interno/    INTERNO: il codice (i .c) e nucleo.h, le dichiarazioni che servono solo
 *                     dentro UIKit. Nessun programma li include.
 *
 * build.sh compila i .c di interno/ in una libreria statica, libuikit.a, e la lega dentro ogni
 * programma: ne entra solo quello che il programma usa. Nel comando di lega viene subito dopo
 * il programma (che usa le sue funzioni) e prima di libgkit.a e libcorekit.a (che usa lei).
 *
 * IL RIORDINO E' IN CORSO. L'obiettivo e' quello di CoreKit e GKit: negli header pubblici solo
 * dichiarazioni, tipi, costanti e i commenti per chi li usa, e il codice tutto nei .c di
 * interno/. A che punto siamo:
 *   1. FATTO: le basi. stile.h, tema.h, materiali.h, tasti.h, testo437.h e
 *      sistema/uikit_compositore.h hanno solo dichiarazioni; il codice (e le tabelle dei due
 *      temi e delle ombre) e' in interno/ (tema.c, stile.c, materiali.c, testo437.c,
 *      uikit_compositore.c).
 *   2. FATTO: il nucleo, questo file. Qui restano i tipi, le costanti e le dichiarazioni delle
 *      funzioni pubbliche; il codice e' in interno/, diviso per argomento: widget.c (l'albero,
 *      contenitori, spazi, etichetta), pulsanti.c (pulsante, casella, scelta, interruttore),
 *      campo.c (campo e ricerca), stato_vuoto.c, avviso.c, finestra_tk.c (la finestra come
 *      widget: misure, zone da ridisegnare, testata, opzioni, chiamate "ogni tanto", sotto il
 *      puntatore e fuoco), barra_strumenti.c (barra, pulsante della barra, pillola, percorso),
 *      scorrimento.c e ciclo.c (il ciclo degli eventi: tutto quello che parla con il
 *      compositore). Le funzioni che servono solo dentro UIKit (ai .c del nucleo e ai
 *      componenti ancora scritti negli header) sono dichiarate in interno/nucleo.h.
 *   3. DA FARE: i componenti complessi (menu, tabella, griglia, barra_laterale, schede,
 *      userbar, oggetti, elemento_file, pagina, paragrafo, pannello).
 *   4. FATTO: i componenti piccoli. grafico.h, etichetta_stato.h, messaggio.h, icona_app.h,
 *      riga_permesso.h, riga_app.h e informazioni.h hanno solo dichiarazioni; il codice e' in
 *      interno/, un .c per header (informazioni_mostra.c a parte: apre la mini finestra con il
 *      ciclo degli eventi, e l'anteprima la lascia fuori come ciclo.c). Quello che un
 *      componente usa di un altro (l'etichetta di stato nella riga di permesso, l'icona
 *      dell'app nella riga dell'app) e' dichiarato in interno/componenti_piccoli.h.
 * Finche' un gruppo non e' fatto, le sue funzioni restano scritte negli header (static) e
 * funzionano come prima.
 *
 *     static struct finestra_tk finestra;
 *     static struct contenitore colonna;
 *     static struct etichetta saluto;
 *     static struct pulsante ok;
 *
 *     static void al_clic(struct pulsante *p, void *dati) { etichetta_testo(&saluto, "Ciao!"); }
 *
 *     int main(void) {
 *         colonna_prepara(&colonna, SPAZIO_TRA);
 *         etichetta_prepara(&saluto, "Premi il pulsante", FONT_TESTO);
 *         pulsante_prepara(&ok, "OK", NESSUNA_ICONA, al_clic, 0);
 *         widget_aggiungi(&colonna.base, &saluto.base);
 *         widget_aggiungi(&colonna.base, &ok.base);
 *         finestra_tk_prepara(&finestra, "Saluto", &colonna.base);
 *         return uikit_esegui(&finestra);
 *     }
 *
 * GLI OGGETTI, IN C. Ogni widget comincia con una 'struct widget' (la "base"): li' ci
 * sono il posto, i figli e il TIPO, cioe' le funzioni che sanno misurarlo, sistemarne i
 * figli, disegnarlo, fargli sentire il puntatore e la tastiera. UIKit chiama quelle,
 * senza sapere se e' un pulsante o una colonna. E' lo stesso schema di GTK, in C.
 *
 * L'ALBERO. I widget stanno uno dentro l'altro. Per disegnare si scende dall'alto,
 * ognuno ritagliato nel suo spazio; per un clic si cerca il widget piu' interno che sta
 * sotto il puntatore.
 *
 * L'IMPAGINAZIONE, in due passaggi: prima ognuno dice quanto spazio vorrebbe (misura),
 * poi i contenitori distribuiscono lo spazio vero (disponi). Righe e colonne mettono i
 * figli uno dopo l'altro; lo spazio in piu' va agli SPAZI ELASTICI (come "flex: 1").
 *
 * GLI STATI. UIKit segna da solo quale widget ha il puntatore SOPRA (per
 * illuminarlo), quale e' PREMUTO, quale ha il FUOCO della tastiera (il campo di testo
 * su cui si e' cliccato). Un widget DISABILITATO si disegna al 40% e non sente niente.
 *
 * IL CICLO DEGLI EVENTI E' DI UIKIT. uikit_esegui non torna finche' la finestra
 * c'e': aspetta le notizie del compositore, le consegna ai widget, ridisegna quando
 * qualcosa e' cambiato. Torna quando si preme "chiudi". L'applicazione interviene solo
 * quando succede qualcosa che le interessa ("quando premono questo pulsante, chiama
 * questa funzione": in GTK i segnali, in AppKit target-action).
 *
 * SI RIDISEGNA SOLO QUELLO CHE E' CAMBIATO (le "zone rovinate" di GTK, AppKit e Wayland). Un
 * widget che cambia lo dice con widget_cambiato (tutto il suo spazio, con i 24 pixel che puo'
 * sbordare) o widget_cambiato_zona (solo un pezzo: la riga di una tabella sotto il
 * puntatore). La finestra raccoglie queste zone e al ridisegno rifa' SOLO LORO: per ognuna si
 * ritaglia il pennello li' e si disegna tutto l'albero, dal fondo della finestra in su, ma i
 * widget che non toccano la zona si saltano e le forme grandi (il vetro, i pannelli) costano
 * solo nel ritaglio. Il risultato e' lo stesso, pixel per pixel, di un ridisegno intero,
 * perche' ogni pixel dipende solo da quello che c'e' in lui. Poi la finestra dice al
 * compositore quali zone sono cambiate (finestra_mostra_zone), e lui ricompone solo li'.
 * Quando cambia l'impaginazione (uikit_ridisponi) si rifa' tutto.
 * Le notizie si consegnano TUTTE prima di ridisegnare: quelle arrivate mentre si disegnava si
 * servono insieme, e si ridisegna una volta sola (i movimenti del puntatore di mezzo non si
 * vedrebbero: conta l'ultimo).
 *
 * IL TEMPO. Un'applicazione che deve aggiornarsi da sola (un orologio, il task manager)
 * chiede a UIKit di chiamarla ogni tanto: finestra_tk_ogni(&finestra, 1000, f, dati).
 * Il ciclo allora non dorme senza limite: aspetta le notizie fino alla prima scadenza.
 * Le chiamate possono essere piu' d'una (fino a MAX_CHIAMATE): le usano anche i widget
 * che si animano, come la barra di scorrimento, solo finche' l'animazione dura.
 *
 * LO SCORRIMENTO. Di un contenuto piu' alto del suo spazio (una lista, una tabella) se ne
 * mostra un pezzo, la VISTA, e la rotella del mouse lo fa scorrere. UIKit da' due
 * pezzi: 'struct scorrimento', i tre numeri (dove si e', quanto e' alto tutto, quanto se
 * ne vede), e la BARRA DI SCORRIMENTO, un widget che sta sopra il contenuto, sul bordo
 * destro, e si vede solo mentre si scorre (come sul Mac). La rotella va al widget sotto
 * il puntatore, o al primo dei suoi genitori che la vuole (il campo 'rotella' del tipo).
 *
 * I WIDGET NASCOSTI non si misurano, non si disegnano e non sentono niente: e' cosi' che
 * si fanno le pagine (piu' contenuti nello stesso posto, se ne vede uno). Dopo averne
 * mostrato o nascosto uno, uikit_ridisponi rifa' l'impaginazione.
 *
 * I WIDGET LI DICHIARA L'APPLICAZIONE, come variabili sue. Una finestra per
 * applicazione, che non cambia misura. Il pulsante "riduci" la nasconde: torna con un
 * clic sulla sua icona nella userbar della scrivania. */
#ifndef UIKIT_H
#define UIKIT_H

#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"
#include "stile.h"           /* con i temi (tema.h) e i materiali (materiali.h) */
#include "tasti.h"           /* i codici dei tasti fisici e gli ascoltatori delle scorciatoie */

/* --- il widget in generale --- */

struct widget;

struct tipo_widget {
    /* quanto spazio vorrebbe */
    void (*misura)(struct widget *w, int *l, int *a);
    /* il suo posto (x, y, l, a) e' deciso: sistema i figli dentro (0: niente figli) */
    void (*disponi)(struct widget *w);
    /* si disegna nel suo spazio (il pennello e' gia' ritagliato li') */
    void (*disegna)(struct widget *w, struct pennello *p);
    /* il puntatore, in coordinate della finestra (0: non gli interessa). Lo riceve il
     * widget su cui si e' premuto, finche' il pulsante non si lascia, anche da fuori */
    void (*puntatore)(struct widget *w, int x, int y, unsigned int pulsanti);
    /* un tasto, se ha il fuoco (0: non prende il fuoco) */
    void (*tasto)(struct widget *w, unsigned int tasto);
    /* la rotella del mouse, girata di 'sx', 'sy' scatti (sy positivo: in basso) con il
     * puntatore sopra di lui o sopra uno dei suoi figli (0: non gli interessa) */
    void (*rotella)(struct widget *w, int sx, int sy);
    /* il puntatore e' appena arrivato sopra di lui, o se n'e' andato: 'sopra' e' gia'
     * cambiato (0: basta il ridisegno che fa UIKit) */
    void (*sopra_cambiato)(struct widget *w);
    /* il puntatore si muove sopra di lui senza pulsanti premuti, in (x, y) (0: non gli
     * interessa). Serve a chi deve sapere SOPRA QUALE SUA PARTE e' il puntatore (i file
     * di una cartella, per illuminare quello li' sotto), non solo se e' sopra di lui */
    void (*movimento)(struct widget *w, int x, int y);
    /* 1: il widget sa disegnarsi disabilitato (piatto, con i colori _DISABILITATO del
     * tema) e UIKit NON lo sbiadisce al 40%. 0 (gli altri): il vecchio modo, 40%. */
    int disegna_disabilitato;
};

#define MAX_FIGLI 16

struct widget {
    const struct tipo_widget *tipo;
    struct widget *genitore;           /* 0 per la finestra, la radice dell'albero */
    int x, y, l, a;                    /* il suo spazio, in coordinate della finestra */
    struct widget *figli[MAX_FIGLI];
    int quanti_figli;
    int sopra;                         /* il puntatore e' sopra di lui (lo segna UIKit) */
    int fuoco;                         /* ha il fuoco della tastiera (lo segna UIKit) */
    int disabilitato;                  /* si vede al 40% e non sente niente */
    int elastico;                      /* prende lo spazio in piu' nel suo contenitore */
    int largo;                         /* nell'altra direzione si allarga a tutto lo spazio */
    int nascosto;                      /* non c'e': non si misura, non si vede, non sente */
};

/* Mette 'figlio' in fondo ai figli di 'genitore' e gli segna il genitore. Al massimo
 * MAX_FIGLI figli: quelli in piu' non entrano (senza avviso). */
void widget_aggiungi(struct widget *genitore, struct widget *figlio);

/* Quanto spazio vorrebbe il widget (in 'l' e 'a'): lo chiede al suo tipo; 0 x 0 se il tipo
 * non ha 'misura'. La usa chi scrive un widget che ne contiene altri, per misurare i figli. */
void widget_misura(struct widget *w, int *l, int *a);

/* Il posto del widget (x, y, l, a) e' deciso: il suo tipo sistema i figli ('disponi'), poi
 * si fa lo stesso con ognuno di loro, giu' per tutto l'albero. */
void widget_disponi(struct widget *w);

/* Disegna il widget e i suoi figli, ognuno ritagliato nel suo spazio piu' WIDGET_SBORDO
 * (anelli e ombre escono un po'). Un widget nascosto, o che non tocca la zona che si sta
 * ridisegnando (il ritaglio del pennello), si salta; uno disabilitato si disegna al 40%, se il
 * suo tipo non sa disegnarsi disabilitato da se'. */
void widget_disegna(struct widget *w, struct pennello *p);

/* 1 se il punto (x, y), in coordinate della finestra, sta nello spazio del widget; se no 0. */
int widget_contiene(const struct widget *w, int x, int y);

/* Un widget e' cambiato (un testo nuovo, un pulsante premuto): va ridisegnato il suo spazio
 * (con lo sbordo). Si risale l'albero fino alla finestra, che si segna la zona. Un widget
 * nascosto (o dentro uno nascosto) non si vede: niente da ridisegnare. */
void widget_cambiato(struct widget *w);

/* Solo un pezzo del widget e' cambiato: il rettangolo (x, y, l, a), in coordinate della
 * finestra (la parte fuori dal widget e dal suo sbordo non conta). */
void widget_cambiato_zona(struct widget *w, int x, int y, int l, int a);

/* Il widget si vede: ne' lui ne' chi lo contiene e' nascosto. */
int widget_visibile(const struct widget *w);

/* Disabilita (1) o riabilita (0) il widget: disabilitato si vede spento e non sente niente
 * (ne' puntatore ne' tasti). Si ridisegna da solo. */
void widget_disabilita(struct widget *w, int disabilitato);

/* Mostra o nasconde un widget. Lo spazio si ridistribuisce con uikit_ridisponi. Il posto
 * dove stava (o dove sta) si ridisegna anche se adesso e' nascosto. */
void widget_nascondi(struct widget *w, int nascosto);

/* Copia la stringa 'da' in 'dove', grande 'massimo' byte (lo 0 finale compreso): quello
 * che non ci sta si taglia. 'dove' finisce sempre con lo 0. */
void copia_testo(char *dove, const char *da, int massimo);

#define MAX_TESTO_WIDGET 64
#define NESSUNA_ICONA (-1)

/* Un testo centrato in verticale nel rettangolo (x, y, ..., a), con il colore del pennello. */
int testo_centrato(struct pennello *p, const struct font *f, int x, int y, int a,
                   const char *s);

/* --- riga e colonna: il contenitore (la "box" di GTK, gli stack di SwiftUI) --- */

struct contenitore {
    struct widget base;
    int in_riga;                       /* 1 riga (da sinistra a destra), 0 colonna */
    int spazio;                        /* tra un figlio e l'altro */
};

/* Prepara un contenitore che mette i figli in RIGA, da sinistra a destra, con 'spazio'
 * pixel tra uno e l'altro. Nell'altra direzione prende tutto lo spazio che riceve ('largo'). */
void riga_prepara(struct contenitore *c, int spazio);

/* Come riga_prepara, ma in COLONNA: i figli dall'alto in basso. */
void colonna_prepara(struct contenitore *c, int spazio);

/* --- lo spazio elastico: vuoto, prende lo spazio che avanza --- */

struct spazio { struct widget base; };

/* Prepara uno spazio elastico: vuoto, prende lo spazio che avanza nel suo contenitore. */
void spazio_prepara(struct spazio *s);

/* Lo SPAZIO FISSO: vuoto, largo 'larghezza' (in una riga) o alto (in una colonna). Serve ad
 * allineare: per esempio i controlli della barra degli strumenti di Files cominciano dove
 * finisce la barra laterale. */
struct spazio_fisso { struct widget base; int misura; };

/* Prepara uno spazio fisso, vuoto, di 'misura' pixel. */
void spazio_fisso_prepara(struct spazio_fisso *s, int misura);

/* --- l'etichetta: un testo --- */

struct etichetta {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    const struct font *font;           /* FONT_TESTO, FONT_TITOLO... */
};

/* Prepara un'etichetta con 'testo' (al massimo MAX_TESTO_WIDGET - 1 byte: il resto si
 * taglia) nel 'font' dato (FONT_TESTO, FONT_TITOLO...), in TESTO_PRIMARIO, centrata in altezza. */
void etichetta_prepara(struct etichetta *e, const char *testo, const struct font *font);

/* Cambia il testo. (Lo spazio dell'etichetta resta quello deciso all'apertura della
 * finestra: un testo piu' lungo si taglia.) */
void etichetta_testo(struct etichetta *e, const char *testo);

/* --- il pulsante: 3 tipi, 3 misure, con o senza icona, anche solo icona --- */

/* I TIPI. Primario (l'azione principale: fondo scuro nel chiaro, chiaro nello scuro, ombra
 * "sollevato-pieno"), Secondario (fondo di controllo, ombra "sollevato"), Ghost (senza
 * fondo: compare solo sotto il puntatore, premuto e disabilitato sono piatti come gli
 * altri). Il colore NON dice piu' il significato (blu/verde/rosso/arancio non esistono
 * nella scala di grigi): chi vuole un'azione di rilievo usa il Primario.
 * I tipi COLORATI dicono il significato dell'azione, con i colori di ruolo (tema.h):
 * Pericolo (rosso: terminare, cancellare, cose che non si annullano), Successo (verde),
 * Avviso (giallo, testo scuro), Informazione (blu). Si comportano come il Primario
 * (ombra "sollevato-pieno", testo e icona nel colore *_TESTO del ruolo). */
enum { PRIMARIO, SECONDARIO, GHOST, PERICOLO, SUCCESSO, AVVISO, INFORMAZIONE };
/* Le MISURE: S 32, M 40, L 48 (ALTEZZA_S/M/L in stile.h); XS 24 solo per i pulsanti icona
 * (la chiusura della mini finestra). MISURA_XS e' in fondo per non cambiare gli indici. */
enum { MISURA_S, MISURA_M, MISURA_L, MISURA_XS };
/* VECCHIO: da eliminare quando le app saranno migrate. I nomi di prima restano come
 * sinonimi, cosi' Files, Impostazioni, Attivita e Contatore compilano senza toccarli:
 * NEUTRO e' il Secondario, tutti i colori (blu, verde, rosso, arancio) il Primario;
 * PICCOLO, MEDIO e GRANDE sono S, M e L. */
enum { NEUTRO = SECONDARIO, BLU = PRIMARIO, VERDE = PRIMARIO, ROSSO = PRIMARIO, ARANCIO = PRIMARIO };
enum { PICCOLO = MISURA_S, MEDIO = MISURA_M, GRANDE = MISURA_L };

struct pulsante;
typedef void (*al_clic_pulsante)(struct pulsante *p, void *dati);

struct pulsante {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];      /* vuoto: pulsante solo icona (tondo) */
    int icona;                         /* ICONA_..., o NESSUNA_ICONA */
    int colore, dimensione;            /* il tipo (PRIMARIO...) e la misura (MISURA_S...); 'colore' per compatibilita' */
    al_clic_pulsante al_clic;          /* chi chiamare quando viene premuto */
    void *dati;                        /* passato cosi' com'e' ad al_clic */
    int premuto;                       /* il pulsante del mouse e' giu', premuto qui sopra */
    int dentro;                        /* ... e il puntatore e' ancora sopra di lui */
};

/* Un pulsante secondario, di misura media. Per un pulsante solo icona, testo "". */
void pulsante_prepara(struct pulsante *b, const char *testo, int icona,
                      al_clic_pulsante al_clic, void *dati);

/* Il tipo (PRIMARIO, SECONDARIO, GHOST, PERICOLO, SUCCESSO, AVVISO, INFORMAZIONE) e la
 * misura (MISURA_S, _M, _L, e _XS per i pulsanti solo icona). Accetta anche i
 * vecchi nomi (NEUTRO, BLU..., PICCOLO...): sono sinonimi. */
void pulsante_stile(struct pulsante *b, int colore, int dimensione);

/* --- la casella di spunta e la scelta singola --- */

enum { SPENTO, ACCESO, MISTO };

struct casella;
typedef void (*al_cambio_casella)(struct casella *c, void *dati);

/* Una casella di spunta, o una scelta singola (il tondo: in un gruppo se ne accende
 * una sola). Si comportano allo stesso modo, cambia il disegno. */
struct casella {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    int valore;                        /* SPENTO, ACCESO, MISTO (solo la casella) */
    int tonda;                         /* 1: scelta singola */
    void *gruppo;                      /* le scelte con lo stesso gruppo si escludono */
    al_cambio_casella al_cambio;
    void *dati;
    int premuto;
};

/* Prepara una casella di spunta con 'testo' e 'valore' (SPENTO, ACCESO, MISTO). Un clic
 * la cambia e chiama 'al_cambio' (se non e' 0) con 'dati'. */
void casella_prepara(struct casella *c, const char *testo, int valore,
                     al_cambio_casella al_cambio, void *dati);

/* Prepara una scelta singola (il tondo) con 'testo', accesa (1) o spenta (0). Le scelte con
 * lo stesso 'gruppo' (un puntatore qualsiasi, lo stesso per tutte) si escludono: accenderne
 * una spegne le altre. Un clic chiama 'al_cambio' (se non e' 0) con 'dati'. */
void scelta_prepara(struct casella *c, const char *testo, void *gruppo, int accesa,
                    al_cambio_casella al_cambio, void *dati);

/* --- l'interruttore --- */

/* L'INTERRUTTORE, dal file Figma ("Interruttore", Componenti base): acceso o spento, con effetto
 * immediato. Un binario INCASSATO (INTERRUTTORE_LARGO x INTERRUTTORE_ALTO, tondo, ombra incassato)
 * con fondo SUPERFICIE_INCASSATA da spento e AZIONE_SFONDO da acceso, e il pomello SOLLEVATO
 * (tondo di INTERRUTTORE_POMELLO, ombra sollevato) a INTERRUTTORE_MARGINE dal bordo: a sinistra
 * da spento (SUPERFICIE_ELEVATA), a destra da acceso (AZIONE_TESTO, come quello che sta sopra
 * un'azione). Disabilitato e' piatto: binario CONTROLLO_SFONDO_DISABILITATO, pomello
 * TESTO_DISABILITATO, niente ombre. Non ha testo: l'etichetta e' un widget accanto. */
struct interruttore;
typedef void (*al_cambio_interruttore)(struct interruttore *i, void *dati);

struct interruttore {
    struct widget base;
    int acceso;
    al_cambio_interruttore al_cambio;
    void *dati;
    int premuto;
};

/* Prepara un interruttore, acceso (1) o spento (0). Un clic lo cambia e chiama
 * 'al_cambio' (se non e' 0) con 'dati'. */
void interruttore_prepara(struct interruttore *i, int acceso, al_cambio_interruttore al_cambio,
                          void *dati);

/* --- il campo di testo --- */

struct campo;
typedef void (*al_cambio_campo)(struct campo *c, void *dati);

struct campo {
    struct widget base;
    char testo[MAX_TESTO_WIDGET];
    char suggerimento[MAX_TESTO_WIDGET];   /* quello che si vede, piu' chiaro, quando e' vuoto */
    int icona;
    int errore;                            /* bordo del fuoco + triangolo di avviso (stato di errore) */
    int piccolo;                           /* 1: la RICERCA (pillola, 180 di larghezza, altezza S) */
    int misura;                            /* MISURA_S, _M (default) o _L: altezza, margini, font */
    int larghezza;                         /* 0: quella del disegno (280, la ricerca 180); se no questa */
    al_cambio_campo al_cambio;
    void *dati;
};

/* Prepara un campo di testo di misura M con il 'testo' di partenza, il 'suggerimento' (si
 * vede quando e' vuoto) e l'icona a sinistra (ICONA_..., o NESSUNA_ICONA). A ogni tasto che lo
 * cambia chiama 'al_cambio' (se non e' 0) con 'dati'. Si allarga a tutto lo spazio che riceve. */
void campo_prepara(struct campo *c, const char *testo, const char *suggerimento,
                   int icona, al_cambio_campo al_cambio, void *dati);

/* La misura del campo o della ricerca: MISURA_S (32), MISURA_M (40, il default del campo), MISURA_L (48). */
void campo_stile(struct campo *c, int misura);

/* Mette (1) o toglie (0) lo stato di errore del campo: bordo del fuoco e triangolo di
 * avviso. Si ridisegna da solo. */
void campo_errore(struct campo *c, int errore);

/* La ricerca piccola, per la barra degli strumenti: un campo di testo come gli altri
 * (si scrive, si cancella, al_cambio a ogni tasto), ma a PILLOLA, alta 32, larga 180, con la
 * lente. Come il campo e' incassata, e col fuoco ha bordo e anello. Non si allarga da solo (se deve, 'elastico'). */
void ricerca_prepara(struct campo *c, const char *suggerimento,
                     al_cambio_campo al_cambio, void *dati);

/* --- lo stato vuoto: un'area senza contenuto, o in errore --- */

/* LO STATO VUOTO, dal file Figma ("Stato vuoto"): al centro dello spazio che riceve, in
 * colonna, un'icona (facoltativa) di ICONA_OGGETTO (48) nel colore d'icona Secondario (le
 * schermate d'errore del disegno la vogliono Primario: stato_vuoto_colore_icona), il titolo
 * (FONT_SEZIONE_NUOVO, TESTO_PRIMARIO) e la spiegazione (FONT_CORPO, TESTO_SECONDARIO, a
 * capo da sola, al massimo STATO_VUOTO_RIGHE righe), tutto centrato; SPAZIO_8 tra un pezzo
 * e l'altro, PADDING ZERO (lo spazio intorno lo da' il pannello che lo contiene). Per una cartella vuota (senza icona), una
 * cartella che non si legge (ICONA_TRIANGLE_ALERT), un permesso negato (ICONA_LOCK).
 * Prende tutto lo spazio che avanza (elastico e largo). */
#define STATO_VUOTO_RIGHE 3

struct stato_vuoto {
    struct widget base;
    int icona;                          /* ICONA_..., o NESSUNA_ICONA */
    int colore_icona;                   /* COLORE_ICONA_... (tema.h): Secondario se non si dice */
    char titolo[MAX_TESTO_WIDGET];
    char spiegazione[STATO_VUOTO_RIGHE * MAX_TESTO_WIDGET];
};

/* Prepara uno stato vuoto con l'icona (ICONA_..., o NESSUNA_ICONA), il 'titolo' e la
 * 'spiegazione' (o 0, se non c'e'). */
void stato_vuoto_prepara(struct stato_vuoto *s, int icona_, const char *titolo,
                         const char *spiegazione);

/* Il colore dell'icona (COLORE_ICONA_..., tema.h). Nel disegno lo stato vuoto ha l'icona
 * Secondario; le schermate d'errore (cartella che non si legge, permesso negato, pacchetto
 * illeggibile) la mettono Primario. */
void stato_vuoto_colore_icona(struct stato_vuoto *s, int modo);

/* Cambia icona e testi (per esempio, da "cartella vuota" a "impossibile leggere"). */
void stato_vuoto_cambia(struct stato_vuoto *s, int icona_, const char *titolo,
                        const char *spiegazione);

/* --- l'avviso: un messaggio che interrompe e chiede conferma --- */

/* L'AVVISO, dal file Figma ("Avviso" e "Velo della finestra"). Un riquadro di vetro largo
 * AVVISO_LARGHEZZA, al centro della finestra che lo apre: un titolo, un messaggio (a capo
 * da solo, al massimo AVVISO_RIGHE righe) e in fondo a destra due pulsanti, Annulla
 * (Secondario) e la conferma (di solito Pericolo, per le cose che non si annullano).
 * Margine FINESTRA_PADDING (24), SPAZIO_8 tra titolo e messaggio, SPAZIO_24 prima dei
 * pulsanti; ombra "finestra".
 *
 * BLOCCANTE o no:
 *   - bloccante: la finestra sotto si spegne (titolo come una finestra sullo sfondo) e
 *     riceve il VELO (SUPERFICIE_VELO, con gli angoli della finestra); finche' non si
 *     risponde non si puo' usare niente, nemmeno chiudere o ridurre la finestra;
 *   - non bloccante: niente velo, la finestra resta attiva e usabile.
 * Invio = conferma, Esc = annulla.
 *
 *     static struct avviso riavvia;
 *     avviso_prepara(&riavvia, "Riavviare \"contatore\"?",
 *                    "Il processo viene chiuso e aperto di nuovo.", "Riavvia", PERICOLO, 1,
 *                    al_riavvia, 0);
 *     finestra_tk_avviso(&finestra, &riavvia);     // una volta, prima di uikit_esegui
 *     ...
 *     avviso_apri(&riavvia);                        // quando serve
 *
 * 'al_risposta' riceve la scelta: AVVISO_CONFERMA (1), AVVISO_ANNULLA (0) o, se c'e' il terzo
 * pulsante, AVVISO_ALTERNATIVA (2); l'avviso si chiude da solo.
 *
 * TRE SCELTE ("Scelte=Tre" in Figma): avviso_alternativa(&v, "Togli solo l'app", SECONDARIO)
 * aggiunge un terzo pulsante tra Annulla e la conferma; il riquadro diventa largo
 * AVVISO_LARGHEZZA_TRE (480). Per esempio Annulla / "Togli solo l'app" / "Togli app e dati". */
#define AVVISO_RIGHE 4
enum { AVVISO_ANNULLA, AVVISO_CONFERMA, AVVISO_ALTERNATIVA };

struct avviso;
typedef void (*al_risposta_avviso)(struct avviso *a, int scelta, void *dati);

/* L'avviso NON e' un figlio della finestra (non sta nel suo albero): la finestra lo tiene a
 * parte (finestra_tk_avviso), lo disegna dopo tutto il resto e gli da' i clic per primo.
 * Il suo 'genitore' e' comunque la finestra, cosi' widget_cambiato la trova. Il velo e
 * l'ombra (che escono dal riquadro piu' dei 24 pixel che un widget puo' sbordare) li
 * disegna la finestra (avviso_sotto); il riquadro, i testi e i pulsanti li disegna lui. */
struct avviso {
    struct widget base;                /* il riquadro */
    char titolo[MAX_TESTO_WIDGET];
    char messaggio[AVVISO_RIGHE * MAX_TESTO_WIDGET];
    int bloccante;
    int x, y, l, a;                    /* il riquadro, in coordinate della finestra */
    struct pulsante annulla, conferma;
    struct pulsante alternativa;       /* il terzo pulsante (avviso_alternativa), se 'tre' */
    int tre;
    al_risposta_avviso al_risposta;
    void *dati;
};

/* Prepara l'avviso (chiuso). 'tipo' e' il tipo del pulsante di conferma (PERICOLO,
 * PRIMARIO...), 'bloccante' 1 o 0. Annulla si puo' rinominare con avviso_annulla. */
void avviso_prepara(struct avviso *v, const char *titolo, const char *messaggio,
                    const char *conferma, int tipo, int bloccante,
                    al_risposta_avviso al_risposta, void *dati);

/* Cambia il testo di Annulla (per esempio "Apri cartella" in un avviso non bloccante). */
void avviso_annulla(struct avviso *v, const char *testo);

/* Aggiunge il terzo pulsante, tra Annulla e la conferma (di solito SECONDARIO): la risposta
 * sara' AVVISO_ALTERNATIVA. Prima di avviso_apri. */
void avviso_alternativa(struct avviso *v, const char *testo, int tipo);

/* Apre l'avviso della finestra (finestra_tk_avviso), al centro, e gli da' la tastiera
 * (Invio, Esc). Si chiude da solo quando si risponde. */
void avviso_apri(struct avviso *v);

/* --- la finestra: la radice dell'albero, con la barra del titolo --- */

struct finestra_tk;

/* Una chiamata "ogni tanto" (finestra_tk_ogni). */
#define MAX_CHIAMATE 4
struct chiamata_tk {
    void (*funzione)(struct finestra_tk *f, void *dati);   /* 0: posto libero */
    void *dati;
    unsigned int ms;
    unsigned int scadenza;             /* quando chiamarla, in tempo() */
};

struct finestra_tk {
    struct widget base;                /* il primo figlio e' il contenuto, il secondo (se
                                        * c'e') la barra degli strumenti */
    struct widget *strumenti;          /* la barra degli strumenti, o 0 (finestra_tk_barra) */
    char titolo[MAX_TITOLO];
    int l_fissa, a_fissa;              /* 0: la misura la decide il contenuto */
    int aree;                          /* 1: corpo a aree, padding CORPO_PADDING (finestra_tk_aree) */
    int senza_margine;                 /* 1: il contenuto arriva fino ai bordi (finestra_tk_bordo) */
    struct finestra finestra;          /* il collegamento con il compositore */
    int attiva;                        /* ha il fuoco: e' in primo piano */
    int da_ridisegnare;                /* c'e' qualcosa da ridisegnare: le zone qui sotto */
    struct zona_finestra cambiate[MAX_ZONE_MOSTRA];   /* le zone cambiate dall'ultimo ridisegno */
    int quante_cambiate;
    int tutta_cambiata;                /* tutta la finestra (l'impaginazione e' cambiata) */
    struct widget *catturato;          /* il widget su cui si e' premuto */
    struct widget *sotto;              /* quello che ha il puntatore sopra */
    struct widget *con_fuoco;          /* quello che riceve i tasti */
    int chiudi_premuto;
    int riduci_premuto;
    int comando_sopra;                 /* il puntatore e' sopra: 0 nessuno, 1 riduci, 2 chiudi */
    int da_chiudere;
    unsigned int pulsanti_prima;
    struct chiamata_tk chiamate[MAX_CHIAMATE];   /* i "chiamami ogni tanto" */
    struct avviso *avviso;             /* l'avviso della finestra (finestra_tk_avviso), o 0 */
    int mini;                          /* 1: mini finestra (finestra_tk_mini) */
    /* la tastiera fisica (tasti.h): chi ascolta le scorciatoie, e i modificatori premuti */
    struct ascoltatore_tk ascoltatori[MAX_ASCOLTATORI];
    unsigned int modificatori;         /* MOD_CTRL | MOD_MAIUSC | MOD_ALT, tenuti giu' adesso */
    /* L'ALTRA FINESTRA dell'applicazione (una sola, per le cose secondarie: le informazioni di
     * un file in una mini finestra), aperta con finestra_tk_apri_altra: il ciclo degli eventi
     * di questa finestra serve anche lei. 0 se non c'e'. */
    struct finestra_tk *altra;
    int aperta;                        /* 1 mentre la finestra c'e' sullo schermo */
    /* UN DESCRITTORE IN PIU' da guardare nel ciclo degli eventi (finestra_tk_guarda): quando
     * e' pronto (c'e' da leggere, o l'altro capo ha chiuso) si chiama al_pronto. 0: nessuno */
    int guardato;
    void (*al_pronto)(struct finestra_tk *f, void *dati);
    void *dati_pronto;
};

/* LA FINESTRA, dal file Figma. Sono DUE pezzi separati, uno sopra l'altro:
 *
 *   - la TESTATA (il titolo): alta TITOLO_ALTEZZA (40), con i soli angoli in ALTO
 *     arrotondati. Dentro, tre cose in fila: a sinistra uno spazio vuoto, al centro il
 *     titolo, a destra i comandi riduci e chiudi. I comandi sono pulsanti S (ALTEZZA_S, 32)
 *     tondi e "ghost" (senza fondo, il fondo compare solo sotto il puntatore), a
 *     TITOLO_MARGINE (4) dal bordo e l'uno dall'altro: 4 + 32 + 4 = 40, cioe' proprio
 *     l'altezza della testata. Lo spazio vuoto a sinistra e' largo quanto il gruppo dei
 *     comandi (TITOLO_COMANDI_LARGHEZZA), cosi' il titolo resta centrato nella finestra.
 *     Sotto la testata nessuna linea, mai (deciso dalla persona), e nessun pixel in piu':
 *     il corpo comincia subito sotto, come nel disegno;
 *   - il CORPO: tutto quello che sta sotto la testata, con i soli angoli in BASSO
 *     arrotondati. Comincia con la barra degli strumenti, se c'e', poi il contenuto
 *     dell'applicazione, con FINESTRA_PADDING (24) intorno. La finestra A AREE
 *     (finestra_tk_aree: la struttura delle finestre del disegno, Files, Attivita, Store,
 *     Terminale) ha invece CORPO_PADDING (8) su tutti i lati e CORPO_GAP (8) tra la barra
 *     degli strumenti e il contenuto: barra laterale, barra degli strumenti e pannello
 *     partono dalla stessa linea, a 8 dal bordo della finestra.
 *
 * La testata non e' contenuto: ha la sua funzione di disegno e le sue misure, e quello che
 * l'applicazione mette nella finestra comincia sempre sotto di lei (finestra_tk_corpo). */

/* Dove comincia il contenuto della finestra, senza il suo margine: sotto la testata e, se c'e'
 * (e non e' nascosta), sotto la barra degli strumenti. */
int finestra_tk_corpo(const struct finestra_tk *f);

/* Prepara la finestra dell'applicazione: il 'titolo' (nella testata) e il 'contenuto',
 * il widget che la riempie (di solito una colonna o una riga). Poi, se servono, le altre
 * finestra_tk_..., e infine uikit_esegui. */
void finestra_tk_prepara(struct finestra_tk *f, const char *titolo,
                         struct widget *contenuto);

/* Mette una barra degli strumenti sotto la barra del titolo (di solito una 'struct
 * barra_strumenti', piu' sotto): la finestra si allunga della sua altezza. Da chiamare
 * prima di uikit_esegui, dopo finestra_tk_prepara. */
void finestra_tk_barra(struct finestra_tk *f, struct widget *strumenti);

/* LA FINESTRA A AREE, la struttura di tutte le finestre del disegno: il corpo sotto la testata
 * ha CORPO_PADDING (8) su tutti i lati e CORPO_GAP (8) tra le aree (barra degli strumenti,
 * barra laterale, pannello). Le aree non hanno padding loro: lo spazio dentro lo da' solo il
 * pannello (pannello.h, o una pagina con il fondo). Con finestra_tk_barra la barra degli
 * strumenti prende tutta la larghezza del corpo, sopra il contenuto; chi la vuole solo sopra
 * un'area (Files: a destra della barra laterale) la mette nel suo contenuto, in una colonna a
 * CORPO_GAP. Prima di uikit_esegui. */
void finestra_tk_aree(struct finestra_tk *f);

/* Il contenuto senza nessun margine, fino ai bordi della finestra: solo per chi disegna tutto
 * da se' (le scene dell'anteprima). Le applicazioni usano finestra_tk_aree. */
void finestra_tk_bordo(struct finestra_tk *f);

/* Cambia il titolo della finestra (per esempio, la cartella in cui si e'). Lo disegna la
 * finestra stessa, nella sua barra: basta ridisegnarla. */
void finestra_tk_titolo(struct finestra_tk *f, const char *titolo);

/* LA MINI FINESTRA, per le cose secondarie (informazioni su un file, un avanzamento, uno
 * strumento): testata da ALTEZZA_BARRA_MINI (28) con il titolo piccolo (FONT_ETICHETTA) e il
 * solo pulsante di chiusura XS (24): non si riduce. Il contenuto ha SPAZIO_16 di margine.
 * Prima di uikit_esegui, dopo finestra_tk_prepara. */
void finestra_tk_mini(struct finestra_tk *f);

/* Da' alla finestra il suo avviso (chiuso): si apre con avviso_apri. Prima di
 * uikit_esegui, dopo finestra_tk_prepara. Una finestra ha un solo avviso alla volta. */
void finestra_tk_avviso(struct finestra_tk *f, struct avviso *v);

/* Chiude la finestra, come il pulsante "chiudi": uikit_esegui torna. */
void finestra_tk_chiudi(struct finestra_tk *f);

/* Una misura fissa per la finestra, invece di quella del contenuto. */
void finestra_tk_dimensione(struct finestra_tk *f, int l, int a);

/* Chiama 'funzione' ogni 'ms' millisecondi, finche' la finestra e' aperta (0: smette).
 * La prima volta dopo 'ms'. La stessa funzione con gli stessi dati e' la stessa chiamata:
 * chiederla di nuovo cambia il suo ritmo (e la sua prossima volta), non ne aggiunge
 * un'altra. Cosi' una funzione puo' anche decidere da se' quando essere richiamata. */
void finestra_tk_ogni(struct finestra_tk *f, unsigned int ms,
                      void (*funzione)(struct finestra_tk *f, void *dati), void *dati);

/* Guarda anche il descrittore 'd' (una pipe, un terminale, un socket dell'app): quando c'e'
 * da leggere, o l'altro capo ha chiuso, il ciclo degli eventi chiama 'funzione', che legge
 * (senza bloccarsi) e segna cosa ridisegnare; come g_io_add_watch in GTK. Uno solo per
 * finestra; 'funzione' 0 smette. */
void finestra_tk_guarda(struct finestra_tk *f, int d,
                        void (*funzione)(struct finestra_tk *f, void *dati), void *dati);

/* Rifa' l'impaginazione dentro la finestra (dopo aver mostrato o nascosto dei widget). */
void uikit_ridisponi(struct finestra_tk *f);

/* --- la barra degli strumenti --- */

/* LA BARRA DEGLI STRUMENTI, dal file Figma. Una riga alta ALTEZZA_S (32) con PADDING ZERO, senza
 * un fondo suo (si vede il vetro della finestra): i suoi controlli S, a GAP_M (8) l'uno
 * dall'altro. Una cosa 'elastica' (il percorso, o uno spazio) prende la larghezza che avanza.
 * Lo spazio intorno lo da' il corpo della finestra: con finestra_tk_barra sta in cima al corpo,
 * larga quanto tutta la finestra meno 8 per lato (finestra_tk_aree, o la striscia di 48 delle
 * altre finestre); oppure l'applicazione la mette dove vuole nel suo contenuto (Files: sopra il
 * pannello, a destra della barra laterale).
 *
 * Un clic sullo spazio vuoto della barra trascina la finestra, come sulla testata (cosi'
 * fa il Mac): vedi "il puntatore", nel ciclo degli eventi.
 *
 *     barra_strumenti_prepara(&strumenti);
 *     barra_strumenti_aggiungi(&strumenti, &frecce.base);
 *     barra_strumenti_aggiungi(&strumenti, &dove.base);
 *     finestra_tk_barra(&finestra, &strumenti.base);
 *
 * Dentro c'e' una riga (il suo unico figlio), grande quanto la barra, che mette in fila le
 * cose. La barra e' alta quanto le sue cose: ALTEZZA_S (32), o 33 con le schede. */
struct barra_strumenti {
    struct widget base;
    struct contenitore riga;
};

/* Prepara una barra degli strumenti vuota. */
void barra_strumenti_prepara(struct barra_strumenti *b);

/* Mette 'w' in fondo alla barra degli strumenti, a destra di quello che c'e' gia'. */
void barra_strumenti_aggiungi(struct barra_strumenti *b, struct widget *w);

/* IL PULSANTE DELLA BARRA, dal file Figma. Fuori da un gruppo e' un pulsante GHOST S: senza
 * fondo a riposo, CONTROLLO_SFONDO sotto il puntatore, CONTROLLO_SFONDO_SOPRA premuto (gli
 * stessi stati del pulsante ghost). Solo icona e' TONDO (32 x 32, icona 16); con un testo
 * (pulsante_barra_testo) e' una PILLOLA, larga quanto il testo piu' PADDING_S per lato.
 * Scatta come i pulsanti normali, quando si lascia. Disabilitato (per esempio "avanti"
 * quando non c'e' dove andare) non sente niente e ha l'icona (o il testo) nel colore
 * Attenuato, come l'"Azione di gruppo" disabilitata del disegno; nessun fondo.
 *
 * Dentro una PILLOLA (il gruppo, sotto) cambia misura e aspetto: e' una voce alta 24
 * (GRUPPO_ELEMENTO). Se il gruppo e' a scelta, la voce accesa e' SOLLEVATA (SUPERFICIE_ELEVATA
 * con l'ombra "sollevato", testo e icona TESTO_PRIMARIO) e le altre sono senza fondo con
 * TESTO_SECONDARIO; sotto il puntatore, una voce spenta prende un fondo piatto leggero
 * (CONTROLLO_SFONDO). Nei gruppi non a scelta (indietro / avanti: il "Gruppo di azioni" del
 * disegno) le voci non si accendono mai: sono ghost di 24 con testo e icona TESTO_SECONDARIO,
 * CONTROLLO_SFONDO sotto il puntatore e CONTROLLO_SFONDO_SOPRA premute. */
struct pulsante_barra;
typedef void (*al_clic_pulsante_barra)(struct pulsante_barra *b, void *dati);

struct pulsante_barra {
    struct widget base;
    int icona;
    char testo[MAX_TESTO_WIDGET];      /* vuoto: si vede l'icona; se no il testo, senza icona */
    int acceso;                        /* in una pillola a scelta: e' quello scelto */
    al_clic_pulsante_barra al_clic;
    void *dati;
    int premuto, dentro;               /* come il pulsante (vedi pressione) */
};

/* La pillola (il gruppo, descritto sotto): qui serve sapere se e' a scelta e la sua misura. */
struct pillola {
    struct widget base;
    int a_scelta;
    int dimensione;                    /* MISURA_S (32, la barra degli strumenti) o MISURA_M (40) */
};

/* LA PILLOLA: un gruppo di pulsanti, il "gruppo di pulsanti" del disegno. E' un binario
 * INCASSATO (contiene e riceve): fondo SUPERFICIE_INCASSATA con l'ombra "incassato" (la
 * parte interna, che lo scava, e un filo esterno), MAI un bordo. Alto ALTEZZA_S (32),
 * pillola (raggio = meta' altezza), con GRUPPO_PADDING (4) intorno alle voci, che sono alte
 * GRUPPO_ELEMENTO (24): 4 + 24 + 4. Le voci con l'icona sono tonde (24 x 24), quelle con un
 * testo larghe quanto il testo piu' PADDING_S per lato. Puo' essere:
 *   - di pulsanti semplici: ognuno fa la sua cosa (indietro, avanti);
 *   - a SCELTA: ne e' acceso uno solo, quello premuto per ultimo (griglia o lista), come
 *     le scelte singole; la voce accesa e' il pezzo mobile, SOLLEVATO dentro il binario. Il
 *     clic si fa sentire anche sul pulsante gia' acceso. */

/* Accende o spegne un pulsante; in una pillola a scelta, accenderlo spegne gli altri. */
void pulsante_barra_accendi(struct pulsante_barra *b, int acceso);

/* Prepara un pulsante della barra con l'icona (ICONA_...). Il clic chiama 'al_clic' (se
 * non e' 0) con 'dati'. */
void pulsante_barra_prepara(struct pulsante_barra *b, int icona,
                            al_clic_pulsante_barra al_clic, void *dati);

/* Un pulsante della barra con un testo invece dell'icona: una scheda (vedi sopra). */
void pulsante_barra_testo(struct pulsante_barra *b, const char *testo,
                          al_clic_pulsante_barra al_clic, void *dati);

/* 'a_scelta': 1 se ne resta acceso uno solo (poi pulsante_barra_accendi per il primo).
 * Nasce S (32, quella della barra degli strumenti); pillola_dimensione la fa M. */
void pillola_prepara(struct pillola *p, int a_scelta);

/* La misura del gruppo (Figma "Gruppo", Dimensione): MISURA_S, alto ALTEZZA_S (32), voci di 24
 * con il testo del pulsante S; o MISURA_M, alto ALTEZZA_M (40), voci di 32 con PADDING_M ai lati
 * e il testo del pulsante M, attaccate (la scelta del tema in Impostazioni). Prima di aprire la
 * finestra. */
void pillola_dimensione(struct pillola *p, int dimensione);

/* Mette il pulsante 'b' in fondo alla pillola. */
void pillola_aggiungi(struct pillola *p, struct pulsante_barra *b);

/* IL PERCORSO: dove si e', come nel Finder. I nomi delle cartelle, dalla piu' esterna a
 * quella in cui si e', separati da una barra "/" (FONT_CORPO, TESTO_SECONDARIO), con
 * PERCORSO_SPAZIO (4) prima e dopo, come nella barra di Files del file Figma. I nomi sono in
 * FONT_CORPO e TESTO_SECONDARIO, e TESTO_PRIMARIO sotto il puntatore: un clic su un nome
 * chiama al_clic con il suo numero (0 il primo). L'ultimo, la cartella in cui si e', e' in
 * FONT_CORPO_MEDIO e TESTO_PRIMARIO, e non si clicca.
 * Il percorso e' elastico: prende la larghezza che avanza nella barra. Se non ci sta,
 * si tolgono i nomi dal principio (restano gli ultimi, i piu' utili); se non ci sta
 * neanche l'ultimo, lo si taglia.
 *
 * Ogni nome e' un widget figlio: cosi' UIKit sa sopra quale nome e' il puntatore
 * (lo accende) e a quale mandare il clic. */
#define MAX_NOMI_PERCORSO 8   /* le cartelle che il percorso sa mostrare */

struct percorso;
typedef void (*al_clic_percorso)(struct percorso *p, int indice, void *dati);

struct nome_percorso {
    struct widget base;
    char nome[MAX_TESTO_WIDGET];
    int indice;
    int premuto, dentro;
};

struct percorso {
    struct widget base;
    struct nome_percorso nomi[MAX_NOMI_PERCORSO];
    int quanti;
    al_clic_percorso al_clic;
    void *dati;
};

/* Prepara un percorso vuoto (i nomi con percorso_metti). Il clic su un nome chiama
 * 'al_clic' (se non e' 0) con il numero del nome e 'dati'. */
void percorso_prepara(struct percorso *p, al_clic_percorso al_clic, void *dati);

/* Cambia il percorso: 'quanti' nomi, dal piu' esterno a quello in cui si e'. Si puo'
 * chiamare anche con la finestra aperta (per esempio dopo un clic su un nome). */
void percorso_metti(struct percorso *p, const char *const *nomi, int quanti);

/* --- lo scorrimento e la barra di scorrimento --- */

/* Dove si e' in un contenuto piu' alto della sua vista, in pixel. Chi disegna il
 * contenuto lo sposta in su di 'posizione'; chi riceve un clic gliela somma. Gli ultimi
 * due campi servono alla rotella (barra_scorrimento_rotella). */
struct scorrimento {
    int posizione;                     /* i pixel del contenuto passati sopra la vista */
    int totale;                        /* l'altezza di tutto il contenuto */
    int vista;                         /* l'altezza della parte che si vede */
    unsigned int rotella_prima;        /* tempo() dello scatto di prima */
    int raffica;                       /* gli scatti della raffica in corso (col segno del verso) */
};

/* Il contenuto o la vista hanno cambiato altezza: la posizione resta dentro i limiti. */
void scorrimento_misure(struct scorrimento *s, int totale, int vista);

/* LA BARRA DI SCORRIMENTO, dal file Figma (il componente "Barra di scorrimento"). Sta
 * SOPRA il contenuto, in una striscia larga 14 pixel lungo il suo bordo destro, e:
 *   - a riposo non si vede (e non si sente: i clic vanno al contenuto sotto);
 *   - quando si scorre compare (in 100 ms): il cursore, sottile (8 pixel), da solo, senza
 *     binario; resta un secondo dopo l'ultimo scatto, poi svanisce (in 300 ms);
 *   - con il puntatore sopra, o mentre la si trascina, dietro al cursore compare (in 150 ms)
 *     il binario da 12 pixel, incassato; il cursore resta dov'e', con 2 pixel di margine per
 *     lato. Finche' il puntatore e' sopra non svanisce.
 * Il cursore e' lungo quanto la parte che si vede, in proporzione (almeno SCORRIMENTO_CURSORE_LUNGHEZZA, 64 pixel), e sta
 * dove si e' nel contenuto. Trascinandolo il contenuto scorre; un clic sul binario porta
 * li' il centro del cursore. Se il contenuto ci sta tutto, la barra non compare mai.
 *
 * LE ANIMAZIONI. A ogni passo si guarda quanto tempo e' passato, e opacita' e spessore si
 * avvicinano al loro obiettivo di conseguenza: anche se un passo arriva in ritardo,
 * l'animazione dura quanto deve. I passi li fa una chiamata "ogni tanto" di UIKit, che
 * c'e' solo mentre qualcosa si muove: con la barra ferma, la finestra non si ridisegna.
 * Mentre la barra aspetta il secondo prima di svanire, la chiamata e' una sola, alla fine.
 *
 * Per usarla: il widget che scorre la tiene tra i suoi figli (widget_aggiungi), le dice
 * dov'e' la vista (barra_scorrimento_posto) e quanto e' alto il contenuto
 * (scorrimento_misure su b->scorrimento), e le passa la rotella
 * (barra_scorrimento_rotella). Disegnando, sposta il contenuto di b->scorrimento.posizione. */

struct barra_scorrimento {
    struct widget base;
    struct scorrimento scorrimento;
    int opacita;                       /* 0 nascosta, 255 del tutto visibile */
    int spessore;                      /* quanto si vede il binario: 0 niente, 255 tutto */
    unsigned int ultimo;               /* tempo() dell'ultimo scatto o trascinamento */
    unsigned int passo_prima;          /* tempo() del passo di animazione di prima */
    unsigned int attesa;               /* ogni quanti ms la si chiama ora (0: non la si chiama) */
    int trascinata;                    /* il cursore e' preso con il mouse */
    int presa;                         /* dove lo si e' preso, dal suo inizio */
    int fuori;                         /* gli ultimi pixel a destra del widget che scorre sono
                                        * il padding del contenitore (barra_scorrimento_riserva) */
};

/* La rotella sopra il contenuto: scorre di 'scatti' (positivi: in basso). 0 se il
 * contenuto ci sta tutto e non c'e' niente da scorrere. */
int barra_scorrimento_rotella(struct barra_scorrimento *b, int scatti);

/* Prepara una barra di scorrimento: ferma e nascosta, finche' non si scorre. */
void barra_scorrimento_prepara(struct barra_scorrimento *b);

/* LA CORSIA DELLA BARRA: la barra non va mai sopra il contenuto. Sta nella striscia di
 * SCORRIMENTO_ZONA (14) lungo il bordo destro del widget che scorre, e quella striscia deve
 * essere vuota: e' il padding a destra.
 *   - 'fuori': se il widget sta in una pagina con un margine, la pagina gli allarga lo spazio
 *     fino al proprio bordo e gli dice quanto (pagina_disponi): quei pixel sono il padding del
 *     contenitore, il contenuto non li usa mai e la barra ci sta dentro (se sono almeno 14);
 *   - 'libero': il padding che il contenuto ha gia' a destra, suo (la griglia: 8);
 *   - se il contenuto scorre e i due insieme non bastano, il contenuto lascia libero quello
 *     che manca: il padding a destra diventa di 14, e li' sta la barra.
 * Dice quanti pixel a destra del widget il contenuto non usa. Se il contenuto ci sta tutto
 * (la barra non c'e') resta solo 'fuori': il padding e' quello di sempre. */
int barra_scorrimento_riserva(const struct barra_scorrimento *b, int libero);

/* La vista del contenuto e' il rettangolo (x, y, l, a): la barra va lungo il suo bordo
 * destro, nella corsia (barra_scorrimento_riserva). Da chiamare quando il widget che scorre
 * viene disposto. */
void barra_scorrimento_posto(struct barra_scorrimento *b, int x, int y, int l, int a);

/* --- il ciclo degli eventi --- */

/* Il puntatore sopra un widget nuovo (o su nessuno): si spegne il vecchio, si accende
 * il nuovo. */
void uikit_sotto(struct finestra_tk *f, struct widget *w);

/* Da' il fuoco della tastiera a 'w' (0: a nessuno); chi lo aveva lo perde. Tutti e due si
 * ridisegnano. Di solito lo fa UIKit da sola, al clic su un widget che prende i tasti. */
void uikit_fuoco(struct finestra_tk *f, struct widget *w);

/* IL PUNTATORE, come lo consegna il ciclo degli eventi. Un clic su "chiudi" chiude; sulla
 * barra (o sul vuoto della barra degli strumenti) e' un "trascinami"; nel contenuto e tra gli strumenti cattura il widget
 * sotto, che sentira' il puntatore finche' il pulsante non si lascia, e se prende i
 * tasti gli da' il fuoco.
 *
 * IL TASTO DESTRO cattura il widget sotto come il sinistro (e gli da' il fuoco): sara'
 * lui, guardando PULSANTE_DESTRO in 'pulsanti', a decidere cosa farne (di solito, un
 * menu). Barra del titolo, "trascinami", riduci e chiudi restano del solo sinistro. I
 * widget che guardano solo il sinistro, al destro non fanno niente.
 *
 * Senza pulsanti premuti, il widget sotto il puntatore che lo vuole sapere riceve il
 * MOVIMENTO (il campo 'movimento' del suo tipo). */

/* --- la tastiera fisica: le scorciatoie (tasti.h) --- */

/* Aggiunge un ascoltatore dei tasti fisici della finestra: lo sentira' prima di quelli
 * aggiunti prima di lui. Di nuovo la stessa funzione con gli stessi dati: niente. */
void finestra_tk_ascolta_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati);

/* Toglie l'ascoltatore dei tasti fisici aggiunto con finestra_tk_ascolta_tasti (la stessa
 * funzione con gli stessi dati). Se non c'e', niente. */
void finestra_tk_smetti_tasti(struct finestra_tk *f, ascolta_tasti_tk funzione, void *dati);

/* --- l'altra finestra (una mini finestra per le cose secondarie) --- */

/* Chiude l'altra finestra (se c'e'): sparisce dallo schermo, la memoria torna libera. */
void finestra_tk_chiudi_altra(struct finestra_tk *f);

/* Misura 'altra' (preparata come ogni finestra: finestra_tk_prepara, di solito anche
 * finestra_tk_mini), la apre sullo schermo e la fa servire dal ciclo degli eventi di 'f'. Se
 * c'e' gia' un'altra finestra aperta, prima la si chiude. 0 se fatto, -1 se no. */
int finestra_tk_apri_altra(struct finestra_tk *f, struct finestra_tk *altra);

/* CAMBIA IL TEMA DI TUTTO IL SISTEMA ("chiaro" o "scuro"): lo scrive nelle preferenze
 * (tema_salva) e chiede al compositore di dirlo a tutte le finestre e ai pannelli aperti
 * (CAMBIA_TEMA, gkit/sistema/protocollo.h): ognuno rilegge il tema e si ridisegna, anche questa finestra (con
 * TEMA_CAMBIATO, come le altre). Funziona solo per chi ha il permesso "impostazioni.sistema"
 * (Impostazioni): per gli altri il file cambia, ma il compositore non dice niente a nessuno.
 * 0, o -1. */
int uikit_cambia_tema(struct finestra_tk *f, const char *nome);

/* Apre la finestra e la fa vivere: torna quando si preme "chiudi" (0) o se il
 * compositore la chiude (1). Serve anche l'altra finestra, se c'e' (finestra_tk_apri_altra):
 * quando si chiude lei (la sua X), si chiude solo lei.
 *
 * IL GIRO. (1) Se c'e' qualcosa da ridisegnare e un'immagine libera, si ridisegna. (2) Si
 * aspetta il compositore (su tutte e due le finestre) fino alla prima scadenza dei "chiamami
 * ogni tanto", e poi si serve TUTTO quello che e' arrivato, senza aspettare piu': i clic, i
 * tasti, la rotella, i movimenti del puntatore (dei movimenti uno di fila all'altro conta
 * l'ultimo). (3) Si chiamano i "chiamami ogni tanto" scaduti: l'input passa davanti alle
 * animazioni, come in GTK, cosi' due scatti veloci restano una raffica anche quando
 * ridisegnare costa. Poi da capo: si ridisegna una volta sola per tutto quello che e'
 * successo. Se le immagini sono tutte e due del compositore non le si aspetta: si continua a
 * servire le notizie finche' lui ne restituisce una (RILASCIATA). */
int uikit_esegui(struct finestra_tk *f);

/* I componenti che stanno ognuno nel suo file (vedi LIBRERIE.md). */
#include "elemento_file.h"    /* l'elemento di un file (e testo_a_capo, che usano anche avviso e stato vuoto) */
#include "oggetti.h"          /* le icone di file e cartelle (l'"Oggetto" di Figma) */
#include "menu.h"             /* il menu: voce, intestazione, separatore, sottomenu */
#include "barra_laterale.h"   /* la barra laterale e le sue voci */
#include "tabella.h"          /* la tabella: intestazioni di colonna con l'ordine, righe */
#include "griglia.h"          /* la griglia di icone: i tile */
#include "pannello.h"         /* il pannello chiaro incassato del contenuto (Files) */
#include "informazioni.h"     /* il pannello delle informazioni, in una mini finestra */
/* i componenti di Attivita' e dello Store */
#include "paragrafo.h"
#include "pagina.h"
#include "schede.h"
#include "grafico.h"
#include "etichetta_stato.h"
#include "messaggio.h"
#include "icona_app.h"
#include "riga_permesso.h"
#include "riga_app.h"

#endif
