/* LA BARRA DI UTILIZZO E LA LEGENDA, dal file Figma (pagina "App - Attivita'", "Barra di
 * utilizzo" e "Legenda") (UIKit, incluso da uikit.h). Come la pagina "Archiviazione" del Mac:
 * quanto e' usato qualcosa (la memoria, il disco), diviso in pezzi. Il codice e' in
 * interno/grafico.c.
 *
 * IL GRAFICO e' l'elenco dei pezzi: nome, tono, quanti byte. La barra e la legenda sono due
 * widget che leggono lo stesso grafico.
 *
 * I TONI. Il disegno e' in scala di grigi: i pezzi si distinguono per quanto sono forti, dal
 * piu' forte al piu' leggero: TESTO_PRIMARIO, TESTO_SECONDARIO, TESTO_DISABILITATO,
 * BORDO_CONTROLLO (i colori del tema: nel chiaro dallo scuro al chiaro, nello scuro al
 * contrario). Il RESTO (la parte libera) non e' un pezzo: e' il fondo della barra.
 *
 * LA BARRA: una pillola alta GRAFICO_SPESSORE (8), fondo SUPERFICIE_INCASSATA, piatta (nel
 * disegno non ha ombre). Ogni pezzo e' la barra intera ritagliata nella sua striscia: gli
 * angoli tondi restano solo ai due capi.
 *
 * LA LEGENDA: una voce per pezzo, alta ALTEZZA_S (32), a SPAZIO_4 l'una dall'altra: il pallino
 * di 8 (il resto: SUPERFICIE_INCASSATA come il fondo della barra), a SPAZIO_8 il nome in
 * FONT_CORPO e TESTO_PRIMARIO, e a destra quanto pesa, FONT_CORPO e TESTO_SECONDARIO. */
#ifndef UIKIT_GRAFICO_H
#define UIKIT_GRAFICO_H

#define MAX_PEZZI_GRAFICO 6
#define NESSUN_PEZZO      (-1)
#define GRAFICO_SPESSORE  8
#define LEGENDA_VOCE      ALTEZZA_S
#define LEGENDA_TRA       SPAZIO_4
#define LEGENDA_PALLINO   8

struct grafico {
    int quanti;
    const char *const *nomi;
    const int *toni;                       /* il tono di ogni pezzo: 0 il piu' forte, 3 il piu' leggero */
    unsigned long long byte[MAX_PEZZI_GRAFICO];
    unsigned long long totale;             /* la barra intera; 0: la somma dei pezzi */
    int resto;                             /* il pezzo che e' il fondo della barra, o NESSUN_PEZZO */
};

/* Scrive in 'd' un numero di byte in testo, come nel disegno: "0,9 MB", "17,6 MB", "1,7 GB",
 * "2 GB" (una cifra dopo la virgola, senza ",0"); sotto un decimo di MB, in KB ("64 KB").
 * Bastano 24 caratteri. Restituisce dove finisce il testo (lo 0 in fondo), per continuare a
 * scrivere di seguito. Non sbaglia mai. */
char *testo_byte(char *d, unsigned long long byte);

/* 1 se i due grafici si vedono uguali con la barra larga 'largo' pixel: stessi numeri nella
 * legenda e pezzi della barra che finiscono negli stessi pixel; 0 se no. Serve a non
 * ridisegnarli se i byte sono cambiati di poco. */
int grafico_uguale(const struct grafico *a, const struct grafico *b, int largo);

/* --- la barra --- */

struct barra_utilizzo { struct widget base; const struct grafico *grafico; };

/* Prepara la barra che disegna il grafico 'g' (che deve restare dov'e'). Prende tutta la
 * larghezza che le si da' (largo). */
void barra_utilizzo_prepara(struct barra_utilizzo *b, const struct grafico *g);

/* --- la legenda --- */

struct legenda { struct widget base; const struct grafico *grafico; };

/* Prepara la legenda del grafico 'g' (che deve restare dov'e'): una voce per pezzo. Prende
 * tutta la larghezza che le si da' (largo). */
void legenda_prepara(struct legenda *l, const struct grafico *g);

#endif
