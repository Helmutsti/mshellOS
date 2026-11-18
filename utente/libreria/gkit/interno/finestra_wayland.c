/* GKit: la finestra normale come CLIENT WAYLAND. Il programma ha UNA connessione (quella che il
 * launcher gli da' al descrittore 9, con WAYLAND_SOCKET: wl_display_connect la usa), e ogni
 * finestra e' una wl_surface con il suo xdg_toplevel. L'API di GKit (gkit.h) resta quella di
 * sempre: gli eventi di Wayland diventano le stesse notizie (struct notizia) e vanno nella coda
 * della finestra giusta (finestra.messe_da_parte).
 *
 *   - La barra del titolo la disegna l'app (UIKit): si chiede la decorazione "client_side" se il
 *     compositore ha zxdg_decoration_manager_v1.
 *   - Le due immagini sono due wl_buffer (ARGB8888) di un pool wl_shm sullo stesso foglio memfd;
 *     wl_buffer.release dice che l'immagine e' tornata libera.
 *   - La tastiera: il compositore da' solo codice del tasto e modificatori; il carattere lo fa
 *     la tabella italiana comune (corekit/tastiera_it.h), e la RIPETIZIONE dei tasti tenuti giu'
 *     la fa questo client (secondo repeat_info), con una scadenza che il ciclo degli eventi
 *     serve (finestra_scadenza_ms, finestra_servi_scadenze).
 *   - Spostare la finestra: xdg_toplevel.move; ridurla: set_minimized; la forma del puntatore:
 *     wp_cursor_shape (se il compositore l'ha); il "ci sei?" (ping) si risponde da solo.
 * Quello che il compositore non ha (decorazioni, cursor shape) si salta senza errori. */
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"

#include "corekit/corekit.h"
#include "corekit/tastiera_it.h"
#include "gkit/gkit.h"
#include "gkit/interno/finestra_wayland.h"

#define MAX_FIN 16        /* finestre Wayland insieme in un programma */

/* Una finestra Wayland. */
struct fw;
struct fw_buffer { struct fw *w; int indice; };
struct fw {
    struct finestra *f;
    struct wl_surface *sup;
    struct xdg_surface *xdg;
    struct xdg_toplevel *top;
    struct zxdg_toplevel_decoration_v1 *deco;
    struct wl_buffer *buf[2];
    struct fw_buffer ib[2];
    int configurata;
    int tasti_fisici;
    unsigned int forma;
};

/* Lo stato del programma: la connessione, i globali, il puntatore e la tastiera. */
static struct {
    struct wl_display *display;
    struct wl_compositor *comp;
    struct wl_shm *shm;
    struct xdg_wm_base *base;
    struct wl_seat *seat;
    struct wl_output *uscita;
    struct zxdg_decoration_manager_v1 *deco;
    struct wp_cursor_shape_manager_v1 *cursore;
    struct wl_pointer *punt;
    struct wl_keyboard *tast;
    struct wp_cursor_shape_device_v1 *forma_dev;
    int seat_versione;
    int schermo_l, schermo_a;
    struct fw *fin[MAX_FIN];
    /* il puntatore */
    struct fw *sotto;               /* la finestra sotto il puntatore */
    uint32_t serial_entrata;        /* per la forma del puntatore */
    uint32_t serial_pulsante;       /* l'ultimo pulsante premuto (per xdg_toplevel.move) */
    unsigned int pulsanti;          /* PULSANTE_... tenuti premuti */
    int px, py;
    /* la tastiera */
    struct fw *fuoco;
    struct tastiera_it kb;
    int rip_ritmo, rip_attesa;      /* repeat_info: tasti al secondo (0: niente) e ms prima */
    int rip_attiva;
    unsigned int rip_codice, rip_scade;
} W;

static int stato = -2;              /* -2 da provare, -1 non c'e', 1 pronta */

static struct fw *finestra_di(struct wl_surface *s) {
    if (!s) return 0;
    for (int i = 0; i < MAX_FIN; i++)
        if (W.fin[i] && W.fin[i]->sup == s) return W.fin[i];
    return 0;
}

/* Mette una notizia nella coda della finestra. I movimenti del puntatore di fila (stessi
 * pulsanti) si fondono: il vecchio non serve piu'. Se la coda e' piena la notizia si perde. */
static void metti(struct fw *w, struct notizia n) {
    struct finestra *f = w->f;
    if (n.tipo == PUNTATORE && f->quante && f->messe_da_parte[f->quante - 1].tipo == PUNTATORE &&
        f->messe_da_parte[f->quante - 1].pulsanti == n.pulsanti) {
        f->messe_da_parte[f->quante - 1] = n;
        return;
    }
    if (f->quante < DA_PARTE) f->messe_da_parte[f->quante++] = n;
}

/* --- il puntatore --- */

static unsigned int forma_wp(unsigned int forma) {
    switch (forma) {
    case FORMA_COLLEGAMENTO:    return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER;
    case FORMA_TESTO:           return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
    case FORMA_NON_DISPONIBILE: return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED;
    case FORMA_ATTESA:          return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
    default:                    return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    }
}

static void applica_forma(struct fw *w) {
    if (!W.forma_dev && W.cursore && W.punt)
        W.forma_dev = wp_cursor_shape_manager_v1_get_pointer(W.cursore, W.punt);
    if (W.forma_dev && W.sotto == w)
        wp_cursor_shape_device_v1_set_shape(W.forma_dev, W.serial_entrata, forma_wp(w->forma));
}

static void notizia_puntatore(struct fw *w) {
    metti(w, (struct notizia){ .tipo = PUNTATORE, .x = W.px, .y = W.py, .pulsanti = W.pulsanti });
}

static void pu_entra(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p;
    struct fw *w = finestra_di(s);
    if (!w) return;
    W.sotto = w;
    W.serial_entrata = serial;
    W.px = wl_fixed_to_int(x);
    W.py = wl_fixed_to_int(y);
    applica_forma(w);
    notizia_puntatore(w);
}
static void pu_esce(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)p; (void)serial;
    struct fw *w = finestra_di(s);
    if (w && W.sotto == w) {
        metti(w, (struct notizia){ .tipo = USCITO });
        W.sotto = 0;
    }
    W.pulsanti = 0;
}
static void pu_muove(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p; (void)t;
    W.px = wl_fixed_to_int(x);
    W.py = wl_fixed_to_int(y);
    if (W.sotto) notizia_puntatore(W.sotto);
}
static void pu_pulsante(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t pulsante, uint32_t stato_) {
    (void)d; (void)p; (void)t;
    unsigned int bit = pulsante == BTN_LEFT ? PULSANTE_SINISTRO : pulsante == BTN_RIGHT ? PULSANTE_DESTRO
                     : pulsante == BTN_MIDDLE ? PULSANTE_CENTRALE : 0;
    if (stato_ == WL_POINTER_BUTTON_STATE_PRESSED) {
        W.pulsanti |= bit;
        W.serial_pulsante = serial;
    } else {
        W.pulsanti &= ~bit;
    }
    if (W.sotto) notizia_puntatore(W.sotto);
}
static void rotella(int asse, int scatti) {
    if (!W.sotto || !scatti) return;
    struct notizia n = { .tipo = ROTELLA, .x = W.px, .y = W.py };
    if (asse == WL_POINTER_AXIS_VERTICAL_SCROLL) n.scatti_y = scatti; else n.scatti_x = scatti;
    metti(W.sotto, n);
}
static void pu_asse(void *d, struct wl_pointer *p, uint32_t t, uint32_t asse, wl_fixed_t valore) {
    (void)d; (void)p; (void)t;
    if (W.seat_versione >= 5) return;          /* gli scatti li dice axis_discrete */
    int v = wl_fixed_to_int(valore);
    rotella((int)asse, v / 10 ? v / 10 : v > 0 ? 1 : v < 0 ? -1 : 0);
}
static void pu_asse_discreto(void *d, struct wl_pointer *p, uint32_t asse, int32_t scatti) {
    (void)d; (void)p;
    rotella((int)asse, scatti);
}
static void pu_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void pu_sorgente(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void pu_fine_asse(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { (void)d; (void)p; (void)t; (void)a; }
static const struct wl_pointer_listener puntatore_ascolto = {
    .enter = pu_entra, .leave = pu_esce, .motion = pu_muove, .button = pu_pulsante,
    .axis = pu_asse, .frame = pu_frame, .axis_source = pu_sorgente, .axis_stop = pu_fine_asse,
    .axis_discrete = pu_asse_discreto,
};

/* --- la tastiera --- */

/* I tasti che non si ripetono: i modificatori e i blocchi. */
static int e_modificatore(unsigned int c) {
    return c == KEY_LEFTCTRL || c == KEY_RIGHTCTRL || c == KEY_LEFTSHIFT || c == KEY_RIGHTSHIFT ||
           c == KEY_LEFTALT || c == KEY_RIGHTALT || c == KEY_LEFTMETA || c == KEY_RIGHTMETA ||
           c == KEY_CAPSLOCK || c == KEY_NUMLOCK || c == KEY_SCROLLLOCK;
}

/* Un tasto (valore 1 premuto, 0 lasciato, 2 ripetuto) alla finestra col fuoco: TASTO_FISICO
 * se l'app l'ha chiesto, poi TASTO con il carattere. */
static void tasto_a(struct fw *w, unsigned int codice, int valore) {
    unsigned int fisico, carattere;
    tastiera_it_tasto(&W.kb, codice, valore, &fisico, &carattere);
    if (w->tasti_fisici && fisico)
        metti(w, (struct notizia){ .tipo = TASTO_FISICO, .tasto = fisico, .premuto = (unsigned int)valore });
    if (carattere) metti(w, (struct notizia){ .tipo = TASTO, .tasto = carattere });
}

static void ta_keymap(void *d, struct wl_keyboard *k, uint32_t formato, int32_t fd, uint32_t byte) {
    (void)d; (void)k; (void)formato; (void)byte;
    close(fd);                         /* la tabella e' la nostra (tastiera_it), non la keymap */
}
static void ta_entra(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *giu) {
    (void)d; (void)k; (void)serial; (void)giu;
    struct fw *w = finestra_di(s);
    if (!w) return;
    W.fuoco = w;
    W.rip_attiva = 0;
    metti(w, (struct notizia){ .tipo = FUOCO, .fuoco = 1 });
}
static void ta_esce(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)k; (void)serial;
    struct fw *w = finestra_di(s);
    W.rip_attiva = 0;
    if (!w) return;
    if (W.fuoco == w) W.fuoco = 0;
    metti(w, (struct notizia){ .tipo = FUOCO, .fuoco = 0 });
}
static void ta_tasto(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t t, uint32_t tasto, uint32_t stato_) {
    (void)d; (void)k; (void)serial; (void)t;
    if (!W.fuoco) return;
    int premuto = stato_ == WL_KEYBOARD_KEY_STATE_PRESSED;
    tasto_a(W.fuoco, tasto, premuto);
    if (premuto && !e_modificatore(tasto) && W.rip_ritmo > 0) {
        W.rip_attiva = 1;
        W.rip_codice = tasto;
        W.rip_scade = tempo() + (unsigned int)W.rip_attesa;
    } else if (!premuto && W.rip_attiva && W.rip_codice == tasto) {
        W.rip_attiva = 0;
    }
}
/* Le maschere XKB di modificatori (le stesse in ogni keymap: Shift 0, Lock 1, Control 2, Mod1 3,
 * Mod2 4, Mod4 6, Mod5 7) nei bit GREZZO_ (come le manda il compositore, compositore_wayland.c). */
static void ta_modificatori(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t premuti, uint32_t agganciati,
                            uint32_t bloccati, uint32_t gruppo) {
    (void)d; (void)k; (void)serial; (void)gruppo;
    uint32_t p = premuti | agganciati;
    unsigned int g = (p & (1u << 0) ? GREZZO_MAIUSC : 0) | (p & (1u << 2) ? GREZZO_CTRL : 0) |
                     (p & (1u << 3) ? GREZZO_ALT : 0) | (p & (1u << 6) ? GREZZO_META : 0) |
                     (p & (1u << 7) ? GREZZO_ALTGR : 0) | (bloccati & (1u << 1) ? GREZZO_BLOC_MAIUSC : 0) |
                     (bloccati & (1u << 4) ? GREZZO_BLOC_NUM : 0);
    tastiera_it_modificatori(&W.kb, g);
}
static void ta_ripetizione(void *d, struct wl_keyboard *k, int32_t ritmo, int32_t attesa) {
    (void)d; (void)k;
    W.rip_ritmo = ritmo;
    W.rip_attesa = attesa;
}
static const struct wl_keyboard_listener tastiera_ascolto = {
    .keymap = ta_keymap, .enter = ta_entra, .leave = ta_esce, .key = ta_tasto,
    .modifiers = ta_modificatori, .repeat_info = ta_ripetizione,
};

int fw_scadenza_ms(void) {
    if (!W.rip_attiva) return -1;
    int d = (int)(W.rip_scade - tempo());
    return d < 0 ? 0 : d;
}

void fw_servi_scadenze(void) {
    int giri = 0;
    while (W.rip_attiva && W.fuoco && W.rip_ritmo > 0 && (int)(tempo() - W.rip_scade) >= 0) {
        tasto_a(W.fuoco, W.rip_codice, 2);
        unsigned int passo = (unsigned int)(1000 / W.rip_ritmo);
        W.rip_scade += passo ? passo : 1;
        if (++giri >= 8) { W.rip_scade = tempo() + (passo ? passo : 1); break; }   /* in ritardo: non si recupera */
    }
}

/* --- il seat, lo schermo, xdg --- */

static void seat_capacita(void *d, struct wl_seat *st, uint32_t capacita) {
    (void)d;
    if ((capacita & WL_SEAT_CAPABILITY_POINTER) && !W.punt) {
        W.punt = wl_seat_get_pointer(st);
        wl_pointer_add_listener(W.punt, &puntatore_ascolto, 0);
    }
    if ((capacita & WL_SEAT_CAPABILITY_KEYBOARD) && !W.tast) {
        W.tast = wl_seat_get_keyboard(st);
        wl_keyboard_add_listener(W.tast, &tastiera_ascolto, 0);
    }
}
static void seat_nome(void *d, struct wl_seat *st, const char *nome) { (void)d; (void)st; (void)nome; }
static const struct wl_seat_listener seat_ascolto = { seat_capacita, seat_nome };

static void output_geometria(void *d, struct wl_output *o, int32_t x, int32_t y, int32_t mm_l, int32_t mm_a,
                             int32_t subpixel, const char *marca, const char *modello, int32_t trasformazione) {
    (void)d; (void)o; (void)x; (void)y; (void)mm_l; (void)mm_a; (void)subpixel; (void)marca; (void)modello;
    (void)trasformazione;
}
static void output_modo(void *d, struct wl_output *o, uint32_t flag, int32_t l, int32_t a, int32_t mhz) {
    (void)d; (void)o; (void)mhz;
    if (flag & WL_OUTPUT_MODE_CURRENT) { W.schermo_l = l; W.schermo_a = a; }
}
static void output_fatto(void *d, struct wl_output *o) { (void)d; (void)o; }
static void output_scala(void *d, struct wl_output *o, int32_t s) { (void)d; (void)o; (void)s; }
static void output_nome(void *d, struct wl_output *o, const char *n) { (void)d; (void)o; (void)n; }
static void output_descrizione(void *d, struct wl_output *o, const char *n) { (void)d; (void)o; (void)n; }
static const struct wl_output_listener output_ascolto = {
    output_geometria, output_modo, output_fatto, output_scala, output_nome, output_descrizione,
};

static void ping(void *dati, struct xdg_wm_base *b, uint32_t serial) { (void)dati; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener base_ascolto = { ping };

static void xdg_configure(void *dati, struct xdg_surface *xdg, uint32_t serial) {
    struct fw *w = dati;
    xdg_surface_ack_configure(xdg, serial);
    w->configurata = 1;
}
static const struct xdg_surface_listener xdg_ascolto = { xdg_configure };

static void top_configure(void *d, struct xdg_toplevel *t, int32_t l, int32_t a, struct wl_array *stati) {
    (void)d; (void)t; (void)l; (void)a; (void)stati;   /* la misura la decide l'app; il fuoco viene dalla tastiera */
}
static void top_chiudi(void *dati, struct xdg_toplevel *t) {
    (void)t;
    metti(dati, (struct notizia){ .tipo = CHIUDITI });
}
static void top_limiti(void *d, struct xdg_toplevel *t, int32_t l, int32_t a) { (void)d; (void)t; (void)l; (void)a; }
static void top_capacita(void *d, struct xdg_toplevel *t, struct wl_array *c) { (void)d; (void)t; (void)c; }
static const struct xdg_toplevel_listener top_ascolto = { top_configure, top_chiudi, top_limiti, top_capacita };

static void deco_configure(void *d, struct zxdg_toplevel_decoration_v1 *z, uint32_t modo) { (void)d; (void)z; (void)modo; }
static const struct zxdg_toplevel_decoration_v1_listener deco_ascolto = { deco_configure };

static void buffer_rilascio(void *dati, struct wl_buffer *b) {
    (void)b;
    struct fw_buffer *ib = dati;
    ib->w->f->libera[ib->indice] = 1;
}
static const struct wl_buffer_listener buffer_ascolto = { buffer_rilascio };

/* --- i globali e la connessione --- */

static void globale(void *dati, struct wl_registry *r, uint32_t nome, const char *i, uint32_t v) {
    (void)dati;
    if (!strcmp(i, wl_compositor_interface.name))
        W.comp = wl_registry_bind(r, nome, &wl_compositor_interface, v < 4 ? v : 4);
    else if (!strcmp(i, wl_shm_interface.name))
        W.shm = wl_registry_bind(r, nome, &wl_shm_interface, 1);
    else if (!strcmp(i, xdg_wm_base_interface.name))
        W.base = wl_registry_bind(r, nome, &xdg_wm_base_interface, v < 3 ? v : 3);
    else if (!strcmp(i, wl_seat_interface.name)) {
        W.seat_versione = v < 7 ? (int)v : 7;
        W.seat = wl_registry_bind(r, nome, &wl_seat_interface, (uint32_t)W.seat_versione);
    } else if (!strcmp(i, wl_output_interface.name) && !W.uscita)
        W.uscita = wl_registry_bind(r, nome, &wl_output_interface, v < 4 ? v : 4);
    else if (!strcmp(i, zxdg_decoration_manager_v1_interface.name))
        W.deco = wl_registry_bind(r, nome, &zxdg_decoration_manager_v1_interface, 1);
    else if (!strcmp(i, wp_cursor_shape_manager_v1_interface.name))
        W.cursore = wl_registry_bind(r, nome, &wp_cursor_shape_manager_v1_interface, 1);
}
static void globale_tolto(void *d, struct wl_registry *r, uint32_t nome) { (void)d; (void)r; (void)nome; }
static const struct wl_registry_listener registro_ascolto = { globale, globale_tolto };

int fw_attivo(void) {
    if (stato != -2) return stato > 0;
    stato = -1;
    if (!getenv("WAYLAND_SOCKET")) return 0;       /* senza launcher: il protocollo nostro */
    W.display = wl_display_connect(0);
    if (!W.display) return 0;
    struct wl_registry *registro = wl_display_get_registry(W.display);
    wl_registry_add_listener(registro, &registro_ascolto, 0);
    wl_display_roundtrip(W.display);               /* i globali */
    if (!W.comp || !W.shm || !W.base) {
        wl_display_disconnect(W.display);
        W.display = 0;
        return 0;
    }
    xdg_wm_base_add_listener(W.base, &base_ascolto, 0);
    tastiera_it_inizia(&W.kb);
    W.rip_ritmo = 25;                              /* finche' il compositore non dice altro */
    W.rip_attesa = 600;
    if (W.seat) wl_seat_add_listener(W.seat, &seat_ascolto, 0);
    if (W.uscita) wl_output_add_listener(W.uscita, &output_ascolto, 0);
    wl_display_roundtrip(W.display);               /* le capacita' del seat e lo schermo */
    wl_display_roundtrip(W.display);               /* repeat_info e il resto di tastiera e puntatore */
    stato = 1;
    return 1;
}

int fw_descrittore(void) { return W.display ? wl_display_get_fd(W.display) : -1; }

void fw_scarica(void) {
    if (W.display) wl_display_flush(W.display);
}

int fw_leggi(int ms) {
    struct wl_display *d = W.display;
    int scad = fw_scadenza_ms();
    if (scad >= 0 && (ms < 0 || scad < ms)) ms = scad;
    while (wl_display_prepare_read(d) != 0)
        if (wl_display_dispatch_pending(d) < 0) return -1;
    int pieno = 0;
    if (wl_display_flush(d) < 0) {
        if (errno != EAGAIN) { wl_display_cancel_read(d); return -1; }
        pieno = 1;                                  /* il compositore non ha ancora letto: si riprovera' */
    }
    struct pollfd p = { wl_display_get_fd(d), POLLIN, 0 };
    int r = poll(&p, 1, pieno && (ms < 0 || ms > 20) ? 20 : ms);
    if (r > 0) {
        if (wl_display_read_events(d) < 0) return -1;
    } else {
        wl_display_cancel_read(d);
        if (r < 0 && errno != EINTR) return -1;
    }
    if (wl_display_dispatch_pending(d) < 0) return -1;
    fw_servi_scadenze();
    return 0;
}

/* --- le finestre --- */

static const char *id_app(char *spazio, int misura) {
    /* /system/apps/<id>/code (o dove sta il pacchetto): l'id e' il nome della cartella */
    char percorso[256];
    long n = readlink("/proc/self/exe", percorso, sizeof percorso - 1);
    if (n > 0) {
        percorso[n] = 0;
        char *fine = strrchr(percorso, '/');
        if (fine && fine != percorso) {
            *fine = 0;
            char *inizio = strrchr(percorso, '/');
            if (inizio && inizio[1]) {
                int i = 0;
                for (inizio++; *inizio && i < misura - 1; inizio++) spazio[i++] = *inizio;
                spazio[i] = 0;
                return spazio;
            }
        }
    }
    return "mshell";
}

int fw_apri(struct finestra *f, int larghezza, int altezza, const char *titolo) {
    int posto = -1;
    for (int i = 0; i < MAX_FIN && posto < 0; i++)
        if (!W.fin[i]) posto = i;
    if (posto < 0) return -1;
    struct fw *w = calloc(1, sizeof *w);
    if (!w) return -1;
    w->f = f;
    w->forma = FORMA_NORMALE;
    f->wl = w;
    W.fin[posto] = w;

    w->sup = wl_compositor_create_surface(W.comp);
    w->xdg = xdg_wm_base_get_xdg_surface(W.base, w->sup);
    xdg_surface_add_listener(w->xdg, &xdg_ascolto, w);
    w->top = xdg_surface_get_toplevel(w->xdg);
    xdg_toplevel_add_listener(w->top, &top_ascolto, w);
    char titolo_corto[MAX_TITOLO], id[64];
    int i = 0;
    for (; i < MAX_TITOLO - 1 && titolo[i]; i++) titolo_corto[i] = titolo[i];
    titolo_corto[i] = 0;
    xdg_toplevel_set_title(w->top, titolo_corto);
    xdg_toplevel_set_app_id(w->top, id_app(id, sizeof id));
    if (W.deco) {                                   /* la barra del titolo la disegna l'app */
        w->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(W.deco, w->top);
        zxdg_toplevel_decoration_v1_add_listener(w->deco, &deco_ascolto, w);
        zxdg_toplevel_decoration_v1_set_mode(w->deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    }
    wl_surface_commit(w->sup);                      /* senza contenuto: il compositore risponde con il configure */
    for (int giri = 0; !w->configurata && giri < 500; giri++)
        if (fw_leggi(20) < 0) { fw_chiudi(f); return -1; }
    if (!w->configurata) { fw_chiudi(f); return -1; }

    int pixel = larghezza * altezza;
    struct wl_shm_pool *pool = wl_shm_create_pool(W.shm, f->foglio, 2 * pixel * 4);
    for (int k = 0; k < 2; k++) {
        w->ib[k] = (struct fw_buffer){ w, k };
        w->buf[k] = wl_shm_pool_create_buffer(pool, k * pixel * 4, larghezza, altezza, larghezza * 4,
                                              WL_SHM_FORMAT_ARGB8888);
        wl_buffer_add_listener(w->buf[k], &buffer_ascolto, &w->ib[k]);
    }
    wl_shm_pool_destroy(pool);
    wl_display_flush(W.display);
    return 0;
}

void fw_chiudi(struct finestra *f) {
    struct fw *w = f->wl;
    if (!w) return;
    f->wl = 0;
    for (int i = 0; i < MAX_FIN; i++)
        if (W.fin[i] == w) W.fin[i] = 0;
    if (W.sotto == w) W.sotto = 0;
    if (W.fuoco == w) { W.fuoco = 0; W.rip_attiva = 0; }
    for (int k = 0; k < 2; k++)
        if (w->buf[k]) wl_buffer_destroy(w->buf[k]);
    if (w->deco) zxdg_toplevel_decoration_v1_destroy(w->deco);
    if (w->top) xdg_toplevel_destroy(w->top);
    if (w->xdg) xdg_surface_destroy(w->xdg);
    if (w->sup) wl_surface_destroy(w->sup);
    wl_display_flush(W.display);
    free(w);
}

int fw_mostra(struct finestra *f, const struct zona_finestra *zone, int n, int tutta) {
    struct fw *w = f->wl;
    wl_surface_attach(w->sup, w->buf[f->in_uso], 0, 0);
    int nuovo = wl_proxy_get_version((struct wl_proxy *)w->sup) >= 4;
    if (tutta) {
        if (nuovo) wl_surface_damage_buffer(w->sup, 0, 0, f->larghezza, f->altezza);
        else wl_surface_damage(w->sup, 0, 0, f->larghezza, f->altezza);
    }
    for (int k = 0; !tutta && k < n; k++) {
        if (zone[k].l <= 0 || zone[k].a <= 0) continue;     /* niente di cambiato */
        if (nuovo) wl_surface_damage_buffer(w->sup, zone[k].x, zone[k].y, zone[k].l, zone[k].a);
        else wl_surface_damage(w->sup, zone[k].x, zone[k].y, zone[k].l, zone[k].a);
    }
    wl_surface_commit(w->sup);
    return wl_display_flush(W.display) < 0 && errno != EAGAIN ? -1 : 0;
}

int fw_trascinami(struct finestra *f) {
    struct fw *w = f->wl;
    if (W.seat && W.serial_pulsante) xdg_toplevel_move(w->top, W.seat, W.serial_pulsante);
    return wl_display_flush(W.display) < 0 && errno != EAGAIN ? -1 : 0;
}

int fw_riducimi(struct finestra *f) {
    struct fw *w = f->wl;
    xdg_toplevel_set_minimized(w->top);
    return wl_display_flush(W.display) < 0 && errno != EAGAIN ? -1 : 0;
}

int fw_tasti_fisici(struct finestra *f) {
    ((struct fw *)f->wl)->tasti_fisici = 1;
    return 0;
}

int fw_forma(struct finestra *f, unsigned int forma) {
    struct fw *w = f->wl;
    if (forma > FORMA_ATTESA) return -1;
    w->forma = forma;
    applica_forma(w);
    return wl_display_flush(W.display) < 0 && errno != EAGAIN ? -1 : 0;
}

int fw_schermo(int *larghezza, int *altezza) {
    if (!W.schermo_l || !W.schermo_a) return -1;
    *larghezza = W.schermo_l;
    *altezza = W.schermo_a;
    return 0;
}
