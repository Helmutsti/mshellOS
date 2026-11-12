/* GKit, il lato del compositore, nella SANDBOX: al posto di gkit/schermo.c (il DRM).
 *
 * Lo schermo e' un file in /run (tmpfs, in memoria), grande come lo schermo di QEMU:
 * 1280x800, 4 byte per pixel (XRGB8888, 0x00RRGGBB), le righe una dopo l'altra. Init lo
 * apre (apri_schermo) e lo presta al compositore, come fa con il DRM; il compositore lo
 * mette nella sua memoria (mappa_schermo) e ci disegna. eventi_sandbox lo legge quando
 * chi fa le prove chiede una foto. Le funzioni sono quelle di gkit.h, con lo stesso
 * comportamento visto dal compositore: lo schermo parte nero, e "cosa e' cambiato" non
 * serve a nessuno (come con le schede video vere). */
#define _GNU_SOURCE
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gkit/gkit.h"
#include "sandbox.h"

int apri_schermo(void) {
    mkdir(SANDBOX_CARTELLA, 0755);
    int d = open(SANDBOX_SCHERMO, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (d < 0) return -1;
    if (ftruncate(d, SANDBOX_BYTE) < 0) {
        close(d);
        return -1;
    }
    return d;
}

void *mappa_schermo(int d, struct info_schermo *info) {
    void *pixel = mmap(0, SANDBOX_BYTE, PROT_READ | PROT_WRITE, MAP_SHARED, d, 0);
    if (pixel == MAP_FAILED) return 0;
    memset(pixel, 0, SANDBOX_BYTE);
    *info = (struct info_schermo){ .passo = SANDBOX_PASSO, .larghezza = SANDBOX_LARGHEZZA,
                                   .altezza = SANDBOX_ALTEZZA, .rosso = 16, .verde = 8, .blu = 0 };
    return pixel;
}

int schermo_cambiato(int d, const struct zona_schermo *zone, int quante) {
    (void)d;
    (void)zone;
    (void)quante;
    return 0;
}
