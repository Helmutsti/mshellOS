/* L'ELEMENTO DI UN FILE nella vista a icone: l'icona grande e sotto il nome, come nel
 * disegno Figma ("Elemento file"). Lo usano Files (le cartelle e i file di una cartella)
 * e la scrivania (la cartella Home in alto a sinistra): stesso disegno, stesse misure.
 *
 *     +--------------+   largo ELEMENTO_LARGHEZZA (84), angoli di 10
 *     |   [icona]    |   l'icona (56) in alto, a 8 dal bordo
 *     |  nome del    |   il nome sotto, centrato, al massimo su due righe; se non ci sta
 *     |  file lun... |   la seconda finisce con "..."
 *     +--------------+
 *
 * Gli stati: normale (niente fondo), sotto il puntatore (ELEMENTO_SOPRA, appena velato),
 * scelto (ELEMENTO_SCELTO, vetro chiaro). L'icona e' un'immagine a colori dei materiali del
 * sistema (gli oggetti, oggetti.h): i file sono "contenuto", non icone dell'interfaccia. */
#ifndef ELEMENTO_FILE_H
#define ELEMENTO_FILE_H

#include "stile.h"

#define ELEMENTO_NORMALE 0
#define ELEMENTO_STATO_SOPRA  1
#define ELEMENTO_STATO_SCELTO 2

#define MAX_RIGA_NOME 96     /* lettere di una riga del nome, compreso lo 0 finale */

/* Quanto e' largo un pezzo di testo (da 'da' a 'a', escluso). */
static inline int larghezza_pezzo(const struct font *f, const char *s, int da, int a) {
    int l = 0;
    for (int i = da; i < a; i++) l += glifo_di(f, (unsigned char)s[i])->avanzo;
    return l;
}

/* Spezza 'testo' in righe larghe al massimo 'largo' pixel, al massimo 'massimo' righe:
 * quante righe sono uscite. Si va a capo dopo uno spazio o un trattino (come fa Figma:
 * "foto-" / "mare.png"); una parola piu' lunga della riga si spezza dove capita. Se il
 * testo non ci sta tutto, l'ultima riga finisce con "...". */
static inline int testo_a_capo(const struct font *f, const char *testo, int largo,
                               char righe[][MAX_RIGA_NOME], int massimo) {
    int pos = 0, n = 0, puntini = larghezza_testo(f, "...");
    while (testo[pos] == ' ') pos++;
    while (testo[pos] && n < massimo) {
        int fine = pos, l = 0;
        while (testo[fine] && fine - pos < MAX_RIGA_NOME - 4) {
            int c = glifo_di(f, (unsigned char)testo[fine])->avanzo;
            if (l + c > largo) break;
            l += c;
            fine++;
        }
        if (fine == pos) fine++;                 /* una lettera piu' larga della riga */
        int taglio = fine;
        if (!testo[fine]) {
            taglio = fine;                       /* il resto ci sta tutto */
        } else if (n == massimo - 1) {           /* l'ultima riga: si taglia con i puntini */
            while (fine > pos && larghezza_pezzo(f, testo, pos, fine) + puntini > largo) fine--;
            int k = 0;
            for (int i = pos; i < fine; i++) righe[n][k++] = testo[i];
            righe[n][k++] = '.'; righe[n][k++] = '.'; righe[n][k++] = '.';
            righe[n][k] = 0;
            return n + 1;
        } else {
            for (int k = fine; k > pos; k--)
                if (testo[k - 1] == ' ' || testo[k - 1] == '-') { taglio = k; break; }
        }
        int k = 0, ultimo = taglio;
        while (ultimo > pos && testo[ultimo - 1] == ' ') ultimo--;   /* senza lo spazio in fondo */
        for (int i = pos; i < ultimo; i++) righe[n][k++] = testo[i];
        righe[n][k] = 0;
        n++;
        pos = taglio;
        while (testo[pos] == ' ') pos++;
    }
    return n;
}

/* Quante righe prende il nome sotto l'icona (1 o 2). */
static inline int elemento_righe(const char *nome) {
    char righe[ELEMENTO_RIGHE_NOME][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_NOME_FILE, nome, ELEMENTO_LARGHEZZA - 2 * ELEMENTO_MARGINE, righe,
                         ELEMENTO_RIGHE_NOME);
    return n ? n : 1;
}

/* L'altezza dell'elemento con quel nome: margine, icona, spazio, le righe, margine. */
static inline int elemento_altezza(const char *nome) {
    return 2 * ELEMENTO_MARGINE + ELEMENTO_ICONA + ELEMENTO_SPAZIO +
           elemento_righe(nome) * FONT_NOME_FILE->altezza;
}

/* Il fondo dell'elemento (sotto il puntatore o scelto), alto 'alto'. */
static inline void elemento_fondo(struct pennello *p, int x, int y, int alto, int stato) {
    if (stato == ELEMENTO_NORMALE) return;
    pennello_colore(p, stato == ELEMENTO_STATO_SCELTO ? ELEMENTO_SCELTO : ELEMENTO_SOPRA);
    riempi_tondo(p, x, y, ELEMENTO_LARGHEZZA, alto, RAGGIO_GRANDE);
}

/* L'icona dell'elemento con l'angolo in (x, y). */
static inline void elemento_icona(struct pennello *p, int x, int y, const struct immagine *icona) {
    disegna_immagine(p, icona, x + (ELEMENTO_LARGHEZZA - ELEMENTO_ICONA) / 2, y + ELEMENTO_MARGINE);
}

/* Dove comincia il nome (la sua prima riga). */
static inline int elemento_nome_y(int y) { return y + ELEMENTO_MARGINE + ELEMENTO_ICONA + ELEMENTO_SPAZIO; }

/* Tutto l'elemento: fondo, icona e nome. 'alto' e' l'altezza del suo posto (in una riga
 * di elementi, quella del piu' alto): il fondo la prende tutta. */
static inline void elemento_disegna(struct pennello *p, int x, int y, int alto,
                                    const struct immagine *icona, const char *nome, int stato) {
    elemento_fondo(p, x, y, alto, stato);
    elemento_icona(p, x, y, icona);
    char righe[ELEMENTO_RIGHE_NOME][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_NOME_FILE, nome, ELEMENTO_LARGHEZZA - 2 * ELEMENTO_MARGINE, righe,
                         ELEMENTO_RIGHE_NOME);
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    for (int i = 0; i < n; i++)
        testo(p, FONT_NOME_FILE, x + (ELEMENTO_LARGHEZZA - larghezza_testo(FONT_NOME_FILE, righe[i])) / 2,
              elemento_nome_y(y) + i * FONT_NOME_FILE->altezza, righe[i]);
}

#endif
