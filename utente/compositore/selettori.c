/* MSHELL: LA CORNICE DEL FOCUS E I SELETTORI (seconda tappa della scrivania). Decisi dalla
 * persona il 19/11/2026; il disegno e' in Figma, file "mshell – interfaccia", pagina
 * "Navigazione". Questo file non si compila da solo: tinywl.c lo include subito dopo le funzioni
 * dei desktop ("MSHELL: I DESKTOP"), di cui usa le strutture e il calcolo dei posti.
 *
 * Tutto si vede SOLO mentre Super e' tenuto premuto, e sparisce quando Super si lascia:
 *  - LA CORNICE DEL FOCUS: intorno all'app che ha la tastiera, un contorno fuori dall'app (l'app
 *    non si rimpicciolisce) e una fascia attaccata sotto, nello spazio libero sotto l'app (cosi'
 *    non copre la barra in alto, che resta visibile). Compare solo con Super+sinistra/destra.
 *    Super+sinistra/destra la sposta sull'app scelta.
 *  - IL SELETTORE DELLE DISPOSIZIONI: si apre con Super+Tab. Una fila al centro dello schermo con
 *    una miniatura per ogni disposizione possibile con le app del desktop; quella in uso ha il
 *    contorno. Ogni Super+Tab passa alla disposizione dopo e il contorno la segue. Con 1 o 3 app
 *    c'e' una disposizione sola, e la fila mostra quella; con 0 app non c'e' niente da disporre e
 *    la fila non compare.
 *  - IL SELETTORE DEI DESKTOP: compare quando Super resta giu' da solo oltre MSHELL_TENUTO_MS,
 *    e subito con Super+su/giu';
 *    Super+sinistra/destra lo chiude e passa alla cornice. Lo schermo si scurisce da sinistra (la sfumatura) e a
 *    sinistra c'e' una colonna con una miniatura per desktop, nell'ordine dei desktop; quello che
 *    si vede ha il contorno e sta sempre a sinistra al centro (in altezza); gli altri stanno
 *    sopra e sotto e scorrono quando si cambia desktop. Tenere Super oltre MSHELL_TENUTO_MS non e' un "tocco".
 * Ogni miniatura e' lo sfondo rimpicciolito con sopra le app come rettangoli di vetro, ai loro
 * posti in scala (bordo e spazio compresi).
 *
 * COME E' FATTO. Tutto sta in un livello della scena sopra i desktop. Ogni volta che qualcosa
 * cambia (Super, il focus, le app, le disposizioni, il desktop che si vede) il livello si svuota e
 * si rifa' da capo, una volta sola per giro del ciclo degli eventi (un "idle" di Wayland): sono
 * poche decine di rettangoli e costa poco, e cosi' le miniature non possono restare indietro
 * rispetto ai desktop. Le due immagini (lo sfondo rimpicciolito e la sfumatura) invece si
 * calcolano una volta sola, e di nuovo solo se cambia la misura dello schermo.
 * GLI EFFETTI (SceneFX, effetti.c): la cornice, i contorni, le miniature e le app dentro di loro
 * hanno gli angoli arrotondati; le miniature dei desktop e il pannello delle disposizioni hanno la
 * loro ombra. Il vetro dei selettori resta un colore semitrasparente: nel disegno non e' sfocato. */

#define MSHELL_LAMPO_MS 500        /* quanto resta la cornice dopo un tocco di Super */
#define MSHELL_TENUTO_MS 400       /* proposta, da confermare: oltre, Super giu' da solo apre il
                                    * selettore dei desktop */
#define MSHELL_SEL_SPAZIO_24 24    /* spazio/24: margine dei selettori */
#define MSHELL_SEL_SPAZIO_16 16    /* spazio/16: fra una miniatura e l'altra */
#define MSHELL_INDICATORE 2        /* indicatore/spessore: il contorno del focus e della scelta */
#define MSHELL_FASCIA 24           /* l'altezza della fascia sotto l'app col focus */
/* Le miniature sono un decimo dello schermo: 128x80 sullo schermo 1280x800 (la persona, 19/11/2026). */
#define MSHELL_MINI_NUM 1
#define MSHELL_MINI_DEN 10
/* bordo/focus scuro: la cornice del focus e i contorni della scelta (pieno) */
#define MSHELL_COLORE_FOCUS 0.89f, 0.89f, 0.90f, 1.0f
/* superficie/vetro scuro: il pannello delle disposizioni e le app nelle miniature */
#define MSHELL_COLORE_VETRO 0.25f, 0.25f, 0.27f, 0.72f
/* La sfumatura nera del selettore dei desktop: l'opacita' a sinistra, a un quarto, a destra. */
#define MSHELL_SFUMATURA_0 0.80f
#define MSHELL_SFUMATURA_25 0.55f
#define MSHELL_SFUMATURA_100 0.20f
/* I PUNTINI DEI DESKTOP (decisione della persona: "puntini sul bordo"): sul bordo sinistro, uno
 * per desktop, dall'alto in basso; quello che si vede e' acceso. Si vedono durante il cambio di
 * desktop e per un attimo dopo. Misure, colori e tempo sono proposte, da confermare. */
#define MSHELL_PUNTINO 6           /* il diametro */
#define MSHELL_PUNTINO_SPAZIO 8    /* fra un puntino e l'altro */
#define MSHELL_PUNTINO_SPENTO 1.0f, 1.0f, 1.0f, 0.45f  /* gli altri desktop */
#define MSHELL_PUNTINI_MS 1000     /* quanto restano dopo il cambio (lo scorrimento e' 250) */

/* Un'immagine fatta a mano, sul modello dello sfondo (mshell_sfondo, piu' sotto in tinywl.c):
 * i pixel stanno in memoria e wlroots li legge con l'accesso diretto ai dati. */
struct mshell_pixel {
	struct wlr_buffer base;
	uint32_t *pixel;
	size_t passo;      /* byte per riga */
	uint32_t formato;  /* DRM_FORMAT_XRGB8888 (opaca) o DRM_FORMAT_ARGB8888 (trasparente) */
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

/* Un'immagine vuota (nera e trasparente) della misura data; NULL se manca la memoria. */
static struct mshell_pixel *mshell_pixel_nuovo(int larghezza, int altezza, uint32_t formato) {
	struct mshell_pixel *p = calloc(1, sizeof(*p));
	if (p == NULL) {
		return NULL;
	}
	p->passo = (size_t)larghezza * 4;
	p->pixel = calloc((size_t)altezza, p->passo);
	if (p->pixel == NULL) {
		free(p);
		return NULL;
	}
	p->formato = formato;
	wlr_buffer_init(&p->base, &mshell_pixel_impl, larghezza, altezza);
	return p;
}

/* Lo stato dei selettori: uno solo, come il compositore. */
static struct {
	struct tinywl_server *server;
	struct wlr_scene_tree *fondo;    /* l'albero dello sfondo: le miniature lo rimpiccioliscono */
	struct wlr_scene_tree *livello;  /* sopra i desktop: tutto quello che si vede con Super */
	struct wl_event_source *tenuto;  /* il timer di Super tenuto giu' da solo */
	struct wl_event_source *rifai;   /* il rifacimento del livello in attesa (NULL: nessuno) */
	bool disposizioni;  /* la fila delle disposizioni e' aperta */
	bool app;           /* il focus e' sulle app (Super+sinistra/destra): si vede la cornice */
	bool desktop;       /* la colonna dei desktop e' aperta */
	bool lampo;         /* dopo un tocco di Super la cornice si vede ancora un attimo */
	struct wl_event_source *lampo_via;  /* il timer che la toglie */
	/* le immagini calcolate una volta, con la misura per cui sono state fatte */
	struct mshell_pixel *mini_sfondo;
	int mini_l, mini_a;
	struct mshell_pixel *sfumatura;
	int sfum_l, sfum_a;
	bool puntini;                      /* i puntini dei desktop si vedono */
	struct wl_event_source *puntini_via; /* il timer che li toglie */
	struct mshell_pixel *pallino[2];   /* il puntino spento [0] e acceso [1] */
} mshell_sel;

/* Un rettangolo pieno. I colori si scrivono "dritti" (come nel disegno), ma wlroots li vuole con
 * il rosso, il verde e il blu gia' moltiplicati per l'opacita': lo si fa qui. */
/* "raggio" e' quello degli angoli (0: dritti). */
static struct wlr_scene_rect *mshell_sel_rett(struct wlr_scene_tree *padre, int x, int y, int l,
		int a, int raggio, float r, float g, float b, float alfa) {
	if (l <= 0 || a <= 0) {
		return NULL;
	}
	const float colore[4] = { r * alfa, g * alfa, b * alfa, alfa };
	struct wlr_scene_rect *rett = wlr_scene_rect_create(padre, l, a, colore);
	wlr_scene_node_set_position(&rett->node, x, y);
	if (raggio > 0) {
		wlr_scene_rect_set_corner_radius(rett, raggio, CORNER_LOCATION_ALL);
	}
	return rett;
}

/* Il contorno tutto FUORI dal riquadro r (che resta intero), arrotondato in tutti gli angoli:
 * "sopra" in alto, "sotto" in basso, MSHELL_INDICATORE ai lati (la cornice del focus ha in basso la
 * sua fascia). E' un rettangolo solo, con un buco grande come il riquadro e con i suoi angoli di
 * raggio "raggio"; fuori gli angoli hanno il raggio piu' lo spessore, cosi' il contorno gira
 * intorno all'angolo con lo stesso spessore. */
static void mshell_sel_contorno(struct wlr_scene_tree *padre, struct wlr_box r, int sopra, int sotto,
		int raggio) {
	int s = MSHELL_INDICATORE;
	struct wlr_scene_rect *rett = mshell_sel_rett(padre, r.x - s, r.y - sopra,
		r.width + 2 * s, r.height + sopra + sotto, raggio + s, MSHELL_COLORE_FOCUS);
	if (rett == NULL) {
		return;
	}
	wlr_scene_rect_set_clipped_region(rett, (struct clipped_region){
		.area = { s, sopra, r.width, r.height },
		.corner_radius = raggio,
		.corners = CORNER_LOCATION_ALL,
	});
}

/* Il pixel dello sfondo che sta sotto il punto (x, y) dello schermo: nero fuori dall'immagine,
 * come si vede davvero sullo schermo. */
static uint32_t mshell_sel_pixel_sfondo(const uint8_t *dati, size_t passo, int l, int a,
		int x, int y) {
	if (x < 0 || y < 0 || x >= l || y >= a) {
		return 0;
	}
	return *(const uint32_t *)(dati + (size_t)y * passo + (size_t)x * 4);
}

/* Lo sfondo rimpicciolito alla misura delle miniature. Si prende l'immagine dello sfondo dalla
 * scena (la stessa che si vede sotto i desktop) e ogni pixel della miniatura e' la media dei
 * pixel dello schermo che copre: piu' fedele dell'ingrandimento "bilineare" della scena, che
 * rimpicciolendo tanto salta dei pixel, e lo si calcola una volta sola invece che a ogni
 * disegno. NULL se non c'e' sfondo (le miniature allora sono nere, come lo schermo). */
static struct mshell_pixel *mshell_sel_mini_sfondo(struct wlr_box schermo, int ml, int ma) {
	if (mshell_sel.mini_sfondo != NULL && mshell_sel.mini_l == ml && mshell_sel.mini_a == ma) {
		return mshell_sel.mini_sfondo;
	}
	if (mshell_sel.mini_sfondo != NULL) {
		wlr_buffer_drop(&mshell_sel.mini_sfondo->base);
		mshell_sel.mini_sfondo = NULL;
	}
	struct wlr_buffer *sfondo = NULL;
	struct wlr_scene_node *nodo;
	wl_list_for_each(nodo, &mshell_sel.fondo->children, link) {
		if (nodo->type == WLR_SCENE_NODE_BUFFER) {
			sfondo = wlr_scene_buffer_from_node(nodo)->buffer;
			break;
		}
	}
	void *dati;
	uint32_t formato;
	size_t passo;
	if (sfondo == NULL || !wlr_buffer_begin_data_ptr_access(sfondo,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &dati, &formato, &passo)) {
		return NULL;
	}
	if (formato != DRM_FORMAT_XRGB8888 && formato != DRM_FORMAT_ARGB8888) {
		wlr_buffer_end_data_ptr_access(sfondo);
		return NULL;
	}
	struct mshell_pixel *mini = mshell_pixel_nuovo(ml, ma, DRM_FORMAT_XRGB8888);
	if (mini == NULL) {
		wlr_buffer_end_data_ptr_access(sfondo);
		return NULL;
	}
	for (int j = 0; j < ma; j++) {
		int y0 = j * schermo.height / ma;
		int y1 = (j + 1) * schermo.height / ma;
		for (int i = 0; i < ml; i++) {
			int x0 = i * schermo.width / ml;
			int x1 = (i + 1) * schermo.width / ml;
			uint32_t r = 0, g = 0, b = 0, quanti = 0;
			for (int y = y0; y < y1; y++) {
				for (int x = x0; x < x1; x++) {
					uint32_t px = mshell_sel_pixel_sfondo(dati, passo, sfondo->width,
						sfondo->height, schermo.x + x, schermo.y + y);
					r += (px >> 16) & 0xff;
					g += (px >> 8) & 0xff;
					b += px & 0xff;
					quanti++;
				}
			}
			if (quanti > 0) {
				mini->pixel[(size_t)j * ml + i] =
					(r / quanti) << 16 | (g / quanti) << 8 | (b / quanti);
			}
		}
	}
	wlr_buffer_end_data_ptr_access(sfondo);
	mshell_sel.mini_sfondo = mini;
	mshell_sel.mini_l = ml;
	mshell_sel.mini_a = ma;
	return mini;
}

/* La sfumatura nera del selettore dei desktop, grande come lo schermo: l'opacita' scende da
 * sinistra a destra, a tratti dritti fra MSHELL_SFUMATURA_0, _25 (a un quarto) e _100. I pixel
 * sono neri, quindi "gia' moltiplicati per l'opacita'" vuol dire solo l'opacita'. */
static struct mshell_pixel *mshell_sel_sfumatura(int l, int a) {
	if (mshell_sel.sfumatura != NULL && mshell_sel.sfum_l == l && mshell_sel.sfum_a == a) {
		return mshell_sel.sfumatura;
	}
	if (mshell_sel.sfumatura != NULL) {
		wlr_buffer_drop(&mshell_sel.sfumatura->base);
		mshell_sel.sfumatura = NULL;
	}
	struct mshell_pixel *s = mshell_pixel_nuovo(l, a, DRM_FORMAT_ARGB8888);
	if (s == NULL) {
		return NULL;
	}
	for (int x = 0; x < l; x++) {
		float f = (x + 0.5f) / l;  /* dove sta la colonna, da 0 (sinistra) a 1 (destra) */
		float alfa = f < 0.25f
			? MSHELL_SFUMATURA_0 + (MSHELL_SFUMATURA_25 - MSHELL_SFUMATURA_0) * (f / 0.25f)
			: MSHELL_SFUMATURA_25 +
				(MSHELL_SFUMATURA_100 - MSHELL_SFUMATURA_25) * ((f - 0.25f) / 0.75f);
		s->pixel[x] = (uint32_t)(alfa * 255.0f + 0.5f) << 24;
	}
	for (int y = 1; y < a; y++) {
		memcpy(&s->pixel[(size_t)y * l], s->pixel, s->passo);  /* le righe sono tutte uguali */
	}
	mshell_sel.sfumatura = s;
	mshell_sel.sfum_l = l;
	mshell_sel.sfum_a = a;
	return s;
}

/* Riporta una misura dello schermo nella miniatura (arrotondando). */
static int mshell_sel_scala(int v, int miniatura, int schermo) {
	return (v * miniatura + schermo / 2) / schermo;
}

/* Una miniatura in (x, y): lo sfondo rimpicciolito e sopra le n app della disposizione "scelta",
 * come rettangoli di vetro ai loro posti in scala. I posti si calcolano con mshell_posto, lo
 * stesso calcolo dei desktop, cosi' la miniatura e' fedele anche nel bordo e nello spazio. */
static void mshell_sel_miniatura(struct wlr_scene_tree *padre, int x, int y,
		struct wlr_box schermo, int ml, int ma, int n, int scelta) {
	struct wlr_scene_tree *albero = wlr_scene_tree_create(padre);
	wlr_scene_node_set_position(&albero->node, x, y);
	struct mshell_pixel *sfondo = mshell_sel_mini_sfondo(schermo, ml, ma);
	if (sfondo != NULL) {
		struct wlr_scene_buffer *b = wlr_scene_buffer_create(albero, &sfondo->base);
		wlr_scene_buffer_set_corner_radius(b, MSHELL_RAGGIO_CONTROLLO, CORNER_LOCATION_ALL);
	} else {
		mshell_sel_rett(albero, 0, 0, ml, ma, MSHELL_RAGGIO_CONTROLLO, 0.0f, 0.0f, 0.0f, 1.0f);
	}
	if (n <= 0) {
		return;
	}
	if (scelta >= mshell_quante_scelte[n]) {
		scelta = 0;  /* come in mshell_disponi */
	}
	/* le app hanno gli angoli delle finestre, in scala (12 diventa 2) */
	int raggio = mshell_sel_scala(MSHELL_RAGGIO_FINESTRA, ml, schermo.width);
	struct wlr_box area = {
		.x = MSHELL_BORDO, .y = MSHELL_BORDO,
		.width = schermo.width - 2 * MSHELL_BORDO, .height = schermo.height - 2 * MSHELL_BORDO,
	};
	for (int i = 0; i < n; i++) {
		struct wlr_box p = mshell_posto(mshell_scelte[n][scelta], i, area);
		int x0 = mshell_sel_scala(p.x, ml, schermo.width);
		int y0 = mshell_sel_scala(p.y, ma, schermo.height);
		int x1 = mshell_sel_scala(p.x + p.width, ml, schermo.width);
		int y1 = mshell_sel_scala(p.y + p.height, ma, schermo.height);
		mshell_sel_rett(albero, x0, y0, x1 - x0, y1 - y0, raggio, MSHELL_COLORE_VETRO);
		/* QUI andra' l'icona dell'app, al centro del suo rettangolo: le icone delle app ancora
		 * non ci sono. */
	}
}

/* La cornice del focus sull'app che ha la tastiera nel desktop che si vede. */
static void mshell_sel_cornice(struct tinywl_server *server) {
	struct mshell_desktop *d = &server->desktop[server->attuale];
	if (d->n == 0 || d->focus >= d->n) {
		return;
	}
	/* il punto dove si vede l'app adesso: se sta scorrendo (animazioni.c), la cornice la segue */
	struct wlr_box p = d->app[d->focus]->posto;
	mshell_anim_dove(d->app[d->focus], &p.x, &p.y);
	/* il contorno con, in basso, la fascia: attaccata sotto l'app, larga quanto l'app piu' i due
	 * contorni; gira intorno agli angoli arrotondati dell'app */
	mshell_sel_contorno(mshell_sel.livello, p, MSHELL_INDICATORE, MSHELL_FASCIA,
		MSHELL_RAGGIO_FINESTRA);
	/* QUI andranno, nella fascia, il titolo dell'app e il pulsante X per chiuderla. Per
	 * scrivere il titolo serve una libreria di caratteri, e quale usare lo decide la persona. */
}

/* IL SEGNAPOSTO dell'app trascinata (la persona, 19/11/2026, ispirato a DashAI): l'app non si
 * vede e al suo posto, dove finira', c'e' un riquadro con il bordo tratteggiato e un velo del
 * colore del focus al 12%. Il tratteggio sono tanti trattini (rettangoli) sui lati dritti. */
#define MSHELL_TRATTO 10   /* lunghezza di un trattino */
#define MSHELL_VUOTO 6     /* spazio fra due trattini */
static void mshell_sel_segnaposto(struct tinywl_server *server) {
	struct tinywl_toplevel *t = server->portata;
	if (t == NULL || t->desktop != server->attuale) {
		return;
	}
	struct wlr_box p = t->posto;
	mshell_anim_dove(t, &p.x, &p.y);
	int s = MSHELL_INDICATORE, r = MSHELL_RAGGIO_FINESTRA;
	mshell_sel_rett(mshell_sel.livello, p.x, p.y, p.width, p.height, r,
		0.89f, 0.89f, 0.90f, 0.12f);
	for (int x = p.x + r; x < p.x + p.width - r; x += MSHELL_TRATTO + MSHELL_VUOTO) {
		int l = x + MSHELL_TRATTO > p.x + p.width - r ? p.x + p.width - r - x : MSHELL_TRATTO;
		mshell_sel_rett(mshell_sel.livello, x, p.y, l, s, 0, MSHELL_COLORE_FOCUS);
		mshell_sel_rett(mshell_sel.livello, x, p.y + p.height - s, l, s, 0, MSHELL_COLORE_FOCUS);
	}
	for (int y = p.y + r; y < p.y + p.height - r; y += MSHELL_TRATTO + MSHELL_VUOTO) {
		int a = y + MSHELL_TRATTO > p.y + p.height - r ? p.y + p.height - r - y : MSHELL_TRATTO;
		mshell_sel_rett(mshell_sel.livello, p.x, y, s, a, 0, MSHELL_COLORE_FOCUS);
		mshell_sel_rett(mshell_sel.livello, p.x + p.width - s, y, s, a, 0, MSHELL_COLORE_FOCUS);
	}
}

/* Il selettore dei desktop: la sfumatura su tutto lo schermo e la colonna delle miniature. */
static void mshell_sel_desktop(struct tinywl_server *server, struct wlr_box schermo,
		int ml, int ma) {
	struct mshell_pixel *s = mshell_sel_sfumatura(schermo.width, schermo.height);
	if (s != NULL) {
		struct wlr_scene_buffer *b = wlr_scene_buffer_create(mshell_sel.livello, &s->base);
		wlr_scene_node_set_position(&b->node, schermo.x, schermo.y);
	}
	int passo = ma + MSHELL_SEL_SPAZIO_16;
	/* il desktop attuale sta sempre a sinistra al centro dello schermo (in altezza), gli altri
	 * sopra e sotto nel loro ordine: cambiando desktop la colonna scorre */
	/* durante il trascinamento al centro c'e' la meta, e in fondo il desktop nuovo proposto
	 * (vuoto); il quadrato colorato sull'angolo della meta dice "stai portando un'app" */
	bool trascina = server->portata != NULL;
	int scelto = trascina ? server->meta : server->attuale;
	int quanti = trascina && server->giri > server->n_desktop ? server->giri : server->n_desktop;
	int centro = schermo.y + (schermo.height - ma) / 2;
	for (int i = 0; i < quanti; i++) {
		struct wlr_box m = {
			.x = schermo.x + MSHELL_SEL_SPAZIO_24,
			.y = centro + (i - scelto) * passo,
			.width = ml, .height = ma,
		};
		bool proposto = i == server->n_desktop;
		int n = proposto ? 0 : server->desktop[i].n;
		int disp = proposto ? 0 : server->desktop[i].scelta;
		mshell_effetti_ombra(mshell_sel.livello, m, MSHELL_RAGGIO_CONTROLLO, &mshell_ombra_sollevato);
		mshell_sel_miniatura(mshell_sel.livello, m.x, m.y, schermo, ml, ma, n, disp);
		if (i == scelto) {
			mshell_sel_contorno(mshell_sel.livello, m, MSHELL_INDICATORE, MSHELL_INDICATORE,
				MSHELL_RAGGIO_CONTROLLO);
		}
	}
	if (trascina) {
		/* L'ANTEPRIMA DELL'APP PORTATA, in alto a sinistra (la persona, 19/11/2026).
		 * PROVVISORIO: un rettangolo colorato grande come una miniatura, al posto
		 * dell'anteprima vera (l'icona o l'app rimpicciolita, da fare) */
		mshell_sel_rett(mshell_sel.livello, schermo.x + MSHELL_SEL_SPAZIO_24,
			schermo.y + MSHELL_SEL_SPAZIO_24 + 16, ml, ma,
			MSHELL_RAGGIO_CONTROLLO, 0.35f, 0.55f, 1.0f, 1.0f);
	}
}

/* Il selettore delle disposizioni: il pannello di vetro al centro con una miniatura per ogni
 * disposizione possibile con le app del desktop che si vede. */
static void mshell_sel_disposizioni(struct tinywl_server *server, struct wlr_box schermo,
		int ml, int ma) {
	struct mshell_desktop *d = &server->desktop[server->attuale];
	if (d->n == 0) {
		return;
	}
	int quante = mshell_quante_scelte[d->n];
	int scelta = d->scelta < quante ? d->scelta : 0;
	int l = 2 * MSHELL_SEL_SPAZIO_24 + quante * ml + (quante - 1) * MSHELL_SEL_SPAZIO_16;
	int a = 2 * MSHELL_SEL_SPAZIO_24 + ma;
	int x = schermo.x + (schermo.width - l) / 2;
	int y = schermo.y + (schermo.height - a) / 2;
	struct wlr_box pannello = { x, y, l, a };
	mshell_effetti_ombra(mshell_sel.livello, pannello, MSHELL_RAGGIO_FINESTRA,
		&mshell_ombra_sovrapposto);
	mshell_sel_rett(mshell_sel.livello, x, y, l, a, MSHELL_RAGGIO_FINESTRA, MSHELL_COLORE_VETRO);
	for (int i = 0; i < quante; i++) {
		struct wlr_box m = {
			.x = x + MSHELL_SEL_SPAZIO_24 + i * (ml + MSHELL_SEL_SPAZIO_16),
			.y = y + MSHELL_SEL_SPAZIO_24,
			.width = ml, .height = ma,
		};
		mshell_sel_miniatura(mshell_sel.livello, m.x, m.y, schermo, ml, ma, d->n, i);
		if (i == scelta) {
			mshell_sel_contorno(mshell_sel.livello, m, MSHELL_INDICATORE, MSHELL_INDICATORE,
				MSHELL_RAGGIO_CONTROLLO);
		}
	}
}

/* Svuota il livello e lo rifa' con lo stato di adesso. Gira come "idle": dopo che l'evento che
 * l'ha chiesto (un tasto, un'app nuova...) ha finito di sistemare i desktop. */
/* Un puntino tondo di diametro MSHELL_PUNTINO, del colore dato, con il bordo sfumato (i pixel
 * del bordo sono coperti solo in parte). Colori gia' moltiplicati per l'opacita', come vuole wlroots. */
static struct mshell_pixel *mshell_sel_pallino(float r, float g, float b, float alfa) {
	int d = MSHELL_PUNTINO;
	struct mshell_pixel *p = mshell_pixel_nuovo(d, d, DRM_FORMAT_ARGB8888);
	if (p == NULL) {
		return NULL;
	}
	double c = d / 2.0;
	for (int y = 0; y < d; y++) {
		for (int x = 0; x < d; x++) {
			/* quanto del pixel sta nel cerchio: 4x4 punti di prova per pixel */
			int dentro = 0;
			for (int sy = 0; sy < 4; sy++) {
				for (int sx = 0; sx < 4; sx++) {
					double px = x + (sx + 0.5) / 4 - c, py = y + (sy + 0.5) / 4 - c;
					dentro += px * px + py * py <= c * c;
				}
			}
			double a = alfa * dentro / 16.0;
			p->pixel[y * d + x] = (uint32_t)(a * 255 + 0.5) << 24 | (uint32_t)(r * a * 255 + 0.5) << 16 |
				(uint32_t)(g * a * 255 + 0.5) << 8 | (uint32_t)(b * a * 255 + 0.5);
		}
	}
	return p;
}

static void mshell_sel_pallino_metti(float *rgba, int quale) {
	if (mshell_sel.pallino[quale] == NULL) {
		mshell_sel.pallino[quale] = mshell_sel_pallino(rgba[0], rgba[1], rgba[2], rgba[3]);
	}
}

/* I puntini: in colonna, centrati in altezza e nel bordo libero SINISTRO (MSHELL_BORDO; la
 * persona, 19/11/2026: "metti i puntini a sx"). */
static void mshell_sel_puntini(struct tinywl_server *server, struct wlr_box schermo) {
	float spento[4] = { MSHELL_PUNTINO_SPENTO }, acceso[4] = { MSHELL_COLORE_FOCUS };
	mshell_sel_pallino_metti(spento, 0);
	mshell_sel_pallino_metti(acceso, 1);
	int n = server->n_desktop;
	int alto = n * MSHELL_PUNTINO + (n - 1) * MSHELL_PUNTINO_SPAZIO;
	int x = schermo.x + (MSHELL_BORDO - MSHELL_PUNTINO) / 2;
	int y = schermo.y + (schermo.height - alto) / 2;
	for (int i = 0; i < n; i++) {
		struct mshell_pixel *p = mshell_sel.pallino[i == server->attuale];
		if (p != NULL) {
			struct wlr_scene_buffer *b = wlr_scene_buffer_create(mshell_sel.livello, &p->base);
			wlr_scene_node_set_position(&b->node, x, y + i * (MSHELL_PUNTINO + MSHELL_PUNTINO_SPAZIO));
		}
	}
}

static void mshell_sel_rifai(void *data);
static void mshell_selettori_aggiorna(struct tinywl_server *server);

/* Il timer: passato il tempo, i puntini se ne vanno. */
static int mshell_sel_puntini_via(void *data) {
	mshell_sel.puntini = false;
	mshell_selettori_aggiorna(mshell_sel.server);
	return 0;
}

/* Usata da mshell_anim_desktop a ogni cambio di desktop: i puntini si vedono per un po'. */
static void mshell_selettori_puntini(struct tinywl_server *server) {
	if (mshell_sel.puntini_via == NULL) {
		mshell_sel.puntini_via = wl_event_loop_add_timer(
			wl_display_get_event_loop(server->wl_display), mshell_sel_puntini_via, NULL);
	}
	mshell_sel.puntini = true;
	wl_event_source_timer_update(mshell_sel.puntini_via, MSHELL_PUNTINI_MS);
	mshell_selettori_aggiorna(server);
}

static void mshell_sel_rifai(void *data) {
	struct tinywl_server *server = mshell_sel.server;
	mshell_sel.rifai = NULL;
	struct wlr_scene_node *nodo, *dopo;
	wl_list_for_each_safe(nodo, dopo, &mshell_sel.livello->children, link) {
		wlr_scene_node_destroy(nodo);
	}
	struct wlr_box schermo;
	wlr_output_layout_get_box(server->output_layout, NULL, &schermo);
	if (wlr_box_empty(&schermo)) {
		return;
	}
	/* il livello resta sopra a tutto, anche ad alberi aggiunti alla scena dopo di lui */
	wlr_scene_node_raise_to_top(&mshell_sel.livello->node);
	if (mshell_sel.puntini || server->portata != NULL) {  /* anche per tutto il trascinamento */
		mshell_sel_puntini(server, schermo);
	}
	if (!server->super_giu && !mshell_sel.lampo) {
		return;
	}
	int ml = schermo.width * MSHELL_MINI_NUM / MSHELL_MINI_DEN;
	int ma = schermo.height * MSHELL_MINI_NUM / MSHELL_MINI_DEN;
	/* la cornice del focus solo quando il focus e' sulle app, mai col selettore dei desktop */
	if (mshell_sel.app && !mshell_sel.desktop) {
		mshell_sel_cornice(server);
	}
	if (mshell_sel.desktop) {
		mshell_sel_desktop(server, schermo, ml, ma);
	}
	mshell_sel_segnaposto(server);
	if (mshell_sel.disposizioni) {
		mshell_sel_disposizioni(server, schermo, ml, ma);
	}
}

/* Chiede di rifare il livello (una volta sola, anche se lo si chiede piu' volte di fila). La
 * chiamano i desktop quando cambia qualcosa: app, disposizioni, focus, desktop che si vede. */
static void mshell_selettori_aggiorna(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL || mshell_sel.rifai != NULL) {
		return;
	}
	mshell_sel.rifai = wl_event_loop_add_idle(wl_display_get_event_loop(server->wl_display),
		mshell_sel_rifai, NULL);
}

/* Il timer di "tenuto" (MSHELL_TENUTO_MS). Super tenuto da solo: compare la cornice del focus
 * (prima si aspetta, perche' potrebbe arrivare un altro tasto, es. spazio). Alt tenuto con
 * Super: si apre il navigatore dei desktop. */
static int mshell_sel_tenuto(void *data) {
	struct tinywl_server *server = mshell_sel.server;
	if (server->super_giu && server->alt_giu && !server->alt_usato) {
		server->alt_tenuto = true;
		mshell_sel.desktop = true;
		mshell_selettori_aggiorna(server);
	} else if (server->super_giu && !server->super_usato &&
			!mshell_livelli_tiene_tastiera(server)) {  /* col cassetto aperto niente cornice */
		mshell_sel.app = true;
		mshell_selettori_aggiorna(server);
	}
	return 0;
}

/* Super premuto o lasciato (la persona, 19/11/2026): premuto, la cornice del focus compare dopo
 * MSHELL_TENUTO_MS se Super e' ancora giu' da solo (il timer); lasciato chiude tutto. */
static void mshell_selettori_super(struct tinywl_server *server, bool premuto) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.disposizioni = false;
	mshell_sel.desktop = false;
	mshell_sel.app = false;
	mshell_sel.lampo = false;
	wl_event_source_timer_update(mshell_sel.tenuto, premuto ? MSHELL_TENUTO_MS : 0);
	mshell_selettori_aggiorna(server);
}

/* Alt premuto con Super: parte il timer del navigatore; lasciato: il timer si ferma. */
static void mshell_selettori_alt(struct tinywl_server *server, bool premuto) {
	(void)server;
	if (mshell_sel.livello == NULL) {
		return;
	}
	wl_event_source_timer_update(mshell_sel.tenuto, premuto ? MSHELL_TENUTO_MS : 0);
}

static int mshell_sel_lampo_via(void *data) {
	(void)data;
	mshell_sel.lampo = false;
	mshell_selettori_aggiorna(mshell_sel.server);
	return 0;
}

/* Dopo un tocco di Super: la cornice si vede ancora un attimo sull'app che ha preso il focus. */
static void mshell_selettori_lampo(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.lampo = true;
	mshell_sel.app = true;
	wl_event_source_timer_update(mshell_sel.lampo_via, MSHELL_LAMPO_MS);
	mshell_selettori_aggiorna(server);
}

/* Tutto chiuso subito (cornice, selettori, timer), anche se Super e' ancora giu': si apre il
 * cassetto, che non deve avere niente sopra. */
static void mshell_selettori_chiudi(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.disposizioni = false;
	mshell_sel.desktop = false;
	mshell_sel.app = false;
	mshell_sel.lampo = false;
	wl_event_source_timer_update(mshell_sel.tenuto, 0);
	mshell_selettori_aggiorna(server);
}

static bool mshell_selettori_disposizioni_aperta(void) {
	return mshell_sel.disposizioni;
}

/* Super+su/giu': il selettore dei desktop si apre subito. */
static void mshell_selettori_desktop(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.desktop = true;
	mshell_sel.app = false;
	wl_event_source_timer_update(mshell_sel.tenuto, 0);
	mshell_selettori_aggiorna(server);
}

/* Super+sinistra/destra: il focus passa alle app; il selettore dei desktop si chiude e si vede
 * la cornice del focus. */
static void mshell_selettori_app(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.desktop = false;
	mshell_sel.app = true;
	wl_event_source_timer_update(mshell_sel.tenuto, 0);
	mshell_selettori_aggiorna(server);
}

/* Super+Tab: si apre (o resta aperta) la fila delle disposizioni. */
static void mshell_selettori_disposizioni(struct tinywl_server *server) {
	if (mshell_sel.livello == NULL) {
		return;
	}
	mshell_sel.disposizioni = true;
	mshell_selettori_aggiorna(server);
}

/* All'avvio, dopo lo sfondo e i desktop: il livello si crea per ultimo, quindi sta sopra. */
static void mshell_selettori_avvia(struct tinywl_server *server, struct wlr_scene_tree *fondo) {
	mshell_sel.server = server;
	mshell_sel.fondo = fondo;
	mshell_sel.livello = wlr_scene_tree_create(&server->scene->tree);
	mshell_sel.tenuto = wl_event_loop_add_timer(wl_display_get_event_loop(server->wl_display),
		mshell_sel_tenuto, NULL);
	mshell_sel.lampo_via = wl_event_loop_add_timer(wl_display_get_event_loop(server->wl_display),
		mshell_sel_lampo_via, NULL);
}
