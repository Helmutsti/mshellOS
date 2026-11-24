/* MSHELL: I VOLI DELLE FINESTRE fra il centro e la colonna, COME LO STAGE MANAGER (la persona,
 * 23/11/2026: "il gruppo che lascia il centro si rimpicciolisce e vola nella colonna, quello scelto
 * si ingrandisce dalla sua miniatura e arriva al centro"; 250 ms con la curva ease-in-out; poi
 * "animazioni per ogni passaggio, non voglio vedere cose scattose"). Non si compila da solo: lo
 * include scrivania.c dopo la colonna, di cui usa le miniature.
 *
 * I VOLI. Le finestre vere non si scalano (perche', in cima ad animazioni.c). In volo si vede invece
 * una MINIATURA della finestra (mshell_mini_in, la stessa della colonna: nodi che puntano alla
 * stessa memoria dei pixel, con l'ombra e gli angoli in scala), rifatta a ogni scatto del timer
 * alla misura di quel momento. Costa poco: sono nodi della scena, niente si ridisegna col
 * processore. Ogni volo ha il suo inizio; quando sono finiti tutti, si accende quello che
 * aspettava.
 * - IL PASSAGGIO DA UN GRUPPO ALL'ALTRO (mshell_voli_parti): il gruppo che lascia il centro si
 *   spegne subito e al suo posto volano le miniature verso il suo riquadro, che si disegna solo
 *   all'arrivo; quello che arriva dalla colonna si accende all'arrivo, prima volano le miniature
 *   dal suo riquadro. La tastiera e' gia' sua da subito; le pill compaiono all'arrivo. Le finestre
 *   oltre la terza (la colonna ne mostra 3) volano dove va la terza, dietro di lei.
 * - UNA FINESTRA SOLA VERSO LA COLONNA (mshell_volo_verso_colonna: Super+Z, il pulsante "–").
 * - UNA FINESTRA CHE ENTRA NELLA SCRIVANIA (mshell_volo_entra: Z, lo split, il trascinamento di un
 *   gruppo al centro): vola dalla sua miniatura al suo posto; la finestra vera intanto sta in un
 *   albero spento (mshell_voli.nascoste) e torna nel suo gruppo all'arrivo.
 * - Un altro cambio al centro durante il volo: il volo in corso finisce subito e parte quello
 *   nuovo; la colonna invece scorre dal punto in cui si trova (mshell_colonna_y_vista).
 * Con MSHELL_MENO_MOVIMENTO=1 niente voli: tutto cambia subito. */

/* Prende nota di dove sta adesso il riquadro del gruppo che arriva (prima del cambio). */
static void mshell_voli_prima(struct tinywl_server *server, struct mshell_gruppo *entra,
		struct mshell_voli_prima *p) {
	p->entra_y = -1;
	for (int i = 0; i < server->n_gruppi; i++) {
		if (server->gruppo[i] == entra) {
			p->entra_y = mshell_colonna_y_vista(server, i);
		}
	}
	mshell_voli_fine(server);  /* un volo in corso finisce subito */
}

/* Dove si vede la finestra vera adesso (la finestra vera, senza l'ombra dell'app). */
static struct wlr_box mshell_volo_vero(struct tinywl_toplevel *t) {
	int x, y;
	mshell_anim_dove(t, &x, &y);
	return (struct wlr_box){ x, y, t->posto.width, t->posto.height };
}

/* Dove sta, nel riquadro con la cima in y_riquadro, la miniatura della k-esima finestra da davanti
 * del gruppo g (come mshell_riquadro e mshell_mini). */
static struct wlr_box mshell_volo_mini(struct tinywl_server *server, struct mshell_gruppo *g,
		struct tinywl_toplevel *t, int k, int y_riquadro) {
	if (k > MSHELL_MINI_QUANTE - 1) {
		k = MSHELL_MINI_QUANTE - 1;
	}
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	double riduci = 1 - MSHELL_MINI_RIDUCI * k;
	int l, a;
	mshell_mini_misura(t, MSHELL_MINI_L * riduci, MSHELL_MINI_A * riduci, &l, &a);
	return (struct wlr_box){
		schermo.x + MSHELL_COLONNA_X + MSHELL_MINI_X + MSHELL_MINI_PASSO_X * k,
		y_riquadro + mshell_riquadro_cima(g) - MSHELL_MINI_PASSO_Y * k, l, a,
	};
}

/* Dove si vede adesso la finestra t nella colonna (la sua miniatura nella pila del suo riquadro, o
 * nello split se e' aperto). */
static struct wlr_box mshell_volo_da_colonna(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	int i = -1;
	for (int k = 0; g != NULL && k < server->n_gruppi; k++) {
		if (server->gruppo[k] == g) {
			i = k;
		}
	}
	if (i < 0) {
		return mshell_volo_vero(t);
	}
	for (int j = 0; j < mshell_ts.n_celle; j++) {
		if (mshell_ts.cella_t[j] == t && mshell_ts.aperto == g) {
			return mshell_ts.cella[j];
		}
	}
	int k = 0;
	while (k < g->n && g->app[g->n - 1 - k] != t) {
		k++;
	}
	return mshell_volo_mini(server, g, t, k, mshell_colonna_y_vista(server, i));
}

static struct mshell_volo *mshell_volo_aggiungi(struct tinywl_toplevel *t, struct wlr_box da,
		struct wlr_box a) {
	if (mshell_voli.n >= MSHELL_VOLI_MAX || da.width <= 0 || a.width <= 0) {
		return NULL;
	}
	struct mshell_volo *v = &mshell_voli.volo[mshell_voli.n++];
	*v = (struct mshell_volo){ .t = t, .da = da, .a = a, .inizio = mshell_anim_adesso() };
	v->albero = wlr_scene_tree_create(mshell_voli.albero);  /* gli ultimi creati stanno sopra */
	return v;
}

/* Rifa' la miniatura in volo alla misura del momento "e" (da 0 a 1). */
static void mshell_volo_disegna(struct mshell_volo *v, double e) {
	struct wlr_scene_node *n, *dopo;
	wl_list_for_each_safe(n, dopo, &v->albero->children, link) {
		wlr_scene_node_destroy(n);
	}
	if (v->t == NULL) {
		return;
	}
	double x = v->da.x + (v->a.x - v->da.x) * e, y = v->da.y + (v->a.y - v->da.y) * e;
	double l = v->da.width + (v->a.width - v->da.width) * e;
	double a = v->da.height + (v->a.height - v->da.height) * e;
	mshell_mini_in(v->albero, v->t, (int)(x + 0.5), (int)(y + 0.5), l, a, false);
}

static void mshell_voli_via(void) {
	mshell_voli.corre = true;
	for (int i = 0; i < mshell_voli.n; i++) {
		mshell_volo_disegna(&mshell_voli.volo[i], 0);
	}
	mshell_anim_avvia();
}

/* Dopo il cambio: "esce" e' il gruppo che ha lasciato il centro (adesso in cima alla colonna, o
 * NULL), "entra" quello arrivato al centro (o NULL). Fa partire i voli. */
static void mshell_voli_parti(struct tinywl_server *server, const struct mshell_voli_prima *p,
		struct mshell_gruppo *esce, struct mshell_gruppo *entra) {
	if (mshell_anim_spenta() || mshell_voli.albero == NULL) {
		return;
	}
	bool qualcosa = false;
	/* chi esce: dalle finestre vere ai loro posti nel riquadro, dalla piu' dietro */
	if (esce != NULL && esce->n > 0 && server->n_gruppi > 0 && server->gruppo[0] == esce) {
		int y = mshell_colonna_y(server, 0);
		for (int k = esce->n - 1; k >= 0; k--) {
			struct tinywl_toplevel *t = esce->app[esce->n - 1 - k];
			mshell_volo_aggiungi(t, mshell_volo_vero(t), mshell_volo_mini(server, esce, t, k, y));
		}
		mshell_voli.atterra = esce;
		qualcosa = true;
	}
	/* chi entra (solo se veniva dalla colonna): dal riquadro alle finestre vere */
	if (entra != NULL && entra->n > 0 && p->entra_y >= 0) {
		for (int k = entra->n - 1; k >= 0; k--) {
			struct tinywl_toplevel *t = entra->app[entra->n - 1 - k];
			mshell_volo_aggiungi(t, mshell_volo_mini(server, entra, t, k, p->entra_y), t->posto);
		}
		mshell_voli.arriva = entra;
		qualcosa = true;
	}
	if (qualcosa) {
		mshell_voli_via();
	}
}

/* La finestra t e' appena passata nel gruppo "nuovo" della colonna (da dove si vedeva: "da"):
 * vola nel suo riquadro, che si disegna all'arrivo. */
static void mshell_volo_verso_colonna(struct tinywl_server *server, struct tinywl_toplevel *t,
		struct mshell_gruppo *nuovo, struct wlr_box da) {
	if (mshell_anim_spenta() || mshell_voli.albero == NULL) {
		return;
	}
	mshell_voli_fine(server);
	int i = 0;
	while (i < server->n_gruppi && server->gruppo[i] != nuovo) {
		i++;
	}
	if (i >= server->n_gruppi) {
		return;
	}
	if (mshell_volo_aggiungi(t, da, mshell_volo_mini(server, nuovo, t, 0, mshell_colonna_y(server, i)))) {
		mshell_voli.atterra = nuovo;
		mshell_voli_via();
	}
}

/* La finestra t e' appena entrata nel gruppo al centro, al suo posto: vola da "da" (dove si vedeva
 * nella colonna); la finestra vera aspetta spenta e si accende all'arrivo. */
static void mshell_volo_entra(struct tinywl_server *server, struct tinywl_toplevel *t, struct wlr_box da) {
	if (mshell_anim_spenta() || mshell_voli.albero == NULL) {
		return;
	}
	struct mshell_volo *v = mshell_volo_aggiungi(t, da, t->posto);
	if (v == NULL) {
		return;
	}
	v->accendi = true;
	if (mshell_voli.nascoste == NULL) {
		mshell_voli.nascoste = wlr_scene_tree_create(&server->scene->tree);
		wlr_scene_node_set_enabled(&mshell_voli.nascoste->node, false);
	}
	wlr_scene_node_reparent(&t->scene_tree->node, mshell_voli.nascoste);
	mshell_voli_via();
}

/* La finestra t torna nel suo gruppo, al suo posto fra le altre (l'ultima del gruppo sta davanti). */
static void mshell_volo_rimetti(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	wlr_scene_node_reparent(&t->scene_tree->node, g != NULL ? g->albero : server->albero_gruppi);
	for (int i = 0; g != NULL && i < g->n; i++) {
		wlr_scene_node_raise_to_top(&g->app[i]->scene_tree->node);
	}
}

/* I voli sono finiti (o li interrompe un altro cambio): via le miniature, si accendono le finestre
 * vere che aspettavano, il riquadro del gruppo atterrato si disegna, le pill tornano. */
static void mshell_voli_fine(struct tinywl_server *server) {
	if (!mshell_voli.corre) {
		return;
	}
	mshell_voli.corre = false;
	for (int i = 0; i < mshell_voli.n; i++) {
		struct mshell_volo *v = &mshell_voli.volo[i];
		wlr_scene_node_destroy(&v->albero->node);
		if (v->accendi && v->t != NULL) {
			mshell_volo_rimetti(server, v->t);
		}
	}
	mshell_voli.n = 0;
	if (mshell_voli.arriva != NULL && mshell_voli.arriva == server->centro) {
		wlr_scene_node_set_enabled(&mshell_voli.arriva->albero->node, true);
	}
	mshell_voli.arriva = NULL;
	mshell_voli.atterra = NULL;
	mshell_colonna_rifai(server);
	mshell_pill_aggiorna(server);
	mshell_anim_puntatore();
}

/* Uno scatto del timer delle animazioni (animazioni.c): true se un volo continua. */
static bool mshell_voli_scatto(struct tinywl_server *server, int64_t ora) {
	if (!mshell_voli.corre) {
		return false;
	}
	bool ancora = false;
	for (int i = 0; i < mshell_voli.n; i++) {
		struct mshell_volo *v = &mshell_voli.volo[i];
		double e = mshell_anim_curva_dolce(v->inizio, MSHELL_DURATA_GRUPPI, ora);
		mshell_volo_disegna(v, e);
		ancora = ancora || e < 1;
	}
	if (!ancora) {
		mshell_voli_fine(server);
	}
	return ancora;
}

/* Una finestra se ne va (gruppo_esce): non vola piu'; se il suo gruppo sparisce, il volo lo
 * dimentica. */
static void mshell_voli_dimentica(struct tinywl_toplevel *t, struct mshell_gruppo *g, bool sparisce) {
	for (int i = 0; t != NULL && i < mshell_voli.n; i++) {
		struct mshell_volo *v = &mshell_voli.volo[i];
		if (v->t == t) {
			if (v->accendi) {
				mshell_volo_rimetti(t->server, t);  /* non resta nell'albero spento */
			}
			v->t = NULL;
			mshell_volo_disegna(v, 0);  /* svuota la miniatura */
		}
	}
	if (!sparisce) {
		return;
	}
	if (mshell_voli.arriva == g) {
		mshell_voli.arriva = NULL;
	}
	if (mshell_voli.atterra == g) {
		mshell_voli.atterra = NULL;
	}
}
