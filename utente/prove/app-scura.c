/* mshell-prova: l'app di prova della scrivania. Apre una finestra tutta scura e basta
 * (decisione della persona, 19/11/2026: "una app con schermata scura e basta"). Serve a
 * provare le disposizioni e i desktop finche' nel sistema non ci sono app vere.
 *
 * Viene dal vecchio programma di prova dei quadrati (git: utente/prove/wayland.c, tolto con il
 * commit 9dde5bd), che a sua volta e' il "simple-shm" di Weston, il piu' piccolo client Wayland
 * che ci sia. Differenza: questa finestra prende la misura che le chiede il compositore (il
 * posto nella disposizione) e si ridisegna ogni volta che la misura cambia.
 *
 * Si collega al compositore di WAYLAND_DISPLAY, apre una finestra (xdg_toplevel) con il titolo
 * "Prova", la riempie del colore scuro e aspetta. Esce quando il compositore le chiede di
 * chiudersi (Super+X). Quello che fa lo scrive su stderr ("prova: ...").
 *
 * IL BUFFER e' un foglio di memoria condivisa (memfd), dato al compositore con wl_shm; uno per
 * ogni misura, e il compositore lo restituisce (release) quando non gli serve piu'. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#define COLORE 0xFF242426u   /* lo scuro della finestra */
#define LARGHEZZA_DA_SOLA 640 /* se il compositore lascia scegliere (misura 0) */
#define ALTEZZA_DA_SOLA   400

static struct wl_compositor *compositore;
static struct wl_shm *shm;
static struct xdg_wm_base *base;
static int chiusa, da_disegnare;
static int32_t chiesta_l, chiesta_a;   /* la misura dell'ultimo configure */

static void riga(const char *testo) { fprintf(stderr, "prova: %s\n", testo); }

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

/* --- la finestra --- */

static void ping(void *dati, struct xdg_wm_base *b, uint32_t serial) { (void)dati; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener base_ascolto = { ping };

/* Il configure finisce qui: si prende nota e si ridisegna alla misura chiesta. */
static void xdg_configure(void *dati, struct xdg_surface *superficie, uint32_t serial) {
    (void)dati;
    xdg_surface_ack_configure(superficie, serial);
    da_disegnare = 1;
}
static const struct xdg_surface_listener xdg_ascolto = { xdg_configure };

static void toplevel_configure(void *dati, struct xdg_toplevel *t, int32_t l, int32_t a, struct wl_array *stati) {
    (void)dati; (void)t; (void)stati;
    chiesta_l = l;
    chiesta_a = a;
}
static void toplevel_chiudi(void *dati, struct xdg_toplevel *t) {
    (void)dati; (void)t;
    riga("chiusa (il compositore ha chiesto di chiudere)");
    chiusa = 1;
}
static void toplevel_limiti(void *dati, struct xdg_toplevel *t, int32_t l, int32_t a) { (void)dati; (void)t; (void)l; (void)a; }
static void toplevel_capacita(void *dati, struct xdg_toplevel *t, struct wl_array *c) { (void)dati; (void)t; (void)c; }
static const struct xdg_toplevel_listener toplevel_ascolto = { toplevel_configure, toplevel_chiudi, toplevel_limiti, toplevel_capacita };

/* --- il disegno --- */

static void restituito(void *dati, struct wl_buffer *b) { (void)dati; wl_buffer_destroy(b); }
static const struct wl_buffer_listener buffer_ascolto = { restituito };

/* Un buffer l x a tutto del colore scuro. */
static struct wl_buffer *crea_buffer(int l, int a) {
    int byte = l * a * 4;
    int foglio = memfd_create("mshell-prova", MFD_CLOEXEC);
    if (foglio < 0 || ftruncate(foglio, byte) < 0) return 0;
    uint32_t *pixel = mmap(0, (size_t)byte, PROT_READ | PROT_WRITE, MAP_SHARED, foglio, 0);
    if (pixel == MAP_FAILED) return 0;
    for (int i = 0; i < l * a; i++) pixel[i] = COLORE;
    munmap(pixel, (size_t)byte);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, foglio, byte);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, l, a, l * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(foglio);                     /* il compositore ha la sua copia del descrittore */
    wl_buffer_add_listener(b, &buffer_ascolto, 0);
    return b;
}

static int disegna(struct wl_surface *superficie) {
    int l = chiesta_l > 0 ? chiesta_l : LARGHEZZA_DA_SOLA;
    int a = chiesta_a > 0 ? chiesta_a : ALTEZZA_DA_SOLA;
    struct wl_buffer *b = crea_buffer(l, a);
    if (!b) return 0;
    wl_surface_attach(superficie, b, 0, 0);
    wl_surface_damage_buffer(superficie, 0, 0, l, a);
    wl_surface_commit(superficie);
    fprintf(stderr, "prova: disegnata %dx%d\n", l, a);
    return 1;
}

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);   /* il compositore di WAYLAND_DISPLAY */
    if (!display) {
        riga("FALLITA: non trovo il compositore (WAYLAND_DISPLAY, XDG_RUNTIME_DIR)");
        return 1;
    }
    struct wl_registry *registro = wl_display_get_registry(display);
    wl_registry_add_listener(registro, &registro_ascolto, 0);
    wl_display_roundtrip(display);
    if (!compositore || !shm || !base) {
        riga("FALLITA: mancano wl_compositor, wl_shm o xdg_wm_base");
        return 1;
    }
    xdg_wm_base_add_listener(base, &base_ascolto, 0);

    struct wl_surface *superficie = wl_compositor_create_surface(compositore);
    struct xdg_surface *xdg = xdg_wm_base_get_xdg_surface(base, superficie);
    xdg_surface_add_listener(xdg, &xdg_ascolto, 0);
    struct xdg_toplevel *finestra = xdg_surface_get_toplevel(xdg);
    xdg_toplevel_add_listener(finestra, &toplevel_ascolto, 0);
    xdg_toplevel_set_title(finestra, "Prova");
    xdg_toplevel_set_app_id(finestra, "mshell-prova");
    wl_surface_commit(superficie);     /* senza contenuto: il compositore risponde con il configure */

    while (!chiusa) {
        if (wl_display_dispatch(display) < 0) {
            riga("FALLITA: connessione persa");
            return 1;
        }
        if (da_disegnare && !chiusa) {
            da_disegnare = 0;
            if (!disegna(superficie)) {
                riga("FALLITA: il buffer non si crea");
                return 1;
            }
        }
    }
    xdg_toplevel_destroy(finestra);
    xdg_surface_destroy(xdg);
    wl_surface_destroy(superficie);
    wl_display_disconnect(display);
    riga("uscita");
    return 0;
}
