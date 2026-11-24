/* MSHELL: LE ANIMAZIONI del compositore. Non si compila da solo: e' incluso da tinywl.c subito
 * dopo effetti.c e prima della scrivania (scrivania.c), di cui usa le strutture. Sta in un file
 * a parte per tenere tinywl.c piccolo: la' ci sono solo le chiamate.
 *
 * COSA SI ANIMA (decisione della persona: "anticipa le animazioni"):
 * - UN'APP CHE CAMBIA POSTO (quando la scrivania la sistema, non quando la si trascina): scorre
 *   dal posto vecchio a quello nuovo. LA MISURA invece cambia subito: si chiede all'app la misura nuova (set_size, come
 *   prima) e intanto si anima solo la posizione. L'altra strada, ingrandire o rimpicciolire
 *   l'immagine dell'app con wlr_scene_buffer_set_dest_size, non regge:
 *     1. la scena di wlroots rimette la misura vera a ogni invio dell'app (surface_reconfigure
 *        in types/scene/surface.c), quindi bisognerebbe riscriverla dopo ogni invio;
 *     2. un'app e' fatta di piu' immagini (sotto-superfici, menu): scalarle una per una non
 *        sposta le loro posizioni, che andrebbero scalate a mano, e si scompongono;
 *     3. i clic vanno alle coordinate vere dell'app: con l'immagine scalata non coinciderebbero
 *        con quello che si vede.
 *   Il prezzo: la misura cambia quando l'app ridisegna, che puo' essere prima o dopo la fine
 *   dello scorrimento; nel frattempo l'app puo' avere la misura vecchia o quella nuova a meta'
 *   strada, e coprire per un attimo la vicina o uscire dal bordo. Nelle prove in QEMU si vede.
 * - UN'APP CHE COMPARE: dissolvenza in entrata (l'opacita' di tutte le sue immagini, menu
 *   compresi, da 0 a 1).
 * - UN'APP CHE SI CHIUDE: dissolvenza in uscita. Quando l'app sparisce la scena di wlroots ne
 *   spegne le immagini; un attimo prima (il nostro ascoltatore e' registrato prima di quello
 *   della scena, vedi mshell_anim_nuova) se ne fa un'ISTANTANEA: per ogni immagine visibile un
 *   nodo nuovo con un riferimento alla stessa memoria dei pixel (wlr_scene_buffer_create la
 *   blocca con wlr_buffer_lock: resta valida anche dopo che l'app se n'e' andata). L'istantanea
 *   sta sotto le finestre, non prende clic, e sparisce a fine dissolvenza (lascia il
 *   riferimento alla memoria).
 *
 *   L'istantanea copia anche gli angoli arrotondati dell'app (effetti.c).
 *
 * IL TEMPO. Un timer del ciclo degli eventi ogni MSHELL_ANIM_PASSO_MS: a ogni scatto si guarda
 * l'orologio e si mette ogni cosa dove deve essere in quel momento. Se il disegno e' lento (in
 * QEMU disegna il processore) si vedono meno passaggi, ma le animazioni finiscono in orario.
 * Un'animazione nuova sulla stessa cosa (tasti premuti in fretta) parte dal punto in cui la cosa
 * si trova in quel momento: niente salti.
 *
 * IL FOCUS E I CLIC. La tastiera passa subito all'app giusta. Mentre le cose scorrono sotto il
 * puntatore fermo, a ogni scatto si ricalcola che cosa c'e' sotto il puntatore, cosi' il clic va
 * all'app che si vede li'.
 *
 * MENO MOVIMENTO: con la variabile d'ambiente MSHELL_MENO_MOVIMENTO=1 niente animazioni, tutto
 * cambia subito. PER ORA e' una variabile d'ambiente: le impostazioni vere arriveranno dopo. */

/* Le durate. I nomi dei token sono una proposta, da confermare: durata/breve e durata/media. */
#define MSHELL_DURATA_BREVE 150  /* token "durata/breve" (proposta, da confermare): dissolvenze */
#define MSHELL_DURATA_MEDIA 250  /* token "durata/media" (proposta, da confermare): scorrimenti */
#define MSHELL_DURATA_CAPSULA 90  /* la capsula sulla finestra (la persona: "una transizione molto rapida") */
#define MSHELL_DURATA_GRUPPI 250  /* il passaggio da un gruppo all'altro (la persona, 23/11/2026: prima 350, poi "abbassa": 250) */
#define MSHELL_ANIM_PASSO_MS 16  /* uno scatto del timer: circa 60 al secondo */

/* Definita piu' avanti in tinywl.c: ricalcola che cosa c'e' sotto il puntatore. */
static void process_cursor_motion(struct tinywl_server *server, uint32_t time);

/* Le animazioni di un'app (tinywl_toplevel->anim). */
struct mshell_anim_app {
	struct tinywl_toplevel *t;
	struct wl_listener sparisce;  /* l'app sparisce: si fa l'istantanea (prima della scena) */
	bool muove;                   /* sta scorrendo da (da_x, da_y) a (a_x, a_y) */
	double da_x, da_y, a_x, a_y;
	int64_t muove_da;
	bool appare;                  /* dissolvenza in entrata in corso */
	int64_t appare_da;
};

/* L'istantanea di un'app chiusa, che svanisce. */
struct mshell_anim_fantasma {
	struct wl_list link;
	struct wlr_scene_tree *albero;
	float partenza;  /* l'opacita' da cui parte (l'app poteva essere ancora a meta' entrata) */
	int64_t inizio;
};

static struct {
	struct tinywl_server *server;
	struct wl_event_source *timer;
	int spenta;  /* MSHELL_MENO_MOVIMENTO: -1 non ancora letta, 0 no, 1 si' */
	struct wl_list fantasmi;
	struct wl_list sfumature;  /* mshell_sfuma.link */
} mshell_anim = { .spenta = -1 };

static bool mshell_anim_spenta(void) {
	if (mshell_anim.spenta < 0) {
		const char *v = getenv("MSHELL_MENO_MOVIMENTO");
		mshell_anim.spenta = v != NULL && strcmp(v, "1") == 0;
	}
	return mshell_anim.spenta;
}

static int64_t mshell_anim_adesso(void) {
	struct timespec ora;
	clock_gettime(CLOCK_MONOTONIC, &ora);
	return (int64_t)ora.tv_sec * 1000 + ora.tv_nsec / 1000000;
}

/* Quanto manca, da 0 (inizio) a 1 (fine), con la curva ease-out cubica: parte veloce e
 * rallenta all'arrivo. */
static double mshell_anim_curva(int64_t inizio, int durata, int64_t ora) {
	double p = (double)(ora - inizio) / durata;
	if (p >= 1) {
		return 1;
	}
	if (p < 0) {
		p = 0;
	}
	double resto = 1 - p;
	return 1 - resto * resto * resto;
}

/* La curva ease-in-out cubica (il passaggio fra i gruppi): parte piano, accelera e rallenta
 * all'arrivo. */
static double mshell_anim_curva_dolce(int64_t inizio, int durata, int64_t ora) {
	double p = (double)(ora - inizio) / durata;
	if (p >= 1) {
		return 1;
	}
	if (p < 0) {
		p = 0;
	}
	if (p < 0.5) {
		return 4 * p * p * p;
	}
	double r = -2 * p + 2;
	return 1 - r * r * r / 2;
}

static int mshell_anim_scatto(void *data);
/* Il passaggio fra i gruppi (scrivania-voli.c): true se continua. */
static bool mshell_voli_scatto(struct tinywl_server *server, int64_t ora);
/* La colonna dei gruppi (scrivania.c): true se qualcosa si muove ancora. */
static bool mshell_colonna_scatto(struct tinywl_server *server, int64_t ora);

/* Fa partire il timer, se non sta gia' andando. */
static void mshell_anim_avvia(void) {
	struct tinywl_server *server = mshell_anim.server;
	if (server == NULL) {
		return;
	}
	if (mshell_anim.timer == NULL) {
		mshell_anim.timer = wl_event_loop_add_timer(
			wl_display_get_event_loop(server->wl_display), mshell_anim_scatto, NULL);
	}
	wl_event_source_timer_update(mshell_anim.timer, MSHELL_ANIM_PASSO_MS);
}

/* Il punto (in alto a sinistra) dove l'app si vede adesso. */
static void mshell_anim_punto(struct tinywl_toplevel *t, int64_t ora, double *x, double *y) {
	struct mshell_anim_app *a = t->anim;
	if (a == NULL || !a->muove) {
		*x = t->posto.x;
		*y = t->posto.y;
		return;
	}
	double e = mshell_anim_curva(a->muove_da, MSHELL_DURATA_MEDIA, ora);
	*x = a->da_x + (a->a_x - a->da_x) * e;
	*y = a->da_y + (a->a_y - a->da_y) * e;
}

/* Usata da mshell_metti_al_posto: dove mettere l'app adesso (a meta' strada se sta scorrendo). */
static void mshell_anim_dove(struct tinywl_toplevel *t, int *x, int *y) {
	double px, py;
	mshell_anim_punto(t, mshell_anim_adesso(), &px, &py);
	*x = (int)(px < 0 ? px - 0.5 : px + 0.5);
	*y = (int)(py < 0 ? py - 0.5 : py + 0.5);
}

/* Ricalcola che cosa c'e' sotto il puntatore fermo (le cose sotto si sono mosse). */
static void mshell_anim_puntatore(void) {
	if (mshell_anim.server == NULL) {
		return;
	}
	process_cursor_motion(mshell_anim.server, (uint32_t)mshell_anim_adesso());
}

/* Al posto di mshell_metti_al_posto, quando la scrivania sistema una finestra: t->posto e' appena
 * cambiato. Se l'app si vede, scorre dal punto in cui si trova adesso (anche a meta' di un'altra
 * corsa) al posto nuovo; se non si vede (un'app nuova, o in un gruppo della colonna) ci va subito. */
static void mshell_anim_sposta(struct tinywl_toplevel *t) {
	struct mshell_anim_app *a = t->anim;
	struct tinywl_server *server = t->server;
	if (a != NULL) {
		int64_t ora = mshell_anim_adesso();
		double x, y;
		bool c_era = a->muove;
		if (a->muove) {
			mshell_anim_punto(t, ora, &x, &y);
		} else {
			/* fermo: si trova dove la geometria di adesso e il nodo dicono */
			struct wlr_box geo;
			wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
			x = t->scene_tree->node.x + geo.x;
			y = t->scene_tree->node.y + geo.y;
		}
		bool si_vede = t->xdg_toplevel->base->surface->mapped && t->gruppo != NULL &&
			t->gruppo == mshell_al_centro(server);
		bool stessa_meta = c_era && a->a_x == t->posto.x && a->a_y == t->posto.y;
		if (stessa_meta) {
			/* sta gia' andando li': continua la sua corsa */
		} else if (!mshell_anim_spenta() && si_vede && (x != t->posto.x || y != t->posto.y)) {
			a->muove = true;
			a->da_x = x;
			a->da_y = y;
			a->a_x = t->posto.x;
			a->a_y = t->posto.y;
			a->muove_da = ora;
			mshell_anim_avvia();
		} else {
			a->muove = false;
		}
	}
	mshell_metti_al_posto(t);
	if (a == NULL || !a->muove) {
		mshell_anim_puntatore();
	}
}

/* La prima volta: il compositore da animare (ce n'e' uno solo) e gli elenchi. */
static void mshell_anim_conosci(struct tinywl_server *server) {
	if (mshell_anim.server == NULL) {
		mshell_anim.server = server;
		wl_list_init(&mshell_anim.fantasmi);
		wl_list_init(&mshell_anim.sfumature);
	}
}

/* LA SFUMATURA DI UN ALBERO (le pill, i suggerimenti, il pannello "Disponi": la persona,
 * 23/11/2026, "non voglio vedere cose scattose"): in MSHELL_DURATA_BREVE tutto quello che c'e'
 * dentro passa da trasparente a pieno (entra) o al contrario (esce: alla fine l'albero si butta).
 * I rettangoli e le ombre non hanno un'opacita': si moltiplica il loro colore (gia' moltiplicato
 * per l'alfa, come vuole wlroots), partendo da quello che avevano; le immagini hanno l'opacita'.
 * Chi butta o rifa' i nodi di un albero che sta sfumando chiama prima mshell_sfuma_ferma. */
#define MSHELL_SFUMA_NODI 96
struct mshell_sfuma {
	struct wl_list link;
	struct wlr_scene_tree *albero;
	int n;
	struct wlr_scene_node *nodo[MSHELL_SFUMA_NODI];
	float colore[MSHELL_SFUMA_NODI][4];  /* il colore pieno (l'opacita' per le immagini) */
	int64_t inizio;
	int durata;
	bool esce;
};

static void mshell_sfuma_raccogli(struct mshell_sfuma *s, struct wlr_scene_tree *t) {
	struct wlr_scene_node *n;
	wl_list_for_each(n, &t->children, link) {
		if (s->n >= MSHELL_SFUMA_NODI) {
			return;
		}
		switch (n->type) {
		case WLR_SCENE_NODE_TREE:
			mshell_sfuma_raccogli(s, wlr_scene_tree_from_node(n));
			break;
		case WLR_SCENE_NODE_RECT:
			memcpy(s->colore[s->n], wlr_scene_rect_from_node(n)->color, sizeof(s->colore[0]));
			s->nodo[s->n++] = n;
			break;
		case WLR_SCENE_NODE_SHADOW:
			memcpy(s->colore[s->n], wlr_scene_shadow_from_node(n)->color, sizeof(s->colore[0]));
			s->nodo[s->n++] = n;
			break;
		case WLR_SCENE_NODE_BUFFER:
			s->colore[s->n][0] = wlr_scene_buffer_from_node(n)->opacity;
			s->nodo[s->n++] = n;
			break;
		default:
			break;
		}
	}
}

static void mshell_sfuma_metti(struct mshell_sfuma *s, float o) {
	for (int i = 0; i < s->n; i++) {
		struct wlr_scene_node *n = s->nodo[i];
		float c[4] = { s->colore[i][0] * o, s->colore[i][1] * o, s->colore[i][2] * o, s->colore[i][3] * o };
		if (n->type == WLR_SCENE_NODE_RECT) {
			wlr_scene_rect_set_color(wlr_scene_rect_from_node(n), c);
		} else if (n->type == WLR_SCENE_NODE_SHADOW) {
			wlr_scene_shadow_set_color(wlr_scene_shadow_from_node(n), c);
		} else if (n->type == WLR_SCENE_NODE_BUFFER) {
			wlr_scene_buffer_set_opacity(wlr_scene_buffer_from_node(n), s->colore[i][0] * o);
		}
	}
}

/* Ferma la sfumatura dell'albero (se ce n'e' una): tutto torna pieno. */
static void mshell_sfuma_ferma(struct wlr_scene_tree *albero) {
	if (mshell_anim.server == NULL) {
		return;
	}
	struct mshell_sfuma *s, *dopo;
	wl_list_for_each_safe(s, dopo, &mshell_anim.sfumature, link) {
		if (s->albero == albero) {
			if (!s->esce) {
				mshell_sfuma_metti(s, 1);
			}
			wl_list_remove(&s->link);
			free(s);
		}
	}
}

static void mshell_sfuma_parti(struct wlr_scene_tree *albero, bool esce, int durata) {
	struct mshell_sfuma *s = calloc(1, sizeof(*s));
	if (s == NULL) {
		if (esce) {
			wlr_scene_node_destroy(&albero->node);
		}
		return;
	}
	s->albero = albero;
	s->esce = esce;
	s->durata = durata;
	s->inizio = mshell_anim_adesso();
	mshell_sfuma_raccogli(s, albero);
	mshell_sfuma_metti(s, esce ? 1 : 0);
	wl_list_insert(&mshell_anim.sfumature, &s->link);
	mshell_anim_avvia();
}

/* Quello che c'e' nell'albero compare sfumando, in "durata" ms. */
static void mshell_sfuma_entra_in(struct wlr_scene_tree *albero, int durata) {
	mshell_sfuma_ferma(albero);
	if (mshell_anim.server == NULL || mshell_anim_spenta() || wl_list_empty(&albero->children)) {
		return;
	}
	mshell_sfuma_parti(albero, false, durata);
}

static void mshell_sfuma_entra(struct wlr_scene_tree *albero) {
	mshell_sfuma_entra_in(albero, MSHELL_DURATA_BREVE);
}

/* Quello che c'e' nell'albero se ne va sfumando in "durata" ms: passa in un albero accanto (stesso
 * posto), che si butta alla fine; l'albero resta vuoto, pronto per altro. */
static void mshell_sfuma_via_in(struct wlr_scene_tree *albero, int durata) {
	mshell_sfuma_ferma(albero);
	if (wl_list_empty(&albero->children)) {
		return;
	}
	if (mshell_anim.server == NULL || mshell_anim_spenta() || albero->node.parent == NULL ||
			!albero->node.enabled) {
		struct wlr_scene_node *n, *dopo;
		wl_list_for_each_safe(n, dopo, &albero->children, link) {
			wlr_scene_node_destroy(n);
		}
		return;
	}
	struct wlr_scene_tree *via = wlr_scene_tree_create(albero->node.parent);
	if (via == NULL) {
		return;
	}
	wlr_scene_node_set_position(&via->node, albero->node.x, albero->node.y);
	wlr_scene_node_place_above(&via->node, &albero->node);
	struct wlr_scene_node *n, *dopo;
	wl_list_for_each_safe(n, dopo, &albero->children, link) {
		wlr_scene_node_reparent(n, via);
	}
	mshell_sfuma_parti(via, true, durata);
}

static void mshell_sfuma_via(struct wlr_scene_tree *albero) {
	mshell_sfuma_via_in(albero, MSHELL_DURATA_BREVE);
}

/* Mette l'opacita' "o" a tutte le immagini di un albero (l'app con i suoi menu). */
static void mshell_anim_opacita_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	wlr_scene_buffer_set_opacity(b, *(float *)data);
}

static void mshell_anim_opacita(struct wlr_scene_tree *albero, float o) {
	wlr_scene_node_for_each_buffer(&albero->node, mshell_anim_opacita_una, &o);
}

/* L'opacita' di adesso di un'app che sta comparendo (1 se e' gia' comparsa). */
static float mshell_anim_quanto_appare(struct tinywl_toplevel *t, int64_t ora) {
	struct mshell_anim_app *a = t->anim;
	if (a == NULL || !a->appare) {
		return 1;
	}
	return (float)mshell_anim_curva(a->appare_da, MSHELL_DURATA_BREVE, ora);
}

/* Usata da xdg_toplevel_map: l'app compare con una dissolvenza. */
static void mshell_anim_compare(struct tinywl_toplevel *t) {
	struct mshell_anim_app *a = t->anim;
	if (a == NULL || mshell_anim_spenta()) {
		return;
	}
	a->muove = false;
	a->appare = true;
	a->appare_da = mshell_anim_adesso();
	mshell_anim_opacita(t->scene_tree, 0);
	mshell_anim_avvia();
}

/* Usata da xdg_toplevel_commit: a ogni invio la scena di wlroots rimette l'opacita' a 1
 * (surface_reconfigure); se l'app sta comparendo si rimette quella giusta, prima del disegno. */
static void mshell_anim_dopo_invio(struct tinywl_toplevel *t) {
	if (t->anim != NULL && t->anim->appare) {
		mshell_anim_opacita(t->scene_tree, mshell_anim_quanto_appare(t, mshell_anim_adesso()));
	}
}

/* Le immagini dell'istantanea non prendono clic: i clic passano a quello che c'e' sotto. */
static bool mshell_anim_niente_clic(struct wlr_scene_buffer *b, double *sx, double *sy) {
	return false;
}

/* Copia un'immagine visibile dell'app nell'istantanea, con la stessa posizione, misura,
 * ritaglio e rotazione. Il nodo nuovo tiene un riferimento alla stessa memoria dei pixel. */
static void mshell_anim_copia(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	struct wlr_scene_tree *albero = data;
	if (b->buffer == NULL) {
		return;
	}
	struct wlr_scene_buffer *copia = wlr_scene_buffer_create(albero, b->buffer);
	if (copia == NULL) {
		return;
	}
	wlr_scene_node_set_position(&copia->node, sx, sy);
	wlr_scene_buffer_set_source_box(copia, &b->src_box);
	wlr_scene_buffer_set_dest_size(copia, b->dst_width, b->dst_height);
	wlr_scene_buffer_set_transform(copia, b->transform);
	wlr_scene_buffer_set_corner_radius(copia, b->corner_radius, b->corners);  /* gli angoli (effetti.c) */
	copia->point_accepts_input = mshell_anim_niente_clic;
}

/* L'app sta sparendo (ascoltatore registrato prima di quello della scena: le sue immagini si
 * vedono ancora). Se si vede, se ne fa l'istantanea che poi svanisce. */
static void mshell_anim_sparisce(struct wl_listener *listener, void *data) {
	struct mshell_anim_app *a = wl_container_of(listener, a, sparisce);
	struct tinywl_toplevel *t = a->t;
	struct tinywl_server *server = t->server;
	if (mshell_anim_spenta() || t->gruppo == NULL || t->gruppo != mshell_al_centro(server) ||
			t->scene_tree == NULL || !t->scene_tree->node.enabled) {
		return;
	}
	/* fuori dal gruppo (il gruppo puo' sparire con la sua ultima finestra), sotto le finestre */
	struct wlr_scene_tree *albero = wlr_scene_tree_create(server->albero_gruppi);
	if (albero == NULL) {
		return;
	}
	/* le coordinate partono dal nodo dell'app, quindi tengono gia' conto della sua posizione */
	wlr_scene_node_for_each_buffer(&t->scene_tree->node, mshell_anim_copia, albero);
	if (wl_list_empty(&albero->children)) {
		wlr_scene_node_destroy(&albero->node);
		return;
	}
	wlr_scene_node_lower_to_bottom(&albero->node);
	struct mshell_anim_fantasma *f = calloc(1, sizeof(*f));
	if (f == NULL) {
		wlr_scene_node_destroy(&albero->node);
		return;
	}
	int64_t ora = mshell_anim_adesso();
	f->albero = albero;
	f->partenza = mshell_anim_quanto_appare(t, ora);
	f->inizio = ora;
	mshell_anim_opacita(albero, f->partenza);
	wl_list_insert(&mshell_anim.fantasmi, &f->link);
	a->appare = false;
	mshell_anim_avvia();
}

/* Usata da server_new_xdg_toplevel PRIMA di creare il nodo della scena dell'app: cosi'
 * l'ascoltatore "sparisce" viene chiamato prima di quello della scena, che spegne le immagini. */
static void mshell_anim_nuova(struct tinywl_server *server, struct tinywl_toplevel *t) {
	mshell_anim_conosci(server);
	struct mshell_anim_app *a = calloc(1, sizeof(*a));
	if (a == NULL) {
		return;  /* senza animazioni: l'app funziona lo stesso */
	}
	a->t = t;
	a->sparisce.notify = mshell_anim_sparisce;
	wl_signal_add(&t->xdg_toplevel->base->surface->events.unmap, &a->sparisce);
	t->anim = a;
}

/* Usata da xdg_toplevel_destroy. */
static void mshell_anim_dimentica(struct tinywl_toplevel *t) {
	if (t->anim == NULL) {
		return;
	}
	wl_list_remove(&t->anim->sparisce.link);
	free(t->anim);
	t->anim = NULL;
}

/* Uno scatto del timer: ogni cosa al suo punto di adesso; si riparte se qualcosa corre ancora. */
static int mshell_anim_scatto(void *data) {
	struct tinywl_server *server = mshell_anim.server;
	int64_t ora = mshell_anim_adesso();
	bool ancora = false;

	struct tinywl_toplevel *t;
	wl_list_for_each(t, &server->toplevels, link) {
		struct mshell_anim_app *a = t->anim;
		if (a == NULL) {
			continue;
		}
		if (a->muove) {
			if (mshell_anim_curva(a->muove_da, MSHELL_DURATA_MEDIA, ora) >= 1) {
				a->muove = false;
			} else {
				ancora = true;
			}
			mshell_metti_al_posto(t);
		}
		if (a->appare) {
			float o = mshell_anim_quanto_appare(t, ora);
			mshell_anim_opacita(t->scene_tree, o);
			if (o >= 1) {
				a->appare = false;
			} else {
				ancora = true;
			}
		}
	}

	struct mshell_sfuma *sf, *sf_dopo;
	wl_list_for_each_safe(sf, sf_dopo, &mshell_anim.sfumature, link) {
		double e = mshell_anim_curva(sf->inizio, sf->durata, ora);
		mshell_sfuma_metti(sf, (float)(sf->esce ? 1 - e : e));
		if (e >= 1) {
			if (sf->esce) {
				wlr_scene_node_destroy(&sf->albero->node);
			}
			wl_list_remove(&sf->link);
			free(sf);
		} else {
			ancora = true;
		}
	}

	struct mshell_anim_fantasma *f, *dopo;
	wl_list_for_each_safe(f, dopo, &mshell_anim.fantasmi, link) {
		double e = mshell_anim_curva(f->inizio, MSHELL_DURATA_BREVE, ora);
		if (e >= 1) {
			wlr_scene_node_destroy(&f->albero->node);  /* lascia la memoria dei pixel */
			wl_list_remove(&f->link);
			free(f);
		} else {
			mshell_anim_opacita(f->albero, f->partenza * (float)(1 - e));
			ancora = true;
		}
	}

	if (mshell_voli_scatto(server, ora)) {
		ancora = true;
	}
	if (mshell_colonna_scatto(server, ora)) {
		ancora = true;
	}

	mshell_anim_puntatore();
	if (ancora) {
		wl_event_source_timer_update(mshell_anim.timer, MSHELL_ANIM_PASSO_MS);
	}
	return 0;
}
