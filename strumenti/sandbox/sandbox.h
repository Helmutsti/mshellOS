/* La sandbox: mshell vero in un container Docker, senza kernel nostro e senza QEMU.
 *
 * Girano i programmi veri (init, compositore, launcher, scrivania, le app), compilati come
 * in build.sh. Cambiano solo i due capi "di hardware":
 *   - lo schermo (gkit/interno/schermo.c, il DRM) diventa un file in memoria, FILE_SCHERMO, grande
 *     come lo schermo di QEMU: schermo_sandbox.c;
 *   - eventi (che legge /dev/input) diventa eventi_sandbox.c: riceve gli eventi di Linux
 *     (struct input_event) da una presa TCP, li passa allo stesso codice di eventi.c, e
 *     sulla stessa presa da' la foto dello schermo.
 * Il resto (strumenti/sandbox/sandbox.py, compila.sh) e' spiegato in quei file. */
#ifndef SANDBOX_H
#define SANDBOX_H

#define SANDBOX_CARTELLA "/run/sandbox"
#define SANDBOX_SCHERMO  SANDBOX_CARTELLA "/schermo"   /* i pixel, XRGB8888 come il DRM */
#define SANDBOX_LARGHEZZA 1280
#define SANDBOX_ALTEZZA    800
#define SANDBOX_PASSO     (SANDBOX_LARGHEZZA * 4)
#define SANDBOX_BYTE      (SANDBOX_PASSO * SANDBOX_ALTEZZA)
#define SANDBOX_PORTA     7000                         /* la presa TCP di eventi_sandbox */

#endif
