/* L'ELENCO DELLE APP: la barra chiede al compositore quali app ci sono, con il protocollo
 * standard wlr-foreign-toplevel-management (il compositore lo offre, utente/compositore/livelli.c).
 * Di ogni app arrivano titolo e stato a pezzi, chiusi da "done"; alla fine la barra mostra il
 * titolo dell'app "attiva", cioe' quella col focus.
 * La connessione al compositore e' quella di GTK: i messaggi di questo protocollo arrivano nello
 * stesso giro di GTK, senza un filo a parte.
 * Lo usa anche il cassetto delle app (cassetto.c): di ogni app si tiene il nome (app_id), e se
 * l'app che si vuole lanciare e' gia' aperta il cassetto chiede al compositore di attivarla
 * (mshell_elenco_app_attiva): il compositore passa al desktop che la ospita. */
#include <string.h>
#include <gdk/wayland/gdkwayland.h>
#include "barra.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

struct app {
	struct zwlr_foreign_toplevel_handle_v1 *voce;
	char *titolo;          /* quello confermato da "done" */
	char *titolo_nuovo;    /* arrivato, in attesa di "done" */
	char *nome;            /* l'app_id, per esempio "mshell-prova" (NULL se non lo dice) */
	gboolean attiva, attiva_nuova;
};

static GList *app;  /* struct app */
static void (*avvisa)(const char *titolo);
static struct wl_seat *posto;  /* il "posto" (seat) di GTK: serve per chiedere l'attivazione */

static void ricalcola(void) {
	const char *titolo = "";
	for (GList *i = app; i; i = i->next) {
		struct app *a = i->data;
		if (a->attiva && a->titolo) {
			titolo = a->titolo;
		}
	}
	avvisa(titolo);
}

static void v_titolo(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v, const char *titolo) {
	struct app *a = dati;
	g_free(a->titolo_nuovo);
	a->titolo_nuovo = g_strdup(titolo);
}

static void v_app_id(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v, const char *id) {
	struct app *a = dati;
	g_free(a->nome);
	a->nome = g_strdup(id);
}

static void v_schermo_entra(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v,
		struct wl_output *o) {
}

static void v_schermo_esce(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v,
		struct wl_output *o) {
}

static void v_stato(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v, struct wl_array *stato) {
	struct app *a = dati;
	a->attiva_nuova = FALSE;
	uint32_t *s;
	wl_array_for_each(s, stato) {
		if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
			a->attiva_nuova = TRUE;
		}
	}
}

static void v_fatto(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v) {
	struct app *a = dati;
	if (a->titolo_nuovo) {
		g_free(a->titolo);
		a->titolo = a->titolo_nuovo;
		a->titolo_nuovo = NULL;
	}
	a->attiva = a->attiva_nuova;
	ricalcola();
}

static void v_chiusa(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v) {
	struct app *a = dati;
	app = g_list_remove(app, a);
	zwlr_foreign_toplevel_handle_v1_destroy(a->voce);
	g_free(a->titolo);
	g_free(a->titolo_nuovo);
	g_free(a->nome);
	g_free(a);
	ricalcola();
}

gboolean mshell_elenco_app_attiva(const char *const *nomi) {
	if (posto == NULL) {
		return FALSE;
	}
	for (GList *i = app; i; i = i->next) {
		struct app *a = i->data;
		for (int n = 0; a->nome != NULL && nomi[n] != NULL; n++) {
			if (strcmp(a->nome, nomi[n]) == 0) {
				zwlr_foreign_toplevel_handle_v1_activate(a->voce, posto);
				return TRUE;
			}
		}
	}
	return FALSE;
}

static void v_genitore(void *dati, struct zwlr_foreign_toplevel_handle_v1 *v,
		struct zwlr_foreign_toplevel_handle_v1 *genitore) {
}

static const struct zwlr_foreign_toplevel_handle_v1_listener ascolta_voce = {
	.title = v_titolo,
	.app_id = v_app_id,
	.output_enter = v_schermo_entra,
	.output_leave = v_schermo_esce,
	.state = v_stato,
	.done = v_fatto,
	.closed = v_chiusa,
	.parent = v_genitore,
};

static void e_nuova(void *dati, struct zwlr_foreign_toplevel_manager_v1 *elenco,
		struct zwlr_foreign_toplevel_handle_v1 *voce) {
	struct app *a = g_new0(struct app, 1);
	a->voce = voce;
	zwlr_foreign_toplevel_handle_v1_add_listener(voce, &ascolta_voce, a);
	app = g_list_append(app, a);
}

static void e_finito(void *dati, struct zwlr_foreign_toplevel_manager_v1 *elenco) {
}

static const struct zwlr_foreign_toplevel_manager_v1_listener ascolta_elenco = {
	.toplevel = e_nuova,
	.finished = e_finito,
};

static void r_globale(void *dati, struct wl_registry *r, uint32_t nome, const char *interfaccia,
		uint32_t versione) {
	if (strcmp(interfaccia, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
		struct zwlr_foreign_toplevel_manager_v1 *elenco = wl_registry_bind(r, nome,
			&zwlr_foreign_toplevel_manager_v1_interface, MIN(versione, 3));
		zwlr_foreign_toplevel_manager_v1_add_listener(elenco, &ascolta_elenco, NULL);
	}
}

static void r_tolto(void *dati, struct wl_registry *r, uint32_t nome) {
}

static const struct wl_registry_listener ascolta_registro = {
	.global = r_globale,
	.global_remove = r_tolto,
};

void mshell_elenco_app_avvia(void (*cambiato)(const char *titolo)) {
	avvisa = cambiato;
	GdkDisplay *schermo = gdk_display_get_default();
	if (!GDK_IS_WAYLAND_DISPLAY(schermo)) {
		g_warning("non e' Wayland: niente titolo dell'app");
		return;
	}
	posto = gdk_wayland_seat_get_wl_seat(gdk_display_get_default_seat(schermo));
	struct wl_display *d = gdk_wayland_display_get_wl_display(GDK_WAYLAND_DISPLAY(schermo));
	struct wl_registry *r = wl_display_get_registry(d);
	wl_registry_add_listener(r, &ascolta_registro, NULL);
}
