/* MSHELL: LA SCRIVANIA, COME LO STAGE MANAGER (le regole sono in cima a tinywl.c, "MSHELL: LA
 * SCRIVANIA"). Questo file non si compila da solo: tinywl.c lo include dopo effetti.c e
 * animazioni.c, e prima di livelli.c, che usa le sue funzioni.
 *
 * I GRUPPI. server->centro e' il gruppo al centro (NULL: il centro e' vuoto); server->gruppo[]
 * tiene quelli della colonna, dall'alto (dal piu' recente). Ogni gruppo ha il suo albero della scena con le sue
 * finestre; si accende solo l'albero del gruppo al centro. Dentro il gruppo le finestre sono
 * nell'ordine in cui si sovrappongono: l'ultima sta davanti ed e' quella col focus.
 *
 * LE FINESTRE sono libere: t->posto e' dove sta la finestra vera (la "geometria" dell'app, senza
 * l'ombra che alcune app disegnano intorno) sullo schermo. La prima volta che compare si mette al
 * centro dello spazio sotto la barra (un dialogo: al centro della finestra madre); la misura la
 * sceglie l'app. Poi la spostano e la ridimensionano le richieste dell'app (tinywl.c).
 *
 * LA COLONNA DEI GRUPPI (il disegno: x 16, riquadri 184x128 a 20 di distanza, centrati in altezza
 * fra la barra e il fondo). E' un albero della scena SOTTO le finestre: si vede dove le finestre
 * non la coprono. Il puntatore contro il bordo sinistro la porta SOPRA le finestre; quando si
 * allontana oltre la colonna torna sotto. Ogni gruppo mostra le sue finestre in miniatura, una
 * dietro l'altra (fino a 3), e sotto a sinistra le icone delle sue app (una per app, fino a 3,
 * quella davanti sopra). Al passaggio del puntatore il gruppo si illumina; un clic lo porta al
 * centro.
 * LE MINIATURE sono "dal vivo" ma non costano: sono altri nodi della scena che puntano alla stessa
 * memoria dei pixel delle finestre (come l'istantanea delle animazioni), rimpiccioliti. Le finestre
 * dei gruppi in colonna non si vedono, quindi non ridisegnano: le miniature si rifanno solo quando
 * cambia qualcosa (gruppi, puntatore sopra un gruppo, un'app che manda un'immagine nuova), una
 * volta per giro del ciclo degli eventi.
 * AGGIUNGERE AL GRUPPO: si preme su un gruppo della colonna e lo si trascina; la sua finestra
 * davanti segue il puntatore in miniatura e il gruppo nella colonna si attenua. Lasciato al centro
 * (oltre la colonna), le sue finestre entrano nel gruppo al centro, la prima dove si e' lasciata.
 * Lasciato sulla colonna non succede niente. Premuto e lasciato senza muoversi e' un clic.
 *
 * I CONTROLLI SULLA FINESTRA: quando il puntatore e' sulla finestra del centro compaiono, come
 * pill SOPRA la finestra (sotto, se sopra non c'e' posto; dentro solo se non c'e' posto ne' sopra
 * ne' sotto; dentro, in alto, sempre, se il gruppo e' disposto con "Disponi"), il nome dell'app e della finestra (allineato a sinistra; trascinandolo si sposta la
 * finestra) e tre pulsanti (allineati a destra): "Disponi", "–" (toglie la finestra dal gruppo: diventa un gruppo a se', in cima alla
 * colonna; se e' sola, il suo gruppo scende nella colonna) e "×" (chiude). Li disegna il
 * compositore sopra la finestra.
 * DISPONI apre la scelta delle disposizioni per le finestre del gruppo: una miniatura per
 * disposizione, un clic la applica. Le finestre scorrono ai loro posti, senza sovrapporsi, su
 * tutto lo schermo sotto la barra (16 dai bordi e fra l'una e l'altra; la colonna non toglie
 * spazio). La finestra davanti prende il primo posto. Il gruppo si ricorda la disposizione: se
 * una finestra entra o esce, le altre si ridispongono; se se ne sposta una a mano, il gruppo torna
 * libero.
 *
 * COME NEL DISEGNO (la persona, 23/11/2026: "deve essere come il disegno, ombre e tutto"): le
 * finestre e le loro miniature hanno l'ombra/finestra (effetti.c); etichetta e pulsanti sono
 * superficie/elevata con testo/primario; il pannello di "Disponi" ha raggio/pannello e
 * l'ombra/sovrapposto, e nelle anteprime lo sfondo vero con le finestre di vetro; vicino al
 * puntatore compaiono i suggerimenti delle schermate 2, 4 e 7. I colori seguono il tema del
 * sistema (MSHELL_TEMA, token.c), come la barra. Il suggerimento della schermata 8 ("Puntatore al
 * bordo sinistro: tornano i gruppi") non c'e': spiega il disegno, non e' un aiuto per chi lo usa.
 *
 * LE ICONE DELLE APP: per ora un quadrato arrotondato colorato con l'iniziale del nome dell'app
 * (come il pannello di ricerca della barra quando un'app non ha un'icona PNG); il colore viene dal
 * nome. Le icone vere arriveranno con le app (Flatpak). Testo con pango e cairo, che nel sistema
 * ci sono gia' (li porta GTK). */
#include <math.h>  /* fabs */
#include <pango/pangocairo.h>

/* Le misure del disegno (schermata "1 · Al lavoro") */
#define MSHELL_COLONNA_X 16          /* la colonna dal bordo sinistro */
#define MSHELL_RIQUADRO_L 184        /* il riquadro di un gruppo */
#define MSHELL_RIQUADRO_A 128
#define MSHELL_RIQUADRO_SPAZIO 20    /* fra un riquadro e l'altro */
#define MSHELL_RIQUADRO_RAGGIO 16    /* gli angoli dell'evidenza al passaggio del puntatore */
#define MSHELL_EVIDENZA 0.28f        /* l'evidenza: bianco al 28% */
#define MSHELL_MINI_X 22             /* la miniatura davanti, nel riquadro */
#define MSHELL_MINI_Y 16
#define MSHELL_MINI_L 150            /* la sua misura massima */
#define MSHELL_MINI_A 100
#define MSHELL_MINI_PASSO_X 14       /* le miniature dietro: piu' a destra, */
#define MSHELL_MINI_PASSO_Y 10       /* piu' in alto */
#define MSHELL_MINI_RIDUCI 0.06      /* e un po' piu' piccole */
#define MSHELL_MINI_QUANTE 3
#define MSHELL_ICONA 36              /* l'icona dell'app, in basso a sinistra nel riquadro */
#define MSHELL_ICONA_X 8
#define MSHELL_ICONA_Y 88
#define MSHELL_ICONA_PASSO 30        /* le icone delle altre app del gruppo, a destra e sotto */
#define MSHELL_ICONE_QUANTE 3
#define MSHELL_SOTTO_BARRA 40        /* le finestre e la colonna stanno sotto la barra */
#define MSHELL_FONDO 16              /* e lasciano questo spazio in basso */
/* la striscia della colonna: oltre questa x il puntatore la fa tornare sotto le finestre */
#define MSHELL_COLONNA_FINE (MSHELL_COLONNA_X + MSHELL_RIQUADRO_L + 16)

/* Un'icona gia' fatta, per nome dell'app. */
struct mshell_icona {
	struct wl_list link;
	char *nome;
	struct mshell_pixel *pixel;
};

/* Lo stato della scrivania: uno solo, come il compositore. */
static struct {
	struct tinywl_server *server;
	struct wlr_scene_tree *colonna;        /* la colonna dei gruppi */
	bool sopra;                            /* la colonna sta sopra le finestre */
	struct mshell_gruppo *acceso;          /* il gruppo della colonna sotto il puntatore */
	struct wl_event_source *rifai;         /* il rifacimento della colonna in attesa */
	struct wl_list icone;                  /* mshell_icona.link */
	/* il trascinamento di un gruppo della colonna */
	struct mshell_gruppo *premuto;         /* il gruppo su cui si e' premuto (NULL: nessuno) */
	double premuto_x, premuto_y;
	bool trascina;                         /* il puntatore si e' mosso: si trascina */
	struct wlr_scene_tree *fantasma;       /* la miniatura che segue il puntatore */
	int fantasma_dx, fantasma_dy;          /* dal puntatore all'angolo della miniatura */
	/* i controlli sulla finestra */
	struct wlr_scene_tree *controlli;      /* sopra le finestre */
	struct tinywl_toplevel *sotto;         /* la finestra coi controlli (NULL: nessuna) */
	/* la scelta delle disposizioni */
	struct wlr_scene_tree *scelta;         /* NULL: chiusa */
	struct mshell_gruppo *scelta_gruppo;
	struct wlr_box scelta_box;             /* il pannello, sullo schermo */
	int n_opzioni, opzione_accesa;
	struct wlr_box opzione[4];             /* le miniature, sullo schermo */
	int opzione_disp[4];                   /* la disposizione di ciascuna */
	float colore_testo[4], colore_elevata[4], colore_focus[4], colore_vetro[4],
		colore_azione[4], colore_azione_testo[4];
	int raggio_pannello;
	/* l'immagine dello sfondo (le anteprime di "Disponi" la rimpiccioliscono). La si tiene (lock):
	 * dopo il primo disegno la scena la lascia andare e tiene solo la sua copia nella scheda video */
	struct wlr_buffer *sfondo;
	/* il suggerimento vicino al puntatore */
	struct wlr_scene_tree *suggerimento;
	char *suggerimento_testo;
	int suggerimento_l;
} mshell_sc;

static void mshell_controlli_segui(struct tinywl_toplevel *t);
static void mshell_controlli_metti(struct tinywl_server *server, struct tinywl_toplevel *t);
static void mshell_scelta_chiudi(void);
static void mshell_pill_aggiorna(struct tinywl_server *server);
static struct tinywl_toplevel *mshell_scelta_t;  /* (scrivania-controlli.c) */
static void mshell_controlli_avvia(void);

/* --- le immagini fatte a mano (icone) --- */

/* Un'immagine fatta a mano, sul modello dello sfondo (mshell_sfondo, piu' sotto in tinywl.c): i
 * pixel stanno in memoria e wlroots li legge con l'accesso diretto ai dati. */
struct mshell_pixel {
	struct wlr_buffer base;
	uint32_t *pixel;
	size_t passo;      /* byte per riga */
	uint32_t formato;  /* DRM_FORMAT_ARGB8888: trasparente, colori gia' moltiplicati per l'opacita' */
};

static void mshell_pixel_destroy(struct wlr_buffer *buffer) {
	struct mshell_pixel *p = wl_container_of(buffer, p, base);
	free(p->pixel);
	free(p);
}

static bool mshell_pixel_begin(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	struct mshell_pixel *p = wl_container_of(buffer, p, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;  /* si calcolano una volta e poi si leggono e basta */
	}
	*data = p->pixel;
	*format = p->formato;
	*stride = p->passo;
	return true;
}

static void mshell_pixel_end(struct wlr_buffer *buffer) {
}

static const struct wlr_buffer_impl mshell_pixel_impl = {
	.destroy = mshell_pixel_destroy,
	.begin_data_ptr_access = mshell_pixel_begin,
	.end_data_ptr_access = mshell_pixel_end,
};

/* L'immagine disegnata da cairo (ARGB32 di cairo: gia' moltiplicata, come ARGB8888). */
static struct mshell_pixel *mshell_pixel_da_cairo(cairo_surface_t *cs) {
	cairo_surface_flush(cs);
	int l = cairo_image_surface_get_width(cs), a = cairo_image_surface_get_height(cs);
	int passo = cairo_image_surface_get_stride(cs);
	struct mshell_pixel *p = calloc(1, sizeof(*p));
	if (p == NULL) {
		return NULL;
	}
	p->passo = (size_t)passo;
	p->pixel = malloc((size_t)passo * a);
	if (p->pixel == NULL) {
		free(p);
		return NULL;
	}
	memcpy(p->pixel, cairo_image_surface_get_data(cs), (size_t)passo * a);
	p->formato = DRM_FORMAT_ARGB8888;
	wlr_buffer_init(&p->base, &mshell_pixel_impl, l, a);
	return p;
}

/* Il nome da mostrare di un'app: l'ultimo pezzo del suo nome tecnico ("org.gnome.Nautilus" ->
 * "Nautilus", "mshell-prova-due" -> "due"). */
static const char *mshell_nome_corto(const char *nome) {
	if (nome == NULL || nome[0] == '\0') {
		return "?";
	}
	const char *c = nome;
	for (const char *p = nome; *p; p++) {
		if ((*p == '.' || *p == '-') && p[1] != '\0') {
			c = p + 1;
		}
	}
	return c;
}

/* I colori delle icone provvisorie: i colori saturi di base del tema scuro (token.css). */
static const char *const mshell_icona_colori[] = {
	"base-blu-saturo-scuro", "base-arancio-saturo-scuro", "base-azzurro-saturo-scuro",
	"base-verde-saturo-scuro", "base-rosa-saturo-scuro", "base-viola-saturo-scuro",
	"base-rosso-saturo-scuro", "base-menta-saturo-scuro", "base-marrone-saturo-scuro",
};

/* L'icona provvisoria dell'app "nome" (vedi in cima), fatta la prima volta e poi tenuta. */
static struct mshell_pixel *mshell_icona(const char *nome) {
	const char *chiave = nome != NULL ? nome : "";
	struct mshell_icona *i;
	wl_list_for_each(i, &mshell_sc.icone, link) {
		if (strcmp(i->nome, chiave) == 0) {
			return i->pixel;
		}
	}
	unsigned int h = 5381;
	for (const char *c = chiave; *c; c++) {
		h = h * 33 + (unsigned char)*c;
	}
	float colore[4];
	mshell_token_colore(mshell_icona_colori[h % (sizeof(mshell_icona_colori) / sizeof(mshell_icona_colori[0]))],
		colore);

	int lato = MSHELL_ICONA;
	cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, lato, lato);
	cairo_t *cr = cairo_create(cs);
	/* il quadrato arrotondato (il raggio delle icone del disegno: 22,5% del lato) */
	double r = lato * 0.225;
	cairo_new_sub_path(cr);
	cairo_arc(cr, lato - r, r, r, -G_PI / 2, 0);
	cairo_arc(cr, lato - r, lato - r, r, 0, G_PI / 2);
	cairo_arc(cr, r, lato - r, r, G_PI / 2, G_PI);
	cairo_arc(cr, r, r, r, G_PI, 3 * G_PI / 2);
	cairo_close_path(cr);
	cairo_set_source_rgba(cr, colore[0], colore[1], colore[2], 1);
	cairo_fill(cr);
	/* l'iniziale, bianca, al centro */
	char lettera[8] = { 0 };
	const char *corto = mshell_nome_corto(nome);
	gunichar u = g_unichar_toupper(g_utf8_get_char_validated(corto, -1));
	g_unichar_to_utf8(u, lettera);
	PangoLayout *pl = pango_cairo_create_layout(cr);
	PangoFontDescription *fd = pango_font_description_from_string("IBM Plex Sans SemiBold");
	pango_font_description_set_absolute_size(fd, lato * 0.5 * PANGO_SCALE);
	pango_layout_set_font_description(pl, fd);
	pango_layout_set_text(pl, lettera, -1);
	int tl, ta;
	pango_layout_get_pixel_size(pl, &tl, &ta);
	cairo_move_to(cr, (lato - tl) / 2.0, (lato - ta) / 2.0);
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	pango_cairo_show_layout(cr, pl);
	pango_font_description_free(fd);
	g_object_unref(pl);
	cairo_destroy(cr);

	struct mshell_pixel *p = mshell_pixel_da_cairo(cs);
	cairo_surface_destroy(cs);
	if (p == NULL) {
		return NULL;
	}
	i = calloc(1, sizeof(*i));
	if (i == NULL) {
		wlr_buffer_drop(&p->base);
		return NULL;
	}
	i->nome = strdup(chiave);
	i->pixel = p;  /* il riferimento lo tiene l'elenco: le icone restano finche' c'e' il compositore */
	wl_list_insert(&mshell_sc.icone, &i->link);
	return p;
}

/* --- i gruppi --- */

static struct mshell_gruppo *mshell_al_centro(struct tinywl_server *server) {
	return server->centro;
}

static void mshell_colonna_rifai(struct tinywl_server *server);

/* Lo spazio delle finestre: tutto lo schermo sotto la barra, meno il fondo. */
static struct wlr_box mshell_area(struct tinywl_server *server) {
	struct wlr_box a;
	wlr_output_layout_get_box(server->output_layout, NULL, &a);
	a.y += MSHELL_SOTTO_BARRA;
	a.height -= MSHELL_SOTTO_BARRA + MSHELL_FONDO;
	return a;
}

/* Mette la finestra al suo posto. La "geometria" dell'app dice dove comincia la finestra vera
 * dentro quello che disegna (alcune app disegnano un'ombra intorno): si tiene conto di quella. */
static void mshell_metti_al_posto(struct tinywl_toplevel *t) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
	int x, y;
	mshell_anim_dove(t, &x, &y);  /* il posto, o il punto a meta' strada se sta scorrendo */
	wlr_scene_node_set_position(&t->scene_tree->node, x - geo.x, y - geo.y);
	mshell_controlli_segui(t);  /* i controlli, se sono suoi, la seguono */
}

/* La tastiera all'app davanti del gruppo al centro (nessuna se non c'e'). Prende il posto del
 * vecchio mshell_mostra (livelli.c la usa quando un menu della barra lascia la tastiera). */
static void mshell_focus_centro(struct tinywl_server *server) {
	struct mshell_gruppo *g = mshell_al_centro(server);
	if (g == NULL || g->n == 0) {
		mshell_elenco_attiva(server, NULL);
		if (!mshell_livelli_tiene_tastiera(server)) {
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
		mshell_pill_aggiorna(server);  /* nessuna finestra attiva: niente pill */
		return;
	}
	struct tinywl_toplevel *t = g->app[g->n - 1];
	focus_toplevel(t, t->xdg_toplevel->base->surface);
	mshell_pill_aggiorna(server);  /* le pill sempre sulla finestra attiva */
}

/* Da focus_toplevel: la finestra passa davanti nel suo gruppo (in fondo all'elenco). */
static void mshell_gruppo_davanti(struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	if (g == NULL) {
		return;
	}
	int i = 0;
	while (i < g->n && g->app[i] != t) {
		i++;
	}
	if (i >= g->n - 1) {
		return;  /* gia' davanti (o non c'e') */
	}
	for (; i < g->n - 1; i++) {
		g->app[i] = g->app[i + 1];
	}
	g->app[g->n - 1] = t;
	mshell_pill_aggiorna(t->server);  /* e' la finestra attiva: le pill passano a lei */
}

/* Toglie il gruppo dalla colonna (se c'e'). */
static void mshell_colonna_togli(struct tinywl_server *server, struct mshell_gruppo *g) {
	for (int k = 0; k < server->n_gruppi; k++) {
		if (server->gruppo[k] == g) {
			memmove(&server->gruppo[k], &server->gruppo[k + 1],
				(size_t)(server->n_gruppi - 1 - k) * sizeof(server->gruppo[0]));
			server->n_gruppi--;
			return;
		}
	}
}

/* Mette il gruppo in cima alla colonna (spento: le sue finestre non si vedono). */
static void mshell_colonna_in_cima(struct tinywl_server *server, struct mshell_gruppo *g) {
	memmove(&server->gruppo[1], &server->gruppo[0], (size_t)server->n_gruppi * sizeof(server->gruppo[0]));
	server->gruppo[0] = g;
	server->n_gruppi++;
	wlr_scene_node_set_enabled(&g->albero->node, false);
}

/* Dopo un cambio al centro: niente scelta ne' controlli aperti, la colonna torna sotto le
 * finestre, la tastiera alla finestra davanti del centro, la colonna si rifa'. */
static void mshell_centro_cambiato(struct tinywl_server *server) {
	mshell_scelta_chiudi();
	mshell_controlli_metti(server, NULL);
	mshell_sc.sopra = false;
	wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	mshell_focus_centro(server);
	mshell_colonna_rifai(server);
	mshell_anim_puntatore();  /* sotto il puntatore fermo adesso c'e' altro */
}

/* Il gruppo "g" va al centro, acceso; quello che c'era (se c'era) scende in cima alla colonna. La
 * tastiera va alla sua finestra davanti. */
static void mshell_porta_al_centro(struct tinywl_server *server, struct mshell_gruppo *g) {
	if (g != server->centro) {
		mshell_colonna_togli(server, g);
		if (server->centro != NULL) {
			mshell_colonna_in_cima(server, server->centro);
		}
		server->centro = g;
	}
	wlr_scene_node_set_enabled(&g->albero->node, true);
	mshell_centro_cambiato(server);
}

/* Il gruppo al centro scende in cima alla colonna: il centro resta vuoto. */
static void mshell_svuota_centro(struct tinywl_server *server) {
	if (server->centro == NULL) {
		return;
	}
	mshell_colonna_in_cima(server, server->centro);
	server->centro = NULL;
	mshell_centro_cambiato(server);
}

/* Un gruppo nuovo, vuoto e spento, ne' al centro ne' nella colonna (lo mette chi lo chiede). NULL
 * se ce ne sono gia' troppi. */
static struct mshell_gruppo *mshell_gruppo_nuovo(struct tinywl_server *server) {
	if (server->n_gruppi + 1 >= MSHELL_GRUPPI_MAX) {
		return NULL;
	}
	struct mshell_gruppo *g = calloc(1, sizeof(*g));
	if (g == NULL) {
		return NULL;
	}
	g->albero = wlr_scene_tree_create(server->albero_gruppi);
	g->disposizione = -1;
	wlr_scene_node_set_enabled(&g->albero->node, false);
	return g;
}

/* La finestra entra nel gruppo, davanti. */
static void mshell_gruppo_entra(struct mshell_gruppo *g, struct tinywl_toplevel *t) {
	g->app[g->n++] = t;
	t->gruppo = g;
	wlr_scene_node_reparent(&t->scene_tree->node, g->albero);
	wlr_scene_node_raise_to_top(&t->scene_tree->node);
}

static void mshell_ridisponi(struct mshell_gruppo *g);

/* La finestra esce dal suo gruppo. Un gruppo rimasto vuoto sparisce; se era quello al centro, il
 * centro resta vuoto. */
static void mshell_gruppo_esce(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct mshell_gruppo *g = t->gruppo;
	if (g == NULL) {
		return;
	}
	t->gruppo = NULL;
	int i = 0;
	while (i < g->n && g->app[i] != t) {
		i++;
	}
	if (i < g->n) {
		memmove(&g->app[i], &g->app[i + 1], (size_t)(g->n - 1 - i) * sizeof(g->app[0]));
		g->n--;
	}
	/* l'albero della finestra torna nella scena, fuori dal gruppo (se il gruppo sparisce, la
	 * finestra non deve sparire con il suo albero) */
	wlr_scene_node_reparent(&t->scene_tree->node, server->albero_gruppi);
	if (mshell_sc.acceso == g) {
		mshell_sc.acceso = NULL;
	}
	if (mshell_sc.premuto == g && g->n == 0) {
		mshell_sc.premuto = NULL;  /* il gruppo che si trascinava non c'e' piu' */
		mshell_sc.trascina = false;
		if (mshell_sc.fantasma != NULL) {
			wlr_scene_node_destroy(&mshell_sc.fantasma->node);
			mshell_sc.fantasma = NULL;
		}
	}
	if (mshell_sc.sotto == t) {
		mshell_sc.sotto = NULL;
	}
	if (mshell_sc.scelta_gruppo == g || mshell_scelta_t == t) {
		mshell_scelta_chiudi();
	}
	mshell_ridisponi(g);
	mshell_pill_aggiorna(server);  /* le pill non restano su una finestra che non c'e' piu' */
	if (g->n == 0) {
		if (server->centro == g) {
			server->centro = NULL;
		} else {
			mshell_colonna_togli(server, g);
		}
		wlr_scene_node_destroy(&g->albero->node);
		free(g);
	}
	mshell_colonna_rifai(server);
}

/* La finestra che compare la prima volta: il suo gruppo e il suo posto (vedi in cima). */
static void mshell_assegna(struct tinywl_server *server, struct tinywl_toplevel *t) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
	struct wlr_box a = mshell_area(server);
	struct wlr_xdg_toplevel *madre = t->xdg_toplevel->parent;
	struct tinywl_toplevel *tm = NULL;
	if (madre != NULL && madre->base->data != NULL) {
		struct wlr_scene_tree *albero = madre->base->data;
		tm = albero->node.data;
	}
	struct mshell_gruppo *g;
	if (tm != NULL && tm->gruppo != NULL && tm->gruppo->n < MSHELL_GRUPPO_MAX) {
		/* un dialogo: nel gruppo della madre, al centro di lei */
		g = tm->gruppo;
		a = tm->posto;
	} else {
		g = mshell_gruppo_nuovo(server);
		if (g == NULL) {
			wlr_log(WLR_ERROR, "mshell: troppi gruppi (%d)", MSHELL_GRUPPI_MAX);
			return;
		}
	}
	t->posto.width = geo.width;
	t->posto.height = geo.height;
	t->posto.x = a.x + (a.width - geo.width) / 2;
	t->posto.y = a.y + (a.height - geo.height) / 2;
	struct wlr_box area = mshell_area(server);
	if (t->posto.y < area.y) {
		t->posto.y = area.y;
	}
	mshell_gruppo_entra(g, t);
	mshell_metti_al_posto(t);
	mshell_porta_al_centro(server, g);
}

/* --- la colonna --- */

/* Le miniature: copia (rimpicciolita) un'immagine della finestra, tagliata alla sua geometria. */
struct mshell_mini {
	struct wlr_scene_tree *padre;
	struct wlr_box geo;   /* la finestra vera, nelle coordinate del suo albero */
	double scala;
	int x, y;             /* dove va l'angolo della finestra vera nella miniatura */
	struct wlr_surface *principale;
};

static bool mshell_niente_clic(struct wlr_scene_buffer *b, double *sx, double *sy) {
	return false;
}

static void mshell_mini_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	struct mshell_mini *m = data;
	if (b->buffer == NULL || wlr_scene_surface_try_from_buffer(b) == NULL) {
		return;  /* solo le immagini dell'app (non i controlli della scrivania sopra di lei) */
	}
	int bl = b->dst_width > 0 ? b->dst_width : b->buffer->width;
	int ba = b->dst_height > 0 ? b->dst_height : b->buffer->height;
	struct wlr_box pezzo = { sx, sy, bl, ba }, dentro;
	if (!wlr_box_intersection(&dentro, &pezzo, &m->geo)) {
		return;  /* tutta fuori dalla finestra vera (l'ombra dell'app) */
	}
	struct wlr_scene_buffer *c = wlr_scene_buffer_create(m->padre, b->buffer);
	if (c == NULL) {
		return;
	}
	/* la parte dell'immagine da prendere, nei suoi pixel (l'immagine puo' avere piu' pixel della
	 * misura a cui si disegna: schermi densi) */
	struct wlr_fbox src = b->src_box;
	if (wlr_fbox_empty(&src)) {
		src = (struct wlr_fbox){ 0, 0, b->buffer->width, b->buffer->height };
	}
	double kx = src.width / bl, ky = src.height / ba;
	struct wlr_fbox taglio = {
		src.x + (dentro.x - sx) * kx, src.y + (dentro.y - sy) * ky,
		dentro.width * kx, dentro.height * ky,
	};
	wlr_scene_buffer_set_source_box(c, &taglio);
	int x = m->x + (int)((dentro.x - m->geo.x) * m->scala + 0.5);
	int y = m->y + (int)((dentro.y - m->geo.y) * m->scala + 0.5);
	int l = (int)(dentro.width * m->scala + 0.5), a = (int)(dentro.height * m->scala + 0.5);
	wlr_scene_buffer_set_dest_size(c, l > 0 ? l : 1, a > 0 ? a : 1);
	wlr_scene_buffer_set_transform(c, b->transform);
	wlr_scene_node_set_position(&c->node, x, y);
	struct wlr_scene_surface *s = wlr_scene_surface_try_from_buffer(b);
	if (s != NULL && s->surface == m->principale) {
		int raggio = (int)(MSHELL_RAGGIO_FINESTRA * m->scala + 0.5);
		wlr_scene_buffer_set_corner_radius(c, raggio > 1 ? raggio : 1, CORNER_LOCATION_ALL);
	}
	c->point_accepts_input = mshell_niente_clic;  /* il clic lo prende il riquadro del gruppo */
}

/* La miniatura della finestra t, grande al massimo lmax x amax, con l'angolo in (x, y) e
 * centrata in altezza in amax. */
static void mshell_mini_in(struct wlr_scene_tree *padre, struct tinywl_toplevel *t, int x, int y,
		double lmax, double amax) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
	if (geo.width <= 0 || geo.height <= 0) {
		return;
	}
	double sl = lmax / geo.width, sa = amax / geo.height;
	double scala = sl < sa ? sl : sa;
	int l = (int)(geo.width * scala + 0.5), a = (int)(geo.height * scala + 0.5);
	y += (int)((amax - a) / 2);
	struct wlr_scene_tree *albero = wlr_scene_tree_create(padre);
	/* l'ombra/finestra rimpicciolita come la finestra (in Figma le miniature sono la finestra in
	 * scala, ombra compresa); lo strato piu' largo sta in fondo */
	for (int i = MSHELL_STRATI_FINESTRA - 1; i >= 0; i--) {
		struct mshell_ombra o = mshell_effetti_ombra_scala(&mshell_ombre_finestra[i], scala);
		mshell_effetti_ombra(albero, (struct wlr_box){ x, y, l, a },
			(int)(MSHELL_RAGGIO_FINESTRA * scala + 0.5), &o);
	}
	/* le posizioni che da' wlr_scene_node_for_each_buffer contano anche quella del nodo da cui si
	 * parte (l'albero della finestra): la finestra vera sta li' dentro */
	geo.x += t->scene_tree->node.x;
	geo.y += t->scene_tree->node.y;
	struct mshell_mini m = {
		.padre = albero, .geo = geo, .scala = scala, .x = x, .y = y,
		.principale = t->xdg_toplevel->base->surface,
	};
	wlr_scene_node_for_each_buffer(&t->scene_tree->node, mshell_mini_una, &m);
}

/* La miniatura della finestra t, la k-esima da davanti, nel riquadro di un gruppo. */
static void mshell_mini(struct wlr_scene_tree *riquadro, struct tinywl_toplevel *t, int k) {
	double riduci = 1 - MSHELL_MINI_RIDUCI * k;
	mshell_mini_in(riquadro, t, MSHELL_MINI_X + MSHELL_MINI_PASSO_X * k,
		MSHELL_MINI_Y - MSHELL_MINI_PASSO_Y * k, MSHELL_MINI_L * riduci, MSHELL_MINI_A * riduci);
}

static void mshell_opacita_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	wlr_scene_buffer_set_opacity(b, *(float *)data);
}

/* Il riquadro del gruppo g nella colonna, con l'angolo in alto a sinistra in (x, y). */
static void mshell_riquadro(struct tinywl_server *server, struct mshell_gruppo *g, int x, int y) {
	struct wlr_scene_tree *r = wlr_scene_tree_create(mshell_sc.colonna);
	wlr_scene_node_set_position(&r->node, x, y);
	/* il fondo del riquadro: prende i clic (anche dove non c'e' una miniatura) e al passaggio del
	 * puntatore e' l'evidenza */
	float alfa = g == mshell_sc.acceso ? MSHELL_EVIDENZA : 0.0f;
	const float colore[4] = { alfa, alfa, alfa, alfa };
	struct wlr_scene_rect *fondo = wlr_scene_rect_create(r, MSHELL_RIQUADRO_L, MSHELL_RIQUADRO_A, colore);
	wlr_scene_rect_set_corner_radius(fondo, MSHELL_RIQUADRO_RAGGIO, CORNER_LOCATION_ALL);
	/* le miniature, dalla piu' dietro (cosi' quella davanti sta sopra) */
	int quante = g->n < MSHELL_MINI_QUANTE ? g->n : MSHELL_MINI_QUANTE;
	for (int k = quante - 1; k >= 0; k--) {
		mshell_mini(r, g->app[g->n - 1 - k], k);
	}
	/* le icone delle app, una per app, dalla finestra davanti */
	const char *nomi[MSHELL_ICONE_QUANTE];
	int n_nomi = 0;
	for (int i = g->n - 1; i >= 0 && n_nomi < MSHELL_ICONE_QUANTE; i--) {
		const char *nome = g->app[i]->xdg_toplevel->app_id;
		nome = nome != NULL ? nome : "";
		bool gia = false;
		for (int j = 0; j < n_nomi; j++) {
			gia = gia || strcmp(nomi[j], nome) == 0;
		}
		if (!gia) {
			nomi[n_nomi++] = nome;
		}
	}
	for (int j = n_nomi - 1; j >= 0; j--) {  /* quella dell'app davanti per ultima: sta sopra */
		struct mshell_pixel *p = mshell_icona(nomi[j]);
		if (p == NULL) {
			continue;
		}
		struct wlr_scene_buffer *b = wlr_scene_buffer_create(r, &p->base);
		wlr_scene_node_set_position(&b->node, MSHELL_ICONA_X + MSHELL_ICONA_PASSO * j, MSHELL_ICONA_Y);
		b->point_accepts_input = mshell_niente_clic;
	}
	/* il gruppo che si sta trascinando, nella colonna, si attenua (il disegno: 35%) */
	if (g == mshell_sc.premuto && mshell_sc.trascina) {
		float o = 0.35f;
		wlr_scene_node_for_each_buffer(&r->node, mshell_opacita_una, &o);
	}
}

/* Dove sta il riquadro i-esimo della colonna (il gruppo server->gruppo[i]). */
static int mshell_colonna_y(struct tinywl_server *server, int i) {
	struct wlr_box a = mshell_area(server);
	int quanti = server->n_gruppi;
	int tutta = quanti * MSHELL_RIQUADRO_A + (quanti - 1) * MSHELL_RIQUADRO_SPAZIO;
	return a.y + (a.height - tutta) / 2 + i * (MSHELL_RIQUADRO_A + MSHELL_RIQUADRO_SPAZIO);
}

static int mshell_colonna_disegna(void *data) {
	struct tinywl_server *server = data;
	struct wlr_scene_node *n, *dopo;
	wl_list_for_each_safe(n, dopo, &mshell_sc.colonna->children, link) {
		wlr_scene_node_destroy(n);
	}
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	for (int i = 0; i < server->n_gruppi; i++) {
		mshell_riquadro(server, server->gruppo[i], schermo.x + MSHELL_COLONNA_X,
			mshell_colonna_y(server, i));
	}
	return 0;
}

/* La colonna si rifa' una volta sola alla fine di questo giro del ciclo degli eventi. */
static void mshell_colonna_rifai(struct tinywl_server *server) {
	if (mshell_sc.colonna == NULL) {
		return;
	}
	if (mshell_sc.rifai == NULL) {
		mshell_sc.rifai = wl_event_loop_add_timer(wl_display_get_event_loop(server->wl_display),
			mshell_colonna_disegna, server);
	}
	wl_event_source_timer_update(mshell_sc.rifai, 1);
}

/* Il gruppo della colonna che si vede nel punto (lx, ly), o NULL: c'e' solo se in quel punto la
 * cosa piu' in alto della scena e' un pezzo della colonna (non una finestra che la copre). */
static struct mshell_gruppo *mshell_colonna_gruppo_a(struct tinywl_server *server, double lx, double ly) {
	double sx, sy;
	struct wlr_scene_node *n = wlr_scene_node_at(&server->scene->tree.node, lx, ly, &sx, &sy);
	while (n != NULL && n != &mshell_sc.colonna->node) {
		n = n->parent != NULL ? &n->parent->node : NULL;
	}
	if (n == NULL) {
		return NULL;
	}
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	for (int i = 0; i < server->n_gruppi; i++) {
		struct wlr_box r = { schermo.x + MSHELL_COLONNA_X, mshell_colonna_y(server, i),
			MSHELL_RIQUADRO_L, MSHELL_RIQUADRO_A };
		if (wlr_box_contains_point(&r, lx, ly)) {
			return server->gruppo[i];
		}
	}
	return NULL;
}

/* Un'app ha mandato un'immagine nuova: se sta in un gruppo della colonna, la sua miniatura si
 * rifa'. */
static void mshell_scrivania_invio(struct tinywl_server *server, struct tinywl_toplevel *t) {
	if (t->gruppo != NULL && t->gruppo != mshell_al_centro(server)) {
		mshell_colonna_rifai(server);
	}
}

/* All'avvio, dopo lo sfondo (il suo albero: "fondo"): l'albero delle finestre e, sotto di lui, la
 * colonna; sopra, i controlli. */
static void mshell_scrivania_avvia(struct tinywl_server *server, struct wlr_scene_tree *fondo) {
	mshell_sc.server = server;
	if (!wl_list_empty(&fondo->children)) {
		struct wlr_scene_node *n = wl_container_of(fondo->children.next, n, link);
		if (n->type == WLR_SCENE_NODE_BUFFER && wlr_scene_buffer_from_node(n)->buffer != NULL) {
			mshell_sc.sfondo = wlr_buffer_lock(wlr_scene_buffer_from_node(n)->buffer);
		}
	}
	wl_list_init(&mshell_sc.icone);
	server->albero_gruppi = wlr_scene_tree_create(&server->scene->tree);
	mshell_sc.colonna = wlr_scene_tree_create(&server->scene->tree);
	wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	mshell_sc.controlli = wlr_scene_tree_create(&server->scene->tree);
	mshell_controlli_avvia();
	/* i colori e le misure del disegno (token.c, nel tema del sistema) */
	mshell_token_colore("testo-primario", mshell_sc.colore_testo);
	mshell_token_colore("superficie-elevata", mshell_sc.colore_elevata);
	mshell_token_colore("superficie-vetro", mshell_sc.colore_vetro);
	mshell_token_colore("bordo-focus", mshell_sc.colore_focus);
	mshell_token_colore("azione-sfondo", mshell_sc.colore_azione);
	mshell_token_colore("azione-testo", mshell_sc.colore_azione_testo);
	mshell_sc.raggio_pannello = mshell_token_numero("raggio-pannello");
}

#include "scrivania-controlli.c"  /* i controlli sulla finestra, Disponi, il trascinamento */
