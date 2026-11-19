/* IL CASSETTO DELLE APP (decisione della persona: un cassetto di vetro, come su iPhone). Si apre
 * e si chiude con Super+spazio: il tasto lo prende il compositore, che lo dice alla barra con il
 * segnale SIGUSR1 (barra.c). Il disegno e' in Figma, pagina "Navigazione", schermate "Cassetto
 * app – Chiaro/Scuro".
 *
 * COM'E' FATTO: una superficie dei livelli ("overlay", come i menu) grande come lo schermo, tutta
 * di vetro (per ora senza sfocatura: solo il colore semitrasparente). Dentro, a 24 px dai bordi:
 *   a sinistra la ricerca in alto e sotto la griglia delle app (6 per riga, icone da 96);
 *   a destra una colonna larga 320: l'orologio grande, i comandi rapidi, le notifiche (gli stessi
 *   pannelli dei menu della barra, pannelli.c).
 * Mentre il cassetto e' aperto la barra di stato non si vede (barra.c): l'ora c'e' gia' qui.
 *
 * COME SI USA (decisioni della persona):
 *   - si apre con il cursore nella ricerca; Super+spazio di nuovo o Esc lo chiudono;
 *   - scrivendo, la griglia mostra solo le app il cui nome contiene il testo, senza contare
 *     maiuscole e accenti; Invio lancia l'app scelta (all'inizio la prima);
 *   - le frecce cambiano l'app scelta nella griglia (il cursore resta nella ricerca); anche un
 *     clic su un'app la lancia;
 *   - se l'app e' gia' aperta non se ne apre un'altra: il compositore la attiva e passa al
 *     desktop che la ospita (elenco-app.c). Un'app nuova si apre nel desktop che si vede.
 *
 * LE APP vengono dai file .desktop (lo standard freedesktop) delle cartelle in "cartelle": quelle
 * del sistema e quelle che esporta Flatpak. Si rileggono a ogni apertura, cosi' un'app appena
 * installata c'e' subito. Si mostrano in ordine di nome. L'icona si prende dal tema delle icone
 * (o dal file, se il .desktop da' un percorso), solo se e' un'immagine PNG: per le SVG servirebbe
 * librsvg, che non c'e'. Senza icona, un riquadro con l'iniziale.
 * LE CARTELLE del disegno (app raggruppate) arriveranno quando ci saranno dei raggruppamenti:
 * per ora non ce ne sono. */
#include <string.h>
#include <gio/gdesktopappinfo.h>
#include <gtk4-layer-shell.h>
#include "barra.h"

#define COLONNE 6      /* app per riga */
#define ICONA 96       /* la misura delle icone */

/* Un'app della griglia. */
struct voce {
	GDesktopAppInfo *app;
	char *per_cercare;   /* il nome senza maiuscole ne' accenti */
	GtkWidget *widget;   /* il pulsante nella griglia (tenuto anche quando non si vede) */
};

static GtkWidget *finestra, *ricerca, *griglia, *scorri, *ora, *data;
static GPtrArray *voci;    /* tutte le app (struct voce), in ordine di nome */
static GPtrArray *viste;   /* quelle che passano la ricerca, nell'ordine della griglia */
static guint scelta;       /* l'app scelta, in "viste" */
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
	g_object_unref(v->widget);
	g_free(v);
}

/* Il percorso dell'icona PNG dell'app, o NULL. */
static char *file_icona(GAppInfo *app) {
	GIcon *icona = g_app_info_get_icon(app);
	char *percorso = NULL;
	if (icona == NULL) {
		return NULL;
	}
	if (G_IS_FILE_ICON(icona)) {
		percorso = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icona)));
	} else {
		GtkIconTheme *tema = gtk_icon_theme_get_for_display(gdk_display_get_default());
		if (!gtk_icon_theme_has_gicon(tema, icona)) {
			return NULL;
		}
		GtkIconPaintable *p = gtk_icon_theme_lookup_by_gicon(tema, icona, ICONA, 1,
			GTK_TEXT_DIR_NONE, 0);
		GFile *f = gtk_icon_paintable_get_file(p);
		if (f != NULL) {
			percorso = g_file_get_path(f);
			g_object_unref(f);
		}
		g_object_unref(p);
	}
	if (percorso != NULL && !g_str_has_suffix(percorso, ".png")) {
		g_clear_pointer(&percorso, g_free);
	}
	return percorso;
}

/* Il quadrato da 96 dell'icona: l'immagine dell'app, o l'iniziale del nome. */
static GtkWidget *riquadro(GAppInfo *app, const char *nome) {
	GtkWidget *r = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(r, "riquadro");
	gtk_widget_set_size_request(r, ICONA, ICONA);
	gtk_widget_set_halign(r, GTK_ALIGN_CENTER);
	/* l'immagine e l'iniziale riempiono il riquadro, ma lui resta da 96 (detto esplicitamente,
	 * "vexpand" non passa a chi lo contiene) */
	gtk_widget_set_vexpand(r, FALSE);
	char *png = file_icona(app);
	if (png != NULL) {
		GtkWidget *immagine = gtk_picture_new_for_filename(png);
		gtk_picture_set_content_fit(GTK_PICTURE(immagine), GTK_CONTENT_FIT_CONTAIN);
		gtk_widget_set_vexpand(immagine, TRUE);
		gtk_box_append(GTK_BOX(r), immagine);
		g_free(png);
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

/* Lancia l'app, o attiva quella gia' aperta, e chiude il cassetto. Il nome con cui l'app si
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

/* Segna l'app scelta e la porta dentro la parte che si vede della griglia. */
static void segna(guint quale) {
	if (scelta < viste->len) {
		gtk_widget_remove_css_class(((struct voce *)viste->pdata[scelta])->widget, "scelta");
	}
	scelta = quale;
	if (scelta >= viste->len) {
		return;
	}
	GtkWidget *w = ((struct voce *)viste->pdata[scelta])->widget;
	gtk_widget_add_css_class(w, "scelta");
	graphene_rect_t r;
	if (gtk_widget_compute_bounds(w, griglia, &r)) {
		gtk_adjustment_clamp_page(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scorri)),
			r.origin.y, r.origin.y + r.size.height);
	}
}

/* Toglie tutte le app dalla griglia (restano in "voci"). */
static void svuota(void) {
	GtkWidget *w;
	while ((w = gtk_widget_get_first_child(griglia)) != NULL) {
		gtk_widget_remove_css_class(w, "scelta");
		gtk_grid_remove(GTK_GRID(griglia), w);
	}
	g_ptr_array_set_size(viste, 0);
}

/* Rifa' la griglia con le app che passano la ricerca; la scelta torna sulla prima. */
static void filtra(void) {
	char *cerca = per_cercare(gtk_editable_get_text(GTK_EDITABLE(ricerca)));
	svuota();
	for (guint i = 0; i < voci->len; i++) {
		struct voce *v = voci->pdata[i];
		if (strstr(v->per_cercare, cerca) != NULL) {
			gtk_grid_attach(GTK_GRID(griglia), v->widget, viste->len % COLONNE,
				viste->len / COLONNE, 1, 1);
			g_ptr_array_add(viste, v);
		}
	}
	g_free(cerca);
	scelta = 0;
	segna(0);
}

static void cambiata(GtkEditable *e, gpointer dati) {
	filtra();
}

static int per_nome(gconstpointer a, gconstpointer b) {
	const struct voce *x = *(struct voce *const *)a, *y = *(struct voce *const *)b;
	return g_utf8_collate(g_app_info_get_name(G_APP_INFO(x->app)),
		g_app_info_get_name(G_APP_INFO(y->app)));
}

/* Rilegge le app dai file .desktop. Se lo stesso file c'e' in piu' cartelle vale il primo. */
static void carica(void) {
	g_ptr_array_set_size(voci, 0);
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
			/* l'app nella griglia: l'icona e sotto il nome; un pulsante che non prende la
			 * tastiera, cosi' il cursore resta nella ricerca */
			GtkWidget *b = gtk_button_new();
			gtk_widget_add_css_class(b, "app");
			gtk_widget_set_focusable(b, FALSE);
			gtk_widget_set_focus_on_click(b, FALSE);
			gtk_widget_set_cursor_from_name(b, "pointer");  /* la manina: si clicca */
			GtkWidget *colonna = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
			gtk_widget_add_css_class(colonna, "colonna-app");
			gtk_box_append(GTK_BOX(colonna), riquadro(G_APP_INFO(app), titolo));
			GtkWidget *l = gtk_label_new(titolo);
			gtk_widget_add_css_class(l, "etichetta");
			gtk_widget_add_css_class(l, "primario");
			gtk_widget_add_css_class(l, "nome-app");
			gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
			gtk_label_set_max_width_chars(GTK_LABEL(l), 1);  /* non allarga la colonna */
			gtk_box_append(GTK_BOX(colonna), l);
			gtk_button_set_child(GTK_BUTTON(b), colonna);
			g_signal_connect(b, "clicked", G_CALLBACK(premuta), v);
			v->widget = g_object_ref_sink(b);
			g_ptr_array_add(voci, v);
		}
		g_dir_close(d);
	}
	g_hash_table_destroy(visti);
	g_ptr_array_sort(voci, per_nome);
}

static void apri(void) {
	svuota();
	carica();
	gtk_editable_set_text(GTK_EDITABLE(ricerca), "");
	filtra();
	avvisa(TRUE);
	gtk_window_present(GTK_WINDOW(finestra));
	gtk_widget_grab_focus(ricerca);
}

void mshell_cassetto_apri_chiudi(void) {
	if (gtk_widget_get_visible(finestra)) {
		chiudi();
	} else {
		apri();
	}
}

/* I tasti, prima che arrivino alla ricerca: Esc chiude, Invio lancia, le frecce cambiano
 * l'app scelta (su e giu' di una riga, sinistra e destra di una app). */
static gboolean tasto(GtkEventControllerKey *k, guint tasto, guint codice, GdkModifierType mod,
		gpointer dati) {
	guint n = viste->len;
	switch (tasto) {
	case GDK_KEY_Escape:
		chiudi();
		return TRUE;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if (scelta < n) {
			lancia(viste->pdata[scelta]);
		}
		return TRUE;
	case GDK_KEY_Left:
		if (scelta > 0) {
			segna(scelta - 1);
		}
		return TRUE;
	case GDK_KEY_Right:
		if (scelta + 1 < n) {
			segna(scelta + 1);
		}
		return TRUE;
	case GDK_KEY_Up:
		if (scelta >= COLONNE) {
			segna(scelta - COLONNE);
		}
		return TRUE;
	case GDK_KEY_Down:
		if (scelta + COLONNE < n) {
			segna(scelta + COLONNE);
		} else if (n > 0 && scelta / COLONNE < (n - 1) / COLONNE) {
			segna(n - 1);  /* la riga sotto e' corta: l'ultima app */
		}
		return TRUE;
	default:
		return FALSE;
	}
}

void mshell_cassetto_minuto(GDateTime *adesso) {
	char testo[64];
	g_snprintf(testo, sizeof(testo), "%02d:%02d", g_date_time_get_hour(adesso),
		g_date_time_get_minute(adesso));
	gtk_label_set_text(GTK_LABEL(ora), testo);
	g_snprintf(testo, sizeof(testo), "%s %d %s", mshell_giorni[mshell_giorno_settimana(adesso)],
		g_date_time_get_day_of_month(adesso), mshell_mesi[g_date_time_get_month(adesso) - 1]);
	gtk_label_set_text(GTK_LABEL(data), testo);
}

/* La ricerca: una pillola alta 48 con la lente e il testo. */
static GtkWidget *crea_ricerca(void) {
	GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(r, "ricerca");
	gtk_widget_set_halign(r, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(r, 560, -1);
	GtkWidget *lente = mshell_icona_nuova_misura(ICONA_CERCA, 20);
	gtk_widget_add_css_class(lente, "secondario");
	gtk_widget_set_valign(lente, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(r), lente);
	ricerca = gtk_text_new();
	gtk_text_set_placeholder_text(GTK_TEXT(ricerca), "Cerca app, file, impostazioni…");
	gtk_widget_add_css_class(ricerca, "testo-ricerca");
	gtk_widget_set_hexpand(ricerca, TRUE);
	gtk_widget_set_valign(ricerca, GTK_ALIGN_CENTER);
	g_signal_connect(ricerca, "changed", G_CALLBACK(cambiata), NULL);
	gtk_box_append(GTK_BOX(r), ricerca);
	return r;
}

/* Il pannello dell'orologio grande: l'ora e sotto la data lunga. */
static GtkWidget *crea_orario(void) {
	GtkWidget *p = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(p, "pannello");
	ora = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(ora), 0);
	gtk_widget_add_css_class(ora, "titolo");
	gtk_widget_add_css_class(ora, "primario");
	gtk_widget_add_css_class(ora, "ora-grande");
	data = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(data), 0);
	gtk_widget_add_css_class(data, "corpo");
	gtk_widget_add_css_class(data, "secondario");
	gtk_box_append(GTK_BOX(p), ora);
	gtk_box_append(GTK_BOX(p), data);
	return p;
}

void mshell_cassetto_crea(const char *tema, void (*cambiato)(gboolean aperto)) {
	avvisa = cambiato;
	voci = g_ptr_array_new_with_free_func(voce_libera);
	viste = g_ptr_array_new();
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
	gtk_layer_init_for_window(GTK_WINDOW(finestra));
	gtk_layer_set_namespace(GTK_WINDOW(finestra), "mshell-barra-cassetto");
	gtk_layer_set_layer(GTK_WINDOW(finestra), GTK_LAYER_SHELL_LAYER_OVERLAY);
	for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++) {
		gtk_layer_set_anchor(GTK_WINDOW(finestra), e, TRUE);
	}
	gtk_layer_set_exclusive_zone(GTK_WINDOW(finestra), -1);  /* tutto lo schermo */
	gtk_layer_set_keyboard_mode(GTK_WINDOW(finestra), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

	GtkWidget *contenuto = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(contenuto, "contenuto-cassetto");

	/* a sinistra: la ricerca e la griglia, in mezzo alla parte libera */
	GtkWidget *sinistra = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(sinistra, "colonna-cassetto");
	gtk_widget_set_hexpand(sinistra, TRUE);
	gtk_box_append(GTK_BOX(sinistra), crea_ricerca());
	scorri = gtk_scrolled_window_new();
	gtk_widget_add_css_class(scorri, "scorri-app");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scorri), GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scorri, TRUE);
	/* la griglia sta in un posto largo come 6 app (piu' i 4 px intorno per la cornice della
	 * scelta, stile.css); le app partono da sinistra */
	GtkWidget *posto = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(posto, "posto-griglia");
	gtk_widget_set_halign(posto, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(posto, COLONNE * ICONA + (COLONNE - 1) * 32 + 2 * 4, -1);
	griglia = gtk_grid_new();
	gtk_widget_add_css_class(griglia, "griglia-app");
	gtk_grid_set_column_homogeneous(GTK_GRID(griglia), TRUE);
	gtk_widget_set_halign(griglia, GTK_ALIGN_START);
	gtk_widget_set_valign(griglia, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(posto), griglia);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scorri), posto);
	gtk_box_append(GTK_BOX(sinistra), scorri);
	gtk_box_append(GTK_BOX(contenuto), sinistra);

	/* a destra: l'orologio grande, i comandi rapidi, le notifiche (fino in fondo) */
	GtkWidget *destra = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(destra, "colonna-laterale");
	gtk_widget_set_size_request(destra, 320, -1);
	gtk_box_append(GTK_BOX(destra), crea_orario());
	gtk_box_append(GTK_BOX(destra), mshell_comandi_nuovi());
	GtkWidget *notifiche = mshell_notifiche_nuove();
	gtk_widget_set_vexpand(notifiche, TRUE);
	gtk_box_append(GTK_BOX(destra), notifiche);
	gtk_box_append(GTK_BOX(contenuto), destra);

	gtk_window_set_child(GTK_WINDOW(finestra), contenuto);

	GtkEventController *tasti = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(tasti, GTK_PHASE_CAPTURE);
	g_signal_connect(tasti, "key-pressed", G_CALLBACK(tasto), NULL);
	gtk_widget_add_controller(finestra, tasti);
}
