/* I MENU DELLE NOTIFICHE E DEI COMANDI RAPIDI. Per ora tutti e due sono fissi: le notifiche
 * sono due esempi (il servizio vero delle notifiche verra' dopo) e i comandi rapidi sono uno
 * scheletro che non fa ancora niente (voluto cosi' dalla persona). */
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

GtkWidget *mshell_comandi_nuovi(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pannello");
	gtk_box_append(GTK_BOX(p), scritta("Comandi rapidi", "sezione", "primario"));
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
