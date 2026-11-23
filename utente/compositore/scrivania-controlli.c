/* MSHELL: I CONTROLLI SULLA FINESTRA, LA SCELTA DELLE DISPOSIZIONI ("Disponi") E IL TRASCINAMENTO
 * DEI GRUPPI DELLA COLONNA. Le regole sono in cima a scrivania.c; questo file non si compila da
 * solo: scrivania.c lo include in fondo. Misure dal disegno (schermate 4, 5, 6 e 7 dello Stage
 * Manager in Figma). */

#define MSHELL_CTRL_MARGINE 10       /* i controlli dal bordo, quando stanno dentro la finestra */
#define MSHELL_CTRL_STACCO 8         /* le pill sopra o sotto la finestra: lo stacco dal bordo */
#define MSHELL_CTRL_SOTTO_BARRA 36   /* sopra la finestra solo se restano sotto la barra (32) */
#define MSHELL_CTRL_ALTO 26          /* l'etichetta e i pulsanti */
#define MSHELL_CTRL_PASSO 6          /* fra un pulsante e l'altro */
#define MSHELL_CTRL_TESTO 12         /* il testo dell'etichetta (testo/etichetta) */
#define MSHELL_CTRL_ICONA 18         /* l'icona dell'app nell'etichetta */
#define MSHELL_CTRL_SINISTRA 6       /* l'etichetta: margine a sinistra (prima dell'icona), */
#define MSHELL_CTRL_DESTRA 10        /* a destra, */
#define MSHELL_CTRL_FRA 6            /* fra l'icona e il testo */
#define MSHELL_DISP_SPAZIO 16        /* le finestre disposte: dai bordi e fra l'una e l'altra */
#define MSHELL_SCELTA_BORDO 24       /* il pannello della scelta: margine interno */
#define MSHELL_SCELTA_MINI_L 184     /* una miniatura di disposizione */
#define MSHELL_SCELTA_MINI_A 115
#define MSHELL_SCELTA_SPAZIO 16      /* fra una miniatura e l'altra */
#define MSHELL_SCELTA_TITOLO 15      /* il titolo del pannello */
#define MSHELL_SCELTA_NOME 12        /* il nome sotto ogni miniatura (testo/etichetta) */
#define MSHELL_SCELTA_RAGGIO 8       /* gli angoli delle miniature */
#define MSHELL_SCELTA_CONTORNO 3     /* il contorno di quella sotto il puntatore (bordo/focus) */
#define MSHELL_SUGG_ALTO 26          /* il suggerimento: alto, */
#define MSHELL_SUGG_LATI 10          /* margine ai lati */
#define MSHELL_TRASCINA_SOGLIA 6     /* oltre questi pixel una pressione diventa un trascinamento */
#define MSHELL_FANTASMA_L 240        /* la miniatura che segue il puntatore (il disegno: 40%) */
#define MSHELL_FANTASMA_A 160
#define MSHELL_FONT_MEDIO "IBM Plex Sans Medium"
#define MSHELL_FONT_FORTE "IBM Plex Sans SemiBold"

enum { P_DISPONI, P_RIDUCI, P_CHIUDI, P_QUANTI };
enum mshell_disp { D_TUTTO, D_COLONNE, D_RIGHE, D_2P1, D_1P2, D_1S2, D_GRIGLIA };

/* LE PILL (la persona, 23/11/2026): la finestra attiva (quella davanti del gruppo al centro) le
 * mostra SEMPRE; un'altra finestra le mostra quando il puntatore e' su di lei o sulla zona delle
 * sue pill. Due serie: [0] la finestra attiva, [1] quella sotto il puntatore (se non e' l'attiva). */
struct mshell_pill {
	struct wlr_scene_tree *albero;  /* dentro mshell_sc.controlli, sopra tutte le finestre */
	struct tinywl_toplevel *t;      /* di chi sono (NULL: nascoste) */
	int l, fila, etichetta_l;       /* larghezza della finestra, y della fila, larghezza del nome */
	char *titolo;                   /* il titolo per cui sono state disegnate */
};
static struct mshell_pill mshell_pill[2];
/* la fila ferma durante il trascinamento di una finestra (vedi mshell_ctrl_y) */
static struct tinywl_toplevel *mshell_ferma_t;
static int mshell_ferma_y, mshell_ferma_lati;
static struct tinywl_toplevel *mshell_scelta_t;  /* la finestra da cui si e' aperta la scelta */

/* Definita in tinywl.c: la finestra (del gruppo al centro) sotto il punto. */
static struct tinywl_toplevel *desktop_toplevel_at(struct tinywl_server *server, double lx,
	double ly, struct wlr_surface **surface, double *sx, double *sy);

/* --- disegnare con cairo e pango --- */

static void mshell_tondo(cairo_t *cr, double x, double y, double l, double a, double r) {
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + l - r, y + r, r, -G_PI / 2, 0);
	cairo_arc(cr, x + l - r, y + a - r, r, 0, G_PI / 2);
	cairo_arc(cr, x + r, y + a - r, r, G_PI / 2, G_PI);
	cairo_arc(cr, x + r, y + r, r, G_PI, 3 * G_PI / 2);
	cairo_close_path(cr);
}

/* Mette nella scena l'immagine disegnata da cairo, in (x, y); NULL se manca la memoria. */
static struct wlr_scene_buffer *mshell_cairo_metti(struct wlr_scene_tree *padre, cairo_surface_t *cs,
		int x, int y) {
	struct mshell_pixel *p = mshell_pixel_da_cairo(cs);
	if (p == NULL) {
		return NULL;
	}
	struct wlr_scene_buffer *b = wlr_scene_buffer_create(padre, &p->base);
	wlr_buffer_drop(&p->base);  /* ora lo tiene la scena */
	if (b != NULL) {
		wlr_scene_node_set_position(&b->node, x, y);
	}
	return b;
}

static PangoLayout *mshell_testo_prepara(cairo_t *cr, const char *testo, const char *font, int px,
		int larghezza_max) {
	PangoLayout *pl = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string(font);
	pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);
	pango_layout_set_font_description(pl, fd);
	pango_font_description_free(fd);
	if (larghezza_max > 0) {
		pango_layout_set_width(pl, larghezza_max * PANGO_SCALE);
		pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_END);
	}
	pango_layout_set_text(pl, testo, -1);
	return pl;
}

/* Quanto e' largo e alto il testo. */
static void mshell_testo_misura(const char *testo, const char *font, int px, int larghezza_max,
		int *l, int *a) {
	cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(cs);
	PangoLayout *pl = mshell_testo_prepara(cr, testo, font, px, larghezza_max);
	pango_layout_get_pixel_size(pl, l, a);
	g_object_unref(pl);
	cairo_destroy(cr);
	cairo_surface_destroy(cs);
}

/* Scrive il testo in (x, y) (angolo in alto a sinistra; "centrata": x e' il centro). */
static void mshell_scritta(struct wlr_scene_tree *padre, const char *testo, const char *font, int px,
		const float colore[4], int x, int y, int larghezza_max, bool centrata) {
	int l, a;
	mshell_testo_misura(testo, font, px, larghezza_max, &l, &a);
	if (l <= 0 || a <= 0) {
		return;
	}
	cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, l, a);
	cairo_t *cr = cairo_create(cs);
	PangoLayout *pl = mshell_testo_prepara(cr, testo, font, px, larghezza_max);
	cairo_set_source_rgba(cr, colore[0], colore[1], colore[2], colore[3]);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_destroy(cr);
	mshell_cairo_metti(padre, cs, centrata ? x - l / 2 : x, y);
	cairo_surface_destroy(cs);
}

/* Un rettangolo pieno, con i colori "dritti" (come nel disegno): wlroots li vuole moltiplicati per
 * l'opacita'. */
static struct wlr_scene_rect *mshell_rett(struct wlr_scene_tree *padre, int x, int y, int l, int a,
		int raggio, const float c[4], float alfa) {
	if (l <= 0 || a <= 0) {
		return NULL;
	}
	float o = c[3] * alfa;
	const float colore[4] = { c[0] * o, c[1] * o, c[2] * o, o };
	struct wlr_scene_rect *r = wlr_scene_rect_create(padre, l, a, colore);
	wlr_scene_node_set_position(&r->node, x, y);
	if (raggio > 0) {
		wlr_scene_rect_set_corner_radius(r, raggio, CORNER_LOCATION_ALL);
	}
	return r;
}

/* Un pulsante tondo con la sua icona (tracciati delle icone Lucide, https://lucide.dev, licenza
 * ISC, su una griglia di 24, come le icone della barra). */
static void mshell_pulsante(struct wlr_scene_tree *padre, int quale, int x, int y) {
	int lato = MSHELL_CTRL_ALTO;
	cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, lato, lato);
	cairo_t *cr = cairo_create(cs);
	const float *f = mshell_sc.colore_elevata, *t = mshell_sc.colore_testo;
	cairo_arc(cr, lato / 2.0, lato / 2.0, lato / 2.0, 0, 2 * G_PI);
	cairo_set_source_rgba(cr, f[0], f[1], f[2], f[3]);
	cairo_fill(cr);
	/* l'icona, al centro, col testo/primario (le misure del disegno) */
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_source_rgba(cr, t[0], t[1], t[2], t[3]);
	double c = lato / 2.0;
	switch (quale) {
	case P_DISPONI:  /* 14 px, tratto 1,4: un riquadro alto a sinistra e due piccoli a destra */
		cairo_set_line_width(cr, 1.4);
		mshell_tondo(cr, c - 6, c - 5.5, 5.5, 11, 1.2);
		mshell_tondo(cr, c + 1.5, c - 5.5, 4.5, 4.5, 1.2);
		mshell_tondo(cr, c + 1.5, c + 1, 4.5, 4.5, 1.2);
		break;
	case P_RIDUCI:  /* 12 px, tratto 1,6: una lineetta di 7 */
		cairo_set_line_width(cr, 1.6);
		cairo_move_to(cr, c - 3.5, c);
		cairo_line_to(cr, c + 3.5, c);
		break;
	case P_CHIUDI:  /* 12 px, tratto 1,6: una croce di 6 */
		cairo_set_line_width(cr, 1.6);
		cairo_move_to(cr, c - 3, c - 3);
		cairo_line_to(cr, c + 3, c + 3);
		cairo_move_to(cr, c + 3, c - 3);
		cairo_line_to(cr, c - 3, c + 3);
		break;
	}
	cairo_stroke(cr);
	cairo_destroy(cr);
	mshell_cairo_metti(padre, cs, x, y);
	cairo_surface_destroy(cs);
}

/* --- il suggerimento ---
 * Un ovale scuro col testo (azione/sfondo e azione/testo, testo/etichetta), vicino al puntatore,
 * come nelle schermate del disegno: sul gruppo della colonna ("Clic: il gruppo va al centro"),
 * mentre si trascina un gruppo sopra il centro ("Lascia al centro: entra nel gruppo") e sul
 * pulsante "–" ("Togli dal gruppo: va nella colonna di sinistra"). Non prende i clic. */

/* Mette il suggerimento "testo" con l'angolo in alto a sinistra in (x, y), dentro lo schermo;
 * NULL lo toglie. Si ridisegna solo se cambia il testo. */
static void mshell_suggerimento(struct tinywl_server *server, const char *testo, int x, int y) {
	if (testo == NULL) {
		if (mshell_sc.suggerimento != NULL) {
			wlr_scene_node_set_enabled(&mshell_sc.suggerimento->node, false);
		}
		return;
	}
	if (mshell_sc.suggerimento == NULL) {
		mshell_sc.suggerimento = wlr_scene_tree_create(mshell_sc.controlli);
	}
	struct wlr_scene_tree *t = mshell_sc.suggerimento;
	if (mshell_sc.suggerimento_testo == NULL || strcmp(mshell_sc.suggerimento_testo, testo) != 0) {
		struct wlr_scene_node *n, *dopo;
		wl_list_for_each_safe(n, dopo, &t->children, link) {
			wlr_scene_node_destroy(n);
		}
		free(mshell_sc.suggerimento_testo);
		mshell_sc.suggerimento_testo = strdup(testo);
		int tl, ta;
		mshell_testo_misura(testo, MSHELL_FONT_MEDIO, MSHELL_CTRL_TESTO, 0, &tl, &ta);
		int l = tl + 2 * MSHELL_SUGG_LATI;
		struct wlr_scene_rect *fondo = mshell_rett(t, 0, 0, l, MSHELL_SUGG_ALTO, MSHELL_SUGG_ALTO / 2,
			mshell_sc.colore_azione, 1.0f);
		if (fondo != NULL) {
			fondo->accepts_input = false;
		}
		mshell_scritta(t, testo, MSHELL_FONT_MEDIO, MSHELL_CTRL_TESTO, mshell_sc.colore_azione_testo,
			MSHELL_SUGG_LATI, (MSHELL_SUGG_ALTO - ta) / 2, 0, false);
		struct wlr_scene_node *ultimo = wl_container_of(t->children.prev, ultimo, link);
		if (ultimo->type == WLR_SCENE_NODE_BUFFER) {
			wlr_scene_buffer_from_node(ultimo)->point_accepts_input = mshell_niente_clic;
		}
		mshell_sc.suggerimento_l = l;
	}
	struct wlr_box s;
	wlr_output_layout_get_box(server->output_layout, NULL, &s);
	if (x + mshell_sc.suggerimento_l > s.x + s.width - MSHELL_DISP_SPAZIO) {
		x = s.x + s.width - MSHELL_DISP_SPAZIO - mshell_sc.suggerimento_l;
	}
	wlr_scene_node_set_position(&t->node, x, y);
	wlr_scene_node_set_enabled(&t->node, true);
	wlr_scene_node_raise_to_top(&t->node);
}

/* --- i controlli sulla finestra --- */

/* DOVE STANNO I CONTROLLI (la persona, 23/11/2026: pill sopra la finestra, non dentro; sotto se
 * sopra e' piu' comodo). Se il gruppo e' disposto, dentro la finestra in alto. Altrimenti: sopra la finestra, staccati di MSHELL_CTRL_STACCO, se restano sotto la
 * barra; altrimenti sotto, se c'e' posto prima del fondo dello schermo; altrimenti (una finestra
 * alta quanto lo schermo) dentro, in alto. Restituisce la y della fila rispetto alla finestra vera
 * e mette in *lati il margine dai suoi bordi a sinistra e a destra. */
static int mshell_ctrl_y_calcola(struct tinywl_toplevel *t, int *lati) {
	/* LE FINESTRE DISPOSTE ("Disponi"): pill sempre dentro, in alto (la persona, 23/11/2026,
	 * strada A del disegno "Pill e finestre disposte": fuori si accavallavano fra una finestra e
	 * l'altra) */
	if (t->gruppo != NULL && t->gruppo->disposizione >= 0) {
		*lati = MSHELL_CTRL_MARGINE;
		return MSHELL_CTRL_MARGINE;
	}
	struct wlr_box s;
	wlr_output_layout_get_box(t->server->output_layout, NULL, &s);
	*lati = 0;
	int sopra = -MSHELL_CTRL_STACCO - MSHELL_CTRL_ALTO;
	if (t->posto.y + sopra >= s.y + MSHELL_CTRL_SOTTO_BARRA) {
		return sopra;
	}
	int sotto = t->posto.height + MSHELL_CTRL_STACCO;
	if (t->posto.y + sotto + MSHELL_CTRL_ALTO <= s.y + s.height - 4) {
		return sotto;
	}
	*lati = MSHELL_CTRL_MARGINE;
	return MSHELL_CTRL_MARGINE;
}

/* Come sopra, ma MENTRE SI TRASCINA o si ridimensiona la finestra la fila resta dove era quando
 * e' cominciato (la persona, 23/11/2026: altrimenti, portando la finestra in alto, le pill
 * saltavano sotto proprio mentre le si teneva); si sposta quando si lascia il tasto. */
static int mshell_ctrl_y(struct tinywl_toplevel *t, int *lati) {
	if (t == mshell_ferma_t && t->server->cursor_mode != TINYWL_CURSOR_PASSTHROUGH &&
			t->server->grabbed_toplevel == t) {
		*lati = mshell_ferma_lati;
		return mshell_ferma_y;
	}
	return mshell_ctrl_y_calcola(t, lati);
}

/* All'inizio di uno spostamento della finestra t: la sua fila di pill si ferma li'. */
static void mshell_ctrl_ferma(struct tinywl_toplevel *t) {
	mshell_ferma_t = t;
	mshell_ferma_y = mshell_ctrl_y_calcola(t, &mshell_ferma_lati);
}

/* Dove sta il pulsante "quale" della finestra t, sullo schermo. */
static struct wlr_box mshell_pulsante_box(struct tinywl_toplevel *t, int quale) {
	int lati, y = mshell_ctrl_y(t, &lati);
	int destra = t->posto.x + t->posto.width - lati;
	int x = destra - MSHELL_CTRL_ALTO - (P_QUANTI - 1 - quale) * (MSHELL_CTRL_ALTO + MSHELL_CTRL_PASSO);
	return (struct wlr_box){ x, t->posto.y + y, MSHELL_CTRL_ALTO, MSHELL_CTRL_ALTO };
}

/* Il nome della finestra t, sullo schermo (largo 0 se le sue pill non si vedono). */
static struct wlr_box mshell_etichetta_box(struct tinywl_toplevel *t) {
	int lati, y = mshell_ctrl_y(t, &lati);
	int l = 0;
	for (int k = 0; k < 2; k++) {
		if (mshell_pill[k].t == t) {
			l = mshell_pill[k].etichetta_l;
		}
	}
	return (struct wlr_box){ t->posto.x + lati, t->posto.y + y, l, MSHELL_CTRL_ALTO };
}

/* La striscia dei controlli piu' lo stacco dalla finestra: finche' il puntatore e' li', i controlli
 * restano (passando dalla finestra alle pill). */
static struct wlr_box mshell_ctrl_striscia(struct tinywl_toplevel *t) {
	int lati, y = mshell_ctrl_y(t, &lati);
	int da = y < 0 ? y : (y >= t->posto.height ? t->posto.height : y);
	int a = y < 0 ? 0 : y + MSHELL_CTRL_ALTO;
	return (struct wlr_box){ t->posto.x, t->posto.y + da, t->posto.width, a - da };
}

/* Il nome dell'app da mostrare, con l'iniziale maiuscola ("nautilus" -> "Nautilus"). */
static void mshell_nome_app(const char *app_id, char *dove, size_t quanto) {
	snprintf(dove, quanto, "%s", mshell_nome_corto(app_id));
	if (dove[0] >= 'a' && dove[0] <= 'z') {
		dove[0] = (char)(dove[0] - 'a' + 'A');
	}
}

/* Rifa' le pill p per la finestra t (nelle coordinate della finestra vera). */
static void mshell_pill_rifai(struct mshell_pill *p, struct tinywl_toplevel *t) {
	struct wlr_scene_tree *albero = p->albero;
	struct wlr_scene_node *n, *dopo;
	wl_list_for_each_safe(n, dopo, &albero->children, link) {
		wlr_scene_node_destroy(n);
	}
	p->etichetta_l = 0;
	if (t == NULL) {
		return;
	}
	/* l'etichetta: icona e "App – Titolo", in un ovale; non oltre i pulsanti */
	char app[64], testo[320];
	mshell_nome_app(t->xdg_toplevel->app_id, app, sizeof(app));
	const char *titolo = t->xdg_toplevel->title;
	if (titolo != NULL && titolo[0] != '\0' && strcmp(titolo, app) != 0) {
		snprintf(testo, sizeof(testo), "%s – %s", app, titolo);
	} else {
		snprintf(testo, sizeof(testo), "%s", app);
	}
	int lati, cy = mshell_ctrl_y(t, &lati);
	int spazio = t->posto.width - 2 * lati - P_QUANTI * (MSHELL_CTRL_ALTO + MSHELL_CTRL_PASSO)
		- MSHELL_CTRL_PASSO;
	int massimo = spazio - (MSHELL_CTRL_SINISTRA + MSHELL_CTRL_ICONA + MSHELL_CTRL_FRA + MSHELL_CTRL_DESTRA);
	if (massimo > 20) {
		int tl, ta;
		mshell_testo_misura(testo, MSHELL_FONT_MEDIO, MSHELL_CTRL_TESTO, massimo, &tl, &ta);
		int l = MSHELL_CTRL_SINISTRA + MSHELL_CTRL_ICONA + MSHELL_CTRL_FRA + tl + MSHELL_CTRL_DESTRA;
		mshell_rett(albero, lati, cy, l, MSHELL_CTRL_ALTO,
			MSHELL_CTRL_ALTO / 2, mshell_sc.colore_elevata, 1.0f);
		struct mshell_pixel *ic = mshell_icona(t->xdg_toplevel->app_id);
		if (ic != NULL) {
			struct wlr_scene_buffer *b = wlr_scene_buffer_create(albero, &ic->base);
			wlr_scene_buffer_set_dest_size(b, MSHELL_CTRL_ICONA, MSHELL_CTRL_ICONA);
			wlr_scene_node_set_position(&b->node, lati + MSHELL_CTRL_SINISTRA,
				cy + (MSHELL_CTRL_ALTO - MSHELL_CTRL_ICONA) / 2);
		}
		mshell_scritta(albero, testo, MSHELL_FONT_MEDIO, MSHELL_CTRL_TESTO,
			mshell_sc.colore_testo,
			lati + MSHELL_CTRL_SINISTRA + MSHELL_CTRL_ICONA + MSHELL_CTRL_FRA,
			cy + (MSHELL_CTRL_ALTO - ta) / 2, massimo, false);
		p->etichetta_l = l;
	}
	/* i pulsanti, a destra */
	for (int i = 0; i < P_QUANTI; i++) {
		struct wlr_box b = mshell_pulsante_box(t, i);
		mshell_pulsante(albero, i, b.x - t->posto.x, b.y - t->posto.y);
	}
}

/* Le pill p vanno sulla finestra t (NULL: nascoste). Si rifanno solo se cambia qualcosa
 * (finestra, larghezza, titolo, posto della fila), altrimenti si spostano e basta. */
static void mshell_pill_metti(struct mshell_pill *p, struct tinywl_toplevel *t) {
	if (p->albero == NULL) {
		return;
	}
	const char *titolo = t != NULL && t->xdg_toplevel->title != NULL ? t->xdg_toplevel->title : "";
	int lati, fila = t != NULL ? mshell_ctrl_y(t, &lati) : 0;
	bool uguali = t == p->t && (t == NULL || (t->posto.width == p->l && fila == p->fila &&
		p->titolo != NULL && strcmp(titolo, p->titolo) == 0));
	if (!uguali) {
		p->t = t;
		p->l = t != NULL ? t->posto.width : 0;
		p->fila = fila;
		free(p->titolo);
		p->titolo = strdup(titolo);
		mshell_pill_rifai(p, t);
	}
	if (t != NULL) {
		wlr_scene_node_set_position(&p->albero->node, t->posto.x, t->posto.y);
	}
}

/* Le pill della finestra attiva e di quella sotto il puntatore (mshell_sc.sotto). */
static void mshell_pill_aggiorna(struct tinywl_server *server) {
	struct mshell_gruppo *c = mshell_al_centro(server);
	struct tinywl_toplevel *attiva = c != NULL && c->n > 0 ? c->app[c->n - 1] : NULL;
	mshell_pill_metti(&mshell_pill[0], attiva);
	mshell_pill_metti(&mshell_pill[1], mshell_sc.sotto != attiva ? mshell_sc.sotto : NULL);
}

/* La finestra sotto il puntatore e' t (NULL: nessuna): le sue pill compaiono. */
static void mshell_controlli_metti(struct tinywl_server *server, struct tinywl_toplevel *t) {
	mshell_sc.sotto = t;
	mshell_pill_aggiorna(server);
}

/* La finestra t si e' mossa (o ha cambiato misura o titolo): le sue pill la seguono. */
static void mshell_controlli_segui(struct tinywl_toplevel *t) {
	for (int k = 0; k < 2 && t != NULL; k++) {
		if (mshell_pill[k].t == t) {
			mshell_pill_metti(&mshell_pill[k], t);
		}
	}
}

/* --- le disposizioni --- */

/* La parte i-esima di "quante" parti uguali dell'area, in colonne o in righe, con lo spazio fra
 * l'una e l'altra; i pixel che avanzano vanno all'ultima. */
static struct wlr_box mshell_parte(struct wlr_box a, int quante, int i, bool colonne) {
	int tot = (colonne ? a.width : a.height) - MSHELL_DISP_SPAZIO * (quante - 1);
	int base = tot / quante;
	int inizio = i * (base + MSHELL_DISP_SPAZIO);
	int misura = (i == quante - 1) ? tot - base * (quante - 1) : base;
	if (colonne) {
		a.x += inizio;
		a.width = misura;
	} else {
		a.y += inizio;
		a.height = misura;
	}
	return a;
}

/* Il posto della finestra i-esima (0: quella davanti) nella disposizione d con n finestre. */
static struct wlr_box mshell_disp_posto(int d, int n, int i, struct wlr_box a) {
	switch (d) {
	case D_COLONNE:
		return mshell_parte(a, n, i, true);
	case D_RIGHE:
		return mshell_parte(a, n, i, false);
	case D_2P1:  /* la prima a sinistra, le altre impilate a destra */
		return i == 0 ? mshell_parte(a, 2, 0, true) : mshell_parte(mshell_parte(a, 2, 1, true), 2, i - 1, false);
	case D_1P2:  /* la prima a destra, le altre impilate a sinistra */
		return i == 0 ? mshell_parte(a, 2, 1, true) : mshell_parte(mshell_parte(a, 2, 0, true), 2, i - 1, false);
	case D_1S2:  /* la prima sopra, le altre affiancate sotto */
		return i == 0 ? mshell_parte(a, 2, 0, false) : mshell_parte(mshell_parte(a, 2, 1, false), 2, i - 1, true);
	case D_GRIGLIA:
		return mshell_parte(mshell_parte(a, 2, i / 2, false), 2, i % 2, true);
	default:
		return a;
	}
}

/* Le disposizioni possibili con n finestre (al massimo 4), la prima e' quella di partenza. */
static int mshell_disp_scelte(int n, int scelte[4]) {
	switch (n) {
	case 1:
		scelte[0] = D_TUTTO;
		return 1;
	case 2:
		scelte[0] = D_COLONNE; scelte[1] = D_RIGHE;
		return 2;
	case 3:
		scelte[0] = D_COLONNE; scelte[1] = D_2P1; scelte[2] = D_1P2; scelte[3] = D_1S2;
		return 4;
	case 4:
		scelte[0] = D_GRIGLIA; scelte[1] = D_COLONNE;
		return 2;
	default:
		scelte[0] = D_COLONNE;
		return n > 0 ? 1 : 0;
	}
}

/* Il nome della disposizione, come nel disegno ("1:1:1", "2+1", "1+2", "1 / 2"). */
static void mshell_disp_nome(int d, int n, char *dove, size_t quanto) {
	switch (d) {
	case D_TUTTO: snprintf(dove, quanto, "Tutto lo schermo"); return;
	case D_RIGHE: snprintf(dove, quanto, "1 / 1"); return;
	case D_2P1: snprintf(dove, quanto, "2+1"); return;
	case D_1P2: snprintf(dove, quanto, "1+2"); return;
	case D_1S2: snprintf(dove, quanto, "1 / 2"); return;
	case D_GRIGLIA: snprintf(dove, quanto, "2:2"); return;
	default:
		dove[0] = '\0';
		for (int i = 0; i < n && strlen(dove) + 3 < quanto; i++) {
			strcat(dove, i == 0 ? "1" : ":1");
		}
	}
}

/* Lo spazio delle finestre disposte: tutto lo schermo sotto la barra, 16 dai bordi. */
static struct wlr_box mshell_disp_area(struct tinywl_server *server) {
	struct wlr_box a;
	wlr_output_layout_get_box(server->output_layout, NULL, &a);
	a.x += MSHELL_DISP_SPAZIO;
	a.width -= 2 * MSHELL_DISP_SPAZIO;
	a.y += MSHELL_SOTTO_BARRA;
	a.height -= MSHELL_SOTTO_BARRA + MSHELL_DISP_SPAZIO;
	return a;
}

/* Dispone le finestre del gruppo secondo d: scorrono ai loro posti (animazioni.c). */
static void mshell_disponi_gruppo(struct tinywl_server *server, struct mshell_gruppo *g, int d) {
	g->disposizione = d;
	struct wlr_box a = mshell_disp_area(server);
	for (int i = 0; i < g->n; i++) {
		struct tinywl_toplevel *t = g->app[g->n - 1 - i];  /* la prima e' quella davanti */
		t->posto = mshell_disp_posto(d, g->n, i, a);
		if (t->xdg_toplevel->base->initialized) {
			wlr_xdg_toplevel_set_size(t->xdg_toplevel, t->posto.width, t->posto.height);
		}
		mshell_anim_sposta(t);
	}
}

/* Le finestre del gruppo sono cambiate: se era disposto, si ridispone (la stessa disposizione se
 * va bene per il nuovo numero di finestre, altrimenti la prima). */
static void mshell_ridisponi(struct mshell_gruppo *g) {
	if (g->disposizione < 0 || g->n == 0) {
		return;
	}
	int scelte[4];
	int k = mshell_disp_scelte(g->n, scelte);
	int d = scelte[0];
	for (int i = 0; i < k; i++) {
		if (scelte[i] == g->disposizione) {
			d = g->disposizione;
		}
	}
	mshell_disponi_gruppo(mshell_sc.server, g, d);
}

/* Spostata o ridimensionata a mano: il suo gruppo non e' piu' disposto. */
static void mshell_libera(struct tinywl_toplevel *t) {
	mshell_ctrl_ferma(t);  /* chiamata all'inizio di ogni spostamento: la fila di pill si ferma */
	if (t->gruppo != NULL) {
		t->gruppo->disposizione = -1;
	}
}

/* --- la scelta delle disposizioni --- */

static void mshell_scelta_chiudi(void) {
	if (mshell_sc.scelta != NULL) {
		wlr_scene_node_destroy(&mshell_sc.scelta->node);
		mshell_sc.scelta = NULL;
	}
	mshell_sc.scelta_gruppo = NULL;
}

/* Disegna il pannello della scelta per il gruppo g, sotto i controlli della finestra t. */
static void mshell_scelta_disegna(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = mshell_sc.scelta_gruppo;
	if (mshell_sc.scelta != NULL) {
		wlr_scene_node_destroy(&mshell_sc.scelta->node);
	}
	mshell_sc.scelta = wlr_scene_tree_create(mshell_sc.controlli);
	int scelte[4];
	int k = mshell_disp_scelte(g->n, scelte);
	mshell_sc.n_opzioni = k;
	int pl = 2 * MSHELL_SCELTA_BORDO + k * MSHELL_SCELTA_MINI_L + (k - 1) * MSHELL_SCELTA_SPAZIO;
	int pa = MSHELL_SCELTA_BORDO + 20 + 16 + MSHELL_SCELTA_MINI_A + 8 + 16 + MSHELL_SCELTA_BORDO;
	if (mshell_sc.scelta_box.width == 0) {
		/* la prima volta: sotto i pulsanti della finestra, allineato al suo bordo destro, dentro
		 * lo schermo */
		struct wlr_box s;
		wlr_output_layout_get_box(server->output_layout, NULL, &s);
		int x = t->posto.x + t->posto.width - pl;
		int y = mshell_pulsante_box(t, 0).y + MSHELL_CTRL_ALTO + MSHELL_CTRL_MARGINE;
		if (x + pl > s.x + s.width - MSHELL_DISP_SPAZIO) {
			x = s.x + s.width - MSHELL_DISP_SPAZIO - pl;
		}
		if (x < s.x + MSHELL_DISP_SPAZIO) {
			x = s.x + MSHELL_DISP_SPAZIO;
		}
		if (y + pa > s.y + s.height - MSHELL_DISP_SPAZIO) {
			y = s.y + s.height - MSHELL_DISP_SPAZIO - pa;
		}
		mshell_sc.scelta_box = (struct wlr_box){ x, y, pl, pa };
	}
	struct wlr_box b = mshell_sc.scelta_box;
	struct wlr_scene_tree *p = mshell_sc.scelta;
	wlr_scene_node_set_position(&p->node, b.x, b.y);
	/* il pannello, con l'ombra/sovrapposto */
	for (int i = MSHELL_STRATI_SOVRAPPOSTO - 1; i >= 0; i--) {
		mshell_effetti_ombra(p, (struct wlr_box){ 0, 0, pl, pa }, mshell_sc.raggio_pannello,
			&mshell_ombre_sovrapposto[i]);
	}
	mshell_rett(p, 0, 0, pl, pa, mshell_sc.raggio_pannello, mshell_sc.colore_elevata, 1.0f);
	char titolo[96];
	if (g->n == 1) {
		snprintf(titolo, sizeof(titolo), "Disponi la finestra");
	} else {
		snprintf(titolo, sizeof(titolo), "Disponi le %d finestre del gruppo", g->n);
	}
	mshell_scritta(p, titolo, MSHELL_FONT_FORTE, MSHELL_SCELTA_TITOLO, mshell_sc.colore_testo,
		MSHELL_SCELTA_BORDO, MSHELL_SCELTA_BORDO, 0, false);
	struct wlr_box area = mshell_disp_area(server);
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	double sx = (double)MSHELL_SCELTA_MINI_L / schermo.width;
	double sy = (double)MSHELL_SCELTA_MINI_A / schermo.height;
	struct wlr_buffer *sfondo = mshell_sc.sfondo;  /* l'immagine dello sfondo, per le anteprime */
	for (int j = 0; j < k; j++) {
		int x = MSHELL_SCELTA_BORDO + j * (MSHELL_SCELTA_MINI_L + MSHELL_SCELTA_SPAZIO);
		int y = MSHELL_SCELTA_BORDO + 20 + 16;
		bool accesa = j == mshell_sc.opzione_accesa;
		mshell_sc.opzione[j] = (struct wlr_box){ b.x + x, b.y + y, MSHELL_SCELTA_MINI_L, MSHELL_SCELTA_MINI_A };
		mshell_sc.opzione_disp[j] = scelte[j];
		/* l'anteprima: lo schermo rimpicciolito (lo sfondo) con le finestre di vetro ai loro posti */
		if (sfondo != NULL) {
			struct wlr_scene_buffer *b = wlr_scene_buffer_create(p, sfondo);
			struct wlr_fbox src = { 0, 0,
				sfondo->width < schermo.width ? sfondo->width : schermo.width,
				sfondo->height < schermo.height ? sfondo->height : schermo.height };
			wlr_scene_buffer_set_source_box(b, &src);
			wlr_scene_buffer_set_dest_size(b, MSHELL_SCELTA_MINI_L, MSHELL_SCELTA_MINI_A);
			wlr_scene_buffer_set_corner_radius(b, MSHELL_SCELTA_RAGGIO, CORNER_LOCATION_ALL);
			wlr_scene_node_set_position(&b->node, x, y);
		} else {
			mshell_rett(p, x, y, MSHELL_SCELTA_MINI_L, MSHELL_SCELTA_MINI_A, MSHELL_SCELTA_RAGGIO,
				mshell_sc.colore_vetro, 1.0f);
		}
		for (int i = 0; i < g->n; i++) {
			struct wlr_box w = mshell_disp_posto(scelte[j], g->n, i, area);
			int wx = x + (int)((w.x - schermo.x) * sx + 0.5), wy = y + (int)((w.y - schermo.y) * sy + 0.5);
			int wl = (int)(w.width * sx + 0.5), wa = (int)(w.height * sy + 0.5);
			/* le finestre di vetro (superficie/vetro), come nel disegno */
			mshell_rett(p, wx, wy, wl, wa, (int)(MSHELL_RAGGIO_FINESTRA * sx + 0.5), mshell_sc.colore_vetro, 1.0f);
		}
		if (accesa) {  /* il contorno, dentro l'anteprima */
			int k2 = MSHELL_SCELTA_CONTORNO;
			struct wlr_scene_rect *c = mshell_rett(p, x, y, MSHELL_SCELTA_MINI_L, MSHELL_SCELTA_MINI_A,
				MSHELL_SCELTA_RAGGIO, mshell_sc.colore_focus, 1.0f);
			wlr_scene_rect_set_clipped_region(c, (struct clipped_region){
				.area = { k2, k2, MSHELL_SCELTA_MINI_L - 2 * k2, MSHELL_SCELTA_MINI_A - 2 * k2 },
				.corner_radius = MSHELL_SCELTA_RAGGIO - k2,
				.corners = CORNER_LOCATION_ALL,
			});
		}
		char nome[32];
		mshell_disp_nome(scelte[j], g->n, nome, sizeof(nome));
		mshell_scritta(p, nome, MSHELL_FONT_MEDIO, MSHELL_SCELTA_NOME, mshell_sc.colore_testo,
			x + MSHELL_SCELTA_MINI_L / 2, y + MSHELL_SCELTA_MINI_A + 8, 0, true);
	}
}

static void mshell_scelta_apri(struct tinywl_server *server, struct tinywl_toplevel *t) {
	mshell_scelta_chiudi();
	mshell_sc.scelta_gruppo = t->gruppo;
	mshell_scelta_t = t;
	mshell_sc.scelta_box = (struct wlr_box){ 0 };
	mshell_sc.opzione_accesa = -1;
	mshell_scelta_disegna(server, t);
}

/* --- togliere dal gruppo e unire --- */

/* La finestra t lascia il suo gruppo (il pulsante "–"): diventa un gruppo a se', in cima alla
 * colonna. Se era sola, scende nella colonna tutto il suo gruppo e il centro resta vuoto. */
static void mshell_togli_dal_gruppo(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	if (g == NULL) {
		return;
	}
	if (g->n == 1) {
		if (g == server->centro) {
			mshell_svuota_centro(server);
		}
		return;
	}
	struct mshell_gruppo *nuovo = mshell_gruppo_nuovo(server);
	if (nuovo == NULL) {
		return;
	}
	mshell_colonna_in_cima(server, nuovo);
	int i = 0;
	while (i < g->n && g->app[i] != t) {
		i++;
	}
	memmove(&g->app[i], &g->app[i + 1], (size_t)(g->n - 1 - i) * sizeof(g->app[0]));
	g->n--;
	mshell_gruppo_entra(nuovo, t);
	if (mshell_sc.sotto == t) {
		mshell_controlli_metti(server, NULL);
	}
	mshell_ridisponi(g);
	mshell_focus_centro(server);
	mshell_colonna_rifai(server);
	mshell_anim_puntatore();
}

/* Il gruppo g (della colonna) e' stato trascinato al centro e lasciato in (x, y): le sue
 * finestre entrano nel gruppo al centro, quella davanti centrata dove lo si e' lasciato. */
static void mshell_unisci(struct tinywl_server *server, struct mshell_gruppo *g, double x, double y) {
	struct mshell_gruppo *c = mshell_al_centro(server);
	if (c == g) {
		return;
	}
	if (c == NULL) {
		mshell_porta_al_centro(server, g);  /* centro vuoto: il gruppo va al centro com'e' */
		return;
	}
	struct wlr_box area = mshell_area(server);
	struct tinywl_toplevel *davanti = NULL;
	while (g->n > 0 && c->n < MSHELL_GRUPPO_MAX) {
		struct tinywl_toplevel *t = g->app[0];  /* dal fondo, cosi' quella davanti resta davanti */
		memmove(&g->app[0], &g->app[1], (size_t)(g->n - 1) * sizeof(g->app[0]));
		g->n--;
		mshell_gruppo_entra(c, t);
		davanti = t;
	}
	if (davanti != NULL) {
		davanti->posto.x = (int)x - davanti->posto.width / 2;
		davanti->posto.y = (int)y - davanti->posto.height / 2;
		if (davanti->posto.x + davanti->posto.width > area.x + area.width) {
			davanti->posto.x = area.x + area.width - davanti->posto.width;
		}
		if (davanti->posto.x < area.x) {
			davanti->posto.x = area.x;
		}
		if (davanti->posto.y + davanti->posto.height > area.y + area.height) {
			davanti->posto.y = area.y + area.height - davanti->posto.height;
		}
		if (davanti->posto.y < area.y) {
			davanti->posto.y = area.y;
		}
		mshell_metti_al_posto(davanti);
	}
	if (g->n == 0) {
		mshell_colonna_togli(server, g);
		if (mshell_sc.acceso == g) {
			mshell_sc.acceso = NULL;
		}
		wlr_scene_node_destroy(&g->albero->node);
		free(g);
	}
	mshell_ridisponi(c);
	mshell_focus_centro(server);
	mshell_colonna_rifai(server);
}

/* --- il puntatore e i clic --- */

static bool mshell_dentro(struct wlr_box b, double x, double y) {
	return x >= b.x && x < b.x + b.width && y >= b.y && y < b.y + b.height;
}

/* Il puntatore si e' mosso. Vero se il movimento e' tutto della scrivania (trascinamento di un
 * gruppo, puntatore sulla scelta delle disposizioni): alle app non va. */
static bool mshell_scrivania_puntatore(struct tinywl_server *server) {
	double px = server->cursor->x, py = server->cursor->y;
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);

	/* IL TRASCINAMENTO di un gruppo della colonna */
	if (mshell_sc.premuto != NULL) {
		struct mshell_gruppo *g = mshell_sc.premuto;
		if (!mshell_sc.trascina && (fabs(px - mshell_sc.premuto_x) > MSHELL_TRASCINA_SOGLIA ||
				fabs(py - mshell_sc.premuto_y) > MSHELL_TRASCINA_SOGLIA) && g->n > 0) {
			mshell_sc.trascina = true;
			mshell_sc.fantasma = wlr_scene_tree_create(mshell_sc.controlli);
			mshell_mini_in(mshell_sc.fantasma, g->app[g->n - 1], 0, 0, MSHELL_FANTASMA_L, MSHELL_FANTASMA_A);
			mshell_sc.fantasma_dx = -MSHELL_FANTASMA_L / 2;
			mshell_sc.fantasma_dy = -MSHELL_FANTASMA_A / 2;
			mshell_colonna_rifai(server);  /* il gruppo nella colonna si attenua */
		}
		if (mshell_sc.fantasma != NULL) {
			wlr_scene_node_set_position(&mshell_sc.fantasma->node,
				(int)px + mshell_sc.fantasma_dx, (int)py + mshell_sc.fantasma_dy);
		}
		/* il suggerimento solo sopra il centro, dove lasciarlo serve (il disegno: 20 a destra e
		 * 32 sotto il puntatore) */
		mshell_suggerimento(server, mshell_sc.trascina && px - schermo.x > MSHELL_COLONNA_FINE ?
			"Lascia al centro: entra nel gruppo" : NULL, (int)px + 20, (int)py + 32);
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
		return true;
	}

	/* LA SCELTA DELLE DISPOSIZIONI aperta: si accende la miniatura sotto il puntatore */
	if (mshell_sc.scelta != NULL) {
		int accesa = -1;
		for (int j = 0; j < mshell_sc.n_opzioni; j++) {
			if (mshell_dentro(mshell_sc.opzione[j], px, py)) {
				accesa = j;
			}
		}
		if (accesa != mshell_sc.opzione_accesa) {
			mshell_sc.opzione_accesa = accesa;
			mshell_scelta_disegna(server, mshell_scelta_t);
		}
		if (mshell_dentro(mshell_sc.scelta_box, px, py)) {
			wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
			wlr_seat_pointer_clear_focus(server->seat);
			return true;
		}
	}

	/* LA COLONNA: sale sopra le finestre al bordo sinistro, torna sotto quando il puntatore se ne
	 * allontana; il gruppo sotto il puntatore si illumina */
	double x = px - schermo.x;
	if (!mshell_sc.sopra && x < 1 && server->n_gruppi > 0) {
		mshell_sc.sopra = true;
		wlr_scene_node_place_above(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	} else if (mshell_sc.sopra && x > MSHELL_COLONNA_FINE) {
		mshell_sc.sopra = false;
		wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	}
	struct mshell_gruppo *g = mshell_colonna_gruppo_a(server, px, py);
	if (g != mshell_sc.acceso) {
		mshell_sc.acceso = g;
		mshell_colonna_rifai(server);
	}
	/* il suggerimento: sul gruppo della colonna (20 a destra e 28 sotto il puntatore), sul
	 * pulsante "–" (sopra il pulsante); altrimenti niente */
	struct tinywl_toplevel *ts = NULL;
	for (int k = 0; k < 2; k++) {
		if (mshell_pill[k].t != NULL && mshell_dentro(mshell_pulsante_box(mshell_pill[k].t, P_RIDUCI), px, py)) {
			ts = mshell_pill[k].t;
		}
	}
	if (g != NULL) {
		mshell_suggerimento(server, "Clic: il gruppo va al centro", (int)px + 20, (int)py + 28);
	} else if (ts != NULL && mshell_sc.scelta == NULL) {
		struct wlr_box b = mshell_pulsante_box(ts, P_RIDUCI);
		mshell_suggerimento(server, "Togli dal gruppo: va nella colonna di sinistra",
			b.x + b.width / 2 - 100, b.y - MSHELL_SUGG_ALTO - 6);
	} else {
		mshell_suggerimento(server, NULL, 0, 0);
	}

	/* I CONTROLLI: sulla finestra del centro sotto il puntatore (restano mentre il puntatore e'
	 * sopra di loro, e mentre la scelta delle disposizioni e' aperta) */
	if (mshell_sc.scelta != NULL) {
		return false;
	}
	if (server->cursor_mode != TINYWL_CURSOR_PASSTHROUGH) {
		return false;
	}
	/* 1. sulle pill che si vedono (o nello stacco fra loro e la finestra): restano quelle */
	struct tinywl_toplevel *t = NULL;
	for (int k = 1; k >= 0 && t == NULL; k--) {
		struct tinywl_toplevel *pt = mshell_pill[k].t;
		if (pt != NULL && (mshell_dentro(mshell_etichetta_box(pt), px, py) ||
				mshell_dentro(mshell_ctrl_striscia(pt), px, py))) {
			t = pt;
		}
	}
	/* 2. la finestra sotto il puntatore */
	if (t == NULL) {
		struct wlr_surface *s = NULL;
		double sx, sy;
		t = desktop_toplevel_at(server, px, py, &s, &sx, &sy);
	}
	/* 3. la zona delle pill di una finestra del centro (dalla piu' in alto), anche senza passare
	 * dalla finestra */
	struct mshell_gruppo *c = mshell_al_centro(server);
	for (int i = c != NULL ? c->n - 1 : -1; i >= 0 && t == NULL; i--) {
		if (mshell_dentro(mshell_ctrl_striscia(c->app[i]), px, py)) {
			t = c->app[i];
		}
	}
	mshell_controlli_metti(server, t);
	return false;
}

/* Un clic (giu': premuto; altrimenti lasciato). Vero se il clic e' della scrivania. */
static bool mshell_scrivania_clic(struct tinywl_server *server, bool giu) {
	double px = server->cursor->x, py = server->cursor->y;
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	if (!giu) {
		/* lasciato: finisce il trascinamento di un gruppo (o era un clic sul gruppo) */
		struct mshell_gruppo *g = mshell_sc.premuto;
		if (g == NULL) {
			return false;
		}
		bool trascinato = mshell_sc.trascina;
		mshell_suggerimento(server, NULL, 0, 0);
		mshell_sc.premuto = NULL;
		mshell_sc.trascina = false;
		if (mshell_sc.fantasma != NULL) {
			wlr_scene_node_destroy(&mshell_sc.fantasma->node);
			mshell_sc.fantasma = NULL;
		}
		if (!trascinato) {
			mshell_porta_al_centro(server, g);
		} else if (px - schermo.x > MSHELL_COLONNA_FINE) {
			mshell_unisci(server, g, px, py);
		} else {
			mshell_colonna_rifai(server);  /* lasciato sulla colonna: niente */
		}
		process_cursor_motion(server, 0);
		return true;
	}
	/* premuto: prima la scelta delle disposizioni, se e' aperta */
	if (mshell_sc.scelta != NULL) {
		for (int j = 0; j < mshell_sc.n_opzioni; j++) {
			if (mshell_dentro(mshell_sc.opzione[j], px, py)) {
				struct mshell_gruppo *g = mshell_sc.scelta_gruppo;
				int d = mshell_sc.opzione_disp[j];
				mshell_scelta_chiudi();
				mshell_disponi_gruppo(server, g, d);
				return true;
			}
		}
		bool dentro = mshell_dentro(mshell_sc.scelta_box, px, py);
		mshell_scelta_chiudi();
		if (dentro) {
			return true;
		}
	}
	/* le pill (prima quelle della finestra sotto il puntatore, poi quelle dell'attiva) */
	for (int k = 1; k >= 0; k--) {
	struct tinywl_toplevel *t = mshell_pill[k].t;
	if (t != NULL) {
		if (mshell_dentro(mshell_etichetta_box(t), px, py)) {
			/* l'etichetta: si trascina la finestra (come dalla barra dell'app, tinywl.c) */
			focus_toplevel(t, t->xdg_toplevel->base->surface);
			server->grabbed_toplevel = t;
			server->cursor_mode = TINYWL_CURSOR_MOVE;
			server->grab_x = px - t->posto.x;
			server->grab_y = py - t->posto.y;
			mshell_libera(t);
			return true;
		}
		for (int i = 0; i < P_QUANTI; i++) {
			if (!mshell_dentro(mshell_pulsante_box(t, i), px, py)) {
				continue;
			}
			switch (i) {
			case P_DISPONI:
				focus_toplevel(t, t->xdg_toplevel->base->surface);
				mshell_scelta_apri(server, t);
				break;
			case P_RIDUCI:
				mshell_togli_dal_gruppo(server, t);
				break;
			case P_CHIUDI:
				wlr_xdg_toplevel_send_close(t->xdg_toplevel);
				break;
			}
			return true;
		}
	}
	}
	/* un gruppo della colonna: si aspetta di vedere se e' un clic o un trascinamento */
	struct mshell_gruppo *g = mshell_colonna_gruppo_a(server, px, py);
	if (g != NULL) {
		mshell_sc.premuto = g;
		mshell_sc.premuto_x = px;
		mshell_sc.premuto_y = py;
		mshell_sc.trascina = false;
		return true;
	}
	return false;
}

/* All'avvio (da mshell_scrivania_avvia): l'albero dei controlli. */
static void mshell_controlli_avvia(void) {
	for (int k = 0; k < 2; k++) {
		mshell_pill[k].albero = wlr_scene_tree_create(mshell_sc.controlli);
	}
}
