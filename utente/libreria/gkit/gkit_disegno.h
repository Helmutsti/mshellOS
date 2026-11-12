/* GKit, gli strumenti di disegno (vedi LIBRERIE.md): come Cairo su Linux o Core Graphics
 * sul Mac. Disegna su una TELA (un pezzo di memoria con dei pixel, per esempio l'immagine
 * di una finestra) e non sa niente di finestre o di compositore. Sa COME si disegna, senza
 * nessun gusto: non conosce i colori del tema, le misure del design, il font e le icone del
 * sistema (quelli sono di UIKit, in uikit/).
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
 * IL TESTO, LE ICONE E LE IMMAGINI si ricevono da chi disegna: un font, un'icona o
 * un'immagine sono tabelle gia' calcolate (struct font, struct icona, struct immagine, qui
 * sotto), e qui si colorano e si mettono sulla tela. Quelli del sistema (i font IBM Plex e
 * Inter, le icone Lucide, le icone dei file) sono di UIKit: uikit/materiali.h.
 *
 * Coordinate in pixel, (0, 0) in alto a sinistra, y verso il basso. */
#ifndef GKIT_DISEGNO_H
#define GKIT_DISEGNO_H

/* --- i materiali che si ricevono: font, icone, immagini ---
 * I tipi soltanto: i dati veri li prepara strumenti/grafica/genera.py (quelli del sistema
 * stanno in uikit/materiali_dati.h). */

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

/* UN'IMMAGINE DA UN FILE (gkit/immagine_file.c): per ora solo PNG, decodificate da
 * stb_image (esterni/stb). Si legge com'e', senza ingrandirla o rimpicciolirla. 0 se fatto,
 * con i pixel in memoria nuova (si liberano con immagine_libera); -1 se il file manca o non
 * e' una PNG, e allora 'im' resta vuota. (Solo per i programmi di mshell: nell'anteprima
 * sul Mac non c'e'.) */
int immagine_carica(const char *percorso, struct immagine *im);
void immagine_libera(struct immagine *im);

#define OPACO(rgb) (0xFF000000u | (unsigned int)(rgb))
/* Un colore con opacita' da 0 a 255: VELATO(0xFFFFFF, 36) e' il bianco al 14%. */
#define VELATO(rgb, alfa) ((unsigned int)(alfa) << 24 | (unsigned int)(rgb))

struct tela {
    unsigned int *pixel;       /* 0xAARRGGBB premoltiplicati, riga dopo riga */
    int larghezza, altezza;
};

/* Lo stato che si salva e si ripristina. */
struct stato_pennello {
    unsigned int colore;
    int opacita;               /* 0-255: moltiplica tutto quello che si disegna */
    int spessore;              /* per linee e contorni dritti */
    int rx, ry, rx2, ry2;      /* il ritaglio: si disegna solo con rx <= x < rx2, ry <= y < ry2 */
};

#define MAX_SALVATI 16

struct pennello {
    struct tela *tela;
    struct stato_pennello s;
    struct stato_pennello salvati[MAX_SALVATI];
    int quanti_salvati;
};

/* --- il pennello --- */

/* Nero pieno, opacita' piena, spessore 1, ritaglio su tutta la tela. */
static inline void pennello_prepara(struct pennello *p, struct tela *t) {
    p->tela = t;
    p->s = (struct stato_pennello){ .colore = OPACO(0), .opacita = 255, .spessore = 1,
                                    .rx = 0, .ry = 0, .rx2 = t->larghezza, .ry2 = t->altezza };
    p->quanti_salvati = 0;
}

static inline void pennello_colore(struct pennello *p, unsigned int argb) { p->s.colore = argb; }

/* Da adesso tutto e' anche piu' trasparente: si moltiplica per 'opacita' (0-255). Serve
 * per i widget disabilitati (al 40%), che restano visibili ma "spenti". */
static inline void pennello_opacita(struct pennello *p, int opacita) {
    p->s.opacita = p->s.opacita * opacita / 255;
}

static inline void pennello_spessore(struct pennello *p, int spessore) {
    p->s.spessore = spessore < 1 ? 1 : spessore;
}

/* Restringe il ritaglio a questo rettangolo (dentro quello che c'era gia'). */
static inline void pennello_ritaglia(struct pennello *p, int x, int y, int l, int a) {
    if (x > p->s.rx) p->s.rx = x;
    if (y > p->s.ry) p->s.ry = y;
    if (x + l < p->s.rx2) p->s.rx2 = x + l;
    if (y + a < p->s.ry2) p->s.ry2 = y + a;
}

static inline void pennello_salva(struct pennello *p) {
    if (p->quanti_salvati < MAX_SALVATI) p->salvati[p->quanti_salvati++] = p->s;
}

static inline void pennello_ripristina(struct pennello *p) {
    if (p->quanti_salvati > 0) p->s = p->salvati[--p->quanti_salvati];
}

/* --- mettere un colore su un pixel --- */

/* x / 255, arrotondato, senza dividere */
static inline unsigned int diviso_255(unsigned int x) { return (x + 128 + ((x + 128) >> 8)) >> 8; }

/* Un pixel premoltiplicato 'sopra' messo su 'sotto'. */
static inline unsigned int sopra_a(unsigned int sopra, unsigned int sotto) {
    unsigned int resto = 255 - (sopra >> 24);
    if (resto == 255) return sotto;
    if (resto == 0) return sopra;
    unsigned int r = 0;
    for (int b = 0; b < 32; b += 8)
        r |= (((sopra >> b) & 0xFF) + diviso_255(((sotto >> b) & 0xFF) * resto)) << b;
    return r;
}

/* Il colore del pennello, coperto per 'copertura' (0-256), sul pixel (x, y). TUTTO il
 * disegno passa da qui: ritaglio, opacita', trasparenza. */
static inline void copri(struct pennello *p, int x, int y, int copertura) {
    if (copertura <= 0 || x < p->s.rx || x >= p->s.rx2 || y < p->s.ry || y >= p->s.ry2) return;
    if (copertura > 256) copertura = 256;
    unsigned int c = p->s.colore;
    unsigned int a = ((c >> 24) * copertura >> 8) * (p->s.opacita + 1) >> 8;   /* 0-255 */
    if (!a) return;
    unsigned int premolt = a << 24 | diviso_255(((c >> 16) & 0xFF) * a) << 16 |
                           diviso_255(((c >> 8) & 0xFF) * a) << 8 | diviso_255((c & 0xFF) * a);
    unsigned int *d = &p->tela->pixel[y * p->tela->larghezza + x];
    *d = sopra_a(premolt, *d);
}

/* Tutta la zona del ritaglio trasparente: si comincia sempre cosi', perche' le forme
 * semitrasparenti si sommano a quello che c'e' gia' sotto. */
static inline void pulisci(struct pennello *p) {
    for (int y = p->s.ry; y < p->s.ry2; y++)
        for (int x = p->s.rx; x < p->s.rx2; x++)
            p->tela->pixel[y * p->tela->larghezza + x] = 0;
}

/* --- le forme dritte --- */

static inline void riempi_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    for (int r = y; r < y + a; r++)
        for (int k = x; k < x + l; k++) copri(p, k, r, 256);
}

/* Il contorno di un rettangolo, spesso quanto il pennello, verso l'interno. */
static inline void contorna_rettangolo(struct pennello *p, int x, int y, int l, int a) {
    int s = p->s.spessore;
    if (2 * s >= l || 2 * s >= a) { riempi_rettangolo(p, x, y, l, a); return; }
    riempi_rettangolo(p, x, y, l, s);
    riempi_rettangolo(p, x, y + a - s, l, s);
    riempi_rettangolo(p, x, y + s, s, a - 2 * s);
    riempi_rettangolo(p, x + l - s, y + s, s, a - 2 * s);
}

/* Una linea dritta da (x1, y1) a (x2, y2), estremi compresi (Bresenham, 1962): si
 * avanza di un pixel alla volta lungo la direzione piu' lunga, tenendo il conto di
 * quanto ci si allontana dalla linea vera nell'altra. Senza bordi lisci. */
static inline void linea(struct pennello *p, int x1, int y1, int x2, int y2) {
    int dx = x2 > x1 ? x2 - x1 : x1 - x2, passo_x = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2, passo_y = y1 < y2 ? 1 : -1;
    int errore = dx - dy;
    int s = p->s.spessore, meta = (s - 1) / 2;
    for (;;) {
        riempi_rettangolo(p, x1 - meta, y1 - meta, s, s);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * errore;
        if (e2 > -dy) { errore -= dy; x1 += passo_x; }
        if (e2 < dx)  { errore += dx; y1 += passo_y; }
    }
}

/* --- le forme arrotondate, con i bordi lisci --- */

/* La radice quadrata intera (cifra binaria per cifra binaria: solo spostamenti e somme). */
static inline unsigned int radice(unsigned long long n) {
    unsigned long long r = 0, bit = 1ULL << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (unsigned int)r;
}

/* Un rettangolo arrotondato, con un raggio per angolo (in alto a sinistra, in alto a
 * destra, in basso a destra, in basso a sinistra). */
struct tondo {
    int x, y, l, a;
    int r[4];
};

static inline struct tondo tondo(int x, int y, int l, int a, int raggio) {
    struct tondo t = { x, y, l, a, { raggio, raggio, raggio, raggio } };
    return t;
}

/* LA DISTANZA del centro del pixel (px, py) dal bordo della forma, in 256esimi di pixel:
 * negativa dentro, positiva fuori. Un rettangolo arrotondato e' un rettangolo piu'
 * piccolo "gonfiato" di r in tutte le direzioni: la distanza dal bordo e' la distanza
 * dal rettangolo piccolo, meno r. */
static inline int distanza(const struct tondo *t, int px, int py) {
    int cx = px * 256 + 128, cy = py * 256 + 128;
    int sinistra = cx < (t->x * 2 + t->l) * 128, sopra = cy < (t->y * 2 + t->a) * 128;
    int r = t->r[sopra ? (sinistra ? 0 : 1) : (sinistra ? 3 : 2)];
    int massimo = (t->l < t->a ? t->l : t->a) / 2;
    if (r > massimo) r = massimo;
    int x1 = (t->x + r) * 256, x2 = (t->x + t->l - r) * 256;
    int y1 = (t->y + r) * 256, y2 = (t->y + t->a - r) * 256;
    int dx = cx < x1 ? x1 - cx : cx > x2 ? cx - x2 : 0;
    int dy = cy < y1 ? y1 - cy : cy > y2 ? cy - y2 : 0;
    if (dx && dy) return (int)radice((unsigned long long)dx * dx + (unsigned long long)dy * dy) - r * 256;
    if (dx || dy) return dx + dy - r * 256;
    /* dentro il rettangolo piccolo: quanto manca al suo lato piu' vicino, piu' r */
    int vicino = cx - x1;
    if (x2 - cx < vicino) vicino = x2 - cx;
    if (cy - y1 < vicino) vicino = cy - y1;
    if (y2 - cy < vicino) vicino = y2 - cy;
    return -vicino - r * 256;
}

/* Quanto e' coperto un pixel a 'distanza' dal bordo (0-256): tutto a mezzo pixel
 * dentro, niente a mezzo pixel fuori, in mezzo a sfumare. */
static inline int coperto(int distanza) {
    int c = 128 - distanza;
    return c < 0 ? 0 : c > 256 ? 256 : c;
}

static inline void riempi_forma(struct pennello *p, const struct tondo *t) {
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++) copri(p, x, y, coperto(distanza(t, x, y)));
}

static inline void riempi_tondo(struct pennello *p, int x, int y, int l, int a, int raggio) {
    struct tondo t = tondo(x, y, l, a, raggio);
    riempi_forma(p, &t);
}

/* Il bordo interno della forma, spesso 'spessore' 256esimi di pixel (256 = 1 pixel,
 * 384 = 1,5): quello che e' dentro la forma ma non dentro la forma ristretta. */
static inline void contorna_forma(struct pennello *p, const struct tondo *t, int spessore) {
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++) {
            int d = distanza(t, x, y);
            copri(p, x, y, coperto(d) - coperto(d + spessore));
        }
}

static inline void contorna_tondo(struct pennello *p, int x, int y, int l, int a, int raggio,
                                  int spessore) {
    struct tondo t = tondo(x, y, l, a, raggio);
    contorna_forma(p, &t, spessore);
}

/* LA SFUMATURA di un'ombra. 'd' e' la distanza con segno (in 256esimi di pixel, come
 * 'distanza') dal bordo della forma che fa l'ombra; 's' e' la meta' della larghezza della
 * sfumatura, in 256esimi di pixel (almeno 256). Restituisce 0-256: 256 tutto dentro la
 * forma (a 's' o piu' dal bordo), 0 tutto fuori, e in mezzo una curva a S (smoothstep,
 * 3v^2 - 2v^3) che inizia e finisce PIANO: una rampa dritta farebbe vedere due "spigoli"
 * dove l'ombra comincia e dove finisce; con la curva a S l'occhio non trova il bordo,
 * che e' quello che fa sembrare una sfocatura vera (una gaussiana, in pratica). Sul
 * bordo esatto vale 128, cioe' meta'. */
static inline int sfuma(int d, int s) {
    int v = (s - d) * 128 / s;                       /* 0-256 lungo la sfumatura */
    if (v <= 0) return 0;
    if (v > 256) v = 256;
    return v * v * (768 - 2 * v) >> 16;
}

/* Un'OMBRA morbida sotto la forma, spostata di (dx, dy), sfocata di 'sfocatura' pixel e
 * DIFFUSA di 'diffusione' pixel (lo "spread" di Figma): positiva, l'ombra e' piu'
 * grande della forma da ogni lato; negativa, piu' piccola (le ombre "sollevate" dei
 * token usano -4 o -12: un'ombra piu' stretta della scheda, che si vede solo sotto e
 * non ai lati, come una luce alta). Si vede solo FUORI dalla forma: dentro c'e' la forma
 * stessa.
 *
 * COME. Si allarga (o si stringe) la forma di 'diffusione' sottraendo diffusione * 256
 * dalla sua distanza con segno: gonfiare un rettangolo arrotondato di d pixel da un
 * rettangolo arrotondato col raggio piu' grande di d, e la sua distanza dal bordo e'
 * esattamente quella di prima meno d. Poi si sfuma con 'sfuma'. Niente buffer, niente
 * sfocatura vera a due passate: ogni pixel dell'ombra si calcola da solo dalla
 * distanza, quindi non costa memoria (un programma di mshell non ha un heap comodo).
 * Il prezzo e' che l'ombra di una forma qualunque e' sempre "la forma, ammorbidita":
 * per un rettangolo arrotondato e' quasi indistinguibile da una gaussiana vera.
 *
 * Con diffusione 0 e' la vecchia ombra_forma, pixel per pixel. */
static inline void ombra_forma_diffusa(struct pennello *p, const struct tondo *t, int dx,
                                       int dy, int sfocatura, int diffusione) {
    struct tondo o = *t;
    o.x += dx;
    o.y += dy;
    int s = sfocatura * 256;
    if (s < 256) s = 256;
    int fuori_dal_bordo = sfocatura + (diffusione > 0 ? diffusione : 0);
    if (diffusione && sfocatura < 1) fuori_dal_bordo++;      /* la sfumatura di 1 px minimo */
    int y0 = o.y - fuori_dal_bordo, y1 = o.y + o.a + fuori_dal_bordo;
    int x0 = o.x - fuori_dal_bordo, x1 = o.x + o.l + fuori_dal_bordo;
    /* non si calcola quello che il ritaglio butterebbe via */
    if (y0 < p->s.ry) y0 = p->s.ry;
    if (y1 > p->s.ry2) y1 = p->s.ry2;
    if (x0 < p->s.rx) x0 = p->s.rx;
    if (x1 > p->s.rx2) x1 = p->s.rx2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int fuori = 256 - coperto(distanza(t, x, y));
            if (!fuori) continue;
            int dolce = sfuma(distanza(&o, x, y) - diffusione * 256, s);
            if (dolce) copri(p, x, y, dolce * fuori >> 8);
        }
}

/* L'ombra di sempre: nessuna diffusione. */
static inline void ombra_forma(struct pennello *p, const struct tondo *t, int dx, int dy,
                               int sfocatura) {
    ombra_forma_diffusa(p, t, dx, dy, sfocatura, 0);
}

/* Un'OMBRA INTERNA ("Inner shadow" di Figma): dentro la forma, un'ombra che nasce dal
 * bordo, nel colore del pennello. Si immagina un BUCO uguale alla forma ma spostato di
 * (dx, dy) (e ristretto di 'diffusione' pixel: positiva, l'ombra entra di piu'): dove
 * il buco non arriva, la forma e' in ombra. Con dy = 1 il buco e' un pixel piu' in
 * basso, quindi lungo il bordo ALTO resta una striscia in ombra, e piu' sfocatura
 * c'e', piu' la striscia sfuma verso l'interno: e' l'effetto "incassato".
 *
 * Ombra = 256 - sfuma(distanza dal buco): e' il contrario dell'ombra esterna (li' si
 * colorava dove la sfumatura vale, qui dove NON vale), e si moltiplica per quanto e'
 * coperto il pixel dalla forma vera: cosi' il bordo liscio (antialiasing) della forma
 * non viene sporcato e il ritaglio del pennello vale come sempre. */
static inline void ombra_interna_diffusa(struct pennello *p, const struct tondo *t, int dx,
                                         int dy, int sfocatura, int diffusione) {
    struct tondo buco = *t;
    buco.x += dx;
    buco.y += dy;
    int s = sfocatura * 256;
    if (s < 256) s = 256;
    int y0 = t->y, y1 = t->y + t->a, x0 = t->x, x1 = t->x + t->l;
    if (y0 < p->s.ry) y0 = p->s.ry;
    if (y1 > p->s.ry2) y1 = p->s.ry2;
    if (x0 < p->s.rx) x0 = p->s.rx;
    if (x1 > p->s.rx2) x1 = p->s.rx2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int dentro = coperto(distanza(t, x, y));
            if (!dentro) continue;
            int ombra = 256 - sfuma(distanza(&buco, x, y) + diffusione * 256, s);
            if (ombra) copri(p, x, y, ombra * dentro >> 8);
        }
}

static inline void ombra_interna(struct pennello *p, const struct tondo *t, int dx, int dy,
                                 int sfocatura) {
    ombra_interna_diffusa(p, t, dx, dy, sfocatura, 0);
}

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
 * per le forme con il fondo semitrasparente: vedi ombra_forma_diffusa.) */
static inline void ombre_strati_fuori(struct pennello *p, const struct tondo *t,
                                      const struct strato_ombra *strati, int n) {
    pennello_salva(p);
    for (int i = n - 1; i >= 0; i--) {
        if (strati[i].tipo != OMBRA_ESTERNA) continue;
        pennello_colore(p, strati[i].colore);
        ombra_forma_diffusa(p, t, strati[i].dx, strati[i].dy, strati[i].sfocatura,
                            strati[i].diffusione);
    }
    pennello_ripristina(p);
}

static inline void ombre_strati_dentro(struct pennello *p, const struct tondo *t,
                                       const struct strato_ombra *strati, int n) {
    pennello_salva(p);
    for (int i = n - 1; i >= 0; i--) {
        if (strati[i].tipo != OMBRA_INTERNA) continue;
        pennello_colore(p, strati[i].colore);
        ombra_interna_diffusa(p, t, strati[i].dx, strati[i].dy, strati[i].sfocatura,
                              strati[i].diffusione);
    }
    pennello_ripristina(p);
}

/* Tutte e due di seguito, per quando il fondo c'e' gia' (o non serve): le esterne
 * non toccano l'interno della forma, quindi non sporcano un fondo gia' disegnato, se non
 * di un soffio sul pixel del bordo. */
static inline void ombre_strati(struct pennello *p, const struct tondo *t,
                                const struct strato_ombra *strati, int n) {
    ombre_strati_fuori(p, t, strati, n);
    ombre_strati_dentro(p, t, strati, n);
}

/* --- cerchi e pillole --- */

/* Una PILLOLA: un rettangolo con gli angoli tondi quanto possono, cioe' di meta'
 * altezza (i pulsanti e le barre di avanzamento). */
static inline void riempi_pillola(struct pennello *p, int x, int y, int l, int a) {
    riempi_tondo(p, x, y, l, a, (l < a ? l : a) / 2);
}

/* Un CERCHIO col centro in (cx, cy), che e' un angolo di pixel (non il centro di un
 * pixel): il cerchio e' simmetrico, con 'raggio' pixel da ogni parte. */
static inline void riempi_cerchio(struct pennello *p, int cx, int cy, int raggio) {
    riempi_tondo(p, cx - raggio, cy - raggio, 2 * raggio, 2 * raggio, raggio);
}

/* --- archi e anelli --- */

/* Il seno di un angolo intero in gradi, moltiplicato per 4096 (punto fisso 4.12).
 * Basta la tabella del primo quarto (0-90 gradi): gli altri tre sono lo stesso
 * disegno specchiato. 91 numeri, nessuna libreria matematica. */
static const short tabella_seno[91] = {
    0, 71, 143, 214, 286, 357, 428, 499, 570, 641, 711, 782, 852,
    921, 991, 1060, 1129, 1198, 1266, 1334, 1401, 1468, 1534, 1600, 1666, 1731,
    1796, 1860, 1923, 1986, 2048, 2110, 2171, 2231, 2290, 2349, 2408, 2465, 2522,
    2578, 2633, 2687, 2741, 2793, 2845, 2896, 2946, 2996, 3044, 3091, 3138, 3183,
    3228, 3271, 3314, 3355, 3396, 3435, 3474, 3511, 3547, 3582, 3617, 3650, 3681,
    3712, 3742, 3770, 3798, 3824, 3849, 3873, 3896, 3917, 3937, 3956, 3974, 3991,
    4006, 4021, 4034, 4046, 4056, 4065, 4074, 4080, 4086, 4090, 4094, 4095, 4096,
};

static inline int seno_gradi(int g) {
    g %= 360;
    if (g < 0) g += 360;
    int quarto = g / 90, r = g % 90;
    int v = (quarto & 1) ? tabella_seno[90 - r] : tabella_seno[r];
    return quarto >= 2 ? -v : v;
}

/* Un ARCO (o un anello intero): la parte di corona circolare tra 'raggio_interno' e
 * 'raggio_esterno' che va da 'angolo_inizio' a 'angolo_fine', in gradi INTERI, con 0 in
 * ALTO e in senso ORARIO (90 = a destra, 180 = in basso, 270 = a sinistra). Se la fine
 * e' minore dell'inizio l'arco gira oltre i 360 (da 350 a 10 sono 20 gradi); se la
 * differenza e' 360 o piu' e' l'anello intero. Il centro (cx, cy) e' un angolo di
 * pixel, come per riempi_cerchio. Spessore = esterno - interno.
 *
 * COME, senza coseni e senza arctan. Per ogni pixel si calcolano due distanze con
 * segno (in 256esimi di pixel, come per le altre forme) e si prende la peggiore:
 *   - dalla CORONA: la distanza del pixel dal centro (con 'radice'), confrontata con i
 *     due raggi: serve il massimo tra "troppo lontano" e "troppo vicino";
 *   - dal SETTORE: l'arco e' una fetta di torta, cioe' l'incrocio (se la fetta e' al
 *     massimo mezzo giro) o l'unione (se e' piu' di mezzo giro) di due mezzi piani, uno
 *     per ogni lato dritto della fetta. La distanza del pixel da una retta per il
 *     centro con direzione u e' il prodotto vettoriale u x P (P = pixel meno centro),
 *     perche' u e' lungo 1. Le direzioni u vengono dalla tabella del seno.
 * Il bordo sfuma con 'coperto' e quindi non ha scalini, qualunque sia l'angolo.
 *
 * I TAPPI (le due estremita'): in 'riempi_arco' sono TONDI, come il "round cap" di
 * Figma e dei cerchi di caricamento: a ogni estremita' c'e' una mezza pillola, cioe'
 * un cerchio di diametro pari allo spessore centrato a meta' corona. Per questo
 * l'arco sporge di mezzo spessore oltre gli angoli dati. Con 'riempi_arco_dritto' i
 * tappi sono tagliati dritti, esattamente sugli angoli (per una barra circolare con
 * l'inizio in un punto preciso). 'riempi_arco_fine' e' lo stesso con tutto in 256esimi
 * di pixel (centro e raggi con la virgola, per i cerchi piccoli e le animazioni). */
static inline void riempi_arco_fine(struct pennello *p, int cx, int cy, int re, int ri,
                                    int inizio, int fine, int tappi_tondi) {
    if (re <= ri || re <= 0) return;
    if (ri < 0) ri = 0;
    int giro = fine - inizio;
    int intero = giro >= 360;
    if (!intero) {
        giro %= 360;
        if (giro < 0) giro += 360;
        if (giro == 0) return;
    }
    /* direzioni (seno, -coseno) delle due estremita', in 1/4096; y verso il basso */
    int u0x = seno_gradi(inizio), u0y = -seno_gradi(inizio + 90);
    int u1x = seno_gradi(fine), u1y = -seno_gradi(fine + 90);
    int meta = (re - ri) / 2, medio = (re + ri) / 2;
    int c0x = (int)(((long long)medio * u0x) >> 12), c0y = (int)(((long long)medio * u0y) >> 12);
    int c1x = (int)(((long long)medio * u1x) >> 12), c1y = (int)(((long long)medio * u1y) >> 12);
    int fuori = (re + 255) / 256 + 1;
    int y0 = (cy >> 8) - fuori, y1 = (cy >> 8) + fuori + 1;
    int x0 = (cx >> 8) - fuori, x1 = (cx >> 8) + fuori + 1;
    if (y0 < p->s.ry) y0 = p->s.ry;
    if (y1 > p->s.ry2) y1 = p->s.ry2;
    if (x0 < p->s.rx) x0 = p->s.rx;
    if (x1 > p->s.rx2) x1 = p->s.rx2;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            int px = x * 256 + 128 - cx, py = y * 256 + 128 - cy;
            int dist = (int)radice((unsigned long long)((long long)px * px + (long long)py * py));
            int d = dist - re;
            if (ri - dist > d) d = ri - dist;                    /* fuori dalla corona */
            if (!intero) {
                int sa = (int)(((long long)u0x * py - (long long)u0y * px) >> 12);   /* >0: dopo l'inizio */
                int sb = -(int)(((long long)u1x * py - (long long)u1y * px) >> 12);  /* >0: prima della fine */
                int ds = giro <= 180 ? -(sa < sb ? sa : sb) : -(sa > sb ? sa : sb);
                if (ds > d) d = ds;
                if (tappi_tondi && d > -128) {
                    int ax = px - c0x, ay = py - c0y, bx = px - c1x, by = py - c1y;
                    if (ax < 0) ax = -ax;
                    if (ay < 0) ay = -ay;
                    if (bx < 0) bx = -bx;
                    if (by < 0) by = -by;
                    if (ax < meta + 256 && ay < meta + 256) {
                        int dc = (int)radice((unsigned long long)ax * ax + (unsigned long long)ay * ay) - meta;
                        if (dc < d) d = dc;
                    }
                    if (bx < meta + 256 && by < meta + 256) {
                        int dc = (int)radice((unsigned long long)bx * bx + (unsigned long long)by * by) - meta;
                        if (dc < d) d = dc;
                    }
                }
            }
            copri(p, x, y, coperto(d));
        }
}

static inline void riempi_arco(struct pennello *p, int cx, int cy, int raggio_esterno,
                               int raggio_interno, int angolo_inizio, int angolo_fine) {
    riempi_arco_fine(p, cx * 256, cy * 256, raggio_esterno * 256, raggio_interno * 256,
                     angolo_inizio, angolo_fine, 1);
}

static inline void riempi_arco_dritto(struct pennello *p, int cx, int cy, int raggio_esterno,
                                      int raggio_interno, int angolo_inizio, int angolo_fine) {
    riempi_arco_fine(p, cx * 256, cy * 256, raggio_esterno * 256, raggio_interno * 256,
                     angolo_inizio, angolo_fine, 0);
}

/* Una riga di luce sul bordo alto, all'interno: quello che e' dentro la forma ma non
 * dentro la forma abbassata di un pixel. Fa sembrare il vetro "in rilievo". */
static inline void luce_sopra(struct pennello *p, const struct tondo *t) {
    struct tondo giu = *t;
    giu.y += 1;
    for (int y = t->y; y < t->y + t->a; y++)
        for (int x = t->x; x < t->x + t->l; x++)
            copri(p, x, y, coperto(distanza(t, x, y)) - coperto(distanza(&giu, x, y)));
}

/* --- il testo --- */

static inline const struct glifo *glifo_di(const struct font *f, unsigned char c) {
    unsigned char i = f->indice[c];
    if (!i && c != ' ') i = f->indice['?'];   /* un carattere che il font non ha */
    return &f->glifi[i];
}

/* Quanto e' largo un testo, in pixel: serve per centrarlo o allinearlo a destra. */
static inline int larghezza_testo(const struct font *f, const char *s) {
    int l = 0;
    for (; *s; s++) l += glifo_di(f, (unsigned char)*s)->avanzo;
    return l;
}

/* Scrive 's' con la cima della riga in (x, y), nel colore del pennello. Restituisce
 * dove finisce (la x dopo l'ultima lettera). */
static inline int testo(struct pennello *p, const struct font *f, int x, int y, const char *s) {
    int base = y + f->ascesa;
    for (; *s; s++) {
        const struct glifo *g = glifo_di(f, (unsigned char)*s);
        const unsigned char *alfa = f->alfa + g->inizio;
        int gx = x + g->x, gy = base - g->y;
        for (int r = 0; r < g->a; r++)
            for (int k = 0; k < g->l; k++) {
                int v = alfa[r * g->l + k];
                if (v) copri(p, gx + k, gy + r, v + (v >> 7));
            }
        x += g->avanzo;
    }
    return x;
}

/* --- le icone e le immagini --- */

/* L'icona 'misure' (le sue 'quante' misure, dalla piu' piccola) di lato 'lato' pixel, nel
 * colore del pennello. Se quella misura non c'e', si usa la piu' grande che ci sta (o la
 * piu' piccola), centrata. Le icone del sistema per numero: icona(), in uikit/materiali.h. */
static inline void disegna_icona(struct pennello *p, const struct icona *misure, int quante,
                                 int lato, int x, int y) {
    const struct icona *scelta = &misure[0];
    for (int i = 1; i < quante; i++)
        if (misure[i].lato <= lato) scelta = &misure[i];
    int l = scelta->lato;
    x += (lato - l) / 2;
    y += (lato - l) / 2;
    for (int r = 0; r < l; r++)
        for (int k = 0; k < l; k++) {
            int v = scelta->alfa[r * l + k];
            if (v) copri(p, x + k, y + r, v + (v >> 7));
        }
}

/* Un'immagine a colori (gia' premoltiplicata) con l'angolo in (x, y). */
static inline void disegna_immagine(struct pennello *p, const struct immagine *im, int x, int y) {
    unsigned int colore = p->s.colore;
    for (int r = 0; r < im->a; r++)
        for (int k = 0; k < im->l; k++) {
            unsigned int px = im->pixel[r * im->l + k];
            unsigned int a = px >> 24;
            if (!a) continue;
            /* si torna al colore non premoltiplicato, e lo si copre per la sua opacita' */
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri(p, x + k, y + r, a + (a >> 7));
        }
    p->s.colore = colore;
}

/* Un'immagine a colori ritagliata a CERCHIO (bordo sfumato), centrata nel quadrato di lato
 * 'lato' con l'angolo in (x, y). Il cerchio e' quello dell'immagine intera (il piu' piccolo
 * dei suoi lati), al centro del quadrato: se l'immagine e' piu' piccola del quadrato, il
 * cerchio e' piu' piccolo e resta centrato. L'immagine non si ingrandisce mai. */
static inline void disegna_immagine_tonda(struct pennello *p, const struct immagine *im, int x, int y,
                                          int lato) {
    int ix = x + (lato - im->l) / 2, iy = y + (lato - im->a) / 2;
    int d = im->l < im->a ? im->l : im->a;
    struct tondo t = tondo(ix + (im->l - d) / 2, iy + (im->a - d) / 2, d, d, d / 2);
    unsigned int colore = p->s.colore;
    for (int r = 0; r < im->a; r++)
        for (int k = 0; k < im->l; k++) {
            unsigned int px = im->pixel[r * im->l + k];
            unsigned int a = px >> 24;
            if (!a) continue;
            int m = coperto(distanza(&t, ix + k, iy + r));
            if (!m) continue;
            unsigned int rr = ((px >> 16) & 0xFF) * 255 / a, gg = ((px >> 8) & 0xFF) * 255 / a,
                         bb = (px & 0xFF) * 255 / a;
            p->s.colore = OPACO(rr << 16 | gg << 8 | bb);
            copri(p, ix + k, iy + r, (int)((a + (a >> 7)) * m >> 8));
        }
    p->s.colore = colore;
}

#endif
