/* MSHELL: LA SCRIVANIA DA TASTIERA E LO SPLIT DI UN GRUPPO (la persona, 23/11/2026: "definire le
 * operazioni in modo da codificarle sia con mouse che con tastiera"). Non si compila da solo: lo
 * include scrivania.c dopo i controlli. Il vassoio e' la colonna dei gruppi a sinistra.
 *
 * I TASTI (Super = il tasto Windows, Cmd sul Mac):
 *   Super+Tab        sceglie il gruppo dopo nella colonna (dal primo, quello usato per ultimo); la
 *                    colonna sale sopra le finestre e la scelta ha un fondo tenue dietro.
 *                    Lasciando Super il gruppo scelto va al centro (scambio).
 *     con Super giu': destra apre lo SPLIT del gruppo scelto (le sue finestre escono dalla pila a
 *                    ventaglio) e passa alla finestra dopo; sinistra torna indietro e dalla prima
 *                    chiude lo split. Lasciando Super con una finestra scelta, la finestra esce dal
 *                    suo gruppo e va al centro in un gruppo nuovo (la persona: "prendo una finestra
 *                    da un gruppo e la porto al centro in un gruppo nuovo").
 *                    Z porta DENTRO il gruppo al centro il gruppo scelto (tutte le sue finestre) o,
 *                    nello split, la finestra scelta; X chiude il gruppo scelto o la finestra
 *                    scelta; Esc annulla.
 *   Super+1..9       il gruppo 1..9 della colonna (nell'ordine della colonna) va al centro.
 *   Super+Alt        lasciando Alt (senza aver premuto un numero) passa alla finestra dopo del
 *                    gruppo al centro, nell'ordine di quando si e' cominciato (dalla piu' recente);
 *                    il giro finisce quando si lascia Super.
 *   Super+Alt+1..9   la finestra 1..9 del gruppo al centro (1: quella davanti).
 *   Super+Alt+Tab    la scelta delle disposizioni del gruppo al centro (il pannello di
 *                    "Disponi"): ogni Tab passa alla disposizione dopo, quella accesa si applica
 *                    quando si lascia Super; Esc (con Super giu') chiude senza cambiare. Dopo
 *                    quelle del gruppo vengono quelle della sola finestra attiva (la seconda fila).
 *   Super+X          chiude la finestra attiva.
 *   Super+Z          manda la finestra attiva nel vassoio (come il pulsante "–"). Una seconda Z,
 *                    sempre tenendo Super, libera la scrivania: tutto il gruppo (con la finestra
 *                    della prima Z, che ci rientra) va nel vassoio e il centro resta vuoto.
 *   Super+spazio     il pannello di ricerca (livelli.c).
 *   CON SHIFT si scorre al contrario (la persona, 24/11/2026): Super+Shift+Tab il gruppo prima,
 *                    Super+Alt lasciando Alt con Shift giu' la finestra prima, Super+Alt+Shift+Tab
 *                    la disposizione prima.
 * (Il vassoio che si apriva tenendo Super da solo e' stato tolto: la persona, 23/11/2026.)
 *
 * LO SPLIT (la persona: "un'animazione di split come su Stage Manager del Mac"; poi "disporre le
 * finestre a scendere, altrimenti si sovrappongono tutte"): le miniature del gruppo escono dalla
 * pila del suo riquadro e scendono una sotto l'altra nella colonna, dalla piu' recente in alto, a
 * MSHELL_SPLIT_SPAZIO l'una dall'altra (piu' piccole se non ci stanno in altezza); il riquadro si
 * allunga e gli altri gruppi della colonna scorrono per fargli posto. In MSHELL_DURATA_MEDIA con la
 * curva ease-in-out; si chiude allo stesso modo, tornando nella pila. La finestra scelta sale sopra le altre con il fondo tenue
 * dietro. Col mouse (la persona: "cliccando sulle icone come su Mac"): un clic sulle icone del
 * riquadro apre e chiude lo split; un clic su una finestra dello split la stacca e la porta al
 * centro in un gruppo nuovo (come lasciare Super); un clic altrove lo chiude.
 *
 * LA SCELTA si vede con un FONDO TENUE dietro (la persona: "come Cmd+Tab sul Mac"; il colore e'
 * controllo/selezione-vetro), niente contorno ("non ci deve essere"). */

#define MSHELL_SPLIT_SPAZIO 12  /* lo split: fra una miniatura e l'altra */
#define MSHELL_FONDO_FUORI 8   /* quanto il fondo della scelta sta fuori dalla cosa scelta */

/* L'indice del gruppo g nella colonna, o -1. */
static int mshell_in_colonna(struct tinywl_server *server, struct mshell_gruppo *g) {
	for (int i = 0; g != NULL && i < server->n_gruppi; i++) {
		if (server->gruppo[i] == g) {
			return i;
		}
	}
	return -1;
}

/* Il gruppo scelto con Super+Tab (NULL: nessuno). */
static struct mshell_gruppo *mshell_tab_gruppo(struct tinywl_server *server) {
	return mshell_ts.tab && mshell_ts.tab_i < server->n_gruppi ? server->gruppo[mshell_ts.tab_i] : NULL;
}

/* Il fondo tenue della scelta. */
static void mshell_fondo_scelta(struct wlr_scene_tree *padre, int x, int y, int l, int a, int raggio) {
	int f = MSHELL_FONDO_FUORI;
	mshell_rett(padre, x - f, y - f, l + 2 * f, a + 2 * f, raggio + f, mshell_sc.colore_selezione, 1.0f);
}

/* Le icone del riquadro i-esimo della colonna (dove un clic apre lo split). */
static struct wlr_box mshell_icone_box(struct tinywl_server *server, int i) {
	struct mshell_gruppo *g = server->gruppo[i];
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	int quante = mshell_riquadro_icone(g);
	int y = mshell_colonna_y(server, i) + mshell_riquadro_cima(g) + mshell_riquadro_davanti(g) +
		MSHELL_ICONA_SPORGE - MSHELL_ICONA;
	return (struct wlr_box){ schermo.x + MSHELL_COLONNA_X + MSHELL_ICONA_X, y,
		MSHELL_ICONA + MSHELL_ICONA_PASSO * (quante > 0 ? quante - 1 : 0), MSHELL_ICONA };
}

/* La colonna sopra le finestre (scelta da tastiera, split) o di nuovo sotto. */
static void mshell_colonna_sopra(struct tinywl_server *server, bool sopra) {
	mshell_sc.sopra = sopra;
	if (sopra) {
		wlr_scene_node_place_above(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	} else {
		wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	}
	mshell_colonna_mostra(server);
}

/* Vero se la colonna deve restare sopra anche col puntatore lontano. */
static bool mshell_tasti_tiene_colonna(void) {
	return mshell_ts.tab || mshell_ts.aperto != NULL;
}

/* --- lo split --- */

/* Chiude lo split subito, senza animazione (il gruppo cambia posto o se ne va). */
static void mshell_split_via(void) {
	mshell_ts.aperto = NULL;
	mshell_ts.sel_f = -1;
	mshell_ts.n_celle = 0;
}

/* Apre (o chiude) lo split del gruppo g, con l'animazione. */
static void mshell_split(struct tinywl_server *server, struct mshell_gruppo *g, bool apri) {
	if (apri) {
		if (mshell_ts.aperto != g) {
			mshell_ts.aperto = g;
			mshell_ts.vent_apre = true;
			mshell_ts.vent_da = mshell_anim_spenta() ? 0 : mshell_anim_adesso();
			mshell_anim_avvia();
		}
	} else if (mshell_ts.aperto != NULL && mshell_anim_spenta()) {
		mshell_split_via();  /* senza animazioni: si chiude subito */
	} else if (mshell_ts.aperto != NULL && mshell_ts.vent_apre) {
		mshell_ts.vent_apre = false;
		mshell_ts.vent_da = mshell_anim_spenta() ? 0 : mshell_anim_adesso();
		mshell_ts.sel_f = -1;
		mshell_anim_avvia();
	}
	mshell_colonna_rifai(server);
}

/* Quanto e' aperto lo split adesso, da 0 (pila) a 1 (ventaglio). */
static double mshell_split_quanto(int64_t ora) {
	double e = mshell_anim_curva_dolce(mshell_ts.vent_da, MSHELL_DURATA_MEDIA, ora);
	return mshell_ts.vent_apre ? e : 1 - e;
}

/* Usata da mshell_riquadro: vero se le miniature del gruppo le disegna lo split. */
static bool mshell_split_di(struct mshell_gruppo *g) {
	return g != NULL && g == mshell_ts.aperto;
}

/* Le miniature dello split a ventaglio aperto, una sotto l'altra dalla cima del riquadro (y_riq):
 * dove stanno e l'altezza che occupano. */
static int mshell_split_posti(struct tinywl_server *server, struct mshell_gruppo *g, int y_riq,
		struct wlr_box dove[MSHELL_GRUPPO_MAX]) {
	struct wlr_box schermo, area = mshell_area(server);
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	int s = MSHELL_SPLIT_SPAZIO;
	double amax = MSHELL_MINI_A;
	if (g->n * amax + (g->n - 1) * s > area.height) {
		amax = (double)(area.height - (g->n - 1) * s) / g->n;
	}
	double lmax = amax * MSHELL_MINI_L / MSHELL_MINI_A;
	int y = y_riq;
	for (int j = 0; j < g->n; j++) {
		int l, a;
		mshell_mini_misura(g->app[g->n - 1 - j], lmax, amax, &l, &a);  /* dalla piu' recente */
		dove[j] = (struct wlr_box){ schermo.x + MSHELL_COLONNA_X + MSHELL_MINI_X, y, l, a };
		y += a + s;
	}
	return y - s - y_riq;
}

/* Usata da mshell_riquadro_alto: l'altezza del riquadro del gruppo aperto, fra la pila e lo split. */
static int mshell_split_alto(struct mshell_gruppo *g, int pila) {
	struct wlr_box dove[MSHELL_GRUPPO_MAX];
	int aperto = mshell_split_posti(mshell_sc.server, g, 0, dove) + MSHELL_ICONA_SPORGE;
	double e = mshell_split_quanto(mshell_anim_adesso());
	return (int)(pila + (aperto - pila) * e + 0.5);
}

/* Usata da mshell_colonna_disegna, dopo i riquadri: le miniature del gruppo aperto, fra la pila
 * e lo split. */
static void mshell_spaccato_disegna(struct tinywl_server *server) {
	mshell_ts.n_celle = 0;
	int i = mshell_in_colonna(server, mshell_ts.aperto);
	if (i < 0 || mshell_ts.aperto->n == 0) {
		mshell_split_via();
		return;
	}
	struct mshell_gruppo *g = mshell_ts.aperto;
	if (mshell_ts.sel_f >= g->n) {
		mshell_ts.sel_f = g->n - 1;
	}
	int y_riq = mshell_colonna_y_vista(server, i);
	double e = mshell_split_quanto(mshell_anim_adesso());
	struct wlr_scene_tree *p = wlr_scene_tree_create(mshell_sc.colonna);
	wlr_scene_node_lower_to_bottom(&p->node);  /* sotto i riquadri: le icone del gruppo restano sopra */
	struct wlr_box aperto[MSHELL_GRUPPO_MAX], dove[MSHELL_GRUPPO_MAX];
	mshell_split_posti(server, g, y_riq, aperto);
	for (int j = 0; j < g->n; j++) {
		struct tinywl_toplevel *t = g->app[g->n - 1 - j];  /* dalla piu' recente */
		struct wlr_box pila = mshell_volo_mini(server, g, t, j, y_riq);
		struct wlr_box v = aperto[j];
		dove[j] = (struct wlr_box){
			(int)(pila.x + (v.x - pila.x) * e + 0.5), (int)(pila.y + (v.y - pila.y) * e + 0.5),
			(int)(pila.width + (v.width - pila.width) * e + 0.5),
			(int)(pila.height + (v.height - pila.height) * e + 0.5),
		};
		mshell_ts.cella[j] = v;
		mshell_ts.cella_t[j] = t;
	}
	mshell_ts.n_celle = e >= 1 ? g->n : 0;  /* i clic solo a split aperto */
	/* dalla piu' in basso, cosi' (nella pila) quella davanti sta sopra */
	for (int j = g->n - 1; j >= 0; j--) {
		mshell_mini_in(p, g->app[g->n - 1 - j], dove[j].x, dove[j].y, dove[j].width, dove[j].height, false);
	}
}

/* Da mshell_colonna_scatto: true se lo split si muove (la colonna va ridisegnata). */
static bool mshell_ventaglio_scatto(struct tinywl_server *server, int64_t ora) {
	if (mshell_ts.aperto == NULL || mshell_ts.vent_da == 0) {
		return false;
	}
	if (mshell_anim_curva_dolce(mshell_ts.vent_da, MSHELL_DURATA_MEDIA, ora) >= 1) {
		mshell_ts.vent_da = 0;
		if (!mshell_ts.vent_apre) {
			mshell_split_via();  /* tornato nella pila */
			if (!mshell_ts.tab) {
				mshell_colonna_sopra(server, false);
			}
		}
	}
	return true;
}

/* Dove va il fondo tenue della scelta: il riquadro del gruppo scelto con Super+Tab o, nello
 * split, la finestra scelta (ai loro posti d'arrivo). Falso: niente scelta. */
static bool mshell_fondo_meta(struct tinywl_server *server, struct wlr_box *b) {
	struct mshell_gruppo *g = mshell_tab_gruppo(server);
	int i = mshell_in_colonna(server, g);
	if (i < 0) {
		return false;
	}
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	int y = mshell_colonna_y(server, i);
	if (mshell_ts.aperto == g && mshell_ts.sel_f >= 0 && mshell_ts.sel_f < g->n) {
		struct wlr_box dove[MSHELL_GRUPPO_MAX];
		mshell_split_posti(server, g, y, dove);
		*b = dove[mshell_ts.sel_f];
		return true;
	}
	*b = (struct wlr_box){ schermo.x + MSHELL_COLONNA_X, y, mshell_riquadro_largo(g), mshell_riquadro_alto(g) };
	return true;
}

/* --- spostare finestre e gruppi --- */

/* La finestra t (di un gruppo della colonna) entra nel gruppo al centro, davanti, dove stava
 * (dentro lo spazio delle finestre); se il centro e' vuoto, ci va lei in un gruppo suo. */
static void mshell_migra(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo, *c = server->centro;
	if (g == NULL || g == c) {
		return;
	}
	if (c == NULL && g->n == 1) {
		mshell_porta_al_centro(server, g);  /* sola nel suo gruppo: va al centro il gruppo */
		return;
	}
	if (c != NULL && c->n >= MSHELL_GRUPPO_MAX) {
		return;  /* il gruppo al centro e' pieno */
	}
	struct mshell_gruppo *nuovo = NULL;
	if (c == NULL && (nuovo = mshell_gruppo_nuovo(server)) == NULL) {
		return;
	}
	struct wlr_box da = mshell_volo_da_colonna(server, t);  /* da dove vola */
	int i = 0;
	while (i < g->n && g->app[i] != t) {
		i++;
	}
	memmove(&g->app[i], &g->app[i + 1], (size_t)(g->n - 1 - i) * sizeof(g->app[0]));
	g->n--;
	/* prima la finestra entra nel gruppo nuovo (il suo albero passa la'), poi il gruppo vecchio,
	 * se e' rimasto vuoto, sparisce col suo albero */
	mshell_gruppo_entra(nuovo != NULL ? nuovo : c, t);
	if (g->n == 0) {
		mshell_colonna_togli(server, g);
		if (mshell_sc.acceso == g) {
			mshell_sc.acceso = NULL;
		}
		if (mshell_ts.aperto == g) {
			mshell_split_via();
		}
		mshell_voli_dimentica(NULL, g, true);
		wlr_scene_node_destroy(&g->albero->node);
		free(g);
	} else {
		mshell_ridisponi(g);
	}
	if (nuovo != NULL) {
		mshell_metti_al_posto(t);
		mshell_porta_al_centro(server, nuovo);
	} else {
		mshell_cascata(server, c, t);  /* non proprio sopra un'altra (come il Mac) */
		struct wlr_box area = mshell_area(server);
		if (t->posto.x + t->posto.width > area.x + area.width) {
			t->posto.x = area.x + area.width - t->posto.width;
		}
		if (t->posto.x < area.x) {
			t->posto.x = area.x;
		}
		if (t->posto.y + t->posto.height > area.y + area.height) {
			t->posto.y = area.y + area.height - t->posto.height;
		}
		if (t->posto.y < mshell_finestra_cima(t)) {
			t->posto.y = mshell_finestra_cima(t);
		}
		mshell_metti_al_posto(t);
		mshell_ridisponi(c);
		mshell_focus_centro(server);
	}
	mshell_volo_entra(server, t, da);  /* vola dalla colonna al suo posto (scrivania-voli.c) */
	mshell_colonna_rifai(server);
	mshell_anim_puntatore();
}

/* Tutto il gruppo g (della colonna) entra nel gruppo al centro, la sua finestra davanti per ultima
 * (cosi' resta davanti). */
static void mshell_migra_gruppo(struct tinywl_server *server, struct mshell_gruppo *g) {
	if (server->centro == NULL) {
		mshell_porta_al_centro(server, g);
		return;
	}
	struct tinywl_toplevel *app[MSHELL_GRUPPO_MAX];
	int n = g->n;
	memcpy(app, g->app, (size_t)n * sizeof(app[0]));  /* g sparisce con l'ultima */
	for (int i = 0; i < n; i++) {
		mshell_migra(server, app[i]);
	}
}

/* La finestra t esce dal suo gruppo (della colonna) e va al centro in un gruppo nuovo; quello che
 * c'era al centro scende nella colonna. Se era sola, va al centro il suo gruppo. */
static void mshell_stacca_al_centro(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	if (g == NULL || g == server->centro) {
		return;
	}
	if (g->n == 1) {
		mshell_porta_al_centro(server, g);
		return;
	}
	struct mshell_gruppo *nuovo = mshell_gruppo_nuovo(server);
	if (nuovo == NULL) {
		return;
	}
	struct wlr_box da = mshell_volo_da_colonna(server, t);  /* da dove vola */
	int i = 0;
	while (i < g->n && g->app[i] != t) {
		i++;
	}
	memmove(&g->app[i], &g->app[i + 1], (size_t)(g->n - 1 - i) * sizeof(g->app[0]));
	g->n--;
	mshell_ridisponi(g);
	mshell_gruppo_entra(nuovo, t);
	mshell_metti_al_posto(t);
	mshell_porta_al_centro(server, nuovo);
	mshell_volo_entra(server, t, da);  /* vola dalla colonna al centro */
}

/* Chiude tutte le finestre del gruppo g (le app spariscono quando hanno finito). */
static void mshell_chiudi_gruppo(struct mshell_gruppo *g) {
	for (int i = 0; i < g->n; i++) {
		wlr_xdg_toplevel_send_close(g->app[i]->xdg_toplevel);
	}
}

/* --- Super+Tab --- */

/* La finestra scelta nello split del gruppo scelto (NULL: nessuna). */
static struct tinywl_toplevel *mshell_tab_finestra(struct tinywl_server *server) {
	struct mshell_gruppo *g = mshell_tab_gruppo(server);
	if (g == NULL || mshell_ts.aperto != g || mshell_ts.sel_f < 0 || mshell_ts.sel_f >= g->n) {
		return NULL;
	}
	return g->app[g->n - 1 - mshell_ts.sel_f];
}

static void mshell_tab_avanti(struct tinywl_server *server, bool indietro) {
	int n = server->n_gruppi;
	if (n == 0) {
		return;
	}
	if (!mshell_ts.tab) {
		mshell_ts.tab = true;
		mshell_ts.tab_i = indietro ? n - 1 : 0;
	} else {
		mshell_ts.tab_i = (mshell_ts.tab_i + (indietro ? n - 1 : 1)) % n;
	}
	mshell_split_via();  /* lo split del gruppo di prima si chiude */
	mshell_colonna_sopra(server, true);
	mshell_colonna_rifai(server);
}

/* Esc, o Z/X gia' fatti: finisce la scelta senza scambio. */
static void mshell_tab_annulla(struct tinywl_server *server) {
	mshell_ts.tab = false;
	mshell_split_via();
	mshell_colonna_sopra(server, false);
	mshell_colonna_rifai(server);
}

/* Lasciato Super: lo scambio (il gruppo scelto al centro) o, con una finestra scelta nello split,
 * la finestra al centro in un gruppo nuovo. */
static void mshell_tab_fine(struct tinywl_server *server) {
	struct mshell_gruppo *g = mshell_tab_gruppo(server);
	struct tinywl_toplevel *t = mshell_tab_finestra(server);
	mshell_ts.tab = false;
	mshell_split_via();
	if (t != NULL) {
		mshell_stacca_al_centro(server, t);
	} else if (g != NULL) {
		mshell_porta_al_centro(server, g);
	} else {
		mshell_colonna_sopra(server, false);
		mshell_colonna_rifai(server);
	}
}

/* Un tasto mentre si sceglie un gruppo con Super+Tab. Vero se e' suo. */
static bool mshell_tab_tasto(struct tinywl_server *server, xkb_keysym_t sym) {
	struct mshell_gruppo *g = mshell_tab_gruppo(server);
	if (g == NULL) {
		return false;
	}
	struct tinywl_toplevel *t = mshell_tab_finestra(server);
	switch (sym) {
	case XKB_KEY_Right:
		if (mshell_ts.aperto != g) {
			mshell_split(server, g, true);
			mshell_ts.sel_f = 0;
		} else if (mshell_ts.sel_f < g->n - 1) {
			mshell_ts.sel_f++;
		}
		mshell_colonna_rifai(server);
		return true;
	case XKB_KEY_Left:
		if (mshell_ts.aperto == g && mshell_ts.sel_f > 0) {
			mshell_ts.sel_f--;
			mshell_colonna_rifai(server);
		} else if (mshell_ts.aperto == g) {
			mshell_split(server, g, false);
		}
		return true;
	case XKB_KEY_z:
	case XKB_KEY_Z:
		mshell_tab_annulla(server);
		if (t != NULL) {
			mshell_migra(server, t);
		} else {
			mshell_migra_gruppo(server, g);
		}
		return true;
	case XKB_KEY_x:
	case XKB_KEY_X:
		if (t != NULL) {
			wlr_xdg_toplevel_send_close(t->xdg_toplevel);
		} else {
			mshell_chiudi_gruppo(g);
			mshell_tab_annulla(server);
		}
		return true;
	case XKB_KEY_Escape:
		mshell_tab_annulla(server);
		return true;
	default:
		return false;
	}
}

/* --- il giro delle finestre, le disposizioni, Super+Z --- */

static void mshell_giro_avanti(struct tinywl_server *server, bool indietro) {
	struct mshell_gruppo *c = server->centro;
	if (c == NULL || c->n < 2) {
		return;
	}
	if (!mshell_ts.giro) {
		mshell_ts.giro = true;
		mshell_ts.giro_n = c->n;
		for (int i = 0; i < c->n; i++) {
			mshell_ts.giro_t[i] = c->app[c->n - 1 - i];  /* dalla piu' recente */
		}
		mshell_ts.giro_i = 0;
	}
	for (int k = 0; k < mshell_ts.giro_n; k++) {  /* la prossima che c'e' ancora */
		mshell_ts.giro_i = (mshell_ts.giro_i + (indietro ? mshell_ts.giro_n - 1 : 1)) % mshell_ts.giro_n;
		struct tinywl_toplevel *t = mshell_ts.giro_t[mshell_ts.giro_i];
		if (t != NULL && t->gruppo == c) {
			focus_toplevel(t, t->xdg_toplevel->base->surface);
			return;
		}
	}
}

/* Super+Alt+Tab: apre la scelta delle disposizioni sulla finestra attiva, poi passa alla dopo. */
static void mshell_disp_avanti(struct tinywl_server *server, bool indietro) {
	struct mshell_gruppo *c = server->centro;
	if (c == NULL || c->n == 0) {
		return;
	}
	if (!mshell_ts.disp || mshell_sc.scelta == NULL) {
		mshell_ts.disp = true;
		mshell_scelta_apri(server, c->app[c->n - 1]);
		int scelte[8], fila1, k = mshell_disp_scelte_pannello(c->n, scelte, &fila1), adesso = 0;
		for (int j = 0; j < k; j++) {
			if (scelte[j] == c->disposizione) {
				adesso = j;  /* (libera: la prima) */
			}
		}
		mshell_sc.opzione_accesa = (adesso + (indietro ? k - 1 : 1)) % k;  /* dalla dopo (o prima) */
	} else {
		int k = mshell_sc.n_opzioni;
		mshell_sc.opzione_accesa = (mshell_sc.opzione_accesa + (indietro ? k - 1 : 1)) % k;
	}
	mshell_scelta_disegna(server, mshell_scelta_t);
}

/* Lasciato Super: si applica la disposizione accesa. */
static void mshell_disp_fine(struct tinywl_server *server) {
	mshell_ts.disp = false;
	struct mshell_gruppo *g = mshell_sc.scelta_gruppo;
	int j = mshell_sc.opzione_accesa;
	if (mshell_sc.scelta == NULL || g == NULL || j < 0 || j >= mshell_sc.n_opzioni) {
		mshell_scelta_chiudi();
		return;
	}
	int d = mshell_sc.opzione_disp[j];
	mshell_scelta_chiudi();
	mshell_disponi_gruppo(server, g, d);
	mshell_anim_puntatore();
}

/* Super+Z: la prima manda la finestra attiva nel vassoio; la seconda libera la scrivania. */
static void mshell_super_z(struct tinywl_server *server) {
	struct mshell_gruppo *c = server->centro;
	if (mshell_ts.z_volte == 0) {
		struct tinywl_toplevel *t = c != NULL && c->n > 0 ? c->app[c->n - 1] : NULL;
		if (t == NULL) {
			return;
		}
		mshell_ts.z_t = c->n > 1 ? t : NULL;  /* sola: con lei va gia' tutto il gruppo */
		mshell_togli_dal_gruppo(server, t);
		mshell_ts.z_volte = 1;
		return;
	}
	if (mshell_ts.z_volte == 1) {
		mshell_ts.z_volte = 2;
		struct tinywl_toplevel *t = mshell_ts.z_t;
		mshell_ts.z_t = NULL;
		if (c == NULL) {
			return;
		}
		if (t != NULL && mshell_in_colonna(server, t->gruppo) >= 0) {
			mshell_migra(server, t);  /* rientra nel suo gruppo, poi va tutto nel vassoio */
		}
		mshell_svuota_centro(server);
	}
}

/* Una finestra se ne va (gruppo_esce): non resta nel giro ne' nello split. */
static void mshell_tasti_dimentica(struct tinywl_toplevel *t) {
	if (mshell_ts.z_t == t) {
		mshell_ts.z_t = NULL;
	}
	for (int i = 0; i < mshell_ts.giro_n; i++) {
		if (mshell_ts.giro_t[i] == t) {
			mshell_ts.giro_t[i] = NULL;
		}
	}
	for (int i = 0; i < mshell_ts.n_celle; i++) {
		if (mshell_ts.cella_t[i] == t) {
			mshell_ts.cella_t[i] = NULL;
		}
	}
}

/* --- da tinywl.c: un tasto premuto o lasciato. Vero se e' della scrivania (non va alle app). --- */
static bool mshell_tasti(struct tinywl_server *server, xkb_keysym_t sym, bool premuto) {
	if (sym == XKB_KEY_Super_L || sym == XKB_KEY_Super_R) {
		server->super_giu = premuto;
		if (premuto) {
			mshell_ts.z_volte = 0;
			mshell_ts.z_t = NULL;
		} else {
			mshell_ts.giro = false;
			if (mshell_ts.tab) {
				mshell_tab_fine(server);
			}
			if (mshell_ts.disp) {
				mshell_disp_fine(server);
			}
		}
		return true;
	}
	if (sym == XKB_KEY_Shift_L || sym == XKB_KEY_Shift_R) {
		mshell_ts.shift_giu = premuto;  /* con Shift si scorre al contrario; Shift va anche alle app */
		return false;
	}
	if (sym == XKB_KEY_Alt_L || sym == XKB_KEY_Alt_R) {
		if (premuto && server->super_giu) {
			mshell_ts.alt_giu = true;
			mshell_ts.alt_altro = false;
			return true;
		}
		if (!premuto && mshell_ts.alt_giu) {
			mshell_ts.alt_giu = false;
			if (server->super_giu && !mshell_ts.alt_altro) {
				mshell_giro_avanti(server, mshell_ts.shift_giu);
			}
			return true;
		}
		return false;
	}
	if (!premuto) {
		return false;
	}
	if (mshell_ts.aperto != NULL && !mshell_ts.tab && sym == XKB_KEY_Escape) {
		mshell_split(server, mshell_ts.aperto, false);  /* lo split aperto col mouse */
		return true;
	}
	if (!server->super_giu) {
		return false;
	}
	if (mshell_ts.alt_giu) {
		mshell_ts.alt_altro = true;
	}
	if (mshell_ts.tab && !mshell_ts.alt_giu && mshell_tab_tasto(server, sym)) {
		return true;
	}
	struct mshell_gruppo *c = server->centro;
	struct tinywl_toplevel *attiva = c != NULL && c->n > 0 ? c->app[c->n - 1] : NULL;
	if (sym >= XKB_KEY_1 && sym <= XKB_KEY_9) {
		int n = (int)(sym - XKB_KEY_1) + 1;
		if (mshell_ts.alt_giu) {
			if (c != NULL && n <= c->n) {
				struct tinywl_toplevel *t = c->app[c->n - n];
				focus_toplevel(t, t->xdg_toplevel->base->surface);
			}
		} else if (n <= server->n_gruppi) {
			mshell_ts.tab = false;
			mshell_split_via();
			mshell_porta_al_centro(server, server->gruppo[n - 1]);
		}
		return true;
	}
	switch (sym) {
	case XKB_KEY_space:
		mshell_livelli_cassetto(server);
		return true;
	case XKB_KEY_Tab:
	case XKB_KEY_ISO_Left_Tab: {  /* (Shift+Tab) */
		bool indietro = sym == XKB_KEY_ISO_Left_Tab || mshell_ts.shift_giu;
		if (mshell_ts.alt_giu) {
			mshell_disp_avanti(server, indietro);  /* Super+Alt+Tab: la disposizione */
		} else {
			mshell_tab_avanti(server, indietro);
		}
		return true;
	}
	case XKB_KEY_Escape:
		if (mshell_ts.disp) {
			mshell_ts.disp = false;
			mshell_scelta_chiudi();
			return true;
		}
		return false;
	case XKB_KEY_x:
	case XKB_KEY_X:
		if (attiva != NULL) {
			wlr_xdg_toplevel_send_close(attiva->xdg_toplevel);
		}
		return true;
	case XKB_KEY_z:
	case XKB_KEY_Z:
		mshell_super_z(server);
		return true;
	default:
		return false;
	}
}

/* --- da mshell_scrivania_clic: un clic premuto. Vero se e' dello split o delle icone. --- */
static bool mshell_tasti_clic(struct tinywl_server *server, double px, double py) {
	if (mshell_in_colonna(server, mshell_ts.aperto) >= 0) {
		for (int j = 0; j < mshell_ts.n_celle; j++) {  /* da sinistra: quella sopra */
			struct tinywl_toplevel *t = mshell_ts.cella_t[j];
			if (t != NULL && mshell_dentro(mshell_ts.cella[j], px, py)) {
				mshell_ts.tab = false;
				mshell_split_via();
				mshell_stacca_al_centro(server, t);
				return true;
			}
		}
	}
	struct mshell_gruppo *g = mshell_colonna_gruppo_a(server, px, py);
	int i = mshell_in_colonna(server, g);
	if (i >= 0 && mshell_dentro(mshell_icone_box(server, i), px, py)) {
		mshell_colonna_sopra(server, true);
		mshell_split(server, g, mshell_ts.aperto != g || !mshell_ts.vent_apre);
		return true;
	}
	if (mshell_ts.aperto != NULL) {  /* un clic altrove chiude lo split */
		mshell_split(server, mshell_ts.aperto, false);
	}
	return false;
}
