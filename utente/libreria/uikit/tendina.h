/* IL MENU A TENDINA, dal file Figma (pagina "Componenti base", "Menu a tendina"; l'elenco e'
 * "Elenco a tendina" in "Componenti complessi"): un campo INCASSATO che mostra la scelta
 * attuale, con una freccia in giu' a destra. Al clic (o con Invio o spazio, se ha il fuoco)
 * apre sotto di se', a 4 pixel (SPAZIO_4) e alla stessa larghezza, l'elenco delle scelte: e'
 * il menu di menu.h (voci M, rientrate di 8, la scelta attuale con la spunta). Si sceglie con
 * il clic o con le frecce e Invio; Esc o un clic fuori lo chiudono senza cambiare niente.
 *
 * Come il campo: tre misure (MISURA_S 32 x 200, MISURA_M 40 x 240, MISURA_L 48 x 280: altezza,
 * margine ai lati, icona, spazio, raggio e font del pulsante, Medium 13/14/16). Il testo e'
 * TESTO_SECONDARIO se non c'e' una scelta (il segnaposto), TESTO_PRIMARIO se c'e'; la freccia
 * e' secondaria col segnaposto, primaria con una scelta. APERTO: stesso fondo, bordo BORDO_FOCUS e anello del fuoco, freccia in
 * su (come il campo con il fuoco, anello e bordo li ha anche chiuso se ha il fuoco).
 * DISABILITATO: piatto, CONTROLLO_SFONDO_DISABILITATO, testo e freccia attenuati, non sente
 * niente.
 *
 * COME SI USA. Le scelte sono voci di menu (voce_menu_tk): 'azione' e' il valore (>= 0),
 * 'testo' quello che si vede; senza icone, scorciatoie o sottomenu (TENDINA_VOCE le scrive cosi'). Il menu e'
 * dentro il widget, ma sta sopra a tutto: si aggiunge alla finestra con tendina_aggiungi_menu,
 * DOPO tutto il resto (come menu_aggiungi).
 *
 *     static const struct voce_menu_tk ordine[] = {
 *         TENDINA_VOCE(0, "Nome"), TENDINA_VOCE(1, "Tipo"),
 *         TENDINA_VOCE(2, "Dimensione"), TENDINA_VOCE(3, "Data"),
 *     };
 *     tendina_prepara(&t, ordine, 4, 0, "Seleziona...", al_cambio, 0);
 *     widget_aggiungi(&riga.base, &t.base);
 *     ...
 *     tendina_aggiungi_menu(&finestra, &t);
 *
 * Il codice e' in interno/tendina.c. */
#ifndef TENDINA_H
#define TENDINA_H

#define TENDINA_SENZA_SCELTA  (-1)
#define TENDINA_STACCO        SPAZIO_4     /* tra il campo e l'elenco */
/* Una scelta: il valore (>= 0) e il testo, senza icona (l'icona 0 esiste: serve NESSUNA_ICONA). */
#define TENDINA_VOCE(valore, testo)  { (valore), (testo), NESSUNA_ICONA, 0, 0, 0 }

struct tendina_tk;
typedef void (*al_cambio_tendina)(struct tendina_tk *t, int valore, void *dati);

struct tendina_tk {
    struct widget base;
    int misura;                            /* MISURA_S, _M (default) o _L */
    const struct voce_menu_tk *voci;       /* le scelte (non si copiano: devono restare) */
    int quante;
    int valore;                            /* l'azione della scelta attuale, o TENDINA_SENZA_SCELTA */
    const char *segnaposto;                /* il testo senza scelta (non si copia) */
    al_cambio_tendina al_cambio;
    void *dati;
    int premuto;
    struct menu_tk menu;                   /* l'elenco */
};

/* Prepara il menu a tendina con le scelte 'voci' ('quante'), quella attuale 'valore' (l'azione
 * di una voce, o TENDINA_SENZA_SCELTA) e il 'segnaposto' da mostrare senza scelta. Quando
 * la persona sceglie, 'al_cambio' (se non e' 0) riceve il nuovo valore, con 'dati'; si chiama
 * anche se sceglie la voce gia' scelta. Misura M. */
void tendina_prepara(struct tendina_tk *t, const struct voce_menu_tk *voci, int quante, int valore,
                     const char *segnaposto, al_cambio_tendina al_cambio, void *dati);

/* La misura (MISURA_S, _M, _L): altezza, margini, font e larghezza di base. */
void tendina_stile(struct tendina_tk *t, int misura);

/* Cambia la scelta attuale (senza chiamare 'al_cambio'). */
void tendina_scegli(struct tendina_tk *t, int valore);

/* La scelta attuale: l'azione della voce, o TENDINA_SENZA_SCELTA. */
int tendina_valore(const struct tendina_tk *t);

/* Aggiunge il menu dell'elenco alla finestra, dopo tutto il resto (sta sopra a tutto). */
void tendina_aggiungi_menu(struct finestra_tk *f, struct tendina_tk *t);

/* Apre l'elenco (se non e' disabilitato); chiuderlo: menu_chiudi(&t->menu). */
void tendina_apri(struct tendina_tk *t);

/* 1 se l'elenco e' aperto. */
int tendina_aperta(const struct tendina_tk *t);

#endif
