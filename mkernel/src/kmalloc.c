/* kmalloc: il quaderno del kernel.
 *
 * Il magazziniere da' pagine intere da 4 KB. Qui le dividiamo in pezzi della misura
 * che serve. La memoria e' una fila di BLOCCHI, ognuno con un'etichetta davanti:
 *
 *   [etichetta| occupato 16 ][etichetta|    libero 200     ][etichetta| occupato 40 ]
 *
 * Chi chiede riceve l'indirizzo subito DOPO l'etichetta: l'etichetta non la vede.
 *
 *   kmalloc: il primo blocco libero abbastanza grande; se avanza molto spazio lo si
 *            spezza in due. Se nessuno va bene, si chiedono pagine al magazziniere.
 *   kfree:   dall'indirizzo si torna all'etichetta, si segna libero, e lo si fonde
 *            con i vicini liberi (contro la frammentazione).
 *
 * I blocchi sono in una lista in ordine di indirizzo: ogni etichetta conosce
 * il blocco prima e quello dopo. */
#include "kernel.h"
#include "pagine.h"
#include "kmalloc.h"

#define LIBERO   0x4C1BE40u   /* "LIBERO" e "OCCUPATO" sono numeri strani apposta:   */
#define OCCUPATO 0x0CC0FA7u   /* se kfree trova altro, il puntatore e' sbagliato    */

#define ALLINEA   8                     /* ogni pezzo parte a un multiplo di 8 */
#define MINIMO    8                     /* non si creano blocchi liberi piu' piccoli */
#define ZONA_INIZIALE (256 * PAGINA)    /* 1 MB */
#define ZONA_MINIMA   (16 * PAGINA)     /* quando si cresce, almeno 64 KB alla volta */

struct blocco {
    unsigned int stato;         /* LIBERO o OCCUPATO */
    unsigned int dimensione;    /* byte utili, dopo l'etichetta */
    struct blocco *prec, *succ; /* i vicini nella lista */
};                              /* 16 byte: i pezzi restano allineati a 8 */

#define ETICHETTA sizeof(struct blocco)

static struct blocco *primo;    /* il blocco con l'indirizzo piu' basso */

/* L'indirizzo dove finisce il blocco: subito dopo il suo spazio utile. */
static char *fine(struct blocco *b) {
    return (char *)(b + 1) + b->dimensione;
}

/* Se 'b' e il blocco dopo sono liberi e attaccati in memoria, ne fa uno solo. */
static void fondi_col_successivo(struct blocco *b) {
    struct blocco *s = b->succ;
    if (!s || b->stato != LIBERO || s->stato != LIBERO) return;
    if (fine(b) != (char *)s) return;   /* zone diverse, non attaccate: non si fonde */

    b->dimensione += ETICHETTA + s->dimensione;   /* si mangia anche l'etichetta di s */
    b->succ = s->succ;
    if (s->succ) s->succ->prec = b;
    s->stato = 0;                                  /* l'etichetta di s non vale piu' */
}

/* Aggiunge una zona nuova presa dal magazziniere, come un unico blocco libero. */
static int aggiungi_zona(unsigned int byte) {
    unsigned int pagine = (byte + PAGINA - 1) / PAGINA;
    unsigned int indirizzo = pagine_chiedi_consecutive(pagine);
    if (!indirizzo) return 0;

    struct blocco *nuovo = (struct blocco *)indirizzo;
    nuovo->stato = LIBERO;
    nuovo->dimensione = pagine * PAGINA - ETICHETTA;

    /* lo si inserisce nella lista al suo posto, in ordine di indirizzo */
    struct blocco *prima = 0, *dopo = primo;
    while (dopo && dopo < nuovo) { prima = dopo; dopo = dopo->succ; }
    nuovo->prec = prima;
    nuovo->succ = dopo;
    if (prima) prima->succ = nuovo; else primo = nuovo;
    if (dopo) dopo->prec = nuovo;

    /* se la zona e' capitata attaccata a un'altra, si fondono */
    fondi_col_successivo(nuovo);
    if (prima) fondi_col_successivo(prima);
    return 1;
}

void kmalloc_prepara(void) {
    if (!aggiungi_zona(ZONA_INIZIALE))
        panico("kmalloc: il magazziniere non ha 1 MB consecutivo");
}

static void *kmalloc_dentro(unsigned int byte) {
    if (byte == 0) return 0;
    byte = (byte + ALLINEA - 1) / ALLINEA * ALLINEA;   /* arrotonda a multiplo di 8 */

    for (int tentativo = 0; tentativo < 2; tentativo++) {
        /* il primo blocco libero abbastanza grande */
        for (struct blocco *b = primo; b; b = b->succ) {
            if (b->stato != LIBERO || b->dimensione < byte) continue;

            /* se avanza spazio per un altro blocco, si spezza in due */
            if (b->dimensione >= byte + ETICHETTA + MINIMO) {
                struct blocco *resto = (struct blocco *)((char *)(b + 1) + byte);
                resto->stato = LIBERO;
                resto->dimensione = b->dimensione - byte - ETICHETTA;
                resto->prec = b;
                resto->succ = b->succ;
                if (b->succ) b->succ->prec = resto;
                b->succ = resto;
                b->dimensione = byte;
            }
            b->stato = OCCUPATO;
            return b + 1;   /* l'indirizzo subito dopo l'etichetta */
        }

        /* nessuno va bene: si chiede una zona nuova al magazziniere e si riprova */
        unsigned int serve = byte + ETICHETTA;
        if (!aggiungi_zona(serve > ZONA_MINIMA ? serve : ZONA_MINIMA)) break;
    }
    return 0;   /* memoria finita */
}

static void kfree_dentro(void *p) {
    if (!p) return;
    struct blocco *b = (struct blocco *)p - 1;   /* indietro di un'etichetta */

    if (b->stato == LIBERO)
        panico("kfree: %p restituito due volte", p);
    if (b->stato != OCCUPATO)
        panico("kfree: %p non viene da kmalloc", p);

    b->stato = LIBERO;
    fondi_col_successivo(b);                    /* prima con quello dopo... */
    if (b->prec) fondi_col_successivo(b->prec); /* ...poi con quello prima */
}

/* Le funzioni pubbliche sono SEZIONI CRITICHE: se un tick fermasse un compito
 * tra "ho trovato un blocco libero" e "l'ho segnato occupato", un altro compito
 * potrebbe ricevere lo stesso blocco. Con le interruzioni spente non succede. */
void *kmalloc(unsigned int byte) {
    unsigned int s = interruzioni_spegni();
    void *p = kmalloc_dentro(byte);
    interruzioni_ripristina(s);
    return p;
}

void kfree(void *p) {
    unsigned int s = interruzioni_spegni();
    kfree_dentro(p);
    interruzioni_ripristina(s);
}

/* --- per vedere cosa succede --- */

void kmalloc_mostra(void) {
    for (struct blocco *b = primo; b; b = b->succ)
        kprintf("  %p  %7u byte  %s\n", (void *)(b + 1), b->dimensione,
                b->stato == LIBERO ? "libero" : "occupato");
}

void kmalloc_conta(unsigned int *blocchi, unsigned int *liberi) {
    *blocchi = *liberi = 0;
    for (struct blocco *b = primo; b; b = b->succ) {
        (*blocchi)++;
        if (b->stato == LIBERO) *liberi += b->dimensione;
    }
}
