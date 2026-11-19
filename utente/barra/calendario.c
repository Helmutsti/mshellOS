/* IL CALENDARIO, il menu del pulsante con giorno e ora. Dall'alto:
 *   l'ora (10:00) e sotto la data lunga (giovedi' 19 novembre 2026);
 *   il mese che si vede, con le frecce per il mese prima e dopo;
 *   la griglia: le iniziali dei giorni (lunedi' per primo) e 6 settimane, sempre 6 righe cosi'
 *   il menu non cambia altezza da un mese all'altro. I giorni fuori dal mese sono spenti, oggi
 *   sta in una pillola.
 * Qui ci sono anche i nomi italiani di giorni e mesi, che usa pure la barra. */
#include "barra.h"

const char *const mshell_giorni_corti[7] = { "lun", "mar", "mer", "gio", "ven", "sab", "dom" };
const char *const mshell_giorni[7] = {
	"lunedì", "martedì", "mercoledì", "giovedì", "venerdì", "sabato",
	"domenica" };
static const char *const iniziali[7] = { "L", "M", "M", "G", "V", "S", "D" };
const char *const mshell_mesi_corti[12] = {
	"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic" };
const char *const mshell_mesi[12] = {
	"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio", "agosto",
	"settembre", "ottobre", "novembre", "dicembre" };
const char *const mshell_mesi_maiuscoli[12] = {
	"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno", "Luglio", "Agosto",
	"Settembre", "Ottobre", "Novembre", "Dicembre" };

int mshell_giorno_settimana(GDateTime *quando) {
	return g_date_time_get_day_of_week(quando) - 1;  /* GLib: 1 = lunedi' */
}

#define SETTIMANE 6

struct calendario {
	GtkWidget *ora, *data, *mese;
	GtkWidget *giorno[SETTIMANE * 7];
	int anno, mese_visto;            /* il mese che si vede (1-12) */
	int oggi_a, oggi_m, oggi_g;      /* la data di oggi */
};

/* Riempie la griglia con il mese che si vede. */
static void riempi(struct calendario *c) {
	char testo[32];
	g_snprintf(testo, sizeof(testo), "%s %d", mshell_mesi_maiuscoli[c->mese_visto - 1], c->anno);
	gtk_label_set_text(GTK_LABEL(c->mese), testo);
	/* si parte dal lunedi' della settimana del primo giorno del mese */
	GDate *d = g_date_new_dmy(1, c->mese_visto, c->anno);
	g_date_subtract_days(d, g_date_get_weekday(d) - G_DATE_MONDAY);
	for (int i = 0; i < SETTIMANE * 7; i++) {
		GtkWidget *g = c->giorno[i];
		g_snprintf(testo, sizeof(testo), "%d", g_date_get_day(d));
		gtk_label_set_text(GTK_LABEL(g), testo);
		gboolean fuori = g_date_get_month(d) != (GDateMonth)c->mese_visto;
		gboolean oggi = g_date_get_year(d) == c->oggi_a && g_date_get_month(d) == (GDateMonth)c->oggi_m &&
			g_date_get_day(d) == c->oggi_g;
		if (fuori) {
			gtk_widget_add_css_class(g, "fuori");
		} else {
			gtk_widget_remove_css_class(g, "fuori");
		}
		if (oggi) {
			gtk_widget_add_css_class(g, "oggi");
		} else {
			gtk_widget_remove_css_class(g, "oggi");
		}
		g_date_add_days(d, 1);
	}
	g_date_free(d);
}

static void cambia_mese(struct calendario *c, int passo) {
	c->mese_visto += passo;
	if (c->mese_visto < 1) {
		c->mese_visto = 12;
		c->anno--;
	} else if (c->mese_visto > 12) {
		c->mese_visto = 1;
		c->anno++;
	}
	riempi(c);
}

static void mese_prima(GtkButton *b, gpointer dati) {
	cambia_mese(dati, -1);
}

static void mese_dopo(GtkButton *b, gpointer dati) {
	cambia_mese(dati, 1);
}

static GtkWidget *scritta(const char *testo, const char *stile1, const char *stile2) {
	GtkWidget *l = gtk_label_new(testo);
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	gtk_widget_add_css_class(l, stile1);
	if (stile2) {
		gtk_widget_add_css_class(l, stile2);
	}
	return l;
}

static GtkWidget *freccia(enum mshell_icona icona, GCallback quando, struct calendario *c) {
	GtkWidget *b = gtk_button_new();
	gtk_button_set_child(GTK_BUTTON(b), mshell_icona_nuova(icona));
	gtk_widget_add_css_class(b, "pulsante");
	gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
	g_signal_connect(b, "clicked", quando, c);
	return b;
}

GtkWidget *mshell_calendario_nuovo(void) {
	struct calendario *c = g_new0(struct calendario, 1);
	GtkWidget *pannello = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(pannello, "pannello");
	g_object_set_data_full(G_OBJECT(pannello), "calendario", c, g_free);

	/* l'ora e la data lunga: un blocco solo */
	GtkWidget *tempo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(tempo, "tempo");
	c->ora = scritta("", "titolo", "primario");
	c->data = scritta("", "corpo", "secondario");
	gtk_box_append(GTK_BOX(tempo), c->ora);
	gtk_box_append(GTK_BOX(tempo), c->data);
	gtk_box_append(GTK_BOX(pannello), tempo);

	/* il mese con le frecce */
	GtkWidget *riga = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(riga, "riga-mese");
	c->mese = scritta("", "sezione", "primario");
	gtk_widget_set_hexpand(c->mese, TRUE);
	gtk_widget_set_valign(c->mese, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(riga), c->mese);
	gtk_box_append(GTK_BOX(riga), freccia(ICONA_FRECCIA_SINISTRA, G_CALLBACK(mese_prima), c));
	gtk_box_append(GTK_BOX(riga), freccia(ICONA_FRECCIA_DESTRA, G_CALLBACK(mese_dopo), c));
	gtk_box_append(GTK_BOX(pannello), riga);

	/* la griglia: le colonne si dividono la larghezza */
	GtkWidget *griglia = gtk_grid_new();
	gtk_widget_add_css_class(griglia, "griglia-giorni");
	gtk_grid_set_column_homogeneous(GTK_GRID(griglia), TRUE);
	for (int i = 0; i < 7; i++) {
		GtkWidget *l = gtk_label_new(iniziali[i]);
		gtk_widget_add_css_class(l, "etichetta");
		gtk_widget_add_css_class(l, "secondario");
		gtk_widget_add_css_class(l, "cella");
		gtk_grid_attach(GTK_GRID(griglia), l, i, 0, 1, 1);
	}
	for (int i = 0; i < SETTIMANE * 7; i++) {
		c->giorno[i] = gtk_label_new("");
		gtk_widget_add_css_class(c->giorno[i], "corpo");
		gtk_widget_add_css_class(c->giorno[i], "giorno");
		gtk_widget_add_css_class(c->giorno[i], "cella");
		gtk_grid_attach(GTK_GRID(griglia), c->giorno[i], i % 7, 1 + i / 7, 1, 1);
	}
	gtk_box_append(GTK_BOX(pannello), griglia);
	return pannello;
}

/* L'ora e la data di adesso; con "torna_a_oggi" si torna anche a vedere il mese di oggi. */
void mshell_calendario_aggiorna(GtkWidget *pannello, GDateTime *adesso, gboolean torna_a_oggi) {
	struct calendario *c = g_object_get_data(G_OBJECT(pannello), "calendario");
	char *ora = g_date_time_format(adesso, "%H:%M");
	gtk_label_set_text(GTK_LABEL(c->ora), ora);
	g_free(ora);
	c->oggi_a = g_date_time_get_year(adesso);
	c->oggi_m = g_date_time_get_month(adesso);
	c->oggi_g = g_date_time_get_day_of_month(adesso);
	char data[64];
	g_snprintf(data, sizeof(data), "%s %d %s %d", mshell_giorni[mshell_giorno_settimana(adesso)],
		c->oggi_g, mshell_mesi[c->oggi_m - 1], c->oggi_a);
	gtk_label_set_text(GTK_LABEL(c->data), data);
	if (torna_a_oggi || c->anno == 0) {
		c->anno = c->oggi_a;
		c->mese_visto = c->oggi_m;
	}
	riempi(c);
}
