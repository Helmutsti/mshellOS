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
 * LA CHIUSURA e' sempre una richiesta: la X manda xdg_toplevel.close e il programma decide (se
 * chiude, il socket si chiude e la finestra sparisce).
 *
 * IL PASSO 2 e' wl_seat: il puntatore e la tastiera (vedi "wl_seat", piu' sotto).
 *
 * COSA NON C'E' ANCORA: wl_output, i popup, il ridimensionamento fatto dalla persona, i ping ai
 * programmi, la forma del cursore scelta dal programma, gli appunti (data_device). */
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

#include "compositore_wayland.h"
#include "gkit/gkit_disegno.h"
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

/* Una SUPERFICIE: quello che il programma disegna. 'in_attesa': le richieste frame fatte dopo
 * l'ultimo commit; 'da_servire': quelle gia' confermate da un commit, che aspettano il prossimo
 * fotogramma del compositore. */
struct finestra_wl;
struct superficie {
    struct wl_resource *risorsa;
    struct wl_resource *buffer;          /* l'ultimo attach, finche' non c'e' il commit */
    struct wl_listener ascolto_buffer;   /* se il programma distrugge il buffer prima del commit */
    int ha_attach;                       /* c'e' stato un attach dall'ultimo commit */
    struct wl_list in_attesa, da_servire;
    struct wl_list link;
    struct finestra_wl *finestra;        /* il suo ruolo (xdg_surface), o 0 */
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
    /* il puntatore (vedi "wl_seat", piu' sotto) */
    int dentro;                          /* il puntatore e' entrato nella superficie (wl_pointer.enter) */
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
    int alta = cornice_testata_altezza(), schermo_l, schermo_a;
    ponte_schermo(&schermo_l, &schermo_a);
    int l = w->cl > LARGHEZZA_MINIMA ? w->cl : LARGHEZZA_MINIMA, a = alta + w->ch;
    if (l > schermo_l) l = schermo_l;      /* piu' grande dello schermo: si taglia */
    if (a > schermo_a) a = schermo_a;
    if (!w->immagine || w->l != l || w->a != a) {
        free(w->immagine);
        w->immagine = malloc((size_t)l * (size_t)a * sizeof(unsigned int));
        w->l = l;
        w->a = a;
        if (!w->immagine) return;
    }
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
    if (ponte_mostra(w->slot, w->immagine, l, a, w->titolo) < 0) {
        /* memoria finita: il compositore ha tolto la finestra. Il posto non e' piu' nostro (se
         * no un'altra finestra lo riuserebbe e ponte_togli toglierebbe lei); si chiede al
         * programma di chiudersi. */
        segna_posto(w->slot, 0);
        w->slot = -1;
        if (w->toplevel) xdg_toplevel_send_close(w->toplevel);
    }
}

/* Il contenuto di un buffer shm in 'contenuto' (rifatto se la misura e' cambiata). 1 se fatto.
 * Il buffer si legge dentro wl_shm_buffer_begin_access: se il programma accorcia il foglio
 * mentre si legge, la libreria lo evita (niente SIGBUS). */
static int copia_buffer(struct finestra_wl *w, struct wl_resource *buffer) {
    struct wl_shm_buffer *shm = wl_shm_buffer_get(buffer);
    if (!shm) return 0;
    uint32_t formato = wl_shm_buffer_get_format(shm);
    int l = wl_shm_buffer_get_width(shm), a = wl_shm_buffer_get_height(shm);
    int passo = wl_shm_buffer_get_stride(shm);
    int schermo_l, schermo_a;
    ponte_schermo(&schermo_l, &schermo_a);
    if (l < 1 || a < 1 || l > schermo_l || a > schermo_a) return 0;
    if (w->cl != l || w->ch != a || !w->contenuto) {
        free(w->contenuto);
        w->contenuto = malloc((size_t)l * (size_t)a * sizeof(unsigned int));
        w->cl = l;
        w->ch = a;
        if (!w->contenuto) return 0;
    }
    unsigned int opaco = formato == WL_SHM_FORMAT_XRGB8888 ? 0xFF000000u : 0;   /* ARGB: ha il suo */
    wl_shm_buffer_begin_access(shm);
    const unsigned char *dati = wl_shm_buffer_get_data(shm);
    for (int r = 0; r < a; r++) {
        const unsigned int *riga = (const unsigned int *)(const void *)(dati + (size_t)r * (size_t)passo);
        for (int c = 0; c < l; c++) w->contenuto[r * l + c] = riga[c] | opaco;
    }
    wl_shm_buffer_end_access(shm);
    return 1;
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

/* Si ignorano: il genitore, l'id dell'app, i limiti di misura, massimizza, schermo intero,
 * riduci (xdg-shell permette al compositore di ignorarle). move, resize e show_window_menu
 * hanno bisogno di un wl_seat (un clic con il suo numero di serie): senza seat un programma non
 * le puo' chiamare, e non ci sono (se le chiama, la libreria lo disconnette: "listener
 * function is NULL"). */
static const struct xdg_toplevel_interface toplevel_interfaccia = {
    .destroy = toplevel_distruggi,
    .set_parent = toplevel_con_risorsa,
    .set_title = toplevel_titolo,
    .set_app_id = toplevel_con_testo,
    .show_window_menu = 0,
    .move = 0,
    .resize = 0,
    .set_max_size = toplevel_con_misura,
    .set_min_size = toplevel_con_misura,
    .set_maximized = toplevel_senza,
    .unset_maximized = toplevel_senza,
    .set_fullscreen = toplevel_con_risorsa,
    .unset_fullscreen = toplevel_senza,
    .set_minimized = toplevel_senza,
};

/* --- xdg_surface --- */

static void xdg_surface_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }

static void xdg_surface_toplevel(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct finestra_wl *w = wl_resource_get_user_data(r);
    if (w->toplevel) {
        wl_resource_post_error(r, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED, "e' gia' una finestra");
        return;
    }
    int posto = ponte_apri(wl_client_get_fd(client));
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
    if (s->finestra) {
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

static void wm_pong(struct wl_client *c, struct wl_resource *r, uint32_t serial) { (void)c; (void)r; (void)serial; }

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
    wl_resource_set_implementation(r, &wm_interfaccia, 0, 0);
}

/* --- wl_surface --- */

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

static void sup_regione(struct wl_client *c, struct wl_resource *r, struct wl_resource *regione) {
    (void)c; (void)r; (void)regione;   /* l'opacita' e dove si clicca: non servono (niente seat) */
}

static void sup_commit(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct superficie *s = wl_resource_get_user_data(r);
    struct finestra_wl *w = s->finestra;
    /* le richieste di "avvisami del prossimo fotogramma" fatte fin qui valgono adesso */
    wl_list_insert_list(s->da_servire.prev, &s->in_attesa);
    wl_list_init(&s->in_attesa);

    struct wl_resource *buffer = s->buffer;
    int con_buffer = s->ha_attach;
    sup_senza_buffer(s);
    s->ha_attach = 0;
    if (!w || w->slot < 0) {              /* senza ruolo di finestra: niente da mostrare */
        if (buffer) wl_buffer_send_release(buffer);
        return;
    }
    if (!w->configurata) {                /* il primo commit, senza contenuto: il configure iniziale */
        if (buffer) wl_buffer_send_release(buffer);
        invia_configure(w);
        return;
    }
    if (!con_buffer) return;              /* solo frame o stato: l'immagine e' quella */
    if (buffer && copia_buffer(w, buffer)) ridisegna(w);
    if (buffer) wl_buffer_send_release(buffer);   /* gia' copiato: il programma lo puo' riusare */
}

static void sup_trasformazione(struct wl_client *c, struct wl_resource *r, int32_t t) { (void)c; (void)r; (void)t; }
static void sup_scala(struct wl_client *c, struct wl_resource *r, int32_t s) { (void)c; (void)r; (void)s; }
static void sup_scostamento(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y) { (void)c; (void)r; (void)x; (void)y; }

static const struct wl_surface_interface sup_interfaccia = {
    .destroy = sup_distruggi,
    .attach = sup_attach,
    .damage = sup_danno,
    .frame = sup_frame,
    .set_opaque_region = sup_regione,
    .set_input_region = sup_regione,
    .commit = sup_commit,
    .set_buffer_transform = sup_trasformazione,
    .set_buffer_scale = sup_scala,
    .damage_buffer = sup_danno,
    .offset = sup_scostamento,
};

static void sup_distrutta(struct wl_resource *r) {
    struct superficie *s = wl_resource_get_user_data(r);
    sup_senza_buffer(s);
    struct wl_list *liste[2] = { &s->in_attesa, &s->da_servire };
    for (int k = 0; k < 2; k++) {
        struct wl_list *l = liste[k];
        for (struct wl_list *p = l->next, *n; p != l; p = n) {
            n = p->next;
            wl_resource_destroy(wl_resource_from_link(p));   /* si toglie da solo dalla lista */
        }
    }
    if (s->finestra) s->finestra->superficie = 0;
    wl_list_remove(&s->link);
    free(s);
}

/* --- wl_region (non serve a niente oggi, ma i programmi la creano) --- */

static void regione_distruggi(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static void regione_rettangolo(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t l, int32_t a) {
    (void)c; (void)r; (void)x; (void)y; (void)l; (void)a;
}
static const struct wl_region_interface regione_interfaccia = {
    .destroy = regione_distruggi,
    .add = regione_rettangolo,
    .subtract = regione_rettangolo,
};

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
    wl_list_insert(&superfici, &s->link);
    wl_resource_set_implementation(x, &sup_interfaccia, s, sup_distrutta);
}

static void comp_regione(struct wl_client *client, struct wl_resource *r, uint32_t id) {
    struct wl_resource *x = wl_resource_create(client, &wl_region_interface, wl_resource_get_version(r), id);
    if (!x) { wl_resource_post_no_memory(r); return; }
    wl_resource_set_implementation(x, &regione_interfaccia, 0, 0);
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

/* La posizione sulla superficie; 1 se e' dentro il contenuto (non sulla cornice). */
static int sulla_superficie(const struct finestra_wl *w, int x, int y, int *sx, int *sy) {
    *sx = x;
    *sy = y - cornice_testata_altezza();
    return *sx >= 0 && *sx < w->cl && *sy >= 0 && *sy < w->ch;
}

static void pu_entra(struct finestra_wl *w, int sx, int sy) {
    struct wl_resource *superficie = risorsa_superficie(w);
    if (!superficie) return;
    uint32_t serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) {
        wl_pointer_send_enter(r, serial, superficie, wl_fixed_from_int(sx), wl_fixed_from_int(sy));
        pu_frame(r);
    }
    w->dentro = 1;
    w->px = sx;
    w->py = sy;
}

static void pu_esce(struct finestra_wl *w) {
    struct wl_resource *superficie = risorsa_superficie(w);
    w->dentro = 0;
    if (!superficie) return;
    uint32_t serial = wl_display_next_serial(display);
    PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) {
        wl_pointer_send_leave(r, serial, superficie);
        pu_frame(r);
    }
}

static uint32_t codice_pulsante(unsigned int bit) {
    return bit == PULSANTE_SINISTRO ? BTN_LEFT : bit == PULSANTE_DESTRO ? BTN_RIGHT : BTN_MIDDLE;
}

/* Il puntatore e' a (x, y) della finestra (cornice compresa) con questi pulsanti giu'. */
static void pu_puntatore(struct finestra_wl *w, int x, int y, unsigned int pulsanti) {
    int sx, sy;
    int dentro = sulla_superficie(w, x, y, &sx, &sy);
    unsigned int nuovi = pulsanti & ~w->visti, lasciati = w->visti & ~pulsanti;
    w->visti = pulsanti;
    w->ignorati &= pulsanti;                  /* un clic cominciato fuori e finito: non c'e' piu' */
    int appena_entrato = 0;
    if (!w->dentro) {
        if (dentro && !w->ignorati) { pu_entra(w, sx, sy); appena_entrato = 1; }
        else { w->ignorati |= nuovi; nuovi = 0; }   /* premuto sulla cornice: non e' del programma */
    }
    if (!w->dentro) return;
    uint32_t t = tempo();
    /* fuori dal contenuto si muove solo se c'e' un clic tenuto (la "presa"); se no e' un leave */
    int cambia = (sx != w->px || sy != w->py) && (dentro || w->consegnati);
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
        PER_DEL_PROGRAMMA(r, &puntatori, w->cliente)
            wl_pointer_send_button(r, serial, t, codice_pulsante(bit),
                                   premuto ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
        w->consegnati = premuto ? w->consegnati | bit : w->consegnati & ~bit;
        detto = 1;
    }
    if (detto) PER_DEL_PROGRAMMA(r, &puntatori, w->cliente) pu_frame(r);
    if (!dentro && !w->consegnati) pu_esce(w);   /* uscito dal contenuto e nessun clic tenuto */
}

/* La rotella: scatti verticali e orizzontali (positivi: in basso e a destra) sul punto (x, y). */
static void pu_rotella(struct finestra_wl *w, int x, int y, int scatti_x, int scatti_y) {
    int sx, sy;
    if (!sulla_superficie(w, x, y, &sx, &sy)) return;
    int ospite = !w->dentro;                  /* senza fuoco il puntatore non c'e': entra solo per girare */
    if (ospite) pu_entra(w, sx, sy);
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
        if (!w || w->cliente != client || !w->dentro || !risorsa_superficie(w)) continue;
        wl_pointer_send_enter(r, wl_display_next_serial(display), risorsa_superficie(w),
                              wl_fixed_from_int(w->px), wl_fixed_from_int(w->py));
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

/* --- quello che vede il compositore --- */

int wayland_inizia(void) {
    wl_list_init(&superfici);
    wl_list_init(&puntatori);
    wl_list_init(&tastiere);
    carica_keymap();
    /* wl_display_add_socket cerca la cartella in XDG_RUNTIME_DIR */
    setenv("XDG_RUNTIME_DIR", CARTELLA_SOCKET, 1);
    display = wl_display_create();
    if (!display) return -1;
    if (wl_display_add_socket(display, NOME_SOCKET) < 0 ||
        !wl_global_create(display, &wl_compositor_interface, 4, 0, lega_compositor) ||
        wl_display_init_shm(display) < 0 ||   /* wl_shm, con ARGB8888 e XRGB8888 */
        !wl_global_create(display, &xdg_wm_base_interface, 3, 0, lega_wm) ||
        !wl_global_create(display, &wl_seat_interface, 7, 0, lega_seat)) {
        wl_display_destroy(display);
        display = 0;
        return -1;
    }
    return wl_event_loop_get_fd(wl_display_get_event_loop(display));
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
        ridisegna(w);                         /* il titolo e i comandi cambiano */
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

void wayland_chiedi_chiusura(int i) {
    struct finestra_wl *w = al_posto(i);
    if (!w || !w->toplevel) return;
    xdg_toplevel_send_close(w->toplevel);
    wl_display_flush_clients(display);
}

/* Una superficie aspetta il prossimo fotogramma e si vede (la finestra ridotta non conta:
 * non si disegna cio' che nessuno guarda). */
static int aspetta_visibile(const struct superficie *s) {
    if (wl_list_empty(&s->da_servire)) return 0;
    if (s->finestra && s->finestra->slot >= 0 && !ponte_visibile(s->finestra->slot)) return 0;
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
