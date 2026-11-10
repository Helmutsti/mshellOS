/* I socket: i tubi tra programmi, a messaggi interi.
 *
 * UN COLLEGAMENTO HA DUE CAPI. Quando un programma si collega, il kernel crea due capi
 * legati tra loro: uno va a chi si collega, l'altro (quando lo accetta) a chi ascoltava.
 * Ogni capo ha la sua CASELLA: mandare da un capo vuol dire copiare il messaggio nella
 * casella dell'altro capo e svegliare chi ci dorme sopra. Ricevere vuol dire prendere
 * il primo messaggio dalla propria casella.
 *
 *     programma A                 kernel                    programma B
 *     manda(3, ...)  -->  [capo di A] ---> casella [capo di B]  -->  ricevi(5, ...)
 *     ricevi(3, ...) <--  casella [capo di A] <--- [capo di B]  <--  manda(5, ...)
 *
 * Il kernel COPIA il messaggio due volte (dal mittente alla casella, dalla casella al
 * destinatario): i due programmi non vedono mai la memoria l'uno dell'altro.
 *
 * CHI DORME E SU COSA. Ognuno dorme solo sul PROPRIO capo: chi riceve aspetta che
 * arrivi un messaggio, chi manda (con la casella dell'altro piena) aspetta che l'altro
 * ne prenda uno. Chi prende un messaggio sveglia l'altro capo. Cosi' nessuno dorme su
 * un capo che un altro programma potrebbe chiudere e liberare mentre lui dorme.
 *
 * CHI ASCOLTA ha un nome e una fila di collegamenti arrivati ma non ancora accettati.
 * I nomi stanno in un elenco del kernel: nessun file sul disco. */
#include "kernel.h"
#include "kmalloc.h"
#include "descrittori.h"
#include "socket.h"
#include "fogli.h"

#define POSTI 16             /* messaggi in una casella */
#define MAX_IN_ARRIVO 8      /* collegamenti in fila da accettare */

struct messaggio {
    unsigned int byte;
    struct aperto *allegato;         /* un foglio che viaggia col messaggio (un uso suo), o 0 */
    unsigned char dati[MAX_MESSAGGIO];
};

struct capo {
    struct aperto aperto;            /* per primo: un capo E' una cosa aperta */
    struct capo *altro;              /* l'altro capo; 0 se ha chiuso */
    struct messaggio casella[POSTI]; /* un anello: si scrive in 'testa', si prende da 'coda' */
    unsigned int testa, coda;
    struct capo *dopo;               /* nella fila di chi ascolta, finche' non e' accettato */
    struct compito *padrone;         /* chi lo tiene (un capo non si passa ad altri) */
};

struct ascolto {
    struct aperto aperto;            /* per primo, come nel capo */
    char nome[MAX_NOME];
    struct capo *in_arrivo;          /* i collegamenti da accettare, il piu' vecchio primo */
    int quanti;
    struct ascolto *dopo;            /* nell'elenco dei nomi */
};

static struct ascolto *nomi;         /* tutti quelli in ascolto */

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static struct ascolto *cerca(const char *nome) {
    for (struct ascolto *s = nomi; s; s = s->dopo)
        if (uguali(s->nome, nome)) return s;
    return 0;
}

/* --- un capo del collegamento --- */

static int vuota(struct capo *c) { return c->testa == c->coda; }
static int piena(struct capo *c) { return (c->testa + 1) % POSTI == c->coda; }

static int capo_ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    struct capo *c = (struct capo *)a;
    *allegato = 0;
    while (vuota(c)) {
        if (!c->altro) return 0;     /* niente in casella e l'altro ha chiuso: finito */
        aperto_aspetta(a);
    }
    struct messaggio *m = &c->casella[c->coda];
    unsigned int n = m->byte < byte ? m->byte : byte;   /* il resto si perde */
    memcpy(dove, m->dati, n);
    *allegato = m->allegato;         /* il suo uso passa a chi riceve */
    c->coda = (c->coda + 1) % POSTI;
    if (c->altro) aperto_sveglia(&c->altro->aperto);   /* c'e' un posto libero */
    return n;
}

/* SI ALLEGANO SOLO FOGLI. Un socket allegato a un messaggio che viaggia dentro se'
 * stesso non si chiuderebbe mai (si terrebbe in vita da solo): Linux per questo ha un
 * raccoglitore apposta. A noi per ora servono solo i fogli. */
static int capo_manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                      int subito) {
    struct capo *c = (struct capo *)a;
    if (byte > MAX_MESSAGGIO || (allegato && !e_un_foglio(allegato))) return -1;
    for (;;) {
        if (!c->altro) return -1;    /* l'altro ha chiuso: non c'e' a chi mandare */
        if (!piena(c->altro)) break;
        if (subito) return -1;       /* chi non puo' aspettare (il compositore) lo sa subito */
        aperto_aspetta(a);           /* ci sveglia l'altro quando prende un messaggio */
    }
    struct capo *d = c->altro;
    struct messaggio *m = &d->casella[d->testa];
    m->byte = byte;
    memcpy(m->dati, da, byte);
    m->allegato = allegato;
    if (allegato) aperto_tieni(allegato);   /* finche' viaggia, lo tiene il messaggio */
    d->testa = (d->testa + 1) % POSTI;
    aperto_sveglia(&d->aperto);
    return byte;
}

static int capo_pronto(struct aperto *a) {
    struct capo *c = (struct capo *)a;
    return !vuota(c) || !c->altro;
}

static void capo_chiudi(struct aperto *a) {
    struct capo *c = (struct capo *)a;
    for (; !vuota(c); c->coda = (c->coda + 1) % POSTI)   /* messaggi mai ricevuti */
        if (c->casella[c->coda].allegato) aperto_lascia(c->casella[c->coda].allegato);
    if (c->altro) {                  /* l'altro lo deve sapere: ricevi gli dara' 0 */
        c->altro->altro = 0;
        aperto_sveglia(&c->altro->aperto);
    }
    kfree(c);
}

static const struct operazioni operazioni_capo = {
    capo_ricevi, capo_manda, capo_pronto, capo_chiudi
};

static struct capo *capo_nuovo(void) {
    struct capo *c = kmalloc(sizeof *c);
    if (!c) return 0;
    memset(c, 0, sizeof *c);
    c->aperto.op = &operazioni_capo;
    return c;
}

/* --- chi ascolta --- */

static int ascolto_ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    (void)a; (void)dove; (void)byte;
    *allegato = 0;
    return -1;                       /* qui arrivano collegamenti, non messaggi: accetta */
}

static int ascolto_manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                         int subito) {
    (void)a; (void)da; (void)byte; (void)allegato; (void)subito;
    return -1;
}

static int ascolto_pronto(struct aperto *a) { return ((struct ascolto *)a)->in_arrivo != 0; }

static void ascolto_chiudi(struct aperto *a) {
    struct ascolto *s = (struct ascolto *)a;
    for (struct ascolto **p = &nomi; *p; p = &(*p)->dopo)   /* il nome si libera */
        if (*p == s) { *p = s->dopo; break; }
    while (s->in_arrivo) {           /* i collegamenti mai accettati: si chiudono */
        struct capo *c = s->in_arrivo;
        s->in_arrivo = c->dopo;
        capo_chiudi(&c->aperto);
    }
    kfree(s);
}

static const struct operazioni operazioni_ascolto = {
    ascolto_ricevi, ascolto_manda, ascolto_pronto, ascolto_chiudi
};

/* --- le chiamate --- */

int socket_ascolta(const char *nome) {
    if (!nome[0] || cerca(nome)) return -1;   /* il nome e' gia' preso */
    struct ascolto *s = kmalloc(sizeof *s);
    if (!s) return -1;
    memset(s, 0, sizeof *s);
    s->aperto.op = &operazioni_ascolto;
    memcpy(s->nome, nome, MAX_NOME);
    int d = descrittore_nuovo(&s->aperto);
    if (d < 0) { kfree(s); return -1; }
    s->dopo = nomi;
    nomi = s;
    return d;
}

int socket_collega(const char *nome) {
    struct ascolto *s = cerca(nome);
    if (!s || s->quanti >= MAX_IN_ARRIVO) return -1;
    struct capo *mio = capo_nuovo(), *suo = capo_nuovo();
    int d = mio && suo ? descrittore_nuovo(&mio->aperto) : -1;
    if (d < 0) { kfree(mio); kfree(suo); return -1; }
    mio->altro = suo;
    suo->altro = mio;
    mio->padrone = compito_attuale();

    struct capo **fondo = &s->in_arrivo;   /* in fondo alla fila di chi ascolta */
    while (*fondo) fondo = &(*fondo)->dopo;
    *fondo = suo;
    s->quanti++;
    aperto_sveglia(&s->aperto);
    return d;
}

int socket_accetta(int d) {
    struct aperto *a = descrittore(d);
    if (!a || a->op != &operazioni_ascolto) return -1;
    struct ascolto *s = (struct ascolto *)a;
    while (!s->in_arrivo) aperto_aspetta(a);
    struct capo *c = s->in_arrivo;
    int nuovo = descrittore_nuovo(&c->aperto);
    if (nuovo < 0) return -1;        /* tabella piena: il collegamento resta in fila */
    s->in_arrivo = c->dopo;
    s->quanti--;
    c->dopo = 0;
    c->padrone = compito_attuale();
    return nuovo;
}

/* Il padrone dell'altro capo e' vivo finche' l'altro capo esiste: quando un compito
 * finisce si chiudono i suoi capi, e il nostro 'altro' torna 0. Un collegamento non
 * ancora accettato non ha padrone dall'altra parte. */
struct compito *socket_altro(int d) {
    struct aperto *a = descrittore(d);
    if (!a || a->op != &operazioni_capo) return 0;
    struct capo *c = (struct capo *)a;
    return c->altro ? c->altro->padrone : 0;
}
