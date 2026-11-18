/* wayland: un client Wayland di prova, e c'e' SOLO per le prove automatiche.
 *
 * Fa quello che fa il "simple-shm" di Weston, il piu' piccolo client Wayland che ci sia: si
 * collega al compositore (il socket di prova, /run/mshell/wayland-prova), apre una finestra
 * (xdg_toplevel) con il titolo "Prova Wayland" e ci disegna dentro qualcosa di riconoscibile,
 * poi aspetta. Risponde ai configure del compositore e, quando riceve "chiuditi" (la X della
 * cornice), scrive nel registro "wayland: chiusa" ed esce. E' il passo 1 della decisione "il
 * compositore passa a Wayland" (SISTEMA.md): la prova e' test/prove/wayland.py.
 *
 * COSA DISEGNA (dentro la finestra, 320 x 200 pixel):
 *   - una sfumatura: il rosso cresce da sinistra a destra, il verde dall'alto in basso, il
 *     blu e' fisso (96);
 *   - quattro quadrati pieni di 48 pixel a 16 dai bordi, uno per angolo, con questi colori che
 *     nella sfumatura non ci sono mai (la prova li cerca nella foto dello schermo):
 *       in alto a sinistra rosso   0xE61E1E     in alto a destra verde  0x1EC83C
 *       in basso a sinistra blu    0x2850E6     in basso a destra giallo 0xF0D228
 *
 * IL BUFFER e' un foglio di memoria condivisa (memfd): il compositore lo legge e lo
 * rilascia subito (wl_buffer.release), perche' ne copia il contenuto.
 *
 * Quello che fa lo scrive nel registro, una riga per cosa ("wayland: ..."): la prova le legge.
 *
 * IL SEAT (passo 2: mouse e tastiera). Il programma chiede wl_seat e, secondo le sue capacita',
 * wl_pointer e wl_keyboard; tutto quello che riceve lo scrive nel registro, una riga per evento
 * (le coordinate sono quelle del CONTENUTO, sotto la testata della cornice):
 *   wayland: seat capacita' puntatore tastiera
 *   wayland: puntatore entra 120,80  /  puntatore esce  /  puntatore muove 130,80
 *   wayland: clic BTN_LEFT premuto 130,80  /  clic BTN_RIGHT rilasciato 130,80
 *   wayland: rotella verticale 10  /  rotella scatti verticale 1  (e orizzontale)
 *   wayland: keymap XKB_V1 56012 byte  /  ripetizione 33 al secondo dopo 500 ms
 *   wayland: tastiera fuoco si (tasti gia' giu': ...)  /  tastiera fuoco no
 *   wayland: tasto 30 premuto  /  tasto 30 rilasciato
 *   wayland: modificatori premuti 0x1 agganciati 0x0 bloccati 0x10
 * Il programma non decodifica i tasti in caratteri (non ha la libreria XKB): scrive i codici di
 * Linux, che la prova controlla. Controlla anche che i numeri di serie non tornino mai indietro.
 *
 * LA MODALITA' "ANIMA" (il ritmo dei fotogrammi, test/prove/wayland_ritmo.py): se il programma si
 * chiama wayland_anima (init li fa partire senza argomenti: il nome e' l'argomento), ridisegna
 * ad ogni "fatto" del frame callback, spostando un quadrato, e chiede subito il prossimo. Ogni
 * secondo scrive "fotogrammi: N ..." (i fotogrammi disegnati in tutto) con il suo tempo e i tick
 * di processore usati finora dal compositore (da /proc): cosi' la prova misura i fotogrammi al
 * secondo e il consumo del compositore, in QEMU come nella sandbox. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include "corekit/corekit.h"

#define LARGHEZZA 320
#define ALTEZZA   200
#define QUADRATO  48
#define MARGINE   16

static struct wl_compositor *compositore;
static struct wl_shm *shm;
static struct xdg_wm_base *base;
static int configurata, chiusa;
static int anima;                          /* il nome del programma e' wayland_anima */
static struct wl_surface *superficie_anima;
static struct wl_buffer *buffer_anima;
static uint32_t *pixel_anima;              /* il buffer, sempre mappato: si ridisegna ad ogni "fatto" */
static unsigned int fotogrammi;            /* i "fatto" ricevuti (e fotogrammi disegnati) */
static struct wl_seat *seat;
static struct wl_pointer *puntatore;
static struct wl_keyboard *tastiera;
static uint32_t ultimo_serial;             /* i numeri di serie non devono mai tornare indietro */
static int ultimo_x, ultimo_y;             /* l'ultima posizione del puntatore, sul contenuto */

/* --- i globali --- */

static void globale(void *dati, struct wl_registry *registro, uint32_t nome, const char *interfaccia, uint32_t versione) {
    (void)dati;
    if (!strcmp(interfaccia, wl_compositor_interface.name))
        compositore = wl_registry_bind(registro, nome, &wl_compositor_interface, versione < 4 ? versione : 4);
    else if (!strcmp(interfaccia, wl_shm_interface.name))
        shm = wl_registry_bind(registro, nome, &wl_shm_interface, 1);
    else if (!strcmp(interfaccia, xdg_wm_base_interface.name))
        base = wl_registry_bind(registro, nome, &xdg_wm_base_interface, versione < 3 ? versione : 3);
    else if (!strcmp(interfaccia, wl_seat_interface.name))
        seat = wl_registry_bind(registro, nome, &wl_seat_interface, versione < 7 ? versione : 7);
}

static void globale_tolto(void *dati, struct wl_registry *registro, uint32_t nome) { (void)dati; (void)registro; (void)nome; }

static const struct wl_registry_listener registro_ascolto = { globale, globale_tolto };

/* --- il seat: puntatore e tastiera --- */

static void riga(const char *a, const char *formato, ...) __attribute__((format(printf, 2, 3)));
static void riga(const char *a, const char *formato, ...) {
    char testo[256];
    va_list v;
    va_start(v, formato);
    vsnprintf(testo, sizeof testo, formato, v);
    va_end(v);
    scrivi_riga("wayland", a, testo);
}

static void controlla_serial(const char *cosa, uint32_t serial) {
    if (serial < ultimo_serial) riga("FALLITA: ", "il numero di serie di %s torna indietro (%u dopo %u)", cosa, serial, ultimo_serial);
    ultimo_serial = serial;
}

static const char *nome_pulsante(uint32_t codice) {
    return codice == BTN_LEFT ? "BTN_LEFT" : codice == BTN_RIGHT ? "BTN_RIGHT" : codice == BTN_MIDDLE ? "BTN_MIDDLE" : "BTN_?";
}

static void pu_entra(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p; (void)s;
    controlla_serial("pointer.enter", serial);
    ultimo_x = wl_fixed_to_int(x);
    ultimo_y = wl_fixed_to_int(y);
    riga("puntatore entra ", "%d,%d", ultimo_x, ultimo_y);
}
static void pu_esce(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)p; (void)s;
    controlla_serial("pointer.leave", serial);
    riga("puntatore esce", "%s", "");
}
static void pu_muove(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)p; (void)t;
    ultimo_x = wl_fixed_to_int(x);
    ultimo_y = wl_fixed_to_int(y);
    riga("puntatore muove ", "%d,%d", ultimo_x, ultimo_y);
}
static void pu_pulsante(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t pulsante, uint32_t stato) {
    (void)d; (void)p; (void)t;
    controlla_serial("pointer.button", serial);
    riga("clic ", "%s %s %d,%d", nome_pulsante(pulsante), stato == WL_POINTER_BUTTON_STATE_PRESSED ? "premuto" : "rilasciato",
         ultimo_x, ultimo_y);
}
static void pu_asse(void *d, struct wl_pointer *p, uint32_t t, uint32_t asse, wl_fixed_t valore) {
    (void)d; (void)p; (void)t;
    riga("rotella ", "%s %d", asse == WL_POINTER_AXIS_VERTICAL_SCROLL ? "verticale" : "orizzontale", wl_fixed_to_int(valore));
}
static void pu_asse_discreto(void *d, struct wl_pointer *p, uint32_t asse, int32_t scatti) {
    (void)d; (void)p;
    riga("rotella scatti ", "%s %d", asse == WL_POINTER_AXIS_VERTICAL_SCROLL ? "verticale" : "orizzontale", scatti);
}
static void pu_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void pu_sorgente(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void pu_fine_asse(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { (void)d; (void)p; (void)t; (void)a; }
static const struct wl_pointer_listener puntatore_ascolto = {
    .enter = pu_entra, .leave = pu_esce, .motion = pu_muove, .button = pu_pulsante,
    .axis = pu_asse, .frame = pu_frame, .axis_source = pu_sorgente, .axis_stop = pu_fine_asse,
    .axis_discrete = pu_asse_discreto,
};

static void ta_keymap(void *d, struct wl_keyboard *k, uint32_t formato, int32_t fd, uint32_t byte) {
    (void)d; (void)k;
    /* la keymap si mappa per controllare che sia leggibile e finisca con lo zero; decodificarla
     * serve la libreria XKB, che il programma di prova non ha */
    char *testo = mmap(0, byte, PROT_READ, MAP_PRIVATE, fd, 0);
    int valida = testo != MAP_FAILED && byte > 1 && testo[byte - 1] == 0 && strstr(testo, "xkb_keymap") != 0;
    if (testo != MAP_FAILED) munmap(testo, byte);
    close(fd);
    riga("keymap ", "%s %u byte%s", formato == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 ? "XKB_V1" : "formato sconosciuto", byte,
         valida ? "" : " (NON VALIDA)");
}
static void ta_entra(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *giu) {
    (void)d; (void)k; (void)s;
    controlla_serial("keyboard.enter", serial);
    char elenco[160] = "";
    uint32_t *c;
    wl_array_for_each(c, giu) {
        size_t n = strlen(elenco);
        snprintf(elenco + n, sizeof elenco - n, "%s%u", n ? "," : "", *c);
    }
    riga("tastiera fuoco si", " (tasti gia' giu': %s)", *elenco ? elenco : "nessuno");
}
static void ta_esce(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)k; (void)s;
    controlla_serial("keyboard.leave", serial);
    riga("tastiera fuoco no", "%s", "");
}
static void ta_tasto(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t t, uint32_t tasto, uint32_t stato) {
    (void)d; (void)k; (void)t;
    controlla_serial("keyboard.key", serial);
    riga("tasto ", "%u %s", tasto, stato == WL_KEYBOARD_KEY_STATE_PRESSED ? "premuto" : "rilasciato");
}
static void ta_modificatori(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t premuti, uint32_t agganciati,
                            uint32_t bloccati, uint32_t gruppo) {
    (void)d; (void)k; (void)gruppo;
    controlla_serial("keyboard.modifiers", serial);
    riga("modificatori ", "premuti 0x%x agganciati 0x%x bloccati 0x%x", premuti, agganciati, bloccati);
}
static void ta_ripetizione(void *d, struct wl_keyboard *k, int32_t ritmo, int32_t attesa) {
    (void)d; (void)k;
    riga("ripetizione ", "%d al secondo dopo %d ms", ritmo, attesa);
}
static const struct wl_keyboard_listener tastiera_ascolto = {
    .keymap = ta_keymap, .enter = ta_entra, .leave = ta_esce, .key = ta_tasto,
    .modifiers = ta_modificatori, .repeat_info = ta_ripetizione,
};

static void seat_capacita(void *d, struct wl_seat *st, uint32_t capacita) {
    (void)d;
    riga("seat capacita' ", "%s%s", capacita & WL_SEAT_CAPABILITY_POINTER ? "puntatore " : "",
         capacita & WL_SEAT_CAPABILITY_KEYBOARD ? "tastiera" : "");
    if ((capacita & WL_SEAT_CAPABILITY_POINTER) && !puntatore) {
        puntatore = wl_seat_get_pointer(st);
        wl_pointer_add_listener(puntatore, &puntatore_ascolto, 0);
    }
    if ((capacita & WL_SEAT_CAPABILITY_KEYBOARD) && !tastiera) {
        tastiera = wl_seat_get_keyboard(st);
        wl_keyboard_add_listener(tastiera, &tastiera_ascolto, 0);
    }
}
static void seat_nome(void *d, struct wl_seat *st, const char *nome) { (void)d; (void)st; riga("seat nome ", "%s", nome); }
static const struct wl_seat_listener seat_ascolto = { seat_capacita, seat_nome };

/* --- xdg --- */

static void ping(void *dati, struct xdg_wm_base *b, uint32_t serial) { (void)dati; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener base_ascolto = { ping };

static void xdg_configure(void *dati, struct xdg_surface *superficie, uint32_t serial) {
    (void)dati;
    xdg_surface_ack_configure(superficie, serial);   /* "ho preso nota": il compositore puo' contare su questa misura */
    configurata = 1;
}
static const struct xdg_surface_listener xdg_ascolto = { xdg_configure };

static void toplevel_configure(void *dati, struct xdg_toplevel *t, int32_t l, int32_t a, struct wl_array *stati) {
    (void)dati; (void)t; (void)l; (void)a;
    int attiva = 0;
    uint32_t *s;
    wl_array_for_each(s, stati) if (*s == XDG_TOPLEVEL_STATE_ACTIVATED) attiva = 1;
    scrivi_riga("wayland", attiva ? "configure attiva" : "configure non attiva", "");
}

static void toplevel_chiudi(void *dati, struct xdg_toplevel *t) {
    (void)dati; (void)t;
    scrivi_riga("wayland", "chiusa", " (il compositore ha chiesto di chiudere)");
    chiusa = 1;
}
static void toplevel_limiti(void *dati, struct xdg_toplevel *t, int32_t l, int32_t a) { (void)dati; (void)t; (void)l; (void)a; }
static void toplevel_capacita(void *dati, struct xdg_toplevel *t, struct wl_array *c) { (void)dati; (void)t; (void)c; }
static const struct xdg_toplevel_listener toplevel_ascolto = { toplevel_configure, toplevel_chiudi, toplevel_limiti, toplevel_capacita };

/* --- il disegno --- */

static void quadrato(uint32_t *pixel, int x, int y, uint32_t colore) {
    for (int r = 0; r < QUADRATO; r++)
        for (int c = 0; c < QUADRATO; c++) pixel[(y + r) * LARGHEZZA + x + c] = colore;
}

static void disegna(uint32_t *pixel) {
    for (int y = 0; y < ALTEZZA; y++)
        for (int x = 0; x < LARGHEZZA; x++)
            pixel[y * LARGHEZZA + x] = 0xFF000000u | (uint32_t)(x * 255 / (LARGHEZZA - 1)) << 16 |
                                       (uint32_t)(y * 255 / (ALTEZZA - 1)) << 8 | 96;
    quadrato(pixel, MARGINE, MARGINE, 0xFFE61E1Eu);
    quadrato(pixel, LARGHEZZA - MARGINE - QUADRATO, MARGINE, 0xFF1EC83Cu);
    quadrato(pixel, MARGINE, ALTEZZA - MARGINE - QUADRATO, 0xFF2850E6u);
    quadrato(pixel, LARGHEZZA - MARGINE - QUADRATO, ALTEZZA - MARGINE - QUADRATO, 0xFFF0D228u);
}

/* Il buffer: un foglio di memoria condivisa (memfd) con il disegno, dato al compositore. */
static struct wl_buffer *crea_buffer(void) {
    int byte = LARGHEZZA * ALTEZZA * 4;
    int foglio = memfd_create("prova-wayland", MFD_CLOEXEC);
    if (foglio < 0 || ftruncate(foglio, byte) < 0) return 0;
    void *dati = mmap(0, (size_t)byte, PROT_READ | PROT_WRITE, MAP_SHARED, foglio, 0);
    if (dati == MAP_FAILED) return 0;
    disegna(dati);
    pixel_anima = dati;
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, foglio, byte);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, LARGHEZZA, ALTEZZA, LARGHEZZA * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(foglio);                     /* il compositore ha la sua copia del descrittore */
    return b;
}

/* --- la modalita' anima --- */

#define LATO_ANIMA 24

/* Il disegno di base con un quadrato bianco che si sposta di 4 pixel a fotogramma, in alto tra i
 * due quadrati degli angoli. */
static void disegna_anima(void) {
    disegna(pixel_anima);
    int x = 80 + (int)(fotogrammi * 4 % (LARGHEZZA - 160 - LATO_ANIMA));
    for (int r = 0; r < LATO_ANIMA; r++)
        for (int c = 0; c < LATO_ANIMA; c++) pixel_anima[(MARGINE + r) * LARGHEZZA + x + c] = 0xFFFFFFFFu;
}

static void fatto(void *dati, struct wl_callback *cb, uint32_t ms);
static const struct wl_callback_listener fatto_ascolto = { fatto };

/* Chiede il prossimo "fatto": vale per il prossimo commit. */
static void chiedi_fotogramma(void) {
    struct wl_callback *cb = wl_surface_frame(superficie_anima);
    wl_callback_add_listener(cb, &fatto_ascolto, 0);
}

static void fatto(void *dati, struct wl_callback *cb, uint32_t ms) {
    (void)dati; (void)ms;
    wl_callback_destroy(cb);
    fotogrammi++;
    disegna_anima();
    chiedi_fotogramma();
    wl_surface_attach(superficie_anima, buffer_anima, 0, 0);
    wl_surface_damage_buffer(superficie_anima, 0, 0, LARGHEZZA, ALTEZZA);
    wl_surface_commit(superficie_anima);
}

static unsigned long long ms_ora(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000 + (unsigned long long)t.tv_nsec / 1000000;
}

/* I tick di processore (utime + stime) del compositore, da /proc/<pid>/stat; 0 se non lo trova. */
static unsigned long long tick_compositore(void) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;
    unsigned long long tick = 0;
    for (struct dirent *v; !tick && (v = readdir(proc));) {
        if (v->d_name[0] < '0' || v->d_name[0] > '9') continue;
        char percorso[64], riga[512];
        snprintf(percorso, sizeof percorso, "/proc/%s/stat", v->d_name);
        FILE *f = fopen(percorso, "r");
        if (!f) continue;
        if (fgets(riga, sizeof riga, f)) {
            char *nome = strchr(riga, '(');
            char *fine = strrchr(riga, ')');
            if (nome && fine && !strncmp(nome + 1, "compositore", 11)) {
                unsigned long long ut = 0, st = 0;
                if (sscanf(fine + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &ut, &st) == 2) tick = ut + st;
            }
        }
        fclose(f);
    }
    closedir(proc);
    return tick;
}

/* Aspetta i messaggi del compositore fino a 'ms' millisecondi, e li smista (i "fatto"). */
static int aspetta_messaggi(struct wl_display *display, int ms) {
    while (wl_display_prepare_read(display) != 0)
        if (wl_display_dispatch_pending(display) < 0) return -1;
    wl_display_flush(display);
    struct pollfd p = { wl_display_get_fd(display), POLLIN, 0 };
    if (poll(&p, 1, ms) > 0) wl_display_read_events(display);
    else wl_display_cancel_read(display);
    return wl_display_dispatch_pending(display) < 0 ? -1 : 0;
}

int main(int quanti, char **argomenti) {
    anima = quanti > 0 && strstr(argomenti[0], "wayland_anima") != 0;
    /* il socket di prova del compositore (compositore_wayland.c): il compositore parte con il
     * sistema, quindi si riprova finche' non c'e' */
    setenv("XDG_RUNTIME_DIR", "/run/mshell", 1);
    struct wl_display *display = 0;
    for (int giro = 0; giro < 200 && !display; giro++) {
        display = wl_display_connect("wayland-prova");
        if (!display) dormi(100);
    }
    if (!display) {
        scrivi_riga("wayland", "FALLITA: non trovo il compositore", "");
        return 1;
    }
    struct wl_registry *registro = wl_display_get_registry(display);
    wl_registry_add_listener(registro, &registro_ascolto, 0);
    wl_display_roundtrip(display);
    if (!compositore || !shm || !base) {
        scrivi_riga("wayland", "FALLITA: mancano wl_compositor, wl_shm o xdg_wm_base", "");
        return 1;
    }
    scrivi_riga("wayland", "collegata: wl_compositor, wl_shm, xdg_wm_base", "");
    xdg_wm_base_add_listener(base, &base_ascolto, 0);
    if (seat) wl_seat_add_listener(seat, &seat_ascolto, 0);   /* le capacita' arrivano al prossimo dispatch */

    struct wl_surface *superficie = wl_compositor_create_surface(compositore);
    struct xdg_surface *xdg = xdg_wm_base_get_xdg_surface(base, superficie);
    xdg_surface_add_listener(xdg, &xdg_ascolto, 0);
    struct xdg_toplevel *finestra = xdg_surface_get_toplevel(xdg);
    xdg_toplevel_add_listener(finestra, &toplevel_ascolto, 0);
    xdg_toplevel_set_title(finestra, "Prova Wayland");
    xdg_toplevel_set_app_id(finestra, "prova-wayland");
    wl_surface_commit(superficie);     /* senza contenuto: il compositore risponde con il configure */
    while (!configurata && !chiusa)
        if (wl_display_dispatch(display) < 0) {
            scrivi_riga("wayland", "FALLITA: la connessione si e' chiusa prima del configure", "");
            return 1;
        }

    struct wl_buffer *buffer = crea_buffer();
    if (!buffer) {
        scrivi_riga("wayland", "FALLITA: il buffer non si crea", "");
        return 1;
    }
    superficie_anima = superficie;
    buffer_anima = buffer;
    if (anima) chiedi_fotogramma();    /* il primo "fatto" arriva dopo questo commit */
    wl_surface_attach(superficie, buffer, 0, 0);
    wl_surface_damage_buffer(superficie, 0, 0, LARGHEZZA, ALTEZZA);
    wl_surface_commit(superficie);
    scrivi_riga("wayland", "finestra mostrata: Prova Wayland", "");

    unsigned long long prossimo_registro = ms_ora() + 1000;
    while (anima && !chiusa) {
        /* ogni secondo una riga, anche a finestra ridotta (allora i fotogrammi non crescono) */
        long long manca = (long long)(prossimo_registro - ms_ora());
        if (aspetta_messaggi(display, manca > 0 ? (int)manca : 0) < 0) {
            scrivi_riga("wayland", "FALLITA: connessione persa", "");
            return 1;
        }
        if (ms_ora() >= prossimo_registro) {
            char riga[160];
            snprintf(riga, sizeof riga, "%u (tempo %llu ms, compositore %llu tick)", fotogrammi, ms_ora(), tick_compositore());
            scrivi_riga("wayland", "fotogrammi: ", riga);
            prossimo_registro += 1000;
        }
    }

    while (!chiusa)
        if (wl_display_dispatch(display) < 0) {
            scrivi_riga("wayland", "FALLITA: connessione persa", "");
            return 1;
        }
    xdg_toplevel_destroy(finestra);
    xdg_surface_destroy(xdg);
    wl_surface_destroy(superficie);
    wl_display_disconnect(display);
    scrivi_riga("wayland", "uscita", "");
    return 0;
}
