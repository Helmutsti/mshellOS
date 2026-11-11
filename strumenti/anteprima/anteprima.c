/* ANTEPRIMA: disegna una scena del toolkit su un PNG, sul Mac, senza QEMU.
 *
 * Il toolkit e' fatto di funzioni 'static inline' negli header: non ha bisogno del kernel
 * per disegnare, solo di una tela (un array di pixel). Qui si include toolkit.h cosi' com'e',
 * si FINGE toolkit_esegui (invece del ciclo degli eventi: impagina, disegna una volta e
 * torna) e si scrive il PNG. Una scena e' un file C con un main() come quello di
 * un'applicazione (anche l'applicazione stessa: vedi scene/impostazioni.c).
 *
 * Opzionale, nella scena: 'static void ritocco(struct finestra_tk *f)' + '#define HA_RITOCCO',
 * chiamata dopo la costruzione e prima del disegno (per dare il fuoco a un campo, ecc.).
 *
 * Uso: anteprima.sh <scena> [scala] [uscita.png]  (il resto lo fa lo script). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "toolkit.h"

/* --- gli scudi: quello che la libreria prenderebbe dal kernel --- */
unsigned int tempo(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned int)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

/* Il disco non c'e': ogni chiamata ai file dice "non trovato" (Files si vede, con la cartella vuota). */
int apri(const char *p, unsigned int m) { (void)p; (void)m; return ERRORE_NON_TROVATO; }
int leggi(int d, void *x, unsigned int n) { (void)d; (void)x; (void)n; return ERRORE_NON_TROVATO; }
int scrivi_in(int d, const void *x, unsigned int n) { (void)d; (void)x; (void)n; return ERRORE_SOLA_LETTURA; }
int info(const char *p, struct info_file *i) { (void)p; (void)i; return ERRORE_NON_TROVATO; }
int crea_cartella(const char *p) { (void)p; return ERRORE_SOLA_LETTURA; }
int cancella(const char *p) { (void)p; return ERRORE_SOLA_LETTURA; }
int rinomina(const char *a, const char *b) { (void)a; (void)b; return ERRORE_SOLA_LETTURA; }
int chiudi(int d) { (void)d; return 0; }
int data_ora(struct data_ora *d) { (void)d; return -1; }

/* --- il finto ciclo degli eventi --- */
static struct finestra_tk *catturata;
static int anteprima_esegui(struct finestra_tk *f) { catturata = f; return 0; }
#define toolkit_esegui anteprima_esegui

#define main scena_main
#include SCENA
#undef main

/* --- il PNG: deflate "stored" (senza compressione), basta e avanza --- */
static uint32_t tabella_crc[256];
static uint32_t crc(uint32_t c, const unsigned char *d, size_t n) {
    if (!tabella_crc[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            tabella_crc[i] = v;
        }
    c = ~c;
    while (n--) c = tabella_crc[(c ^ *d++) & 255] ^ (c >> 8);
    return ~c;
}
static void be32(unsigned char *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void pezzo(FILE *f, const char *tipo, const unsigned char *d, uint32_t n) {
    unsigned char b[4]; be32(b, n); fwrite(b, 1, 4, f);
    fwrite(tipo, 1, 4, f); if (n) fwrite(d, 1, n, f);
    uint32_t c = crc(0, (const unsigned char *)tipo, 4);
    c = crc(c, d, n); be32(b, c); fwrite(b, 1, 4, f);
}
static int scrivi_png(const char *nome, const unsigned char *rgb, int l, int a) {
    size_t riga = (size_t)l * 3 + 1, tot = riga * a;
    unsigned char *crudo = malloc(tot);
    for (int y = 0; y < a; y++) {
        crudo[y * riga] = 0;
        memcpy(crudo + y * riga + 1, rgb + (size_t)y * l * 3, (size_t)l * 3);
    }
    size_t blocchi = (tot + 65534) / 65535, dim = 2 + tot + blocchi * 5 + 4;
    unsigned char *z = malloc(dim), *q = z;
    *q++ = 0x78; *q++ = 1;
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < tot; i++) { s1 = (s1 + crudo[i]) % 65521; s2 = (s2 + s1) % 65521; }
    for (size_t o = 0; o < tot; o += 65535) {
        size_t n = tot - o < 65535 ? tot - o : 65535;
        *q++ = o + n >= tot; *q++ = n; *q++ = n >> 8; *q++ = ~n; *q++ = ~n >> 8;
        memcpy(q, crudo + o, n); q += n;
    }
    be32(q, s2 << 16 | s1); q += 4;
    FILE *f = fopen(nome, "wb");
    if (!f) return -1;
    unsigned char testa[13]; be32(testa, l); be32(testa + 4, a);
    testa[8] = 8; testa[9] = 2; testa[10] = testa[11] = testa[12] = 0;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    pezzo(f, "IHDR", testa, 13); pezzo(f, "IDAT", z, (uint32_t)(q - z)); pezzo(f, "IEND", 0, 0);
    fclose(f); free(crudo); free(z);
    return 0;
}

/* Lo sfondo sotto il vetro: un gradiente viola-azzurro, come una scrivania. SFONDO=nero|bianco
 * per un fondo piatto. */
static void sfondo(unsigned char *rgb, int l, int a, const char *tipo) {
    for (int y = 0; y < a; y++)
        for (int x = 0; x < l; x++) {
            unsigned char *p = rgb + ((size_t)y * l + x) * 3;
            if (tipo && !strcmp(tipo, "nero")) p[0] = p[1] = p[2] = 0;
            else if (tipo && !strcmp(tipo, "bianco")) p[0] = p[1] = p[2] = 255;
            else {
                p[0] = 70 + 120 * x / l; p[1] = 90 + 60 * y / a; p[2] = 200 - 70 * x / l;
            }
        }
}

int main(int argc, char **argv) {
    const char *uscita = argc > 1 ? argv[1] : "anteprima.png";
    int scala = argc > 2 ? atoi(argv[2]) : 1;
    if (scala < 1) scala = 1;
    ((int (*)())scena_main)(0, 0);   /* main() o main(argc, argv): qui e' lo stesso */
    if (!catturata) { fprintf(stderr, "la scena non ha chiamato toolkit_esegui\n"); return 1; }
    struct finestra_tk *f = catturata;

    int l, a;
    widget_misura(&f->base, &l, &a);
    f->base.x = f->base.y = 0; f->base.l = l; f->base.a = a;
    widget_disponi(&f->base);
    f->attiva = 1;
#ifdef HA_RITOCCO
    ritocco(f);
    widget_disponi(&f->base);
#endif

    static unsigned int *pixel;
    pixel = calloc((size_t)l * a, 4);
    struct tela t = { pixel, l, a };
    struct pennello p;
    pennello_prepara(&p, &t);
    widget_disegna(&f->base, &p);

    int M = 16, L = l + 2 * M, A = a + 2 * M;       /* un po' di sfondo intorno */
    unsigned char *fondo = malloc((size_t)L * A * 3);
    sfondo(fondo, L, A, getenv("SFONDO"));
    for (int y = 0; y < a; y++)
        for (int x = 0; x < l; x++) {
            unsigned int s = pixel[y * l + x], al = s >> 24;
            unsigned char *d = fondo + ((size_t)(y + M) * L + x + M) * 3;
            d[0] = ((s >> 16) & 255) + d[0] * (255 - al) / 255;
            d[1] = ((s >> 8) & 255) + d[1] * (255 - al) / 255;
            d[2] = (s & 255) + d[2] * (255 - al) / 255;
        }
    unsigned char *out = fondo; int OL = L, OA = A;
    if (scala > 1) {
        OL = L * scala; OA = A * scala;
        out = malloc((size_t)OL * OA * 3);
        for (int y = 0; y < OA; y++)
            for (int x = 0; x < OL; x++)
                memcpy(out + ((size_t)y * OL + x) * 3, fondo + ((size_t)(y / scala) * L + x / scala) * 3, 3);
    }
    if (scrivi_png(uscita, out, OL, OA) < 0) { perror(uscita); return 1; }
    printf("%s  (%dx%d, finestra %dx%d)\n", uscita, OL, OA, l, a);
    return 0;
}
