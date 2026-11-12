/* Doom per mshell: la parte che collega il motore (doomgeneric, GPL: vedi LICENSE) al
 * sistema. Il motore chiama sei funzioni (DG_...) e qui si risponde con:
 *   - lo schermo: una finestra di mshell. Il motore disegna 320x200 in DG_ScreenBuffer; qui
 *     si ingrandisce (il vicino piu' vicino, quindi pixel netti) a 640x480, il 4:3 con cui
 *     si giocava davvero (i pixel di Doom non erano quadrati), sotto una barra del titolo;
 *   - la tastiera: le notizie TASTO_FISICO (tasto premuto / lasciato, codici di Linux)
 *     diventano i tasti di Doom (doomkeys.h);
 *   - il tempo: clock_gettime e nanosleep.
 * Il suono non c'e' (il motore e' compilato senza). I salvataggi e la configurazione vanno
 * in /data/users/1000/appsdata/doom, il WAD (doom1.wad, shareware) sta nelle risorse
 * dell'applicazione. */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* Solo GKit: la finestra (gkit.h) e gli strumenti di disegno (gkit_disegno.h). Per la barra
 * si leggono dalle risorse del sistema, con GKit, soltanto il font e l'icona X (i file in
 * /system/resources): niente stile, temi, materiali o componenti di UIKit. */
#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"
#include "doomkeys.h"
#include "doomgeneric.h"

#define BARRA    28
#define LARG     640
#define ALT      480
#define TITOLO_FINESTRA "Doom"

static struct finestra fin;
static int fuoco = 1;

/* la barra: il font del titolo e l'icona X, dalle risorse del sistema */
#define FILE_FONT_TITOLO "fonts/inter-semibold-13.font"
#define FILE_ICONA_CHIUDI "icons/lucide/x.icon"
#define ICONA_MISURE_MAX 8
static struct font font_titolo;
static struct icona icona_chiudi[ICONA_MISURE_MAX];
static int icona_chiudi_misure;

/* Se un file manca, GKit lo scrive nel registro e da' un font di riserva o un'icona vuota:
 * Doom parte lo stesso. */
static void carica_barra(void) {
    char percorso[PERCORSO_RISORSA_MAX];
    percorso_risorsa(percorso, sizeof percorso, FILE_FONT_TITOLO);
    font_da_file(percorso, &font_titolo);
    percorso_risorsa(percorso, sizeof percorso, FILE_ICONA_CHIUDI);
    int n = icona_da_file(percorso, icona_chiudi, ICONA_MISURE_MAX);
    icona_chiudi_misure = n > 0 ? n : 1;
}
static unsigned int pulsanti_prima;

/* --- i tasti: codice di Linux (KEY_...) -> tasto di Doom ---
 * Sparare e usare sono i tasti "virtuali" di doomgeneric, KEY_FIRE e KEY_USE (i comandi
 * predefiniti, m_controls.c: key_fire e key_use), non KEY_RCTRL e lo spazio del Doom
 * originale: Ctrl (sinistro e destro) da' KEY_FIRE, Spazio KEY_USE, come nelle tabelle di
 * doomgeneric (i_input.c). Maiusc corre (key_speed = KEY_RSHIFT), Alt (sinistro e Alt Gr)
 * va di lato (key_strafe = KEY_RALT). */
static const unsigned char da_linux[128] = {
    [1] = KEY_ESCAPE,
    [2] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE, KEY_TAB,
    [16] = 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', KEY_ENTER, KEY_FIRE,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', KEY_RSHIFT, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_RSHIFT,
    [55] = '*', KEY_RALT, KEY_USE, KEY_CAPSLOCK,
    [59] = KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    [87] = KEY_F11, KEY_F12,
    [96] = KEY_ENTER, KEY_FIRE, '/',
    [100] = KEY_RALT,
    [102] = KEY_HOME, KEY_UPARROW, KEY_PGUP, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_END,
            KEY_DOWNARROW, KEY_PGDN, KEY_INS, KEY_DEL,
    [119] = KEY_PAUSE,
};

#define CODA 128
static struct { int premuto; unsigned char tasto; } coda[CODA];
static int coda_da, coda_quanti;
static unsigned char tenuti[256];

static void metti_tasto(int premuto, unsigned char tasto) {
    if (coda_quanti == CODA) return;
    int k = (coda_da + coda_quanti++) % CODA;
    coda[k].premuto = premuto;
    coda[k].tasto = tasto;
    tenuti[tasto] = (unsigned char)premuto;
}

/* La prossima notizia senza aspettare. 0 se non ce n'e'. */
static int prendi_notizia(struct notizia *n) {
    for (;;) {
        if (fin.quante) {
            finestra_notizia(&fin, n);
            return 1;
        }
        struct da_guardare g = { fin.collegamento, 0 };
        if (aspetta(&g, 1, 0) <= 0) return 0;
        if (!finestra_ricevi(&fin, n)) exit(0);   /* il compositore ha chiuso */
        if (!finestra_di_servizio(n)) return 1;
    }
}

static void servi_notizie(void) {
    struct notizia n;
    while (prendi_notizia(&n)) {
        if (n.tipo == TASTO_FISICO) {
            if (n.tasto < sizeof da_linux && da_linux[n.tasto]) metti_tasto(n.premuto != 0, da_linux[n.tasto]);
        } else if (n.tipo == FUOCO) {
            fuoco = (int)n.fuoco;
            if (!fuoco)   /* senza fuoco i tasti non arrivano piu': si lasciano tutti */
                for (int t = 1; t < 256; t++)
                    if (tenuti[t]) metti_tasto(0, (unsigned char)t);
        } else if (n.tipo == PUNTATORE) {
            int clic = (n.pulsanti & PULSANTE_SINISTRO) && !(pulsanti_prima & PULSANTE_SINISTRO);
            pulsanti_prima = n.pulsanti;
            if (clic && n.y < BARRA) {
                if (n.x >= LARG - BARRA) exit(0);   /* la x per chiudere */
                finestra_trascinami(&fin);
            }
        } else if (n.tipo == USCITO) {
            pulsanti_prima = 0;
        }
    }
}

/* --- le funzioni che vuole il motore --- */

void DG_Init(void) {
    if (finestra_apri_come(&fin, LARG, ALT + BARRA, TITOLO_FINESTRA, RUOLO_FINESTRA, 0, 0, 0) < 0) {
        scrivi_testo("doom: non riesco ad aprire la finestra\n");
        exit(1);
    }
    finestra_tasti_fisici(&fin);
    carica_barra();
}

void DG_DrawFrame(void) {
    unsigned int *tela = finestra_tela(&fin);
    if (!tela) exit(0);

    /* la barra */
    struct tela t = { tela, LARG, ALT + BARRA };
    struct pennello p;
    pennello_prepara(&p, &t);
    pennello_colore(&p, OPACO(fuoco ? 0x8E1B14 : 0x4A4A4A));
    riempi_rettangolo(&p, 0, 0, LARG, BARRA);
    pennello_colore(&p, OPACO(0xFFFFFF));
    testo(&p, &font_titolo, 14, (BARRA - font_titolo.altezza) / 2, TITOLO_FINESTRA);
    disegna_icona(&p, icona_chiudi, icona_chiudi_misure, 14, LARG - BARRA + 7, 7);

    /* il gioco, ingrandito: ogni riga di arrivo prende la sua riga di partenza; se e' la
     * stessa della precedente si copia la riga gia' pronta */
    static int colonna[LARG], pronte;
    if (!pronte++)
        for (int x = 0; x < LARG; x++) colonna[x] = x * DOOMGENERIC_RESX / LARG;
    int ultima = -1;
    for (int y = 0; y < ALT; y++) {
        unsigned int *riga = tela + (BARRA + y) * LARG;
        int sy = y * DOOMGENERIC_RESY / ALT;
        if (sy == ultima) {
            for (int x = 0; x < LARG; x++) riga[x] = riga[x - LARG];
            continue;
        }
        const uint32_t *da = DG_ScreenBuffer + sy * DOOMGENERIC_RESX;
        for (int x = 0; x < LARG; x++) riga[x] = 0xFF000000u | da[colonna[x]];
        ultima = sy;
    }
    finestra_mostra(&fin);
}

void DG_SleepMs(uint32_t ms) {
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&t, 0);
}

uint32_t DG_GetTicksMs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000u + t.tv_nsec / 1000000);
}

int DG_GetKey(int *premuto, unsigned char *tasto) {
    if (!coda_quanti) servi_notizie();
    if (!coda_quanti) return 0;
    *premuto = coda[coda_da].premuto;
    *tasto = coda[coda_da].tasto;
    coda_da = (coda_da + 1) % CODA;
    coda_quanti--;
    return 1;
}

void DG_SetWindowTitle(const char *titolo) { (void)titolo; }

int main(void) {
    char *argv[] = { "doom", "-iwad", "/system/apps/doom/resources/doom1.wad",
                     "-savedir", "/data/users/1000/appsdata/doom/", 0 };
    doomgeneric_Create(5, argv);
    for (;;) doomgeneric_Tick();
}
