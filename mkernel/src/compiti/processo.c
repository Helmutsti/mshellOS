/* Il descrittore di un processo: come pidfd in Linux.
 *
 * Chi fa partire un programma ne riceve il numero; con processo(numero) ottiene un
 * descrittore che diventa "pronto" quando quel programma finisce. Si aspetta con
 * ricevi (che torna 0 quando e' finito) o con aspetta, insieme a tutto il resto: e'
 * cosi' che init si accorge che un servizio e' caduto, e lo fa ripartire.
 *
 * Il descrittore non tiene in mano il compito (che lo scheduler libera quando finisce):
 * si ricorda solo il suo numero, e lo scheduler, liberando un compito, avvisa tutti i
 * descrittori con quel numero. I numeri dei compiti non si riusano. */
#include "kernel.h"
#include "kmalloc.h"
#include "descrittori.h"
#include "compiti.h"
#include "processo.h"

struct processo {
    struct aperto aperto;
    int numero;
    int finito;
    struct processo *dopo;      /* nell'elenco di tutti i descrittori di processo */
};

static struct processo *tutti;

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    struct processo *p = (struct processo *)a;
    (void)dove; (void)byte;
    *allegato = 0;
    while (!p->finito) aperto_aspetta(a);
    return 0;   /* finito: come la fine di un file */
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                 int subito) {
    (void)a; (void)da; (void)byte; (void)allegato; (void)subito;
    return -1;
}

static int pronto(struct aperto *a) { return ((struct processo *)a)->finito; }

static void chiudi(struct aperto *a) {
    for (struct processo **p = &tutti; *p; p = &(*p)->dopo)
        if (&(*p)->aperto == a) { *p = (*p)->dopo; break; }
    kfree(a);
}

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

int processo_apri(int numero) {
    /* un numero mai dato: no. Uno gia' finito (e liberato): un descrittore gia' pronto.
     * Cosi' non conta se il programma fa in tempo a finire prima che lo si chieda. */
    if (numero <= 0 || numero >= compito_prossimo_numero()) return -1;
    struct processo *p = kmalloc(sizeof *p);
    if (!p) return -1;
    memset(p, 0, sizeof *p);
    p->aperto.op = &operazioni;
    p->numero = numero;
    p->finito = !compito_esiste(numero);
    int d = descrittore_nuovo(&p->aperto);
    if (d < 0) { kfree(p); return -1; }
    p->dopo = tutti;
    tutti = p;
    return d;
}

void processo_finito(int numero) {
    for (struct processo *p = tutti; p; p = p->dopo)
        if (p->numero == numero && !p->finito) {
            p->finito = 1;
            aperto_sveglia(&p->aperto);
        }
}
