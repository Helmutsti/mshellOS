/* La finestra "Apri/Salva" del servizio picker (disegno: disegni/ApriSalva/).
 *
 * Divisione del lavoro:
 * - picker.c (il servizio) sa DOVE sono i file: apre la Home e ogni cartella in modo sicuro
 *   (openat2 + RESOLVE_BENEATH) e passa alla finestra solo l'elenco, tramite la sorgente.
 * - picker_finestra.c (l'interfaccia) mostra la finestra, la navigazione tra cartelle, il
 *   campo Nome, l'avviso "Sostituire il file?" e restituisce cosa ha scelto la persona.
 *   Non apre mai file da sola.
 *
 * I percorsi sono sempre RELATIVI alla Home: "" e' la Home, "Documenti/Lavoro" una
 * sottocartella. Mai "/" all'inizio, mai "." o "..". */
#ifndef PICKER_FINESTRA_H
#define PICKER_FINESTRA_H

#include <stddef.h>

enum picker_modo { PICKER_MODO_APRI, PICKER_MODO_SALVA };

struct picker_voce {
    char nome[256];
    int cartella;       /* 1 cartella, 0 file normale (altri tipi non arrivano mai) */
    long long byte;     /* solo per i file */
};

struct picker_sorgente {
    /* Riempie v (al massimo max voci) con il contenuto della cartella `percorso`,
     * cartelle prima e poi per nome. Torna il numero di voci, -1 se non si puo' leggere. */
    int (*elenca)(void *ctx, const char *percorso, struct picker_voce *v, int max);
    /* 1 se in `percorso` esiste gia' un file `nome`, 0 se no. */
    int (*esiste)(void *ctx, const char *percorso, const char *nome);
    void *ctx;
};

/* Mostra la finestra sopra il programma `sopra` (uikit_esegui_scelta) e aspetta la persona.
 * In Salva `proposto` e' il nome iniziale del campo; il pulsante Salva e' spento se il nome
 * non e' valido; se il file esiste, chiede "Sostituire il file?" (Annulla torna alla finestra).
 * Torna 0 se la persona ha scelto (cartella in `percorso`, nome del file in `nome`),
 * 1 se ha annullato (Annulla, Esc, chiudi), -1 se la finestra non si e' potuta mostrare. */
int picker_finestra(enum picker_modo modo, const char *sopra, const char *proposto,
                    const struct picker_sorgente *s,
                    char *percorso, size_t n_percorso, char *nome, size_t n_nome);

/* Regola del nome valido, la stessa per la finestra e per il servizio:
 * non vuoto, al massimo 255 byte, senza "/", non "." o "..", senza caratteri di controllo. */
int picker_nome_valido(const char *nome);

#endif
