/* mshell-barra: le parti del programma si parlano attraverso queste dichiarazioni.
 * Il programma e' spiegato in testa a barra.c. */
#pragma once
#include <gtk/gtk.h>

/* LE ICONE (icone.c): disegnate con cairo dai tracciati delle icone Lucide, nel colore del
 * testo del widget (la proprieta' CSS "color"). */
enum mshell_icona { ICONA_CAMPANELLA, ICONA_MENU, ICONA_FRECCIA_SINISTRA, ICONA_FRECCIA_DESTRA,
	ICONA_CERCA };
GtkWidget *mshell_icona_nuova(enum mshell_icona quale);                      /* 16 px */
GtkWidget *mshell_icona_nuova_misura(enum mshell_icona quale, int misura);

/* I NOMI ITALIANI di giorni e mesi (calendario.c): fatti a mano, perche' le traduzioni del
 * sistema (locale) potrebbero non esserci. */
extern const char *const mshell_giorni_corti[7];  /* lunedi' = 0 */
extern const char *const mshell_giorni[7];
extern const char *const mshell_mesi_corti[12];
extern const char *const mshell_mesi[12];
extern const char *const mshell_mesi_maiuscoli[12];
int mshell_giorno_settimana(GDateTime *quando);  /* 0 = lunedi' */

/* I PANNELLI DEI MENU (calendario.c, pannelli.c). */
GtkWidget *mshell_calendario_nuovo(void);
void mshell_calendario_aggiorna(GtkWidget *calendario, GDateTime *adesso, gboolean torna_a_oggi);
GtkWidget *mshell_notifiche_nuove(void);
GtkWidget *mshell_comandi_nuovi(void);

/* L'ELENCO DELLE APP (elenco-app.c): chiama "cambiato" con il titolo dell'app col focus ("" se
 * nessuna) ogni volta che cambia. */
void mshell_elenco_app_avvia(void (*cambiato)(const char *titolo));
/* Se c'e' gia' un'app aperta con uno di questi nomi (app_id; l'elenco finisce con NULL), chiede
 * al compositore di attivarla e da' vero. */
gboolean mshell_elenco_app_attiva(const char *const *nomi);

/* IL CASSETTO DELLE APP (cassetto.c). "cambiato" si chiama quando si apre (TRUE) o si chiude. */
void mshell_cassetto_crea(const char *tema, void (*cambiato)(gboolean aperto));
void mshell_cassetto_apri_chiudi(void);
void mshell_cassetto_minuto(GDateTime *adesso);  /* l'orologio grande */
