/* Il file system virtuale (vedi vfs.h per il contratto con i formati, vfs_operazioni.h
 * per quello con il resto del kernel).
 *
 * I MONTAGGI. Una tabellina: ogni posto e' un formato montato su un disco, e la
 * cartella che COPRE (per "/" nessuna). Percorrendo un percorso, quando si arriva su una
 * cartella coperta si passa alla radice del formato montato li': il resto del percorso
 * prosegue dentro di lui. Come in Unix.
 *
 * I PERCORSI si percorrono un pezzo alla volta, da "/". Per ogni pezzo si chiede al
 * formato il nodo con quel nome nella cartella dove si e' arrivati. Le cartelle
 * attraversate si tengono in una PILA: ".." toglie l'ultima (e dalla radice resta alla
 * radice, come in Unix), "." non fa niente, e cosi' ".." funziona anche quando si esce
 * da un formato montato e si torna in quello di sopra, senza chiedere ".." al formato.
 * Le barre ripetute ("/Sistema//init") valgono come una.
 *
 * I COLLEGAMENTI SIMBOLICI: un nodo il cui contenuto e' un percorso. Quando se ne
 * incontra uno, il suo contenuto prende il suo posto nel percorso ancora da fare: se
 * comincia con / si riparte dalla radice, se no dalla cartella dove sta il collegamento.
 * Al massimo VFS_MAX_LINK di fila: un collegamento che punta a se stesso farebbe girare
 * per sempre (Linux: ELOOP, dopo 40).
 *
 * UN COMPITO ALLA VOLTA: ogni operazione pubblica prende il lucchetto del file system,
 * percorre, chiede al formato e lo lascia. I formati quindi non devono preoccuparsi di
 * due compiti che li usano insieme. (Linux ha lucchetti per ogni nodo, per lavorare in
 * parallelo; a noi basta uno.) */
#include "kernel.h"
#include "kmalloc.h"
#include "lucchetto.h"
#include "chiamate.h"          /* UID_SISTEMA, PERMESSO_... */
#include "vfs_operazioni.h"

#define MAX_MONTAGGI 8
#define MAX_PROFONDITA 64      /* cartelle una dentro l'altra in un percorso */

struct montaggio {
    int usato;
    struct montato m;
    struct nodo_vfs sopra;     /* la cartella che copre (per "/": m = 0) */
};

static struct montaggio montaggi[MAX_MONTAGGI];
static struct montato *radice;              /* il formato montato su "/" */
static struct lucchetto lucchetto_vfs = LUCCHETTO("file system");

/* Lo stato di un percorso a meta'. E' grande (piu' di 5 KB): si prende con kmalloc, non
 * sulla pila del kernel, che e' di 8 KB. */
struct cammino {
    char resto[VFS_PERCORSO_MAX];           /* il percorso, con i collegamenti gia' messi dentro */
    struct nodo_vfs pila[MAX_PROFONDITA];   /* le cartelle attraversate: in cima quella dove si e' */
    int profondita;
    char nome[VFS_NOME_MAX + 1];            /* l'ultimo nome, quando serve la cartella che lo contiene */
    unsigned int lunghezza;
};

static unsigned int lunghezza(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

static int stesso_nodo(const struct nodo_vfs *a, const struct nodo_vfs *b) {
    return a->m == b->m && a->nodo == b->nodo;
}

/* Se su 'n' e' montato un formato, la radice di quello (e cosi' via, se ce n'e' un altro
 * sopra). */
static struct nodo_vfs attraversa(struct nodo_vfs n) {
    for (int i = 0; i < MAX_MONTAGGI; i++) {
        struct montaggio *g = &montaggi[i];
        if (g->usato && g->sopra.m && stesso_nodo(&g->sopra, &n)) {
            n.m = &g->m;
            n.nodo = g->m.radice;
            i = -1;                         /* da capo: forse c'e' un altro formato sopra */
        }
    }
    return n;
}

static int info_dentro(const struct nodo_vfs *n, struct info_vfs *i) {
    memset(i, 0, sizeof *i);
    return n->m->tipo->info(n->m, n->nodo, i);
}

/* Il formato puo' dare meno byte di quelli chiesti: si richiede finche' non si arriva a
 * 'byte' o alla fine del file. */
static long long leggi_dentro(const struct nodo_vfs *n, unsigned long long da, void *dove,
                              unsigned int byte) {
    unsigned int letti = 0;
    while (letti < byte) {
        long long r = n->m->tipo->leggi(n->m, n->nodo, da + letti, (char *)dove + letti,
                                        byte - letti);
        if (r < 0) return r;
        if (r == 0) break;                  /* fine del file */
        letti += (unsigned int)r;
    }
    return letti;
}

/* Si puo' scrivere su questo montaggio? 'sa_farlo': il formato ha l'operazione. */
static int scrivibile(const struct montato *m, int sa_farlo) {
    return m->sola_lettura || !sa_farlo ? VFS_ERR_SOLA_LETTURA : 0;
}

/* --- percorrere un percorso --- */

/* Percorre 'percorso' e mette in *n il suo nodo. 'segui': se l'ultimo pezzo e' un
 * collegamento, si va dove punta. 'al_genitore': ci si ferma prima dell'ultimo pezzo, e
 * in *n va la cartella che lo contiene, in c->nome il suo nome (per creare, cancellare,
 * rinominare: cose che si chiedono alla cartella). */
static int cammina(struct cammino *c, const char *percorso, int segui, int al_genitore,
                   struct nodo_vfs *n) {
    if (!radice) return VFS_ERR_NON_TROVATO;
    if (percorso[0] != '/') return VFS_ERR_NOME;   /* solo percorsi assoluti */
    unsigned int l = lunghezza(percorso);
    if (l >= VFS_PERCORSO_MAX) return VFS_ERR_NOME;
    memcpy(c->resto, percorso, l + 1);
    c->pila[0] = (struct nodo_vfs){ radice, radice->radice };
    c->profondita = 1;
    int collegamenti = 0;
    unsigned int pos = 0;                   /* dove si e' arrivati in c->resto */

    for (;;) {
        while (c->resto[pos] == '/') pos++;
        if (!c->resto[pos]) {
            /* finito il percorso: si e' nella cartella in cima alla pila ("/", "/Sistema/.") */
            if (al_genitore) return VFS_ERR_NOME;   /* non c'e' un ultimo nome */
            *n = c->pila[c->profondita - 1];
            return 0;
        }

        /* il prossimo pezzo, fino alla barra */
        const char *pezzo = c->resto + pos;
        unsigned int lung = 0;
        while (pezzo[lung] && pezzo[lung] != '/') lung++;
        if (lung > VFS_NOME_MAX) return VFS_ERR_NOME;
        pos += lung;
        unsigned int dopo = pos;
        while (c->resto[dopo] == '/') dopo++;
        int ultimo = !c->resto[dopo];
        int barra_finale = ultimo && c->resto[pos] == '/';   /* "/Sistema/": vuole una cartella */
        int punto = lung == 1 && pezzo[0] == '.';
        int due_punti = lung == 2 && pezzo[0] == '.' && pezzo[1] == '.';

        if (al_genitore && ultimo) {
            if (punto || due_punti) return VFS_ERR_NOME;
            *n = c->pila[c->profondita - 1];
            memcpy(c->nome, pezzo, lung);
            c->nome[lung] = 0;
            c->lunghezza = lung;
            return 0;
        }
        if (punto) continue;
        if (due_punti) {                    /* la cartella di sopra; dalla radice, la radice */
            if (c->profondita > 1) c->profondita--;
            continue;
        }

        /* lo si chiede al formato della cartella dove si e' */
        struct nodo_vfs qui = c->pila[c->profondita - 1];
        unsigned long long trovato;
        int r = qui.m->tipo->cerca(qui.m, qui.nodo, pezzo, lung, &trovato);
        if (r < 0) return r;
        struct nodo_vfs prossimo = attraversa((struct nodo_vfs){ qui.m, trovato });
        struct info_vfs i;
        if ((r = info_dentro(&prossimo, &i)) < 0) return r;

        if (i.tipo == VFS_COLLEGAMENTO && (!ultimo || segui || barra_finale)) {
            /* il contenuto del collegamento prende il suo posto: "bersaglio" + il resto */
            if (++collegamenti > VFS_MAX_LINK) return VFS_ERR_TROPPI_LINK;
            if (i.dimensione == 0) return VFS_ERR_NON_TROVATO;
            unsigned int resto = lunghezza(c->resto + pos);   /* "" o "/..." */
            if (i.dimensione + resto >= VFS_PERCORSO_MAX) return VFS_ERR_NOME;
            unsigned int bersaglio = (unsigned int)i.dimensione;
            memmove(c->resto + bersaglio, c->resto + pos, resto + 1);
            long long letti = leggi_dentro(&prossimo, 0, c->resto, bersaglio);
            if (letti < 0) return (int)letti;
            if (letti != bersaglio) return VFS_ERR_FORMATO;
            for (unsigned int k = 0; k < bersaglio; k++)
                if (!c->resto[k]) return VFS_ERR_FORMATO;     /* uno 0 in mezzo al percorso */
            if (c->resto[0] == '/') c->profondita = 1;        /* assoluto: dalla radice */
            pos = 0;
            continue;
        }
        if (ultimo) {
            if (barra_finale && i.tipo != VFS_CARTELLA) return VFS_ERR_NON_CARTELLA;
            *n = prossimo;
            return 0;
        }
        if (i.tipo != VFS_CARTELLA) return VFS_ERR_NON_CARTELLA;   /* "/Sistema/init/x" */
        if (c->profondita == MAX_PROFONDITA) return VFS_ERR_NOME;
        c->pila[c->profondita++] = prossimo;
    }
}

/* --- le operazioni --- */

static int monta_dentro(struct cammino *c, const char *dove, const struct tipo_fs *tipo,
                        struct dispositivo_blocchi *d, int sola_lettura) {
    struct montaggio *posto = 0;
    for (int i = 0; i < MAX_MONTAGGI && !posto; i++)
        if (!montaggi[i].usato) posto = &montaggi[i];
    if (!posto) return VFS_ERR_PIENO;

    struct nodo_vfs sopra = { 0, 0 };
    if (!radice) {
        /* il primo montaggio e' per forza la radice: prima non c'e' nessuna cartella */
        if (dove[0] != '/' || dove[1]) return VFS_ERR_NON_TROVATO;
    } else {
        struct info_vfs i;
        int r = cammina(c, dove, 1, 0, &sopra);
        if (r < 0) return r;
        if (sopra.m == radice && sopra.nodo == radice->radice) return VFS_ERR_ESISTE;
        if ((r = info_dentro(&sopra, &i)) < 0) return r;
        if (i.tipo != VFS_CARTELLA) return VFS_ERR_NON_CARTELLA;
    }

    memset(&posto->m, 0, sizeof posto->m);
    posto->m.tipo = tipo;
    posto->m.disco = d;
    posto->m.sola_lettura = sola_lettura;
    int r = tipo->monta(&posto->m, d, sola_lettura);
    if (r < 0) return r;
    posto->sopra = sopra;
    posto->usato = 1;
    if (!radice) radice = &posto->m;
    return 0;
}

static int crea_dentro(struct cammino *c, const char *percorso, unsigned int tipo,
                       unsigned int permessi, unsigned int uid, struct nodo_vfs *creato) {
    struct nodo_vfs cartella;
    struct info_vfs i;
    unsigned long long nodo;
    int r = cammina(c, percorso, 0, 1, &cartella);
    if (r < 0) return r;
    r = cartella.m->tipo->cerca(cartella.m, cartella.nodo, c->nome, c->lunghezza, &nodo);
    if (r == 0) return VFS_ERR_ESISTE;
    if (r != VFS_ERR_NON_TROVATO) return r;
    if ((r = scrivibile(cartella.m, cartella.m->tipo->crea != 0)) < 0) return r;
    if ((r = info_dentro(&cartella, &i)) < 0) return r;
    if (!vfs_puo(&i, uid, PERMESSO_SCRIVERE)) return VFS_ERR_PERMESSO;
    r = cartella.m->tipo->crea(cartella.m, cartella.nodo, c->nome, c->lunghezza, tipo,
                               permessi, uid, &nodo);
    if (r < 0) return r;
    if (creato) *creato = (struct nodo_vfs){ cartella.m, nodo };
    return 0;
}

static int cancella_dentro(struct cammino *c, const char *percorso, unsigned int uid) {
    struct nodo_vfs cartella;
    struct info_vfs i;
    unsigned long long nodo;
    int r = cammina(c, percorso, 0, 1, &cartella);
    if (r < 0) return r;
    r = cartella.m->tipo->cerca(cartella.m, cartella.nodo, c->nome, c->lunghezza, &nodo);
    if (r < 0) return r;
    if ((r = scrivibile(cartella.m, cartella.m->tipo->cancella != 0)) < 0) return r;
    if ((r = info_dentro(&cartella, &i)) < 0) return r;
    if (!vfs_puo(&i, uid, PERMESSO_SCRIVERE)) return VFS_ERR_PERMESSO;
    return cartella.m->tipo->cancella(cartella.m, cartella.nodo, c->nome, c->lunghezza);
}

static int rinomina_dentro(struct cammino *c, struct cammino *c2, const char *da,
                           const char *a, unsigned int uid) {
    struct nodo_vfs da_cartella, a_cartella;
    struct info_vfs i;
    unsigned long long nodo;
    int r = cammina(c, da, 0, 1, &da_cartella);
    if (r < 0) return r;
    if ((r = cammina(c2, a, 0, 1, &a_cartella)) < 0) return r;
    r = da_cartella.m->tipo->cerca(da_cartella.m, da_cartella.nodo, c->nome, c->lunghezza, &nodo);
    if (r < 0) return r;
    if (da_cartella.m != a_cartella.m) return VFS_ERR_NON_SUPPORTATO;   /* tra due formati */
    if ((r = scrivibile(da_cartella.m, da_cartella.m->tipo->rinomina != 0)) < 0) return r;
    /* si toglie da una cartella e si mette in un'altra: si deve poter scrivere in tutte e due */
    if ((r = info_dentro(&da_cartella, &i)) < 0) return r;
    if (!vfs_puo(&i, uid, PERMESSO_SCRIVERE)) return VFS_ERR_PERMESSO;
    if ((r = info_dentro(&a_cartella, &i)) < 0) return r;
    if (!vfs_puo(&i, uid, PERMESSO_SCRIVERE)) return VFS_ERR_PERMESSO;
    return da_cartella.m->tipo->rinomina(da_cartella.m, da_cartella.nodo, c->nome, c->lunghezza,
                                         a_cartella.nodo, c2->nome, c2->lunghezza);
}

static int cambia_dentro(struct cammino *c, const char *percorso, unsigned int proprietario,
                         unsigned int permessi, unsigned int uid) {
    struct nodo_vfs n;
    struct info_vfs i;
    int r = cammina(c, percorso, 1, 0, &n);
    if (r < 0) return r;
    if ((r = info_dentro(&n, &i)) < 0) return r;
    if ((r = scrivibile(n.m, n.m->tipo->cambia != 0)) < 0) return r;
    /* il sistema puo' tutto; il proprietario solo i permessi, e il file resta suo */
    if (uid != UID_SISTEMA && (uid != i.proprietario || proprietario != i.proprietario))
        return VFS_ERR_PERMESSO;
    return n.m->tipo->cambia(n.m, n.nodo, permessi & 07777, proprietario);
}

/* --- le operazioni pubbliche: col lucchetto, e con lo stato del percorso in kmalloc --- */

int vfs_monta(const char *dove, const struct tipo_fs *tipo, struct dispositivo_blocchi *d,
              int sola_lettura) {
    struct cammino *c = kmalloc(sizeof *c);
    if (!c) return VFS_ERR_MEMORIA;
    lucchetto_prendi(&lucchetto_vfs);
    int r = monta_dentro(c, dove, tipo, d, sola_lettura);
    lucchetto_lascia(&lucchetto_vfs);
    kfree(c);
    return r;
}

int vfs_cerca(const char *percorso, int segui, struct nodo_vfs *n) {
    struct cammino *c = kmalloc(sizeof *c);
    if (!c) return VFS_ERR_MEMORIA;
    lucchetto_prendi(&lucchetto_vfs);
    int r = cammina(c, percorso, segui, 0, n);
    lucchetto_lascia(&lucchetto_vfs);
    kfree(c);
    return r;
}

int vfs_info(const struct nodo_vfs *n, struct info_vfs *i) {
    lucchetto_prendi(&lucchetto_vfs);
    int r = info_dentro(n, i);
    lucchetto_lascia(&lucchetto_vfs);
    return r;
}

int vfs_puo(const struct info_vfs *i, unsigned int uid, unsigned int cosa) {
    if (uid == UID_SISTEMA) return 1;
    unsigned int miei = uid == i->proprietario ? i->permessi >> 6 : i->permessi;   /* la cifra giusta */
    return (miei & cosa) == cosa;
}

long long vfs_leggi(const struct nodo_vfs *n, unsigned long long da, void *dove,
                    unsigned int byte) {
    lucchetto_prendi(&lucchetto_vfs);
    long long r = leggi_dentro(n, da, dove, byte);
    lucchetto_lascia(&lucchetto_vfs);
    return r;
}

int vfs_voce(const struct nodo_vfs *cartella, unsigned long long *cursore,
             struct voce_vfs *v, struct nodo_vfs *sotto) {
    lucchetto_prendi(&lucchetto_vfs);
    int r = cartella->m->tipo->voce(cartella->m, cartella->nodo, cursore, v);
    if (r == 1 && sotto) *sotto = attraversa((struct nodo_vfs){ cartella->m, v->nodo });
    lucchetto_lascia(&lucchetto_vfs);
    return r;
}

int vfs_si_scrive(const struct nodo_vfs *n) {
    return scrivibile(n->m, n->m->tipo->scrivi != 0);   /* non cambia mai: niente lucchetto */
}

long long vfs_scrivi(const struct nodo_vfs *n, unsigned long long da, const void *dati,
                     unsigned int byte) {
    lucchetto_prendi(&lucchetto_vfs);
    long long r = scrivibile(n->m, n->m->tipo->scrivi != 0);
    if (r == 0) r = n->m->tipo->scrivi(n->m, n->nodo, da, dati, byte);
    lucchetto_lascia(&lucchetto_vfs);
    return r;
}

int vfs_tronca(const struct nodo_vfs *n, unsigned long long dimensione) {
    lucchetto_prendi(&lucchetto_vfs);
    int r = scrivibile(n->m, n->m->tipo->tronca != 0);
    if (r == 0) r = n->m->tipo->tronca(n->m, n->nodo, dimensione);
    lucchetto_lascia(&lucchetto_vfs);
    return r;
}

int vfs_crea(const char *percorso, unsigned int tipo, unsigned int permessi, unsigned int uid,
             struct nodo_vfs *creato) {
    struct cammino *c = kmalloc(sizeof *c);
    if (!c) return VFS_ERR_MEMORIA;
    lucchetto_prendi(&lucchetto_vfs);
    int r = crea_dentro(c, percorso, tipo, permessi, uid, creato);
    lucchetto_lascia(&lucchetto_vfs);
    kfree(c);
    return r;
}

int vfs_cancella(const char *percorso, unsigned int uid) {
    struct cammino *c = kmalloc(sizeof *c);
    if (!c) return VFS_ERR_MEMORIA;
    lucchetto_prendi(&lucchetto_vfs);
    int r = cancella_dentro(c, percorso, uid);
    lucchetto_lascia(&lucchetto_vfs);
    kfree(c);
    return r;
}

int vfs_rinomina(const char *da, const char *a, unsigned int uid) {
    struct cammino *c = kmalloc(sizeof *c), *c2 = kmalloc(sizeof *c2);
    int r = VFS_ERR_MEMORIA;
    if (c && c2) {
        lucchetto_prendi(&lucchetto_vfs);
        r = rinomina_dentro(c, c2, da, a, uid);
        lucchetto_lascia(&lucchetto_vfs);
    }
    kfree(c);
    kfree(c2);
    return r;
}

int vfs_cambia(const char *percorso, unsigned int proprietario, unsigned int permessi,
               unsigned int uid) {
    struct cammino *c = kmalloc(sizeof *c);
    if (!c) return VFS_ERR_MEMORIA;
    lucchetto_prendi(&lucchetto_vfs);
    int r = cambia_dentro(c, percorso, proprietario, permessi, uid);
    lucchetto_lascia(&lucchetto_vfs);
    kfree(c);
    return r;
}

const char *vfs_messaggio(int errore) {
    switch (errore) {
    case VFS_ERR_DISCO:          return "errore del disco";
    case VFS_ERR_FORMATO:        return "il disco non ha questo formato, o e' rovinato";
    case VFS_ERR_NON_SUPPORTATO: return "il formato usa una funzione che non sappiamo gestire";
    case VFS_ERR_NON_TROVATO:    return "non esiste";
    case VFS_ERR_ESISTE:         return "esiste gia'";
    case VFS_ERR_NON_CARTELLA:   return "non e' una cartella";
    case VFS_ERR_E_CARTELLA:     return "e' una cartella";
    case VFS_ERR_NON_VUOTA:      return "la cartella non e' vuota";
    case VFS_ERR_PIENO:          return "pieno";
    case VFS_ERR_NOME:           return "percorso non valido (assoluto, nomi fino a 255 byte)";
    case VFS_ERR_PERMESSO:       return "permesso negato";
    case VFS_ERR_SOLA_LETTURA:   return "sola lettura";
    case VFS_ERR_MEMORIA:        return "memoria finita";
    case VFS_ERR_TROPPI_LINK:    return "troppi collegamenti simbolici di fila";
    default:                     return "errore sconosciuto";
    }
}
