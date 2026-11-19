/* MSHELL: GLI EFFETTI (angoli arrotondati, vetro sfocato, ombre), con SceneFX. Questo file non si
 * compila da solo: tinywl.c lo include subito prima di animazioni.c, e lo usano le app (tinywl.c),
 * la cornice e i selettori (selettori.c) e i livelli (livelli.c).
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
 *   raggio/controllo 6   le miniature dei selettori
 *   il vetro: sfocatura dello sfondo 14 (BACKGROUND_BLUR di Figma)
 *   ombra/sollevato      le miniature dei desktop; ombra/sovrapposto: il pannello delle disposizioni
 *
 * IL VETRO DEI LIVELLI. Il compositore sfoca quello che sta dietro le superfici dei livelli
 * (layer-shell) che si presentano con uno dei nomi (namespace) di mshell_vetro_nomi: la barra di
 * stato e i suoi menu. Il programma il vetro lo chiede cosi', con il suo nome, senza protocolli
 * nuovi. Si sfoca solo dietro i pixel non del tutto trasparenti: la barra e i menu sono finestre
 * trasparenti con sopra il colore superficie/vetro, quindi il vetro ha la loro forma.
 * IL LIMITE di questo modo: anche l'ombra che GTK disegna intorno al pannello di un menu e' fatta
 * di pixel non del tutto trasparenti, quindi dietro l'ombra si sfoca anche (un alone sfocato
 * intorno al pannello, che copre anche l'orologio della barra). La barra invece non ha un fondo
 * di vetro: si sfoca solo dietro le sue scritte e icone, e non si vede. Come sistemarlo e' da
 * decidere con la persona. */
#include <scenefx/types/fx/corner_location.h>
#include <scenefx/types/fx/clipped_region.h>

#define MSHELL_RAGGIO_FINESTRA 12  /* raggio/finestra */
#define MSHELL_RAGGIO_CONTROLLO 6  /* raggio/controllo */

/* LA SFOCATURA del vetro. SceneFX sfoca a passate (Kawase doppio): con 2 passate di raggio 4
 * l'effetto e' vicino alla sfocatura 14 di Figma. Luminosita', contrasto e saturazione restano
 * quelli veri (1) e niente grana: Figma non li cambia. */
#define MSHELL_VETRO_PASSATE 2
#define MSHELL_VETRO_RAGGIO 4

/* I nomi delle superfici dei livelli che hanno il vetro dietro (vedi in cima). */
static const char *const mshell_vetro_nomi[] = { "mshell-barra", "mshell-barra-menu", NULL };

/* Una delle ombre del disegno, come la scrive Figma (DROP_SHADOW): colore (dritto, non
 * moltiplicato per l'opacita'), spostamento in giu', sfocatura ("radius" di Figma) e allargamento
 * ("spread"). SceneFX fa un'ombra sola per nodo: di ogni ombra del disegno si prende lo strato
 * piu' largo, quello che si vede. */
struct mshell_ombra {
	float colore[4];
	int y, sfocatura, allarga;
};
/* ombra/sollevato, strato 4: #14141a0f, y 6, raggio 12, spread -4 */
static const struct mshell_ombra mshell_ombra_sollevato = {
	{ 0x14 / 255.0f, 0x14 / 255.0f, 0x1a / 255.0f, 0x0f / 255.0f }, 6, 12, -4 };
/* ombra/sovrapposto, strato 4: #14141a24, y 20, raggio 40, spread -12 */
static const struct mshell_ombra mshell_ombra_sovrapposto = {
	{ 0x14 / 255.0f, 0x14 / 255.0f, 0x1a / 255.0f, 0x24 / 255.0f }, 20, 40, -12 };

/* All'avvio, dopo la scena: i valori della sfocatura del vetro (uguali per tutto). */
static void mshell_effetti_avvia(struct wlr_scene *scena) {
	struct blur_data vetro = {
		.num_passes = MSHELL_VETRO_PASSATE,
		.radius = MSHELL_VETRO_RAGGIO,
		.noise = 0.0f,
		.brightness = 1.0f,
		.contrast = 1.0f,
		.saturation = 1.0f,
	};
	wlr_scene_set_blur_data(scena, vetro);
}

/* L'ombra del riquadro r (coordinate di "padre"), con gli angoli di raggio "raggio" come il
 * riquadro. Sotto il riquadro l'ombra non si disegna (come in Figma, dove l'ombra non si vede
 * attraverso un riempimento trasparente come il vetro). Va creata PRIMA del riquadro, cosi' sta
 * sotto. */
static void mshell_effetti_ombra(struct wlr_scene_tree *padre, struct wlr_box r, int raggio,
		const struct mshell_ombra *o) {
	/* la forma che fa ombra: il riquadro allargato e spostato; il nodo e' piu' grande di lei di
	 * "sfocatura" per lato (in SceneFX la sfumatura sta in quella striscia) */
	int s = o->sfocatura;
	struct wlr_box forma = {
		.x = r.x - o->allarga, .y = r.y - o->allarga + o->y,
		.width = r.width + 2 * o->allarga, .height = r.height + 2 * o->allarga,
	};
	if (forma.width <= 0 || forma.height <= 0) {
		return;
	}
	int raggio_forma = raggio + o->allarga > 0 ? raggio + o->allarga : 0;
	struct wlr_scene_shadow *ombra = wlr_scene_shadow_create(padre,
		forma.width + 2 * s, forma.height + 2 * s, raggio_forma, (float)s, o->colore);
	if (ombra == NULL) {
		return;
	}
	wlr_scene_node_set_position(&ombra->node, forma.x - s, forma.y - s);
	wlr_scene_shadow_set_clipped_region(ombra, (struct clipped_region){
		.area = { r.x - (forma.x - s), r.y - (forma.y - s), r.width, r.height },
		.corner_radius = raggio,
		.corners = CORNER_LOCATION_ALL,
	});
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

static void mshell_effetti_app(struct wlr_scene_tree *albero, struct wlr_xdg_surface *xdg) {
	if (wl_list_empty(&albero->children)) {
		return;
	}
	struct wlr_scene_node *superficie = wl_container_of(albero->children.next, superficie, link);
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(xdg, &geo);
	wlr_scene_subsurface_tree_set_clip(superficie, &geo);
	wlr_scene_node_for_each_buffer(superficie, mshell_effetti_angolo_una, xdg->surface);
}

/* IL VETRO DEI LIVELLI: se il nome della superficie e' fra quelli col vetro, dietro alle sue
 * immagini (e a quelle dei suoi menu a comparsa) si sfoca. Si chiama a ogni invio. */
static void mshell_effetti_vetro_una(struct wlr_scene_buffer *b, int sx, int sy, void *data) {
	wlr_scene_buffer_set_backdrop_blur(b, true);
	wlr_scene_buffer_set_backdrop_blur_optimized(b, false);  /* vero: sfoca anche le app sotto */
	wlr_scene_buffer_set_backdrop_blur_ignore_transparent(b, true);
}

static void mshell_effetti_livello(struct wlr_scene_tree *albero, const char *nome) {
	if (nome == NULL) {
		return;
	}
	for (int i = 0; mshell_vetro_nomi[i] != NULL; i++) {
		if (strcmp(nome, mshell_vetro_nomi[i]) == 0) {
			wlr_scene_node_for_each_buffer(&albero->node, mshell_effetti_vetro_una, NULL);
			return;
		}
	}
}
