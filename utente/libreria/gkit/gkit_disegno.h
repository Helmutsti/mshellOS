/* GKit, gli strumenti di disegno (vedi LIBRERIE.md): come Cairo su Linux o Core Graphics
 * sul Mac. Disegna su una TELA (un pezzo di memoria con dei pixel, per esempio l'immagine
 * di una finestra) e non sa niente di finestre o di compositore. Sa COME si disegna, senza
 * nessun gusto: non conosce i colori del tema, le misure del design, il font e le icone del
 * sistema (quelli sono di UIKit, in uikit/). E' un header PUBBLICO di GKit (gkit.h spiega i
 * livelli): il codice e' in gkit/interno/ (pennello.c, forme.c, testo.c, immagini.c,
 * immagine_file.c, materiali_file.c).
 *
 * L'IDEA DEL PENNELLO. Invece di passare colore, opacita' e ritaglio a ogni disegno, si
 * prepara un pennello e si dice "da adesso questo colore": i disegni dopo lo usano. Il
 * pennello si puo' SALVARE e RIPRISTINARE, come un segnalibro.
 *
 *     struct tela t = { pixel, larghezza, altezza };
 *     struct pennello p;
 *     pennello_prepara(&p, &t);
 *     pulisci(&p);                                   tutta trasparente
 *     pennello_colore(&p, 0x24FFFFFF);               bianco al 14%
 *     riempi_tondo(&p, 10, 10, 120, 36, 8);          angoli di raggio 8
 *     pennello_colore(&p, OPACO(0xFFFFFF));
 *     testo(&p, font, 22, 19, "Ciao");               con il font che riceve
 *
 * I COLORI sono 0xAARRGGBB: AA e' quanto e' opaco (0 invisibile, FF pieno). I pixel della
 * tela invece sono PREMOLTIPLICATI: ogni colore e' gia' moltiplicato per la sua opacita'
 * (un bianco al 50% e' 0x80808080). E' il formato di Wayland e delle schede video: per
 * mettere un pixel sopra un altro basta "sopra + sotto * (1 - opacita' di sopra)".
 *
 * I BORDI LISCI (antialiasing). Per ogni pixel si calcola QUANTO e' coperto dalla forma,
 * da 0 a 256, e lo si colora solo per quella parte. Per le forme arrotondate si usa la
 * DISTANZA del centro del pixel dal bordo: dentro di mezzo pixel o piu', coperto tutto;
 * fuori di mezzo pixel o piu', niente; in mezzo, a sfumare. Tutto con numeri interi, in
 * 256esimi di pixel: i programmi non possono usare i numeri con la virgola (il kernel
 * non salva quella parte del processore quando passa da un programma all'altro).
 *
 * TUTTO PASSA DAL PENNELLO. Ogni disegno colora solo dentro il ritaglio del pennello, con
 * il suo colore e la sua opacita', e mette i pixel SOPRA quelli che ci sono (le forme
 * semitrasparenti si sommano a quello che c'e' sotto). Un pennello con il colore del tutto
 * trasparente, o con opacita' 0, non lascia segni.
 *
 * IL TESTO, LE ICONE E LE IMMAGINI si ricevono da chi disegna: un font, un'icona o
 * un'immagine sono tabelle gia' calcolate (struct font, struct icona, struct immagine, qui
 * sotto), e qui si colorano e si mettono sulla tela. GKit sa leggerli da un file su disco
 * (font_da_file, icona_da_file, immagine_da_file, qui sotto), ma non sa quali siano quelli
 * del sistema (i font IBM Plex e Inter, le icone Lucide, gli oggetti): sono di UIKit,
 * uikit/materiali.h.
 *
 * Coordinate in pixel, (0, 0) in alto a sinistra, y verso il basso. Nessuna funzione di
 * disegno puo' non riuscire: quello che cade fuori dalla tela o dal ritaglio non si disegna. */
#ifndef GKIT_DISEGNO_H
#define GKIT_DISEGNO_H

/* --- i materiali che si ricevono: font, icone, immagini ---
 * Tabelle gia' calcolate: le prepara strumenti/grafica/genera.py (le lettere e le icone gia'
 * disegnate, misura per misura) e stanno in file su disco (qui sotto, "I MATERIALI DA UN
 * FILE"). */

/* Una lettera: dove sta la sua tabella di alfa, dove va rispetto al punto di scrittura
 * (x a destra, y in su dalla linea di base) e di quanto si avanza dopo. */
struct glifo {
    unsigned int inizio;
    signed char x, y;
    unsigned char l, a, avanzo;
};

struct font {
    const struct glifo *glifi;
    const unsigned char *alfa;
    unsigned char indice[256];    /* carattere (code page 437) -> glifo; 0 = non c'e' */
    unsigned char altezza;        /* l'altezza di una riga */
    unsigned char ascesa;         /* dalla cima della riga alla linea di base */
};

/* Un'icona a una misura: lato x lato pixel di alfa. Un'icona intera e' un elenco delle sue
 * misure, dalla piu' piccola alla piu' grande. */
struct icona {
    unsigned char lato;
    const unsigned char *alfa;
};

/* Un'immagine a colori: l x a pixel 0xAARRGGBB premoltiplicati. */
struct immagine {
    unsigned short l, a;
    const unsigned int *pixel;
};

/* UN'IMMAGINE DA UN FILE (gkit/interno/immagine_file.c): per ora solo PNG, decodificate da
 * stb_image (esterni/stb). Si legge com'e', senza ingrandirla o rimpicciolirla. 0 se fatto,
 * con i pixel in memoria nuova (si liberano con immagine_libera); -1 se il file manca, e'
 * piu' grande di 4 MB o non e' una PNG, e allora 'im' resta vuota. (Solo per i programmi di
 * mshell: nell'anteprima sul Mac non c'e'.) */
int immagine_carica(const char *percorso, struct immagine *im);

/* Restituisce la memoria dei pixel di un'immagine letta con immagine_carica, e la lascia
 * vuota (0 x 0). */
void immagine_libera(struct immagine *im);

/* I MATERIALI DA UN FILE (gkit/interno/materiali_file.c): un font, un'icona o un'immagine
 * gia' calcolati, nei formati di strumenti/grafica/formato.py (.font, .icon, .image). Il file
 * si mette in memoria in sola lettura (mmap) e le tabelle puntano dentro: i programmi che
 * usano lo stesso file ne condividono la memoria. Resta in memoria finche' il programma vive
 * (non si libera). Il file si controlla tutto prima di usarlo; se manca o e' rovinato lo si
 * scrive sullo standard error ("gkit: non riesco a usare <file>: <perche'>"), si torna -1 e
 * si da' qualcosa che si disegna senza pericolo:
 *   font_da_file       0 se fatto; se no un font di riserva: ogni lettera e' un rettangolo
 *                      vuoto (si vede che il testo c'e' e che il font manca), alto 16;
 *   icona_da_file      riempie 'misure' (al massimo 'massimo') e torna quante sono; se non
 *                      va, misure[0] e' un'icona vuota (lato 0: con quante = 1 non disegna niente);
 *   immagine_da_file   0 se fatto; se no un'immagine vuota (0 x 0: non disegna niente). */
int font_da_file(const char *percorso, struct font *f);
int icona_da_file(const char *percorso, struct icona *misure, int massimo);
int immagine_da_file(const char *percorso, struct immagine *im);

/* LA CARTELLA DELLE RISORSE del sistema: /system/resources (fonts/, icons/, wallpapers/),
 * o quella della variabile d'ambiente MSHELL_RISORSE se c'e' (per gli attrezzi che girano
 * fuori da mshell: l'anteprima la fa puntare a risorse/ del repository). risorse_cartella la
 * restituisce; percorso_risorsa scrive in 'dove' "<cartella>/<nome>" e torna 0, o -1 se non
 * ci sta in 'spazio' (e allora e' tagliato). */
#define RISORSE_CARTELLA "/system/resources"
#define PERCORSO_RISORSA_MAX 256
const char *risorse_cartella(void);
int percorso_risorsa(char *dove, int spazio, const char *nome);

/* --- i colori e la tela --- */

#define OPACO(rgb) (0xFF000000u | (unsigned int)(rgb))
/* Un colore con opacita' da 0 a 255: VELATO(0xFFFFFF, 36) e' il bianco al 14%. */
#define VELATO(rgb, alfa) ((unsigned int)(alfa) << 24 | (unsigned int)(rgb))

struct tela {
    unsigned int *pixel;       /* 0xAARRGGBB premoltiplicati, riga dopo riga */
    int larghezza, altezza;
};

/* --- il pennello --- */

/* Lo stato che si salva e si ripristina. */
struct stato_pennello {
    unsigned int colore;
    int opacita;               /* 0-255: moltiplica tutto quello che si disegna */
    int spessore;              /* per linee e contorni dritti */
    int rx, ry, rx2, ry2;      /* il ritaglio: si disegna solo con rx <= x < rx2, ry <= y < ry2 */
};

#define MAX_SALVATI 16

/* Il pennello: lo prepara pennello_prepara, e si cambia con le funzioni qui sotto. */
struct pennello {
    struct tela *tela;
    struct stato_pennello s;
    struct stato_pennello salvati[MAX_SALVATI];
    int quanti_salvati;
};

/* Prepara il pennello per disegnare sulla tela 't': nero pieno, opacita' piena, spessore 1,
 * ritaglio su tutta la tela, niente salvato. */
void pennello_prepara(struct pennello *p, struct tela *t);

/* Da adesso si disegna nel colore 'argb' (0xAARRGGBB, non premoltiplicato). */
void pennello_colore(struct pennello *p, unsigned int argb);

/* Da adesso tutto e' anche piu' trasparente: l'opacita' si moltiplica per 'opacita' (0-255).
 * Serve per i widget disabilitati (al 40%), che restano visibili ma "spenti". */
void pennello_opacita(struct pennello *p, int opacita);

/* Lo spessore di linee e contorni dritti, in pixel (almeno 1). */
void pennello_spessore(struct pennello *p, int spessore);

/* Restringe il ritaglio a questo rettangolo (dentro quello che c'era gia': non si allarga mai;
 * per tornare a quello di prima si ripristina il pennello). */
void pennello_ritaglia(struct pennello *p, int x, int y, int l, int a);

/* Mette da parte lo stato del pennello (colore, opacita', spessore, ritaglio), al massimo
 * MAX_SALVATI uno sopra l'altro: oltre, non si salva niente. */
void pennello_salva(struct pennello *p);

/* Torna all'ultimo stato messo da parte con pennello_salva (se non ce n'e', niente). */
void pennello_ripristina(struct pennello *p);

/* Rende trasparente (0) tutta la zona del ritaglio: si comincia sempre cosi', perche' le
 * forme semitrasparenti si sommano a quello che c'e' gia' sotto. */
void pulisci(struct pennello *p);

/* --- le forme dritte --- */

/* Un rettangolo pieno, con l'angolo in (x, y), largo 'l' e alto 'a'. */
void riempi_rettangolo(struct pennello *p, int x, int y, int l, int a);

/* Il contorno di un rettangolo, spesso quanto il pennello, verso l'interno. */
void contorna_rettangolo(struct pennello *p, int x, int y, int l, int a);

/* Una linea dritta da (x1, y1) a (x2, y2), estremi compresi, spessa quanto il pennello.
 * Senza bordi lisci. */
void linea(struct pennello *p, int x1, int y1, int x2, int y2);

/* --- le forme arrotondate, con i bordi lisci --- */

/* Un rettangolo arrotondato, con un raggio per angolo (in alto a sinistra, in alto a
 * destra, in basso a destra, in basso a sinistra). Un raggio piu' grande di meta' del lato
 * piu' corto conta come quella meta'. */
struct tondo {
    int x, y, l, a;
    int r[4];
};

/* Un rettangolo arrotondato con lo stesso raggio ai quattro angoli. */
struct tondo tondo(int x, int y, int l, int a, int raggio);

/* La forma 't' piena. */
void riempi_forma(struct pennello *p, const struct tondo *t);

/* Un rettangolo arrotondato pieno: riempi_forma di tondo(x, y, l, a, raggio). */
void riempi_tondo(struct pennello *p, int x, int y, int l, int a, int raggio);

/* Il bordo interno della forma, spesso 'spessore' 256esimi di pixel (256 = 1 pixel,
 * 384 = 1,5): quello che e' dentro la forma ma non dentro la forma ristretta. Con spessore
 * 0 o meno, niente. */
void contorna_forma(struct pennello *p, const struct tondo *t, int spessore);

/* contorna_forma di tondo(x, y, l, a, raggio). */
void contorna_tondo(struct pennello *p, int x, int y, int l, int a, int raggio, int spessore);

/* --- le ombre --- */

/* Un'OMBRA morbida sotto la forma, nel colore del pennello, spostata di (dx, dy), sfocata di
 * 'sfocatura' pixel e DIFFUSA di 'diffusione' pixel (lo "spread" di Figma): positiva,
 * l'ombra e' piu' grande della forma da ogni lato; negativa, piu' piccola (le ombre
 * "sollevate" dei token usano -4 o -12: un'ombra piu' stretta della scheda, che si vede solo
 * sotto e non ai lati, come una luce alta). Si vede solo FUORI dalla forma: dentro c'e' la
 * forma stessa. Ogni pixel si calcola da solo dalla distanza dal bordo (niente sfocatura
 * vera a due passate, niente memoria): per un rettangolo arrotondato e' quasi
 * indistinguibile da una gaussiana vera. */
void ombra_forma_diffusa(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura,
                         int diffusione);

/* L'ombra di sempre: ombra_forma_diffusa senza diffusione. */
void ombra_forma(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura);

/* Un'OMBRA INTERNA ("Inner shadow" di Figma): dentro la forma, un'ombra che nasce dal
 * bordo, nel colore del pennello. Si immagina un BUCO uguale alla forma ma spostato di
 * (dx, dy) (e ristretto di 'diffusione' pixel: positiva, l'ombra entra di piu'): dove
 * il buco non arriva, la forma e' in ombra. Con dy = 1 il buco e' un pixel piu' in
 * basso, quindi lungo il bordo ALTO resta una striscia in ombra, e piu' sfocatura
 * c'e', piu' la striscia sfuma verso l'interno: e' l'effetto "incassato". Il bordo liscio
 * della forma non si sporca. */
void ombra_interna_diffusa(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura,
                           int diffusione);

/* ombra_interna_diffusa senza diffusione. */
void ombra_interna(struct pennello *p, const struct tondo *t, int dx, int dy, int sfocatura);

/* --- le ombre a strati --- */

/* Un'ombra vera di Figma e' fatta di piu' ombre sovrapposte (una vicina e netta, una
 * larga e leggera), e ognuna e' ESTERNA ("Drop shadow") o INTERNA ("Inner shadow").
 * Questa e' una riga della lista, uguale a una voce dei token (ombra/sollevato...):
 * 'colore' e' 0xAARRGGBB con l'opacita' gia' dentro (0.06 -> alfa 15), 'tipo' e'
 * OMBRA_ESTERNA o OMBRA_INTERNA, il resto e' in pixel. */
#define OMBRA_ESTERNA 0
#define OMBRA_INTERNA 1

struct strato_ombra {
    unsigned int colore;
    int tipo;
    int dx, dy, sfocatura, diffusione;
};

/* Quante righe ha una lista di strati scritta come array. */
#define OMBRA_STRATI(tabella)          ((int)(sizeof(tabella) / sizeof((tabella)[0])))

/* L'ORDINE. La lista si scrive come nei token (e come il pannello di Figma): la PRIMA
 * riga e' quella che sta SOPRA a tutte. Per disegnare si fa al contrario, dall'ultima
 * alla prima, cosi' la piu' grande e lontana (di solito l'ultima) va sotto e la piu'
 * piccola e vicina sopra. In tutto l'ordine e':
 *
 *     1. ombre_strati_fuori(...)   le esterne, dall'ultima alla prima
 *     2. il chiamante disegna il FONDO della forma (riempi_forma, vetro, ecc.)
 *     3. ombre_strati_dentro(...)  le interne, dall'ultima alla prima
 *     4. contorni, luce, testo
 *
 * Il fondo sta in mezzo perche' le ombre esterne devono stare DIETRO e le interne
 * DAVANTI al fondo. (Le esterne non vengono disegnate sotto la forma, come in Figma
 * per le forme con il fondo semitrasparente: vedi ombra_forma_diffusa.) Ognuna usa il
 * colore della sua riga; il pennello alla fine e' com'era prima. */

/* Le ombre esterne degli 'n' strati, dall'ultimo al primo. */
void ombre_strati_fuori(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati,
                        int n);

/* Le ombre interne degli 'n' strati, dall'ultimo al primo. */
void ombre_strati_dentro(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati,
                         int n);

/* Tutte e due di seguito, per quando il fondo c'e' gia' (o non serve): le esterne
 * non toccano l'interno della forma, quindi non sporcano un fondo gia' disegnato, se non
 * di un soffio sul pixel del bordo. */
void ombre_strati(struct pennello *p, const struct tondo *t, const struct strato_ombra *strati, int n);

/* --- cerchi e pillole --- */

/* Una PILLOLA: un rettangolo con gli angoli tondi quanto possono, cioe' di meta'
 * altezza (i pulsanti e le barre di avanzamento). */
void riempi_pillola(struct pennello *p, int x, int y, int l, int a);

/* Un CERCHIO col centro in (cx, cy), che e' un angolo di pixel (non il centro di un
 * pixel): il cerchio e' simmetrico, con 'raggio' pixel da ogni parte. */
void riempi_cerchio(struct pennello *p, int cx, int cy, int raggio);

/* --- archi e anelli --- */

/* Un ARCO (o un anello intero): la parte di corona circolare tra 'raggio_interno' e
 * 'raggio_esterno' che va da 'angolo_inizio' a 'angolo_fine', in gradi INTERI, con 0 in
 * ALTO e in senso ORARIO (90 = a destra, 180 = in basso, 270 = a sinistra). Se la fine
 * e' minore dell'inizio l'arco gira oltre i 360 (da 350 a 10 sono 20 gradi); se la
 * differenza e' 360 o piu' e' l'anello intero. Il centro (cx, cy) e' un angolo di
 * pixel, come per riempi_cerchio. Spessore = esterno - interno. Il bordo e' liscio,
 * qualunque sia l'angolo.
 *
 * I TAPPI (le due estremita'): in riempi_arco sono TONDI, come il "round cap" di Figma e
 * dei cerchi di caricamento: a ogni estremita' c'e' una mezza pillola, e l'arco sporge di
 * mezzo spessore oltre gli angoli dati. Con riempi_arco_dritto i tappi sono tagliati
 * dritti, esattamente sugli angoli (per una barra circolare con l'inizio in un punto
 * preciso). */
void riempi_arco(struct pennello *p, int cx, int cy, int raggio_esterno, int raggio_interno,
                 int angolo_inizio, int angolo_fine);
void riempi_arco_dritto(struct pennello *p, int cx, int cy, int raggio_esterno, int raggio_interno,
                        int angolo_inizio, int angolo_fine);

/* Lo stesso arco con il centro (cx, cy) e i raggi 're' (esterno) e 'ri' (interno) in
 * 256esimi di pixel (con la virgola, per i cerchi piccoli e le animazioni); 'tappi_tondi'
 * 1 come riempi_arco, 0 come riempi_arco_dritto. */
void riempi_arco_fine(struct pennello *p, int cx, int cy, int re, int ri, int inizio, int fine,
                      int tappi_tondi);

/* Una riga di luce sul bordo alto, all'interno: quello che e' dentro la forma ma non
 * dentro la forma abbassata di un pixel. Fa sembrare il vetro "in rilievo". */
void luce_sopra(struct pennello *p, const struct tondo *t);

/* --- il testo --- */

/* La lettera del font per il carattere 'c' (code page 437); se il font non ce l'ha, quella
 * del '?'. Non e' mai 0. */
const struct glifo *glifo_di(const struct font *f, unsigned char c);

/* Quanto e' largo un testo, in pixel: serve per centrarlo o allinearlo a destra. */
int larghezza_testo(const struct font *f, const char *s);

/* Scrive 's' (code page 437) con la cima della riga in (x, y), nel colore del pennello.
 * Restituisce dove finisce (la x dopo l'ultima lettera). */
int testo(struct pennello *p, const struct font *f, int x, int y, const char *s);

/* Lo stesso, con la SPAZIATURA TRA LE LETTERE (il "letterSpacing" di Figma): 'spaziatura' e'
 * in centesimi di pixel (24 = un quarto di pixel in piu' dopo ogni lettera, -24 = in meno;
 * l'ultima compresa, come in Figma). Le lettere restano intere: si sposta solo il punto
 * dove cade ciascuna, arrotondato al pixel. Con 0 e' identico a testo / larghezza_testo. */
int larghezza_testo_spaziato(const struct font *f, const char *s, int spaziatura);
int testo_spaziato(struct pennello *p, const struct font *f, int x, int y, const char *s,
                   int spaziatura);

/* --- le icone e le immagini --- */

/* L'icona 'misure' (le sue 'quante' misure, dalla piu' piccola) di lato 'lato' pixel, con
 * l'angolo in (x, y), nel colore del pennello. Se quella misura non c'e', si usa la piu'
 * grande che ci sta (o la piu' piccola), centrata. Le icone del sistema per numero: icona(),
 * in uikit/materiali.h. */
void disegna_icona(struct pennello *p, const struct icona *misure, int quante, int lato, int x, int y);

/* Un'immagine a colori (gia' premoltiplicata) con l'angolo in (x, y). Il colore del pennello
 * non conta; l'opacita' e il ritaglio si'. */
void disegna_immagine(struct pennello *p, const struct immagine *im, int x, int y);

/* Un'immagine a colori ritagliata a CERCHIO (bordo sfumato), centrata nel quadrato di lato
 * 'lato' con l'angolo in (x, y). Il cerchio e' quello dell'immagine intera (il piu' piccolo
 * dei suoi lati), al centro del quadrato: se l'immagine e' piu' piccola del quadrato, il
 * cerchio e' piu' piccolo e resta centrato. L'immagine non si ingrandisce mai. */
void disegna_immagine_tonda(struct pennello *p, const struct immagine *im, int x, int y, int lato);

/* Un'immagine a colori con gli angoli arrotondati di 'raggio' (bordo liscio), con l'angolo in
 * (x, y): come disegna_immagine, ma fuori dal rettangolo arrotondato non si disegna. */
void disegna_immagine_arrotondata(struct pennello *p, const struct immagine *im, int x, int y,
                                  int raggio);

/* --- rimpicciolire un'immagine --- */

/* L'immagine 'da' rimpicciolita per RIEMPIRE l x a pixel senza deformarla (come "cover" sul
 * web): la si scala finche' copre tutto il rettangolo e si taglia quello che avanza, in parti
 * uguali dai due lati, tenendo il centro. Ogni pixel nuovo e' la media dei pixel di 'da' che
 * copre, pesati per quanto ne copre: rimpicciolendo non si perde niente e non escono scalini.
 * I pixel restano premoltiplicati. Scrive in 'pixel' (l * a, di chi chiama) e torna
 * l'immagine che li usa. Se 'da' e' vuota (0 x 0: il materiale manca) i pixel sono
 * trasparenti. E' fatta per rimpicciolire: se 'da' e' piu' piccola, ogni suo pixel diventa un
 * quadretto. */
struct immagine immagine_rimpicciolita(const struct immagine *da, unsigned int *pixel, int l, int a);

#endif
