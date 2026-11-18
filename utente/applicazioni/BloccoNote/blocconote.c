/* blocconote: l'editor di testo grafico (PIANO_EDITOR.md). Una finestra con l'area di testo
 * (uikit/area_testo.h) e, nella barra degli strumenti, quattro pulsanti solo icona come nel disegno
 * (disegni/BloccoNote/): Nuovo, Apri, Salva, Salva con nome.
 *
 * L'APP NON HA PERMESSI SUI FILE (manifest: permessi = []). Ogni file le arriva dal picker, la
 * finestra Apri/Salva del sistema (corekit/picker.h), che le da' SOLO il file scelto, gia' aperto:
 * in lettura per Apri, in scrittura (creato o svuotato) per Salva con nome. Mentre la persona sceglie,
 * l'app resta ferma dentro la chiamata (e' il modo del picker).
 *
 *   Nuovo         documento vuoto, "Senza titolo".
 *   Apri          legge il file scelto come testo UTF-8 nell'area di testo.
 *   Salva con nome  chiede dove salvare e scrive il testo. Anche Salva, se il documento e' nuovo.
 *   Salva         su un documento aperto con Apri: ANCORA NON FA NIENTE. Il file di Apri e' aperto in
 *                 sola lettura e l'app non puo' riaprirlo da se': aspetta la decisione su come darle
 *                 la scrittura (picker che consegna anche la scrittura, o Salva che ripassa dal
 *                 picker). Per ora si salva con Salva con nome.
 *
 * MODIFICHE NON SALVATE: la prima volta che la persona cambia il testo (area_testo_al_cambiato) il
 * titolo prende un asterisco subito dopo il nome, "nome* - Blocco note", che sparisce quando si salva.
 * Premendo Nuovo o Apri con modifiche non salvate l'app chiede "Salvare le modifiche?" con l'avviso a
 * tre scelte di UIKit: Non salvare, Annulla, Salva. Chiudere la finestra NON chiede niente (non e'
 * richiesto): le modifiche si perdono.
 *
 * Le misure sono quelle del disegno: finestra 760 x 588 a aree (corpo con 8 di padding e 8 tra le
 * aree), la barra degli strumenti con quattro pulsanti solo icona (Secondario, S), e sotto l'area di
 * testo, che e' il pannello chiaro incassato e riempie il resto. */
#include "uikit/uikit.h"
#include "corekit/picker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LARGHEZZA 760
#define ALTEZZA 588
#define NOME_MASSIMO 256                 /* il nome di un file (come il picker) */
#define FILE_MASSIMO (8 << 20)           /* un file piu' grande di 8 MiB non si apre */
#define SENZA_TITOLO "Senza titolo"
#define SENZA_TITOLO_FILE "Senza titolo.txt"   /* il nome che si propone a Salva con nome, come nel disegno */

static struct finestra_tk finestra;
static struct barra_strumenti strumenti;
static struct pulsante p_nuovo, p_apri, p_salva, p_salva_nome;
static struct area_testo area;
static struct avviso salvare;

/* Il documento: il nome (vuoto: nuovo, "Senza titolo"), se e' stato aperto con Apri (ha un file) e se
 * ha modifiche non salvate. */
static char nome[NOME_MASSIMO];
static int ha_file, modificato;

/* Cosa fare dopo la risposta all'avviso "Salvare le modifiche?". */
enum { DOPO_NIENTE, DOPO_NUOVO, DOPO_APRI };
static int dopo;

static void titolo_aggiorna(void) {
    char t[NOME_MASSIMO + 40];
    snprintf(t, sizeof t, "%s%s \xe2\x80\x93 Blocco note",        /* U+2013, il trattino lungo */
             nome[0] ? nome : SENZA_TITOLO, modificato ? "*" : "");
    finestra_tk_titolo(&finestra, t);
}

static void documento(const char *nuovo_nome, int con_file) {
    snprintf(nome, sizeof nome, "%s", nuovo_nome);
    ha_file = con_file;
    modificato = 0;
    titolo_aggiorna();
}

/* La persona ha cambiato il testo: da ora ci sono modifiche non salvate. */
static void al_cambiato(struct area_testo *a, void *dati) {
    (void)a; (void)dati;
    if (modificato) return;
    modificato = 1;
    titolo_aggiorna();
}

static void fai_nuovo(void) {
    area_testo_imposta(&area, "");
    documento("", 0);
    finestra_tk_dai_fuoco(&finestra, &area.base);       /* dopo il clic sul pulsante si scrive subito */
}

/* Legge tutto il file 'd' (e lo chiude) e lo mette nell'area di testo. 0 se non ci riesce (allora il
 * documento non cambia). */
static int leggi_nel_documento(int d, const char *nome_file) {
    int cap = 4096, n = 0, k;
    char *b = malloc((size_t)cap + 1);
    if (!b) { chiudi(d); return 0; }
    while ((k = leggi(d, b + n, (unsigned)(cap - n))) > 0) {
        n += k;
        if (n < cap) continue;
        if (cap >= FILE_MASSIMO) { n = -1; break; }
        char *nb = realloc(b, (size_t)cap * 2 + 1);
        if (!nb) { n = -1; break; }
        b = nb;
        cap *= 2;
    }
    chiudi(d);
    if (k < 0 || n < 0) { free(b); return 0; }
    b[n] = 0;
    area_testo_imposta(&area, b);
    free(b);
    documento(nome_file, 1);
    finestra_tk_dai_fuoco(&finestra, &area.base);
    return 1;
}

static void fai_apri(void) {
    char scelto[NOME_MASSIMO];
    int d = picker_apri(scelto, sizeof scelto);
    if (d < 0) return;                    /* annullato (o il picker non risponde): tutto com'era */
    leggi_nel_documento(d, scelto);
}

/* Scrive il testo nel file 'd' (e lo chiude). 1 se e' andato tutto. */
static int scrivi_dal_documento(int d) {
    int n = area_testo_lunghezza(&area);
    char *b = malloc((size_t)n + 1);
    if (!b) { chiudi(d); return 0; }
    area_testo_copia(&area, b, n + 1);
    int fatti = 0, ok = 1;
    while (fatti < n) {
        int k = scrivi_in(d, b + fatti, (unsigned)(n - fatti));
        if (k <= 0) { ok = 0; break; }
        fatti += k;
    }
    chiudi(d);
    free(b);
    return ok;
}

/* Salva con nome: la finestra Salva del picker (che chiede da se' "Sostituire il file?"). 1 se il
 * testo e' stato salvato. */
static int salva_con_nome(void) {
    char scelto[NOME_MASSIMO];
    int d = picker_salva(nome[0] ? nome : SENZA_TITOLO_FILE, scelto, sizeof scelto);
    if (d < 0) return 0;
    if (!scrivi_dal_documento(d)) return 0;
    documento(scelto, 1);
    return 1;
}

/* Salva. Documento nuovo: come Salva con nome. Documento aperto con Apri: ANCORA NIENTE (0), vedi in
 * cima: aspetta la decisione su come dare all'app la scrittura. */
static int salva(void) {
    if (!ha_file) return salva_con_nome();
    return 0;
}

/* Fa Nuovo o Apri; se ci sono modifiche non salvate chiede prima "Salvare le modifiche?". */
static void con_controllo(int cosa) {
    if (!modificato) {
        if (cosa == DOPO_NUOVO) fai_nuovo(); else fai_apri();
        return;
    }
    dopo = cosa;
    char m[NOME_MASSIMO + 64];
    snprintf(m, sizeof m, "Le modifiche a %s andranno perse se non le salvi.", nome[0] ? nome : SENZA_TITOLO);
    snprintf(salvare.messaggio, sizeof salvare.messaggio, "%s", m);
    avviso_apri(&salvare);
}

/* I tre pulsanti dell'avviso, da sinistra: Non salvare (AVVISO_ANNULLA), Annulla (AVVISO_ALTERNATIVA),
 * Salva (AVVISO_CONFERMA). */
static void risposta_salvare(struct avviso *v, int scelta, void *dati) {
    (void)v; (void)dati;
    int cosa = dopo;
    dopo = DOPO_NIENTE;
    if (scelta == AVVISO_ALTERNATIVA) return;                  /* Annulla: resta tutto com'e' */
    if (scelta == AVVISO_CONFERMA && !salva()) return;         /* Salva: se non salva, non si va avanti */
    if (cosa == DOPO_NUOVO) fai_nuovo(); else if (cosa == DOPO_APRI) fai_apri();
}

static void al_nuovo(struct pulsante *p, void *dati) { (void)p; (void)dati; con_controllo(DOPO_NUOVO); }
static void al_apri(struct pulsante *p, void *dati) { (void)p; (void)dati; con_controllo(DOPO_APRI); }
static void al_salva(struct pulsante *p, void *dati) { (void)p; (void)dati; salva(); }
static void al_salva_nome(struct pulsante *p, void *dati) { (void)p; (void)dati; salva_con_nome(); }

/* Un pulsante solo icona della barra: Secondario, S, con il suo suggerimento (obbligatorio sui
 * pulsanti senza testo). */
static void pulsante_della_barra(struct pulsante *p, int icona, const char *suggerimento,
                                 al_clic_pulsante al_clic) {
    pulsante_prepara(p, "", icona, al_clic, 0);
    pulsante_stile(p, SECONDARIO, MISURA_S);
    widget_suggerimento(&p->base, suggerimento, 0);
    barra_strumenti_aggiungi(&strumenti, &p->base);
}

int main(void) {
    barra_strumenti_prepara(&strumenti);
    pulsante_della_barra(&p_nuovo, ICONA_PLUS, "Nuovo", al_nuovo);
    pulsante_della_barra(&p_apri, ICONA_FOLDER, "Apri", al_apri);
    pulsante_della_barra(&p_salva, ICONA_SAVE, "Salva", al_salva);
    pulsante_della_barra(&p_salva_nome, ICONA_FILE_PEN, "Salva con nome", al_salva_nome);

    area_testo_prepara(&area);
    area_testo_segnaposto(&area, "Scrivi qui\xe2\x80\xa6");       /* U+2026, i tre puntini */
    area_testo_al_cambiato(&area, al_cambiato, 0);

    /* "Salvare le modifiche?": l'avviso di UIKit a tre scelte, bloccante. Il pulsante piu' a sinistra
     * e' di solito Annulla: qui lo si chiama "Non salvare", "Annulla" e' quello di mezzo, e Esc deve
     * dare Annulla (avviso_esc), non perdere il lavoro. */
    avviso_prepara(&salvare, "Salvare le modifiche?", "", "Salva", PRIMARIO, 1, risposta_salvare, 0);
    avviso_annulla(&salvare, "Non salvare");
    avviso_alternativa(&salvare, "Annulla", SECONDARIO);
    avviso_esc(&salvare, AVVISO_ALTERNATIVA);

    finestra_tk_prepara(&finestra, SENZA_TITOLO " \xe2\x80\x93 Blocco note", &area.base);
    finestra_tk_barra(&finestra, &strumenti.base);
    finestra_tk_aree(&finestra);
    finestra_tk_avviso(&finestra, &salvare);
    finestra_tk_dimensione(&finestra, LARGHEZZA, ALTEZZA);
    finestra_tk_dai_fuoco(&finestra, &area.base);       /* si scrive subito */
    return uikit_esegui(&finestra);
}
