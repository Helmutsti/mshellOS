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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
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

/* --- i globali --- */

static void globale(void *dati, struct wl_registry *registro, uint32_t nome, const char *interfaccia, uint32_t versione) {
    (void)dati;
    if (!strcmp(interfaccia, wl_compositor_interface.name))
        compositore = wl_registry_bind(registro, nome, &wl_compositor_interface, versione < 4 ? versione : 4);
    else if (!strcmp(interfaccia, wl_shm_interface.name))
        shm = wl_registry_bind(registro, nome, &wl_shm_interface, 1);
    else if (!strcmp(interfaccia, xdg_wm_base_interface.name))
        base = wl_registry_bind(registro, nome, &xdg_wm_base_interface, versione < 3 ? versione : 3);
}

static void globale_tolto(void *dati, struct wl_registry *registro, uint32_t nome) { (void)dati; (void)registro; (void)nome; }

static const struct wl_registry_listener registro_ascolto = { globale, globale_tolto };

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
