/* crea_disco: prepara il disco di mkernel, sul computer che compila.
 *
 *     crea_disco IMMAGINE SETTORI /percorso=file [/percorso=file ...]
 *
 * Crea un file IMMAGINE di SETTORI settori da 512 byte, ci scrive un mfs vuoto e ci
 * copia dentro i file indicati (per esempio /Sistema/init=build/utente/init; le cartelle che mancano si creano).
 *
 * IL TRUCCO: non riscrive mfs. Compila INSIEME a questo file lo stesso src/fs/mfs.c del
 * kernel, e gli da' un "disco finto" fatto con il file IMMAGINE (le funzioni disco_*
 * qui sotto). Cosi' il formato del disco e' lo stesso per forza: lo scrive lo stesso
 * codice che poi, nel kernel, lo leggera'. Le cose del kernel che mfs.c usa e che qui
 * non esistono (il lucchetto, kernel.h) le sostituisce strumenti/finto/. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disco.h"
#include "mfs.h"

static FILE *immagine;
static unsigned int settori;

/* --- il disco finto: un settore = 512 byte del file --- */

int disco_leggi(unsigned int n, void *dati) {
    if (n >= settori || fseek(immagine, (long)n * SETTORE, SEEK_SET)) return -1;
    return fread(dati, SETTORE, 1, immagine) == 1 ? 0 : -1;
}

int disco_scrivi(unsigned int n, const void *dati) {
    if (n >= settori || fseek(immagine, (long)n * SETTORE, SEEK_SET)) return -1;
    return fwrite(dati, SETTORE, 1, immagine) == 1 ? 0 : -1;
}

unsigned int disco_settori(void) { return settori; }

/* --- --- */

static void errore(const char *cosa, const char *dettaglio) {
    fprintf(stderr, "crea_disco: %s: %s\n", cosa, dettaglio);
    exit(1);
}

/* Crea le cartelle che mancano lungo il percorso (/Sistema/Servizi/compositore crea
 * /Sistema e /Sistema/Servizi), del sistema, con 0755. */
static void crea_cartelle(const char *percorso) {
    char parziale[256];
    for (int i = 1; percorso[i] && i < (int)sizeof parziale; i++) {
        if (percorso[i] != '/') continue;
        memcpy(parziale, percorso, i);
        parziale[i] = 0;
        int r = mfs_mkdir(parziale, 0, 0755);
        if (r < 0 && r != MFS_ERR_ESISTE) errore(parziale, mfs_messaggio(r));
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: crea_disco IMMAGINE SETTORI /percorso=file[:permessi] "
                        "+/cartella:proprietario:permessi ...\n");
        return 1;
    }
    settori = (unsigned int)strtoul(argv[2], 0, 10);

    /* un file tutto a zero, grande come il disco */
    immagine = fopen(argv[1], "w+b");
    if (!immagine) errore(argv[1], "non riesco a crearlo");
    if (fseek(immagine, (long)settori * SETTORE - 1, SEEK_SET) || fputc(0, immagine) == EOF)
        errore(argv[1], "non riesco a ingrandirlo");

    int r = mfs_formatta();
    if (r < 0) errore("formatta", mfs_messaggio(r));

    for (int i = 3; i < argc; i++) {
        if (argv[i][0] == '+') {   /* una cartella: +/percorso:proprietario:permessi */
            char *percorso = argv[i] + 1, *p1 = strchr(percorso, ':');
            char *p2 = p1 ? strchr(p1 + 1, ':') : 0;
            if (!p2) errore(argv[i], "serve +/cartella:proprietario:permessi");
            *p1 = *p2 = 0;
            unsigned int proprietario = (unsigned int)strtoul(p1 + 1, 0, 10);
            unsigned int permessi = (unsigned int)strtoul(p2 + 1, 0, 8);
            crea_cartelle(percorso);
            r = mfs_mkdir(percorso, proprietario, permessi);
            if (r < 0 && r != MFS_ERR_ESISTE) errore(percorso, mfs_messaggio(r));
            if ((r = mfs_permessi(percorso, proprietario, permessi)) < 0) errore(percorso, mfs_messaggio(r));
            printf("  %-40s cartella  di %u, %03o\n", percorso, proprietario, permessi);
            continue;
        }
        char *uguale = strchr(argv[i], '=');
        if (!uguale) errore(argv[i], "serve /percorso=file");
        *uguale = 0;
        const char *percorso = argv[i], *sorgente = uguale + 1;
        /* i permessi in fondo, in ottale (":644"); se non ci sono, 755: un programma */
        unsigned int permessi = 0755;
        char *due_punti = strrchr(uguale + 1, ':');
        if (due_punti && due_punti[1] >= '0' && due_punti[1] <= '7') {
            *due_punti = 0;
            permessi = (unsigned int)strtoul(due_punti + 1, 0, 8);
        }

        FILE *f = fopen(sorgente, "rb");
        if (!f) errore(sorgente, "non riesco a leggerlo");
        static char dati[1 << 20];
        size_t byte = fread(dati, 1, sizeof dati, f);
        fclose(f);

        crea_cartelle(percorso);
        r = mfs_crea(percorso, dati, (unsigned int)byte, 0, permessi);   /* tutto del sistema */
        if (r < 0) errore(percorso, mfs_messaggio(r));
        printf("  %-40s %7zu byte  %03o\n", percorso, byte, permessi);
    }

    fclose(immagine);
    return 0;
}
