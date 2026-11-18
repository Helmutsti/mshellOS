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
 * LA MODALITA' "CSD" (la barra del titolo la disegna il programma, test/prove/wayland_senza_cornice.py):
 * se il programma si chiama wayland_csd chiede al compositore xdg-decoration client_side e si
 * disegna da solo la barra (BARRA_ALTEZZA = 40 pixel, viola, con un quadratino chiaro a destra:
 * "riduci"), sopra lo stesso disegno di prima: la finestra e' quindi 320 x 240 e il contenuto
 * comincia sempre 40 pixel sotto il bordo, ma stavolta la barra e' sua. Fa questo, e per ogni cosa
 * scrive nel registro una riga:
 *   - trascinare: un clic sulla barra chiama xdg_toplevel.move ("wayland: move chiesto");
 *   - ridurre: un clic sul quadratino chiama set_minimized ("wayland: riduci chiesto");
 *   - la forma del puntatore (cursor-shape): sulla barra a sinistra "testo" (barra a I), al centro
 *     "collegamento" (la mano), a destra "normale"; la dice a ogni enter e quando cambia ("wayland:
 *     forma testo"). Sul contenuto, sempre "normale";
 *   - xdg_wm_base.ping: risponde col pong e scrive "wayland: ping"; premendo il tasto X (KEY_X)
 *     smette di rispondere ("wayland: non rispondo piu' ai ping"), per provare la barra di emergenza,
 *     e con Z (KEY_Z) riprende ("wayland: rispondo di nuovo ai ping");
 *   - "wayland: decorazione client_side" (o server_side) quando il compositore dice il modo.
 *
 * LA MODALITA' "SOTTO" (le sottosuperfici, test/prove/wayland_sottosuperfici.py): se il programma si
 * chiama wayland_sotto, sopra il disegno di base (320 x 200, la barra e' la nostra) mette quattro
 * sottosuperfici (wl_subcompositor), tutte sincrone all'inizio, che si vedono nella foto:
 *   A  96 x 64 in (40, 30), blu al 50% (alfa: lascia vedere il disegno sotto)
 *   B  64 x 64 in (120, 60), arancione pieno, sopra A (ha una zona in comune con lei)
 *   C  64 x 64 in (290, 150), viola pieno, in parte FUORI dal bordo della finestra (si taglia)
 *   D  64 x 48 in (200, 20), verde acqua, che prende il puntatore solo nella meta' sinistra
 *      (wl_surface.set_input_region)
 * Le posizioni sono sul contenuto della finestra. Il puntatore, il clic e gli enter sono scritti nel
 * registro come sempre, e se la superficie non e' la principale con il suo nome ("(sotto A)"), con le
 * coordinate DI QUELLA superficie. I tasti fanno i passi (ognuno scrive una riga "wayland: ..."):
 *   1: A in (40, 100) e B sotto A, senza commit del genitore (non vale ancora)
 *   2: commit del genitore (adesso vale)
 *   3: A cambia colore (blu -> verde) con un commit suo, sincrona: non si vede ancora
 *   4: commit del genitore: si vede il verde
 *   5: A diventa asincrona (set_desync) e cambia colore (giallo pieno) con un commit suo: si vede subito
 *   6: la superficie D e' distrutta (il puntatore puo' esserci sopra)
 *   7: una superficie genitore (senza finestra) e la sua figlia: il genitore e' distrutto, la figlia resta
 *      inerte e si aggiorna senza danni
 *   8: errore di protocollo voluto (get_subsurface con il genitore uguale alla superficie): scrive "errore
 *      di protocollo bad_parent" e il programma finisce (il compositore deve restare in piedi).
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
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"

#include "corekit/corekit.h"

#define LARGHEZZA 320
#define ALTEZZA   200
#define QUADRATO  48
#define MARGINE   16

#define BARRA_ALTEZZA 40                   /* la barra del titolo disegnata dal programma (modalita' csd) */
#define BARRA_COLORE  0xFF6A2FA8u
#define RIDUCI_LATO   24                   /* il quadratino "riduci": a 8 dal bordo destro, a 8 dall'alto */
#define RIDUCI_COLORE 0xFFE0E0E0u

static int csd;                            /* il nome del programma e' wayland_csd */
static int copia;                          /* il nome del programma e' wayland_copia: mette una selezione */
static int incolla;                        /* il nome e' wayland_incolla: riceve la selezione */
static int sotto;                          /* il nome del programma e' wayland_sotto */
static int errore_atteso;                  /* l'errore di protocollo e' voluto: non e' una FALLITA */
static struct wl_subcompositor *subcompositore;
static struct wl_display *display_prova;
static struct wl_surface *radice_prova;    /* la superficie principale */
static int altezza_buffer = ALTEZZA;       /* ALTEZZA, o ALTEZZA + BARRA_ALTEZZA in modalita' csd */
static int sposta_y;                       /* dove comincia il disegno di base: 0, o BARRA_ALTEZZA */
static int muto;                           /* non risponde piu' ai ping */
static uint32_t ping_tenuto;               /* l'ultimo ping a cui non ha risposto: alla ripresa si risponde */
static struct zxdg_decoration_manager_v1 *decorazioni;
static struct wp_cursor_shape_manager_v1 *forme;
static struct wp_cursor_shape_device_v1 *forma_puntatore;
static struct xdg_toplevel *toplevel_csd;
static uint32_t serial_entra;
static int forma_ora = -1;                 /* l'ultima forma chiesta: 0 normale, 1 collegamento, 2 testo */
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
static struct wl_data_device_manager *appunti_manager;
static struct wl_output *uscita;           /* lo schermo (wl_output) */
static int schermo_l, schermo_a, schermo_hz, schermo_scala;
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
    else if (!strcmp(interfaccia, wl_subcompositor_interface.name))
        subcompositore = wl_registry_bind(registro, nome, &wl_subcompositor_interface, 1);
    else if (!strcmp(interfaccia, zxdg_decoration_manager_v1_interface.name))
        decorazioni = wl_registry_bind(registro, nome, &zxdg_decoration_manager_v1_interface, 1);
    else if (!strcmp(interfaccia, wp_cursor_shape_manager_v1_interface.name))
        forme = wl_registry_bind(registro, nome, &wp_cursor_shape_manager_v1_interface, 1);
    else if (!strcmp(interfaccia, wl_seat_interface.name))
        seat = wl_registry_bind(registro, nome, &wl_seat_interface, versione < 7 ? versione : 7);
    else if (!strcmp(interfaccia, wl_data_device_manager_interface.name))
        appunti_manager = wl_registry_bind(registro, nome, &wl_data_device_manager_interface, versione < 3 ? versione : 3);
    else if (!strcmp(interfaccia, wl_output_interface.name) && !uscita)
        uscita = wl_registry_bind(registro, nome, &wl_output_interface, versione < 4 ? versione : 4);
}

/* --- lo schermo (wl_output): la misura, il ritmo e la scala; a "done" si scrive nel registro ---
 *   wayland: schermo 1280x800 a 60 Hz, scala 1 */
static void output_geometria(void *d, struct wl_output *o, int32_t x, int32_t y, int32_t mm_l, int32_t mm_a,
                             int32_t subpixel, const char *marca, const char *modello, int32_t trasformazione) {
    (void)d; (void)o; (void)x; (void)y; (void)mm_l; (void)mm_a; (void)subpixel; (void)marca; (void)modello;
    (void)trasformazione;
}
static void output_modo(void *d, struct wl_output *o, uint32_t flag, int32_t l, int32_t a, int32_t mhz) {
    (void)d; (void)o;
    if (flag & WL_OUTPUT_MODE_CURRENT) { schermo_l = l; schermo_a = a; schermo_hz = mhz / 1000; }
}
static void output_fatto(void *d, struct wl_output *o) {
    (void)d; (void)o;
    char riga[96];
    snprintf(riga, sizeof riga, "schermo %dx%d a %d Hz, scala %d", schermo_l, schermo_a, schermo_hz, schermo_scala);
    scrivi_riga("wayland", riga, "");
}
static void output_scala(void *d, struct wl_output *o, int32_t s) { (void)d; (void)o; schermo_scala = s; }
static void output_nome(void *d, struct wl_output *o, const char *n) { (void)d; (void)o; (void)n; }
static void output_descrizione(void *d, struct wl_output *o, const char *n) { (void)d; (void)o; (void)n; }

static const struct wl_output_listener output_ascolto = {
    output_geometria, output_modo, output_fatto, output_scala, output_nome, output_descrizione,
};

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

/* --- le sottosuperfici di prova (modalita' sotto) --- */

struct sotto_prova {
    struct wl_surface *superficie;
    struct wl_subsurface *sotto;
    char nome;
};
static struct sotto_prova sub_a = { .nome = 'A' }, sub_b = { .nome = 'B' }, sub_c = { .nome = 'C' }, sub_d = { .nome = 'D' };
static struct sotto_prova *tutte_sotto[] = { &sub_a, &sub_b, &sub_c, &sub_d };
static const char *focus_nome = "";        /* la superficie che ha il puntatore: "" (la principale) o " (sotto A)" */

static void nome_superficie(struct wl_surface *s) {
    static char testo[16];
    focus_nome = "";
    for (unsigned i = 0; i < sizeof tutte_sotto / sizeof *tutte_sotto; i++)
        if (s && tutte_sotto[i]->superficie == s) {
            snprintf(testo, sizeof testo, " (sotto %c)", tutte_sotto[i]->nome);
            focus_nome = testo;
        }
}

/* La forma che il programma vuole in (x, y), in modalita' csd (vedi in testa): 2 testo, 1 collegamento, 0 normale. */
static int forma_per(int x, int y) {
    if (y >= BARRA_ALTEZZA) return 0;
    return x < 96 ? 2 : x < 192 ? 1 : 0;
}

static void scegli_forma(struct wl_pointer *p, int forma) {
    if (!forme || forma == forma_ora) return;
    if (!forma_puntatore) forma_puntatore = wp_cursor_shape_manager_v1_get_pointer(forme, p);
    wp_cursor_shape_device_v1_set_shape(forma_puntatore, serial_entra,
        forma == 2 ? WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT : forma == 1 ? WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER
                                                                      : WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    forma_ora = forma;
    riga("forma ", "%s", forma == 2 ? "testo" : forma == 1 ? "collegamento" : "normale");
}

static void pu_entra(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s, wl_fixed_t x, wl_fixed_t y) {
    (void)d;
    controlla_serial("pointer.enter", serial);
    ultimo_x = wl_fixed_to_int(x);
    ultimo_y = wl_fixed_to_int(y);
    nome_superficie(s);
    riga("puntatore entra ", "%d,%d%s", ultimo_x, ultimo_y, focus_nome);
    serial_entra = serial;
    forma_ora = -1;                     /* a ogni enter il compositore riparte dalla freccia: si dice di nuovo */
    if (csd) scegli_forma(p, forma_per(ultimo_x, ultimo_y));
}
static void pu_esce(void *d, struct wl_pointer *p, uint32_t serial, struct wl_surface *s) {
    (void)d; (void)p; (void)s;
    controlla_serial("pointer.leave", serial);
    riga("puntatore esce", "%s", focus_nome);
    focus_nome = "";
}
static void pu_muove(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)t;
    ultimo_x = wl_fixed_to_int(x);
    ultimo_y = wl_fixed_to_int(y);
    riga("puntatore muove ", "%d,%d%s", ultimo_x, ultimo_y, focus_nome);
    if (csd) scegli_forma(p, forma_per(ultimo_x, ultimo_y));
}
static void pu_pulsante(void *d, struct wl_pointer *p, uint32_t serial, uint32_t t, uint32_t pulsante, uint32_t stato) {
    (void)d; (void)p; (void)t;
    controlla_serial("pointer.button", serial);
    riga("clic ", "%s %s %d,%d%s", nome_pulsante(pulsante), stato == WL_POINTER_BUTTON_STATE_PRESSED ? "premuto" : "rilasciato",
         ultimo_x, ultimo_y, focus_nome);
    /* modalita' csd: un clic sulla barra (disegnata da noi) trascina la finestra, quello sul
     * quadratino la riduce */
    if (csd && toplevel_csd && pulsante == BTN_LEFT && stato == WL_POINTER_BUTTON_STATE_PRESSED && ultimo_y < BARRA_ALTEZZA) {
        int sul_quadratino = ultimo_x >= LARGHEZZA - 8 - RIDUCI_LATO && ultimo_x < LARGHEZZA - 8 && ultimo_y >= 8 && ultimo_y < 8 + RIDUCI_LATO;
        if (sul_quadratino) {
            xdg_toplevel_set_minimized(toplevel_csd);
            riga("riduci chiesto", "%s", "");
        } else {
            xdg_toplevel_move(toplevel_csd, seat, serial);
            riga("move chiesto", "%s", "");
        }
    }
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
static void passo_sotto(uint32_t tasto);
static void passo_appunti(uint32_t tasto);

static void ta_tasto(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t t, uint32_t tasto, uint32_t stato) {
    (void)d; (void)k; (void)t;
    controlla_serial("keyboard.key", serial);
    riga("tasto ", "%u %s", tasto, stato == WL_KEYBOARD_KEY_STATE_PRESSED ? "premuto" : "rilasciato");
    if (sotto && stato == WL_KEYBOARD_KEY_STATE_PRESSED) passo_sotto(tasto);
    if (incolla && stato == WL_KEYBOARD_KEY_STATE_PRESSED) passo_appunti(tasto);
    if (csd && tasto == KEY_X && stato == WL_KEYBOARD_KEY_STATE_PRESSED) {
        muto = 1;
        riga("non rispondo piu' ai ping", "%s", "");
    } else if (csd && tasto == KEY_Z && stato == WL_KEYBOARD_KEY_STATE_PRESSED && muto) {
        muto = 0;                       /* e risponde al ping che aveva lasciato senza risposta */
        if (ping_tenuto) xdg_wm_base_pong(base, ping_tenuto);
        ping_tenuto = 0;
        riga("rispondo di nuovo ai ping", "%s", "");
    }
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

static void ping(void *dati, struct xdg_wm_base *b, uint32_t serial) {
    (void)dati;
    if (csd) riga("ping ", "%u%s", serial, muto ? " (non rispondo)" : "");
    if (!muto) xdg_wm_base_pong(b, serial);
    else ping_tenuto = serial;
}

static void deco_configure(void *dati, struct zxdg_toplevel_decoration_v1 *d, uint32_t modo) {
    (void)dati; (void)d;
    riga("decorazione ", "%s", modo == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ? "client_side" : "server_side");
}
static const struct zxdg_toplevel_decoration_v1_listener deco_ascolto = { deco_configure };
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
        for (int c = 0; c < QUADRATO; c++) pixel[(sposta_y + y + r) * LARGHEZZA + x + c] = colore;
}

static void disegna(uint32_t *pixel) {
    for (int y = 0; y < ALTEZZA; y++)
        for (int x = 0; x < LARGHEZZA; x++)
            pixel[(sposta_y + y) * LARGHEZZA + x] = 0xFF000000u | (uint32_t)(x * 255 / (LARGHEZZA - 1)) << 16 |
                                                    (uint32_t)(y * 255 / (ALTEZZA - 1)) << 8 | 96;
    quadrato(pixel, MARGINE, MARGINE, 0xFFE61E1Eu);
    quadrato(pixel, LARGHEZZA - MARGINE - QUADRATO, MARGINE, 0xFF1EC83Cu);
    quadrato(pixel, MARGINE, ALTEZZA - MARGINE - QUADRATO, 0xFF2850E6u);
    quadrato(pixel, LARGHEZZA - MARGINE - QUADRATO, ALTEZZA - MARGINE - QUADRATO, 0xFFF0D228u);
    if (!csd) return;
    /* la barra del titolo, disegnata da noi: viola, con il quadratino "riduci" */
    for (int y = 0; y < BARRA_ALTEZZA; y++)
        for (int x = 0; x < LARGHEZZA; x++) pixel[y * LARGHEZZA + x] = BARRA_COLORE;
    for (int y = 8; y < 8 + RIDUCI_LATO; y++)
        for (int x = LARGHEZZA - 8 - RIDUCI_LATO; x < LARGHEZZA - 8; x++) pixel[y * LARGHEZZA + x] = RIDUCI_COLORE;
}

/* Il buffer: un foglio di memoria condivisa (memfd) con il disegno, dato al compositore. */
static struct wl_buffer *crea_buffer(void) {
    int byte = LARGHEZZA * altezza_buffer * 4;
    int foglio = memfd_create("prova-wayland", MFD_CLOEXEC);
    if (foglio < 0 || ftruncate(foglio, byte) < 0) return 0;
    void *dati = mmap(0, (size_t)byte, PROT_READ | PROT_WRITE, MAP_SHARED, foglio, 0);
    if (dati == MAP_FAILED) return 0;
    disegna(dati);
    pixel_anima = dati;
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, foglio, byte);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, LARGHEZZA, altezza_buffer, LARGHEZZA * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(foglio);                     /* il compositore ha la sua copia del descrittore */
    return b;
}

/* --- la modalita' sotto --- */

/* Un buffer tutto di un colore (ARGB8888, premoltiplicato), l x a. */
static struct wl_buffer *buffer_pieno(int l, int a, uint32_t colore) {
    int byte = l * a * 4;
    int foglio = memfd_create("prova-sotto", MFD_CLOEXEC);
    if (foglio < 0 || ftruncate(foglio, byte) < 0) return 0;
    uint32_t *dati = mmap(0, (size_t)byte, PROT_READ | PROT_WRITE, MAP_SHARED, foglio, 0);
    if (dati == MAP_FAILED) return 0;
    for (int i = 0; i < l * a; i++) dati[i] = colore;
    munmap(dati, (size_t)byte);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, foglio, byte);
    struct wl_buffer *b = wl_shm_pool_create_buffer(pool, 0, l, a, l * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(foglio);
    return b;
}

/* Disegna 'colore' sulla superficie 'su' (l x a) e fa il commit (di quella superficie). */
static void riempi(struct wl_surface *su, int l, int a, uint32_t colore) {
    wl_surface_attach(su, buffer_pieno(l, a, colore), 0, 0);
    wl_surface_damage_buffer(su, 0, 0, l, a);
    wl_surface_commit(su);
}

static void sotto_crea(struct sotto_prova *sp, struct wl_surface *genitore, int x, int y, int l, int a, uint32_t colore) {
    sp->superficie = wl_compositor_create_surface(compositore);
    sp->sotto = wl_subcompositor_get_subsurface(subcompositore, sp->superficie, genitore);
    wl_subsurface_set_position(sp->sotto, x, y);
    riempi(sp->superficie, l, a, colore);   /* sincrona: aspetta il commit della principale */
}

#define COLORE_A1 0x8000007Fu   /* blu al 50%, premoltiplicato */
#define COLORE_A2 0x80007F00u   /* verde al 50% */
#define COLORE_A3 0xFFDCDC00u   /* giallo pieno */

static void crea_sottosuperfici(struct wl_surface *radice) {
    if (!subcompositore) {
        scrivi_riga("wayland", "FALLITA: manca wl_subcompositor", "");
        return;
    }
    sotto_crea(&sub_a, radice, 40, 30, 96, 64, COLORE_A1);
    sotto_crea(&sub_b, radice, 120, 60, 64, 64, 0xFFF08C14u);
    sotto_crea(&sub_c, radice, 290, 150, 64, 64, 0xFF8C28D2u);
    sotto_crea(&sub_d, radice, 200, 20, 64, 48, 0xFF14B4A0u);
    struct wl_region *meta = wl_compositor_create_region(compositore);   /* solo la meta' sinistra prende il puntatore */
    wl_region_add(meta, 0, 0, 32, 48);
    wl_surface_set_input_region(sub_d.superficie, meta);
    wl_region_destroy(meta);
    wl_surface_commit(sub_d.superficie);
    scrivi_riga("wayland", "sottosuperfici create: A B C D", "");
}

/* I passi del programma, uno per tasto (vedi in testa). */
static void passo_sotto(uint32_t tasto) {
    if (!subcompositore) return;
    switch (tasto) {
    case KEY_1:
        wl_subsurface_set_position(sub_a.sotto, 40, 100);
        wl_subsurface_place_below(sub_b.sotto, sub_a.superficie);
        riga("passo 1: A spostata e B sotto A, il genitore non ha fatto commit", "%s", "");
        break;
    case KEY_2:
        wl_surface_commit(radice_prova);
        riga("passo 2: commit del genitore", "%s", "");
        break;
    case KEY_3:
        riempi(sub_a.superficie, 96, 64, COLORE_A2);
        riga("passo 3: A verde (sincrona, commit suo)", "%s", "");
        break;
    case KEY_4:
        wl_surface_commit(radice_prova);
        riga("passo 4: commit del genitore", "%s", "");
        break;
    case KEY_5:
        wl_subsurface_set_desync(sub_a.sotto);
        riempi(sub_a.superficie, 96, 64, COLORE_A3);
        riga("passo 5: A asincrona e gialla", "%s", "");
        break;
    case KEY_6:
        wl_surface_destroy(sub_d.superficie);       /* il wl_subsurface resta: e' inerte */
        wl_subsurface_destroy(sub_d.sotto);
        sub_d.superficie = 0;
        riga("passo 6: D distrutta", "%s", "");
        break;
    case KEY_7: {
        struct wl_surface *genitore = wl_compositor_create_surface(compositore);   /* senza ruolo: non ha finestra */
        struct wl_surface *figlia = wl_compositor_create_surface(compositore);
        struct wl_subsurface *ss = wl_subcompositor_get_subsurface(subcompositore, figlia, genitore);
        riempi(figlia, 16, 16, 0xFFFFFFFFu);
        wl_surface_commit(genitore);
        wl_surface_destroy(genitore);
        riempi(figlia, 16, 16, 0xFF000000u);        /* la figlia e' inerte: nessun danno */
        wl_subsurface_destroy(ss);
        wl_surface_destroy(figlia);
        riga("passo 7: genitore distrutto, figlia aggiornata e distrutta", "%s", "");
        break;
    }
    case KEY_8: {
        struct wl_surface *s = wl_compositor_create_surface(compositore);
        errore_atteso = 1;
        wl_subcompositor_get_subsurface(subcompositore, s, s);      /* il genitore e' la superficie stessa */
        riga("passo 8: get_subsurface con il genitore uguale alla superficie", "%s", "");
        break;
    }
    default: return;
    }
    wl_display_flush(display_prova);
}

/* --- gli appunti di prova (modalita' copia e incolla) --- */

#define MIME_TESTO "text/plain;charset=utf-8"
#define TESTO_COPIA "ciao dagli appunti"
#define TESTO_SECONDO "secondo testo"

static struct wl_data_device *appunti_dispositivo;
static struct wl_data_source *appunti_sorgente;
static const char *appunti_testo = TESTO_COPIA;   /* quello che la nostra sorgente da' */
static int appunti_proprio;                       /* la selezione di adesso e' la nostra */
static int offerta_con_testo;                     /* l'offerta in arrivo ha il tipo MIME_TESTO */

static void sorgente_bersaglio(void *d, struct wl_data_source *s, const char *m) { (void)d; (void)s; (void)m; }
static void sorgente_manda(void *d, struct wl_data_source *s, const char *mime, int32_t fd) {
    (void)d; (void)s;
    if (!strcmp(mime, MIME_TESTO)) {
        ssize_t n = write(fd, appunti_testo, strlen(appunti_testo));
        (void)n;
    }
    close(fd);
    riga("appunti ", "dato a chi lo chiede: %s", mime);
}
static void sorgente_cancellata(void *d, struct wl_data_source *s) {
    (void)d;
    wl_data_source_destroy(s);
    appunti_sorgente = 0;
    appunti_proprio = 0;
    riga("appunti ", "sorgente cancellata");
    if (copia) chiusa = 1;   /* chi copia esce */
}
static void sorgente_azione_nessuna(void *d, struct wl_data_source *s) { (void)d; (void)s; }
static void sorgente_azione(void *d, struct wl_data_source *s, uint32_t a) { (void)d; (void)s; (void)a; }
static const struct wl_data_source_listener sorgente_ascolto = {
    sorgente_bersaglio, sorgente_manda, sorgente_cancellata, sorgente_azione_nessuna, sorgente_azione_nessuna, sorgente_azione,
};

/* Mette una sorgente nostra (con MIME_TESTO e il testo dato) come selezione. */
static void appunti_copia(const char *testo) {
    appunti_testo = testo;
    appunti_sorgente = wl_data_device_manager_create_data_source(appunti_manager);
    wl_data_source_add_listener(appunti_sorgente, &sorgente_ascolto, 0);
    wl_data_source_offer(appunti_sorgente, MIME_TESTO);
    appunti_proprio = 1;
    wl_data_device_set_selection(appunti_dispositivo, appunti_sorgente, ultimo_serial);
    riga("appunti ", "copiato: %s", testo);
    wl_display_flush(display_prova);
}

static void offerta_tipo(void *d, struct wl_data_offer *o, const char *mime) {
    (void)d; (void)o;
    if (!strcmp(mime, MIME_TESTO)) offerta_con_testo = 1;
}
static void offerta_azioni_a(void *d, struct wl_data_offer *o, uint32_t a) { (void)d; (void)o; (void)a; }
static const struct wl_data_offer_listener offerta_ascolto = { offerta_tipo, offerta_azioni_a, offerta_azioni_a };

static void disp_offerta(void *d, struct wl_data_device *dev, struct wl_data_offer *o) {
    (void)d; (void)dev;
    offerta_con_testo = 0;
    wl_data_offer_add_listener(o, &offerta_ascolto, 0);
}
static void disp_entra(void *d, struct wl_data_device *dev, uint32_t s, struct wl_surface *su, wl_fixed_t x, wl_fixed_t y,
                       struct wl_data_offer *o) { (void)d; (void)dev; (void)s; (void)su; (void)x; (void)y; (void)o; }
static void disp_esce(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }
static void disp_muove(void *d, struct wl_data_device *dev, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)d; (void)dev; (void)t; (void)x; (void)y;
}
static void disp_rilascia(void *d, struct wl_data_device *dev) { (void)d; (void)dev; }
static void disp_selezione(void *d, struct wl_data_device *dev, struct wl_data_offer *o) {
    (void)d; (void)dev;
    if (!o) { riga("appunti ", "selezione vuota"); return; }
    if (appunti_proprio) {      /* la selezione e' la nostra: leggerla vorrebbe dire aspettare noi stessi */
        riga("appunti ", "selezione propria");
        wl_data_offer_destroy(o);
        return;
    }
    if (!offerta_con_testo) { riga("appunti ", "selezione senza testo"); wl_data_offer_destroy(o); return; }
    int pipe_[2];
    if (pipe(pipe_) < 0) { riga("FALLITA: ", "pipe"); return; }
    wl_data_offer_receive(o, MIME_TESTO, pipe_[1]);
    close(pipe_[1]);
    wl_display_flush(display_prova);
    char testo[256];
    size_t n = 0;
    ssize_t k;
    while (n < sizeof testo - 1 && (k = read(pipe_[0], testo + n, sizeof testo - 1 - n)) > 0) n += (size_t)k;
    close(pipe_[0]);
    testo[n] = 0;
    wl_data_offer_destroy(o);
    riga("appunti ", "ricevuto: %s", testo);
}
static const struct wl_data_device_listener disp_ascolto = {
    disp_offerta, disp_entra, disp_esce, disp_muove, disp_rilascia, disp_selezione,
};

/* I passi dell'incolla, uno per tasto: 1 prende la selezione con un testo suo, 2 distrugge la sua sorgente. */
static void passo_appunti(uint32_t tasto) {
    if (!appunti_manager || !appunti_dispositivo) return;
    if (tasto == KEY_1) appunti_copia(TESTO_SECONDO);
    else if (tasto == KEY_2 && appunti_sorgente) {
        wl_data_source_destroy(appunti_sorgente);
        appunti_sorgente = 0;
        appunti_proprio = 0;
        riga("appunti ", "sorgente distrutta");
        wl_display_flush(display_prova);
    }
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
    csd = quanti > 0 && strstr(argomenti[0], "wayland_csd") != 0;
    sotto = quanti > 0 && strstr(argomenti[0], "wayland_sotto") != 0;
    copia = quanti > 0 && strstr(argomenti[0], "wayland_copia") != 0;
    incolla = quanti > 0 && strstr(argomenti[0], "wayland_incolla") != 0;
    if (incolla) dormi(3000);   /* si apre DOPO chi copia: la sua finestra e' l'ultima, con il fuoco */
    if (csd) {
        altezza_buffer = ALTEZZA + BARRA_ALTEZZA;
        sposta_y = BARRA_ALTEZZA;
    }
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
    display_prova = display;
    struct wl_registry *registro = wl_display_get_registry(display);
    wl_registry_add_listener(registro, &registro_ascolto, 0);
    wl_display_roundtrip(display);
    if (!compositore || !shm || !base) {
        scrivi_riga("wayland", "FALLITA: mancano wl_compositor, wl_shm o xdg_wm_base", "");
        return 1;
    }
    scrivi_riga("wayland", "collegata: wl_compositor, wl_shm, xdg_wm_base", "");
    xdg_wm_base_add_listener(base, &base_ascolto, 0);
    if (seat) wl_seat_add_listener(seat, &seat_ascolto, 0);
    if ((copia || incolla) && appunti_manager && seat) {
        appunti_dispositivo = wl_data_device_manager_get_data_device(appunti_manager, seat);
        wl_data_device_add_listener(appunti_dispositivo, &disp_ascolto, 0);
    } else if (copia || incolla) {
        scrivi_riga("wayland", "FALLITA: manca wl_data_device_manager", "");
        return 1;
    }
    if (uscita) wl_output_add_listener(uscita, &output_ascolto, 0);   /* arriva al prossimo dispatch */   /* le capacita' arrivano al prossimo dispatch */

    struct wl_surface *superficie = wl_compositor_create_surface(compositore);
    struct xdg_surface *xdg = xdg_wm_base_get_xdg_surface(base, superficie);
    xdg_surface_add_listener(xdg, &xdg_ascolto, 0);
    struct xdg_toplevel *finestra = xdg_surface_get_toplevel(xdg);
    xdg_toplevel_add_listener(finestra, &toplevel_ascolto, 0);
    xdg_toplevel_set_title(finestra, "Prova Wayland");
    if (csd) {
        toplevel_csd = finestra;
        if (!decorazioni) {
            scrivi_riga("wayland", "FALLITA: manca xdg-decoration", "");
            return 1;
        }
        struct zxdg_toplevel_decoration_v1 *deco = zxdg_decoration_manager_v1_get_toplevel_decoration(decorazioni, finestra);
        zxdg_toplevel_decoration_v1_add_listener(deco, &deco_ascolto, 0);
        zxdg_toplevel_decoration_v1_set_mode(deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    }
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
    radice_prova = superficie;
    buffer_anima = buffer;
    if (sotto) crea_sottosuperfici(superficie);
    if (anima) chiedi_fotogramma();    /* il primo "fatto" arriva dopo questo commit */
    wl_surface_attach(superficie, buffer, 0, 0);
    wl_surface_damage_buffer(superficie, 0, 0, LARGHEZZA, altezza_buffer);
    wl_surface_commit(superficie);
    scrivi_riga("wayland", "finestra mostrata: Prova Wayland", "");
    if (copia) {
        wl_display_roundtrip(display);
        appunti_copia(TESTO_COPIA);
    }

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
            if (errore_atteso) {            /* l'errore di protocollo voluto (passo 8): il compositore ci ha chiuso */
                const struct wl_interface *interfaccia = 0;
                uint32_t id;
                uint32_t codice = wl_display_get_protocol_error(display, &interfaccia, &id);
                riga("errore di protocollo ", "%s su %s", codice == WL_SUBCOMPOSITOR_ERROR_BAD_PARENT ? "bad_parent" : "altro",
                     interfaccia ? interfaccia->name : "?");
                return 0;
            }
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
