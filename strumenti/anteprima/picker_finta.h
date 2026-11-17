/* Una sorgente finta per le scene della finestra Apri/Salva (utente/programmi/picker_finestra.c):
 * la Home del disegno (disegni/ApriSalva/), con le stesse 5 voci: Desktop, Documenti, Immagini
 * (4, 12 e 9 elementi) e appunti.txt (2 KB), lista-spesa.txt (1 KB). Il file "appunti.txt"
 * esiste gia'. Niente disco: l'anteprima non ne ha. */
#include <string.h>
#include "../../utente/programmi/picker_finestra.h"
#include "../../utente/programmi/picker_nome.c"
#include "../../utente/programmi/picker_finestra.c"

static int finta_elenca(void *ctx, const char *percorso, struct picker_voce *v, int max) {
    (void)ctx;
    memset(v, 0, sizeof *v * max);
    if (!percorso[0]) {
        static const char *cartelle[3] = { "Desktop", "Documenti", "Immagini" };
        for (int i = 0; i < 3; i++) { strcpy(v[i].nome, cartelle[i]); v[i].cartella = 1; }
        strcpy(v[3].nome, "appunti.txt");     v[3].byte = 2048;
        strcpy(v[4].nome, "lista-spesa.txt"); v[4].byte = 1024;
        return 5;
    }
    int n = !strcmp(percorso, "Desktop") ? 4 : !strcmp(percorso, "Documenti") ? 12 : 9;
    for (int i = 0; i < n && i < max; i++) strcpy(v[i].nome, "file.txt");
    return n;
}

static int finta_esiste(void *ctx, const char *percorso, const char *nome) {
    (void)ctx;
    return !percorso[0] && !strcmp(nome, "appunti.txt");
}

static const struct picker_sorgente sorgente_finta = { finta_elenca, finta_esiste, 0 };
