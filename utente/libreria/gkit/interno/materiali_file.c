/* GKit: i materiali gia' calcolati (font, icone, immagini) da un file su disco. Le
 * dichiarazioni e il perche' sono in gkit_disegno.h; com'e' fatto ogni file e' scritto in
 * strumenti/grafica/formato.py, che li scrive.
 *
 * Il file si mette in memoria con mmap, in sola lettura, e si usa dove sta: le tabelle
 * (struct font, struct icona, struct immagine) puntano dentro il file. I programmi che usano
 * lo stesso file ne condividono le pagine (le tiene Linux, nella sua cache dei file), e un
 * programma legge davvero dal disco solo i pezzi che disegna. Non si toglie mai: un
 * materiale resta finche' il programma vive.
 *
 * Prima di usarlo, il file si controlla tutto: le lettere, il numero di versione, che ogni
 * tabella stia dentro il file. Un file che manca o e' rovinato non fa cadere il programma:
 * si scrive perche' sullo standard error (il registro del programma) e si da' qualcosa che
 * si disegna senza pericolo (vedi ogni funzione).
 *
 * Usa solo le funzioni di Linux (open, fstat, mmap): niente CoreKit, cosi'
 * gira anche fuori da mshell (l'anteprima, sul Mac). */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gkit/gkit_disegno.h"

#define VERSIONE_MATERIALI 1
#define FONT_TESTA         272     /* lettere, versione, misure, conti, indice[256] */
#define GLIFO_BYTE         12
#define IMMAGINE_TESTA     16

_Static_assert(sizeof(struct glifo) == GLIFO_BYTE, "struct glifo deve essere come nel file .font");

/* --- la cartella delle risorse --- */

const char *risorse_cartella(void) {
    const char *r = getenv("MSHELL_RISORSE");
    return r && *r ? r : RISORSE_CARTELLA;
}

int percorso_risorsa(char *dove, int spazio, const char *nome) {
    const char *pezzi[3] = { risorse_cartella(), "/", nome };
    int n = 0;
    for (int i = 0; i < 3; i++)
        for (const char *c = pezzi[i]; *c; c++) {
            if (n >= spazio - 1) { dove[n] = 0; return -1; }
            dove[n++] = *c;
        }
    dove[n] = 0;
    return 0;
}

/* --- leggere e controllare --- */

static void scrivi_errore(const char *s) {
    ssize_t r = write(2, s, strlen(s));
    (void)r;
}

/* Il messaggio, una riga: "gkit: non riesco a usare <percorso>: <perche'>". */
static void avvisa(const char *percorso, const char *perche) {
    scrivi_errore("gkit: non riesco a usare ");
    scrivi_errore(percorso);
    scrivi_errore(": ");
    scrivi_errore(perche);
    scrivi_errore("\n");
}

/* Il file intero in memoria, in sola lettura; 0 se non si puo' (e allora *perche' dice cosa). */
static const unsigned char *mappa(const char *percorso, unsigned long *quanti, const char **perche) {
    int d = open(percorso, O_RDONLY | O_CLOEXEC);
    if (d < 0) { *perche = "il file non c'e' o non si puo' leggere"; return 0; }
    struct stat s;
    void *m = MAP_FAILED;
    if (fstat(d, &s) < 0 || s.st_size <= 0) *perche = "il file e' vuoto";
    else if ((m = mmap(0, (size_t)s.st_size, PROT_READ, MAP_PRIVATE, d, 0)) == MAP_FAILED)
        *perche = "non si mette in memoria (mmap)";
    close(d);
    if (m == MAP_FAILED) return 0;
    *quanti = (unsigned long)s.st_size;
    return m;
}

static unsigned int u16(const unsigned char *p) { return p[0] | (unsigned int)p[1] << 8; }
static unsigned int u32(const unsigned char *p) {
    return p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 | (unsigned int)p[3] << 24;
}

/* Le prime 6 byte: le 4 lettere e la versione giusta. */
static int testa_giusta(const unsigned char *m, unsigned long n, const char *lettere, const char **perche) {
    if (n < 8 || memcmp(m, lettere, 4)) { *perche = "non e' un file di questo tipo"; return 0; }
    if (u16(m + 4) != VERSIONE_MATERIALI) { *perche = "versione del formato sconosciuta"; return 0; }
    return 1;
}

/* --- il font di riserva: ogni lettera e' un rettangolo vuoto (come i font veri per un
 * carattere che non hanno), lo spazio e' uno spazio. Si vede che il testo c'e', e che il
 * font manca. --- */

#define RISERVA_L 6
#define RISERVA_A 10
static const unsigned char riserva_alfa[RISERVA_L * RISERVA_A] = {
    255, 255, 255, 255, 255, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255,   0,   0,   0,   0, 255,
    255, 255, 255, 255, 255, 255,
};
static const struct glifo riserva_glifi[2] = {
    { 0, 0, 0, 0, 0, 4 },                                   /* lo spazio */
    { 0, 1, RISERVA_A, RISERVA_L, RISERVA_A, RISERVA_L + 2 }, /* ogni altro carattere */
};

static void font_di_riserva(struct font *f) {
    f->glifi = riserva_glifi;
    f->alfa = riserva_alfa;
    for (int c = 0; c < 256; c++) f->indice[c] = c == ' ' ? 0 : 1;
    f->altezza = 16;
    f->ascesa = 12;
}

/* --- i tre tipi di file --- */

int font_da_file(const char *percorso, struct font *f) {
    const char *perche = 0;
    unsigned long n = 0;
    const unsigned char *m = mappa(percorso, &n, &perche);
    if (m && testa_giusta(m, n, "MSFN", &perche)) {
        unsigned long glifi = n >= FONT_TESTA ? u32(m + 8) : 0, alfa = n >= FONT_TESTA ? u32(m + 12) : 0;
        if (n < FONT_TESTA || glifi < 1 || glifi > 256 || n != FONT_TESTA + glifi * GLIFO_BYTE + alfa)
            perche = "la lunghezza del file non torna con quello che dice la testa";
        else {
            const struct glifo *g = (const struct glifo *)(m + FONT_TESTA);
            for (unsigned long i = 0; i < glifi && !perche; i++)
                if (g[i].inizio + (unsigned long)g[i].l * g[i].a > alfa) perche = "una lettera esce dal file";
            for (int c = 0; c < 256 && !perche; c++)
                if (m[16 + c] >= glifi) perche = "l'indice dei caratteri punta fuori";
            if (!perche) {
                f->glifi = g;
                f->alfa = m + FONT_TESTA + glifi * GLIFO_BYTE;
                memcpy(f->indice, m + 16, 256);
                f->altezza = m[6];
                f->ascesa = m[7];
                return 0;
            }
        }
    }
    if (m) munmap((void *)m, n);
    avvisa(percorso, perche);
    font_di_riserva(f);
    return -1;
}

int icona_da_file(const char *percorso, struct icona *misure, int massimo) {
    const char *perche = 0;
    unsigned long n = 0;
    const unsigned char *m = mappa(percorso, &n, &perche);
    if (m && testa_giusta(m, n, "MSIC", &perche)) {
        int quante = (int)u16(m + 6);
        if (quante < 1 || quante > massimo || n < 8 + 8ul * quante) perche = "il numero delle misure non va";
        for (int i = 0; i < quante && !perche; i++) {
            unsigned long lato = u32(m + 8 + 8 * i), dove = u32(m + 12 + 8 * i);
            if (lato < 1 || lato > 255 || dove + lato * lato > n) perche = "una misura esce dal file";
            else if (i && lato <= misure[i - 1].lato) perche = "le misure non vanno dalla piu' piccola";
            else misure[i] = (struct icona){ (unsigned char)lato, m + dove };
        }
        if (!perche) return quante;
    }
    if (m) munmap((void *)m, n);
    avvisa(percorso, perche);
    misure[0] = (struct icona){ 0, 0 };
    return -1;
}

int immagine_da_file(const char *percorso, struct immagine *im) {
    const char *perche = 0;
    unsigned long n = 0;
    const unsigned char *m = mappa(percorso, &n, &perche);
    if (m && testa_giusta(m, n, "MSIM", &perche)) {
        unsigned long l = n >= IMMAGINE_TESTA ? u32(m + 8) : 0, a = n >= IMMAGINE_TESTA ? u32(m + 12) : 0;
        if (n < IMMAGINE_TESTA || l < 1 || a < 1 || l > 0xFFFF || a > 0xFFFF || n != IMMAGINE_TESTA + l * a * 4)
            perche = "la lunghezza del file non torna con le misure dell'immagine";
        else {
            *im = (struct immagine){ (unsigned short)l, (unsigned short)a,
                                     (const unsigned int *)(m + IMMAGINE_TESTA) };
            return 0;
        }
    }
    if (m) munmap((void *)m, n);
    avvisa(percorso, perche);
    *im = (struct immagine){ 0 };
    return -1;
}
