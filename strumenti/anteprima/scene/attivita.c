/* L'applicazione Attivita, vera, con dei numeri finti (quelli della pagina "Disegno -
 * Attivita'" del Figma) al posto di quelli del kernel. La pagina si sceglie con la
 * variabile PAGINA=processi|memoria|disco (senza: processi, con la riga di "attivita"
 * scelta e quella di "compositore" sotto il puntatore). Il disco non c'e' (apri dice
 * "non trovato"): Sistema, Applicazioni e Utenti valgono 0 e tutto l'usato e' "Altro". */
#include <stdlib.h>
#include <string.h>

static const struct info_processo finti[] = {
    { 1, 0, 2, 1153434, "init" },          { 2, 0, 3, 18454937, "compositore" },
    { 3, 0, 3, 943718, "launcher" },       { 4, 0, 3, 5557452, "scrivania" },
    { 5, 1000, 1, 3984588, "attivita" },   { 6, 1000, 3, 2516582, "contatore" },
    { 7, 1000, 3, 3040870, "impostazioni" }, { 8, 1000, 2, 629145, "esempio" },
};

int elenco_processi(struct info_processo *v, int max) {
    int n = (int)(sizeof finti / sizeof finti[0]);
    if (n > max) n = max;
    for (int i = 0; i < n; i++) v[i] = finti[i];
    return n;
}
int termina_processo(int numero) { (void)numero; return -1; }
int stato_memoria(struct info_memoria *m) {
    m->totale = 2048ull << 20;
    m->libera = 1735ull << 20;
    m->condivisa = 113ull << 20;
    return 0;
}
int stato_disco(const char *percorso, struct info_disco *d) {
    d->totale = 32ull << 20;
    d->libero = strcmp(percorso, "/data") ? 30ull << 20 : 28ull << 20;
    return 0;
}

#include "../../../utente/applicazioni/Attivita/attivita.c"

#define HA_RITOCCO
static void ritocco(struct finestra_tk *f) {
    (void)f;
    const char *pagina = getenv("PAGINA");
    struct pulsante_barra *b = &scheda_processi;
    if (pagina && !strcmp(pagina, "memoria")) b = &scheda_memoria;
    if (pagina && !strcmp(pagina, "disco")) b = &scheda_disco;
    pulsante_barra_accendi(b, 1);
    mostra_pagina(b, 0);
    if (b == &scheda_processi) {
        tabella.scelto = 5;
        tabella.sotto = 1;
        aggiorna_termina();
    }
}
