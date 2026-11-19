/* mshell-barra: LA BARRA DI STATO E I SUOI MENU (decisione della persona: un programma a parte,
 * in C con GTK 4 e gtk4-layer-shell). La fa partire il compositore (mshell-compositore -s).
 *
 * LA BARRA e' una striscia trasparente alta 32 px in cima allo schermo, su tutta la larghezza,
 * nei 32 px di bordo che le app lasciano liberi. Sta nel livello "top" del compositore (sopra le
 * app) e non chiede spazio alle app (zona esclusiva 0). A sinistra il titolo dell'app col focus
 * (lo dice il compositore, elenco-app.c); a destra giorno e ora, la campanella e il menu: sono
 * pulsanti, e ognuno apre il suo menu a tendina (calendario, notifiche, comandi rapidi).
 *
 * I MENU (calendario, notifiche, comandi rapidi) sono menu a comparsa di Wayland (xdg_popup):
 * dei GtkPopover che hanno per genitore la barra (decisione della persona). Il pannello sta sotto
 * la barra, con l'angolo in alto a destra a 32 px dall'alto e da destra (sul bordo delle app).
 * GTK ci disegna solo il contenuto su fondo di vetro, senza freccetta, cornice, ombra o angoli:
 * gli angoli arrotondati, la sfocatura dietro e l'ombra li mette il compositore (decisione della
 * persona: le decorazioni sono del compositore), che riconosce i menu a comparsa della superficie
 * "mshell-barra" (utente/compositore/effetti.c).
 * Il menu chiede al compositore la "presa" (lo fa GTK, perche' il popover si chiude da solo):
 *   - un clic fuori dal menu lo chiude, e quel clic non arriva all'app sotto;
 *   - un clic su un altro pulsante della barra chiude il menu e apre l'altro; un clic sullo
 *     stesso pulsante lo chiude;
 *   - Esc lo chiude (il compositore da' la tastiera alla barra finche' il menu e' aperto).
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

static GtkWidget *menu[MENU_QUANTI];      /* i menu a comparsa (GtkPopover) */
static GtkWidget *pulsante[MENU_QUANTI];  /* i pulsanti della barra (GtkMenuButton), uno per menu */
static GtkWidget *pannello[MENU_QUANTI];  /* i pannelli dei menu */
static GtkWidget *riga;                   /* il contenuto della barra */
static GtkWidget *titolo_app;             /* il pulsante col titolo dell'app col focus */
static GtkWidget *orologio;               /* la scritta con giorno e ora */
static GtkWidget *barra;                  /* la superficie della barra */
static GtkWidget *tasti_visti;            /* STRUMENTO DI PROVA: gli ultimi tasti premuti */

/* STRUMENTO DI PROVA: rilegge il file dei tasti del compositore e mostra le ultime 6 righe. */
static gboolean aggiorna_tasti(gpointer dati) {
	(void)dati;
	char percorso[256];
	const char *dir = g_getenv("XDG_RUNTIME_DIR");
	if (dir == NULL) {
		return G_SOURCE_CONTINUE;
	}
	g_snprintf(percorso, sizeof(percorso), "%s/mshell-tasti", dir);
	gchar *testo = NULL;
	if (!g_file_get_contents(percorso, &testo, NULL, NULL)) {
		return G_SOURCE_CONTINUE;
	}
	gchar **righe = g_strsplit(g_strstrip(testo), "\n", -1);
	guint n = g_strv_length(righe);
	guint da = n > 6 ? n - 6 : 0;
	GString *s = g_string_new("tasti: ");
	for (guint i = da; i < n; i++) {
		g_string_append(s, righe[i]);
		g_string_append(s, "  ");
	}
	gtk_label_set_text(GTK_LABEL(tasti_visti), s->str);
	g_string_free(s, TRUE);
	g_strfreev(righe);
	g_free(testo);
	return G_SOURCE_CONTINUE;
}

/* Chiude il menu aperto, se c'e'. */
static void chiudi_menu(void) {
	for (int i = 0; i < MENU_QUANTI; i++) {
		gtk_popover_popdown(GTK_POPOVER(menu[i]));
	}
}

/* Un menu si apre (lo apre il suo pulsante, un GtkMenuButton: un clic lo apre, un altro clic lo
 * chiude, e un clic su un altro pulsante passa all'altro menu). */
static void menu_aperto(GtkWidget *p, gpointer dati) {
	enum menu quale = GPOINTER_TO_INT(dati);
	if (quale == CALENDARIO) {
		/* ogni volta che si apre, il calendario riparte dal mese di oggi */
		GDateTime *adesso = g_date_time_new_now_local();
		mshell_calendario_aggiorna(pannello[CALENDARIO], adesso, TRUE);
		g_date_time_unref(adesso);
	}
	/* il punto a cui si attacca: il proprio pulsante (la persona, 19/11/2026: "i menu devono
	 * aprirsi sotto il proprio tasto"), largo quanto lui e alto quanto la barra, cosi' il menu
	 * comincia sotto la barra; il menu ci mette il suo angolo in alto a destra (halign END, in
	 * crea_menu), cioe' il suo bordo destro e' quello del pulsante. Il rettangolo si da' nelle
	 * coordinate del pulsante, che e' il genitore del menu; la cima si conta dalla finestra della
	 * barra, che e' grande come la superficie. */
	graphene_point_t in_barra = GRAPHENE_POINT_INIT(0, 0);
	graphene_point_t nel_pulsante;
	if (gtk_widget_compute_point(barra, pulsante[quale], &in_barra, &nel_pulsante)) {
		GdkRectangle punto = { 0, (int)nel_pulsante.y, gtk_widget_get_width(pulsante[quale]), 32 };
		gtk_popover_set_pointing_to(GTK_POPOVER(p), &punto);
	}
	gtk_widget_add_css_class(pulsante[quale], "aperto");
}

static gboolean apri_dopo(gpointer dati);

/* Il menu si e' chiuso: dal suo pulsante, con Esc, con un clic fuori o da chiudi_menu.
 * Se si e' chiuso per un clic (tasto del mouse giu' adesso) sul pulsante di un ALTRO menu, si
 * apre quello: un clic solo per passare da un menu all'altro (la persona, 19/11/2026). GTK quel
 * clic lo usa solo per chiudere e non lo passa al pulsante. */
static void menu_chiuso(GtkPopover *p, gpointer dati) {
	int quale = GPOINTER_TO_INT(dati);
	gtk_widget_remove_css_class(pulsante[quale], "aperto");
	GdkSurface *sup = gtk_native_get_surface(GTK_NATIVE(barra));
	GdkDisplay *disp = gtk_widget_get_display(barra);
	GdkSeat *seat = disp != NULL ? gdk_display_get_default_seat(disp) : NULL;
	GdkDevice *mouse = seat != NULL ? gdk_seat_get_pointer(seat) : NULL;
	double x, y;
	GdkModifierType tasti;
	if (sup == NULL || mouse == NULL ||
			!gdk_surface_get_device_position(sup, mouse, &x, &y, &tasti) ||
			!(tasti & GDK_BUTTON1_MASK)) {
		return;
	}
	graphene_point_t in_barra = GRAPHENE_POINT_INIT(x, y), in_riga;
	if (!gtk_widget_compute_point(barra, riga, &in_barra, &in_riga)) {
		return;
	}
	GtkWidget *sotto = gtk_widget_pick(riga, in_riga.x, in_riga.y, GTK_PICK_DEFAULT);
	for (int i = 0; i < MENU_QUANTI; i++) {
		if (i != quale && sotto != NULL &&
				(sotto == pulsante[i] || gtk_widget_is_ancestor(sotto, pulsante[i]))) {
			g_idle_add(apri_dopo, GINT_TO_POINTER(i));
		}
	}
}

/* Apre il menu del pulsante "quale" (dopo che l'altro si e' chiuso). */
static gboolean apri_dopo(gpointer dati) {
	gtk_menu_button_popup(GTK_MENU_BUTTON(pulsante[GPOINTER_TO_INT(dati)]));
	return G_SOURCE_REMOVE;
}

/* Un clic sulla barra fuori dai pulsanti chiude il menu aperto. Un clic sul pulsante di un ALTRO
 * menu, mentre uno e' aperto, chiude quello e apre l'altro con un clic solo (la persona,
 * 19/11/2026): da solo GTK userebbe il primo clic solo per chiudere. */
static void clic_barra(GtkGestureClick *g, int n, double x, double y, gpointer dati) {
	GtkWidget *sotto = gtk_widget_pick(riga, x, y, GTK_PICK_DEFAULT);
	if (sotto == NULL || gtk_widget_get_ancestor(sotto, GTK_TYPE_BUTTON) == NULL) {
		chiudi_menu();
		return;
	}
	int aperto = -1, cliccato = -1;
	for (int i = 0; i < MENU_QUANTI; i++) {
		if (gtk_widget_get_visible(menu[i])) {
			aperto = i;
		}
		if (sotto == pulsante[i] || gtk_widget_is_ancestor(sotto, pulsante[i])) {
			cliccato = i;
		}
	}
	if (aperto >= 0 && cliccato >= 0 && cliccato != aperto) {
		chiudi_menu();
		gtk_gesture_set_state(GTK_GESTURE(g), GTK_EVENT_SEQUENCE_CLAIMED);
		g_idle_add(apri_dopo, GINT_TO_POINTER(cliccato));
	}
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
		chiudi_menu();
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
	GtkWidget *b = gtk_menu_button_new();
	gtk_menu_button_set_child(GTK_MENU_BUTTON(b), contenuto);
	gtk_widget_add_css_class(b, "pulsante");
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

	riga = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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

	/* STRUMENTO DI PROVA: gli ultimi tasti premuti, scritti dal compositore in
	 * $XDG_RUNTIME_DIR/mshell-tasti (vedi tinywl.c) */
	tasti_visti = gtk_label_new("");
	gtk_widget_add_css_class(tasti_visti, "etichetta");
	gtk_widget_add_css_class(tasti_visti, "primario");
	gtk_widget_set_valign(tasti_visti, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_end(tasti_visti, 16);
	gtk_box_append(GTK_BOX(riga), tasti_visti);
	g_timeout_add(100, aggiorna_tasti, NULL);

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

/* I menu: un popover per pannello, del suo pulsante, sotto il punto che dice menu_aperto, con il
 * bordo destro su quello del punto. Niente freccetta; il resto dell'aspetto di GTK (cornice,
 * ombra, angoli) lo azzera stile.css. */
static void crea_menu(const char *tema) {
	pannello[CALENDARIO] = mshell_calendario_nuovo();
	pannello[NOTIFICHE] = mshell_notifiche_nuove();
	pannello[COMANDI] = mshell_comandi_nuovi();
	for (int i = 0; i < MENU_QUANTI; i++) {
		GtkWidget *p = gtk_popover_new();
		gtk_widget_add_css_class(p, "menu");
		gtk_widget_add_css_class(p, tema);
		gtk_popover_set_has_arrow(GTK_POPOVER(p), FALSE);
		gtk_popover_set_position(GTK_POPOVER(p), GTK_POS_BOTTOM);
		gtk_widget_set_halign(p, GTK_ALIGN_END);
		gtk_popover_set_child(GTK_POPOVER(p), pannello[i]);
		gtk_menu_button_set_popover(GTK_MENU_BUTTON(pulsante[i]), p);
		g_signal_connect(p, "show", G_CALLBACK(menu_aperto), GINT_TO_POINTER(i));
		g_signal_connect(p, "closed", G_CALLBACK(menu_chiuso), GINT_TO_POINTER(i));
		menu[i] = p;
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
	barra = crea_barra();
	crea_menu(tema);
	mshell_cassetto_crea(tema, cassetto_cambiato);
	g_unix_signal_add(SIGUSR1, segnale_cassetto, NULL);
	scocca_minuto(NULL);
	mshell_elenco_app_avvia(titolo_cambiato);
	gtk_window_present(GTK_WINDOW(barra));

	GMainLoop *giro = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(giro);
	return 0;
}
