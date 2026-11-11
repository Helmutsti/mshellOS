/* La descrizione di un'applicazione: /Applicazioni/<Cartella>/descrizione.
 *
 * Un file di testo, una riga "chiave = valore" per voce; le righe che cominciano con #
 * sono commenti. Le voci:
 *     nome      = come si chiama per la persona (sotto l'icona, nel menu)
 *     programma = quale file della cartella far partire
 *     icona     = un'icona Lucide (settings, plus, square...)
 *     colore    = il fondo dell'icona: neutro, blu, verde, rosso, arancio
 * La leggono la scrivania (per mostrarla) e il launcher (per farla partire). */
#ifndef DESCRIZIONE_H
#define DESCRIZIONE_H

#include "libreria.h"

struct descrizione {
    char nome[32];
    char programma[MAX_PERCORSO];   /* il percorso completo: /Applicazioni/<Cartella>/<programma> */
    char icona[24];
    char colore[16];
};

static inline int descrizione_uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static inline void descrizione_copia(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

static inline void descrizione_aggiungi(char *dove, const char *da, int massimo) {
    int n = lunghezza(dove);
    descrizione_copia(dove + n, da, massimo - n);
}

/* Una riga: si tolgono gli spazi intorno a chiave e valore. */
static inline void descrizione_riga(struct descrizione *d, const char *cartella, char *riga) {
    while (*riga == ' ' || *riga == '\t') riga++;
    if (!*riga || *riga == '#') return;           /* vuota, o un commento */
    char *uguale = riga;
    while (*uguale && *uguale != '=') uguale++;
    if (!*uguale) return;
    char *fine = uguale;
    *uguale = 0;
    while (fine > riga && (fine[-1] == ' ' || fine[-1] == '\t')) *--fine = 0;
    char *valore = uguale + 1;
    while (*valore == ' ' || *valore == '\t') valore++;
    for (int n = lunghezza(valore); n > 0 && (valore[n - 1] == ' ' || valore[n - 1] == '\r'); n--)
        valore[n - 1] = 0;

    if (descrizione_uguali(riga, "nome")) descrizione_copia(d->nome, valore, sizeof d->nome);
    else if (descrizione_uguali(riga, "icona")) descrizione_copia(d->icona, valore, sizeof d->icona);
    else if (descrizione_uguali(riga, "colore")) descrizione_copia(d->colore, valore, sizeof d->colore);
    else if (descrizione_uguali(riga, "programma")) {
        for (const char *c = valore; *c; c++) if (*c == '/') return;   /* solo un file della cartella */
        descrizione_copia(d->programma, "/Applicazioni/", MAX_PERCORSO);
        descrizione_aggiungi(d->programma, cartella, MAX_PERCORSO);
        descrizione_aggiungi(d->programma, "/", MAX_PERCORSO);
        descrizione_aggiungi(d->programma, valore, MAX_PERCORSO);
    }
}

/* Legge la descrizione dell'applicazione nella cartella 'cartella' (solo il nome, per
 * esempio "Contatore"). 0 se c'e' e dice quale programma far partire, -1 se no. */
static inline int leggi_descrizione(const char *cartella, struct descrizione *d) {
    *d = (struct descrizione){ .nome = { 0 } };
    for (const char *c = cartella; *c; c++) if (*c == '/') return -1;   /* una cartella sola */
    descrizione_copia(d->nome, cartella, sizeof d->nome);
    char percorso[MAX_PERCORSO] = "/Applicazioni/";
    descrizione_aggiungi(percorso, cartella, MAX_PERCORSO);
    descrizione_aggiungi(percorso, "/descrizione", MAX_PERCORSO);
    char testo[512];
    int f = apri(percorso, APRI_LEGGERE);
    if (f < 0) return -1;
    int n = leggi(f, testo, sizeof testo - 1);
    chiudi(f);
    if (n <= 0) return -1;
    testo[n] = 0;
    for (char *riga = testo; *riga;) {            /* riga per riga */
        char *dopo = riga;
        while (*dopo && *dopo != '\n') dopo++;
        int ultima = !*dopo;
        *dopo = 0;
        descrizione_riga(d, cartella, riga);
        if (ultima) break;
        riga = dopo + 1;
    }
    return d->programma[0] ? 0 : -1;
}

#endif
