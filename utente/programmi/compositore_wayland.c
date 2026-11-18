/* Il server Wayland del compositore (vedi compositore_wayland.h).
 *
 * COME FUNZIONA, per chi non conosce Wayland. Un programma si collega al socket e chiede alla
 * libreria (libwayland-server) i "globali" che il compositore offre. Ne usa tre:
 *   - wl_compositor: crea le SUPERFICI (wl_surface). Una superficie e' un rettangolo di pixel
 *     che il programma riempie: attach (questo buffer), damage (dove e' cambiato), frame
 *     (avvisami quando posso disegnare il prossimo), commit (adesso vale quello che ho detto);
 *   - wl_shm: i BUFFER in memoria condivisa. Il programma crea un foglio (memfd), lo manda con
 *     il suo descrittore, e il compositore lo legge. La libreria fa tutto da sola: a noi dice
 *     solo "ecco il buffer" (wl_shm_buffer_get). Formati: ARGB8888 e XRGB8888;
 *   - xdg_wm_base: dice che una superficie e' una FINESTRA (xdg_surface + xdg_toplevel), con
 *     titolo, e si accorda con il compositore sulla misura (configure / ack_configure).
 *
 * LA FINESTRA, DENTRO IL COMPOSITORE. Una finestra Wayland disegna solo il suo contenuto. Il
 * compositore ci mette intorno la cornice nostra (il vetro, la testata con il titolo, i
 * comandi): UIKit la disegna (cornice_disegna, uikit_compositore.h), identica a quella delle
 * finestre nostre. L'immagine che il compositore vede e' quindi
 *     cornice (40 di testata + vetro)  +  contenuto del programma sotto la testata
 * dentro un'immagine che e' memoria nostra (non un foglio condiviso): a ogni commit con un
 * buffer nuovo si COPIA il contenuto (e si rilascia subito il buffer: il programma lo puo'
 * riusare), si rifa' l'immagine e si dice al compositore (ponte_mostra). Spostare, portare
 * davanti, ridurre e chiudere con la X li fa il compositore sulla cornice (al_mouse).
 *
 * LA BARRA DEL TITOLO LA PUO' DISEGNARE IL PROGRAMMA (xdg-decoration, la decisione e' della
 * persona: come GNOME e il Mac). Se il programma non crea l'oggetto decoration, o chiede
 * server_side, e' tutto come sopra. Se chiede client_side la finestra NON ha la cornice nostra:
 * l'immagine e' il buffer del programma cosi' com'e', le coordinate del puntatore sono quelle del
 * buffer (nessuno spostamento della testata), e il compositore non gestisce piu' trascinamento e
 * comandi (ponte_senza_cornice): li chiede il programma, con xdg_toplevel.move (il numero di
 * serie e' quello del clic, che deve essere ancora premuto) e set_minimized, e con la chiusura
 * come sempre (close). Ombra, vetro e angoli sono quelli delle finestre nostre. La forma del
 * puntatore la dice il programma con cursor-shape (set_shape), e il compositore la mostra come
 * quella di MOSTRA_PUNTATORE delle finestre nostre.
 *
 * I PING. Il compositore, come per le finestre nostre, ogni secondo chiede "ci sei?"
 * (xdg_wm_base.ping) a chi ha una finestra, e dopo 3 secondi senza pong la giudica bloccata:
 * stessa regola e stessa barra di emergenza di compositore.c (controlla_risposte, ponte_pong).
 *
 * LA CHIUSURA e' sempre una richiesta: la X manda xdg_toplevel.close e il programma decide (se
 * chiude, il socket si chiude e la finestra sparisce).
 *
 * LE SOTTOSUPERFICI (wl_subcompositor v1, wl_subsurface). Le usano i programmi veri (foot ne ha bisogno per
 * avviarsi). Il contenuto della finestra (quello che sta sotto la testata) e' la superficie principale
 * con le sottosuperfici sopra e sotto, nelle loro posizioni, tagliato ai bordi della principale
 * (componi). Posizione e ordine valgono al commit del genitore; il contenuto di una sottosuperficie
 * "sincrona" (la prima volta lo e' sempre) vale col commit del genitore (cache), quello di una
 * "asincrona" col suo. Il puntatore va alla superficie che sta sotto (colpisci), con le coordinate di
 * quella, e rispetta wl_surface.set_input_region se e' una somma di rettangoli (se no, tutta). Vedi il
 * commento di 'struct superficie'. Prova: wayland_sottosuperfici (client wayland_sotto).
 *
 * IL PASSO 2 e' wl_seat: il puntatore e la tastiera (vedi "wl_seat", piu' sotto).
 *
 * COSA NON C'E' ANCORA: la scala (wl_surface.set_buffer_scale e set_buffer_transform si accettano ma si
 * disegna sempre a scala 1, senza ruotare), l'opacita' (set_opaque_region), i popup, il ridimensionamento fatto dalla persona, gli appunti
 * (data_device), la forma del cursore come immagine (wl_pointer.set_cursor: solo le forme per
 * nome di cursor-shape). */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>   /* BTN_LEFT, KEY_MAX */
#include <wayland-server.h>
#include "xdg-shell-server-protocol.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include "cursor-shape-v1-server-protocol.h"

#include "compositore_wayland.h"
#include "gkit/gkit_disegno.h"
#include "gkit/sistema/protocollo.h"   /* PING */
#include "gkit/sistema/pixel.h"
#include "uikit/sistema/uikit_compositore.h"

/* Dove si apre il socket: la cartella dei nomi del sistema (SISTEMA.md, "Le porte"), che e' in
 * memoria (/run) e dove init e i servizi hanno gia' i loro. Il nome dice che e' di prova. */
#define CARTELLA_SOCKET "/run/mshell"
#define NOME_SOCKET     "wayland-prova"

/* Piu' stretta di cosi' la cornice non ha posto per il titolo e i comandi. */
#define LARGHEZZA_MINIMA 160

static struct wl_display *display;

/* quanti pixel di scorrimento vale uno scatto della rotella (wl_pointer.axis; weston usa 10) */
#define PIXEL_PER_SCATTO 10

/* Una regione (di input): fino a MAX_RETT rettangoli sommati; 'tutta' vale per "tutta la superficie"
 * (nessuna regione, oppure una troppo complicata: con sottrazioni o con troppi rettangoli). */
#define MAX_RETT 16
struct area {
    int n, tutta;
    int r[MAX_RETT][4];                  /* x, y, larghezza, altezza */
};

/* Una SUPERFICIE: quello che il programma disegna. 'in_attesa': le richieste frame fatte dopo
 * l'ultimo commit; 'da_servire': quelle gia' confermate da un commit, che aspettano il prossimo
 * fotogramma del compositore.
 *
 * LE SOTTOSUPERFICI (wl_subcompositor). Una superficie puo' avere sopra o sotto di se' altre
 * superfici (le "sottosuperfici"), ognuna a una posizione sua, relativa al genitore. Servono ai
 * programmi veri (un video dentro una pagina, il cursore del testo, i menu a tendina). Il
 * contenuto che il compositore vede per una finestra e' l'insieme: la superficie principale con le
 * sue sottosuperfici, nell'ordine 'pila', tagliato ai bordi della principale (componi).
 *   - 'pila' e' l'ordine dal fondo alla cima: i figli e, al posto giusto, il 'segno' della superficie
 *     stessa. 'pila_att' e' l'ordine richiesto (place_above/below) che vale al commit del genitore.
 *   - lo stato di una sottosuperficie (contenuto, regione) e' "sincrono" (valore di partenza): un suo
 *     commit va nella cache ('cache_*') e vale quando il genitore applica il suo; se e' "asincrono"
 *     (set_desync) e nessun antenato e' sincrono, vale subito.
 *   - 'pix' e' il contenuto valido (premoltiplicato, l x a), 0 se la superficie non e' mappata. */
struct finestra_wl;
struct superficie {
    struct wl_resource *risorsa;
    struct wl_resource *buffer;          /* l'ultimo attach, finche' non c'e' il commit */
    struct wl_listener ascolto_buffer;   /* se il programma distrugge il buffer prima del commit */
    int ha_attach;                       /* c'e' stato un attach dall'ultimo commit */
    struct wl_list in_attesa, da_servire;
    struct wl_list link;
    struct finestra_wl *finestra;        /* il suo ruolo (xdg_surface), o 0 */
    struct area input_attesa;            /* set_input_region dall'ultimo commit (se ha_input) */
    int ha_input;
    unsigned int *pix;                   /* il contenuto valido */
    int l, a;
    struct area input;                   /* dove si clicca */
    /* il ruolo di sottosuperficie */
    int e_sotto;                         /* ha (o ha avuto) il ruolo wl_subsurface */
    struct wl_resource *sotto;           /* il wl_subsurface, o 0 */
    struct superficie *genitore;         /* 0: la principale, o una sottosuperficie rimasta senza genitore */
    int x, y, px, py;                    /* posizione valida e in attesa, sul genitore */
    int sync;                            /* set_sync / set_desync */
    struct wl_list fratello, fratello_att;   /* nella pila (valida / in attesa) del genitore */
    struct wl_list pila, pila_att, segno, segno_att;
    /* lo stato di un commit non ancora valido (sincrono: aspetta il genitore) */
    int cache;
    int cache_contenuto, cache_ha_input;
    unsigned int *cache_pix;
    int cache_l, cache_a;
    struct area cache_input;
    struct wl_list in_cache;             /* i frame callback del commit nella cache */
};
static struct wl_list superfici;

/* Una FINESTRA (xdg_surface): nasce con get_xdg_surface, diventa finestra del compositore
 * quando ha il ruolo di toplevel (slot >= 0) e il primo contenuto. */
struct finestra_wl {
    struct superficie *superficie;
    struct wl_resource *xdg_surface, *toplevel;
    int slot;                            /* il posto nel compositore, o -1 */
    char titolo[MAX_TITOLO];
    unsigned int serial;                 /* l'ultimo configure mandato */
    int configurata;                     /* il primo configure e' partito */
    int attiva;                          /* ha il fuoco */
    unsigned int *contenuto;             /* l'ultimo contenuto del programma, cl x ch */
    int cl, ch;
    unsigned int *immagine;              /* cornice + contenuto: l x a */
    int l, a;
    struct wl_client *cliente;           /* il programma: a lui vanno i wl_pointer e wl_keyboard */
    struct superficie *focus;            /* la superficie (principale o sotto) che il puntatore ha davanti: enter/leave sono sue */
    /* chi disegna la barra del titolo (xdg-decoration): 'decorazione' e' l'oggetto, se il programma
     * l'ha creato; 'csd_voluto' cio' che ha chiesto (client_side), 'csd' cio' che vale per
     * l'immagine che c'e' (si adegua al primo buffer dopo la richiesta) */
    struct wl_resource *decorazione;
    int csd_voluto, csd;
    unsigned int ping_serial;            /* il ping mandato, di cui si aspetta il pong (0: nessuno) */
    uint32_t serial_clic;                /* il numero di serie dell'ultimo clic mandato (per move) */
    uint32_t serial_entra;               /* quello dell'ultimo enter del puntatore (per set_shape) */
    /* il puntatore (vedi "wl_seat", piu' sotto) */
    int dentro;                          /* il puntatore e' entrato in una superficie (wl_pointer.enter): 'focus' */
    int px, py;                          /* l'ultima posizione detta al programma, sulla superficie */
    unsigned int visti;                  /* i pulsanti (PULSANTE_...) tenuti giu' all'ultima notizia */
    unsigned int consegnati;             /* quelli premuti dentro la superficie, di cui il programma sa */
    unsigned int ignorati;               /* quelli premuti fuori (sulla cornice): il programma non li vede */
};
/* chi e' di Wayland in ogni posto del compositore: cresce con i posti (a raddoppio, a zero) */
static struct finestra_wl **per_posto;
static int posti_wl;

static struct finestra_wl *al_posto(int posto) {
    return posto >= 0 && posto < posti_wl ? per_posto[posto] : 0;
}

/* Segna chi e' al posto; 0 se la tabella non cresce (memoria finita). Togliere (w = 0) non cresce mai. */
static int segna_posto(int posto, struct finestra_wl *w) {
    if (posto >= posti_wl) {
        if (!w) return 1;
        int nuovi = posti_wl ? posti_wl : 8;
        while (nuovi <= posto) nuovi *= 2;
        struct finestra_wl **t = realloc(per_posto, (size_t)nuovi * sizeof *t);
        if (!t) return 0;
        memset(&t[posti_wl], 0, (size_t)(nuovi - posti_wl) * sizeof *t);
        per_posto = t;
        posti_wl = nuovi;
    }
    per_posto[posto] = w;
    return 1;
}

/* --- l'immagine della finestra --- */

/* Un pixel premoltiplicato per 'c' (0-256: quanto sta dentro la forma). */
static unsigned int coperto_di(unsigned int p, int c) {
    if (c >= 256) return p;
    unsigned int r = 0;
    for (int b = 0; b < 32; b += 8) r |= ((((p >> b) & 0xFF) * (unsigned int)c) >> 8) << b;
    return r;
}

/* Rifa' l'immagine (cornice e contenuto) e la da' al compositore. La misura e' quella del
 * contenuto piu' la testata: se il programma cambia misura, cambia anche la finestra. */
static void ridisegna(struct finestra_wl *w) {
    if (w->slot < 0 || !w->contenuto) return;
    int csd = w->csd;       /* la barra e' del programma: niente cornice, l'immagine e' il suo buffer */
    int alta = csd ? 0 : cornice_testata_altezza(), schermo_l, schermo_a;
    ponte_schermo(&schermo_l, &schermo_a);
    int l = csd || w->cl > LARGHEZZA_MINIMA ? w->cl : LARGHEZZA_MINIMA, a = alta + w->ch;
    if (l > schermo_l) l = schermo_l;      /* piu' grande dello schermo: si taglia */
    if (a > schermo_a) a = schermo_a;
    if (!w->immagine || w->l != l || w->a != a) {
        free(w->immagine);
        w->immagine = malloc((size_t)l * (size_t)a * sizeof(unsigned int));
        w->l = l;
        w->a = a;
        if (!w->immagine) return;
    }
    ponte_senza_cornice(w->slot, csd);
    if (csd) {
        /* senza cornice: l'immagine e' il buffer del programma, com'e' (gli angoli, se li vuole
         * arrotondati, li fa lui: trasparenti) */
        memcpy(w->immagine, w->contenuto, (size_t)l * (size_t)a * sizeof(unsigned int));
    } else {
        struct tela tela = { w->immagine, l, a };
        struct pennello pennello;
        pennello_prepara(&pennello, &tela);
        cornice_disegna(&pennello, l, a, w->titolo, w->attiva);
        /* il contenuto, sul vetro, sotto la testata (un ARGB8888 trasparente lascia vedere il vetro) */
        int righe = w->ch < a - alta ? w->ch : a - alta, colonne = w->cl < l ? w->cl : l;
        for (int r = 0; r < righe; r++)
            sopra_a_riga(&w->immagine[(alta + r) * l], &w->contenuto[r * w->cl], colonne);
        /* gli angoli in basso sono arrotondati: il contenuto non deve uscirne */
        int raggio = cornice_raggio();
        struct tondo forma = tondo(0, 0, l, a, raggio);
        for (int y = a - raggio; y < a; y++) {
            if (y < alta) continue;
            for (int x = 0; x < raggio && x < l; x++) w->immagine[y * l + x] = coperto_di(w->immagine[y * l + x], coperto(distanza(&forma, x, y)));
            for (int x = l - raggio > raggio ? l - raggio : raggio; x < l; x++)
                w->immagine[y * l + x] = coperto_di(w->immagine[y * l + x], coperto(distanza(&forma, x, y)));
        }
    }
    if (ponte_mostra(w->slot, w->immagine, l, a, w->titolo) < 0) {
        /* memoria finita: il compositore ha tolto la finestra. Il posto non e' piu' nostro (se
         * no un'altra finestra lo riuserebbe e ponte_togli toglierebbe lei); si chiede al
         * programma di chiudersi. */
        segna_posto(w->slot, 0);
        w->slot = -1;
        if (w->toplevel) xdg_toplevel_send_close(w->toplevel);
    }
}

/* Il contenuto di un buffer shm: una copia nuova, premoltiplicata (l x a), in *pix. 1 se fatto, 0 se il
 * buffer non va (non e' shm, misura vuota o piu' grande dello schermo, memoria finita).
 * Il buffer si legge dentro wl_shm_buffer_begin_access: se il programma accorcia il foglio
 * mentre si legge, la libreria lo evita (niente SIGBUS). */
static int copia_buffer(struct wl_resource *buffer, unsigned int **pix, int *pl, int *pa) {
    struct wl_shm_buffer *shm = wl_shm_buffer_get(buffer);
    if (!shm) return 0;
    uint32_t formato = wl_shm_buffer_get_format(shm);
    int l = wl_shm_buffer_get_width(shm), a = wl_shm_buffer_get_height(shm);
    int passo = wl_shm_buffer_get_stride(shm);
    int schermo_l, schermo_a;
    ponte_schermo(&schermo_l, &schermo_a);
    if (l < 1 || a < 1 || l > schermo_l || a > schermo_a) return 0;
    unsigned int *nuovo = malloc((size_t)l * (size_t)a * sizeof(unsigned int));
    if (!nuovo) return 0;
    unsigned int opaco = formato == WL_SHM_FORMAT_XRGB8888 ? 0xFF000000u : 0;   /* ARGB: ha il suo */
    wl_shm_buffer_begin_access(shm);
    const unsigned char *dati = wl_shm_buffer_get_data(shm);
    for (int r = 0; r < a; r++) {
        const unsigned int *riga = (const unsigned int *)(const void *)(dati + (size_t)r * (size_t)passo);
        for (int c = 0; c < l; c++) nuovo[r * l + c] = riga[c] | opaco;
    }
    wl_shm_buffer_end_access(shm);
    *pix = nuovo;
    *pl = l;
    *pa = a;
    return 1;
}

/* --- le sottosuperfici: l'albero, lo stato che vale, la composizione --- */

/* Oltre questa profondita' (sottosuperfici di sottosuperfici...) non si disegna ne' si cerca. */
#define PROFONDITA_MAX 32
/* Una posizione piu' lontana di cosi' si taglia (le somme lungo l'albero non vanno in overflow). */
#define POSIZIONE_MAX 100000

/* La principale: la cima dell'albero. (Il nome 'radice' e' gia' la radice quadrata di pixel.h.) */
static struct superficie *capostipite(struct superficie *s) {
    for (int n = 0; s->genitore && n < 1000; n++) s = s->genitore;
    return s;
}

/* Il commit di s e' "sincrono"? Lo e' se lo e' lei o un antenato (finche' si e' sottosuperfici). */
static int sync_effettivo(const struct superficie *s) {
    for (int n = 0; s && s->e_sotto && n < 1000; s = s->genitore, n++)
        if (s->sync) return 1;
    return 0;
}

/* Dove sta s sulla finestra (somma delle posizioni valide lungo gli antenati). */
static void origine(const struct superficie *s, int *ox, int *oy) {
    int x = 0, y = 0;
    for (int n = 0; s->genitore && n < 1000; s = s->genitore, n++) { x += s->x; y += s->y; }
    *ox = x;
    *oy = y;
}

/* Il punto (x, y) della superficie e' dentro la regione? */
static int nell_area(const struct area *r, int x, int y) {
    if (r->tutta) return 1;
    for (int i = 0; i < r->n; i++)
        if (x >= r->r[i][0] && y >= r->r[i][1] && x < r->r[i][0] + r->r[i][2] && y < r->r[i][1] + r->r[i][3]) return 1;
    return 0;
}

/* Il contenuto in cache diventa quello valido: pixel, regione, frame callback. 1 se i pixel sono cambiati. */
static int applica_stato(struct superficie *s) {
    if (!s->cache) return 0;
    s->cache = 0;
    int cambiato = s->cache_contenuto;
    if (s->cache_contenuto) {
        free(s->pix);
        s->pix = s->cache_pix;
        s->l = s->cache_l;
        s->a = s->cache_a;
        s->cache_pix = 0;
        s->cache_contenuto = 0;
    }
    if (s->cache_ha_input) { s->input = s->cache_input; s->cache_ha_input = 0; }
    wl_list_insert_list(s->da_servire.prev, &s->in_cache);
    wl_list_init(&s->in_cache);
    return cambiato;
}

/* Lo stato di s vale. E vale anche cio' che il genitore applica ai figli: la posizione, l'ordine, e lo
 * stato in cache dei figli (a cascata). 1 se e' cambiato qualcosa che si vede. */
static int applica(struct superficie *s, int prof) {
    int cambiato = applica_stato(s);
    if (prof > PROFONDITA_MAX) return cambiato;
    for (struct wl_list *p = s->pila.next; p != &s->pila; p = p->next) {
        if (p == &s->segno) continue;
        struct superficie *c = wl_container_of(p, c, fratello);
        if (c->x != c->px || c->y != c->py) cambiato = 1;
        c->x = c->px;
        c->y = c->py;
    }
    /* l'ordine in attesa diventa quello valido: ogni nodo, nell'ordine in attesa, va in fondo alla pila */
    struct wl_list *cursore = s->pila.next;
    for (struct wl_list *q = s->pila_att.next; q != &s->pila_att; q = q->next) {
        struct wl_list *vero = &s->segno;
        if (q != &s->segno_att) {
            struct superficie *c = wl_container_of(q, c, fratello_att);
            vero = &c->fratello;
        }
        if (vero != cursore) cambiato = 1;
        else cursore = cursore->next;
        wl_list_remove(vero);
        wl_list_insert(s->pila.prev, vero);
    }
    for (struct wl_list *p = s->pila.next; p != &s->pila; p = p->next) {
        if (p == &s->segno) continue;
        struct superficie *c = wl_container_of(p, c, fratello);
        if (c->cache && applica(c, prof + 1)) cambiato = 1;
    }
    return cambiato;
}

/* Disegna s e i suoi figli (nell'ordine della pila) su 'dest' (dl x da), con s in (ox, oy): quello che esce
 * dai bordi si taglia. Il mescolamento e' quello del resto: premoltiplicato. Una sottosuperficie senza
 * contenuto non c'e', e con lei i suoi figli. */
static void componi_albero(struct superficie *s, int ox, int oy, int prof, unsigned int *dest, int dl, int da) {
    if (prof > PROFONDITA_MAX) return;
    for (struct wl_list *p = s->pila.next; p != &s->pila; p = p->next) {
        if (p == &s->segno) {
            int x0 = ox < 0 ? 0 : ox, x1 = ox + s->l > dl ? dl : ox + s->l;
            if (!s->pix || x0 >= x1) continue;
            for (int r = 0; r < s->a; r++) {
                int y = oy + r;
                if (y < 0 || y >= da) continue;
                sopra_a_riga(&dest[y * dl + x0], &s->pix[r * s->l + (x0 - ox)], x1 - x0);
            }
        } else {
            struct superficie *c = wl_container_of(p, c, fratello);
            if (c->pix) componi_albero(c, ox + c->x, oy + c->y, prof + 1, dest, dl, da);
        }
    }
}

/* L'immagine del contenuto della finestra (w->contenuto, cl x ch): la principale con le sottosuperfici. 1 se fatta. */
static int componi(struct finestra_wl *w) {
    struct superficie *s = w->superficie;
    if (!s || !s->pix) return 0;
    if (w->cl != s->l || w->ch != s->a || !w->contenuto) {
        free(w->contenuto);
        w->contenuto = malloc((size_t)s->l * (size_t)s->a * sizeof(unsigned int));
        w->cl = s->l;
        w->ch = s->a;
        if (!w->contenuto) return 0;
    }
    memset(w->contenuto, 0, (size_t)s->l * (size_t)s->a * sizeof(unsigned int));
    componi_albero(s, 0, 0, 0, w->contenuto, s->l, s->a);
    return 1;
}

/* Qualcosa che si vede e' cambiato in s o sotto di lei: si rifa' l'immagine della finestra a cui appartiene. */
static void ricompone(struct superficie *s) {
    struct finestra_wl *w = capostipite(s)->finestra;
    if (w && w->slot >= 0 && w->configurata && componi(w)) ridisegna(w);
}

/* Quale superficie sta sotto il punto (px, py) della finestra: la piu' in alto che e' mappata e ha l'input
 * li' (senza regione, tutta). In *sx, *sy il punto sulla superficie trovata; 0 se nessuna. */
static struct superficie *colpisci(struct superficie *s, int ox, int oy, int px, int py, int prof, int *sx, int *sy) {
    if (prof > PROFONDITA_MAX) return 0;
    for (struct wl_list *p = s->pila.prev; p != &s->pila; p = p->prev) {
        if (p == &s->segno) {
            int lx = px - ox, ly = py - oy;
            if (s->pix && lx >= 0 && ly >= 0 && lx < s->l && ly < s->a && nell_area(&s->input, lx, ly)) {
                *sx = lx;
                *sy = ly;
                return s;
            }
        } else {
            struct superficie *c = wl_container_of(p, c, fratello);
            if (!c->pix) continue;
            struct superficie *r = colpisci(c, ox + c->x, oy + c->y, px, py, prof + 1, sx, sy);
            if (r) return r;
        }
    }
    return 0;
}

/* --- xdg_toplevel --- */

/* Un configure: la misura la decide il programma (0 x 0), e se la finestra ha il fuoco lo
 * stato "attivo". Il programma risponde con ack_configure. */
static void invia_configure(struct finestra_wl *w) {
    struct wl_array stati;
    wl_array_init(&stati);
    if (w->attiva) {
        uint32_t *s = wl_array_add(&stati, sizeof *s);
        if (s) *s = XDG_TOPLEVEL_STATE_ACTIVATED;
    }
    if (w->decorazione)     /* chi disegna la barra: prima del configure, che lo rende valido */
        zxdg_toplevel_decoration_v1_send_configure(w->decorazione, w->csd_voluto ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
                                                                                   : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    xdg_toplevel_send_configure(w->toplevel, 0, 0, &stati);
    xdg_surface_send_configure(w->xdg_surface, ++w->serial);
    wl_array_release(&stati);
    w->configurata = 1;
}

static void toplevel_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
/* Le richieste che si ignorano (una funzione per ogni forma di argomenti) */
static void toplevel_senza(struct wl_client *c, struct wl_resource *r) { (void)c; (void)r; }
static void toplevel_con_risorsa(struct wl_client *c, struct wl_resource *r, struct wl_resource *x) { (void)c; (void)r; (void)x; }
static void toplevel_con_testo(struct wl_client *c, struct wl_resource *r, const char *t) { (void)c; (void)r; (void)t; }
static void toplevel_con_misura(struct wl_client *c, struct wl_resource *r, int32_t l, int32_t a) { (void)c; (void)r; (void)l; (void)a; }

static void toplevel_titolo(struct wl_client *c, struct wl_resource *r, const char *titolo) {
    (void)c;
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w) return;
    size_t n = strlen(titolo);
    if (n > MAX_TITOLO - 1) n = MAX_TITOLO - 1;
    memcpy(w->titolo, titolo, n);
    w->titolo[n] = 0;
    ridisegna(w);   /* se la finestra e' gia' sullo schermo, il titolo cambia subito */
}

/* La finestra sparisce: il toplevel e' distrutto (dal programma o perche' e' finito). */
static void toplevel_distrutto(struct wl_resource *r) {
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w) return;
    w->toplevel = 0;
    if (w->slot >= 0) {
        int posto = w->slot;
        w->slot = -1;
        segna_posto(posto, 0);
        ponte_togli(posto);
    }
    w->configurata = 0;
}

/* TRASCINAMI: xdg_toplevel.move. Vale solo se 'serial' e' quello dell'ultimo clic che abbiamo
 * mandato al programma e il pulsante e' ancora giu' (come TRASCINAMI delle finestre nostre: se
 * l'ha gia' lasciato non si sposta niente); ponte_trascinami controlla anche che il clic sia
 * sulla finestra. */
static void toplevel_muovi(struct wl_client *c, struct wl_resource *r, struct wl_resource *seat, uint32_t serial) {
    (void)c; (void)seat;
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w || w->slot < 0 || !w->consegnati || serial != w->serial_clic) return;
    ponte_trascinami(w->slot);
}

/* RIDUCIMI: xdg_toplevel.set_minimized. */
static void toplevel_riduci(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (w && w->slot >= 0) ponte_riduci(w->slot);
}

/* Si ignorano: il genitore, l'id dell'app, i limiti di misura, massimizza, schermo intero, il menu
 * della finestra e il ridimensionamento (xdg-shell permette al compositore di ignorarle). */
static void toplevel_menu(struct wl_client *c, struct wl_resource *r, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {
    (void)c; (void)r; (void)seat; (void)serial; (void)x; (void)y;
}
static void toplevel_ridimensiona(struct wl_client *c, struct wl_resource *r, struct wl_resource *seat, uint32_t serial, uint32_t bordo) {
    (void)c; (void)r; (void)seat; (void)serial; (void)bordo;
}
static const struct xdg_toplevel_interface toplevel_interfaccia = {
    .destroy = toplevel_distruggi,
    .set_parent = toplevel_con_risorsa,
    .set_title = toplevel_titolo,
    .set_app_id = toplevel_con_testo,
    .show_window_menu = toplevel_menu,
    .move = toplevel_muovi,
    .resize = toplevel_ridimensiona,
    .set_max_size = toplevel_con_misura,
    .set_min_size = toplevel_con_misura,
    .set_maximized = toplevel_senza,
    .unset_maximized = toplevel_senza,
    .set_fullscreen = toplevel_con_risorsa,
    .unset_fullscreen = toplevel_senza,
    .set_minimized = toplevel_riduci,
};

/* --- xdg_surface --- */

static void xdg_surface_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void xdg_surface_toplevel(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (w->toplevel) {
        wl_resource_post_error(r, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "e' gia' una finestra");
        return;
    }
    int posto = ponte_apri(wl_client_get_fd(client), wl_client_get_user_data(client));
    struct wl_resource *t = wl_resource_create(client, &xdg_toplevel_interface, wl_resource_get_version(r), id);
    if (posto < 0 || !t || !segna_posto(posto, w)) {
        if (posto >= 0) ponte_togli(posto);
        wl_resource_post_no_memory(r);
        return;
    }
    wl_resource_set_implementation(t, &toplevel_interfaccia, w, toplevel_distrutto);
    w->toplevel = t;
    w->slot = posto;
}

static void xdg_surface_popup(struct wl_client *c, struct wl_resource *r, uint32_t id, struct wl_resource *genitore,
                              struct wl_resource *posizionatore) {
    (void)c; (void)id; (void)genitore; (void)posizionatore;
    wl_resource_post_error(r, WL_DISPLAY_ERROR_INVALID_METHOD, "i popup non ci sono ancora");
}

static void xdg_surface_geometria(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t l, int32_t a) {
    (void)c; (void)r; (void)x; (void)y; (void)l; (void)a;   /* tutto il buffer e' la finestra */
}

static void xdg_surface_ack(struct wl_client *c, struct wl_resource *r, uint32_t serial) {
    (void)c; (void)r; (void)serial;
}

static const struct xdg_surface_interface xdg_surface_interfaccia = {
    .destroy = xdg_surface_distruggi,
    .get_toplevel = xdg_surface_toplevel,
    .get_popup = xdg_surface_popup,
    .set_window_geometry = xdg_surface_geometria,
    .ack_configure = xdg_surface_ack,
};

static void xdg_surface_distrutta(struct wl_resource *r) {
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (w->decorazione) wl_resource_set_user_data(w->decorazione, 0);   /* la finestra sta per sparire */
    if (w->toplevel) {                    /* il toplevel non e' ancora distrutto: si stacca */
        wl_resource_set_user_data(w->toplevel, 0);
        if (w->slot >= 0) {
            segna_posto(w->slot, 0);
            ponte_togli(w->slot);
        }
    }
    if (w->superficie) w->superficie->finestra = 0;
    free(w->contenuto);
    free(w->immagine);
    free(w);
}

/* --- xdg_wm_base --- */

static void wm_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void wm_posizionatore(struct wl_client *c, struct wl_resource *r, uint32_t id) {
    (void)c; (void)id;
    wl_resource_post_error(r, WL_DISPLAY_ERROR_INVALID_METHOD, "i popup non ci sono ancora");
}

static void wm_xdg_surface(struct wl_client *client, struct wl_resource *r, uint32_t id, struct wl_resource *superficie) {
    struct superficie *s = wl_resource_get_user_data(superficie);
    if (s->finestra || s->e_sotto) {
        wl_resource_post_error(r, XDG_WM_BASE_ERROR_ROLE, "la superficie ha gia' un ruolo");
        return;
    }
    struct finestra_wl *w = calloc(1, sizeof *w);
    struct wl_resource *x = wl_resource_create(client, &xdg_surface_interface, wl_resource_get_version(r), id);
    if (!w || !x) {
        free(w);
        wl_resource_post_no_memory(r);
        return;
    }
    w->superficie = s;
    w->cliente = client;
    w->xdg_surface = x;
    w->slot = -1;
    s->finestra = w;
    wl_resource_set_implementation(x, &xdg_surface_interfaccia, w, xdg_surface_distrutta);
}

static void risorsa_tolta_base(struct wl_resource *r) { wl_list_remove(wl_resource_get_link(r)); }

/* PING e PONG. Il compositore chiede "ci sei?" (wayland_notizia, PING) con xdg_wm_base.ping: il
 * numero di serie e' quello di compositore.c, e si tiene nella finestra a cui si e' chiesto. Il
 * pong con quel numero va a ponte_pong, che lo gira come il PONG delle finestre nostre. Un pong
 * con un numero che nessuna finestra del programma aspetta si ignora. */
static struct wl_list basi;      /* gli xdg_wm_base di tutti i programmi */

static void wm_pong(struct wl_client *c, struct wl_resource *r, uint32_t serial) {
    (void)r;
    for (int i = 0; i < posti_wl; i++) {
        struct finestra_wl *w = per_posto[i];
        if (!w || w->cliente != c || !w->ping_serial || w->ping_serial != serial) continue;
        w->ping_serial = 0;
        ponte_pong(i, serial);
        return;
    }
}

static const struct xdg_wm_base_interface wm_interfaccia = {
    .destroy = wm_distruggi,
    .create_positioner = wm_posizionatore,
    .get_xdg_surface = wm_xdg_surface,
    .pong = wm_pong,
};

static void lega_wm(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &xdg_wm_base_interface, versione > 3 ? 3 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &wm_interfaccia, 0, risorsa_tolta_base);
    wl_list_insert(&basi, wl_resource_get_link(r));
}

/* --- wl_region: dove si clicca ---
 * Se ne tiene solo la forma semplice: una somma di rettangoli (add), al massimo MAX_RETT. Una sottrazione
 * o troppi rettangoli la rendono "tutta la superficie" (come se non ci fosse regione). */

static void regione_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static void regione_tolta(struct wl_resource *r) { free(wl_resource_get_user_data(r)); }
static void regione_aggiungi(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t l, int32_t a) {
    (void)c;
    struct area *g = wl_resource_get_user_data(r);
    if (g->tutta || l <= 0 || a <= 0) return;
    if (g->n >= MAX_RETT) { g->tutta = 1; return; }
    g->r[g->n][0] = x;
    g->r[g->n][1] = y;
    g->r[g->n][2] = l;
    g->r[g->n][3] = a;
    g->n++;
}
static void regione_sottrai(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t l, int32_t a) {
    (void)c; (void)x; (void)y; (void)l; (void)a;
    struct area *g = wl_resource_get_user_data(r);
    g->tutta = 1;
}
static const struct wl_region_interface regione_interfaccia = {
    .destroy = regione_distruggi,
    .add = regione_aggiungi,
    .subtract = regione_sottrai,
};

/* --- wl_surface --- */

static void pu_esce(struct finestra_wl *w);

/* La superficie non e' piu' sotto il suo genitore (distrutta o staccata): si toglie dalle sue pile. */
static void stacca(struct superficie *s) {
    if (!s->genitore) return;
    wl_list_remove(&s->fratello);
    wl_list_remove(&s->fratello_att);
    s->genitore = 0;
}

/* Il puntatore e' su una superficie che non e' piu' dell'albero della sua finestra (staccata): leave. */
static void focus_controlla(void) {
    for (int i = 0; i < posti_wl; i++) {
        struct finestra_wl *w = per_posto[i];
        if (!w || !w->dentro || !w->focus || (w->superficie && capostipite(w->focus) == w->superficie)) continue;
        pu_esce(w);
        w->ignorati = w->visti;
        w->consegnati = 0;
    }
}

static void sup_senza_buffer(struct superficie *s) {
    if (s->buffer) wl_list_remove(&s->ascolto_buffer.link);
    s->buffer = 0;
}

static void buffer_distrutto(struct wl_listener *l, void *dati) {
    (void)dati;
    struct superficie *s = wl_container_of(l, s, ascolto_buffer);
    s->buffer = 0;           /* ha distrutto il buffer prima del commit: e' come un attach vuoto */
}

static void sup_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void sup_attach(struct wl_client *c, struct wl_resource *r, struct wl_resource *buffer, int32_t x, int32_t y) {
    (void)c; (void)x; (void)y;
    struct superficie *s = wl_resource_get_user_data(r);
    sup_senza_buffer(s);
    s->buffer = buffer;
    s->ha_attach = 1;
    if (buffer) {
        s->ascolto_buffer.notify = buffer_distrutto;
        wl_resource_add_destroy_listener(buffer, &s->ascolto_buffer);
    }
}

/* Dove e' cambiato non ci serve: a ogni commit si copia tutto il contenuto (e il compositore
 * rifa' tutta la finestra). */
static void sup_danno(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t l, int32_t a) {
    (void)c; (void)r; (void)x; (void)y; (void)l; (void)a;
}

static void callback_distrutto(struct wl_resource *r) { wl_list_remove(wl_resource_get_link(r)); }

static void sup_frame(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct superficie *s = wl_resource_get_user_data(r);
    struct wl_resource *cb = wl_resource_create(client, &wl_callback_interface, 1, id);
    if (!cb) { wl_resource_post_no_memory(r); return; }
    wl_resource_set_implementation(cb, 0, 0, callback_distrutto);
    wl_list_insert(s->in_attesa.prev, wl_resource_get_link(cb));
}

/* L'opacita' (dove la superficie e' piena): non serve, si mescola sempre. */
static void sup_regione(struct wl_client *c, struct wl_resource *r, struct wl_resource *regione) {
    (void)c; (void)r; (void)regione;
}

/* DOVE SI CLICCA: la regione vale al commit (come il resto). Senza regione, tutta la superficie. */
static void sup_regione_input(struct wl_client *c, struct wl_resource *r, struct wl_resource *regione) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    if (regione) s->input_attesa = *(struct area *)wl_resource_get_user_data(regione);
    else { memset(&s->input_attesa, 0, sizeof s->input_attesa); s->input_attesa.tutta = 1; }
    s->ha_input = 1;
}

/* IL COMMIT. Quello che il programma ha detto dall'ultimo commit (buffer, regione, frame) diventa lo stato
 * "in cache" di questa superficie. Se non e' sincrona, vale subito (e con lei la posizione e l'ordine dei
 * suoi figli e il loro stato in cache); se lo e' (una sottosuperficie sincrona, di default), vale quando il
 * genitore applica il suo. Il buffer si copia e si rilascia subito: il programma lo puo' riusare. */
static void sup_commit(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    struct finestra_wl *w = s->finestra;
    int sync = sync_effettivo(s);
    /* le richieste di "avvisami del prossimo fotogramma" fatte fin qui valgono adesso (o col genitore) */
    wl_list_insert_list(sync ? s->in_cache.prev : s->da_servire.prev, &s->in_attesa);
    wl_list_init(&s->in_attesa);

    struct wl_resource *buffer = s->buffer;
    int con_buffer = s->ha_attach;
    sup_senza_buffer(s);
    s->ha_attach = 0;
    if (!s->e_sotto) {                    /* la principale */
        if (!w || w->slot < 0) {          /* senza ruolo di finestra: niente da mostrare */
            if (buffer) wl_buffer_send_release(buffer);
            return;
        }
        if (!w->configurata) {            /* il primo commit, senza contenuto: il configure iniziale */
            if (buffer) wl_buffer_send_release(buffer);
            invia_configure(w);
            return;
        }
        if (buffer) w->csd = w->csd_voluto;     /* chi disegna la barra: vale da questo buffer */
    }
    if (con_buffer) {
        if (buffer) {
            unsigned int *pix;
            int pl, pa;
            if (copia_buffer(buffer, &pix, &pl, &pa)) {
                free(s->cache_pix);
                s->cache_pix = pix;
                s->cache_l = pl;
                s->cache_a = pa;
                s->cache_contenuto = 1;
            }
            wl_buffer_send_release(buffer);
        } else if (s->e_sotto) {          /* attach vuoto: la sottosuperficie non e' piu' mappata */
            free(s->cache_pix);
            s->cache_pix = 0;
            s->cache_contenuto = 1;
        }
    }
    if (s->ha_input) {
        s->cache_input = s->input_attesa;
        s->cache_ha_input = 1;
        s->ha_input = 0;
    }
    s->cache = 1;
    if (sync) return;                     /* aspetta il commit del genitore */
    if (applica(s, 0)) ricompone(s);
}

/* La scala e la trasformazione del buffer: si accettano, ma si disegna sempre a scala 1 e senza ruotare. */
static void sup_trasformazione(struct wl_client *c, struct wl_resource *r, int32_t t) { (void)c; (void)r; (void)t; }
static void sup_scala(struct wl_client *c, struct wl_resource *r, int32_t s) {
    (void)c;
    if (s < 1) wl_resource_post_error(r, WL_SURFACE_ERROR_INVALID_SCALE, "la scala deve essere almeno 1");
}
static void sup_scostamento(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y) { (void)c; (void)r; (void)x; (void)y; }

static const struct wl_surface_interface sup_interfaccia = {
    .destroy = sup_distruggi,
    .attach = sup_attach,
    .damage = sup_danno,
    .frame = sup_frame,
    .set_opaque_region = sup_regione,
    .set_input_region = sup_regione_input,
    .commit = sup_commit,
    .set_buffer_transform = sup_trasformazione,
    .set_buffer_scale = sup_scala,
    .damage_buffer = sup_danno,
    .offset = sup_scostamento,
};

static void sup_distrutta(struct wl_resource *r) {
    struct superficie *s = wl_resource_get_user_data(r);
    sup_senza_buffer(s);
    struct wl_list *liste[3] = { &s->in_attesa, &s->da_servire, &s->in_cache };
    for (int k = 0; k < 3; k++) {
        struct wl_list *l = liste[k];
        for (struct wl_list *p = l->next, *n; p != l; p = n) {
            n = p->next;
            wl_resource_destroy(wl_resource_from_link(p));   /* si toglie da solo dalla lista */
        }
    }
    /* il puntatore non l'ha piu': la superficie non c'e' piu', niente leave */
    for (int i = 0; i < posti_wl; i++) {
        struct finestra_wl *w = per_posto[i];
        if (!w || w->focus != s) continue;
        w->focus = 0;
        w->dentro = 0;
        w->consegnati = 0;
        w->ignorati = w->visti;
    }
    /* i figli restano senza genitore (inerti: non si vedono piu'), lei esce dall'albero del suo */
    struct superficie *cima = s->genitore ? capostipite(s->genitore) : 0;
    for (struct wl_list *p = s->pila.next, *n; p != &s->pila; p = n) {
        n = p->next;
        if (p == &s->segno) continue;
        struct superficie *c = wl_container_of(p, c, fratello);
        stacca(c);
    }
    stacca(s);
    if (s->sotto) wl_resource_set_user_data(s->sotto, 0);
    if (cima) ricompone(cima);
    focus_controlla();
    if (s->finestra) s->finestra->superficie = 0;
    free(s->pix);
    free(s->cache_pix);
    wl_list_remove(&s->link);
    free(s);
}

/* --- wl_subcompositor e wl_subsurface ---
 *
 * get_subsurface(superficie, genitore) da' alla superficie il ruolo di sottosuperficie del genitore:
 * nasce in cima alla pila dei fratelli, in posizione (0, 0) e sincrona. Gli errori di protocollo sono
 * quelli del wayland.xml: la superficie ha gia' un ruolo (finestra o sottosuperficie), il genitore e' lei
 * stessa o una sua discendente (un giro nell'albero), il fratello di place_above/below non e' un fratello.
 * Posizione e ordine (set_position, place_above, place_below) valgono al commit del genitore. */

static void sotto_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

/* Il wl_subsurface e' distrutto: la superficie esce subito dallo schermo (resta senza genitore). */
static void sotto_distrutta(struct wl_resource *r) {
    struct superficie *s = wl_resource_get_user_data(r);
    if (!s) return;                       /* la superficie e' gia' distrutta */
    s->sotto = 0;
    struct superficie *cima = s->genitore ? capostipite(s->genitore) : 0;
    stacca(s);
    free(s->pix);
    s->pix = 0;
    free(s->cache_pix);
    s->cache_pix = 0;
    s->cache = 0;
    s->cache_contenuto = 0;
    s->cache_ha_input = 0;
    if (cima) ricompone(cima);
    focus_controlla();
}

static void sotto_posizione(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    if (!s) return;
    s->px = x < -POSIZIONE_MAX ? -POSIZIONE_MAX : x > POSIZIONE_MAX ? POSIZIONE_MAX : x;
    s->py = y < -POSIZIONE_MAX ? -POSIZIONE_MAX : y > POSIZIONE_MAX ? POSIZIONE_MAX : y;
}

/* place_above e place_below: nell'ordine in attesa, subito sopra (o sotto) il fratello o il genitore. */
static void sotto_ordina(struct wl_resource *r, struct wl_resource *fratello, int sopra) {
    struct superficie *s = wl_resource_get_user_data(r);
    if (!s || !s->genitore) return;
    struct superficie *f = wl_resource_get_user_data(fratello), *g = s->genitore;
    if (f == s || (f != g && f->genitore != g)) {
        wl_resource_post_error(r, WL_SUBSURFACE_ERROR_BAD_SURFACE, "la superficie non e' un fratello ne' il genitore");
        return;
    }
    struct wl_list *rif = f == g ? &g->segno_att : &f->fratello_att;
    wl_list_remove(&s->fratello_att);
    wl_list_insert(sopra ? rif : rif->prev, &s->fratello_att);
}
static void sotto_sopra(struct wl_client *c, struct wl_resource *r, struct wl_resource *f) { (void)c; sotto_ordina(r, f, 1); }
static void sotto_sotto(struct wl_client *c, struct wl_resource *r, struct wl_resource *f) { (void)c; sotto_ordina(r, f, 0); }

static void sotto_sincrona(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    if (s) s->sync = 1;
}

/* Da asincrona: se c'e' uno stato in cache e nessun antenato e' sincrono, vale subito. */
static void sotto_asincrona(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    if (!s) return;
    s->sync = 0;
    if (s->cache && !sync_effettivo(s) && applica(s, 0)) ricompone(s);
}

static const struct wl_subsurface_interface sotto_interfaccia = {
    .destroy = sotto_distruggi,
    .set_position = sotto_posizione,
    .place_above = sotto_sopra,
    .place_below = sotto_sotto,
    .set_sync = sotto_sincrona,
    .set_desync = sotto_asincrona,
};

static void subc_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void subc_prendi(struct wl_client *client, struct wl_resource *r, uint32_t id, struct wl_resource *superficie,
                        struct wl_resource *genitore) {
    struct superficie *s = wl_resource_get_user_data(superficie), *g = wl_resource_get_user_data(genitore);
    if (s->finestra || s->sotto) {
        wl_resource_post_error(r, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, "la superficie ha gia' un ruolo");
        return;
    }
    int n = 0;
    for (struct superficie *a = g; a && n < 1000; a = a->genitore, n++)
        if (a == s) {
            wl_resource_post_error(r, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT, "il genitore e' la superficie o una sua discendente");
            return;
        }
    struct wl_resource *x = wl_resource_create(client, &wl_subsurface_interface, wl_resource_get_version(r), id);
    if (!x) { wl_resource_post_no_memory(r); return; }
    s->e_sotto = 1;
    s->sotto = x;
    s->genitore = g;
    s->x = s->y = s->px = s->py = 0;
    s->sync = 1;
    wl_list_insert(g->pila.prev, &s->fratello);          /* in cima ai fratelli */
    wl_list_insert(g->pila_att.prev, &s->fratello_att);
    wl_resource_set_implementation(x, &sotto_interfaccia, s, sotto_distrutta);
}

static const struct wl_subcompositor_interface subc_interfaccia = {
    .destroy = subc_distruggi,
    .get_subsurface = subc_prendi,
};

static void lega_subcompositor(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati; (void)versione;
    struct wl_resource *r = wl_resource_create(client, &wl_subcompositor_interface, 1, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &subc_interfaccia, 0, 0);
}

/* --- wl_compositor --- */

static void comp_superficie(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct superficie *s = calloc(1, sizeof *s);
    struct wl_resource *x = wl_resource_create(client, &wl_surface_interface, wl_resource_get_version(r), id);
    if (!s || !x) {
        free(s);
        wl_resource_post_no_memory(r);
        return;
    }
    s->risorsa = x;
    wl_list_init(&s->in_attesa);
    wl_list_init(&s->da_servire);
    wl_list_init(&s->in_cache);
    s->input.tutta = s->input_attesa.tutta = s->cache_input.tutta = 1;   /* senza regione si clicca dappertutto */
    wl_list_init(&s->pila);
    wl_list_init(&s->pila_att);
    wl_list_insert(&s->pila, &s->segno);                 /* la pila ha solo lei */
    wl_list_insert(&s->pila_att, &s->segno_att);
    wl_list_insert(&superfici, &s->link);
    wl_resource_set_implementation(x, &sup_interfaccia, s, sup_distrutta);
}

static void comp_regione(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct wl_resource *x = wl_resource_create(client, &wl_region_interface, wl_resource_get_version(r), id);
    struct area *g = calloc(1, sizeof *g);
    if (!x || !g) { free(g); wl_resource_post_no_memory(r); return; }
    wl_resource_set_implementation(x, &regione_interfaccia, g, regione_tolta);
}

static const struct wl_compositor_interface comp_interfaccia = {
    .create_surface = comp_superficie,
    .create_region = comp_regione,
};

static void lega_compositor(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &wl_compositor_interface, versione > 4 ? 4 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &comp_interfaccia, 0, 0);
}

/* --- wl_seat: il puntatore e la tastiera ---
 *
 * Un "seat" e' un posto davanti al computer: una tastiera e un mouse della stessa persona. Il
 * programma ne chiede il wl_pointer (il mouse) e il wl_keyboard (la tastiera) e da li' gli
 * arrivano gli eventi. Il compositore decide A CHI vanno con le stesse regole delle nostre
 * finestre, senza regole nuove: le notizie PUNTATORE, USCITO, ROTELLA e FUOCO sono quelle di
 * avvisa (compositore.c) per una finestra con il collegamento, e arrivano qui (wayland_notizia);
 * i tasti grezzi li manda al_tasto_grezzo (wayland_tasto).
 *
 * IL PUNTATORE. Le coordinate che il programma vede sono quelle della SUPERFICIE, cioe' del
 * contenuto: la testata (la cornice nostra) non e' sua. Sulla cornice il programma non sente
 * niente: ci pensa il compositore (trascinare, ridurre, chiudere). Un clic cominciato sulla
 * cornice non gli arriva, nemmeno quando il puntatore entra nel contenuto tenendo premuto; uno
 * cominciato nel contenuto gli arriva anche quando il puntatore esce, fino al rilascio (come in
 * ogni compositore: la "presa" del clic). Il puntatore e' "dentro" da enter a leave.
 * Gli eventi di un gruppo (spostamento + pulsante) finiscono con wl_pointer.frame.
 * La rotella a una finestra senza fuoco (che non sente il puntatore, scelta della persona)
 * arriva lo stesso: per quel gruppo il puntatore "entra", gira, "esce".
 *
 * LA TASTIERA. Il programma riceve la keymap (il testo XKB dell'italiano, in un memfd: vedi
 * carica_keymap), enter e leave quando la finestra prende o perde il FUOCO, i tasti con il
 * codice di Linux (la libreria XKB del programma aggiunge 8) e i modificatori. La ripetizione
 * dei tasti la fa il programma (repeat_info): il compositore scarta le ripetizioni del kernel. */

/* wl_pointer.set_cursor: il compositore mostra sempre la sua freccia (la forma scelta dal
 * programma non c'e' ancora). release e' il distruttore. */
static void puntatore_cursore(struct wl_client *c, struct wl_resource *r, uint32_t serial, struct wl_resource *superficie,
                              int32_t x, int32_t y) {
    (void)c; (void)r; (void)serial; (void)superficie; (void)x; (void)y;
}
static void puntatore_rilascia(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static const struct wl_pointer_interface puntatore_interfaccia = {
    .set_cursor = puntatore_cursore,
    .release = puntatore_rilascia,
};
static void tastiera_rilascia(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static const struct wl_keyboard_interface tastiera_interfaccia = {
    .release = tastiera_rilascia,
};

/* i wl_pointer e i wl_keyboard di tutti i programmi (ogni wl_seat.get_pointer ne crea uno) */
static struct wl_list puntatori, tastiere;

static void risorsa_tolta(struct wl_resource *r) { wl_list_remove(wl_resource_get_link(r)); }

/* Un ciclo sulle risorse di una lista che sono del programma 'c' */
#define PER_DEL_PROGRAMMA(r, lista, c) \
    for (struct wl_resource *r = wl_resource_from_link((lista)->next); wl_resource_get_link(r) != (lista); \
         r = wl_resource_from_link(wl_resource_get_link(r)->next)) \
        if (wl_resource_get_client(r) == (c))

/* La keymap: il testo XKB in un memfd chiuso (sigilli: nessuno lo puo' piu' cambiare). -1 se non
 * c'e': allora il seat non ha la tastiera. */
static int keymap_fd = -1;
static uint32_t keymap_byte;

#define FILE_KEYMAP "/system/resources/keymaps/it.xkb"

/* LA KEYMAP si compila durante la compilazione del sistema (strumenti/compilazione/tastiera.sh:
 * xkbcomp con il layout "it" di xkeyboard-config) e sta tra le risorse del sistema. Qui si legge
 * una volta, all'avvio: dentro il compositore non c'e' (e non deve esserci) nessuna libreria XKB.
 * Wayland vuole il testo CON lo zero finale, e la sua misura. */
static void carica_keymap(void) {
    int f = open(FILE_KEYMAP, O_RDONLY | O_CLOEXEC);
    struct stat st;
    if (f < 0 || fstat(f, &st) < 0 || st.st_size < 1 || st.st_size > (1 << 20)) {
        if (f >= 0) close(f);
        scrivi_riga("compositore", "senza keymap (" FILE_KEYMAP "): i programmi Wayland non hanno la tastiera", "");
        return;
    }
    size_t n = (size_t)st.st_size;
    char *testo = malloc(n + 1);
    size_t letti = 0;
    while (testo && letti < n) {
        ssize_t k = read(f, testo + letti, n - letti);
        if (k <= 0) break;
        letti += (size_t)k;
    }
    close(f);
    int m = testo && letti == n ? memfd_create("keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING) : -1;
    if (m >= 0) {
        testo[n] = 0;
        if (write(m, testo, n + 1) != (ssize_t)(n + 1)) { close(m); m = -1; }
    }
    free(testo);
    if (m < 0) return;
    fcntl(m, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL);
    keymap_fd = m;
    keymap_byte = (uint32_t)(n + 1);
}

/* La finestra con il fuoco (quella "attiva" che c'e' ancora): il compositore ne da' una sola. */
static struct finestra_wl *finestra_con_fuoco(void) {
    for (int i = 0; i < posti_wl; i++)
        if (per_posto[i] && per_posto[i]->attiva && per_posto[i]->toplevel) return per_posto[i];
    return 0;
}

static struct wl_resource *risorsa_superficie(const struct finestra_wl *w) {
    return w->superficie ? w->superficie->risorsa : 0;
}

/* --- il puntatore --- */

static void pu_frame(struct wl_resource *r) {
    if (wl_resource_get_version(r) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(r);
}

/* Il punto (x, y) della finestra (cornice compresa) sul suo contenuto (senza cornice nostra la finestra e' il buffer). */
static void sul_contenuto(const struct finestra_wl *w, int x, int y, int *cx, int *cy) {
    *cx = x;
    *cy = y - (w->csd ? 0 : cornice_testata_altezza());
}

/* La superficie (principale o sotto) che sta sotto il punto (x, y) della finestra, e il punto su di lei.
 * 0 se il punto e' sulla cornice, fuori dal contenuto, o se li' nessuna superficie prende il puntatore. */
static struct superficie *bersaglio(const struct finestra_wl *w, int x, int y, int *sx, int *sy) {
    int cx, cy;
    sul_contenuto(w, x, y, &cx, &cy);
    if (!w->superficie || cx < 0 || cx >= w->cl || cy < 0 || cy >= w->ch) return 0;
    return colpisci(w->superficie, 0, 0, cx, cy, 0, sx, sy);
}

/* wl_pointer.enter: il puntatore entra nella superficie 'sup' (della finestra w), nel punto (sx, sy) di lei. */
static void pu_entra(struct finestra_wl *w, struct superficie *sup, int sx, int sy) {
    struct wl_resource *superficie = sup->risorsa;
    uint32_t serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) {
        wl_pointer_send_enter(r, serial, superficie, wl_fixed_from_int(sx), wl_fixed_from_int(sy));
        pu_frame(r);
    }
    w->serial_entra = serial;
    ponte_forma(w->slot, FORMA_NORMALE);      /* a ogni enter si riparte dalla freccia: la forma la dice il programma */
    w->focus = sup;
    w->dentro = 1;
    w->px = sx;
    w->py = sy;
}

/* wl_pointer.leave dalla superficie che aveva il puntatore. */
static void pu_esce(struct finestra_wl *w) {
    struct superficie *f = w->focus;
    w->dentro = 0;
    w->focus = 0;
    if (!f) return;
    uint32_t serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) {
        wl_pointer_send_leave(r, serial, f->risorsa);
        pu_frame(r);
    }
}

static uint32_t codice_pulsante(unsigned int bit) {
    return bit == PULSANTE_SINISTRO ? BTN_LEFT : bit == PULSANTE_DESTRO ? BTN_RIGHT : BTN_MIDDLE;
}

/* Il puntatore deve essere su 'hit' (0: su nessuna): se era su un'altra superficie, leave e poi enter. 1 se e'
 * appena entrato. Un clic cominciato fuori (ignorati) non lascia entrare. */
static int pu_cambia(struct finestra_wl *w, struct superficie *hit, int hx, int hy) {
    if (w->dentro && hit == w->focus) return 0;
    if (w->dentro) pu_esce(w);
    if (!hit || w->ignorati) return 0;
    pu_entra(w, hit, hx, hy);
    return 1;
}

/* Il puntatore e' a (x, y) della finestra (cornice compresa) con questi pulsanti giu'. Va alla superficie che
 * sta sotto (principale o sotto), con le coordinate di quella; un clic tenuto giu' tiene il puntatore sulla
 * superficie in cui e' cominciato (la "presa"), anche se il puntatore passa su un'altra. */
static void pu_puntatore(struct finestra_wl *w, int x, int y, unsigned int pulsanti) {
    int hx = 0, hy = 0;
    struct superficie *hit = bersaglio(w, x, y, &hx, &hy);
    unsigned int nuovi = pulsanti & ~w->visti, lasciati = w->visti & ~pulsanti;
    w->visti = pulsanti;
    w->ignorati &= pulsanti;                  /* un clic cominciato fuori e finito: non c'e' piu' */
    int appena_entrato = 0;
    if (!(w->dentro && w->consegnati)) {      /* senza presa conta quello che sta sotto il puntatore */
        appena_entrato = pu_cambia(w, hit, hx, hy);
        if (!w->dentro) w->ignorati |= nuovi, nuovi = 0;   /* premuto sulla cornice: non e' del programma */
    }
    if (!w->dentro) return;
    int cx, cy, ox, oy;
    sul_contenuto(w, x, y, &cx, &cy);
    origine(w->focus, &ox, &oy);
    int sx = cx - ox, sy = cy - oy;           /* il punto sulla superficie che ha il puntatore */
    uint32_t t = tempo();
    int cambia = sx != w->px || sy != w->py;
    if (!appena_entrato && cambia) {
        PER_DEL_PROGRAMMA(r, &puntatori, w->cliente)
            wl_pointer_send_motion(r, t, wl_fixed_from_int(sx), wl_fixed_from_int(sy));
        w->px = sx;
        w->py = sy;
    }
    int detto = cambia && !appena_entrato;    /* c'e' qualcosa da chiudere con un frame */
    for (unsigned int bit = 1; bit <= PULSANTE_CENTRALE; bit <<= 1) {
        int premuto = (nuovi & bit) != 0;
        if (!premuto && !(lasciati & bit & w->consegnati)) continue;
        uint32_t serial = wl_display_next_serial(display);
        if (premuto) w->serial_clic = serial;     /* xdg_toplevel.move lo cita */
        PER_DEL_PROGRAMMA(r, &puntatori, w->cliente)
            wl_pointer_send_button(r, serial, t, codice_pulsante(bit),
                                   premuto ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
        w->consegnati = premuto ? w->consegnati | bit : w->consegnati & ~bit;
        detto = 1;
    }
    if (detto) PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) pu_frame(r);
    /* niente clic tenuto e il puntatore non e' piu' sulla superficie che lo aveva: leave, e se sta su
     * un'altra superficie della finestra, enter */
    if (!w->consegnati && hit != w->focus) pu_cambia(w, hit, hx, hy);
}

/* La rotella: scatti verticali e orizzontali (positivi: in basso e a destra) sul punto (x, y). */
static void pu_rotella(struct finestra_wl *w, int x, int y, int scatti_x, int scatti_y) {
    int sx, sy;
    struct superficie *hit = bersaglio(w, x, y, &sx, &sy);
    if (!hit) return;
    int ospite = !w->dentro;                  /* senza fuoco il puntatore non c'e': entra solo per girare */
    if (ospite) pu_entra(w, hit, sx, sy);
    uint32_t t = tempo();
    PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) {
        int v5 = wl_resource_get_version(r) >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION;
        if (v5) wl_pointer_send_axis_source(r, WL_POINTER_AXIS_SOURCE_WHEEL);
        for (int asse = 0; asse < 2; asse++) {
            int scatti = asse == 0 ? scatti_y : scatti_x;
            if (!scatti) continue;
            enum wl_pointer_axis a = asse == 0 ? WL_POINTER_AXIS_VERTICAL_SCROLL : WL_POINTER_AXIS_HORIZONTAL_SCROLL;
            if (v5) wl_pointer_send_axis_discrete(r, a, scatti);
            wl_pointer_send_axis(r, t, a, wl_fixed_from_int(scatti * PIXEL_PER_SCATTO));
        }
        pu_frame(r);
    }
    if (ospite) pu_esce(w);
}

/* --- la tastiera --- */

static unsigned char tasti_giu[(KEY_MAX + 8) / 8];   /* i tasti tenuti giu' adesso (codici di Linux) */
/* GREZZO_... adesso. Si parte con Bloc Num acceso, come eventi (che lo accende all'avvio): il
 * primo tasto allinea comunque lo stato a quello vero. */
static unsigned int modificatori = GREZZO_BLOC_NUM;

/* I modificatori come li vuole XKB: le maschere dei modificatori "veri", che sono gli stessi
 * in ogni keymap (Shift 0, Lock 1, Control 2, Mod1 3, Mod2 4, Mod4 6, Mod5 7): Alt e' Mod1, Bloc
 * Num Mod2, il logo Mod4, AltGr (ISO_Level3_Shift) Mod5. 'premuti' e 'bloccati' sono le due maschere. */
static void maschere_xkb(unsigned int mod, uint32_t *premuti, uint32_t *bloccati) {
    *premuti = (mod & GREZZO_MAIUSC ? 1u << 0 : 0) | (mod & GREZZO_CTRL ? 1u << 2 : 0) | (mod & GREZZO_ALT ? 1u << 3 : 0) |
               (mod & GREZZO_META ? 1u << 6 : 0) | (mod & GREZZO_ALTGR ? 1u << 7 : 0);
    *bloccati = (mod & GREZZO_BLOC_MAIUSC ? 1u << 1 : 0) | (mod & GREZZO_BLOC_NUM ? 1u << 4 : 0);
}

static void ta_modificatori(struct wl_resource *r, uint32_t serial, unsigned int mod) {
    uint32_t premuti, bloccati;
    maschere_xkb(mod, &premuti, &bloccati);
    wl_keyboard_send_modifiers(r, serial, premuti, 0, bloccati, 0);
}

/* Il programma riceve il fuoco della tastiera: enter con i tasti gia' giu', poi i modificatori. */
static void ta_entra_in(struct wl_resource *r, struct finestra_wl *w) {
    struct wl_resource *superficie = risorsa_superficie(w);
    if (!superficie) return;
    struct wl_array giu;
    wl_array_init(&giu);
    for (int k = 0; k <= KEY_MAX; k++)
        if (tasti_giu[k / 8] >> (k % 8) & 1) {
            uint32_t *e = wl_array_add(&giu, sizeof *e);
            if (e) *e = (uint32_t)k;
        }
    wl_keyboard_send_enter(r, wl_display_next_serial(display), superficie, &giu);
    wl_array_release(&giu);
    ta_modificatori(r, wl_display_next_serial(display), modificatori);
}

static void ta_entra(struct finestra_wl *w) {
    PER_DEL_PROGRAMMA(r, &tastiere, w->cliente) ta_entra_in(r, w);
}

static void ta_esce(struct finestra_wl *w) {
    struct wl_resource *superficie = risorsa_superficie(w);
    if (!superficie) return;
    uint32_t serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &tastiere, w->cliente) wl_keyboard_send_leave(r, serial, superficie);
}

/* --- wl_seat --- */

static void seat_puntatore(struct wl_client *client, struct wl_resource *seat, uint32_t id) {
    struct wl_resource *r = wl_resource_create(client, &wl_pointer_interface, wl_resource_get_version(seat), id);
    if (!r) { wl_resource_post_no_memory(seat); return; }
    wl_resource_set_implementation(r, &puntatore_interfaccia, 0, risorsa_tolta);
    wl_list_insert(&puntatori, wl_resource_get_link(r));
    /* se il puntatore e' gia' dentro una sua finestra (l'ha chiesto dopo), lo si dice adesso */
    for (int i = 0; i < posti_wl; i++) {
        struct finestra_wl *w = per_posto[i];
        if (!w || w->cliente != client || !w->dentro || !w->focus) continue;
        w->serial_entra = wl_display_next_serial(display);
        wl_pointer_send_enter(r, w->serial_entra, w->focus->risorsa, wl_fixed_from_int(w->px), wl_fixed_from_int(w->py));
        pu_frame(r);
    }
}

static void seat_tastiera(struct wl_client *client, struct wl_resource *seat, uint32_t id) {
    if (keymap_fd < 0) {   /* senza keymap il seat non ha la tastiera (capacita' non detta) */
        wl_resource_post_error(seat, WL_SEAT_ERROR_MISSING_CAPABILITY, "il seat non ha la tastiera");
        return;
    }
    struct wl_resource *r = wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(seat), id);
    if (!r) { wl_resource_post_no_memory(seat); return; }
    wl_resource_set_implementation(r, &tastiera_interfaccia, 0, risorsa_tolta);
    wl_list_insert(&tastiere, wl_resource_get_link(r));
    wl_keyboard_send_keymap(r, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymap_fd, keymap_byte);
    if (wl_resource_get_version(r) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)   /* il ritmo: 30 ms = 33 al secondo */
        wl_keyboard_send_repeat_info(r, 1000 / TASTI_RIPETI_OGNI, TASTI_RIPETI_DOPO);
    struct finestra_wl *f = finestra_con_fuoco();   /* se ha gia' il fuoco, enter subito */
    if (f && f->cliente == client) ta_entra_in(r, f);
}

static void seat_tocco(struct wl_client *c, struct wl_resource *r, uint32_t id) {
    (void)c; (void)id;
    wl_resource_post_error(r, WL_SEAT_ERROR_MISSING_CAPABILITY, "il seat non ha lo schermo a tocco");
}
static void seat_rilascia(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static const struct wl_seat_interface seat_interfaccia = {
    .get_pointer = seat_puntatore,
    .get_keyboard = seat_tastiera,
    .get_touch = seat_tocco,
    .release = seat_rilascia,
};

static void lega_seat(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &wl_seat_interface, versione > 7 ? 7 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &seat_interfaccia, 0, 0);
    wl_seat_send_capabilities(r, WL_SEAT_CAPABILITY_POINTER | (keymap_fd >= 0 ? WL_SEAT_CAPABILITY_KEYBOARD : 0));
    if (wl_resource_get_version(r) >= WL_SEAT_NAME_SINCE_VERSION) wl_seat_send_name(r, "seat0");
}

/* --- wl_output: lo schermo ---
 * Uno schermo solo, che non cambia: a chi lo chiede si dice subito tutto e poi "done".
 * geometry: posizione 0,0 e misura fisica sconosciuta (0 x 0 mm, come le macchine virtuali);
 * mode: la misura in pixel, a 60 Hz (il ritmo dei fotogrammi, wayland_fotogramma);
 * scale 1 (nessun ingrandimento); name e description (versione 4). */
static void output_rilascia(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static const struct wl_output_interface output_interfaccia = { .release = output_rilascia };

static void lega_output(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &wl_output_interface, versione > 4 ? 4 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &output_interfaccia, 0, 0);
    int l, a;
    ponte_schermo(&l, &a);
    wl_output_send_geometry(r, 0, 0, 0, 0, WL_OUTPUT_SUBPIXEL_UNKNOWN, "mshell", "schermo",
                            WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(r, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, l, a, 60000);
    int v = wl_resource_get_version(r);
    if (v >= WL_OUTPUT_SCALE_SINCE_VERSION) wl_output_send_scale(r, 1);
    if (v >= WL_OUTPUT_NAME_SINCE_VERSION) wl_output_send_name(r, "schermo-1");
    if (v >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION) wl_output_send_description(r, "lo schermo di mshell");
    if (v >= WL_OUTPUT_DONE_SINCE_VERSION) wl_output_send_done(r);
}

/* --- xdg-decoration: chi disegna la barra del titolo ---
 *
 * Il programma crea un oggetto decoration per il suo toplevel e chiede client_side (la barra la
 * disegna lui) o server_side (la nostra cornice), oppure toglie la richiesta (unset_mode). Si
 * risponde sempre con un configure del modo, e si fa quello che chiede: senza oggetto decoration
 * il modo e' server_side, cioe' com'era prima. */

static void deco_configura(struct finestra_wl *w) {
    if (w->toplevel && w->configurata) invia_configure(w);   /* se no, lo porta il primo configure */
}

static void deco_modo(struct wl_client *c, struct wl_resource *r, uint32_t modo) {
    (void)c;
    if (modo != ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE && modo != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
        wl_resource_post_error(r, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE, "modo sconosciuto");
        return;
    }
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w) return;
    w->csd_voluto = modo == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    deco_configura(w);
}

static void deco_senza_modo(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w) return;
    w->csd_voluto = 0;                       /* nessuna preferenza: la nostra cornice */
    deco_configura(w);
}

static void deco_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void deco_distrutta(struct wl_resource *r) {
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (!w) return;
    w->decorazione = 0;
    w->csd_voluto = 0;                       /* senza oggetto: server_side, come oggi */
}

static const struct zxdg_toplevel_decoration_v1_interface deco_interfaccia = {
    .destroy = deco_distruggi,
    .set_mode = deco_modo,
    .unset_mode = deco_senza_modo,
};

static void decorazioni_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void decorazioni_prendi(struct wl_client *client, struct wl_resource *r, uint32_t id, struct wl_resource *toplevel) {
    struct finestra_wl *w = wl_resource_get_user_data(toplevel);
    struct wl_resource *d = wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface, wl_resource_get_version(r), id);
    if (!d) { wl_resource_post_no_memory(r); return; }
    if (w && w->decorazione) {
        wl_resource_post_error(r, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED, "il toplevel ha gia' una decorazione");
        return;
    }
    wl_resource_set_implementation(d, &deco_interfaccia, w, deco_distrutta);
    if (!w) return;
    w->decorazione = d;
    deco_configura(w);                       /* se il primo configure e' gia' partito, il modo va detto adesso */
}

static const struct zxdg_decoration_manager_v1_interface decorazioni_interfaccia = {
    .destroy = decorazioni_distruggi,
    .get_toplevel_decoration = decorazioni_prendi,
};

static void lega_decorazioni(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &zxdg_decoration_manager_v1_interface, versione > 1 ? 1 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &decorazioni_interfaccia, 0, 0);
}

/* --- cursor-shape: la forma del puntatore, per nome ---
 *
 * Il programma sceglie una forma del puntatore con un nome (default, text, pointer...), citando il
 * numero di serie dell'enter. Il compositore la mostra mentre il puntatore sta su quella finestra,
 * come MOSTRA_PUNTATORE delle finestre nostre: le forme che abbiamo sono la freccia, la mano, la
 * barra a I, il divieto e l'attesa (FORMA_..., gkit.h); le altre, la freccia. */

static unsigned int forma_di(uint32_t nome) {
    switch (nome) {
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER: return FORMA_COLLEGAMENTO;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT:
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT: return FORMA_TESTO;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT:
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS: return FORMA_ATTESA;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED:
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP: return FORMA_NON_DISPONIBILE;
    default: return FORMA_NORMALE;           /* default e tutte le altre */
    }
}

/* Il dispositivo del puntatore ha come dato il suo wl_pointer; quello della penna (tablet) nessuno,
 * e per la penna non facciamo niente. */
static void forma_scegli(struct wl_client *c, struct wl_resource *r, uint32_t serial, uint32_t nome) {
    if (!wl_resource_get_user_data(r)) return;
    for (int i = 0; i < posti_wl; i++) {
        struct finestra_wl *w = per_posto[i];
        if (!w || w->cliente != c || !w->dentro || w->serial_entra != serial) continue;
        ponte_forma(i, forma_di(nome));      /* una forma sconosciuta e' la freccia */
        return;
    }
}

static void forma_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static const struct wp_cursor_shape_device_v1_interface forma_interfaccia = {
    .destroy = forma_distruggi,
    .set_shape = forma_scegli,
};

static void forme_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void forme_dispositivo(struct wl_client *client, struct wl_resource *r, uint32_t id, void *dato) {
    struct wl_resource *d = wl_resource_create(client, &wp_cursor_shape_device_v1_interface, wl_resource_get_version(r), id);
    if (!d) { wl_resource_post_no_memory(r); return; }
    wl_resource_set_implementation(d, &forma_interfaccia, dato, 0);
}

static void forme_puntatore(struct wl_client *c, struct wl_resource *r, uint32_t id, struct wl_resource *puntatore) {
    forme_dispositivo(c, r, id, puntatore);
}

static void forme_penna(struct wl_client *c, struct wl_resource *r, uint32_t id, struct wl_resource *penna) {
    (void)penna;
    forme_dispositivo(c, r, id, 0);          /* non c'e' nessuna penna: il dispositivo c'e' ma non fa niente */
}

static const struct wp_cursor_shape_manager_v1_interface forme_interfaccia = {
    .destroy = forme_distruggi,
    .get_pointer = forme_puntatore,
    .get_tablet_tool_v2 = forme_penna,
};

static void lega_forme(struct wl_client *client, void *dati, uint32_t versione, uint32_t id) {
    (void)dati;
    struct wl_resource *r = wl_resource_create(client, &wp_cursor_shape_manager_v1_interface, versione > 1 ? 1 : (int)versione, id);
    if (!r) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(r, &forme_interfaccia, 0, 0);
}

/* --- quello che vede il compositore --- */

int wayland_inizia(void) {
    wl_list_init(&superfici);
    wl_list_init(&puntatori);
    wl_list_init(&tastiere);
    wl_list_init(&basi);
    carica_keymap();
    /* wl_display_add_socket cerca la cartella in XDG_RUNTIME_DIR */
    setenv("XDG_RUNTIME_DIR", CARTELLA_SOCKET, 1);
    display = wl_display_create();
    if (!display) return -1;
    if (wl_display_add_socket(display, NOME_SOCKET) < 0 ||
        !wl_global_create(display, &wl_compositor_interface, 4, 0, lega_compositor) ||
        !wl_global_create(display, &wl_subcompositor_interface, 1, 0, lega_subcompositor) ||
        wl_display_init_shm(display) < 0 ||   /* wl_shm, con ARGB8888 e XRGB8888 */
        !wl_global_create(display, &xdg_wm_base_interface, 3, 0, lega_wm) ||
        !wl_global_create(display, &wl_seat_interface, 7, 0, lega_seat) ||
        !wl_global_create(display, &wl_output_interface, 4, 0, lega_output) ||
        !wl_global_create(display, &zxdg_decoration_manager_v1_interface, 1, 0, lega_decorazioni) ||
        !wl_global_create(display, &wp_cursor_shape_manager_v1_interface, 1, 0, lega_forme)) {
        wl_display_destroy(display);
        display = 0;
        return -1;
    }
    return wl_event_loop_get_fd(wl_display_get_event_loop(display));
}

/* LA CONNESSIONE DI UN'APP, dal launcher (PORTA_WAYLAND): il capo 'd' diventa un client del
 * server, e il programma dell'app gli resta attaccato (wl_client_set_user_data): e' quello che
 * dice il launcher, non il kernel, come per la porta del protocollo nostro. */
int wayland_cliente(int d, const char *programma) {
    if (!display || d < 0) return -1;
    char *copia = strdup(programma);
    if (!copia) return -1;
    struct wl_client *c = wl_client_create(display, d);   /* da qui il descrittore e' suo */
    if (!c) { free(copia); return -1; }
    wl_client_set_user_data(c, copia, free);
    return 0;
}

void wayland_servi(void) {
    if (!display) return;
    wl_event_loop_dispatch(wl_display_get_event_loop(display), 0);
    wl_display_flush_clients(display);
}

void wayland_notizia(int i, const struct notizia *n) {
    struct finestra_wl *w = al_posto(i);
    if (!w) return;
    switch (n->tipo) {
    case FUOCO:
        if (w->attiva == (int)(n->fuoco != 0)) return;
        w->attiva = n->fuoco != 0;
        if (!w->csd) ridisegna(w);            /* il titolo e i comandi cambiano (se la barra e' nostra) */
        if (w->toplevel && w->configurata) invia_configure(w);   /* e il programma lo sa */
        if (w->attiva) ta_entra(w);           /* il fuoco della tastiera: enter con i tasti gia' giu' */
        else ta_esce(w);
        break;
    case PUNTATORE: pu_puntatore(w, n->x, n->y, n->pulsanti); break;
    case USCITO:
        if (w->dentro) pu_esce(w);
        w->ignorati = w->visti;               /* i clic ancora tenuti il programma non li sa piu' */
        w->consegnati = 0;
        break;
    case ROTELLA: pu_rotella(w, n->x, n->y, n->scatti_x, n->scatti_y); break;
    case PING: {                              /* "ci sei?": xdg_wm_base.ping, il pong torna da wm_pong */
        w->ping_serial = n->numero ? n->numero : 1;
        PER_DEL_PROGRAMMA(r, &basi, w->cliente) { xdg_wm_base_send_ping(r, w->ping_serial); break; }
        break;
    }
    default: return;                          /* TASTO e TASTO_FISICO: ci sono i tasti grezzi */
    }
    wl_display_flush_clients(display);
}

/* Un tasto grezzo. Lo stato (tasti giu', modificatori) si tiene sempre, anche senza fuoco: il
 * programma che lo prende ne ha bisogno nell'enter. Al fuoco: key e, se i modificatori sono
 * cambiati, modifiers subito dopo (come fa weston). */
void wayland_tasto(unsigned int codice, int valore, unsigned int mod) {
    if (!display || valore == 2 || codice > KEY_MAX) return;   /* la ripetizione la fa il programma */
    if (valore) tasti_giu[codice / 8] |= (unsigned char)(1u << (codice % 8));
    else tasti_giu[codice / 8] &= (unsigned char)~(1u << (codice % 8));
    unsigned int prima = modificatori;
    modificatori = mod;
    struct finestra_wl *w = finestra_con_fuoco();
    if (!w) return;
    uint32_t t = tempo(), serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &tastiere, w->cliente) {
        wl_keyboard_send_key(r, serial, t, codice, valore ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
        if (mod != prima) ta_modificatori(r, wl_display_next_serial(display), mod);
    }
    wl_display_flush_clients(display);
}

void wayland_termina(int i) {
    struct finestra_wl *w = al_posto(i);
    if (!w || !w->cliente) return;
    wl_client_destroy(w->cliente);    /* le sue finestre spariscono (toplevel_distrutto) */
}

void wayland_chiedi_chiusura(int i) {
    struct finestra_wl *w = al_posto(i);
    if (!w || !w->toplevel) return;
    xdg_toplevel_send_close(w->toplevel);
    wl_display_flush_clients(display);
}

/* Una superficie aspetta il prossimo fotogramma e si vede (la finestra ridotta non conta:
 * non si disegna cio' che nessuno guarda). */
static int aspetta_visibile(struct superficie *s) {
    if (wl_list_empty(&s->da_servire)) return 0;
    struct finestra_wl *w = capostipite(s)->finestra;   /* una sottosuperficie e' visibile come la sua finestra */
    if (w && w->slot >= 0 && !ponte_visibile(w->slot)) return 0;
    return 1;
}

int wayland_vuole_fotogramma(void) {
    if (!display) return 0;
    struct superficie *s;
    wl_list_for_each(s, &superfici, link)
        if (aspetta_visibile(s)) return 1;
    return 0;
}

void wayland_fotogramma(unsigned int ms) {
    if (!display) return;
    struct superficie *s;
    wl_list_for_each(s, &superfici, link) {
        if (!aspetta_visibile(s)) continue;   /* ridotta: aspetta, riparte al tic dopo il ritorno */
        for (struct wl_list *p = s->da_servire.next, *n; p != &s->da_servire; p = n) {
            n = p->next;
            struct wl_resource *cb = wl_resource_from_link(p);
            wl_callback_send_done(cb, ms);
            wl_resource_destroy(cb);
        }
    }
    wl_display_flush_clients(display);
}
