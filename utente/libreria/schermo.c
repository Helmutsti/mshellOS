/* La libreria dei programmi, sopra Linux: lo schermo.
 *
 * In Linux lo schermo e' del DRM ("Direct Rendering Manager"), un file per scheda video:
 * /dev/dri/card0. Il primo programma che lo apre ne diventa il PADRONE ("DRM master"):
 * solo lui puo' decidere cosa si vede. Gli altri restano fuori finche' lui non lo chiude.
 * Noi usiamo la parte piu' semplice del DRM, il "KMS" con i "dumb buffer":
 *
 *   1. le RISORSE della scheda: i CONNETTORI (le uscite: HDMI, quella virtuale di QEMU...),
 *      i CRTC (chi legge un'immagine dalla memoria e la manda a un'uscita) e gli ENCODER
 *      (tra i due);
 *   2. un connettore collegato, e la sua MODALITA': 1280x800 (vedi scegli_modo);
 *   3. un "dumb buffer": un'immagine in memoria, senza scheda grafica che ci disegni
 *      (si disegna con la CPU), con un "framebuffer" che dice com'e' fatta (32 bit per
 *      pixel, XRGB8888: 0x00RRGGBB);
 *   4. la si mette nella memoria del programma (mmap) e la si fa vedere (SETCRTC).
 *
 * Alcune schede (quelle virtuali, come virtio-gpu di QEMU) non leggono quella memoria
 * da sole: bisogna dir loro cosa e' cambiato (DIRTYFB), e la copiano. E' schermo_cambiato.
 * Con le schede vere non serve, ma non fa male.
 *
 * Il programma che ha lo schermo e' uno solo (il compositore), quindi qui basta tenere
 * da parte le cose di uno schermo solo. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#include "libreria.h"
#include "interno.h"

#define LARGHEZZA_VOLUTA 1280
#define ALTEZZA_VOLUTA    800

static unsigned int framebuffer;   /* il numero del framebuffer che si vede */

static int domanda(int d, unsigned long richiesta, void *dati) {
    int r;
    do r = ioctl(d, richiesta, dati); while (r < 0 && (errno == EINTR || errno == EAGAIN));
    return r;
}

/* Il primo /dev/dri/cardN che sa fare i dumb buffer, se lo schermo e' libero: se un
 * altro e' gia' il padrone, -1. */
int apri_schermo(void) {
    for (int n = 0; n < 8; n++) {
        char percorso[] = "/dev/dri/card0";
        percorso[sizeof percorso - 2] = (char)('0' + n);
        int d = open(percorso, O_RDWR | O_CLOEXEC);
        if (d < 0) continue;
        struct drm_get_cap cap = { .capability = DRM_CAP_DUMB_BUFFER };
        if (domanda(d, DRM_IOCTL_GET_CAP, &cap) == 0 && cap.value &&
            domanda(d, DRM_IOCTL_SET_MASTER, 0) == 0)
            return d;
        close(d);
    }
    return -1;
}

/* Il CRTC che puo' mandare l'immagine al connettore: quello che usa gia', se c'e',
 * altrimenti il primo che va bene a uno dei suoi encoder. 0 se nessuno. */
static unsigned int trova_crtc(int d, const struct drm_mode_card_res *r, const unsigned int *crtc,
                               const struct drm_mode_get_connector *c, const unsigned int *encoder) {
    for (unsigned int i = 0; i < c->count_encoders; i++) {
        struct drm_mode_get_encoder e = { .encoder_id = encoder[i] };
        if (domanda(d, DRM_IOCTL_MODE_GETENCODER, &e) < 0) continue;
        if (e.encoder_id == c->encoder_id && e.crtc_id) return e.crtc_id;
        for (unsigned int k = 0; k < r->count_crtcs && k < 32; k++)
            if (e.possible_crtcs & (1u << k)) return crtc[k];
    }
    return 0;
}

/* 1280x800 a 60 Hz con i tempi dello standard VESA (DMT, "reduced blanking"): per gli
 * schermi virtuali che non la mettono nel loro elenco (vedi scegli_modo). */
static struct drm_mode_modeinfo modo_voluto = {
    .clock = 71000,
    .hdisplay = 1280, .hsync_start = 1328, .hsync_end = 1360, .htotal = 1440,
    .vdisplay = 800, .vsync_start = 803, .vsync_end = 809, .vtotal = 823,
    .vrefresh = 60, .flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC,
    .type = DRM_MODE_TYPE_USERDEF, .name = "1280x800",
};

/* La modalita' da usare, tra quelle del connettore:
 *   1. 1280x800, se c'e' nell'elenco;
 *   2. se lo schermo e' VIRTUALE (quello di QEMU), 1280x800 lo stesso: uno schermo
 *      virtuale prende qualsiasi misura. Serve perche' nell'elenco c'e' cio' che dice
 *      QEMU: con una finestra vera, la "preferita" e' la misura che la finestra ha in quel
 *      momento (640x480, quella del firmware), e 1280x800 spesso non c'e';
 *   3. se no (un monitor vero) la piu' grande che non supera 1280x800 (il compositore
 *      piu' grande non la sa disegnare), a parita' quella che il monitor preferisce;
 *   4. se nessuna ci sta, 0. */
static struct drm_mode_modeinfo *scegli_modo(struct drm_mode_modeinfo *modi, unsigned int quanti,
                                             unsigned int tipo_connettore) {
    for (unsigned int k = 0; k < quanti; k++)
        if (modi[k].hdisplay == LARGHEZZA_VOLUTA && modi[k].vdisplay == ALTEZZA_VOLUTA) return &modi[k];
    if (tipo_connettore == DRM_MODE_CONNECTOR_VIRTUAL) return &modo_voluto;
    struct drm_mode_modeinfo *m = 0;
    for (unsigned int k = 0; k < quanti; k++) {
        if (modi[k].hdisplay > LARGHEZZA_VOLUTA || modi[k].vdisplay > ALTEZZA_VOLUTA) continue;
        unsigned long area = (unsigned long)modi[k].hdisplay * modi[k].vdisplay;
        unsigned long finora = m ? (unsigned long)m->hdisplay * m->vdisplay : 0;
        if (!m || area > finora || (area == finora && (modi[k].type & DRM_MODE_TYPE_PREFERRED))) m = &modi[k];
    }
    return m;
}

void *mappa_schermo(int d, struct info_schermo *info) {
    /* 1. le risorse: prima quante sono, poi gli elenchi */
    unsigned int connettori[16], crtc[16];
    struct drm_mode_card_res r = { 0 };
    if (domanda(d, DRM_IOCTL_MODE_GETRESOURCES, &r) < 0) return 0;
    if (r.count_connectors > 16) r.count_connectors = 16;
    if (r.count_crtcs > 16) r.count_crtcs = 16;
    r.count_fbs = r.count_encoders = 0;
    r.connector_id_ptr = (unsigned long)connettori;
    r.crtc_id_ptr = (unsigned long)crtc;
    if (domanda(d, DRM_IOCTL_MODE_GETRESOURCES, &r) < 0) return 0;

    /* 2. un connettore collegato, con la modalita' che vogliamo */
    for (unsigned int i = 0; i < r.count_connectors && i < 16; i++) {
        struct drm_mode_modeinfo modi[64];
        unsigned int encoder[8];
        struct drm_mode_get_connector c = { .connector_id = connettori[i] };
        if (domanda(d, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0) continue;
        if (c.connection != 1 || c.count_modes == 0) continue;   /* 1: collegato */
        if (c.count_modes > 64) c.count_modes = 64;
        if (c.count_encoders > 8) c.count_encoders = 8;
        c.count_props = 0;
        c.modes_ptr = (unsigned long)modi;
        c.encoders_ptr = (unsigned long)encoder;
        if (domanda(d, DRM_IOCTL_MODE_GETCONNECTOR, &c) < 0) continue;
        if (c.count_modes > 64) c.count_modes = 64;
        if (c.count_encoders > 8) c.count_encoders = 8;

        struct drm_mode_modeinfo *m = scegli_modo(modi, c.count_modes, c.connector_type);
        if (!m) continue;
        unsigned int quale_crtc = trova_crtc(d, &r, crtc, &c, encoder);
        if (!quale_crtc) continue;

        /* 3. l'immagine (dumb buffer) e il suo framebuffer */
        struct drm_mode_create_dumb immagine = { .width = m->hdisplay, .height = m->vdisplay, .bpp = 32 };
        if (domanda(d, DRM_IOCTL_MODE_CREATE_DUMB, &immagine) < 0) return 0;
        struct drm_mode_fb_cmd fb = { .width = m->hdisplay, .height = m->vdisplay, .pitch = immagine.pitch,
                                      .bpp = 32, .depth = 24, .handle = immagine.handle };
        if (domanda(d, DRM_IOCTL_MODE_ADDFB, &fb) < 0) return 0;

        /* 4. nella nostra memoria, e sullo schermo */
        struct drm_mode_map_dumb dove = { .handle = immagine.handle };
        if (domanda(d, DRM_IOCTL_MODE_MAP_DUMB, &dove) < 0) return 0;
        void *pixel = mmap(0, immagine.size, PROT_READ | PROT_WRITE, MAP_SHARED, d, (off_t)dove.offset);
        if (pixel == MAP_FAILED) return 0;
        memset(pixel, 0, immagine.size);
        struct drm_mode_crtc accendi = { .crtc_id = quale_crtc, .fb_id = fb.fb_id,
                                         .set_connectors_ptr = (unsigned long)&connettori[i],
                                         .count_connectors = 1, .mode = *m, .mode_valid = 1 };
        if (domanda(d, DRM_IOCTL_MODE_SETCRTC, &accendi) < 0) {
            munmap(pixel, immagine.size);
            return 0;
        }
        framebuffer = fb.fb_id;
        *info = (struct info_schermo){ .passo = immagine.pitch, .larghezza = m->hdisplay,
                                       .altezza = m->vdisplay, .rosso = 16, .verde = 8, .blu = 0 };
        return pixel;
    }
    return 0;
}

int schermo_cambiato(int d, const struct zona_schermo *zone, int quante) {
    if (!framebuffer || quante <= 0) return 0;
    struct drm_clip_rect r[MAX_ZONE_SCHERMO];
    int n = 0;
    for (int i = 0; i < quante && n < MAX_ZONE_SCHERMO; i++) {
        if (zone[i].x0 >= zone[i].x1 || zone[i].y0 >= zone[i].y1) continue;
        r[n++] = (struct drm_clip_rect){ (unsigned short)zone[i].x0, (unsigned short)zone[i].y0,
                                         (unsigned short)zone[i].x1, (unsigned short)zone[i].y1 };
    }
    if (!n) return 0;
    struct drm_mode_fb_dirty_cmd cambiate = { .fb_id = framebuffer, .num_clips = (unsigned int)n,
                                              .clips_ptr = (unsigned long)r };
    /* ENOSYS: la scheda la memoria la legge da sola, e non ha bisogno di saperlo */
    if (domanda(d, DRM_IOCTL_MODE_DIRTYFB, &cambiate) < 0 && errno != ENOSYS) return -1;
    return 0;
}
