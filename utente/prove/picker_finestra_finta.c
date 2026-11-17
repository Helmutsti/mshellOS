/* La finestra "Apri/Salva" FINTA, senza grafica: c'e' SOLO per la prova del servizio picker
 * (test/prove/picker.py). Implementa picker_finestra (utente/programmi/picker_finestra.h) al
 * posto della finestra vera (picker_finestra.c): programmi.mk lega picker.c con questo file in
 * un programma a parte, bin/prove/picker_finto, e build.sh lo mette AL POSTO del picker del
 * sistema solo nel disco della prova (disco_picker.img). Nel disco normale c'e' sempre il vero.
 *
 * Non mostra niente e non aspetta nessuno: "sceglie" da sola, leggendo a ogni richiesta il file
 *     /data/tests/window/choice
 * (chiave=valore, # commento; una cartella dentro /data/tests: init fa partire solo i file):
 *     folder=Documenti     la cartella (relativa alla Home) in cui "la persona" sceglie
 *     open=prova.txt       Apri: il file da scegliere in quella cartella
 * Come la finestra vera, usa SOLO la sorgente che le da' il servizio (elenca, esiste): non apre
 * mai file da sola.
 *   Apri:  elenca la cartella e sceglie il file 'open' se c'e' nell'elenco come file;
 *          se non c'e', annulla (come la persona che preme Annulla).
 *   Salva: sceglie la cartella 'folder' con il nome proposto; se il nome non e' valido annulla
 *          (la finestra vera terrebbe spento il pulsante Salva); se il file c'e' gia' lo scrive
 *          nel registro (la finestra vera chiederebbe "Sostituire il file?": qui si' sempre).
 * Ogni scelta la scrive nel registro ("picker finto: ..."): la prova la legge. Senza il file
 * di comando annulla sempre. */
#include "corekit/corekit.h"
#include "../programmi/picker_finestra.h"
#include <stdio.h>
#include <string.h>

#define COMANDO "/data/tests/window/choice"

static char cartella[1024], da_aprire[256];

/* Legge il file di comando. 0, o -1 se non c'e'. */
static int leggi_comando(void) {
    cartella[0] = da_aprire[0] = 0;
    int d = apri(COMANDO, APRI_LEGGERE);
    if (d < 0) return -1;
    static char testo[2048];
    int n = leggi(d, testo, sizeof testo - 1);
    chiudi(d);
    if (n < 0) n = 0;
    testo[n] = 0;
    for (char *riga = testo; riga && *riga;) {
        char *fine = strchr(riga, '\n');
        if (fine) *fine++ = 0;
        char *uguale = strchr(riga, '=');
        if (riga[0] != '#' && uguale) {
            *uguale = 0;
            const char *valore = uguale + 1;
            if (!strcmp(riga, "folder")) snprintf(cartella, sizeof cartella, "%s", valore);
            else if (!strcmp(riga, "open")) snprintf(da_aprire, sizeof da_aprire, "%s", valore);
        }
        riga = fine;
    }
    return 0;
}

static int copia(char *dove, size_t massimo, const char *testo) {
    if (strlen(testo) >= massimo) return -1;
    strcpy(dove, testo);
    return 0;
}

int picker_finestra(enum picker_modo modo, const char *sopra, const char *proposto,
                    const struct picker_sorgente *s,
                    char *percorso, size_t n_percorso, char *nome, size_t n_nome) {
    static struct picker_voce voci[256];
    scrivi_riga("picker finto", modo == PICKER_MODO_SALVA ? "Salva sopra " : "Apri sopra ", sopra);
    if (leggi_comando() < 0) {
        scrivi_riga("picker finto", "manca " COMANDO ": annullo", "");
        return 1;
    }
    int quante = s->elenca(s->ctx, cartella, voci, 256);
    char riga[64];
    snprintf(riga, sizeof riga, "%d voci in ", quante);
    scrivi_riga("picker finto", riga, cartella[0] ? cartella : "(Home)");
    if (quante < 0) return 1;
    if (copia(percorso, n_percorso, cartella) < 0) return -1;
    if (modo == PICKER_MODO_APRI) {
        for (int i = 0; i < quante; i++)
            if (!voci[i].cartella && !strcmp(voci[i].nome, da_aprire)) {
                scrivi_riga("picker finto", "scelgo ", da_aprire);
                return copia(nome, n_nome, da_aprire) < 0 ? -1 : 0;
            }
        scrivi_riga("picker finto", "nell'elenco manca ", da_aprire);
        return 1;
    }
    if (!picker_nome_valido(proposto)) {
        scrivi_riga("picker finto", "nome non valido: annullo", "");
        return 1;
    }
    if (s->esiste(s->ctx, cartella, proposto)) scrivi_riga("picker finto", "sostituisco ", proposto);
    scrivi_riga("picker finto", "salvo come ", proposto);
    return copia(nome, n_nome, proposto) < 0 ? -1 : 0;
}
