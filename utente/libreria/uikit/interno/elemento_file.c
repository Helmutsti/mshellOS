/* UIKit, i componenti: l'elemento di un file (icona e nome sotto) e il testo che va a capo
 * (testo_a_capo, che usano anche l'avviso, lo stato vuoto, la griglia e gli oggetti). Le
 * dichiarazioni sono in elemento_file.h; testo_a_capo in interno/nucleo.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

int testo_a_capo(const struct font *f, const char *testo, int largo,
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

int elemento_righe(const char *nome) {
    char righe[ELEMENTO_RIGHE_NOME][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_NOME_FILE, nome, ELEMENTO_LARGHEZZA - 2 * ELEMENTO_MARGINE, righe,
                         ELEMENTO_RIGHE_NOME);
    return n ? n : 1;
}

int elemento_altezza(const char *nome) {
    return 2 * ELEMENTO_MARGINE + ELEMENTO_ICONA + ELEMENTO_SPAZIO +
           elemento_righe(nome) * FONT_NOME_FILE->altezza;
}

void elemento_fondo(struct pennello *p, int x, int y, int alto, int stato) {
    if (stato == ELEMENTO_NORMALE) return;
    pennello_colore(p, stato == ELEMENTO_STATO_SCELTO ? ELEMENTO_SCELTO : ELEMENTO_SOPRA);
    riempi_tondo(p, x, y, ELEMENTO_LARGHEZZA, alto, RAGGIO_GRANDE);
}

void elemento_icona(struct pennello *p, int x, int y, const struct immagine *icona) {
    disegna_immagine(p, icona, x + (ELEMENTO_LARGHEZZA - ELEMENTO_ICONA) / 2, y + ELEMENTO_MARGINE);
}

int elemento_nome_y(int y) { return y + ELEMENTO_MARGINE + ELEMENTO_ICONA + ELEMENTO_SPAZIO; }

void elemento_nome(struct pennello *p, int x, int y, const char *nome) {
    char righe[ELEMENTO_RIGHE_NOME][MAX_RIGA_NOME];
    int n = testo_a_capo(FONT_NOME_FILE, nome, ELEMENTO_LARGHEZZA - 2 * ELEMENTO_MARGINE, righe,
                         ELEMENTO_RIGHE_NOME);
    pennello_colore(p, CONTENUTO_PRINCIPALE);
    for (int i = 0; i < n; i++)
        testo(p, FONT_NOME_FILE, x + (ELEMENTO_LARGHEZZA - larghezza_testo(FONT_NOME_FILE, righe[i])) / 2,
              elemento_nome_y(y) + i * FONT_NOME_FILE->altezza, righe[i]);
}

void elemento_disegna(struct pennello *p, int x, int y, int alto,
                      const struct immagine *icona, const char *nome, int stato) {
    elemento_fondo(p, x, y, alto, stato);
    elemento_icona(p, x, y, icona);
    elemento_nome(p, x, y, nome);
}
