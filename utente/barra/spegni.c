/* LO SPEGNIMENTO DEL COMPUTER (decisione della persona, 27/11/2026). Il pulsante con l'icona
 * "power" sta in alto a destra nel pannello dei comandi rapidi (pannelli.c); al clic il menu si
 * chiude e compare la RICHIESTA DI CONFERMA, come sul Mac: un riquadro al centro dello schermo,
 * sopra tutto, con lo schermo dietro un po' scurito. "Vuoi spegnere il computer?", con "Annulla" e
 * "Spegni". Annulla, Esc o un clic sulla zona scurita chiudono il riquadro senza fare niente.
 *
 * COM'E' FATTO: una superficie dei livelli ("overlay", la piu' alta) attaccata a tutti i bordi,
 * quindi grande come lo schermo, che chiede la tastiera in modo esclusivo: finche' c'e', i tasti
 * sono suoi (Esc) e le app non la possono riprendere (utente/compositore/livelli.c). Tutta la
 * superficie e' il velo che scurisce (superficie/velo); il riquadro sta al centro. Niente vetro:
 * il nome della superficie non e' fra quelli che il compositore sfoca (effetti.c), quindi il
 * riquadro e' pieno (superficie/elevata) e la sua ombra la disegna la barra, come i pannelli del
 * pannello di ricerca.
 *
 * "SPEGNI" chiede lo spegnimento a systemd-logind (il servizio di systemd che tiene le sessioni e
 * accende e spegne il computer), sul bus di sistema di D-Bus, con GDBus di GLib: il metodo
 * PowerOff di org.freedesktop.login1.Manager, "interattivo" (se serve un permesso, logind lo puo'
 * chiedere). Il permesso lo da' polkit; se logind dice di no, l'errore va nel registro (l'uscita
 * d'errore della barra) e non succede altro. */
#include <gtk4-layer-shell.h>
#include "barra.h"

static GtkWidget *finestra;  /* la superficie: il velo grande come lo schermo */
static GtkWidget *riquadro;  /* il riquadro al centro */

static void chiudi(void) {
	gtk_widget_set_visible(finestra, FALSE);
}

gboolean mshell_spegni_aperto(void) {
	return finestra != NULL && gtk_widget_get_visible(finestra);
}

void mshell_spegni_chiedi(void) {
	gtk_window_present(GTK_WINDOW(finestra));
}

/* La risposta di logind: se ha detto di no (per esempio manca il permesso), lo si scrive. */
static void spento(GObject *bus, GAsyncResult *risultato, gpointer dati) {
	GError *errore = NULL;
	GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(bus), risultato, &errore);
	if (r == NULL) {
		g_printerr("mshell-barra: logind non spegne: %s\n", errore->message);
		g_error_free(errore);
		return;
	}
	g_variant_unref(r);
}

static void bus_pronto(GObject *sorgente, GAsyncResult *risultato, gpointer dati) {
	GError *errore = NULL;
	GDBusConnection *bus = g_bus_get_finish(risultato, &errore);
	if (bus == NULL) {
		g_printerr("mshell-barra: niente bus di sistema: %s\n", errore->message);
		g_error_free(errore);
		return;
	}
	g_dbus_connection_call(bus, "org.freedesktop.login1", "/org/freedesktop/login1",
		"org.freedesktop.login1.Manager", "PowerOff", g_variant_new("(b)", TRUE), NULL,
		G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, -1, NULL, spento, NULL);
	g_object_unref(bus);
}

static void spegni_premuto(GtkButton *b, gpointer dati) {
	chiudi();
	g_bus_get(G_BUS_TYPE_SYSTEM, NULL, bus_pronto, NULL);
}

static void annulla_premuto(GtkButton *b, gpointer dati) {
	chiudi();
}

static gboolean tasto(GtkEventControllerKey *c, guint tasto, guint codice, GdkModifierType mod,
		gpointer dati) {
	if (tasto == GDK_KEY_Escape) {
		chiudi();
		return TRUE;
	}
	return FALSE;
}

/* Un clic sul velo, fuori dal riquadro, chiude. */
static void clic(GtkGestureClick *g, int n, double x, double y, gpointer dati) {
	GtkWidget *sotto = gtk_widget_pick(finestra, x, y, GTK_PICK_DEFAULT);
	if (sotto == NULL || (sotto != riquadro && !gtk_widget_is_ancestor(sotto, riquadro))) {
		chiudi();
	}
}

/* Un pulsante del riquadro. Non prende il fuoco della tastiera: solo Esc chiude da tastiera, e
 * Invio o spazio non premono niente per sbaglio. */
static GtkWidget *pulsante(const char *testo, const char *stile, GCallback premuto) {
	GtkWidget *b = gtk_button_new();
	GtkWidget *l = gtk_label_new(testo);
	gtk_widget_add_css_class(l, "corpo-medio");
	gtk_button_set_child(GTK_BUTTON(b), l);
	gtk_widget_add_css_class(b, "pulsante-conferma");
	gtk_widget_add_css_class(b, stile);
	gtk_widget_set_focusable(b, FALSE);
	gtk_widget_set_hexpand(b, TRUE);
	g_signal_connect(b, "clicked", premuto, NULL);
	return b;
}

void mshell_spegni_crea(const char *tema) {
	finestra = gtk_window_new();
	gtk_window_set_decorated(GTK_WINDOW(finestra), FALSE);
	gtk_widget_add_css_class(finestra, "velo");
	gtk_widget_add_css_class(finestra, tema);
	gtk_layer_init_for_window(GTK_WINDOW(finestra));
	gtk_layer_set_namespace(GTK_WINDOW(finestra), "mshell-barra-spegni");
	gtk_layer_set_layer(GTK_WINDOW(finestra), GTK_LAYER_SHELL_LAYER_OVERLAY);
	for (int e = 0; e < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; e++) {
		gtk_layer_set_anchor(GTK_WINDOW(finestra), e, TRUE);
	}
	gtk_layer_set_exclusive_zone(GTK_WINDOW(finestra), -1);  /* tutto lo schermo */
	gtk_layer_set_keyboard_mode(GTK_WINDOW(finestra), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

	riquadro = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(riquadro, "riquadro-conferma");
	gtk_widget_set_halign(riquadro, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(riquadro, GTK_ALIGN_CENTER);
	GtkWidget *domanda = gtk_label_new("Vuoi spegnere il computer?");
	gtk_widget_add_css_class(domanda, "sezione");
	gtk_widget_add_css_class(domanda, "primario");
	gtk_box_append(GTK_BOX(riquadro), domanda);
	GtkWidget *scelte = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(scelte, "scelte-conferma");
	gtk_box_set_homogeneous(GTK_BOX(scelte), TRUE);
	gtk_box_append(GTK_BOX(scelte), pulsante("Annulla", "annulla", G_CALLBACK(annulla_premuto)));
	gtk_box_append(GTK_BOX(scelte), pulsante("Spegni", "conferma", G_CALLBACK(spegni_premuto)));
	gtk_box_append(GTK_BOX(riquadro), scelte);
	gtk_window_set_child(GTK_WINDOW(finestra), riquadro);

	GtkEventController *tasti = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(tasti, GTK_PHASE_CAPTURE);
	g_signal_connect(tasti, "key-pressed", G_CALLBACK(tasto), NULL);
	gtk_widget_add_controller(finestra, tasti);
	GtkGesture *g = gtk_gesture_click_new();
	g_signal_connect(g, "pressed", G_CALLBACK(clic), NULL);
	gtk_widget_add_controller(finestra, GTK_EVENT_CONTROLLER(g));
}
