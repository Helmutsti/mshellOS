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
 * fra la barra e il fondo). E' un albero della scena SOTTO le finestre, NASCOSTO quando al centro ci
 * sono finestre (mshell_colonna_mostra). Il puntatore contro il bordo sinistro la porta SOPRA le finestre; quando si
 * allontana oltre la colonna torna sotto. Ogni gruppo mostra le sue finestre in miniatura, una
 * dietro l'altra (fino a 3), e sotto a sinistra le icone delle sue app (una per app, fino a 3,
 * quella davanti sopra). Ogni gruppo occupa lo spazio che serve alle sue miniature (che hanno la
 * forma delle finestre). Un clic lo porta al centro.
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
 * LE ICONE DELLE APP: l'icona vera dell'app (la persona, 24/11/2026), cercata dal suo file .desktop
 * (la riga Icon=) nel tema "hicolor" delle app Flatpak e del sistema, disegnata con librsvg se e'
 * un SVG (quasi tutte le app di Flathub) o letta con cairo se e' un PNG. Se l'app non ha icona: un
 * quadrato arrotondato colorato con la sua iniziale (come il pannello di ricerca della barra); il
 * colore viene dal nome. Testo con pango e cairo, che nel sistema ci sono gia' (li porta GTK). */
#include <math.h>  /* fabs */
#include <pango/pangocairo.h>
#include <librsvg/rsvg.h>

/* Le misure del disegno (schermata "1 · Al lavoro") */
#define MSHELL_COLONNA_X 16          /* la colonna dal bordo sinistro */
#define MSHELL_RIQUADRO_L 184        /* il riquadro di un gruppo */
#define MSHELL_RIQUADRO_SPAZIO 20    /* fra un riquadro e l'altro */
#define MSHELL_MINI_X 22             /* la miniatura davanti, nel riquadro */
#define MSHELL_MINI_L 150            /* la sua misura massima */
#define MSHELL_MINI_A 100
#define MSHELL_MINI_PASSO_X 14       /* le miniature dietro: piu' a destra, */
#define MSHELL_MINI_PASSO_Y 10       /* piu' in alto */
#define MSHELL_MINI_RIDUCI 0.06      /* e un po' piu' piccole */
#define MSHELL_MINI_QUANTE 3
#define MSHELL_ICONA_PIXEL (2 * MSHELL_ICONA)  /* l'immagine dell'icona, al doppio */
#define MSHELL_ICONA 36              /* l'icona dell'app, in basso a sinistra nel riquadro */
#define MSHELL_ICONA_X 8
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
	bool col_vista;                        /* la colonna si vede (o sta entrando) */
	double col_da, col_a;                  /* la colonna scivola da x a x (0: al suo posto) */
	int64_t col_t0;                        /* da quando (0: ferma) */
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
	struct wlr_scene_tree *controlli_alti; /* sopra la barra in alto: la capsula a cavallo del bordo */
	struct tinywl_toplevel *sotto;         /* la finestra coi controlli (NULL: nessuna) */
	/* la scelta delle disposizioni */
	struct wlr_scene_tree *scelta;         /* NULL: chiusa */
	struct mshell_gruppo *scelta_gruppo;
	struct wlr_box scelta_box;             /* il pannello, sullo schermo */
	int n_opzioni, opzione_accesa;
	struct wlr_box opzione[5];             /* le miniature, sullo schermo */
	int opzione_disp[5];                   /* la disposizione di ciascuna */
	float colore_testo[4], colore_elevata[4], colore_focus[4], colore_vetro[4], colore_selezione[4],
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
static int mshell_finestra_cima(struct tinywl_toplevel *t);
static void mshell_controlli_avvia(void);

/* Il passaggio da un gruppo all'altro (scrivania-voli.c). */
#define MSHELL_VOLI_MAX (2 * MSHELL_GRUPPO_MAX)
struct mshell_volo {
	struct tinywl_toplevel *t;     /* NULL: la finestra se n'e' andata durante il volo */
	struct wlr_scene_tree *albero; /* la sua miniatura in volo */
	struct wlr_box da, a;          /* da dove a dove (la finestra vera, sullo schermo) */
	int64_t inizio;
	bool accendi;                  /* la finestra vera aspetta spenta e torna nel gruppo alla fine */
};
static struct {
	struct wlr_scene_tree *albero;  /* sopra le finestre, sotto i controlli */
	struct wlr_scene_tree *nascoste;  /* spento: le finestre vere che aspettano la fine del volo */
	bool corre;
	struct mshell_volo volo[MSHELL_VOLI_MAX];
	int n;
	struct mshell_gruppo *arriva;   /* il gruppo che arriva al centro: si accende alla fine */
	struct mshell_gruppo *atterra;  /* quello che va nella colonna: il riquadro alla fine */
} mshell_voli;
/* prima di un cambio al centro */
struct mshell_voli_prima {
	int entra_y;  /* il riquadro del gruppo che arriva al centro (-1: non era nella colonna) */
};
static void mshell_voli_prima(struct tinywl_server *server, struct mshell_gruppo *entra,
	struct mshell_voli_prima *p);
static void mshell_voli_parti(struct tinywl_server *server, const struct mshell_voli_prima *p,
	struct mshell_gruppo *esce, struct mshell_gruppo *entra);
static void mshell_voli_fine(struct tinywl_server *server);
static void mshell_voli_dimentica(struct tinywl_toplevel *t, struct mshell_gruppo *g, bool sparisce);
static int mshell_colonna_y_vista(struct tinywl_server *server, int i);

/* La tastiera e lo split di un gruppo (scrivania-tasti.c). */
static struct {
	bool tab;                        /* Super+Tab: si sta scegliendo un gruppo, */
	int tab_i;                       /* questo (indice nella colonna) */
	bool alt_giu, alt_altro;         /* Super+Alt: Alt giu', e se intanto si e' premuto altro */
	bool shift_giu;                  /* Shift giu': si scorre al contrario */
	bool giro;                       /* il giro delle finestre con Super+Alt, */
	struct tinywl_toplevel *giro_t[MSHELL_GRUPPO_MAX];  /* nell'ordine di quando e' cominciato */
	int giro_n, giro_i;
	struct mshell_gruppo *aperto;    /* il gruppo con lo split aperto (NULL: nessuno) */
	bool vent_apre;                  /* lo split si apre (o e' aperto); falso: si chiude */
	int64_t vent_da;                 /* quando e' partita l'animazione dello split (0: ferma) */
	int sel_f;                       /* la finestra scelta nello split (-1: nessuna) */
	struct wlr_box cella[MSHELL_GRUPPO_MAX];  /* le finestre dello split, sullo schermo */
	struct tinywl_toplevel *cella_t[MSHELL_GRUPPO_MAX];
	int n_celle;
	bool fondo_noto;                 /* il fondo tenue della scelta: dove si vede, */
	struct wlr_box fondo_da, fondo_a;  /* da dove a dove scivola, */
	int64_t fondo_t0;                /* da quando (0: fermo) */
	bool disp;                       /* Super+Alt+Tab: si sta scegliendo la disposizione */
	int z_volte;                     /* quante Z con Super tenuto (Z Z: libera la scrivania) */
	struct tinywl_toplevel *z_t;     /* la finestra mandata nel vassoio dalla prima Z */
} mshell_ts = { .sel_f = -1 };
static void mshell_spaccato_disegna(struct tinywl_server *server);
static bool mshell_split_di(struct mshell_gruppo *g);
static int mshell_split_alto(struct mshell_gruppo *g, int pila);
static bool mshell_tasti_tiene_colonna(void);
static bool mshell_tasti_clic(struct tinywl_server *server, double px, double py);
static void mshell_tasti_dimentica(struct tinywl_toplevel *t);
static void mshell_colonna_mostra(struct tinywl_server *server);
static int mshell_colonna_disegna(void *data);
static bool mshell_fondo_meta(struct tinywl_server *server, struct wlr_box *b);
static void mshell_fondo_scelta(struct wlr_scene_tree *padre, int x, int y, int l, int a, int raggio);
static bool mshell_ventaglio_scatto(struct tinywl_server *server, int64_t ora);
static int mshell_in_colonna(struct tinywl_server *server, struct mshell_gruppo *g);
static struct wlr_box mshell_icone_box(struct tinywl_server *server, int i);

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

/* Le cartelle dove stanno i file .desktop (in applications/) e le icone (in icons/), dalle app
 * Flatpak dell'utente a quelle del sistema. */
static char **mshell_cartelle_dati(void) {
	const char *casa = g_get_home_dir();
	char **c = g_new0(char *, 5);
	c[0] = g_build_filename(casa, ".local/share/flatpak/exports/share", NULL);
	c[1] = g_strdup("/var/lib/flatpak/exports/share");
	c[2] = g_build_filename(casa, ".local/share", NULL);
	c[3] = g_strdup("/usr/share");
	return c;
}

/* IL NOME VERO DELL'APP della finestra t: per un'app Flatpak quello con cui e' installata (lo dice
 * la sua scatola, nel file /.flatpak-info che Flatpak le mette dentro: la Calcolatrice di GNOME
 * chiama la sua finestra "gnome-calculator", ma e' installata come "org.gnome.Calculator", come il
 * suo file .desktop e la sua icona; lo stesso fa GNOME); per le altre il nome della finestra
 * (app_id). Si legge una volta e si tiene. */
static const char *mshell_app_vera(struct tinywl_toplevel *t) {
	if (t->app_vera == NULL) {
		pid_t pid = 0;
		wl_client_get_credentials(wl_resource_get_client(t->xdg_toplevel->resource), &pid, NULL, NULL);
		char *f = g_strdup_printf("/proc/%d/root/.flatpak-info", (int)pid);
		GKeyFile *k = g_key_file_new();
		if (pid > 0 && g_key_file_load_from_file(k, f, G_KEY_FILE_NONE, NULL)) {
			t->app_vera = g_key_file_get_string(k, "Application", "name", NULL);
		}
		g_key_file_free(k);
		g_free(f);
		if (t->app_vera == NULL) {
			t->app_vera = g_strdup(t->xdg_toplevel->app_id != NULL ? t->xdg_toplevel->app_id : "");
		}
	}
	return t->app_vera;
}

/* Il file dell'icona dell'app "nome" (il nome vero, mshell_app_vera), o NULL. */
static char *mshell_icona_file(const char *nome) {
	if (nome == NULL || nome[0] == '\0') {
		return NULL;
	}
	char **cartelle = mshell_cartelle_dati();
	/* il nome dell'icona: dalla riga Icon= del file .desktop, altrimenti l'app_id stesso */
	char *icona = NULL;
	char *desktop = g_strconcat(nome, ".desktop", NULL);
	for (int i = 0; cartelle[i] != NULL && icona == NULL; i++) {
		char *f = g_build_filename(cartelle[i], "applications", desktop, NULL);
		GKeyFile *k = g_key_file_new();
		if (g_key_file_load_from_file(k, f, G_KEY_FILE_NONE, NULL)) {
			icona = g_key_file_get_string(k, "Desktop Entry", "Icon", NULL);
		}
		g_key_file_free(k);
		g_free(f);
	}
	g_free(desktop);
	/* se non c'e' un file col suo nome, quello che dice che la finestra e' sua (StartupWMClass: la
	 * Calcolatrice di GNOME si chiama "gnome-calculator", il suo file "org.gnome.Calculator") */
	for (int i = 0; cartelle[i] != NULL && icona == NULL; i++) {
		char *dir = g_build_filename(cartelle[i], "applications", NULL);
		GDir *d = g_dir_open(dir, 0, NULL);
		const char *voce;
		while (d != NULL && icona == NULL && (voce = g_dir_read_name(d)) != NULL) {
			if (!g_str_has_suffix(voce, ".desktop")) {
				continue;
			}
			char *f = g_build_filename(dir, voce, NULL);
			GKeyFile *k = g_key_file_new();
			if (g_key_file_load_from_file(k, f, G_KEY_FILE_NONE, NULL)) {
				char *classe = g_key_file_get_string(k, "Desktop Entry", "StartupWMClass", NULL);
				if (classe != NULL && g_ascii_strcasecmp(classe, nome) == 0) {
					icona = g_key_file_get_string(k, "Desktop Entry", "Icon", NULL);
				}
				g_free(classe);
			}
			g_key_file_free(k);
			g_free(f);
		}
		if (d != NULL) {
			g_dir_close(d);
		}
		g_free(dir);
	}
	if (icona == NULL) {
		icona = g_strdup(nome);
	}
	char *trovato = NULL;
	if (g_path_is_absolute(icona)) {
		if (g_file_test(icona, G_FILE_TEST_IS_REGULAR)) {
			trovato = g_strdup(icona);
		}
	} else {
		/* il tema hicolor (prima il disegno vettoriale, poi le immagini dalla piu' grande) e
		 * pixmaps */
		static const char *const misure[] = { "scalable", "512x512", "256x256", "128x128", "96x96",
			"64x64", "48x48", NULL };
		static const char *const tipi[] = { ".svg", ".png", NULL };
		for (int i = 0; cartelle[i] != NULL && trovato == NULL; i++) {
			for (int m = 0; misure[m] != NULL && trovato == NULL; m++) {
				for (int e = 0; tipi[e] != NULL && trovato == NULL; e++) {
					char *base = g_strconcat(icona, tipi[e], NULL);
					char *f = g_build_filename(cartelle[i], "icons/hicolor", misure[m], "apps", base, NULL);
					if (g_file_test(f, G_FILE_TEST_IS_REGULAR)) {
						trovato = f;
					} else {
						g_free(f);
					}
					g_free(base);
				}
			}
			for (int e = 0; tipi[e] != NULL && trovato == NULL; e++) {
				char *base = g_strconcat(icona, tipi[e], NULL);
				char *f = g_build_filename(cartelle[i], "pixmaps", base, NULL);
				if (g_file_test(f, G_FILE_TEST_IS_REGULAR)) {
					trovato = f;
				} else {
					g_free(f);
				}
				g_free(base);
			}
		}
	}
	g_free(icona);
	g_strfreev(cartelle);
	return trovato;
}

/* Disegna nel quadrato "lato" x "lato" di cr l'icona del file f (SVG o PNG). Falso se non ci riesce. */
static bool mshell_icona_disegna(cairo_t *cr, const char *f, int lato) {
	if (g_str_has_suffix(f, ".svg")) {
		RsvgHandle *h = rsvg_handle_new_from_file(f, NULL);
		if (h == NULL) {
			return false;
		}
		RsvgRectangle dove = { 0, 0, lato, lato };
		bool ok = rsvg_handle_render_document(h, cr, &dove, NULL);
		g_object_unref(h);
		return ok;
	}
	cairo_surface_t *png = cairo_image_surface_create_from_png(f);
	if (cairo_surface_status(png) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(png);
		return false;
	}
	int l = cairo_image_surface_get_width(png), a = cairo_image_surface_get_height(png);
	double scala = (double)lato / (l > a ? l : a);
	cairo_save(cr);
	cairo_translate(cr, (lato - l * scala) / 2, (lato - a * scala) / 2);
	cairo_scale(cr, scala, scala);
	cairo_set_source_surface(cr, png, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	cairo_paint(cr);
	cairo_restore(cr);
	cairo_surface_destroy(png);
	return true;
}

/* L'icona dell'app "nome" (vedi in cima), fatta la prima volta e poi tenuta. E' disegnata al
 * doppio della misura (MSHELL_ICONA_PIXEL), cosi' resta nitida anche sugli schermi densi. */
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

	int lato = MSHELL_ICONA_PIXEL;
	cairo_surface_t *cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, lato, lato);
	cairo_t *cr = cairo_create(cs);
	char *file = mshell_icona_file(nome);
	bool vera = file != NULL && mshell_icona_disegna(cr, file, lato);
	g_free(file);
	if (!vera) {
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
	}
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
			g->y_noto = false;  /* quando torna nella colonna, ci compare al suo posto */
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
	mshell_ts.aperto = NULL;  /* lo split si chiude */
	mshell_ts.sel_f = -1;
	mshell_ts.n_celle = 0;
	mshell_scelta_chiudi();
	mshell_controlli_metti(server, NULL);
	mshell_sc.sopra = false;
	wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	mshell_colonna_mostra(server);
	mshell_focus_centro(server);
	mshell_colonna_rifai(server);
	mshell_anim_puntatore();  /* sotto il puntatore fermo adesso c'e' altro */
}

/* Il gruppo "g" va al centro, acceso; quello che c'era (se c'era) scende in cima alla colonna. La
 * tastiera va alla sua finestra davanti. */
static void mshell_porta_al_centro(struct tinywl_server *server, struct mshell_gruppo *g) {
	if (g != server->centro) {
		struct mshell_voli_prima prima;
		mshell_voli_prima(server, g, &prima);
		struct mshell_gruppo *esce = server->centro;
		mshell_colonna_togli(server, g);
		if (server->centro != NULL) {
			mshell_colonna_in_cima(server, server->centro);
		}
		server->centro = g;
		mshell_voli_parti(server, &prima, esce, g);  /* scrivania-voli.c */
	}
	/* se sta arrivando in volo, le finestre vere si accendono all'arrivo */
	wlr_scene_node_set_enabled(&g->albero->node, mshell_voli.arriva != g);
	mshell_centro_cambiato(server);
}

/* Il gruppo al centro scende in cima alla colonna: il centro resta vuoto. */
static void mshell_svuota_centro(struct tinywl_server *server) {
	if (server->centro == NULL) {
		return;
	}
	struct mshell_voli_prima prima;
	mshell_voli_prima(server, NULL, &prima);
	struct mshell_gruppo *esce = server->centro;
	mshell_colonna_in_cima(server, server->centro);
	server->centro = NULL;
	mshell_voli_parti(server, &prima, esce, NULL);  /* scrivania-voli.c */
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
	mshell_voli_dimentica(t, g, g->n == 0);  /* non vola piu' (scrivania-voli.c) */
	mshell_tasti_dimentica(t);  /* ne' resta nel giro o nello spaccato (scrivania-tasti.c) */
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

/* L'APERTURA TRASLATA, COME SUL MAC (la persona, 23/11/2026: "le finestre continuano ad aprirsi
 * una sull'altra"): se nel punto in cui va la finestra t c'e' gia' l'angolo di un'altra (del gruppo
 * g; con g NULL, di qualsiasi gruppo), t scende in basso a destra di MSHELL_CASCATA, finche' trova
 * un punto libero; se esce dallo spazio delle finestre ricomincia in alto a sinistra. Non vale per
 * i gruppi disposti con "Disponi". */
#define MSHELL_CASCATA 28
static void mshell_cascata(struct tinywl_server *server, struct mshell_gruppo *g, struct tinywl_toplevel *t) {
	if (g != NULL && g->disposizione >= 0) {
		return;
	}
	struct wlr_box area = mshell_area(server);
	for (int giri = 0; giri < 32; giri++) {
		bool sopra = false;
		struct tinywl_toplevel *w;
		wl_list_for_each(w, &server->toplevels, link) {
			if (w != t && w->gruppo != NULL && (g == NULL || w->gruppo == g) &&
					abs(w->posto.x - t->posto.x) < 8 && abs(w->posto.y - t->posto.y) < 8) {
				sopra = true;
			}
		}
		if (!sopra) {
			return;
		}
		t->posto.x += MSHELL_CASCATA;
		t->posto.y += MSHELL_CASCATA;
		if (t->posto.x + t->posto.width > area.x + area.width ||
				t->posto.y + t->posto.height > area.y + area.height) {
			t->posto.x = area.x + MSHELL_CASCATA;
			t->posto.y = mshell_finestra_cima(t);
		}
	}
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
	if (t->posto.y < mshell_finestra_cima(t)) {
		t->posto.y = mshell_finestra_cima(t);  /* le pill devono starci sopra */
	}
	if (tm == NULL) {
		mshell_cascata(server, NULL, t);  /* non proprio sopra un'altra finestra */
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

/* La misura della miniatura della finestra t, grande al massimo lmax x amax (con la forma della
 * finestra); 0 se la finestra non ha ancora una misura. */
static double mshell_mini_misura(struct tinywl_toplevel *t, double lmax, double amax, int *l, int *a) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
	*l = *a = 0;
	if (geo.width <= 0 || geo.height <= 0) {
		return 0;
	}
	double sl = lmax / geo.width, sa = amax / geo.height;
	double scala = sl < sa ? sl : sa;
	*l = (int)(geo.width * scala + 0.5);
	*a = (int)(geo.height * scala + 0.5);
	return scala;
}

/* La miniatura della finestra t, grande al massimo lmax x amax, con l'angolo in (x, y); se
 * "centra", centrata in altezza in amax. */
static void mshell_mini_in(struct wlr_scene_tree *padre, struct tinywl_toplevel *t, int x, int y,
		double lmax, double amax, bool centra) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(t->xdg_toplevel->base, &geo);
	int l, a;
	double scala = mshell_mini_misura(t, lmax, amax, &l, &a);
	if (scala <= 0) {
		return;
	}
	if (centra) {
		y += (int)((amax - a) / 2);
	}
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

/* LO SPAZIO DI UN GRUPPO NELLA COLONNA si adatta alle sue finestre (la persona, 23/11/2026: niente
 * spazio fisso): le miniature hanno la forma delle finestre (al massimo MSHELL_MINI_L x
 * MSHELL_MINI_A), quelle dietro sporgono in alto di MSHELL_MINI_PASSO_Y ciascuna, e le icone
 * stanno sull'angolo in basso a sinistra di quella davanti (sporgono sotto di 8, come nel disegno).
 * "cima": dove comincia la miniatura davanti, nel riquadro. */
#define MSHELL_ICONA_SPORGE 8
static int mshell_riquadro_cima(struct mshell_gruppo *g) {
	int quante = g->n < MSHELL_MINI_QUANTE ? g->n : MSHELL_MINI_QUANTE;
	return MSHELL_MINI_PASSO_Y * (quante > 0 ? quante - 1 : 0);
}

/* L'altezza della miniatura davanti del gruppo (almeno quella dell'icona). */
static int mshell_riquadro_davanti(struct mshell_gruppo *g) {
	int l, a = 0;
	if (g->n > 0) {
		mshell_mini_misura(g->app[g->n - 1], MSHELL_MINI_L, MSHELL_MINI_A, &l, &a);
	}
	return a > MSHELL_ICONA ? a : MSHELL_ICONA;
}

/* Quante icone ha il riquadro: una per app, fino a MSHELL_ICONE_QUANTE. */
static int mshell_riquadro_icone(struct mshell_gruppo *g) {
	const char *nomi[MSHELL_ICONE_QUANTE];
	int n_nomi = 0;
	for (int i = g->n - 1; i >= 0 && n_nomi < MSHELL_ICONE_QUANTE; i--) {
		const char *nome = mshell_app_vera(g->app[i]);
		nome = nome != NULL ? nome : "";
		bool gia = false;
		for (int j = 0; j < n_nomi; j++) {
			gia = gia || strcmp(nomi[j], nome) == 0;
		}
		if (!gia) {
			nomi[n_nomi++] = nome;
		}
	}
	return n_nomi;
}

/* LA LARGHEZZA DEL RIQUADRO (la persona, 24/11/2026: "il riquadro si stringe intorno alla
 * miniatura, altezza fissa e larghezza variabile"): quanto serve alle miniature (di misura come
 * prima, al massimo MSHELL_MINI_L x MSHELL_MINI_A) e alle icone, con lo stesso margine a destra del
 * riquadro pieno; al massimo MSHELL_RIQUADRO_L. Con lo split aperto resta pieno. */
static int mshell_riquadro_largo(struct mshell_gruppo *g) {
	if (mshell_split_di(g) || g->n == 0) {
		return MSHELL_RIQUADRO_L;
	}
	int destra = 0;
	int quante = g->n < MSHELL_MINI_QUANTE ? g->n : MSHELL_MINI_QUANTE;
	for (int k = 0; k < quante; k++) {
		double riduci = 1 - MSHELL_MINI_RIDUCI * k;
		int l, a;
		mshell_mini_misura(g->app[g->n - 1 - k], MSHELL_MINI_L * riduci, MSHELL_MINI_A * riduci, &l, &a);
		int d = MSHELL_MINI_X + MSHELL_MINI_PASSO_X * k + l;
		destra = d > destra ? d : destra;
	}
	int icone = MSHELL_ICONA_X + MSHELL_ICONA_PASSO * (mshell_riquadro_icone(g) - 1) + MSHELL_ICONA;
	destra = icone > destra ? icone : destra;
	int l = destra + (MSHELL_RIQUADRO_L - MSHELL_MINI_X - MSHELL_MINI_L);
	return l < MSHELL_RIQUADRO_L ? l : MSHELL_RIQUADRO_L;
}

static int mshell_riquadro_alto(struct mshell_gruppo *g) {
	int pila = mshell_riquadro_cima(g) + mshell_riquadro_davanti(g) + MSHELL_ICONA_SPORGE;
	return mshell_split_di(g) ? mshell_split_alto(g, pila) : pila;  /* lo split lo allunga */
}

/* La miniatura della finestra t, la k-esima da davanti, nel riquadro di un gruppo: in alto
 * allineata con quella davanti, piu' su di MSHELL_MINI_PASSO_Y per ognuna. */
static void mshell_mini(struct wlr_scene_tree *riquadro, struct tinywl_toplevel *t, int k, int cima) {
	double riduci = 1 - MSHELL_MINI_RIDUCI * k;
	mshell_mini_in(riquadro, t, MSHELL_MINI_X + MSHELL_MINI_PASSO_X * k,
		cima - MSHELL_MINI_PASSO_Y * k, MSHELL_MINI_L * riduci, MSHELL_MINI_A * riduci, false);
}

static void mshell_opacita_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	wlr_scene_buffer_set_opacity(b, *(float *)data);
}

/* Il riquadro del gruppo g nella colonna, con l'angolo in alto a sinistra in (x, y). */
static void mshell_riquadro(struct tinywl_server *server, struct mshell_gruppo *g, int x, int y) {
	struct wlr_scene_tree *r = wlr_scene_tree_create(mshell_sc.colonna);
	wlr_scene_node_set_position(&r->node, x, y);
	/* il fondo del riquadro, trasparente: prende i clic anche dove non c'e' una miniatura (niente
	 * evidenza al passaggio del puntatore: tolta dalla persona, 23/11/2026) */
	const float trasparente[4] = { 0, 0, 0, 0 };
	wlr_scene_rect_create(r, mshell_riquadro_largo(g), mshell_riquadro_alto(g), trasparente);
	/* le miniature, dalla piu' dietro (cosi' quella davanti sta sopra); col gruppo aperto le
	 * disegna lo split (scrivania-tasti.c) */
	int cima = mshell_riquadro_cima(g);
	int quante = g->n < MSHELL_MINI_QUANTE ? g->n : MSHELL_MINI_QUANTE;
	for (int k = quante - 1; k >= 0 && !mshell_split_di(g); k--) {
		mshell_mini(r, g->app[g->n - 1 - k], k, cima);
	}
	int icona_y = cima + mshell_riquadro_davanti(g) + MSHELL_ICONA_SPORGE - MSHELL_ICONA;
	/* le icone delle app, una per app, dalla finestra davanti */
	const char *nomi[MSHELL_ICONE_QUANTE];
	int n_nomi = 0;
	for (int i = g->n - 1; i >= 0 && n_nomi < MSHELL_ICONE_QUANTE; i--) {
		const char *nome = mshell_app_vera(g->app[i]);
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
		wlr_scene_buffer_set_dest_size(b, MSHELL_ICONA, MSHELL_ICONA);
		wlr_scene_node_set_position(&b->node, MSHELL_ICONA_X + MSHELL_ICONA_PASSO * j, icona_y);
		b->point_accepts_input = mshell_niente_clic;
	}
	/* il gruppo che si sta trascinando, nella colonna, si attenua (il disegno: 35%) */
	if (g == mshell_sc.premuto && mshell_sc.trascina) {
		float o = 0.35f;
		wlr_scene_node_for_each_buffer(&r->node, mshell_opacita_una, &o);
	}
}

/* Dove sta il riquadro i-esimo della colonna (il gruppo server->gruppo[i]): i riquadri, ognuno
 * alto quanto serve, uno sotto l'altro a MSHELL_RIQUADRO_SPAZIO, centrati in altezza. */
static int mshell_colonna_y(struct tinywl_server *server, int i) {
	struct wlr_box a = mshell_area(server);
	int tutta = 0, prima = 0;
	for (int k = 0; k < server->n_gruppi; k++) {
		if (k == i) {
			prima = tutta;
		}
		tutta += mshell_riquadro_alto(server->gruppo[k]) + (k > 0 ? MSHELL_RIQUADRO_SPAZIO : 0);
		if (k == i && k > 0) {
			prima += MSHELL_RIQUADRO_SPAZIO;
		}
	}
	return a.y + (a.height - tutta) / 2 + prima;
}

/* LA COLONNA E' NASCOSTA quando al centro ci sono finestre (la persona, 23/11/2026: "il vassoio
 * deve essere normalmente nascosto se ci sono pagine sullo schermo"): si vede col centro vuoto, e
 * quando sale sopra le finestre (il puntatore al bordo sinistro, Super+Tab, lo split). */
/* Dove sta adesso la colonna, in orizzontale (0: al suo posto; negativo: fuori a sinistra). */
static double mshell_colonna_x(int64_t ora) {
	if (mshell_sc.col_t0 == 0) {
		return mshell_sc.col_da;
	}
	double e = mshell_anim_curva_dolce(mshell_sc.col_t0, MSHELL_DURATA_MEDIA, ora);
	return mshell_sc.col_da + (mshell_sc.col_a - mshell_sc.col_da) * e;
}

static void mshell_colonna_mostra(struct tinywl_server *server) {
	if (mshell_sc.colonna == NULL) {
		return;
	}
	struct mshell_gruppo *c = server->centro;
	bool vista = mshell_sc.sopra || c == NULL || c->n == 0;
	if (vista == mshell_sc.col_vista) {
		return;
	}
	/* SCIVOLA da sinistra (e torna a sinistra), come la striscia dello Stage Manager (la persona,
	 * 23/11/2026) */
	int64_t ora = mshell_anim_adesso();
	double adesso = mshell_colonna_x(ora);
	mshell_sc.col_vista = vista;
	mshell_sc.col_da = adesso;
	mshell_sc.col_a = vista ? 0 : -MSHELL_COLONNA_FINE;
	if (vista) {
		wlr_scene_node_set_enabled(&mshell_sc.colonna->node, true);
	}
	if (mshell_anim_spenta()) {
		mshell_sc.col_t0 = 0;
		mshell_sc.col_da = mshell_sc.col_a;
		wlr_scene_node_set_position(&mshell_sc.colonna->node, (int)mshell_sc.col_a, 0);
		wlr_scene_node_set_enabled(&mshell_sc.colonna->node, vista);
		return;
	}
	mshell_sc.col_t0 = ora;
	mshell_anim_avvia();
}

/* Uno scatto del timer delle animazioni (animazioni.c): la colonna che scivola, i riquadri che
 * scorrono, il fondo della scelta, lo split. True se qualcosa si muove ancora. */
static bool mshell_colonna_scatto(struct tinywl_server *server, int64_t ora) {
	if (mshell_sc.colonna == NULL) {
		return false;
	}
	bool ancora = false;
	if (mshell_sc.col_t0 != 0) {
		wlr_scene_node_set_position(&mshell_sc.colonna->node, (int)mshell_colonna_x(ora), 0);
		if (mshell_anim_curva_dolce(mshell_sc.col_t0, MSHELL_DURATA_MEDIA, ora) >= 1) {
			mshell_sc.col_t0 = 0;
			mshell_sc.col_da = mshell_sc.col_a;
			if (!mshell_sc.col_vista) {
				wlr_scene_node_set_enabled(&mshell_sc.colonna->node, false);
			}
		} else {
			ancora = true;
		}
	}
	bool rifai = mshell_ts.fondo_t0 != 0 || mshell_ventaglio_scatto(server, ora);
	for (int i = 0; i < server->n_gruppi && !rifai; i++) {
		rifai = server->gruppo[i]->y_t0 != 0;
	}
	if (rifai) {
		mshell_colonna_disegna(server);
		ancora = true;
	}
	return ancora;
}

/* Dove si vede adesso il riquadro i-esimo della colonna: quando il suo posto cambia (un gruppo
 * nasce, sparisce, cambia misura, i gruppi si riordinano) ci scorre in MSHELL_DURATA_MEDIA. Mentre
 * lo split si apre o si chiude segue il posto di ogni momento. */
static int mshell_colonna_y_vista(struct tinywl_server *server, int i) {
	struct mshell_gruppo *g = server->gruppo[i];
	int y = mshell_colonna_y(server, i);
	if (!g->y_noto || mshell_ts.vent_da != 0 || mshell_anim_spenta()) {
		g->y_noto = true;
		g->y_da = g->y_a = y;
		g->y_t0 = 0;
		return y;
	}
	int64_t ora = mshell_anim_adesso();
	double e = g->y_t0 != 0 ? mshell_anim_curva_dolce(g->y_t0, MSHELL_DURATA_MEDIA, ora) : 1;
	int adesso = (int)(g->y_da + (g->y_a - g->y_da) * e + 0.5);
	if (y != g->y_a) {
		g->y_da = adesso;
		g->y_a = y;
		g->y_t0 = ora;
		mshell_anim_avvia();
		return adesso;
	}
	if (e >= 1) {
		g->y_t0 = 0;
	}
	return adesso;
}

/* Il fondo tenue della scelta (scrivania-tasti.c, mshell_fondo_meta): scivola dalla scelta di
 * prima alla nuova in MSHELL_DURATA_MEDIA. */
static void mshell_fondo_disegna(struct tinywl_server *server) {
	struct wlr_box meta;
	if (!mshell_fondo_meta(server, &meta)) {
		mshell_ts.fondo_noto = false;
		return;
	}
	int64_t ora = mshell_anim_adesso();
	struct wlr_box b = meta;
	if (!mshell_ts.fondo_noto || mshell_ts.vent_da != 0 || mshell_anim_spenta()) {
		mshell_ts.fondo_noto = true;
		mshell_ts.fondo_da = mshell_ts.fondo_a = meta;
		mshell_ts.fondo_t0 = 0;
	} else {
		double e = mshell_ts.fondo_t0 != 0 ? mshell_anim_curva_dolce(mshell_ts.fondo_t0, MSHELL_DURATA_MEDIA, ora) : 1;
		struct wlr_box d = mshell_ts.fondo_da, a = mshell_ts.fondo_a;
		b = (struct wlr_box){ (int)(d.x + (a.x - d.x) * e + 0.5), (int)(d.y + (a.y - d.y) * e + 0.5),
			(int)(d.width + (a.width - d.width) * e + 0.5), (int)(d.height + (a.height - d.height) * e + 0.5) };
		if (memcmp(&meta, &mshell_ts.fondo_a, sizeof(meta)) != 0) {
			mshell_ts.fondo_da = b;
			mshell_ts.fondo_a = meta;
			mshell_ts.fondo_t0 = ora;
			mshell_anim_avvia();
		} else if (e >= 1) {
			mshell_ts.fondo_t0 = 0;
		}
	}
	struct wlr_scene_tree *p = wlr_scene_tree_create(mshell_sc.colonna);
	mshell_fondo_scelta(p, b.x, b.y, b.width, b.height, mshell_ts.aperto != NULL && mshell_ts.sel_f >= 0 ?
		8 /* come le miniature delle disposizioni (MSHELL_SCELTA_RAGGIO) */ : mshell_sc.raggio_pannello);
	wlr_scene_node_lower_to_bottom(&p->node);  /* dietro le miniature */
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
		if (server->gruppo[i] == mshell_voli.atterra) {
			continue;  /* sta atterrando in volo: il suo riquadro si disegna all'arrivo */
		}
		mshell_riquadro(server, server->gruppo[i], schermo.x + MSHELL_COLONNA_X,
			mshell_colonna_y_vista(server, i));  /* a meta' strada, se sta scorrendo */
	}
	mshell_spaccato_disegna(server);  /* scrivania-tasti.c */
	mshell_fondo_disegna(server);
	mshell_colonna_mostra(server);
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
			mshell_riquadro_largo(server->gruppo[i]), mshell_riquadro_alto(server->gruppo[i]) };
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
	mshell_anim_conosci(server);  /* le animazioni della scrivania partono anche senza app */
	if (!wl_list_empty(&fondo->children)) {
		struct wlr_scene_node *n = wl_container_of(fondo->children.next, n, link);
		if (n->type == WLR_SCENE_NODE_BUFFER && wlr_scene_buffer_from_node(n)->buffer != NULL) {
			mshell_sc.sfondo = wlr_buffer_lock(wlr_scene_buffer_from_node(n)->buffer);
		}
	}
	wl_list_init(&mshell_sc.icone);
	server->albero_gruppi = wlr_scene_tree_create(&server->scene->tree);
	mshell_voli.albero = wlr_scene_tree_create(&server->scene->tree);  /* i voli, sopra le finestre */
	mshell_sc.colonna = wlr_scene_tree_create(&server->scene->tree);
	mshell_sc.col_vista = true;  /* all'inizio si vede (il centro e' vuoto) */
	wlr_scene_node_place_below(&mshell_sc.colonna->node, &server->albero_gruppi->node);
	mshell_sc.controlli = wlr_scene_tree_create(&server->scene->tree);
	mshell_controlli_avvia();
	/* i colori e le misure del disegno (token.c, nel tema del sistema) */
	mshell_token_colore("testo-primario", mshell_sc.colore_testo);
	mshell_token_colore("superficie-elevata", mshell_sc.colore_elevata);
	mshell_token_colore("superficie-vetro", mshell_sc.colore_vetro);
	mshell_token_colore("bordo-focus", mshell_sc.colore_focus);
	mshell_token_colore("controllo-selezione-vetro", mshell_sc.colore_selezione);
	mshell_token_colore("azione-sfondo", mshell_sc.colore_azione);
	mshell_token_colore("azione-testo", mshell_sc.colore_azione_testo);
	mshell_sc.raggio_pannello = mshell_token_numero("raggio-pannello");
}

#include "scrivania-voli.c"  /* il passaggio da un gruppo all'altro */
#include "scrivania-controlli.c"  /* i controlli sulla finestra, Disponi, il trascinamento */
#include "scrivania-tasti.c"  /* la tastiera e lo split di un gruppo */
