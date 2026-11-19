/* mshell-barra: LA BARRA DI STATO E I SUOI MENU (decisione della persona: un programma a parte,
 * in C con GTK 4 e gtk4-layer-shell). La fa partire il compositore (mshell-compositore -s).
 *
 * LA BARRA e' una striscia trasparente alta 32 px in cima allo schermo, su tutta la larghezza,
 * nei 32 px di bordo che le app lasciano liberi. Sta nel livello "top" del compositore (sopra le
 * app) e non chiede spazio alle app (zona esclusiva 0). A sinistra il titolo dell'app col focus
 * (lo dice il compositore, elenco-app.c); a destra giorno e ora, la campanella e il menu: sono
 * pulsanti, e ognuno apre il suo menu a tendina (calendario, notifiche, comandi rapidi).
 *
 * I MENU: ogni pannello (calendario, notifiche, comandi rapidi) e' una superficie a parte, nel
 * livello "overlay", grande come il pannello, sotto la barra e allineata a destra con le app.
 * GTK ci disegna solo il contenuto su fondo di vetro: gli angoli arrotondati, la sfocatura dietro
 * e l'ombra li mette il compositore (decisione della persona: le decorazioni sono del
 * compositore), che le riconosce dal nome "mshell-barra-menu" (utente/compositore/effetti.c).
 * La superficie del pannello chiede la tastiera in esclusiva, quindi riceve Esc (che chiude il
 * menu); quando sparisce il compositore ridà la tastiera all'app.
 * PER CHIUDERE CON UN CLIC FUORI, mentre un menu e' aperto c'e' anche una superficie "dei clic":
 * trasparente, grande come lo schermo, nello stesso livello ma sotto il pannello (compare prima
 * di lui, e il compositore mette sopra l'ultima arrivata). Non disegna niente e il compositore non
 * la decora. Cosi':
 *   - un clic fuori dal pannello cade su di lei e chiude il menu (senza arrivare all'app sotto);
 *   - la fascia della barra (i 32 px in alto) e' esclusa dalla sua zona dei clic: un clic li'
 *     arriva alla barra, quindi un clic su un altro pulsante passa all'altro menu e un clic sullo
 *     stesso pulsante lo chiude.
 *
 * L'ASPETTO viene solo da stile.css (in /usr/share/mshell/barra/), un foglio di stile nostro che
 * parte da zero: prima azzera tutto quello che darebbe il tema di GTK, poi mette le nostre regole
 * con i nomi dei valori del disegno. I menu seguono il tema del sistema (per ora la variabile
 * d'ambiente MSHELL_TEMA=chiaro|scuro, se manca scuro); la barra sta sopra la foto dello sfondo e
 * usa sempre i colori scuri.
 *
 * IL CASSETTO DELLE APP (cassetto.c) sta in questo stesso programma, in una terza superficie:
 * cosi' usa lo stesso stile, gli stessi pannelli e la stessa connessione al compositore (l'elenco
 * delle app, per attivare quelle gia' aperte). Super+spazio lo prende il compositore, che lo dice
 * a questo programma con il segnale SIGUSR1 ("apri o chiudi il cassetto"): e' il modo piu'
 * semplice, perche' il compositore sa gia' quale programma e' la barra (quello delle superfici
 * "mshell-barra...") e non serve un canale apposta. Mentre il cassetto e' aperto la barra non si
 * vede (nel disegno non c'e': l'ora grande, i comandi rapidi e le notifiche sono nel cassetto). */
#include <glib-unix.h>
#include <signal.h>
#include <gtk4-layer-shell.h>
#include "barra.h"

#define STILE "/usr/share/mshell/barra/stile.css"

enum menu { NESSUNO = -1, CALENDARIO, NOTIFICHE, COMANDI, MENU_QUANTI };

static GtkWidget *finestra_clic;          /* la superficie dei clic fuori dal menu */
static GtkWidget *finestra_menu[MENU_QUANTI];  /* le superfici dei menu, una per pannello */
static GtkWidget *pulsante[MENU_QUANTI];  /* i pulsanti della barra, uno per menu */
static GtkWidget *pannello[MENU_QUANTI];  /* i pannelli dei menu */
static enum menu aperto = NESSUNO;
static GtkWidget *titolo_app;             /* il pulsante col titolo dell'app col focus */
static GtkWidget *orologio;               /* la scritta con giorno e ora */
static GtkWidget *barra;                  /* la superficie della barra */

/* Apre il menu "quale" (NESSUNO: chiude). */
static void apri(enum menu quale) {
	if (aperto != NESSUNO) {
		gtk_widget_remove_css_class(pulsante[aperto], "aperto");
		gtk_widget_set_visible(finestra_menu[aperto], FALSE);
	}
	aperto = quale;
	if (quale == NESSUNO) {
		gtk_widget_set_visible(finestra_clic, FALSE);
		return;
	}
	if (quale == CALENDARIO) {
		/* ogni volta che si apre, il calendario riparte dal mese di oggi */
		GDateTime *adesso = g_date_time_new_now_local();
		mshell_calendario_aggiorna(pannello[CALENDARIO], adesso, TRUE);
		g_date_time_unref(adesso);
	}
	gtk_widget_add_css_class(pulsante[quale], "aperto");
	/* prima la superficie dei clic (se non c'e' gia'), poi il pannello, che cosi' le sta sopra */
	gtk_widget_set_visible(finestra_clic, TRUE);
	gtk_widget_set_visible(finestra_menu[quale], TRUE);
}

static void pulsante_premuto(GtkButton *b, gpointer dati) {
	enum menu quale = GPOINTER_TO_INT(dati);
	apri(aperto == quale ? NESSUNO : quale);
}

/* Un clic sulla barra fuori dai pulsanti chiude il menu aperto. */
static void clic_barra(GtkGestureClick *g, int n, double x, double y, gpointer dati) {
	GtkWidget *radice = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
	GtkWidget *sotto = gtk_widget_pick(radice, x, y, GTK_PICK_DEFAULT);
	if (aperto != NESSUNO && (sotto == NULL || gtk_widget_get_ancestor(sotto, GTK_TYPE_BUTTON) == NULL)) {
		apri(NESSUNO);
	}
}

/* Un clic sulla superficie dei clic (cioe' fuori dal pannello) chiude il menu. */
static void clic_fuori(GtkGestureClick *g, int n, double x, double y, gpointer dati) {
	apri(NESSUNO);
}

static gboolean tasto_menu(GtkEventControllerKey *k, guint tasto, guint codice,
		GdkModifierType mod, gpointer dati) {
	if (tasto == GDK_KEY_Escape) {
		apri(NESSUNO);
		return TRUE;
	}
	return FALSE;
}

/* La fascia della barra non e' della superficie dei clic: i clic li' vanno alla barra.
 * (La zona dei clic si da' al compositore ogni volta che la superficie compare.) */
static void clic_compare(GtkWidget *w, gpointer dati) {
	cairo_rectangle_int_t sotto_la_barra = { 0, 32, G_MAXINT16, G_MAXINT16 };
	cairo_region_t *zona = cairo_region_create_rectangle(&sotto_la_barra);
	gdk_surface_set_input_region(gtk_native_get_surface(GTK_NATIVE(w)), zona);
	cairo_region_destroy(zona);
}

/* Giorno e ora nella barra (gio 19 nov  10:00) e nel calendario; si rifanno allo scoccare di
 * ogni minuto. */
static gboolean scocca_minuto(gpointer dati) {
	GDateTime *adesso = g_date_time_new_now_local();
	char testo[64];
	g_snprintf(testo, sizeof(testo), "%s %d %s  %02d:%02d",
		mshell_giorni_corti[mshell_giorno_settimana(adesso)],
		g_date_time_get_day_of_month(adesso), mshell_mesi_corti[g_date_time_get_month(adesso) - 1],
		g_date_time_get_hour(adesso), g_date_time_get_minute(adesso));
	gtk_label_set_text(GTK_LABEL(orologio), testo);
	mshell_calendario_aggiorna(pannello[CALENDARIO], adesso, FALSE);
	mshell_cassetto_minuto(adesso);
	/* il prossimo giro allo scoccare del minuto dopo (+ un attimo, per non arrivare prima) */
	int ms = (60 - g_date_time_get_second(adesso)) * 1000 -
		g_date_time_get_microsecond(adesso) / 1000 + 50;
	g_date_time_unref(adesso);
	g_timeout_add(ms, scocca_minuto, NULL);
	return G_SOURCE_REMOVE;
}

/* Il cassetto si apre (la barra e un menu aperto spariscono) o si chiude (la barra torna). */
static void cassetto_cambiato(gboolean aperto) {
	if (aperto) {
		apri(NESSUNO);
	}
	gtk_widget_set_visible(barra, !aperto);
}

/* Super+spazio, detto dal compositore. */
static gboolean segnale_cassetto(gpointer dati) {
	mshell_cassetto_apri_chiudi();
	return G_SOURCE_CONTINUE;
}

static void titolo_cambiato(const char *titolo) {
	GtkWidget *scritta = gtk_widget_get_first_child(titolo_app);
	gtk_label_set_text(GTK_LABEL(scritta), titolo);
	gtk_widget_set_visible(titolo_app, titolo[0] != '\0');
}

static GtkWidget *nuovo_pulsante(GtkWidget *contenuto, enum menu quale) {
	GtkWidget *b = gtk_button_new();
	gtk_button_set_child(GTK_BUTTON(b), contenuto);
	gtk_widget_add_css_class(b, "pulsante");
	g_signal_connect(b, "clicked", G_CALLBACK(pulsante_premuto), GINT_TO_POINTER(quale));
	pulsante[quale] = b;
	return b;
}

static GtkWidget *crea_barra(void) {
	GtkWidget *w = gtk_window_new();
	gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
	gtk_widget_add_css_class(w, "barra");
	gtk_widget_add_css_class(w, "scuro");  /* sopra la foto: sempre i colori scuri */
	gtk_layer_init_for_window(GTK_WINDOW(w));
	gtk_layer_set_namespace(GTK_WINDOW(w), "mshell-barra");
	gtk_layer_set_layer(GTK_WINDOW(w), GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_anchor(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(w), 0);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(w), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

	GtkWidget *riga = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(riga, "riga-barra");

	/* il titolo dell'app col focus: ha l'aspetto di un pulsante, ma per ora non apre niente */
	titolo_app = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(titolo_app, "pulsante");
	GtkWidget *scritta = gtk_label_new("");
	gtk_widget_add_css_class(scritta, "etichetta");
	gtk_widget_add_css_class(scritta, "primario");
	gtk_label_set_ellipsize(GTK_LABEL(scritta), PANGO_ELLIPSIZE_END);
	gtk_box_append(GTK_BOX(titolo_app), scritta);
	gtk_widget_set_visible(titolo_app, FALSE);
	gtk_widget_set_valign(titolo_app, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(riga), titolo_app);

	GtkWidget *spazio = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(spazio, TRUE);
	gtk_box_append(GTK_BOX(riga), spazio);

	orologio = gtk_label_new("");
	gtk_widget_add_css_class(orologio, "etichetta");
	gtk_widget_add_css_class(orologio, "primario");
	gtk_box_append(GTK_BOX(riga), nuovo_pulsante(orologio, CALENDARIO));
	gtk_box_append(GTK_BOX(riga), nuovo_pulsante(mshell_icona_nuova(ICONA_CAMPANELLA), NOTIFICHE));
	gtk_box_append(GTK_BOX(riga), nuovo_pulsante(mshell_icona_nuova(ICONA_MENU), COMANDI));
	for (int i = 0; i < MENU_QUANTI; i++) {
		gtk_widget_set_valign(pulsante[i], GTK_ALIGN_CENTER);
	}

	GtkGesture *clic = gtk_gesture_click_new();
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(clic), GTK_PHASE_CAPTURE);
	g_signal_connect(clic, "pressed", G_CALLBACK(clic_barra), NULL);
	gtk_widget_add_controller(riga, GTK_EVENT_CONTROLLER(clic));

	gtk_window_set_child(GTK_WINDOW(w), riga);
	return w;
}

/* Una superficie dei livelli sopra le app, "overlay", che non chiede spazio alle app: i suoi
 * bordi si contano da quelli dello schermo, barra compresa. */
static GtkWidget *nuova_superficie(const char *nome, GtkLayerShellKeyboardMode tastiera) {
	GtkWidget *w = gtk_window_new();
	gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
	gtk_layer_init_for_window(GTK_WINDOW(w));
	gtk_layer_set_namespace(GTK_WINDOW(w), nome);
	gtk_layer_set_layer(GTK_WINDOW(w), GTK_LAYER_SHELL_LAYER_OVERLAY);
	gtk_layer_set_exclusive_zone(GTK_WINDOW(w), -1);
	gtk_layer_set_keyboard_mode(GTK_WINDOW(w), tastiera);
	return w;
}

/* La superficie dei clic: tutto lo schermo, trasparente (vedi in cima). */
static GtkWidget *crea_finestra_clic(void) {
	GtkWidget *w = nuova_superficie("mshell-barra-clic", GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
	for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++) {
		gtk_layer_set_anchor(GTK_WINDOW(w), e, TRUE);
	}
	GtkGesture *clic = gtk_gesture_click_new();
	g_signal_connect(clic, "pressed", G_CALLBACK(clic_fuori), NULL);
	gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(clic));
	g_signal_connect(w, "map", G_CALLBACK(clic_compare), NULL);
	return w;
}

/* La superficie di un menu: grande come il suo pannello, in alto a destra, sotto la barra (32 px)
 * e con il bordo destro su quello delle app (32 px dal bordo dello schermo). */
static GtkWidget *crea_finestra_menu(const char *tema, GtkWidget *contenuto) {
	GtkWidget *w = nuova_superficie("mshell-barra-menu", GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
	gtk_widget_add_css_class(w, "menu");
	gtk_widget_add_css_class(w, tema);
	gtk_layer_set_anchor(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_anchor(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
	gtk_layer_set_margin(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_TOP, 32);
	gtk_layer_set_margin(GTK_WINDOW(w), GTK_LAYER_SHELL_EDGE_RIGHT, 32);
	gtk_window_set_child(GTK_WINDOW(w), contenuto);
	GtkEventController *tasti = gtk_event_controller_key_new();
	g_signal_connect(tasti, "key-pressed", G_CALLBACK(tasto_menu), NULL);
	gtk_widget_add_controller(w, tasti);
	return w;
}

static void crea_menu(const char *tema) {
	finestra_clic = crea_finestra_clic();
	pannello[CALENDARIO] = mshell_calendario_nuovo();
	pannello[NOTIFICHE] = mshell_notifiche_nuove();
	pannello[COMANDI] = mshell_comandi_nuovi();
	for (int i = 0; i < MENU_QUANTI; i++) {
		finestra_menu[i] = crea_finestra_menu(tema, pannello[i]);
	}
}

int main(int argc, char *argv[]) {
	gtk_init();
	if (!gtk_layer_is_supported()) {
		g_printerr("mshell-barra: il compositore non ha i livelli (layer-shell)\n");
		return 1;
	}
	/* il nostro foglio di stile, sopra a quello del tema di GTK (che azzera) */
	GtkCssProvider *stile = gtk_css_provider_new();
	gtk_css_provider_load_from_path(stile, STILE);
	gtk_style_context_add_provider_for_display(gdk_display_get_default(),
		GTK_STYLE_PROVIDER(stile), GTK_STYLE_PROVIDER_PRIORITY_USER);

	/* le scritte come nel disegno: sfumate ai bordi ma senza "hinting" (l'aggiustamento delle
	 * lettere alla griglia dei pixel), che a queste misure deforma i segni piccoli, per esempio
	 * l'accento di "giovedi'" */
	g_object_set(gtk_settings_get_default(), "gtk-font-rendering", GTK_FONT_RENDERING_MANUAL,
		"gtk-xft-antialias", 1, "gtk-xft-hinting", 0, "gtk-xft-hintstyle", "hintnone",
		"gtk-xft-rgba", "none", "gtk-hint-font-metrics", FALSE, NULL);

	const char *tema = g_strcmp0(g_getenv("MSHELL_TEMA"), "chiaro") == 0 ? "chiaro" : "scuro";
	crea_menu(tema);
	barra = crea_barra();
	mshell_cassetto_crea(tema, cassetto_cambiato);
	g_unix_signal_add(SIGUSR1, segnale_cassetto, NULL);
	scocca_minuto(NULL);
	mshell_elenco_app_avvia(titolo_cambiato);
	gtk_window_present(GTK_WINDOW(barra));

	GMainLoop *giro = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(giro);
	return 0;
}
