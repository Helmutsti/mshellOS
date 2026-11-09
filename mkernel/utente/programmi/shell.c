/* shell: legge una riga alla volta ed esegue il comando.
 *
 * Gira in ring 3 come tutti i programmi: la lancia init. Per ora un solo comando:
 *   echo TESTO   scrive TESTO */
#include "libreria.h"

#define MAX_RIGA 128

/* "echo ciao" -> "ciao": quello che viene dopo il primo spazio (o niente). */
static const char *dopo_il_comando(const char *riga) {
    while (*riga && *riga != ' ') riga++;
    while (*riga == ' ') riga++;
    return riga;
}

/* Copia in 'comando' la prima parola della riga. */
static void prima_parola(const char *riga, char *comando, int max) {
    int n = 0;
    while (riga[n] && riga[n] != ' ' && n < max - 1) { comando[n] = riga[n]; n++; }
    comando[n] = 0;
}

int main(void) {
    char riga[MAX_RIGA], comando[MAX_RIGA];
    pulisci_schermo();   /* si parte da uno schermo pulito */
    for (;;) {
        scrivi_testo("shell> ");
        if (leggi_riga(riga, MAX_RIGA) < 0) {
            scrivi_testo("shell: la tastiera e' di un altro programma\n");
            return 1;
        }
        prima_parola(riga, comando, MAX_RIGA);
        if (comando[0] == 0) continue;            /* riga vuota */

        if (uguali(comando, "echo")) {
            scrivi_testo(dopo_il_comando(riga));
            scrivi("\n", 1);
        } else {
            scrivi_testo("comando sconosciuto: ");
            scrivi_testo(comando);
            scrivi("\n", 1);
        }
    }
}
