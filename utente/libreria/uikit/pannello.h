/* IL PANNELLO DEL CONTENUTO, dal file Figma ("Pannello contenuto" delle finestre: Files,
 * Terminale). Il contenuto principale di una finestra non sta sul vetro ma in un pannello
 * chiaro incassato: SUPERFICIE_ELEVATA, raggio RAGGIO_PANNELLO. Non ha margine suo: sta nel
 * corpo della finestra a aree (finestra_tk_aree), che gli lascia CORPO_PADDING (8) intorno e
 * CORPO_GAP (8) verso le altre aree.
 *
 * E' il SOLO livello con padding (la regola della spaziatura): 'padding' su tutti i lati
 * (SPAZIO_8 se i figli hanno righe con un fondo e il loro padding, come la tabella; SPAZIO_16 se
 * no, come il Terminale). I figli stanno dentro il padding, senza padding loro. Un figlio che
 * scorre da solo (ha la sua barra di scorrimento: la tabella, la griglia, il terminale) riceve
 * anche il padding destro, come nella pagina (pagina_disponi): la sua barra sta li', non sopra
 * il contenuto.
 *
 * I figli stanno uno sopra l'altro, tutti grandi quanto il pannello: se ne mostra uno alla
 * volta (gli altri nascosti), come le pagine. Il codice e' in interno/pannello.c. */
#ifndef PANNELLO_H
#define PANNELLO_H

struct pannello_tk {
    struct widget base;
    int padding;                       /* su tutti i lati: SPAZIO_8 (con righe) o SPAZIO_16 */
};

/* Elastico e largo: prende tutto lo spazio che avanza. */
void pannello_prepara(struct pannello_tk *p, int padding);

#endif
