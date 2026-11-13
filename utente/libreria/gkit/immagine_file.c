/* GKit: un'immagine da un file su disco (le dichiarazioni sono in gkit_disegno.h).
 *
 * Per ora solo le PNG. Il file lo legge questo codice, con le funzioni della libreria dei
 * programmi; i byte li decodifica stb_image (esterni/stb, di altri, pubblico dominio o MIT),
 * che da' i pixel in RGBA non premoltiplicati. Qui si passano al formato delle tele di GKit:
 * 0xAARRGGBB premoltiplicati (vedi gkit_disegno.h). */
#include "corekit/corekit.h"
#include "gkit_disegno.h"

/* Le due funzioni di stb_image che servono (esterni/stb/stb_image.h): dichiarate qui, per
 * non tirare dentro gli header della libreria C. */
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y,
                                     int *canali, int canali_voluti);
void stbi_image_free(void *pixel);

#define IMMAGINE_FILE_MAX (4 << 20)   /* un file piu' grande non si legge (4 MB) */

/* Tutto il file in memoria (da liberare con free). Torna i byte, 0 se non si legge. */
static unsigned char *leggi_tutto(const char *percorso, int *quanti) {
    int d = apri(percorso, APRI_LEGGERE);
    if (d < 0) return 0;
    int spazio = 16384, n = 0;
    unsigned char *byte = malloc(spazio);
    while (byte) {
        if (n == spazio) {                    /* pieno: il doppio */
            if (spazio >= IMMAGINE_FILE_MAX) { free(byte); byte = 0; break; }
            unsigned char *piu = malloc(2 * spazio);
            if (piu) for (int i = 0; i < n; i++) piu[i] = byte[i];
            free(byte);
            byte = piu;
            spazio *= 2;
            continue;
        }
        int letti = leggi(d, byte + n, spazio - n);
        if (letti < 0) { free(byte); byte = 0; break; }
        if (letti == 0) break;
        n += letti;
    }
    chiudi(d);
    *quanti = n;
    return byte;
}

int immagine_carica(const char *percorso, struct immagine *im) {
    *im = (struct immagine){ 0 };
    int n = 0;
    unsigned char *byte = leggi_tutto(percorso, &n);
    if (!byte) return -1;
    int l = 0, a = 0, canali = 0;
    unsigned char *rgba = stbi_load_from_memory(byte, n, &l, &a, &canali, 4);
    free(byte);
    if (!rgba) return -1;
    unsigned int *pixel = l > 0 && a > 0 && l <= 0xFFFF && a <= 0xFFFF
                          ? malloc((unsigned long)l * a * 4) : 0;
    if (!pixel) { stbi_image_free(rgba); return -1; }
    for (int i = 0; i < l * a; i++) {
        const unsigned char *s = rgba + 4 * i;
        unsigned int alfa = s[3];
        pixel[i] = alfa << 24 | (s[0] * alfa + 127) / 255 << 16 | (s[1] * alfa + 127) / 255 << 8 |
                   (s[2] * alfa + 127) / 255;
    }
    stbi_image_free(rgba);
    im->l = (unsigned short)l;
    im->a = (unsigned short)a;
    im->pixel = pixel;
    return 0;
}

void immagine_libera(struct immagine *im) {
    if (im->pixel) free((void *)im->pixel);
    *im = (struct immagine){ 0 };
}
