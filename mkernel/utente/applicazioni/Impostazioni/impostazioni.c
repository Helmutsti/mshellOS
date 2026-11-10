/* impostazioni: la finestra "Impostazioni" del file Figma "mshell - interfaccia",
 * fatta con i widget del toolkit. Serve a vedere i componenti insieme: titoli, campo
 * di testo, caselle di spunta, scelte singole, pulsanti.
 *
 * Le impostazioni non si salvano ancora da nessuna parte: "Annulla" e "Salva" chiudono
 * la finestra. */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna, temi, azioni;
static struct etichetta account, tema;
static struct campo utente;
static struct casella accedi, notifiche, scuro, chiaro;
static struct spazio spinta, a_destra;
static struct pulsante annulla, salva;
static int gruppo_tema;   /* il suo indirizzo fa da nome del gruppo delle scelte del tema */

static void chiudi_finestra(struct pulsante *p, void *dati) {
    (void)p; (void)dati;
    finestra_tk_chiudi(&finestra);
}

int main(void) {
    colonna_prepara(&colonna, SPAZIO_TRA);
    etichetta_prepara(&account, "Account", FONT_TITOLO);
    campo_prepara(&utente, "mario.rossi", "Nome utente", ICONA_USER, 0, 0);
    casella_prepara(&accedi, "Accedi automaticamente", ACCESO, 0, 0);
    casella_prepara(&notifiche, "Mostra le notifiche a schermo bloccato", SPENTO, 0, 0);
    etichetta_prepara(&tema, "Tema", FONT_TITOLO);
    riga_prepara(&temi, 20);
    scelta_prepara(&scuro, "Vetro scuro", &gruppo_tema, 1, 0, 0);
    scelta_prepara(&chiaro, "Vetro chiaro", &gruppo_tema, 0, 0, 0);
    spazio_prepara(&spinta);                 /* spinge i pulsanti in fondo */
    riga_prepara(&azioni, 8);
    spazio_prepara(&a_destra);               /* ... e a destra */
    pulsante_prepara(&annulla, "Annulla", NESSUNA_ICONA, chiudi_finestra, 0);
    pulsante_prepara(&salva, "Salva", ICONA_SAVE, chiudi_finestra, 0);
    pulsante_stile(&salva, BLU, MEDIO);

    widget_aggiungi(&temi.base, &scuro.base);
    widget_aggiungi(&temi.base, &chiaro.base);
    widget_aggiungi(&azioni.base, &a_destra.base);
    widget_aggiungi(&azioni.base, &annulla.base);
    widget_aggiungi(&azioni.base, &salva.base);
    widget_aggiungi(&colonna.base, &account.base);
    widget_aggiungi(&colonna.base, &utente.base);
    widget_aggiungi(&colonna.base, &accedi.base);
    widget_aggiungi(&colonna.base, &notifiche.base);
    widget_aggiungi(&colonna.base, &tema.base);
    widget_aggiungi(&colonna.base, &temi.base);
    widget_aggiungi(&colonna.base, &spinta.base);
    widget_aggiungi(&colonna.base, &azioni.base);

    finestra_tk_prepara(&finestra, "Impostazioni", &colonna.base);
    finestra_tk_dimensione(&finestra, 460, 380);   /* la misura del disegno */
    return toolkit_esegui(&finestra);
}
