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
 * Quello che fa lo scrive nel registro, una riga per cosa ("wayland: ..."): la prova le legge. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
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
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, foglio, byte);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, LARGHEZZA, ALTEZZA, LARGHEZZA * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(foglio);                     /* il compositore ha la sua copia del descrittore */
    return b;
}

int main(void) {
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
    wl_surface_attach(superficie, buffer, 0, 0);
    wl_surface_damage_buffer(superficie, 0, 0, LARGHEZZA, ALTEZZA);
    wl_surface_commit(superficie);
    scrivi_riga("wayland", "finestra mostrata: Prova Wayland", "");

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
