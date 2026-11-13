/* impostazioni: la finestra "Impostazioni" del file Figma "mshell - interfaccia" (pagina
 * "App - Impostazioni", finestra 228:53; disegni/Impostazioni/descrizione.md), fatta con i
 * componenti di UIKit.
 *
 * LA FINESTRA e' a aree, come Files (finestra_tk_aree: il corpo ha 8 di padding e 8 tra le
 * aree): a sinistra la barra laterale con le sei sezioni, a destra il pannello chiaro del
 * contenuto (padding 16, 16 tra le righe). Niente barra degli strumenti.
 *
 * LE SEZIONI. Solo "Aspetto" ha un contenuto:
 *   - "Tema": il gruppo M Chiaro / Scuro. All'apertura e' acceso il tema in uso; sceglierne
 *     un altro lo scrive nelle preferenze e lo fa cambiare a tutto il sistema
 *     (uikit_cambia_tema: il compositore lo dice a tutte le finestre e ai pannelli, anche a
 *     questa, che si ridisegna come le altre);
 *   - "Sfondo della scrivania": l'anteprima dello sfondo e il pulsante "Cambia" (per ora non
 *     fa niente).
 * Le altre sezioni si possono scegliere e mostrano il pannello vuoto.
 *
 * LE ICONE delle sezioni sono quelle Lucide del disegno: palette, globe, keyboard, shield,
 * wifi e info. */
#include "uikit/uikit.h"

/* --- l'anteprima dello sfondo ---
 * Nel disegno e' un rettangolo di 120 x 75 con gli angoli RAGGIO_CONTROLLO_NUOVO. Mostra lo
 * sfondo vero della scrivania (il materiale sfondo_piccolo, lo stesso che usa la scrivania),
 * rimpicciolito per riempire l'anteprima senza deformarlo. Lo sfondo e' uguale nei due temi,
 * quindi anche l'anteprima: la si calcola una volta, la prima volta che si disegna. */
#define ANTEPRIMA_L 120
#define ANTEPRIMA_A 75

static unsigned int anteprima_pixel[ANTEPRIMA_L * ANTEPRIMA_A];
static struct immagine anteprima_sfondo;   /* 0 x 0 finche' non e' calcolata */

static void anteprima_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = ANTEPRIMA_L;
    *a = ANTEPRIMA_A;
}

static void anteprima_disegna(struct widget *w, struct pennello *p) {
    if (!anteprima_sfondo.l)
        anteprima_sfondo = immagine_rimpicciolita(&sfondo_piccolo, anteprima_pixel, ANTEPRIMA_L, ANTEPRIMA_A);
    disegna_immagine_arrotondata(p, &anteprima_sfondo, w->x, w->y, RAGGIO_CONTROLLO_NUOVO);
}

static const struct tipo_widget tipo_anteprima = { anteprima_misura, 0, anteprima_disegna, 0, 0, 0, 0, 0, 0 };

/* --- la finestra --- */

static struct finestra_tk finestra;
static struct contenitore corpo;
static struct barra_laterale_tk laterale;
static struct pannello_tk pannello;
static struct contenitore aspetto, riga_tema, riga_sfondo;
static struct etichetta tema, sfondo;
static struct spazio spinta_tema, spinta_sfondo;
static struct pillola temi;
static struct pulsante_barra chiaro, scuro;
static struct widget anteprima;
static struct pulsante cambia;

static int voce_aspetto, scelta;   /* la sezione scelta (numero di voce della barra laterale) */

static int laterale_selezionata(struct barra_laterale_tk *b, int voce, void *dati) {
    (void)b; (void)dati;
    return voce == scelta;
}

static void laterale_clic(struct barra_laterale_tk *b, int voce, void *dati) {
    (void)b; (void)dati;
    if (voce == scelta) return;
    scelta = voce;
    aspetto.base.nascosto = voce != voce_aspetto;
    widget_cambiato(&laterale.base);
    widget_cambiato(&pannello.base);
}

/* Un clic su Chiaro o Scuro: se non e' gia' il tema in uso, lo si cambia a tutto il sistema. */
static void scegli_tema(struct pulsante_barra *b, void *dati) {
    (void)dati;
    int vuole_scuro = b == &scuro;
    if (vuole_scuro == TEMA_SCURO_IN_USO) return;
    uikit_cambia_tema(&finestra, vuole_scuro ? "scuro" : "chiaro");
}

int main(void) {
    /* la barra laterale: le sei sezioni, senza intestazioni */
    barra_laterale_prepara(&laterale, laterale_selezionata, laterale_clic, 0);
    voce_aspetto = barra_laterale_voce(&laterale, "Aspetto", ICONA_PALETTE);
    barra_laterale_voce(&laterale, "Lingua e ora", ICONA_GLOBE);
    barra_laterale_voce(&laterale, "Tastiera e mouse", ICONA_KEYBOARD);
    barra_laterale_voce(&laterale, "App e sicurezza", ICONA_SHIELD);
    barra_laterale_voce(&laterale, "Rete", ICONA_WIFI);
    barra_laterale_voce(&laterale, "Informazioni", ICONA_INFO);
    scelta = voce_aspetto;

    /* Aspetto, riga "Tema": l'etichetta a sinistra, il gruppo M a destra */
    riga_prepara(&riga_tema, SPAZIO_16);
    etichetta_prepara(&tema, "Tema", FONT_CORPO_MEDIO);
    spazio_prepara(&spinta_tema);
    pillola_prepara(&temi, 1);
    pillola_dimensione(&temi, MISURA_M);
    pulsante_barra_testo(&chiaro, "Chiaro", scegli_tema, 0);
    pulsante_barra_testo(&scuro, "Scuro", scegli_tema, 0);
    pillola_aggiungi(&temi, &chiaro);
    pillola_aggiungi(&temi, &scuro);
    pulsante_barra_accendi(TEMA_SCURO_IN_USO ? &scuro : &chiaro, 1);   /* quello in uso */
    widget_aggiungi(&riga_tema.base, &tema.base);
    widget_aggiungi(&riga_tema.base, &spinta_tema.base);
    widget_aggiungi(&riga_tema.base, &temi.base);

    /* riga "Sfondo della scrivania": l'etichetta, l'anteprima e "Cambia" (16 tra i due) */
    riga_prepara(&riga_sfondo, SPAZIO_16);
    etichetta_prepara(&sfondo, "Sfondo della scrivania", FONT_CORPO_MEDIO);
    spazio_prepara(&spinta_sfondo);
    anteprima = (struct widget){ .tipo = &tipo_anteprima };
    pulsante_prepara(&cambia, "Cambia", NESSUNA_ICONA, 0, 0);   /* Secondario M; per ora non fa niente */
    widget_aggiungi(&riga_sfondo.base, &sfondo.base);
    widget_aggiungi(&riga_sfondo.base, &spinta_sfondo.base);
    widget_aggiungi(&riga_sfondo.base, &anteprima);
    widget_aggiungi(&riga_sfondo.base, &cambia.base);

    colonna_prepara(&aspetto, SPAZIO_16);
    widget_aggiungi(&aspetto.base, &riga_tema.base);
    widget_aggiungi(&aspetto.base, &riga_sfondo.base);

    /* il pannello: padding 16, le righe senza padding loro */
    pannello_prepara(&pannello, SPAZIO_16);
    widget_aggiungi(&pannello.base, &aspetto.base);

    riga_prepara(&corpo, CORPO_GAP);
    widget_aggiungi(&corpo.base, &laterale.base);
    widget_aggiungi(&corpo.base, &pannello.base);

    finestra_tk_prepara(&finestra, "Impostazioni", &corpo.base);
    finestra_tk_aree(&finestra);
    finestra_tk_dimensione(&finestra, 800, 520);   /* il disegno */
    return uikit_esegui(&finestra);
}
