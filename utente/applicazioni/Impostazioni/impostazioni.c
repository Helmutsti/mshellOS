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
 *   - "Sfondo della scrivania": l'anteprima dello sfondo scelto e il pulsante "Cambia", che apre
 *     l'elenco (il menu di menu.h, lo stesso del menu a tendina, a 4 sotto il pulsante) con
 *     "Predefinito" e le foto della cartella /data/users/1000/wallpapers, in ordine
 *     alfabetico, con la scelta attuale spuntata. Scegliere salva la preferenza (sfondo.h),
 *     aggiorna l'anteprima e avvisa la scrivania (stessa notizia del cambio di tema).
 * Le altre sezioni si possono scegliere e mostrano il pannello vuoto.
 *
 * LE ICONE delle sezioni sono quelle Lucide del disegno: palette, globe, keyboard, shield,
 * wifi e info. */
#include "uikit/uikit.h"

/* --- l'anteprima dello sfondo ---
 * Nel disegno e' un rettangolo di 120 x 75 con gli angoli RAGGIO_CONTROLLO_NUOVO. Mostra lo
 * sfondo scelto (la foto, o il materiale sfondo_piccolo per il Predefinito), rimpicciolito per
 * riempire l'anteprima senza deformarlo. Lo sfondo e' uguale nei due temi, quindi anche
 * l'anteprima: la si calcola all'avvio e quando la persona sceglie (anteprima_calcola). */
#define ANTEPRIMA_L 120
#define ANTEPRIMA_A 75

static unsigned int anteprima_pixel[ANTEPRIMA_L * ANTEPRIMA_A];
static struct immagine anteprima_sfondo;   /* 0 x 0 finche' non e' calcolata */

static void anteprima_misura(struct widget *w, int *l, int *a) {
    (void)w;
    *l = ANTEPRIMA_L;
    *a = ANTEPRIMA_A;
}

/* Calcola l'anteprima di 'file' (la foto) o, se e' vuoto o non si legge, del Predefinito. */
static void anteprima_calcola(const char *file) {
    struct immagine foto = { 0 };
    int fatto = 0;
    if (file[0]) {
        char percorso[64 + SFONDO_FILE_MAX] = "/data/users/1000/wallpapers/";
        int n = 0;
        while (percorso[n]) n++;
        for (int i = 0; file[i]; i++) percorso[n++] = file[i];
        percorso[n] = 0;
        if (immagine_carica(percorso, &foto) == 0) {
            anteprima_sfondo = immagine_rimpicciolita(&foto, anteprima_pixel, ANTEPRIMA_L, ANTEPRIMA_A);
            immagine_libera(&foto);
            fatto = 1;
        }
    }
    if (!fatto)
        anteprima_sfondo = immagine_rimpicciolita(&sfondo_piccolo, anteprima_pixel, ANTEPRIMA_L, ANTEPRIMA_A);
}

static void anteprima_disegna(struct widget *w, struct pennello *p) {
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

/* L'elenco degli sfondi: azione 0 = Predefinito, k + 1 = la foto k. */
static struct menu_tk menu_sfondi;
static struct voce_menu_tk voci_sfondi[SFONDI_MAX + 1];
static char file_sfondi[SFONDI_MAX][SFONDO_FILE_MAX];
static char nomi_sfondi[SFONDI_MAX][SFONDO_NOME_MAX];
static int quanti_sfondi;
static int sfondo_scelto;   /* l'azione della voce scelta */

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

static int sfondo_spuntato(int azione, void *dati) {
    (void)dati;
    return azione == sfondo_scelto;
}

/* Una voce scelta: si salva, si aggiorna l'anteprima e si avvisa la scrivania. */
static void sfondo_scelta(struct menu_tk *m, int azione, void *dati) {
    (void)m; (void)dati;
    const char *file = azione > 0 ? file_sfondi[azione - 1] : "";
    if (sfondo_salva(file) < 0) return;
    sfondo_scelto = azione;
    anteprima_calcola(file);
    widget_cambiato(&anteprima);
    uikit_cambia_tema(&finestra, TEMA_SCURO_IN_USO ? "scuro" : "chiaro");   /* la notizia a tutti; il tema resta */
}

static void apri_sfondi(struct pulsante *b, void *dati) {
    (void)dati;
    menu_apri(&menu_sfondi, voci_sfondi, quanti_sfondi + 1, b->base.x, b->base.y + b->base.a + SPAZIO_4);
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
    pulsante_prepara(&cambia, "Cambia", NESSUNA_ICONA, apri_sfondi, 0);   /* Secondario M */

    /* le scelte: Predefinito e le foto; quella nelle preferenze e' la scelta attuale */
    char scelto[SFONDO_FILE_MAX];
    sfondo_leggi(scelto, sizeof scelto);
    quanti_sfondi = sfondo_elenca(file_sfondi, SFONDI_MAX);
    voci_sfondi[0] = (struct voce_menu_tk)TENDINA_VOCE(0, "Predefinito");
    for (int k = 0; k < quanti_sfondi; k++) {
        sfondo_nome_mostrato(file_sfondi[k], nomi_sfondi[k], SFONDO_NOME_MAX);
        voci_sfondi[k + 1] = (struct voce_menu_tk)TENDINA_VOCE(k + 1, nomi_sfondi[k]);
        int uguale = 1;
        for (int i = 0; uguale; i++) { if (file_sfondi[k][i] != scelto[i]) uguale = 0; if (!scelto[i]) break; }
        if (uguale) sfondo_scelto = k + 1;
    }
    if (!sfondo_scelto) scelto[0] = 0;   /* la preferenza punta a una foto che non c'e': Predefinito */
    anteprima_calcola(scelto);
    menu_prepara(&menu_sfondi, MISURA_M, 0, 0, sfondo_spuntato, sfondo_scelta, 0);
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
    menu_aggiungi(&finestra, &menu_sfondi);   /* sta sopra a tutto: dopo il resto */
    finestra_tk_dimensione(&finestra, 800, 520);   /* il disegno */
    return uikit_esegui(&finestra);
}
