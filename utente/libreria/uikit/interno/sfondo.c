/* UIKit: lo sfondo della scrivania scelto dalla persona (le dichiarazioni sono in uikit/sfondo.h). */
#include <string.h>

#include "uikit/uikit.h"

/* La scelta (il nome del file) la tiene catalog (corekit/catalog.h); i file degli sfondi stanno
 * ancora qui, e si leggono da qui. */
#define CARTELLA_SFONDI "/data/users/1000/wallpapers"

static int finisce_con_jpg(const char *s) {
    int n = (int)strlen(s);
    if (n < 5) return 0;
    const char *e = s + n - 4;
    return e[0] == '.' && (e[1] | 32) == 'j' && (e[2] | 32) == 'p' && (e[3] | 32) == 'g';
}

/* Un nome di file valido: niente "/" e non comincia con "." (la preferenza e' un file di testo). */
static int nome_valido(const char *s) {
    if (!s[0] || strlen(s) >= SFONDO_FILE_MAX || s[0] == '.') return 0;
    for (; *s; s++) if (*s == '/') return 0;
    return 1;
}

int sfondo_elenca(char nomi[][SFONDO_FILE_MAX], int max) {
    int d = apri(CARTELLA_SFONDI, APRI_LEGGERE);
    if (d < 0) return 0;
    int n = 0;
    struct voce_cartella v;
    while (n < max && leggi(d, &v, sizeof v) > 0) {   /* le voci arrivano in ordine alfabetico */
        if (v.tipo != TIPO_FILE || !finisce_con_jpg(v.nome) || !nome_valido(v.nome)) continue;
        strcpy(nomi[n++], v.nome);
    }
    chiudi(d);
    return n;
}

void sfondo_nome_mostrato(const char *file, char *nome, int dimensione) {
    int n = (int)strlen(file);
    if (finisce_con_jpg(file)) n -= 4;
    if (n > dimensione - 1) n = dimensione - 1;
    for (int i = 0; i < n; i++) nome[i] = file[i];
    nome[n] = 0;
    if (n > 0 && nome[0] >= 'a' && nome[0] <= 'z') nome[0] = (char)(nome[0] - 32);
}

void sfondo_da_catalog(const struct catalog_valori *v, char *file, int dimensione) {
    file[0] = 0;
    if ((int)strlen(v->sfondo) < dimensione && nome_valido(v->sfondo)) strcpy(file, v->sfondo);
}

void sfondo_leggi(char *file, int dimensione) {
    struct catalog_valori v;
    catalog_leggi(&v);                       /* se catalog non risponde: lo sfondo predefinito */
    sfondo_da_catalog(&v, file, dimensione);
}

int sfondo_salva(const char *file) {
    if (file[0] && !nome_valido(file)) return -1;
    return catalog_cambia(0, file) == 0 ? 0 : -1;
}

int sfondo_disegna_foto(const char *file, unsigned int *pixel, int l, int a) {
    if (!nome_valido(file)) return -1;
    char percorso[sizeof CARTELLA_SFONDI + SFONDO_FILE_MAX + 1];
    strcpy(percorso, CARTELLA_SFONDI "/");
    strcat(percorso, file);
    struct immagine foto;
    if (immagine_carica(percorso, &foto) < 0) return -1;
    immagine_rimpicciolita(&foto, pixel, l, a);
    immagine_libera(&foto);
    return 0;
}
