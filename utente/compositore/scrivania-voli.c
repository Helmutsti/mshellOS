/* MSHELL: IL PASSAGGIO DA UN GRUPPO ALL'ALTRO, COME LO STAGE MANAGER (la persona, 23/11/2026:
 * "1", cioe' il gruppo che lascia il centro si rimpicciolisce e vola nella colonna, quello scelto
 * si ingrandisce dalla sua miniatura e arriva al centro; 250 ms con la curva ease-in-out; gli altri
 * riquadri della colonna scorrono al posto nuovo insieme al volo). Non si compila da solo: lo
 * include scrivania.c dopo la colonna, di cui usa le miniature.
 *
 * I VOLI. Le finestre vere non si scalano (perche', in cima ad animazioni.c). In volo si vede invece
 * una MINIATURA della finestra (mshell_mini_in, la stessa della colonna: nodi che puntano alla
 * stessa memoria dei pixel, con l'ombra e gli angoli in scala), rifatta a ogni scatto del timer
 * alla misura di quel momento: dalla finestra vera al suo posto nel riquadro, o al contrario.
 * Costa poco: sono nodi della scena, niente si ridisegna col processore.
 * - Il gruppo che lascia il centro: il suo albero si spegne subito (come prima) e al suo posto
 *   volano le miniature; il suo riquadro nella colonna ha gia' il suo spazio ma si disegna solo
 *   all'arrivo.
 * - Il gruppo che arriva: il suo riquadro sparisce subito dalla colonna; le sue finestre vere si
 *   accendono all'arrivo, prima volano le miniature. La tastiera e' gia' sua da subito; le pill
 *   compaiono all'arrivo.
 * - Le finestre oltre la terza (la colonna ne mostra 3) volano dove va la terza, dietro di lei.
 * - Un altro cambio durante il volo (un clic su un altro gruppo): il volo in corso finisce subito
 *   e parte quello nuovo; la colonna invece scorre dal punto in cui si trova.
 * Con MSHELL_MENO_MOVIMENTO=1 niente voli: tutto cambia subito, come prima. */

/* Prende nota di dove stanno adesso i riquadri della colonna (prima del cambio). */
static void mshell_voli_prima(struct tinywl_server *server, struct mshell_gruppo *entra,
		struct mshell_voli_prima *p) {
	p->n = 0;
	p->entra_y = -1;
	for (int i = 0; i < server->n_gruppi; i++) {
		int y = mshell_colonna_y_vista(server, i);
		p->g[p->n] = server->gruppo[i];
		p->y[p->n++] = y;
		if (server->gruppo[i] == entra) {
			p->entra_y = y;
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

static void mshell_volo_aggiungi(struct tinywl_toplevel *t, struct wlr_box da, struct wlr_box a) {
	if (mshell_voli.n >= MSHELL_VOLI_MAX || da.width <= 0 || a.width <= 0) {
		return;
	}
	struct mshell_volo *v = &mshell_voli.volo[mshell_voli.n++];
	v->t = t;
	v->da = da;
	v->a = a;
	v->albero = wlr_scene_tree_create(mshell_voli.albero);  /* gli ultimi creati stanno sopra */
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

/* Dopo il cambio: "esce" e' il gruppo che ha lasciato il centro (adesso in cima alla colonna, o
 * NULL), "entra" quello arrivato al centro (o NULL). Fa partire i voli e lo scorrimento della
 * colonna. */
static void mshell_voli_parti(struct tinywl_server *server, const struct mshell_voli_prima *p,
		struct mshell_gruppo *esce, struct mshell_gruppo *entra) {
	if (mshell_anim_spenta() || mshell_voli.albero == NULL) {
		return;
	}
	mshell_voli.inizio = mshell_anim_adesso();
	mshell_voli.n = 0;
	mshell_voli.col_n = 0;
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
	/* gli altri riquadri della colonna scorrono dal posto di prima a quello nuovo */
	for (int i = 0; i < server->n_gruppi; i++) {
		for (int j = 0; j < p->n; j++) {
			if (p->g[j] == server->gruppo[i] && p->y[j] != mshell_colonna_y(server, i)) {
				mshell_voli.col_g[mshell_voli.col_n] = p->g[j];
				mshell_voli.col_da[mshell_voli.col_n++] = p->y[j];
				qualcosa = true;
			}
		}
	}
	if (!qualcosa) {
		return;
	}
	mshell_voli.corre = true;
	for (int i = 0; i < mshell_voli.n; i++) {
		mshell_volo_disegna(&mshell_voli.volo[i], 0);
	}
	mshell_anim_avvia();
}

/* Il volo e' finito (o lo interrompe un altro cambio): via le miniature, si accendono le finestre
 * vere del gruppo arrivato, il riquadro del gruppo atterrato si disegna, le pill tornano. */
static void mshell_voli_fine(struct tinywl_server *server) {
	if (!mshell_voli.corre) {
		return;
	}
	mshell_voli.corre = false;
	for (int i = 0; i < mshell_voli.n; i++) {
		wlr_scene_node_destroy(&mshell_voli.volo[i].albero->node);
	}
	mshell_voli.n = 0;
	mshell_voli.col_n = 0;
	if (mshell_voli.arriva != NULL && mshell_voli.arriva == server->centro) {
		wlr_scene_node_set_enabled(&mshell_voli.arriva->albero->node, true);
	}
	mshell_voli.arriva = NULL;
	mshell_voli.atterra = NULL;
	mshell_colonna_rifai(server);
	mshell_pill_aggiorna(server);
	mshell_anim_puntatore();
}

/* Uno scatto del timer delle animazioni (animazioni.c): true se il volo continua. */
static bool mshell_voli_scatto(struct tinywl_server *server, int64_t ora) {
	if (!mshell_voli.corre) {
		return false;
	}
	double e = mshell_anim_curva_dolce(mshell_voli.inizio, MSHELL_DURATA_GRUPPI, ora);
	if (e >= 1) {
		mshell_voli_fine(server);
		return false;
	}
	for (int i = 0; i < mshell_voli.n; i++) {
		mshell_volo_disegna(&mshell_voli.volo[i], e);
	}
	if (mshell_voli.col_n > 0) {
		mshell_colonna_disegna(server);  /* i riquadri al punto di adesso */
	}
	return true;
}

/* Dove si vede adesso il riquadro i-esimo della colonna: a meta' strada, se sta scorrendo. */
static int mshell_colonna_y_vista(struct tinywl_server *server, int i) {
	int y = mshell_colonna_y(server, i);
	if (!mshell_voli.corre) {
		return y;
	}
	for (int j = 0; j < mshell_voli.col_n; j++) {
		if (mshell_voli.col_g[j] == server->gruppo[i]) {
			double e = mshell_anim_curva_dolce(mshell_voli.inizio, MSHELL_DURATA_GRUPPI,
				mshell_anim_adesso());
			return (int)(mshell_voli.col_da[j] + (y - mshell_voli.col_da[j]) * e + 0.5);
		}
	}
	return y;
}

/* Una finestra se ne va (gruppo_esce): non vola piu'; se il suo gruppo sparisce, il volo lo
 * dimentica. */
static void mshell_voli_dimentica(struct tinywl_toplevel *t, struct mshell_gruppo *g, bool sparisce) {
	for (int i = 0; i < mshell_voli.n; i++) {
		if (mshell_voli.volo[i].t == t) {
			mshell_voli.volo[i].t = NULL;
			mshell_volo_disegna(&mshell_voli.volo[i], 0);  /* svuota la miniatura */
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
	for (int j = 0; j < mshell_voli.col_n; j++) {
		if (mshell_voli.col_g[j] == g) {
			mshell_voli.col_g[j] = NULL;
		}
	}
}
