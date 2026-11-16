/* CoreKit, pubblico: catalog, il servizio delle impostazioni della persona (SISTEMA.md, sezione
 * 1). Oggi due: il tema (chiaro o scuro) e lo sfondo. Ogni programma le puo' leggere e farsi
 * avvisare quando cambiano; cambiarle chiede il permesso "settings.system".
 *
 * Catalog e' l'unico che legge e scrive i file delle preferenze (/data/users/<uid>/preferences:
 * "tema" e "sfondo"); gira come la persona. Ascolta sul nome "catalog", che gli presta init.
 *
 * I messaggi seguono lo STANDARD di SISTEMA.md (sezione 3; messaggi.h): testo chiave=valore con
 * la busta (request, id, result). Qui il CONTRATTO; le funzioni in fondo lo nascondono, e
 * stanno in interno/catalog.c. Sullo stesso collegamento si possono fare piu' richieste.
 *
 * LE CHIAVI DELLE IMPOSTAZIONI:
 *     theme       light o dark
 *     wallpaper   il nome di un file in /data/users/<uid>/wallpapers (es. lago.jpg), o vuoto per
 *                 lo sfondo predefinito. Niente "/", non comincia con ".", al massimo
 *                 CATALOG_MAX_SFONDO - 1 byte.
 *
 * LE RICHIESTE (il valore di request):
 *   get     nessun dato. Risposta ok con theme e wallpaper.
 *   set     theme e/o wallpaper, i valori nuovi (quelli che mancano non cambiano). Risposta ok
 *           (con theme e wallpaper come sono ora), invalid (un valore sbagliato, o nessuno dei
 *           due: non cambia niente), denied (manca il permesso), error (il file non si scrive).
 *           Se qualcosa cambia davvero, catalog avvisa chi ha chiesto watch.
 *   watch   nessun dato. Risposta ok con theme e wallpaper; da li', su QUESTO collegamento,
 *           arrivano gli avvisi: un messaggio con notice=changed, theme e wallpaper (tutti e due,
 *           come sono ora), senza id e senza result, ogni volta che qualcosa cambia. Chi
 *           osserva deve leggere gli avvisi: se la sua casella si riempie, catalog chiude il
 *           collegamento (non aspetta nessuno).
 * Una richiesta sconosciuta: invalid.
 *
 * I PERMESSI (ha_permesso, sistema/servizi.h; oggi provvisoria: dice sempre si' e lo scrive
 * nel registro):
 *     get, watch   nessuno: ogni programma legge le impostazioni (APPLICAZIONI.md, sezione 9)
 *     set          settings.system; e in piu', come per CAMBIA_TEMA nel compositore, il
 *                  controllo vero: il sistema (uid 0) o un'app del sistema con
 *                  "settings.system" nel manifesto (pacchetto_ha_permesso) */
#ifndef CATALOG_H
#define CATALOG_H

#include "corekit.h"

#define NOME_CATALOG "catalog"

/* le richieste (il valore di request) */
#define CATALOG_RICHIESTA_LEGGI   "get"
#define CATALOG_RICHIESTA_CAMBIA  "set"
#define CATALOG_RICHIESTA_OSSERVA "watch"

/* le chiavi */
#define CATALOG_CHIAVE_TEMA   "theme"
#define CATALOG_CHIAVE_SFONDO "wallpaper"
#define CATALOG_CHIAVE_AVVISO "notice"
#define CATALOG_AVVISO_CAMBIATE "changed"

/* i valori del tema */
#define CATALOG_TEMA_CHIARO "light"
#define CATALOG_TEMA_SCURO  "dark"

#define CATALOG_MAX_TEMA   8     /* byte del tema, compreso lo 0 */
#define CATALOG_MAX_SFONDO 64    /* byte del nome dello sfondo, compreso lo 0 (come SFONDO_FILE_MAX) */

/* Le impostazioni, come le da' catalog. */
struct catalog_valori {
    char tema[CATALOG_MAX_TEMA];        /* CATALOG_TEMA_CHIARO o CATALOG_TEMA_SCURO */
    char sfondo[CATALOG_MAX_SFONDO];    /* "" = lo sfondo predefinito */
};

/* I valori quando catalog non risponde: tema chiaro, sfondo predefinito. */
void catalog_predefiniti(struct catalog_valori *v);

/* Le impostazioni, in *v. 0, o -1 se catalog non risponde (allora *v ha i predefiniti). */
int catalog_leggi(struct catalog_valori *v);

/* Gli esiti di catalog_cambia, oltre a 0 (fatto) e -1 (catalog non risponde). */
#define CATALOG_NEGATO     (-2)   /* manca il permesso settings.system */
#define CATALOG_NON_VALIDO (-3)   /* un valore sbagliato */
#define CATALOG_ERRORE     (-4)   /* il file non si e' potuto scrivere */

/* Cambia il tema e/o lo sfondo (0: quello non cambia; "" per lo sfondo vuol dire il
 * predefinito). 0, o uno degli esiti qui sopra. */
int catalog_cambia(const char *tema, const char *sfondo);

/* Si fa avvisare quando le impostazioni cambiano. Mette quelle di ora in *v e da' il
 * collegamento da tenere d'occhio (con aspetta, corekit.h) e da leggere con catalog_avviso;
 * -1 se catalog non risponde (*v ha i predefiniti). Il collegamento si chiude con chiudi. */
int catalog_osserva(struct catalog_valori *v);

/* Legge un avviso dal collegamento 'd' di catalog_osserva, e mette in *v le impostazioni
 * nuove. 1 se e' arrivato, 0 se catalog ha chiuso (e' caduto o ripartito: si richiama
 * catalog_osserva), -1 se e' arrivato qualcos'altro. Dorme se non c'e' niente: si chiama
 * quando aspetta dice che 'd' e' pronto. */
int catalog_avviso(int d, struct catalog_valori *v);

#endif
