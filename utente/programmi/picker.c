/* picker: il servizio che mostrera' la finestra "Apri/Salva" sopra l'app che l'ha chiesta
 * (disegni/ApriSalva/descrizione.md; APPLICAZIONI.md, il servizio picker).
 *
 * QUESTO E' UN PICKER PROVVISORIO, SCRITTO SOLO PER PROVARE IL COMPOSITORE: sara' sostituito
 * dal vero picker (UIKit, la finestra disegnata, il protocollo con le app). Non fa nessuna
 * scelta. Sa fare una cosa sola: se nel disco c'e' la cartella /data/tests/scelta (la mette
 * solo il disco della prova test/prove/scelta.py), apre una finestra di scelta vuota
 * (480 x 421, chiara, titolo "Apri") sopra l'app Files (/system/apps/files/code), la
 * tiene aperta 15 secondi, la chiude, aspetta 10 secondi e ricomincia. (Il disegno dice Esempio,
 * ma una finestra di 320 x 220 sta tutta sotto una di 480 x 421: non si vedrebbe il velo. Un
 * servizio non sa se l'app ha una finestra: se Files non ne ha, il compositore la mette al
 * centro dello schermo. Il ciclo serve alla prova, che apre Files quando vuole e aspetta il
 * giro dopo.)
 * Senza la cartella dorme per sempre: nel disco normale e in tutte le altre prove non fa niente.
 *
 * Gira come la persona (uid 1000, lo fa partire cosi' init). Il compositore accetta la finestra
 * di scelta solo da questo programma (/system/services/picker/picker, RUOLO_SCELTA). */
#include "corekit/corekit.h"
#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"

#define SOPRA     "/system/apps/files/code"
#define MARCATORE "/data/tests/scelta"
#define LARGA 480
#define ALTA  421
#define APERTA_MS 15000
#define CHIUSA_MS 10000

/* Aperta la finestra, la si disegna (chiara, con gli angoli arrotondati) e la si mostra. */
static int disegna(struct finestra *f) {
    unsigned int *pixel = finestra_tela(f);
    if (!pixel) return -1;
    for (int k = 0; k < LARGA * ALTA; k++) pixel[k] = 0;
    struct tela t = { pixel, LARGA, ALTA };
    struct pennello p;
    pennello_prepara(&p, &t);
    pennello_colore(&p, OPACO(0xF4F4F6));
    struct tondo forma = tondo(0, 0, LARGA, ALTA, RAGGIO_PREDEFINITO);
    riempi_forma(&p, &forma);
    return finestra_mostra(f) < 0 ? -1 : 0;
}

int main(void) {
    int d = apri(MARCATORE, APRI_LEGGERE);
    if (d < 0)
        for (;;) dormi(60 * 60 * 1000);       /* niente prova: non fa niente */
    chiudi(d);
    scrivi_riga("picker", "acceso dalla cartella ", MARCATORE);
    dormi(3000);
    for (;;) {
        struct finestra f;
        int aperta = finestra_apri_scelta(&f, LARGA, ALTA, "Apri", SOPRA, RAGGIO_PREDEFINITO);
        if (aperta < 0) scrivi_riga("picker", "il compositore non risponde", "");
        if (aperta == 0 && disegna(&f) == 0) {
            scrivi_riga("picker", "finestra di scelta aperta sopra ", SOPRA);
            unsigned int fine = tempo() + APERTA_MS;
            while (tempo() < fine) {   /* i "ci sei?" del compositore, e basta */
                struct da_guardare g = { .descrittore = f.collegamento };
                struct notizia n;
                if (aspetta(&g, 1, 500) < 0 || (g.pronto && !finestra_ricevi(&f, &n))) break;   /* il compositore ha chiuso */
            }
            finestra_chiudi(&f);
            scrivi_riga("picker", "finestra di scelta chiusa", "");
        } else {
            scrivi_riga("picker", "non riesco ad aprire la finestra di scelta", "");
        }
        dormi(CHIUSA_MS);
    }
}
