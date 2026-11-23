/* MSHELL: GLI EFFETTI (angoli arrotondati, vetro sfocato, ombre), con SceneFX. Questo file non si
 * compila da solo: tinywl.c lo include subito prima di animazioni.c, e lo usano le app (tinywl.c),
 * la scrivania (scrivania.c) e i livelli (livelli.c).
 *
 * SCENEFX (decisione della persona, 19/11/2026; https://github.com/wlrfx/scenefx, versione 0.2.1,
 * la serie per wlroots 0.18): prende il posto della scena di wlroots. Ha le stesse funzioni
 * (wlr_scene_...), con in piu' gli effetti, e disegna con OpenGL ES 2 (fx_renderer_create, in
 * main). Si compila nell'immagine degli attrezzi (strumenti/Dockerfile) ed entra tutta dentro
 * mshell-compositore (collegata "statica"). Le funzioni della scena che SceneFX non riscrive (le
 * superfici, le app, i livelli: wlr_scene_xdg_surface_create e simili) restano quelle di wlroots,
 * che per creare i nodi chiamano le funzioni di SceneFX: per questo il compositore le espone a
 * wlroots (costruisci.sh, -Wl,--export-dynamic).
 * Nel disegno con il processore (QEMU senza scheda video vera) OpenGL lo fa Mesa con llvmpipe:
 * per questo greetd lancia il compositore con WLR_RENDERER_ALLOW_SOFTWARE=1 (costruisci.sh).
 *
 * LE MISURE vengono dal disegno in Figma ("mshell – interfaccia", pagina "Navigazione", sezione
 * "Navigazione – chiaro e scuro"; variabili della collezione "Misure"):
 *   raggio/finestra 12   le app, il pannello delle disposizioni, i menu della barra
 *   raggio/controllo 6   i controlli
 *   il vetro: sfocatura dello sfondo 14 (BACKGROUND_BLUR di Figma)
 *   ombra/sollevato      le miniature dei gruppi (scrivania.c); ombra/sovrapposto: i menu
 *
 * IL VETRO DEI LIVELLI. Il compositore sfoca quello che sta dietro le superfici dei livelli
 * (layer-shell) che si presentano con uno dei nomi (namespace) di mshell_vetro_nomi, e dietro i
 * loro menu a comparsa. Il programma il vetro lo chiede cosi', con il suo nome, senza protocolli
 * nuovi. Si sfoca solo dietro i pixel non del tutto trasparenti. La barra di stato non ha un fondo:
 * dietro di lei si sfoca solo sotto le scritte e le icone, e non si vede.
 *
 * I MENU DEI LIVELLI (decisione della persona, 19/11/2026: le decorazioni le disegna il
 * compositore, non il singolo programma: ombre, angoli arrotondati, vetro). I menu a comparsa
 * (xdg_popup) delle superfici con uno dei nomi di mshell_menu_nomi (i menu della barra: calendario,
 * notifiche, comandi rapidi) sono pannelli: il compositore da' loro gli angoli (raggio/finestra) e
 * l'ombra/sovrapposto, oltre al vetro. Il programma disegna solo il riempimento (superficie/vetro):
 * niente ombra e niente angoli suoi. Un'ombra disegnata dal programma starebbe dentro la
 * superficie, quindi sopra il vetro: dietro di lei si sfocherebbe anche, e l'alone coprirebbe la
 * barra. */
#include <scenefx/types/fx/corner_location.h>
#include <scenefx/types/fx/clipped_region.h>

#define MSHELL_RAGGIO_FINESTRA mshell_t.raggio_finestra    /* raggio/finestra (token.c) */
#define MSHELL_RAGGIO_CONTROLLO mshell_t.raggio_controllo  /* raggio/controllo (token.c) */

/* LA SFOCATURA del vetro. SceneFX sfoca a passate (Kawase doppio): con 2 passate di raggio 4
 * l'effetto e' vicino alla sfocatura 14 di Figma. Luminosita', contrasto e saturazione restano
 * quelli veri (1) e niente grana: Figma non li cambia. */
#define MSHELL_VETRO_PASSATE 2
#define MSHELL_VETRO_RAGGIO 4

/* I nomi delle superfici dei livelli che hanno il vetro dietro (vedi in cima). */
static const char *const mshell_vetro_nomi[] = { "mshell-barra", "mshell-barra-cassetto", NULL };
/* I nomi delle superfici dei livelli i cui menu a comparsa sono pannelli (vedi in cima). */
static const char *const mshell_menu_nomi[] = { "mshell-barra", NULL };

/* Uno strato di un'ombra del disegno, come lo scrive Figma (DROP_SHADOW): colore (dritto, non
 * moltiplicato per l'opacita'), spostamento in giu', sfocatura ("radius" di Figma) e allargamento
 * ("spread"). SceneFX fa uno strato solo per nodo: un'ombra a piu' strati vuole piu' nodi. */
struct mshell_ombra {
	float colore[4];
	int y, sfocatura, allarga;
};
/* Le ombre vengono dai token (token.c), lette in mshell_effetti_avvia:
 *   ombra/sollevato, solo lo strato 4 (il piu' largo, quello che si vede);
 *   ombra/sovrapposto, gli strati 2, 3 e 4 (il primo di Figma e' un'ombra interna del tutto
 *   trasparente). Sono quelle decise dalla persona (10%, 25%, 40% di nero): in token.css ci sono
 *   anche quelle di Figma. */
static struct mshell_ombra mshell_ombra_sollevato;
#define MSHELL_STRATI_SOVRAPPOSTO 3
static struct mshell_ombra mshell_ombre_sovrapposto[MSHELL_STRATI_SOVRAPPOSTO];
/* ombra/finestra (le finestre e le loro miniature, il disegno dello Stage Manager): gli strati 2, 3
 * e 4 (il primo e' un'ombra interna quasi trasparente) */
#define MSHELL_STRATI_FINESTRA 3
static struct mshell_ombra mshell_ombre_finestra[MSHELL_STRATI_FINESTRA];

/* Lo strato "strato" dell'ombra "nome" (per esempio "sovrapposto", 3) dai token. */
static void mshell_effetti_ombra_leggi(const char *nome, int strato, struct mshell_ombra *o) {
	char t[96];
	snprintf(t, sizeof(t), "effetto-ombra-%s-%d-colore", nome, strato);
	mshell_token_colore(t, o->colore);
	snprintf(t, sizeof(t), "effetto-ombra-%s-%d-y", nome, strato);
	o->y = mshell_token_numero(t);
	snprintf(t, sizeof(t), "effetto-ombra-%s-%d-raggio", nome, strato);
	o->sfocatura = mshell_token_numero(t);
	snprintf(t, sizeof(t), "effetto-ombra-%s-%d-allarga", nome, strato);
	o->allarga = mshell_token_numero(t);
}
/* uno strato solo, il piu' largo, per chi ne vuole uno */
#define mshell_ombra_sovrapposto (mshell_ombre_sovrapposto[2])

/* La sfocatura del vetro (uguale per tutto), a "forza" da 0 a 1: all'avvio 1; la comparsa del
 * cassetto la fa crescere insieme alla sua opacita' (livelli.c), perche' la sfocatura non segue
 * l'opacita' delle immagini e comparirebbe di colpo. */
static void mshell_effetti_vetro_forza(struct wlr_scene *scena, float forza) {
	int raggio = (int)(MSHELL_VETRO_RAGGIO * forza + 0.5f);
	struct blur_data vetro = {
		.num_passes = raggio > 0 ? MSHELL_VETRO_PASSATE : 0,
		.radius = raggio,
		.noise = 0.0f,
		.brightness = 1.0f,
		.contrast = 1.0f,
		.saturation = 1.0f,
	};
	wlr_scene_set_blur_data(scena, vetro);
}

/* All'avvio, dopo la scena: i valori della sfocatura del vetro (uguali per tutto). */
static void mshell_effetti_avvia(struct wlr_scene *scena) {
	mshell_effetti_ombra_leggi("sollevato", 4, &mshell_ombra_sollevato);
	for (int i = 0; i < MSHELL_STRATI_SOVRAPPOSTO; i++) {
		mshell_effetti_ombra_leggi("sovrapposto", 2 + i, &mshell_ombre_sovrapposto[i]);
	}
	for (int i = 0; i < MSHELL_STRATI_FINESTRA; i++) {
		mshell_effetti_ombra_leggi("finestra", 2 + i, &mshell_ombre_finestra[i]);
	}
	mshell_effetti_vetro_forza(scena, 1.0f);
}


/* Mette lo strato d'ombra "ombra" sotto il riquadro r (coordinate del padre dell'ombra), con gli
 * angoli di raggio "raggio" come il riquadro. Sotto il riquadro l'ombra non si disegna (come in
 * Figma, dove l'ombra non si vede attraverso un riempimento trasparente come il vetro). Falso se
 * lo strato non ha forma (il riquadro e' troppo piccolo per l'allargamento). */
static bool mshell_effetti_ombra_metti(struct wlr_scene_shadow *ombra, struct wlr_box r,
		int raggio, const struct mshell_ombra *o) {
	/* la forma che fa ombra: il riquadro allargato e spostato; il nodo e' piu' grande di lei di
	 * "sfocatura" per lato (in SceneFX la sfumatura sta in quella striscia) */
	int s = o->sfocatura;
	struct wlr_box forma = {
		.x = r.x - o->allarga, .y = r.y - o->allarga + o->y,
		.width = r.width + 2 * o->allarga, .height = r.height + 2 * o->allarga,
	};
	if (forma.width <= 0 || forma.height <= 0) {
		return false;
	}
	wlr_scene_shadow_set_size(ombra, forma.width + 2 * s, forma.height + 2 * s);
	wlr_scene_shadow_set_corner_radius(ombra, raggio + o->allarga > 0 ? raggio + o->allarga : 0);
	wlr_scene_node_set_position(&ombra->node, forma.x - s, forma.y - s);
	wlr_scene_shadow_set_clipped_region(ombra, (struct clipped_region){
		.area = { r.x - (forma.x - s), r.y - (forma.y - s), r.width, r.height },
		.corner_radius = raggio,
		.corners = CORNER_LOCATION_ALL,
	});
	return true;
}

/* Uno strato d'ombra nuovo, ancora senza misura (la mette mshell_effetti_ombra_metti). */
static struct wlr_scene_shadow *mshell_effetti_ombra_nuova(struct wlr_scene_tree *padre,
		const struct mshell_ombra *o) {
	return wlr_scene_shadow_create(padre, 0, 0, 0, (float)o->sfocatura, o->colore);
}

/* L'ombra (uno strato) del riquadro r, fatta una volta sola. Va creata PRIMA del riquadro, cosi'
 * sta sotto. */
static void mshell_effetti_ombra(struct wlr_scene_tree *padre, struct wlr_box r, int raggio,
		const struct mshell_ombra *o) {
	struct wlr_scene_shadow *ombra = mshell_effetti_ombra_nuova(padre, o);
	if (ombra != NULL && !mshell_effetti_ombra_metti(ombra, r, raggio, o)) {
		wlr_scene_node_destroy(&ombra->node);
	}
}

/* GLI ANGOLI DELLE APP. Si arrotonda l'immagine della superficie principale dell'app (non i suoi
 * menu, che hanno la loro forma). Prima la si ritaglia alla "geometria" dell'app: alcune app
 * (quelle GTK) disegnano un'ombra intorno alla finestra, e gli angoli vanno sulla finestra vera,
 * non sul bordo dell'ombra. Il ritaglio si mette solo sull'albero della superficie dell'app: e' il
 * primo figlio del suo albero, perche' wlr_scene_xdg_surface_create lo crea per primo; i popup
 * (menu) arrivano dopo, accanto a lui, e cosi' non si ritagliano. Si rifa' a ogni invio dell'app:
 * la geometria puo' cambiare e costa poco (SceneFX non fa niente se il valore e' lo stesso). */
static void mshell_effetti_angolo_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	struct wlr_surface *superficie = data;
	struct wlr_scene_surface *s = wlr_scene_surface_try_from_buffer(b);
	if (s != NULL && s->surface == superficie) {
		wlr_scene_buffer_set_corner_radius(b, MSHELL_RAGGIO_FINESTRA, CORNER_LOCATION_ALL);
	}
}

static void mshell_effetti_angoli(struct wlr_scene_node *superficie, struct wlr_xdg_surface *xdg) {
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(xdg, &geo);
	wlr_scene_subsurface_tree_set_clip(superficie, &geo);
	wlr_scene_node_for_each_buffer(superficie, mshell_effetti_angolo_una, xdg->surface);
}

/* Gli strati di un'ombra fatta di piu' nodi (le finestre, i menu dei livelli), che la seguono. */
struct mshell_ombre_menu {
	struct wlr_scene_shadow *strato[MSHELL_STRATI_SOVRAPPOSTO];
};

static bool mshell_effetti_e_ombra(struct wlr_scene_node *n, const struct mshell_ombre_menu *ombre) {
	for (int i = 0; i < MSHELL_STRATI_SOVRAPPOSTO; i++) {
		if (ombre->strato[i] != NULL && n == &ombre->strato[i]->node) {
			return true;
		}
	}
	return false;
}

/* L'ombra a strati "o" (n strati) sotto il riquadro r dell'albero, nei nodi "ombre" (fatti la
 * prima volta, in fondo all'albero: lo seguono e se ne vanno con lui). */
static void mshell_effetti_ombre_metti(struct wlr_scene_tree *albero, struct mshell_ombre_menu *ombre,
		struct wlr_box r, int raggio, const struct mshell_ombra *o, int n) {
	for (int i = 0; i < n; i++) {
		if (ombre->strato[i] == NULL) {
			ombre->strato[i] = mshell_effetti_ombra_nuova(albero, &o[i]);
			if (ombre->strato[i] == NULL) {
				continue;
			}
			wlr_scene_node_lower_to_bottom(&ombre->strato[i]->node);
		}
		wlr_scene_node_set_enabled(&ombre->strato[i]->node,
			mshell_effetti_ombra_metti(ombre->strato[i], r, raggio, &o[i]));
	}
}

/* Gli angoli della finestra e la sua ombra/finestra (il disegno dello Stage Manager), sotto la
 * finestra vera (la geometria). Si rifa' a ogni invio dell'app. */
static void mshell_effetti_app(struct wlr_scene_tree *albero, struct wlr_xdg_surface *xdg,
		struct mshell_ombre_menu *ombre) {
	struct wlr_scene_node *n, *superficie = NULL;
	wl_list_for_each(n, &albero->children, link) {
		if (!mshell_effetti_e_ombra(n, ombre)) {
			superficie = n;  /* la superficie dell'app: il primo figlio che non e' l'ombra */
			break;
		}
	}
	if (superficie == NULL) {
		return;
	}
	mshell_effetti_angoli(superficie, xdg);
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(xdg, &geo);
	mshell_effetti_ombre_metti(albero, ombre, geo, MSHELL_RAGGIO_FINESTRA, mshell_ombre_finestra,
		MSHELL_STRATI_FINESTRA);
}

/* Lo strato o rimpicciolito di "scala" (l'ombra delle miniature, come in Figma). */
static struct mshell_ombra mshell_effetti_ombra_scala(const struct mshell_ombra *o, double scala) {
	struct mshell_ombra s = *o;
	s.y = (int)(o->y * scala + 0.5);
	s.sfocatura = (int)(o->sfocatura * scala + 0.5);
	s.allarga = (int)(o->allarga * scala + (o->allarga < 0 ? -0.5 : 0.5));
	return s;
}

static bool mshell_effetti_nome_fra(const char *nome, const char *const *nomi) {
	for (int i = 0; nome != NULL && nomi[i] != NULL; i++) {
		if (strcmp(nome, nomi[i]) == 0) {
			return true;
		}
	}
	return false;
}

/* IL VETRO DEI LIVELLI (vedi in cima): se il nome della superficie e' fra quelli col vetro, dietro
 * alle sue immagini (e a quelle dei suoi menu a comparsa) si sfoca. Si chiama a ogni invio. */
static void mshell_effetti_vetro_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	wlr_scene_buffer_set_backdrop_blur(b, true);
	/* la sfocatura conservata (il nodo "sfocato" di tinywl.c): sfondo e app sotto, sfocati una
	 * volta sola; prima si rifaceva in ogni pezzo ridisegnato e si vedevano dei quadratini */
	wlr_scene_buffer_set_backdrop_blur_optimized(b, true);
	wlr_scene_buffer_set_backdrop_blur_ignore_transparent(b, true);
}

static void mshell_effetti_livello(struct wlr_scene_tree *albero, const char *nome) {
	if (mshell_effetti_nome_fra(nome, mshell_vetro_nomi)) {
		wlr_scene_node_for_each_buffer(&albero->node, mshell_effetti_vetro_una, NULL);
	}
}

/* I MENU DEI LIVELLI (vedi in cima): se la superficie genitore ha uno dei nomi di
 * mshell_menu_nomi, il menu a comparsa "xdg" (albero: il suo albero della scena) prende gli angoli
 * e l'ombra/sovrapposto. Si chiama a ogni invio del menu. Gli strati dell'ombra ("ombre", del
 * menu) nascono qui la prima volta, nell'albero del menu e sotto di lui: cosi' lo seguono, si
 * nascondono con lui e se ne vanno quando si distrugge (con il suo albero). */
static void mshell_effetti_menu(struct wlr_scene_tree *albero, struct wlr_xdg_surface *xdg,
		const char *nome_genitore, struct mshell_ombre_menu *ombre) {
	if (!mshell_effetti_nome_fra(nome_genitore, mshell_menu_nomi)) {
		return;
	}
	/* gli angoli, sulla "geometria" del menu, come le app. La superficie e' il primo figlio
	 * dell'albero che non e' uno strato dell'ombra (gli strati stanno sotto di lei). */
	struct wlr_scene_node *n;
	wl_list_for_each(n, &albero->children, link) {
		if (!mshell_effetti_e_ombra(n, ombre)) {
			mshell_effetti_angoli(n, xdg);
			break;
		}
	}
	/* l'ombra: tre strati sotto il menu, il piu' largo in fondo. Nell'albero del menu la sua
	 * geometria comincia in 0, 0. */
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(xdg, &geo);
	struct wlr_box r = { 0, 0, geo.width, geo.height };
	for (int i = 0; i < MSHELL_STRATI_SOVRAPPOSTO; i++) {
		const struct mshell_ombra *o = &mshell_ombre_sovrapposto[i];
		if (ombre->strato[i] == NULL) {
			ombre->strato[i] = mshell_effetti_ombra_nuova(albero, o);
			if (ombre->strato[i] == NULL) {
				continue;
			}
			wlr_scene_node_lower_to_bottom(&ombre->strato[i]->node);
		}
		wlr_scene_node_set_enabled(&ombre->strato[i]->node,
			mshell_effetti_ombra_metti(ombre->strato[i], r, MSHELL_RAGGIO_FINESTRA, o));
	}
}
