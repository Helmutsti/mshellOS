/* mshell-compositore: il compositore Wayland di mshell.
 *
 * DA DOVE VIENE: e' tinywl, l'esempio ufficiale di wlroots, copiato dal pacchetto dei sorgenti
 * di wlroots 0.18.2 (la stessa versione del pacchetto Debian 13 libwlroots-0.18 0.18.2-3), file
 * tinywl/tinywl.c:
 *   https://gitlab.freedesktop.org/wlroots/wlroots/-/releases/0.18.2/downloads/wlroots-0.18.2.tar.gz
 * NON E' PIU' TINYWL PURO: le aggiunte di mshell sono segnate con "MSHELL:" (sono solo queste):
 *   - LO SFONDO: all'avvio legge /usr/share/mshell/sfondo (pixel pronti, preparati durante la
 *     costruzione da strumenti/sfondo.py, che ne descrive il formato) e lo mette sotto tutte le
 *     finestre. Se il file manca o non va, lo sfondo resta nero come in tinywl.
 *   - LA SCRIVANIA, COME LO STAGE MANAGER: le finestre stanno in gruppi; un gruppo al centro,
 *     gli altri in miniatura nella colonna a sinistra. Le regole sono nel blocco "MSHELL: LA
 *     SCRIVANIA" piu' sotto, il codice in scrivania.c (incluso da questo file). Alt+F1 non c'e'
 *     piu'.
 *   - LE ANIMAZIONI: le app che cambiano posto, che compaiono e che si chiudono. Il codice e' in
 *     animazioni.c (incluso da questo file), qui ci sono le chiamate.
 *   - I LIVELLI E L'ELENCO DELLE APP (per la barra di stato): i protocolli layer-shell e
 *     wlr-foreign-toplevel-management. Il codice e' in livelli.c (spiegato in testa a quel
 *     file), incluso piu' sotto; qui ci sono solo gli agganci, segnati "MSHELL:".
 *   - GLI EFFETTI CON SCENEFX: la scena e il disegno (OpenGL ES 2) sono quelli di SceneFX invece
 *     di quelli di wlroots, per gli angoli arrotondati, il vetro sfocato e le ombre. Il codice e'
 *     in effetti.c (incluso da questo file, spiegato in testa); qui ci sono le chiamate.
 * Tutto il resto e' il file originale. La tastiera italiana e il puntatore di mshell non
 * cambiano il codice: vengono da XKB_DEFAULT_LAYOUT e dal tema Xcursor "default" (costruisci.sh).
 *
 * LA LICENZA: MIT, quella di wlroots (file LICENSE del pacchetto dei sorgenti):
 *   Copyright (c) 2017, 2018 Drew DeVault
 *   Copyright (c) 2014 Jari Vetoniemi
 *   Copyright (c) 2023 The wlroots contributors
 *   Permission is hereby granted, free of charge, to any person obtaining a copy of this
 *   software and associated documentation files (the "Software"), to deal in the Software
 *   without restriction, including without limitation the rights to use, copy, modify, merge,
 *   publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 *   to whom the Software is furnished to do so, subject to the following conditions:
 *   The above copyright notice and this permission notice shall be included in all copies or
 *   substantial portions of the Software.
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 *   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 *   FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *   DEALINGS IN THE SOFTWARE.
 *
 * COME SI USA: mshell-compositore [-s comando]. Disegna sempre con OpenGL ES 2 (SceneFX); senza
 * scheda video vera ci vuole WLR_RENDERER_ALLOW_SOFTWARE=1 (effetti.c); "-s" fa partire un programma gia' collegato (WAYLAND_DISPLAY).
 * Alt+Esc chiude il compositore; i tasti della scrivania sono nel blocco "MSHELL: I DESKTOP".
 * Si compila in costruisci.sh. */
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>    /* MSHELL: memmove (i gruppi) */
#include <signal.h>    /* MSHELL: kill (Super+spazio alla barra, livelli.c) */
#include <time.h>
#include <unistd.h>
#include <execinfo.h>  /* MSHELL: backtrace, il registro delle cadute */
#include <fcntl.h>     /* MSHELL: open, il registro */
#include <wayland-server-core.h>
/* MSHELL: la scena e il disegno di SceneFX (effetti.c). La scena va prima di tutto: ha lo
 * stesso nome e la stessa protezione di quella di wlroots, che cosi' non entra piu'. */
#include <scenefx/types/wlr_scene.h>
#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>  /* MSHELL: per lo sfondo */
#include <drm_fourcc.h>                 /* MSHELL: per lo sfondo */
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_cursor_shape_v1.h>  /* MSHELL: il puntatore chiesto per nome */
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>  /* MSHELL: l'accordo sulla barra del titolo */
#include <wlr/util/box.h>  /* MSHELL: i posti delle app */
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* MSHELL: LA SCRIVANIA, COME LO STAGE MANAGER. Decisa dalla persona il 21-23/11/2026, al posto dei
 * desktop e delle disposizioni del 19/11; il disegno e' in Figma, file "mshell – interfaccia",
 * pagina "Vista d'insieme – concept", sezione "Stage Manager – gruppi a sinistra, disposizione
 * rapida" (8 schermate, chiare e scure, con le note).
 * - Ogni finestra sta in un GRUPPO, uno solo. Un gruppo sta AL CENTRO (si vede e si usa) o nella
 *   COLONNA DEI GRUPPI a sinistra, in miniatura, dal piu' recente in alto.
 * - Al centro le finestre sono libere: si spostano e si ridimensionano come chiede l'app (dalla
 *   barra che disegna lei), e si sovrappongono.
 * - Un clic su un gruppo della colonna lo porta al centro; quello che c'era scende in cima alla
 *   colonna.
 * - LA COLONNA NON TOGLIE SPAZIO alle finestre (la persona, 21/11/2026): sta sotto di loro, ed e'
 *   nascosta quando al centro ci sono finestre (la persona, 23/11/2026); il puntatore contro il
 *   bordo sinistro la porta sopra le finestre, finche' non se ne allontana.
 * - Niente desktop multipli (la persona, 23/11/2026): i gruppi li sostituiscono.
 * PROVVISORIO (da decidere con la persona): un'app che si apre fa un gruppo nuovo al centro (come
 * il Mac); una finestra figlia (un dialogo) va nel gruppo della sua finestra madre.
 * IL CENTRO PUO' ESSERE VUOTO (la persona, 23/11/2026): il "–" sull'ultima finestra del gruppo al
 * centro porta il gruppo nella colonna e lascia il centro vuoto (non scambia con un altro gruppo);
 * anche chiudendo l'ultima finestra il centro resta vuoto.
 * I TASTI (la persona, 23/11/2026: ogni operazione col mouse e con la tastiera): Super+Tab,
 * Super+1..9, Super+Alt, Super+Alt+1..9, Super+Alt+Tab, Super+X, Super+Z, Super+spazio e lo split
 * di un gruppo, descritti in cima a scrivania-tasti.c. Resta Alt+Esc (chiude il
 * compositore).
 * Il codice e' in scrivania.c, incluso piu' sotto. */
#include "token.c"  /* MSHELL: i valori del disegno, letti da /usr/share/mshell/token.css */
#define MSHELL_GRUPPO_MAX 8   /* finestre in un gruppo */
#define MSHELL_GRUPPI_MAX 32  /* gruppi in tutto */

struct tinywl_toplevel;
struct mshell_gruppo {
	struct wlr_scene_tree *albero;  /* le sue finestre; acceso solo se il gruppo e' al centro */
	struct tinywl_toplevel *app[MSHELL_GRUPPO_MAX];  /* dal fondo: l'ultima sta davanti */
	int n;
	int disposizione;  /* la disposizione scelta con "Disponi" (-1: finestre libere) */
	/* dove si vede il suo riquadro nella colonna mentre scorre (scrivania.c, mshell_colonna_y_vista) */
	bool y_noto;
	int y_da, y_a;
	int64_t y_t0;  /* 0: fermo */
};

/* For brevity's sake, struct members are annotated where they are used. */
enum tinywl_cursor_mode {
	TINYWL_CURSOR_PASSTHROUGH,
	TINYWL_CURSOR_MOVE,
	TINYWL_CURSOR_RESIZE,
};

struct tinywl_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
	struct wl_list toplevels;
	struct wlr_xdg_decoration_manager_v1 *decorazioni;  /* MSHELL: l'accordo sulla barra */
	struct wl_listener nuova_decorazione;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wlr_cursor_shape_manager_v1 *forme;  /* MSHELL: il puntatore chiesto per nome */
	struct wl_listener forma;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum tinywl_cursor_mode cursor_mode;
	struct tinywl_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	/* MSHELL: la scrivania (scrivania.c): il gruppo al centro (NULL: centro vuoto) e quelli della
	 * colonna, dall'alto (dal piu' recente) */
	struct wlr_scene_tree *albero_gruppi;  /* le finestre, sopra lo sfondo e la colonna */
	struct mshell_gruppo *centro;
	struct mshell_gruppo *gruppo[MSHELL_GRUPPI_MAX];
	int n_gruppi;
	bool super_giu;  /* Super e' tenuto premuto (Super+spazio) */
	struct mshell_livelli *livelli;  /* MSHELL: i livelli e l'elenco delle app (livelli.c) */
	/* MSHELL: LA SFOCATURA CONSERVATA (effetti.c): un nodo della scena sopra le finestre e sotto
	 * i livelli in alto; quando e' "sporco" sfoca una volta sola quello che c'e' sotto di lui
	 * (sfondo e app) e lo conserva. Il vetro del cassetto e dei menu usa quell'immagine: il
	 * puntatore o un'icona che cambiano ridisegnano solo il loro pezzo, senza rifare la
	 * sfocatura (niente quadratini, poca fatica). */
	struct wlr_scene_optimized_blur *sfocato;
	/* MSHELL: ogni superficie che nasce, per rifare la sfocatura conservata quando una sotto il
	 * vetro cambia (livelli.c) */
	struct wl_listener nuova_superficie_vista;
};

struct tinywl_output {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct tinywl_toplevel {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;  /* MSHELL: il pulsante "riduci" della barra dell'app */
	/* MSHELL: L'ACCORDO SULLA BARRA DEL TITOLO (xdg-decoration; la persona, 24/11/2026): l'app che lo
	 * usa e lascia la barra al sistema ha la capsula di mshell; quella che non lo usa (le app GTK) o
	 * che chiede di disegnarla da se' tiene la sua, e la capsula non c'e'. NULL: nessun accordo. */
	struct wlr_xdg_toplevel_decoration_v1 *decorazione;
	struct wl_listener decorazione_modo;
	struct wl_listener decorazione_via;
	char *app_vera;  /* MSHELL: il nome vero dell'app, per l'icona (NULL: non ancora letto; scrivania.c) */
	struct mshell_gruppo *gruppo;  /* MSHELL: il suo gruppo (NULL: in nessuno ancora; scrivania.c) */
	struct wlr_box posto;  /* MSHELL: dove sta la finestra vera sullo schermo (scrivania.c) */
	struct wlr_box libero;  /* MSHELL: dove stava libera prima di essere disposta (width 0: mai) */
	struct mshell_ombre_menu *ombre;  /* MSHELL: la sua ombra (effetti.c) */
	struct mshell_anim_app *anim;  /* MSHELL: le sue animazioni (animazioni.c) */
	struct wlr_foreign_toplevel_handle_v1 *voce;  /* MSHELL: la sua voce nell'elenco delle app */
	struct wl_listener titolo;                    /* MSHELL: (livelli.c) */
	struct wl_listener attiva;                    /* MSHELL: (livelli.c) */
};

struct tinywl_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct tinywl_keyboard {
	struct wl_list link;
	struct tinywl_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* MSHELL: l'accordo sulla barra del titolo (piu' sotto, vicino alle richieste delle finestre) */
static bool mshell_ha_capsula(struct tinywl_toplevel *t);
static void mshell_decorazione_rispondi(struct tinywl_toplevel *t);
static void mshell_decorazione_lascia(struct tinywl_toplevel *t);
/* MSHELL: dai livelli e dall'elenco delle app (livelli.c, incluso piu' sotto) */
static bool mshell_livelli_tiene_tastiera(struct tinywl_server *server);
static void mshell_elenco_attiva(struct tinywl_server *server, struct tinywl_toplevel *t);
static void mshell_livelli_cassetto(struct tinywl_server *server);
static bool mshell_tasti(struct tinywl_server *server, xkb_keysym_t sym, bool premuto);  /* scrivania-tasti.c */
static void mshell_gruppo_davanti(struct tinywl_toplevel *t);  /* MSHELL: scrivania.c */

static void focus_toplevel(struct tinywl_toplevel *toplevel, struct wlr_surface *surface) {
	/* Note: this function only deals with keyboard focus. */
	if (toplevel == NULL) {
		return;
	}
	struct tinywl_server *server = toplevel->server;
	struct wlr_seat *seat = server->seat;
	/* MSHELL: davanti anche nel suo gruppo (scrivania.c) */
	mshell_gruppo_davanti(toplevel);
	/* MSHELL: un'app non ancora comparsa (map) non e' nell'elenco delle finestre: il focus lo
	 * prende quando compare. */
	if (!toplevel->xdg_toplevel->base->surface->mapped) {
		return;
	}
	/* MSHELL: e' l'app col focus (lo sanno l'elenco e le app); se pero' la tastiera e' di un
	 * menu della barra, la prendera' quando il menu si chiude (livelli.c) */
	mshell_elenco_attiva(server, toplevel);
	if (mshell_livelli_tiene_tastiera(server)) {
		return;
	}
	struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		/* Don't re-focus an already focused surface. */
		return;
	}
	if (prev_surface) {
		/*
		 * Deactivate the previously focused surface. This lets the client know
		 * it no longer has focus and the client will repaint accordingly, e.g.
		 * stop displaying a caret.
		 */
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel != NULL) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	/* Move the toplevel to the front */
	wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	wl_list_remove(&toplevel->link);
	wl_list_insert(&server->toplevels, &toplevel->link);
	/* Activate the new surface */
	wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
	/*
	 * Tell the seat to have the keyboard enter this surface. wlroots will keep
	 * track of this and automatically send key events to the appropriate
	 * clients without additional work on your part.
	 */
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, toplevel->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}
}

/* MSHELL: le animazioni (animazioni.c) e la scrivania (scrivania.c), incluse piu' sotto; queste
 * servono prima. */
static void mshell_anim_dove(struct tinywl_toplevel *t, int *x, int *y);
static void mshell_metti_al_posto(struct tinywl_toplevel *t);
static struct mshell_gruppo *mshell_al_centro(struct tinywl_server *server);
static void mshell_libera(struct tinywl_toplevel *t);

#include "effetti.c"  /* MSHELL: angoli arrotondati, vetro sfocato e ombre (SceneFX) */
#include "animazioni.c"  /* MSHELL: le animazioni (posti, comparire, chiudere) */
#include "scrivania.c"  /* MSHELL: la scrivania: gruppi, finestre libere, colonna dei gruppi */
#include "livelli.c"  /* MSHELL: i livelli e l'elenco delle app */

static void keyboard_handle_modifiers(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static bool handle_keybinding(struct tinywl_server *server, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	/* MSHELL: tolto Alt+F1 (passare alla finestra dopo): la scrivania si usa col mouse */
	default:
		return false;
	}
	return true;
}

static void keyboard_handle_key(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct tinywl_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	/* MSHELL: il tasto Super e le scorciatoie della scrivania non vanno alle app (scrivania-tasti.c:
	 * Super+Tab, Super+1..9, Super+Alt, Super+X, Super+Z, Super+spazio). */
	bool premuto = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
	for (int i = 0; i < nsyms; i++) {
		if (mshell_tasti(server, syms[i], premuto)) {
			handled = true;
		}
	}
	if (!handled && (modifiers & WLR_MODIFIER_ALT) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt to
		 * process it as a compositor keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	/* This event is raised by the keyboard base wlr_input_device to signal
	 * the destruction of the wlr_keyboard. It will no longer receive events
	 * and should be destroyed.
	 */
	struct tinywl_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void server_new_keyboard(struct tinywl_server *server,
		struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct tinywl_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct tinywl_server *server,
		struct wlr_input_device *device) {
	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct tinywl_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}
	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In TinyWL we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct tinywl_server *server = wl_container_of(
			listener, server, request_cursor);
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

/* MSHELL: IL PUNTATORE CHIESTO PER NOME (cursor-shape; la persona, 24/11/2026: i puntatori tutti
 * uguali). L'app dice quale puntatore vuole ("text", "pointer", "wait"...) e lo disegna il
 * compositore col tema "mshell" (costruisci.sh), invece di mandarne uno suo: le app Flatpak non
 * vedono i temi del sistema e userebbero i loro, di un'altra misura e un altro disegno. Se il tema
 * non ha quel nome, il puntatore normale. Solo per l'app che ha il puntatore, come sopra. */
static void mshell_forma_puntatore(struct wl_listener *listener, void *data) {
	struct tinywl_server *server = wl_container_of(listener, server, forma);
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *e = data;
	if (e->seat_client != server->seat->pointer_state.focused_client ||
			e->device_type != WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER) {
		return;
	}
	const char *nome = wlr_cursor_shape_v1_name(e->shape);
	if (wlr_xcursor_manager_get_xcursor(server->cursor_mgr, nome, 1) == NULL) {
		nome = "default";
	}
	wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, nome);
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in tinywl we always honor
	 */
	struct tinywl_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static struct tinywl_toplevel *desktop_toplevel_at(
		struct tinywl_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	/* This returns the topmost node in the scene at the given layout coords.
	 * We only care about surface nodes as we are specifically looking for a
	 * surface in the surface tree of a tinywl_toplevel. */
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	/* Find the node corresponding to the tinywl_toplevel at the root of this
	 * surface tree, it is the only one for which we set the data field. */
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	/* MSHELL: una superficie dei livelli (livelli.c) non sta sotto un'app */
	if (tree == NULL) {
		return NULL;
	}
	/* MSHELL: solo le finestre del gruppo al centro prendono il puntatore (scrivania.c) */
	struct tinywl_toplevel *toplevel = tree->node.data;
	if (toplevel->gruppo == NULL || toplevel->gruppo != mshell_al_centro(server)) {
		*surface = NULL;
		return NULL;
	}
	return toplevel;
}

static void reset_cursor_mode(struct tinywl_server *server) {
	/* Reset the cursor mode to passthrough. */
	struct tinywl_toplevel *presa = server->grabbed_toplevel;
	server->cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	server->grabbed_toplevel = NULL;
	/* MSHELL: lasciata la finestra, le sue pill vanno dove c'e' posto (scrivania-controlli.c) */
	if (presa != NULL) {
		mshell_controlli_segui(presa);
	}
}

static void process_cursor_move(struct tinywl_server *server, uint32_t time) {
	/* Move the grabbed toplevel to the new position.
	 * MSHELL: si sposta il suo posto (la finestra vera, scrivania.c) */
	struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
	toplevel->posto.x = (int)(server->cursor->x - server->grab_x);
	toplevel->posto.y = (int)(server->cursor->y - server->grab_y);
	if (toplevel->posto.y < mshell_finestra_cima(toplevel)) {
		toplevel->posto.y = mshell_finestra_cima(toplevel);  /* MSHELL: non sale oltre (scrivania-controlli.c) */
	}
	mshell_metti_al_posto(toplevel);
}

static void process_cursor_resize(struct tinywl_server *server, uint32_t time) {
	/*
	 * Resizing the grabbed toplevel can be a little bit complicated, because we
	 * could be resizing from any corner or edge. This not only resizes the
	 * toplevel on one or two axes, but can also move the toplevel if you resize
	 * from the top or left edges (or top-left corner).
	 *
	 * Note that some shortcuts are taken here. In a more fleshed-out
	 * compositor, you'd wait for the client to prepare a buffer at the new
	 * size, then commit any movement that was prepared.
	 */
	struct tinywl_toplevel *toplevel = server->grabbed_toplevel;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top < mshell_finestra_cima(toplevel)) {
			new_top = mshell_finestra_cima(toplevel);  /* MSHELL: non sale oltre (scrivania-controlli.c) */
		}
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	/* MSHELL: il posto nuovo della finestra vera (scrivania.c) */
	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	toplevel->posto = (struct wlr_box){ new_left, new_top, new_width, new_height };
	mshell_metti_al_posto(toplevel);
	wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, new_width, new_height);
}

static void process_cursor_motion(struct tinywl_server *server, uint32_t time) {
	/* If the mode is non-passthrough, delegate to those functions. */
	if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
		process_cursor_move(server, time);
		return;
	} else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
		process_cursor_resize(server, time);
		return;
	}

	/* MSHELL: la scrivania segue il puntatore (scrivania.c): la colonna dei gruppi, i controlli
	 * sulla finestra; durante il trascinamento di una miniatura il puntatore e' solo suo */
	if (mshell_scrivania_puntatore(server)) {
		return;
	}

	/* Otherwise, find the toplevel under the pointer and send the event along. */
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	/* MSHELL: qui serve solo la superficie sotto il puntatore, non l'app */
	desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	/* MSHELL: "!surface" invece di "!toplevel": sopra una superficie dei livelli (la barra) il
	 * puntatore lo sceglie lei, come le app */
	if (!surface) {
		/* If there's no toplevel under the cursor, set the cursor image to a
		 * default. This is what makes the cursor image appear when you move it
		 * around the screen, not over any toplevels. */
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	}
	if (surface) {
		/*
		 * Send pointer enter and motion events.
		 *
		 * The enter event gives the surface "pointer focus", which is distinct
		 * from keyboard focus. You get pointer focus by moving the pointer over
		 * a window.
		 *
		 * Note that wlroots will avoid sending duplicate enter/motion events if
		 * the surface has already has pointer focus or if the client is already
		 * aware of the coordinates passed.
		 */
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		/* Clear pointer focus so future button events and such are not sent to
		 * the last client to have the cursor over it. */
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(server->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits a button
	 * event. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;
	/* MSHELL: se un menu a comparsa ha la presa, il clic e' solo suo: lo riceve lui o, se e' fuori,
	 * lo chiude (lo fa wlroots); in tutti e due i casi non da' il focus a quello che sta sotto
	 * (livelli.c). Va guardato prima, perche' la presa finisce durante l'avviso qui sotto. */
	bool presa = mshell_livelli_presa(server);
	/* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);
	if (presa && event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}
	/* MSHELL: i clic della scrivania (scrivania.c): la colonna dei gruppi, i controlli sulla
	 * finestra, la scelta delle disposizioni. Se il clic e' loro, alle app non va. */
	if (mshell_scrivania_clic(server, event->state == WL_POINTER_BUTTON_STATE_PRESSED)) {
		return;
	}
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct tinywl_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
		/* If you released any buttons, we exit interactive move/resize mode. */
		reset_cursor_mode(server);
	} else {
		/* Focus that client if the button was _pressed_ */
		mshell_livelli_clic(server, surface);  /* MSHELL: o la superficie dei livelli (livelli.c) */
		focus_toplevel(toplevel, surface);
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	struct tinywl_server *server =
		wl_container_of(listener, server, cursor_frame);
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(server->seat);
}

static void output_frame(struct wl_listener *listener, void *data) {
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	struct tinywl_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
		scene, output->wlr_output);

	/* Render the scene if needed and commit the output */
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	/* This function is called when the backend requests a new state for
	 * the output. For example, Wayland and X11 backends request a new mode
	 * when the output window is resized. */
	struct tinywl_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	struct tinywl_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct tinywl_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* Configures the output created by the backend to use our allocator
	 * and our renderer. Must be done once, before commiting the output */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	/* The output may be disabled, switch it on. */
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	/* Some backends don't have modes. DRM+KMS does, and we need to set a mode
	 * before we can use the output. The mode is a tuple of (width, height,
	 * refresh rate), and each monitor supports only a specific set of modes. We
	 * just pick the monitor's preferred mode, a more sophisticated compositor
	 * would let the user configure it. */
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	/* Atomically applies the new output state. */
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
	/* MSHELL: la sfocatura conservata e' grande come lo schermo */
	wlr_scene_optimized_blur_set_size(server->sfocato, wlr_output->width, wlr_output->height);

	/* Allocates and configures our state for this output */
	struct tinywl_output *output = calloc(1, sizeof(*output));
	output->wlr_output = wlr_output;
	output->server = server;

	/* Sets up a listener for the frame event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Sets up a listener for the state request event. */
	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	/* Sets up a listener for the destroy event. */
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	/* Adds this to the output layout. The add_auto function arranges outputs
	 * from left-to-right in the order they appear. A more sophisticated
	 * compositor would let the user configure the arrangement of outputs in the
	 * layout.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout,
		wlr_output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

	/* MSHELL: lo schermo e' cambiato: la colonna dei gruppi si rifa' (scrivania.c) */
	mshell_colonna_rifai(server);
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	/* Called when the surface is mapped, or ready to display on-screen. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, map);

	mshell_elenco_compare(toplevel);  /* MSHELL: la sua voce nell'elenco delle app (livelli.c) */
	wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

	/* MSHELL: la finestra prende il suo gruppo e il suo posto, e il gruppo va al centro
	 * (scrivania.c) */
	mshell_assegna(toplevel->server, toplevel);
	mshell_effetti_app(toplevel->scene_tree, toplevel->xdg_toplevel->base, toplevel->ombre);

	focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
	mshell_anim_compare(toplevel);  /* MSHELL: compare con una dissolvenza (animazioni.c) */
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	/* Called when the surface is unmapped, and should no longer be shown. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

	/* Reset the cursor mode if the grabbed toplevel was unmapped. */
	if (toplevel == toplevel->server->grabbed_toplevel) {
		reset_cursor_mode(toplevel->server);
	}

	wl_list_remove(&toplevel->link);
	mshell_elenco_sparisce(toplevel);  /* MSHELL: via la sua voce dall'elenco (livelli.c) */

	/* MSHELL: la finestra sparita esce dal suo gruppo; se era al centro, la tastiera passa alla
	 * finestra davanti del gruppo al centro (scrivania.c) */
	struct tinywl_server *server = toplevel->server;
	bool era_al_centro = toplevel->gruppo != NULL && toplevel->gruppo == mshell_al_centro(server);
	mshell_gruppo_esce(server, toplevel);
	if (era_al_centro) {
		mshell_focus_centro(server);
	}
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, commit);

	if (toplevel->xdg_toplevel->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface.
		 * MSHELL: la misura la sceglie l'app (0,0, come tinywl); il gruppo e il posto li prende
		 * quando compare (xdg_toplevel_map, scrivania.c). */
		wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);
		mshell_decorazione_rispondi(toplevel);  /* MSHELL: chi disegna la barra */
	} else if (toplevel->gruppo != NULL) {
		/* MSHELL: la misura della finestra vera la decide l'app (anche quando cambia da se');
		 * la geometria puo' cambiare a ogni invio: si rimette al posto */
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(toplevel->xdg_toplevel->base, &geo);
		toplevel->posto.width = geo.width;
		toplevel->posto.height = geo.height;
		mshell_metti_al_posto(toplevel);
		/* MSHELL: gli angoli arrotondati, sulla finestra vera dell'app (effetti.c) */
		mshell_effetti_app(toplevel->scene_tree, toplevel->xdg_toplevel->base, toplevel->ombre);
		mshell_scrivania_invio(toplevel->server, toplevel);  /* la miniatura, se e' in colonna */
	}
	mshell_anim_dopo_invio(toplevel);  /* MSHELL: l'opacita' se sta comparendo (animazioni.c) */
	/* MSHELL: se e' sotto il vetro aperto, la sfocatura conservata la rifa' livelli.c
	 * (mshell_superficie_vista_commit), per lei e per le sue sottosuperfici */
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_toplevel is destroyed. */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

	/* MSHELL: di solito e' gia' uscita dal gruppo quando e' sparita; per sicurezza */
	mshell_gruppo_esce(toplevel->server, toplevel);
	mshell_anim_dimentica(toplevel);  /* MSHELL: animazioni.c */
	g_free(toplevel->app_vera);  /* MSHELL */
	free(toplevel->ombre);  /* MSHELL: i nodi dell'ombra se ne vanno con l'albero della finestra */

	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);
	wl_list_remove(&toplevel->request_minimize.link);
	if (toplevel->decorazione != NULL) {
		mshell_decorazione_lascia(toplevel);
	}

	free(toplevel);
}

static void begin_interactive(struct tinywl_toplevel *toplevel,
		enum tinywl_cursor_mode mode, uint32_t edges) {
	/* This function sets up an interactive move or resize operation, where the
	 * compositor stops propegating pointer events to clients and instead
	 * consumes them itself, to move or resize windows. */
	struct tinywl_server *server = toplevel->server;
	struct wlr_surface *focused_surface =
		server->seat->pointer_state.focused_surface;
	if (toplevel->xdg_toplevel->base->surface !=
			wlr_surface_get_root_surface(focused_surface)) {
		/* Deny move/resize requests from unfocused clients. */
		return;
	}
	server->grabbed_toplevel = toplevel;
	server->cursor_mode = mode;
	mshell_libera(toplevel);  /* MSHELL: spostata a mano, il gruppo non e' piu' disposto */

	/* MSHELL: si parte dal posto della finestra vera (scrivania.c) */
	if (mode == TINYWL_CURSOR_MOVE) {
		server->grab_x = server->cursor->x - toplevel->posto.x;
		server->grab_y = server->cursor->y - toplevel->posto.y;
	} else {
		struct wlr_box geo_box = toplevel->posto;

		double border_x = geo_box.x + ((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		double border_y = geo_box.y + ((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo_box;

		server->resize_edges = edges;
	}
}

static void xdg_toplevel_request_move(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	/* MSHELL: di nuovo come tinywl (le finestre al centro sono libere, scrivania.c) */
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_move);
	begin_interactive(toplevel, TINYWL_CURSOR_MOVE, 0);
}

static void xdg_toplevel_request_resize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check the
	 * provided serial against a list of button press serials sent to this
	 * client, to prevent the client from requesting this whenever they want. */
	/* MSHELL: di nuovo come tinywl (le finestre al centro sono libere, scrivania.c) */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_resize);
	begin_interactive(toplevel, TINYWL_CURSOR_RESIZE, event->edges);
}

static void xdg_toplevel_request_maximize(
		struct wl_listener *listener, void *data) {
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on client-side
	 * decorations. tinywl doesn't support maximization, but to conform to
	 * xdg-shell protocol we still must send a configure.
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
	 * However, if the request was sent before an initial commit, we don't do
	 * anything and let the client finish the initial surface setup. */
	struct tinywl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
	/* MSHELL: il pulsante "ingrandisci" della barra dell'app fa come "Disponi" della capsula (la
	 * persona, 24/11/2026): si apre la scelta delle disposizioni (scrivania-controlli.c) */
	if (toplevel->xdg_toplevel->base->surface->mapped && toplevel->gruppo != NULL) {
		focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
		mshell_scelta_apri(toplevel->server, toplevel);
	}
}

/* MSHELL: il pulsante "riduci" della barra dell'app fa come "-" della capsula: la finestra lascia
 * il suo gruppo e va nella colonna (scrivania-controlli.c) */
static void xdg_toplevel_request_minimize(struct wl_listener *listener, void *data) {
	struct tinywl_toplevel *toplevel = wl_container_of(listener, toplevel, request_minimize);
	if (toplevel->xdg_toplevel->base->surface->mapped && toplevel->gruppo != NULL) {
		mshell_togli_dal_gruppo(toplevel->server, toplevel);
	}
}

/* --- MSHELL: L'ACCORDO SULLA BARRA DEL TITOLO (xdg-decoration) ---
 * Si risponde all'app solo quando la sua finestra e' pronta (dopo il primo invio, come chiede
 * wlroots): se ha chiesto di disegnare la barra da se' la disegna lei, altrimenti la fa il
 * sistema (la capsula). */
static void mshell_decorazione_rispondi(struct tinywl_toplevel *t) {
	if (t->decorazione == NULL || !t->xdg_toplevel->base->initialized) {
		return;
	}
	wlr_xdg_toplevel_decoration_v1_set_mode(t->decorazione,
		t->decorazione->requested_mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE ?
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE : WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	mshell_controlli_segui(t);  /* la capsula compare o sparisce */
}

static void mshell_decorazione_modo(struct wl_listener *listener, void *data) {
	struct tinywl_toplevel *t = wl_container_of(listener, t, decorazione_modo);
	mshell_decorazione_rispondi(t);
}

static void mshell_decorazione_lascia(struct tinywl_toplevel *t) {
	wl_list_remove(&t->decorazione_modo.link);
	wl_list_remove(&t->decorazione_via.link);
	t->decorazione = NULL;
}

static void mshell_decorazione_via(struct wl_listener *listener, void *data) {
	struct tinywl_toplevel *t = wl_container_of(listener, t, decorazione_via);
	mshell_decorazione_lascia(t);
	mshell_controlli_segui(t);  /* niente piu' accordo: la barra e' dell'app */
}

static void mshell_nuova_decorazione(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *d = data;
	struct wlr_scene_tree *albero = d->toplevel->base->data;
	struct tinywl_toplevel *t = albero != NULL ? albero->node.data : NULL;
	if (t == NULL || t->decorazione != NULL) {
		return;
	}
	t->decorazione = d;
	t->decorazione_modo.notify = mshell_decorazione_modo;
	wl_signal_add(&d->events.request_mode, &t->decorazione_modo);
	t->decorazione_via.notify = mshell_decorazione_via;
	wl_signal_add(&d->events.destroy, &t->decorazione_via);
	mshell_decorazione_rispondi(t);
}

/* MSHELL: la finestra t ha la capsula di mshell (la barra la disegna il sistema). */
static bool mshell_ha_capsula(struct tinywl_toplevel *t) {
	return t->decorazione != NULL &&
		t->decorazione->requested_mode != WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
}

static void xdg_toplevel_request_fullscreen(
		struct wl_listener *listener, void *data) {
	/* Just as with request_maximize, we must send a configure here. */
	struct tinywl_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	/* This event is raised when a client creates a new toplevel (application window). */
	struct tinywl_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	/* Allocate a tinywl_toplevel for this surface */
	struct tinywl_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	toplevel->server = server;
	toplevel->xdg_toplevel = xdg_toplevel;
	/* MSHELL: prima del nodo della scena, perche' l'istantanea per la dissolvenza in uscita va
	 * fatta prima che la scena spenga le immagini dell'app (animazioni.c) */
	mshell_anim_nuova(server, toplevel);
	toplevel->scene_tree =
		wlr_scene_xdg_surface_create(&toplevel->server->scene->tree, xdg_toplevel->base);
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;
	toplevel->ombre = calloc(1, sizeof(*toplevel->ombre));  /* MSHELL: effetti.c */

	/* Listen to the various events it can emit */
	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	/* cotd */
	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
	toplevel->request_minimize.notify = xdg_toplevel_request_minimize;  /* MSHELL */
	wl_signal_add(&xdg_toplevel->events.request_minimize, &toplevel->request_minimize);
}

static void xdg_popup_commit(struct wl_listener *listener, void *data) {
	/* Called when a new surface state is committed. */
	struct tinywl_popup *popup = wl_container_of(listener, popup, commit);

	if (popup->xdg_popup->base->initial_commit) {
		/* When an xdg_surface performs an initial commit, the compositor must
		 * reply with a configure so the client can map the surface.
		 * tinywl sends an empty configure. A more sophisticated compositor
		 * might change an xdg_popup's geometry to ensure it's not positioned
		 * off-screen, for example. */
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

static void xdg_popup_destroy(struct wl_listener *listener, void *data) {
	/* Called when the xdg_popup is destroyed. */
	struct tinywl_popup *popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);

	free(popup);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
	/* This event is raised when a client creates a new popup. */
	struct wlr_xdg_popup *xdg_popup = data;
	/* MSHELL: un popup che nasce senza genitore e' di una superficie dei livelli: il genitore
	 * glielo da' dopo il protocollo dei livelli, e lo segue livelli.c */
	if (xdg_popup->parent == NULL) {
		return;
	}

	struct tinywl_popup *popup = calloc(1, sizeof(*popup));
	popup->xdg_popup = xdg_popup;

	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	assert(parent != NULL);
	struct wlr_scene_tree *parent_tree = parent->data;
	xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

	popup->commit.notify = xdg_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

	popup->destroy.notify = xdg_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

/* MSHELL: LO SFONDO. Un wlr_buffer fatto a mano: i pixel stanno in memoria (letti dal file) e
 * wlroots li legge con l'"accesso diretto ai dati" (begin/end_data_ptr_access), che e' quello
 * che usa il disegno con il processore (pixman). */
struct mshell_sfondo {
	struct wlr_buffer base;
	void *pixel;
	size_t passo;  /* byte per riga */
};

static void mshell_sfondo_destroy(struct wlr_buffer *buffer) {
	struct mshell_sfondo *sfondo = wl_container_of(buffer, sfondo, base);
	free(sfondo->pixel);
	free(sfondo);
}

static bool mshell_sfondo_begin(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	struct mshell_sfondo *sfondo = wl_container_of(buffer, sfondo, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;  /* lo sfondo si legge e basta */
	}
	*data = sfondo->pixel;
	*format = DRM_FORMAT_XRGB8888;
	*stride = sfondo->passo;
	return true;
}

static void mshell_sfondo_end(struct wlr_buffer *buffer) {
}

static const struct wlr_buffer_impl mshell_sfondo_impl = {
	.destroy = mshell_sfondo_destroy,
	.begin_data_ptr_access = mshell_sfondo_begin,
	.end_data_ptr_access = mshell_sfondo_end,
};

/* Legge il file (larghezza e altezza a 32 bit little-endian, come il processore, poi i pixel
 * XRGB8888) e mette l'immagine nell'albero dato, in alto a sinistra. */
static void mshell_sfondo_carica(struct wlr_scene_tree *albero, const char *percorso) {
	FILE *f = fopen(percorso, "rb");
	if (!f) {
		wlr_log(WLR_INFO, "nessuno sfondo (%s): resta nero", percorso);
		return;
	}
	uint32_t misura[2];
	struct mshell_sfondo *sfondo = NULL;
	if (fread(misura, sizeof(misura), 1, f) != 1 || misura[0] == 0 || misura[1] == 0 ||
			misura[0] > 16384 || misura[1] > 16384) {
		goto male;
	}
	sfondo = calloc(1, sizeof(*sfondo));
	sfondo->passo = (size_t)misura[0] * 4;
	sfondo->pixel = malloc(sfondo->passo * misura[1]);
	if (!sfondo->pixel || fread(sfondo->pixel, sfondo->passo, misura[1], f) != misura[1]) {
		goto male;
	}
	fclose(f);
	wlr_buffer_init(&sfondo->base, &mshell_sfondo_impl, misura[0], misura[1]);
	wlr_scene_buffer_create(albero, &sfondo->base);
	wlr_buffer_drop(&sfondo->base);  /* ora lo tiene la scena */
	wlr_log(WLR_INFO, "sfondo %ux%u da %s", misura[0], misura[1], percorso);
	return;
male:
	wlr_log(WLR_ERROR, "sfondo %s non valido: resta nero", percorso);
	if (sfondo) {
		free(sfondo->pixel);
		free(sfondo);
	}
	fclose(f);
}

/* MSHELL: IL REGISTRO DEL COMPOSITORE. I suoi messaggi (stderr) vanno in
 * /var/tmp/mshell-compositore.log (la partizione dei dati: resta anche dopo una caduta o un
 * riavvio; il registro di prima si tiene in .prima). Se il compositore cade (memoria sbagliata,
 * abort), scrive li' l'elenco delle funzioni in cui si trovava, per capire dove. */
#define MSHELL_REGISTRO "/var/tmp/mshell-compositore.log"
static void mshell_registro_caduta(int segnale) {
	void *dove[64];
	int n = backtrace(dove, 64);
	const char testa[] = "\nMSHELL: CADUTA, le funzioni in cui si trovava:\n";
	ssize_t w = write(STDERR_FILENO, testa, sizeof(testa) - 1);
	(void)w;
	backtrace_symbols_fd(dove, n, STDERR_FILENO);
	fsync(STDERR_FILENO);  /* sul disco subito: se poi la macchina si spegne di colpo, resta */
	signal(segnale, SIG_DFL);
	raise(segnale);
}

static void mshell_registro_avvia(void) {
	rename(MSHELL_REGISTRO, MSHELL_REGISTRO ".prima");
	int fd = open(MSHELL_REGISTRO, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		dup2(fd, STDERR_FILENO);
		close(fd);
	}
	signal(SIGSEGV, mshell_registro_caduta);
	signal(SIGABRT, mshell_registro_caduta);
	signal(SIGBUS, mshell_registro_caduta);
}

int main(int argc, char *argv[]) {
	mshell_registro_avvia();  /* MSHELL: messaggi e cadute in /var/tmp/mshell-compositore.log */
	wlr_log_init(WLR_DEBUG, NULL);
	mshell_token_carica();  /* MSHELL: i valori del disegno (token.c) */
	char *startup_cmd = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	struct tinywl_server server = {0};
	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	server.wl_display = wl_display_create();
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		return 1;
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients.
	 * MSHELL: non si sceglie: e' sempre quello di SceneFX (OpenGL ES 2), l'unico che sa fare
	 * gli effetti (effetti.c). WLR_RENDERER non conta piu'. */
	server.renderer = fx_renderer_create(server.backend);
	if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	server.allocator = wlr_allocator_autocreate(server.backend,
		server.renderer);
	if (server.allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		return 1;
	}

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces, the subcompositor allows to
	 * assign the role of subsurfaces to surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below.*/
	struct wlr_compositor *compositore =
		wlr_compositor_create(server.wl_display, 5, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server.output_layout = wlr_output_layout_create(server.wl_display);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);
	mshell_effetti_avvia(server.scene);  /* MSHELL: la sfocatura del vetro (effetti.c) */

	/* MSHELL: lo sfondo, in un albero tutto suo creato per primo: le finestre, aggiunte dopo
	 * alla scena, stanno tutte sopra. Non e' una superficie, quindi i clic lo attraversano
	 * (desktop_toplevel_at non trova finestre) e il puntatore resta quello normale. */
	struct wlr_scene_tree *mshell_fondo = wlr_scene_tree_create(&server.scene->tree);
	mshell_sfondo_carica(mshell_fondo, "/usr/share/mshell/sfondo");

	/* MSHELL: la scrivania sopra lo sfondo: la colonna dei gruppi e sopra le finestre
	 * (scrivania.c) */
	mshell_scrivania_avvia(&server, mshell_fondo);
	server.sfocato = wlr_scene_optimized_blur_create(&server.scene->tree, 0, 0);
	wlr_scene_node_place_above(&server.sfocato->node, &server.albero_gruppi->node);

	/* MSHELL: i livelli (sotto e sopra le finestre) e l'elenco delle app: livelli.c */
	mshell_livelli_crea(&server);
	mshell_sotto_vetro_avvia(&server, compositore);  /* MSHELL: livelli.c */

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html.
	 */
	wl_list_init(&server.toplevels);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
	server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	server.new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);
	/* MSHELL: l'accordo sulla barra del titolo (vedi mshell_nuova_decorazione) */
	server.decorazioni = wlr_xdg_decoration_manager_v1_create(server.wl_display);
	server.nuova_decorazione.notify = mshell_nuova_decorazione;
	wl_signal_add(&server.decorazioni->events.new_toplevel_decoration, &server.nuova_decorazione);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). */
	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	server.cursor_mode = TINYWL_CURSOR_PASSTHROUGH;
	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
			&server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_display, "seat0");
	mshell_livelli_seat(&server);  /* MSHELL: la fine delle prese dei menu a comparsa (livelli.c) */
	server.forme = wlr_cursor_shape_manager_v1_create(server.wl_display, 1);  /* MSHELL */
	server.forma.notify = mshell_forma_puntatore;
	wl_signal_add(&server.forme->events.request_set_shape, &server.forma);
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
			&server.request_cursor);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
			&server.request_set_selection);

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	/* Set the WAYLAND_DISPLAY environment variable to our socket and run the
	 * startup command if requested. */
	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}
	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(server.wl_display);

	/* Once wl_display_run returns, we destroy all clients then shut down the
	 * server. */
	wl_display_destroy_clients(server.wl_display);
	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wlr_cursor_destroy(server.cursor);
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.wl_display);
	return 0;
}
