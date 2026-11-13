/* LE INFORMAZIONI, dal file Figma ("Finestra mini - Informazioni", nella sezione "Finestra" di
 * "Componenti complessi"): le informazioni su una cosa (un file), in una MINI FINESTRA a parte,
 * larga INFORMAZIONI_LARGHEZZA (320): la barra del titolo mini (28, il titolo "etichetta" e la
 * sola X), sotto il contenuto con SPAZIO_16 di padding (quello della mini finestra) e le righe a
 * SPAZIO_8 l'una dall'altra. Ogni riga e' alta INTERLINEA_PICCOLO (16): a sinistra il nome in
 * "piccolo" TESTO_SECONDARIO, a destra il valore in "piccolo" TESTO_PRIMARIO (tagliato con
 * "..." se non ci sta). Il codice e' in interno/informazioni.c e interno/informazioni_mostra.c.
 *
 *     static struct finestra_tk mini;
 *     static struct informazioni_tk info;
 *     informazioni_prepara(&mini, &info);
 *     informazioni_riga(&info, "Dimensione", "412 KB"); ...
 *     informazioni_mostra(&finestra, &mini, &info, "Informazioni - foto.png");
 *
 * Si chiude con la sua X o con Esc. */
#ifndef UIKIT_INFORMAZIONI_H
#define UIKIT_INFORMAZIONI_H

#define INFORMAZIONI_LARGHEZZA 320
#define INFORMAZIONI_TRA       SPAZIO_8
#define MAX_RIGHE_INFORMAZIONI 8

struct informazioni_tk {
    struct widget base;
    char etichette[MAX_RIGHE_INFORMAZIONI][MAX_TESTO_WIDGET];
    char valori[MAX_RIGHE_INFORMAZIONI][MAX_CELLA];
    int quante;
};

/* Prepara la mini finestra 'mini' con dentro le informazioni 'in' (vuote). La finestra non si
 * apre ancora: la apre informazioni_mostra. */
void informazioni_prepara(struct finestra_tk *mini, struct informazioni_tk *in);

/* Toglie tutte le righe. */
void informazioni_svuota(struct informazioni_tk *in);

/* Aggiunge in fondo una riga: il nome e il valore (tagliati a MAX_TESTO_WIDGET e MAX_CELLA).
 * Oltre MAX_RIGHE_INFORMAZIONI righe non aggiunge niente. */
void informazioni_riga(struct informazioni_tk *in, const char *etichetta, const char *valore);

/* Apre (o riapre, con le righe nuove) la mini finestra, come l'altra finestra di 'principale'
 * (finestra_tk_apri_altra). Il titolo, se e' piu' lungo di quanto ci sta, finisce con "...".
 * Restituisce 0 se la finestra e' aperta, -1 se no (come finestra_tk_apri_altra). */
int informazioni_mostra(struct finestra_tk *principale, struct finestra_tk *mini,
                        struct informazioni_tk *in, const char *titolo);

#endif
