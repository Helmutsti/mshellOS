/* picker: l'osservatore della prova del servizio picker (test/prove/picker.py), e c'e' SOLO
 * per le prove.
 *
 * Sta solo nel disco disco_picker.img (build.sh), in /data/tests: init lo fa partire
 * all'accensione, come sistema. Non chiede niente a nessuno: guarda i file. Nella prova Esempio
 * chiede al picker di salvare "../fuori.txt", e il servizio deve rifiutarlo senza creare niente;
 * questo programma controlla, da fuori, che un file fuori.txt non compaia MAI nei due posti dove
 * finirebbe se il ".." passasse:
 *     /data/users/1000/home/fuori.txt   (".." partendo da Documenti, la cartella della prova)
 *     /data/users/1000/fuori.txt        (".." partendo dalla Home: fuori dalla Home)
 * e scrive quando compare il file salvato davvero (Documenti/prova.txt). Righe sulla seriale:
 *     "prova picker: osservo la Home"            all'inizio
 *     "prova picker: prova.txt c'e'"             quando compare il file salvato
 *     "prova picker: <percorso> c'e' FALLITA"    se compare un fuori.txt
 * Guarda ogni 200 ms per 15 minuti, poi finisce (non e' un servizio: init non lo fa ripartire). */
#include "corekit/corekit.h"
#include <sys/stat.h>

#define CASA "/data/users/1000"

static int c_e(const char *percorso) {
    struct stat s;
    return lstat(percorso, &s) == 0;
}

int main(void) {
    static const char *fuori[] = { CASA "/home/fuori.txt", CASA "/fuori.txt" };
    int visto_salvato = 0, visto_fuori[2] = { 0, 0 };
    scrivi_riga("prova picker", "osservo la Home", "");
    for (int giro = 0; giro < 15 * 60 * 5; giro++) {
        if (!visto_salvato && c_e(CASA "/home/Documenti/prova.txt")) {
            visto_salvato = 1;
            scrivi_riga("prova picker", "prova.txt c'e'", "");
        }
        for (int i = 0; i < 2; i++)
            if (!visto_fuori[i] && c_e(fuori[i])) {
                visto_fuori[i] = 1;
                scrivi_riga("prova picker", fuori[i], " c'e' FALLITA");
            }
        dormi(200);
    }
    scrivi_riga("prova picker", "fine dell'osservazione", "");
    return 0;
}
