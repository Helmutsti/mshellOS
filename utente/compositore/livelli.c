/* MSHELL: I LIVELLI E L'ELENCO DELLE APP. Questo file non si compila da solo: tinywl.c lo
 * include (#include "livelli.c") subito prima di keyboard_handle_modifiers, cosi' vede le sue
 * strutture e le funzioni dei desktop. In tinywl.c restano solo gli agganci, segnati "MSHELL:".
 *
 * I LIVELLI (protocollo layer-shell, wlr_layer_shell_v1). E' il modo standard con cui un
 * programma della scrivania (la barra di stato, mshell-barra) chiede al compositore una
 * superficie che non e' un'app: dice in quale livello sta, a quali bordi dello schermo si
 * attacca e se vuole la tastiera. I quattro livelli, dal basso:
 *     sfondo
 *     "background", "bottom"    sotto i desktop, sopra lo sfondo
 *     i desktop con le app
 *     "top", "overlay"          sopra i desktop (la barra e i suoi menu)
 * - LA ZONA ESCLUSIVA (la striscia che una superficie chiede di tenere libera dalle app) qui
 *   NON sposta le app: la barra vive nei 32 px di bordo che le app lasciano comunque liberi
 *   (MSHELL_BORDO). Quindi ogni superficie si sistema rispetto a tutto lo schermo.
 * - LA TASTIERA: una superficie di "top" o "overlay" che la chiede in modo esclusivo la prende
 *   appena compare e la tiene finche' non sparisce o smette di chiederla (cosi' un menu aperto
 *   riceve Esc). Una che la chiede "a richiesta" (on_demand) la prende quando ci si fa clic.
 *   Quando la superficie la lascia, la tastiera torna all'app col focus del desktop che si vede.
 *   Mentre la tiene una superficie esclusiva, le app non la possono riprendere (focus_toplevel).
 * - I POPUP (i menu a comparsa) delle superfici dei livelli si disegnano nell'albero della loro
 *   superficie e si tengono dentro lo schermo.
 *
 * L'ELENCO DELLE APP (protocollo wlr-foreign-toplevel-management, l'aiuto di wlroots). Fa sapere
 * agli altri programmi quali app ci sono, con titolo e nome, e quale ha il focus: la barra ci
 * legge il titolo da mostrare a sinistra. Una voce nasce quando l'app compare, sparisce con lei,
 * e il titolo si aggiorna quando l'app lo cambia. "Ha il focus" vuol dire: e' l'app col focus
 * del desktop che si vede, anche mentre un menu della barra tiene la tastiera.
 * Chi legge l'elenco puo' anche chiedere di ATTIVARE un'app: il cassetto delle app lo fa quando
 * si sceglie un'app gia' aperta. Il compositore passa al desktop che la ospita e le da' il focus.
 *
 * IL CASSETTO DELLE APP lo disegna la barra (mshell-barra, utente/barra/cassetto.c). Super+spazio
 * lo prende il compositore, che lo dice alla barra con il segnale SIGUSR1 ("apri o chiudi"): la
 * barra e' il programma che ha le superfici con il nome "mshell-barra..." (il namespace dei
 * livelli), e il suo numero di processo lo sa la connessione Wayland. */
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>

#define MSHELL_LIVELLI 4  /* background, bottom, top, overlay (enum zwlr_layer_shell_v1_layer) */

struct mshell_livelli {
	struct tinywl_server *server;
	struct wlr_layer_shell_v1 *shell;
	struct wl_listener nuova_superficie;
	struct wlr_scene_tree *albero[MSHELL_LIVELLI];  /* un albero della scena per livello */
	struct wl_list superfici;                       /* mshell_livello.link */
	struct wlr_layer_surface_v1 *tastiera;          /* l'ultima che ha preso la tastiera */
	struct wlr_foreign_toplevel_manager_v1 *elenco;
};

/* Una superficie di un livello. */
struct mshell_livello {
	struct wl_list link;
	struct mshell_livelli *livelli;
	struct wlr_layer_surface_v1 *superficie;
	struct wlr_scene_layer_surface_v1 *scena;
	struct mshell_ombre_livello ombre;  /* l'ombra, se e' un pannello (effetti.c) */
	struct wl_listener commit;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener nuovo_popup;
};

/* Un popup che ha per genitore una superficie di un livello. */
struct mshell_popup_livello {
	struct wlr_xdg_popup *popup;
	struct mshell_livello *genitore;
	struct wl_listener commit;
	struct wl_listener destroy;
};

/* Vero se la tastiera e' di una superficie che la vuole in modo esclusivo: le app aspettano. */
static bool mshell_livelli_tiene_tastiera(struct tinywl_server *server) {
	struct wlr_layer_surface_v1 *ls = server->livelli->tastiera;
	return ls != NULL &&
		ls->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
		server->seat->keyboard_state.focused_surface == ls->surface;
}

static void mshell_livelli_dai_tastiera(struct mshell_livelli *l, struct wlr_layer_surface_v1 *ls) {
	struct wlr_seat *seat = l->server->seat;
	l->tastiera = ls;
	struct wlr_keyboard *k = wlr_seat_get_keyboard(seat);
	if (k != NULL) {
		wlr_seat_keyboard_notify_enter(seat, ls->surface, k->keycodes, k->num_keycodes,
			&k->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(seat, ls->surface, NULL, 0, NULL);
	}
}

/* Rifa' la scelta della tastiera dopo che una superficie e' cambiata. "tranne" e' quella che
 * sta sparendo (non va contata anche se risulta ancora accesa). */
static void mshell_livelli_tastiera(struct mshell_livelli *l, struct wlr_layer_surface_v1 *tranne) {
	/* la piu' in alto fra quelle accese di top/overlay che vogliono la tastiera in esclusiva */
	struct wlr_layer_surface_v1 *vuole = NULL;
	struct mshell_livello *s;
	wl_list_for_each(s, &l->superfici, link) {
		struct wlr_layer_surface_v1 *ls = s->superficie;
		if (ls == tranne || !ls->surface->mapped ||
				ls->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP ||
				ls->current.keyboard_interactive !=
					ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
			continue;
		}
		if (vuole == NULL || ls->current.layer >= vuole->current.layer) {
			vuole = ls;
		}
	}
	struct wlr_seat *seat = l->server->seat;
	if (vuole != NULL) {
		if (seat->keyboard_state.focused_surface != vuole->surface) {
			mshell_livelli_dai_tastiera(l, vuole);
		}
		return;
	}
	/* nessuna la vuole in esclusiva: se ce l'aveva una che sparisce o non la vuole piu', la
	 * tastiera torna all'app col focus del desktop che si vede */
	struct wlr_layer_surface_v1 *ls = l->tastiera;
	if (ls != NULL && (ls == tranne || !ls->surface->mapped ||
			ls->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)) {
		l->tastiera = NULL;
		if (seat->keyboard_state.focused_surface == ls->surface) {
			mshell_mostra(l->server, l->server->attuale);
		}
	}
}

/* Sistema la superficie rispetto a tutto il suo schermo (la zona esclusiva si ignora, vedi in
 * cima: per questo le superfici non si spostano l'una con l'altra e basta sistemare quella che
 * e' cambiata). wlroots calcola posto e misura dai bordi chiesti e manda la configurazione. */
static void mshell_livello_disponi(struct mshell_livello *s) {
	struct wlr_layer_surface_v1 *ls = s->superficie;
	struct wlr_box tutto;
	wlr_output_layout_get_box(s->livelli->server->output_layout, ls->output, &tutto);
	struct wlr_box libero = tutto;  /* buttato: le app non si spostano */
	wlr_scene_layer_surface_v1_configure(s->scena, &tutto, &libero);
}

static void mshell_livello_commit(struct wl_listener *listener, void *data) {
	struct mshell_livello *s = wl_container_of(listener, s, commit);
	struct wlr_layer_surface_v1 *ls = s->superficie;
	struct mshell_livelli *l = s->livelli;
	if (ls->initial_commit) {
		/* al primo invio il compositore deve rispondere con la misura */
		mshell_livello_disponi(s);
		return;
	}
	if (ls->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		wlr_scene_node_reparent(&s->scena->tree->node, l->albero[ls->current.layer]);
	}
	if (ls->current.committed != 0) {
		mshell_livello_disponi(s);
	}
	/* il vetro, gli angoli e l'ombra, se e' fra le superfici che li hanno (effetti.c) */
	mshell_effetti_livello(s->scena->tree, ls, &s->ombre);
	mshell_livelli_tastiera(l, NULL);
}

static void mshell_livello_unmap(struct wl_listener *listener, void *data) {
	struct mshell_livello *s = wl_container_of(listener, s, unmap);
	mshell_livelli_tastiera(s->livelli, s->superficie);
}

static void mshell_livello_destroy(struct wl_listener *listener, void *data) {
	struct mshell_livello *s = wl_container_of(listener, s, destroy);
	struct mshell_livelli *l = s->livelli;
	wl_list_remove(&s->link);
	wl_list_remove(&s->commit.link);
	wl_list_remove(&s->unmap.link);
	wl_list_remove(&s->destroy.link);
	wl_list_remove(&s->nuovo_popup.link);
	if (l->tastiera == s->superficie) {
		mshell_livelli_tastiera(l, s->superficie);
		l->tastiera = NULL;
	}
	free(s);  /* l'albero della scena lo toglie wlroots con la superficie */
}

static void mshell_popup_livello_commit(struct wl_listener *listener, void *data) {
	struct mshell_popup_livello *p = wl_container_of(listener, p, commit);
	if (!p->popup->base->initial_commit) {
		/* il vetro come la superficie genitore (effetti.c) */
		mshell_effetti_livello(p->genitore->scena->tree, p->genitore->superficie,
			&p->genitore->ombre);
		return;
	}
	/* il popup resta dentro lo schermo: il riquadro e' quello dello schermo, nelle coordinate
	 * della superficie genitore (che sta dove l'ha messa mshell_livelli_disponi) */
	struct wlr_layer_surface_v1 *ls = p->genitore->superficie;
	struct wlr_box schermo = {0};
	if (ls->output != NULL) {
		wlr_output_layout_get_box(p->genitore->livelli->server->output_layout, ls->output,
			&schermo);
	}
	schermo.x -= p->genitore->scena->tree->node.x;
	schermo.y -= p->genitore->scena->tree->node.y;
	wlr_xdg_popup_unconstrain_from_box(p->popup, &schermo);
	wlr_xdg_surface_schedule_configure(p->popup->base);
}

static void mshell_popup_livello_destroy(struct wl_listener *listener, void *data) {
	struct mshell_popup_livello *p = wl_container_of(listener, p, destroy);
	wl_list_remove(&p->commit.link);
	wl_list_remove(&p->destroy.link);
	free(p);
}

/* Un popup della superficie: va nel suo albero. I popup dei popup li segue gia' tinywl
 * (server_new_xdg_popup), che trova l'albero del genitore in base->data. */
static void mshell_livello_nuovo_popup(struct wl_listener *listener, void *data) {
	struct mshell_livello *s = wl_container_of(listener, s, nuovo_popup);
	struct wlr_xdg_popup *popup = data;
	popup->base->data = wlr_scene_xdg_surface_create(s->scena->tree, popup->base);
	struct mshell_popup_livello *p = calloc(1, sizeof(*p));
	p->popup = popup;
	p->genitore = s;
	p->commit.notify = mshell_popup_livello_commit;
	wl_signal_add(&popup->base->surface->events.commit, &p->commit);
	p->destroy.notify = mshell_popup_livello_destroy;
	wl_signal_add(&popup->events.destroy, &p->destroy);
}

static void mshell_livelli_nuova_superficie(struct wl_listener *listener, void *data) {
	struct mshell_livelli *l = wl_container_of(listener, l, nuova_superficie);
	struct wlr_layer_surface_v1 *ls = data;
	/* se il programma non sceglie lo schermo, il primo */
	if (ls->output == NULL) {
		if (wl_list_empty(&l->server->outputs)) {
			wlr_layer_surface_v1_destroy(ls);
			return;
		}
		struct tinywl_output *o = wl_container_of(l->server->outputs.next, o, link);
		ls->output = o->wlr_output;
	}
	struct mshell_livello *s = calloc(1, sizeof(*s));
	s->livelli = l;
	s->superficie = ls;
	s->scena = wlr_scene_layer_surface_v1_create(l->albero[ls->pending.layer], ls);
	ls->data = s;
	s->commit.notify = mshell_livello_commit;
	wl_signal_add(&ls->surface->events.commit, &s->commit);
	s->unmap.notify = mshell_livello_unmap;
	wl_signal_add(&ls->surface->events.unmap, &s->unmap);
	s->destroy.notify = mshell_livello_destroy;
	wl_signal_add(&ls->events.destroy, &s->destroy);
	s->nuovo_popup.notify = mshell_livello_nuovo_popup;
	wl_signal_add(&ls->events.new_popup, &s->nuovo_popup);
	wl_list_insert(&l->superfici, &s->link);
}

/* Un clic: se e' su una superficie che vuole la tastiera, la prende (se non la tiene gia' in
 * esclusiva un'altra). */
static void mshell_livelli_clic(struct tinywl_server *server, struct wlr_surface *surface) {
	if (surface == NULL) {
		return;
	}
	struct wlr_layer_surface_v1 *ls =
		wlr_layer_surface_v1_try_from_wlr_surface(wlr_surface_get_root_surface(surface));
	struct mshell_livelli *l = server->livelli;
	if (ls == NULL || !ls->surface->mapped ||
			ls->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE ||
			(mshell_livelli_tiene_tastiera(server) && l->tastiera != ls)) {
		return;
	}
	if (server->seat->keyboard_state.focused_surface != ls->surface) {
		mshell_livelli_dai_tastiera(l, ls);
	}
}

/* L'app t compare: nasce la sua voce nell'elenco, con il titolo che segue quello dell'app. */
static void mshell_elenco_titolo(struct wl_listener *listener, void *data) {
	struct tinywl_toplevel *t = wl_container_of(listener, t, titolo);
	const char *titolo = t->xdg_toplevel->title;
	wlr_foreign_toplevel_handle_v1_set_title(t->voce, titolo ? titolo : "");
}

/* Qualcuno (il cassetto) chiede di attivare l'app t: si passa al suo desktop, scorrendo verso
 * di lui, e lei prende il focus. Se la tastiera e' del cassetto, l'app la prende quando il
 * cassetto si chiude (mshell_livelli_tastiera). */
static void mshell_elenco_richiesta_attiva(struct wl_listener *listener, void *data) {
	struct tinywl_toplevel *t = wl_container_of(listener, t, attiva);
	struct tinywl_server *server = t->server;
	if (t->desktop < 0) {
		return;
	}
	struct mshell_desktop *d = &server->desktop[t->desktop];
	for (int i = 0; i < d->n; i++) {
		if (d->app[i] == t) {
			d->focus = i;
		}
	}
	int verso = t->desktop == server->attuale ? 0 : (t->desktop > server->attuale ? 1 : -1);
	mshell_mostra_verso(server, t->desktop, verso);
}

static void mshell_elenco_compare(struct tinywl_toplevel *t) {
	t->voce = wlr_foreign_toplevel_handle_v1_create(t->server->livelli->elenco);
	if (t->xdg_toplevel->app_id != NULL) {
		wlr_foreign_toplevel_handle_v1_set_app_id(t->voce, t->xdg_toplevel->app_id);
	}
	t->titolo.notify = mshell_elenco_titolo;
	wl_signal_add(&t->xdg_toplevel->events.set_title, &t->titolo);
	mshell_elenco_titolo(&t->titolo, NULL);
	t->attiva.notify = mshell_elenco_richiesta_attiva;
	wl_signal_add(&t->voce->events.request_activate, &t->attiva);
}

/* L'app t sparisce: anche la sua voce. */
static void mshell_elenco_sparisce(struct tinywl_toplevel *t) {
	if (t->voce == NULL) {
		return;
	}
	wl_list_remove(&t->titolo.link);
	wl_list_remove(&t->attiva.link);
	wlr_foreign_toplevel_handle_v1_destroy(t->voce);
	t->voce = NULL;
}

/* L'app col focus e' t (NULL: nessuna, per esempio un desktop vuoto). Lo sanno l'elenco e le app
 * stesse ("attiva", che di solito cambia il loro aspetto). Si dice solo a chi cambia, perche'
 * ogni avviso all'app le fa ridisegnare la finestra. */
static void mshell_elenco_attiva(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct tinywl_toplevel *x;
	wl_list_for_each(x, &server->toplevels, link) {
		bool attiva = x == t;
		if (x->xdg_toplevel->scheduled.activated != attiva) {
			wlr_xdg_toplevel_set_activated(x->xdg_toplevel, attiva);
		}
		if (x->voce != NULL) {
			wlr_foreign_toplevel_handle_v1_set_activated(x->voce, attiva);
		}
	}
}

/* Super+spazio: dice alla barra di aprire o chiudere il cassetto (SIGUSR1, vedi in cima). */
static void mshell_livelli_cassetto(struct tinywl_server *server) {
	struct mshell_livello *s;
	wl_list_for_each(s, &server->livelli->superfici, link) {
		const char *nome = s->superficie->namespace;
		if (nome != NULL && strncmp(nome, "mshell-barra", strlen("mshell-barra")) == 0) {
			pid_t pid;
			wl_client_get_credentials(wl_resource_get_client(s->superficie->resource), &pid,
				NULL, NULL);
			kill(pid, SIGUSR1);
			return;
		}
	}
	wlr_log(WLR_ERROR, "mshell: niente cassetto, la barra non c'e'");
}

/* Prepara tutto, all'avvio, dopo l'albero dei desktop: gli alberi dei livelli sotto e sopra i
 * desktop, il protocollo dei livelli (versione 4, quella di wlroots 0.18) e l'elenco. */
static void mshell_livelli_crea(struct tinywl_server *server) {
	struct mshell_livelli *l = calloc(1, sizeof(*l));
	server->livelli = l;
	l->server = server;
	wl_list_init(&l->superfici);
	for (int i = 0; i < MSHELL_LIVELLI; i++) {
		l->albero[i] = wlr_scene_tree_create(&server->scene->tree);
	}
	wlr_scene_node_place_below(&l->albero[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]->node,
		&server->albero_desktop->node);
	wlr_scene_node_place_below(&l->albero[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]->node,
		&server->albero_desktop->node);
	l->shell = wlr_layer_shell_v1_create(server->wl_display, 4);
	l->nuova_superficie.notify = mshell_livelli_nuova_superficie;
	wl_signal_add(&l->shell->events.new_surface, &l->nuova_superficie);
	l->elenco = wlr_foreign_toplevel_manager_v1_create(server->wl_display);
}

