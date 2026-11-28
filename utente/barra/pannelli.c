/* I MENU DELLE NOTIFICHE E DEI COMANDI RAPIDI. Per ora tutti e due sono fissi: le notifiche
 * sono due esempi (il servizio vero delle notifiche verra' dopo) e i comandi rapidi sono uno
 * scheletro che non fa ancora niente (voluto cosi' dalla persona), tranne il pulsante per spegnere
 * il computer in alto a destra (la persona, 27/11/2026; spegni.c). */
#include "barra.h"

static GtkWidget *scritta(const char *testo, const char *stile1, const char *stile2) {
	GtkWidget *l = gtk_label_new(testo);
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	gtk_widget_add_css_class(l, stile1);
	gtk_widget_add_css_class(l, stile2);
	return l;
}

/* Una scheda: titolo e riga sotto. L'icona dell'app che manda la notifica (nel disegno, a
 * sinistra) per ora non c'e': arrivera' con il servizio delle notifiche, che sapra' di quale
 * app e' ogni notifica. */
static GtkWidget *scheda(const char *titolo, const char *riga) {
	GtkWidget *s = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(s, "scheda");
	gtk_box_append(GTK_BOX(s), scritta(titolo, "corpo-medio", "primario"));
	gtk_box_append(GTK_BOX(s), scritta(riga, "piccolo", "secondario"));
	return s;
}

GtkWidget *mshell_notifiche_nuove(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pannello");
	gtk_box_append(GTK_BOX(p), scritta("Notifiche", "sezione", "primario"));
	gtk_box_append(GTK_BOX(p), scheda("Aggiornamenti pronti", "3 app possono essere aggiornate"));
	gtk_box_append(GTK_BOX(p), scheda("Memoria quasi piena", "Doom usa 1,2 GB"));
	return p;
}

static GtkWidget *blocco(const char *nome) {
	GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(b, "blocco");
	gtk_box_append(GTK_BOX(b), scritta(nome, "corpo", "primario"));
	return b;
}

/* Il pulsante per spegnere: chiude il menu (o il pannello di ricerca, che nella pagina Widget ha
 * lo stesso pannello dei comandi rapidi) e chiede la conferma (spegni.c). */
static void spegni_premuto(GtkButton *b, gpointer dati) {
	GtkWidget *menu = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_POPOVER);
	if (menu != NULL) {
		gtk_popover_popdown(GTK_POPOVER(menu));
	}
	if (mshell_cassetto_aperto()) {
		mshell_cassetto_apri_chiudi();
	}
	mshell_spegni_chiedi();
}

GtkWidget *mshell_comandi_nuovi(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pannello");
	/* il titolo a sinistra e, sulla stessa riga a destra, il pulsante per spegnere il computer
	 * (la persona, 27/11/2026) */
	GtkWidget *titolo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(titolo, "riga-titolo");
	GtkWidget *nome = scritta("Comandi rapidi", "sezione", "primario");
	gtk_widget_set_hexpand(nome, TRUE);
	gtk_box_append(GTK_BOX(titolo), nome);
	GtkWidget *spegni = gtk_button_new();
	gtk_button_set_child(GTK_BUTTON(spegni), mshell_icona_nuova(ICONA_SPEGNI));
	gtk_widget_add_css_class(spegni, "pulsante");
	gtk_widget_add_css_class(spegni, "spegni");
	gtk_widget_set_focusable(spegni, FALSE);
	gtk_widget_set_valign(spegni, GTK_ALIGN_CENTER);
	g_signal_connect(spegni, "clicked", G_CALLBACK(spegni_premuto), NULL);
	gtk_box_append(GTK_BOX(titolo), spegni);
	gtk_box_append(GTK_BOX(p), titolo);
	/* la griglia 2x2: le due colonne si dividono la larghezza */
	GtkWidget *g = gtk_grid_new();
	gtk_widget_add_css_class(g, "griglia-comandi");
	gtk_grid_set_column_homogeneous(GTK_GRID(g), TRUE);
	gtk_grid_attach(GTK_GRID(g), blocco("Wi-Fi"), 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(g), blocco("Bluetooth"), 1, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(g), blocco("Non disturbare"), 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(g), blocco("Tema scuro"), 1, 1, 1, 1);
	gtk_box_append(GTK_BOX(p), g);
	/* le righe piene */
	gtk_box_append(GTK_BOX(p), blocco("Volume"));
	gtk_box_append(GTK_BOX(p), blocco("Luminosità"));
	gtk_box_append(GTK_BOX(p), blocco("Batteria e spegnimento"));
	return p;
}
