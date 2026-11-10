/* malloc e free: la memoria a pezzi per i programmi.
 *
 * Il kernel da' solo pagine intere, spostando il confine della zona dinamica (la
 * chiamata rottura). Qui le si divide nei pezzi che servono, come fa kmalloc nel kernel
 * e la malloc della libreria C su Linux. La zona e' una fila di BLOCCHI, ognuno con
 * un'etichetta davanti:
 *
 *   [etichetta| occupato 16 ][etichetta|    libero 200     ][etichetta| occupato 40 ]
 *
 * Chi chiede riceve l'indirizzo subito DOPO l'etichetta.
 *
 *   malloc: il primo blocco libero abbastanza grande; se avanza molto spazio lo si
 *           spezza in due. Se nessuno va bene, si sposta il confine piu' avanti (almeno
 *           64 KB alla volta: chiamare il kernel costa) e il pezzo nuovo si aggiunge in
 *           fondo alla fila.
 *   free:   dall'indirizzo si torna all'etichetta, si segna libero e lo si fonde con i
 *           vicini liberi (contro la frammentazione).
 *
 * Le pagine del pezzo nuovo il kernel le da' solo quando si toccano: una malloc grande
 * e mai usata non consuma memoria. */
#include "libreria.h"

#define LIBERO   0x4C1BE40u   /* numeri strani apposta: se free trova altro, il puntatore */
#define OCCUPATO 0x0CC0FA7u   /* che gli hanno dato e' sbagliato                          */

#define ALLINEA  8
#define MINIMO   8
#define CRESCITA (64 * 1024)
#define PAGINA_UTENTE 4096

struct blocco {
    unsigned int stato;         /* LIBERO o OCCUPATO */
    unsigned int dimensione;    /* byte utili, dopo l'etichetta */
    struct blocco *prec, *succ; /* i vicini nella fila, in ordine di indirizzo */
};
#define ETICHETTA sizeof(struct blocco)

static struct blocco *primo, *ultimo;
static unsigned int fine_zona;   /* dove arriva adesso la zona: il confine */

static char *fine(struct blocco *b) { return (char *)(b + 1) + b->dimensione; }

static void fondi_col_successivo(struct blocco *b) {
    struct blocco *s = b->succ;
    if (!s || b->stato != LIBERO || s->stato != LIBERO || fine(b) != (char *)s) return;
    b->dimensione += ETICHETTA + s->dimensione;
    b->succ = s->succ;
    if (s->succ) s->succ->prec = b;
    else ultimo = b;
    s->stato = 0;
}

/* Sposta il confine piu' avanti di almeno 'byte' (piu' l'etichetta), e aggiunge il
 * pezzo nuovo come un blocco libero in fondo. 0 se il kernel dice di no. */
static int cresci(unsigned int byte) {
    if (!fine_zona) fine_zona = rottura(0);
    unsigned int n = byte + ETICHETTA;
    if (n < CRESCITA) n = CRESCITA;
    n = (n + PAGINA_UTENTE - 1) & ~(PAGINA_UTENTE - 1);
    if (fine_zona + n < fine_zona || rottura(fine_zona + n) != fine_zona + n) return 0;

    struct blocco *b = (struct blocco *)fine_zona;   /* qui il kernel da' la prima pagina */
    b->stato = LIBERO;
    b->dimensione = n - ETICHETTA;
    b->prec = ultimo;
    b->succ = 0;
    if (ultimo) ultimo->succ = b;
    else primo = b;
    ultimo = b;
    fine_zona += n;
    if (b->prec) fondi_col_successivo(b->prec);      /* attaccato al vecchio ultimo libero */
    return 1;
}

void *malloc(unsigned int byte) {
    if (!byte) return 0;
    byte = (byte + ALLINEA - 1) & ~(ALLINEA - 1);
    for (int giro = 0; giro < 2; giro++) {
        for (struct blocco *b = primo; b; b = b->succ) {
            if (b->stato != LIBERO || b->dimensione < byte) continue;
            if (b->dimensione >= byte + ETICHETTA + MINIMO) {   /* avanza molto: si spezza */
                struct blocco *resto = (struct blocco *)((char *)(b + 1) + byte);
                resto->stato = LIBERO;
                resto->dimensione = b->dimensione - byte - ETICHETTA;
                resto->prec = b;
                resto->succ = b->succ;
                if (b->succ) b->succ->prec = resto;
                else ultimo = resto;
                b->succ = resto;
                b->dimensione = byte;
            }
            b->stato = OCCUPATO;
            return b + 1;
        }
        if (!cresci(byte)) return 0;   /* nessun blocco va bene: si sposta il confine */
    }
    return 0;
}

void free(void *p) {
    if (!p) return;
    struct blocco *b = (struct blocco *)p - 1;
    if (b->stato != OCCUPATO) return;   /* non e' un pezzo di malloc, o e' gia' libero */
    b->stato = LIBERO;
    fondi_col_successivo(b);
    if (b->prec) fondi_col_successivo(b->prec);
}
