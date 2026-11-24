/* IL PANNELLO DI RICERCA (prima era il cassetto delle app). Il disegno e' in Figma, pagina
 * "Scrivania", sezione "Pannello di ricerca – stile della barra" (foto in disegni/Scrivania/figma/
 * ricerca-*.png). Si apre e si chiude con Super+spazio (il tasto lo prende il compositore, che lo
 * dice alla barra con il segnale SIGUSR1, barra.c) o con il pulsante con la lente nella barra.
 *
 * COM'E' FATTO: una superficie dei livelli ("overlay") grande come lo schermo, tutta di vetro (la
 * sfocatura la fa il compositore, effetti.c). La barra di stato resta sopra (barra.c). Il pannello
 * ha tre pagine, una alla volta, in alto al centro (a 96 px dall'alto); in fondo, al centro, la
 * pillola con i nomi delle pagine (Widget, Ricerca, App), che si possono anche cliccare:
 *   WIDGET   a sinistra i comandi rapidi e sotto le notifiche, a destra il calendario (gli stessi
 *            pannelli dei menu della barra);
 *   RICERCA  il campo di ricerca, una pillola di vetro, e sotto i risultati, a gruppi, in un
 *            riquadro di vetro che c'e' solo quando ci sono risultati (scelta B della persona,
 *            20/11/2026: niente ombre, tutto piu' leggero). Per ora c'e' solo il gruppo "App":
 *            file e impostazioni arriveranno con Files e con le Impostazioni; "Usate spesso" e
 *            "Recenti" (la ricerca vuota del disegno) quando qualcuno terra' il conto di cosa si usa.
 *            Per adesso, con la ricerca vuota, sotto il campo non c'e' niente;
 *   APP      tutte le app in ordine di nome, 8 per riga, direttamente sul vetro, senza riquadro
 *            ne' titolo (scelta B della persona, 20/11/2026).
 * La cosa scelta con le frecce (un risultato, un'app) ha un fondo tenue.
 * Si apre sulla pagina Ricerca, con il cursore nel campo.
 * LE PAGINE SCORRONO in orizzontale, una accanto all'altra, larghe come lo schermo: passando da
 * una all'altra scorrono in MSHELL_DURATA_DESKTOP con la curva ease-in-out (parte piano, accelera e
 * rallenta all'arrivo), come il cambio di desktop del compositore (la persona, 20/11/2026). Con
 * MSHELL_MENO_MOVIMENTO=1 si passa subito.
 *
 * I TASTI (dalle didascalie del disegno):
 *   Widget   -> va alla Ricerca; Esc chiude. (Su, giu' e Invio sui comandi rapidi arriveranno
 *            quando i comandi rapidi faranno qualcosa: per ora sono uno scheletro.)
 *   Ricerca  su e giu' scelgono il risultato; <- Widget; -> App; Invio apre; Esc chiude
 *   App      le frecce si muovono fra le app; <- dalla prima colonna torna alla Ricerca; Invio
 *            apre; Esc chiude
 * Anche un clic su un risultato o su un'app la apre.
 * Se l'app e' gia' aperta non se ne apre un'altra: il compositore la attiva e passa al desktop che
 * la ospita (elenco-app.c). Un'app nuova si apre nel desktop che si vede.
 *
 * LE APP vengono dai file .desktop (lo standard freedesktop) delle cartelle in "cartelle": quelle
 * del sistema e quelle che esporta Flatpak. Si rileggono a ogni apertura, cosi' un'app appena
 * installata c'e' subito. L'icona si prende dal tema delle icone (o dal file, se il .desktop da' un
 * percorso): SVG (con librsvg, la persona 24/11/2026) o PNG. Senza icona, un riquadro con
 * l'iniziale. */
#include <string.h>
#include <gio/gdesktopappinfo.h>
#include <gtk4-layer-shell.h>
#include "barra.h"

#define COLONNE 8            /* app per riga nella pagina App */
#define ICONA_APP 56         /* l'icona nella pagina App */
#define ICONA_RIGA 24        /* l'icona in un risultato della ricerca (icona/oggetto-s) */
#define ALTEZZA_MASSIMA 560  /* oltre, i risultati e le app scorrono */
#define MSHELL_DURATA_DESKTOP 350  /* ms, come il cambio di desktop del compositore (animazioni.c) */

enum pagina { WIDGET, RICERCA, APP, PAGINE };
static const char *const nomi_pagine[PAGINE] = { "Widget", "Ricerca", "App" };

/* Un'app: la sua riga fra i risultati e il suo posto nella pagina App (tenuti anche quando non
 * si vedono). */
struct voce {
	GDesktopAppInfo *app;
	char *per_cercare;   /* il nome senza maiuscole ne' accenti */
	GtkWidget *riga;     /* il risultato nella pagina Ricerca */
	GtkWidget *posto;    /* l'app nella pagina App */
};

static GtkWidget *finestra, *pagine, *linguetta[PAGINE];
static int larghezza;          /* la larghezza dello schermo: quella di ogni pagina */
static double scorre_da, scorre_a;  /* lo scorrimento delle pagine: da dove a dove (in px) */
static gint64 scorre_inizio;   /* quando e' partito (microsecondi dell'orologio dei fotogrammi) */
static guint scorre_id;        /* il richiamo a ogni fotogramma, 0 se fermo */
static GtkWidget *ricerca, *gruppo_app, *elenco_app, *scorri_risultati;
static GtkWidget *griglia, *scorri_app, *calendario;
static enum pagina pagina;
static GPtrArray *voci;    /* tutte le app (struct voce), in ordine di nome */
static GPtrArray *trovate; /* quelle che passano la ricerca, nell'ordine dei risultati */
static guint scelta;       /* il risultato scelto, in "trovate" */
static guint scelta_app;   /* l'app scelta nella pagina App, in "voci" */
static void (*avvisa)(gboolean aperto);
/* le cartelle dei file .desktop: prima quella delle app Flatpak della persona (come vuole lo
 * standard, le cose della persona vengono prima di quelle del sistema), poi quella delle app
 * Flatpak del sistema, poi quella del sistema; il percorso della prima si fa all'avvio */
static const char *cartelle[] = {
	NULL,  /* ~/.local/share/flatpak/exports/share/applications */
	"/var/lib/flatpak/exports/share/applications",
	"/usr/share/applications",
};

/* Il testo come si confronta nella ricerca: senza accenti (le lettere si scompongono, NFD, e si
 * buttano i segni) e senza maiuscole. */
static char *per_cercare(const char *testo) {
	char *scomposto = g_utf8_normalize(testo, -1, G_NORMALIZE_NFD);
	if (scomposto == NULL) {
		return g_strdup("");
	}
	GString *s = g_string_new(NULL);
	for (const char *p = scomposto; *p; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (!g_unichar_ismark(c)) {
			g_string_append_unichar(s, c);
		}
	}
	char *risultato = g_utf8_casefold(s->str, -1);
	g_string_free(s, TRUE);
	g_free(scomposto);
	return risultato;
}

static void voce_libera(gpointer dati) {
	struct voce *v = dati;
	g_object_unref(v->app);
	g_free(v->per_cercare);
	g_object_unref(v->riga);
	g_object_unref(v->posto);
	g_free(v);
}

static GtkWidget *scritta(const char *testo, const char *stile1, const char *stile2) {
	GtkWidget *l = gtk_label_new(testo);
	gtk_label_set_xalign(GTK_LABEL(l), 0);
	gtk_widget_add_css_class(l, stile1);
	if (stile2 != NULL) {
		gtk_widget_add_css_class(l, stile2);
	}
	return l;
}

/* L'icona dell'app, disegnata alla misura giusta (SVG o PNG, dal tema delle icone o da un file), o
 * NULL se l'app non ne ha una. Gli SVG li legge librsvg (la persona, 24/11/2026). */
static GdkPaintable *icona_app(GAppInfo *app, int misura) {
	GIcon *icona = g_app_info_get_icon(app);
	if (icona == NULL) {
		return NULL;
	}
	if (G_IS_FILE_ICON(icona)) {
		GFile *f = g_file_icon_get_file(G_FILE_ICON(icona));
		if (!g_file_query_exists(f, NULL)) {
			return NULL;
		}
		return GDK_PAINTABLE(gtk_icon_paintable_new_for_file(f, misura, 1));
	}
	GtkIconTheme *tema = gtk_icon_theme_get_for_display(gdk_display_get_default());
	if (!gtk_icon_theme_has_gicon(tema, icona)) {
		return NULL;
	}
	return GDK_PAINTABLE(gtk_icon_theme_lookup_by_gicon(tema, icona, misura, 1, GTK_TEXT_DIR_NONE, 0));
}

/* Il quadrato dell'icona, grande "misura": l'immagine dell'app, o l'iniziale del nome. */
static GtkWidget *riquadro(GAppInfo *app, const char *nome, int misura) {
	GtkWidget *r = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(r, misura == ICONA_APP ? "riquadro" : "riquadro-piccolo");
	gtk_widget_set_size_request(r, misura, misura);
	gtk_widget_set_halign(r, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(r, GTK_ALIGN_CENTER);
	/* l'immagine e l'iniziale riempiono il riquadro, ma lui resta della sua misura (detto
	 * esplicitamente, "vexpand" non passa a chi lo contiene) */
	gtk_widget_set_vexpand(r, FALSE);
	gtk_widget_set_hexpand(r, FALSE);
	GdkPaintable *icona = icona_app(app, misura);
	if (icona != NULL) {
		/* GtkImage con la misura in pixel: resta grande esattamente quanto il riquadro (una
		 * GtkPicture prenderebbe la misura naturale dell'immagine e allargherebbe il riquadro) */
		GtkWidget *immagine = gtk_image_new_from_paintable(icona);
		gtk_image_set_pixel_size(GTK_IMAGE(immagine), misura);
		gtk_widget_set_vexpand(immagine, TRUE);
		gtk_box_append(GTK_BOX(r), immagine);
		g_object_unref(icona);
	} else {
		gtk_widget_add_css_class(r, "senza-icona");
		char iniziale[8] = "";
		g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(nome)), iniziale);
		GtkWidget *l = gtk_label_new(iniziale);
		gtk_widget_add_css_class(l, "iniziale");
		gtk_widget_set_vexpand(l, TRUE);
		gtk_box_append(GTK_BOX(r), l);
	}
	return r;
}

static void chiudi(void) {
	if (gtk_widget_get_visible(finestra)) {
		gtk_widget_set_visible(finestra, FALSE);
		avvisa(FALSE);
	}
}

/* Lancia l'app, o attiva quella gia' aperta, e chiude il pannello. Il nome con cui l'app si
 * presenta al compositore (app_id) di solito e' quello del suo file .desktop senza ".desktop"
 * (cosi' fanno GTK, Qt e Flatpak); qualche app lo dice nel file (StartupWMClass). */
static void lancia(struct voce *v) {
	char *id = g_strdup(g_app_info_get_id(G_APP_INFO(v->app)));
	if (id != NULL && g_str_has_suffix(id, ".desktop")) {
		id[strlen(id) - strlen(".desktop")] = '\0';
	}
	const char *nomi[] = { id ? id : "", g_desktop_app_info_get_startup_wm_class(v->app), NULL };
	if (!mshell_elenco_app_attiva(nomi)) {
		GdkAppLaunchContext *c = gdk_display_get_app_launch_context(gdk_display_get_default());
		GError *errore = NULL;
		if (!g_app_info_launch(G_APP_INFO(v->app), NULL, G_APP_LAUNCH_CONTEXT(c), &errore)) {
			g_warning("non parte %s: %s", g_app_info_get_name(G_APP_INFO(v->app)),
				errore->message);
			g_error_free(errore);
		}
		g_object_unref(c);
	}
	g_free(id);
	chiudi();
}

static void premuta(GtkButton *b, gpointer dati) {
	lancia(dati);
}

/* Porta il widget w dentro la parte che si vede di quello che scorre. */
static void fai_vedere(GtkWidget *w, GtkWidget *scorri) {
	GtkWidget *dentro = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(scorri));
	graphene_rect_t r;
	if (dentro != NULL && gtk_widget_compute_bounds(w, dentro, &r)) {
		gtk_adjustment_clamp_page(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scorri)),
			r.origin.y, r.origin.y + r.size.height);
	}
}

/* Segna il risultato scelto. */
static void segna(guint quale) {
	if (scelta < trovate->len) {
		gtk_widget_remove_css_class(((struct voce *)trovate->pdata[scelta])->riga, "scelta");
	}
	scelta = quale;
	if (scelta < trovate->len) {
		GtkWidget *w = ((struct voce *)trovate->pdata[scelta])->riga;
		gtk_widget_add_css_class(w, "scelta");
		fai_vedere(w, scorri_risultati);
	}
}

/* Segna l'app scelta nella pagina App. */
static void segna_app(guint quale) {
	if (scelta_app < voci->len) {
		gtk_widget_remove_css_class(((struct voce *)voci->pdata[scelta_app])->posto, "scelta");
	}
	scelta_app = quale;
	if (scelta_app < voci->len) {
		GtkWidget *w = ((struct voce *)voci->pdata[scelta_app])->posto;
		gtk_widget_add_css_class(w, "scelta");
		fai_vedere(w, scorri_app);
	}
}

/* Rifa' i risultati con le app che passano la ricerca; la scelta torna sul primo. Con la ricerca
 * vuota non c'e' nessun risultato. */
static void filtra(void) {
	segna(G_MAXUINT);
	GtkWidget *w;
	while ((w = gtk_widget_get_first_child(elenco_app)) != NULL) {
		gtk_box_remove(GTK_BOX(elenco_app), w);
	}
	g_ptr_array_set_size(trovate, 0);
	const char *testo = gtk_editable_get_text(GTK_EDITABLE(ricerca));
	if (testo[0] != '\0') {
		char *cerca = per_cercare(testo);
		for (guint i = 0; i < voci->len; i++) {
			struct voce *v = voci->pdata[i];
			if (strstr(v->per_cercare, cerca) != NULL) {
				gtk_box_append(GTK_BOX(elenco_app), v->riga);
				g_ptr_array_add(trovate, v);
			}
		}
		g_free(cerca);
	}
	gtk_widget_set_visible(scorri_risultati, trovate->len > 0);
	segna(0);
}

static void cambiata(GtkEditable *e, gpointer dati) {
	filtra();
}

/* La curva ease-in-out cubica: da 0 a 1, parte piano, accelera e rallenta all'arrivo. */
static double curva_dolce(double p) {
	if (p < 0.5) {
		return 4 * p * p * p;
	}
	double resto = -2 * p + 2;
	return 1 - resto * resto * resto / 2;
}

/* A ogni fotogramma: le pagine al punto di adesso. */
static gboolean scorri_passo(GtkWidget *w, GdkFrameClock *orologio, gpointer dati) {
	double p = (double)(gdk_frame_clock_get_frame_time(orologio) - scorre_inizio) /
		(MSHELL_DURATA_DESKTOP * 1000.0);
	GtkAdjustment *x = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pagine));
	if (p >= 1) {
		gtk_adjustment_set_value(x, scorre_a);
		scorre_id = 0;
		return G_SOURCE_REMOVE;
	}
	gtk_adjustment_set_value(x, scorre_da + (scorre_a - scorre_da) * curva_dolce(MAX(p, 0)));
	return G_SOURCE_CONTINUE;
}

/* Rimette le pagine sulla pagina giusta, se non stanno scorrendo. */
static gboolean rimetti(gpointer dati) {
	GtkAdjustment *x = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pagine));
	if (scorre_id == 0) {
		gtk_adjustment_set_value(x, scorre_a);
	}
	return G_SOURCE_REMOVE;
}

/* La misura delle pagine e' cambiata (per esempio la prima volta che il pannello compare: prima
 * la striscia non e' larga e lo scorrimento si ferma a 0, cioe' sul Widget). Questo avviene
 * mentre GTK sistema le misure, e uno spostamento chiesto adesso non verrebbe ridisegnato: le
 * pagine si rimettono a posto subito dopo. */
static void misura_cambiata(GtkAdjustment *x, gpointer dati) {
	if (scorre_id == 0 && gtk_adjustment_get_value(x) != scorre_a) {
		g_idle_add(rimetti, NULL);
	}
}

/* Le pagine scorrono fino alla pagina p; "subito": senza scorrere. Uno scorrimento nuovo parte
 * dal punto in cui sono adesso. */
static void scorri_a(enum pagina p, gboolean subito) {
	GtkAdjustment *x = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pagine));
	if (scorre_id != 0) {
		gtk_widget_remove_tick_callback(pagine, scorre_id);
		scorre_id = 0;
	}
	scorre_a = (double)p * larghezza;
	if (subito || g_strcmp0(g_getenv("MSHELL_MENO_MOVIMENTO"), "1") == 0 ||
			gtk_widget_get_frame_clock(pagine) == NULL) {
		gtk_adjustment_set_value(x, scorre_a);
		return;
	}
	scorre_da = gtk_adjustment_get_value(x);
	scorre_inizio = gdk_frame_clock_get_frame_time(gtk_widget_get_frame_clock(pagine));
	scorre_id = gtk_widget_add_tick_callback(pagine, scorri_passo, NULL, NULL);
}

/* Mostra la pagina p (scorrendo, se "subito" e' falso): la sua linguetta e' quella piena. */
static void vai_come(enum pagina p, gboolean subito) {
	pagina = p;
	scorri_a(p, subito);
	for (int i = 0; i < PAGINE; i++) {
		if (i == (int)p) {
			gtk_widget_add_css_class(linguetta[i], "scelta");
		} else {
			gtk_widget_remove_css_class(linguetta[i], "scelta");
		}
	}
	if (p == RICERCA) {
		gtk_widget_grab_focus(ricerca);
		gtk_editable_set_position(GTK_EDITABLE(ricerca), -1);
	} else {
		gtk_widget_grab_focus(finestra);
	}
	if (p == APP) {
		segna_app(scelta_app < voci->len ? scelta_app : 0);
	}
}

static void vai(enum pagina p) {
	vai_come(p, FALSE);
}

static void linguetta_premuta(GtkButton *b, gpointer dati) {
	vai(GPOINTER_TO_INT(dati));
}

static int per_nome(gconstpointer a, gconstpointer b) {
	const struct voce *x = *(struct voce *const *)a, *y = *(struct voce *const *)b;
	return g_utf8_collate(g_app_info_get_name(G_APP_INFO(x->app)),
		g_app_info_get_name(G_APP_INFO(y->app)));
}

/* Un pulsante che non prende la tastiera (il cursore resta nella ricerca), con la manina. */
static GtkWidget *pulsante_nuovo(const char *stile, GtkWidget *dentro) {
	GtkWidget *b = gtk_button_new();
	gtk_widget_add_css_class(b, stile);
	gtk_widget_set_focusable(b, FALSE);
	gtk_widget_set_focus_on_click(b, FALSE);
	gtk_widget_set_cursor_from_name(b, "pointer");
	gtk_button_set_child(GTK_BUTTON(b), dentro);
	return b;
}

/* Il risultato della ricerca: l'icona, il nome e a destra che cos'e' ("App"). */
static GtkWidget *riga_nuova(struct voce *v, const char *titolo) {
	GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(r, "contenuto-risultato");
	gtk_box_append(GTK_BOX(r), riquadro(G_APP_INFO(v->app), titolo, ICONA_RIGA));
	GtkWidget *nome = scritta(titolo, "corpo", "primario");
	gtk_label_set_ellipsize(GTK_LABEL(nome), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand(nome, TRUE);
	gtk_box_append(GTK_BOX(r), nome);
	gtk_box_append(GTK_BOX(r), scritta("App", "piccolo", "secondario"));
	GtkWidget *b = pulsante_nuovo("risultato", r);
	g_signal_connect(b, "clicked", G_CALLBACK(premuta), v);
	return b;
}

/* L'app nella pagina App: l'icona e sotto il nome. */
static GtkWidget *posto_nuovo(struct voce *v, const char *titolo) {
	GtkWidget *colonna = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(colonna, "colonna-app");
	gtk_box_append(GTK_BOX(colonna), riquadro(G_APP_INFO(v->app), titolo, ICONA_APP));
	GtkWidget *l = gtk_label_new(titolo);
	gtk_widget_add_css_class(l, "etichetta");
	gtk_widget_add_css_class(l, "primario");
	gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars(GTK_LABEL(l), 1);  /* non allarga la colonna */
	gtk_box_append(GTK_BOX(colonna), l);
	GtkWidget *b = pulsante_nuovo("app", colonna);
	g_signal_connect(b, "clicked", G_CALLBACK(premuta), v);
	return b;
}

/* Toglie tutte le app dai risultati e dalla pagina App. */
static void svuota(void) {
	segna(G_MAXUINT);
	segna_app(G_MAXUINT);
	GtkWidget *w;
	while ((w = gtk_widget_get_first_child(elenco_app)) != NULL) {
		gtk_box_remove(GTK_BOX(elenco_app), w);
	}
	while ((w = gtk_widget_get_first_child(griglia)) != NULL) {
		gtk_grid_remove(GTK_GRID(griglia), w);
	}
	g_ptr_array_set_size(trovate, 0);
	g_ptr_array_set_size(voci, 0);
}

/* Rilegge le app dai file .desktop (se lo stesso file c'e' in piu' cartelle vale il primo) e
 * rifa' la pagina App. */
static void carica(void) {
	svuota();
	GHashTable *visti = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (guint c = 0; c < G_N_ELEMENTS(cartelle); c++) {
		const char *cartella = cartelle[c];
		GDir *d = g_dir_open(cartella, 0, NULL);
		if (d == NULL) {
			continue;
		}
		const char *nome;
		while ((nome = g_dir_read_name(d)) != NULL) {
			if (!g_str_has_suffix(nome, ".desktop") || g_hash_table_contains(visti, nome)) {
				continue;
			}
			g_hash_table_add(visti, g_strdup(nome));
			char *percorso = g_build_filename(cartella, nome, NULL);
			GDesktopAppInfo *app = g_desktop_app_info_new_from_filename(percorso);
			g_free(percorso);
			if (app == NULL) {
				continue;
			}
			if (!g_app_info_should_show(G_APP_INFO(app))) {
				g_object_unref(app);
				continue;
			}
			const char *titolo = g_app_info_get_name(G_APP_INFO(app));
			struct voce *v = g_new0(struct voce, 1);
			v->app = app;
			v->per_cercare = per_cercare(titolo);
			v->riga = g_object_ref_sink(riga_nuova(v, titolo));
			v->posto = g_object_ref_sink(posto_nuovo(v, titolo));
			g_ptr_array_add(voci, v);
		}
		g_dir_close(d);
	}
	g_hash_table_destroy(visti);
	g_ptr_array_sort(voci, per_nome);
	for (guint i = 0; i < voci->len; i++) {
		struct voce *v = voci->pdata[i];
		gtk_grid_attach(GTK_GRID(griglia), v->posto, i % COLONNE, i / COLONNE, 1, 1);
	}
	scelta_app = 0;
}

/* Quando la superficie del pannello c'e' davvero: solo adesso si avvisa la barra, che cosi'
 * passa sopra il pannello (barra.c). */
static void comparso(GtkWidget *w, gpointer dati) {
	avvisa(TRUE);
}

static void apri(void) {
	carica();
	gtk_editable_set_text(GTK_EDITABLE(ricerca), "");
	filtra();
	GDateTime *adesso = g_date_time_new_now_local();
	mshell_calendario_aggiorna(calendario, adesso, TRUE);
	g_date_time_unref(adesso);
	gtk_window_present(GTK_WINDOW(finestra));
	vai_come(RICERCA, TRUE);  /* si apre gia' sulla Ricerca, senza scorrere */
}

void mshell_cassetto_apri_chiudi(void) {
	if (gtk_widget_get_visible(finestra)) {
		chiudi();
	} else {
		apri();
	}
}

gboolean mshell_cassetto_aperto(void) {
	return finestra != NULL && gtk_widget_get_visible(finestra);
}

/* I tasti nella pagina Ricerca. */
static gboolean tasto_ricerca(guint tasto) {
	guint n = trovate->len;
	switch (tasto) {
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if (scelta < n) {
			lancia(trovate->pdata[scelta]);
		}
		return TRUE;
	case GDK_KEY_Up:
		if (scelta > 0 && scelta < n) {
			segna(scelta - 1);
		}
		return TRUE;
	case GDK_KEY_Down:
		if (scelta + 1 < n) {
			segna(scelta + 1);
		}
		return TRUE;
	case GDK_KEY_Left:
		vai(WIDGET);
		return TRUE;
	case GDK_KEY_Right:
		vai(APP);
		return TRUE;
	default:
		return FALSE;
	}
}

/* I tasti nella pagina App. */
static gboolean tasto_app(guint tasto) {
	guint n = voci->len;
	switch (tasto) {
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if (scelta_app < n) {
			lancia(voci->pdata[scelta_app]);
		}
		return TRUE;
	case GDK_KEY_Left:
		if (scelta_app % COLONNE == 0) {
			vai(RICERCA);  /* dalla prima colonna si torna alla ricerca */
		} else {
			segna_app(scelta_app - 1);
		}
		return TRUE;
	case GDK_KEY_Right:
		if (scelta_app + 1 < n && (scelta_app + 1) % COLONNE != 0) {
			segna_app(scelta_app + 1);
		}
		return TRUE;
	case GDK_KEY_Up:
		if (scelta_app >= COLONNE) {
			segna_app(scelta_app - COLONNE);
		}
		return TRUE;
	case GDK_KEY_Down:
		if (scelta_app + COLONNE < n) {
			segna_app(scelta_app + COLONNE);
		} else if (n > 0 && scelta_app / COLONNE < (n - 1) / COLONNE) {
			segna_app(n - 1);  /* la riga sotto e' corta: l'ultima app */
		}
		return TRUE;
	default:
		return FALSE;
	}
}

/* I tasti, prima che arrivino al campo di ricerca: Esc chiude; il resto dipende dalla pagina. */
static gboolean tasto(GtkEventControllerKey *k, guint tasto, guint codice, GdkModifierType mod,
		gpointer dati) {
	if (tasto == GDK_KEY_Escape) {
		chiudi();
		return TRUE;
	}
	switch (pagina) {
	case WIDGET:
		if (tasto == GDK_KEY_Right) {
			vai(RICERCA);
			return TRUE;
		}
		return FALSE;
	case RICERCA:
		return tasto_ricerca(tasto);
	case APP:
		return tasto_app(tasto);
	default:
		return FALSE;
	}
}

void mshell_cassetto_minuto(GDateTime *adesso) {
	mshell_calendario_aggiorna(calendario, adesso, FALSE);
}

/* Un pannello di vetro con gli angoli e l'ombra (in questa superficie li disegna la barra). */
static GtkWidget *scheda(GtkWidget *dentro) {
	gtk_widget_add_css_class(dentro, "scheda-ricerca");
	return dentro;
}

/* La pagina Widget: a sinistra i comandi rapidi e sotto le notifiche, a destra il calendario. */
static GtkWidget *crea_widget(void) {
	GtkWidget *riga = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(riga, "pagina-widget");
	gtk_widget_set_halign(riga, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(riga, GTK_ALIGN_START);
	GtkWidget *sinistra = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(sinistra, "colonna-widget");
	gtk_widget_set_valign(sinistra, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(sinistra), scheda(mshell_comandi_nuovi()));
	gtk_box_append(GTK_BOX(sinistra), scheda(mshell_notifiche_nuove()));
	gtk_box_append(GTK_BOX(riga), sinistra);
	calendario = scheda(mshell_calendario_nuovo());
	gtk_widget_set_valign(calendario, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(riga), calendario);
	return riga;
}

/* La pagina Ricerca: il campo con la lente e sotto i risultati (per ora solo il gruppo "App"). */
static GtkWidget *crea_ricerca(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pannello-ricerca");
	gtk_widget_set_halign(p, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(p, GTK_ALIGN_START);
	gtk_widget_set_size_request(p, 640, -1);

	GtkWidget *campo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(campo, "campo-ricerca");
	GtkWidget *lente = mshell_icona_nuova_misura(ICONA_CERCA, 20);
	gtk_widget_add_css_class(lente, "primario");
	gtk_widget_set_valign(lente, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(campo), lente);
	ricerca = gtk_text_new();
	gtk_text_set_placeholder_text(GTK_TEXT(ricerca), "Cerca app, file, impostazioni…");
	gtk_widget_add_css_class(ricerca, "testo-ricerca");
	gtk_widget_set_hexpand(ricerca, TRUE);
	gtk_widget_set_valign(ricerca, GTK_ALIGN_CENTER);
	g_signal_connect(ricerca, "changed", G_CALLBACK(cambiata), NULL);
	gtk_box_append(GTK_BOX(campo), ricerca);
	gtk_box_append(GTK_BOX(p), campo);

	/* i risultati: i gruppi uno sotto l'altro; se sono tanti scorrono */
	GtkWidget *gruppi = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(gruppi, "gruppi");
	gruppo_app = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(gruppo_app, "gruppo");
	gtk_box_append(GTK_BOX(gruppo_app), scritta("App", "intestazione", "secondario"));
	elenco_app = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(elenco_app, "elenco");
	gtk_box_append(GTK_BOX(gruppo_app), elenco_app);
	gtk_box_append(GTK_BOX(gruppi), gruppo_app);
	scorri_risultati = gtk_scrolled_window_new();
	gtk_widget_add_css_class(scorri_risultati, "risultati");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scorri_risultati), GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scorri_risultati), TRUE);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scorri_risultati),
		ALTEZZA_MASSIMA);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scorri_risultati), gruppi);
	gtk_box_append(GTK_BOX(p), scorri_risultati);
	return p;
}

/* La pagina App: la griglia di tutte le app, sul vetro. */
static GtkWidget *crea_app(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pagina-app");
	gtk_widget_set_halign(p, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(p, GTK_ALIGN_START);
	griglia = gtk_grid_new();
	gtk_widget_add_css_class(griglia, "griglia-app");
	gtk_grid_set_column_homogeneous(GTK_GRID(griglia), TRUE);
	gtk_widget_set_halign(griglia, GTK_ALIGN_START);
	scorri_app = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scorri_app), GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scorri_app), TRUE);
	gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scorri_app), TRUE);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scorri_app), ALTEZZA_MASSIMA);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scorri_app), griglia);
	gtk_box_append(GTK_BOX(p), scorri_app);
	return p;
}

/* La rotella o il tappetino in orizzontale non muovono le pagine. */
static gboolean rotella_ferma(GtkEventControllerScroll *c, double dx, double dy, gpointer dati) {
	return dx != 0 && dy == 0;
}

/* La pillola in fondo con i nomi delle pagine. */
static GtkWidget *crea_linguette(void) {
	GtkWidget *l = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(l, "linguette");
	gtk_widget_set_halign(l, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(l, GTK_ALIGN_END);
	for (int i = 0; i < PAGINE; i++) {
		GtkWidget *nome = scritta(nomi_pagine[i], "etichetta", NULL);
		linguetta[i] = pulsante_nuovo("linguetta", nome);
		g_signal_connect(linguetta[i], "clicked", G_CALLBACK(linguetta_premuta),
			GINT_TO_POINTER(i));
		gtk_box_append(GTK_BOX(l), linguetta[i]);
	}
	return l;
}

void mshell_cassetto_crea(const char *tema, void (*cambiato)(gboolean aperto)) {
	avvisa = cambiato;
	voci = g_ptr_array_new_with_free_func(voce_libera);
	trovate = g_ptr_array_new();
	cartelle[0] = g_build_filename(g_get_home_dir(), ".local/share/flatpak/exports/share/applications", NULL);
	/* le icone che esporta Flatpak (le altre il tema le cerca gia' in /usr/share/icons) */
	GtkIconTheme *icone = gtk_icon_theme_get_for_display(gdk_display_get_default());
	gtk_icon_theme_add_search_path(icone, "/var/lib/flatpak/exports/share/icons");
	char *icone_utente = g_build_filename(g_get_home_dir(), ".local/share/flatpak/exports/share/icons", NULL);
	gtk_icon_theme_add_search_path(icone, icone_utente);
	g_free(icone_utente);

	finestra = gtk_window_new();
	gtk_window_set_decorated(GTK_WINDOW(finestra), FALSE);
	gtk_widget_add_css_class(finestra, "cassetto");
	gtk_widget_add_css_class(finestra, tema);
	gtk_widget_set_focusable(finestra, TRUE);
	gtk_layer_init_for_window(GTK_WINDOW(finestra));
	gtk_layer_set_namespace(GTK_WINDOW(finestra), "mshell-barra-cassetto");
	gtk_layer_set_layer(GTK_WINDOW(finestra), GTK_LAYER_SHELL_LAYER_OVERLAY);
	for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++) {
		gtk_layer_set_anchor(GTK_WINDOW(finestra), e, TRUE);
	}
	gtk_layer_set_exclusive_zone(GTK_WINDOW(finestra), -1);  /* tutto lo schermo */
	gtk_layer_set_keyboard_mode(GTK_WINDOW(finestra), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
	g_signal_connect(finestra, "map", G_CALLBACK(comparso), NULL);

	/* LE PAGINE: una striscia di tre posti larghi come lo schermo, ognuno con la sua pagina in
	 * alto al centro, dentro una finestra che scorre in orizzontale; lo scorrimento lo comanda
	 * solo scorri_a (niente barra di scorrimento, e la rotella o il tappetino non la muovono).
	 * Le linguette stanno sopra, in fondo. */
	GListModel *schermi = gdk_display_get_monitors(gdk_display_get_default());
	GdkMonitor *schermo = g_list_model_get_n_items(schermi) > 0 ?
		g_list_model_get_item(schermi, 0) : NULL;
	GdkRectangle misura = { 0, 0, 1280, 800 };
	if (schermo != NULL) {
		gdk_monitor_get_geometry(schermo, &misura);
		g_object_unref(schermo);
	}
	larghezza = misura.width;
	GtkWidget *striscia = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *(*crea[PAGINE])(void) = { crea_widget, crea_ricerca, crea_app };
	for (int i = 0; i < PAGINE; i++) {
		GtkWidget *posto = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_add_css_class(posto, "pagina");
		gtk_widget_set_size_request(posto, larghezza, -1);
		gtk_box_append(GTK_BOX(posto), crea[i]());
		gtk_box_append(GTK_BOX(striscia), posto);
	}
	pagine = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pagine), GTK_POLICY_EXTERNAL,
		GTK_POLICY_NEVER);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pagine), striscia);
	g_signal_connect(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(pagine)), "changed",
		G_CALLBACK(misura_cambiata), NULL);
	/* il cursore che entra nel campo di ricerca non deve far scorrere le pagine */
	gtk_viewport_set_scroll_to_focus(
		GTK_VIEWPORT(gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(pagine))), FALSE);
	GtkEventController *rotella = gtk_event_controller_scroll_new(
		GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL);
	gtk_event_controller_set_propagation_phase(rotella, GTK_PHASE_CAPTURE);
	g_signal_connect(rotella, "scroll", G_CALLBACK(rotella_ferma), NULL);
	gtk_widget_add_controller(pagine, rotella);

	GtkWidget *sopra = gtk_overlay_new();
	gtk_widget_add_css_class(sopra, "contenuto-cassetto");
	gtk_overlay_set_child(GTK_OVERLAY(sopra), pagine);
	gtk_overlay_add_overlay(GTK_OVERLAY(sopra), crea_linguette());
	gtk_window_set_child(GTK_WINDOW(finestra), sopra);

	GtkEventController *tasti = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(tasti, GTK_PHASE_CAPTURE);
	g_signal_connect(tasti, "key-pressed", G_CALLBACK(tasto), NULL);
	gtk_widget_add_controller(finestra, tasti);
}
