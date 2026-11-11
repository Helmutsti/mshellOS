/* EXT4: le cartelle.
 *
 * Una cartella e' un file fatto di VOCI una dopo l'altra (ext4_dir_entry_2):
 *
 *     byte 0-3   il numero dell'inode (0 = voce vuota, da saltare)
 *     byte 4-5   quanti byte occupa la voce (rec_len): dove comincia la prossima
 *     byte 6     quanti byte e' lungo il nome
 *     byte 7     il tipo (1 file, 2 cartella, 7 collegamento...; c'e' con "filetype")
 *     byte 8-    il nome, senza 0 finale
 *
 * Una voce non passa mai da un blocco al successivo: l'ultima di ogni blocco si allunga
 * fino alla fine. Cancellare un file vuol dire allungare la voce prima, che "copre" la
 * cancellata. Le prime due voci sono "." e "..".
 *
 * L'INDICE (htree, funzione dir_index). Una cartella con migliaia di file letta voce per
 * voce e' lenta: ext4 aggiunge un indice, un albero ordinato per un "hash" del nome. Ma
 * lo nasconde dentro blocchi che restano voci valide: il blocco 0 ha "." e poi ".." che
 * si allunga fino alla fine del blocco (dentro, coperta, c'e' la radice dell'indice); i
 * nodi interni sono blocchi con una sola voce vuota lunga quanto il blocco. Leggendo
 * tutto di fila e saltando le voci vuote si vedono quindi tutti i nomi, e solo quelli.
 * Qui si fa cosi' sempre, anche per cercare un nome (l'indice per cercare piu' in fretta
 * e' una tappa successiva).
 *
 * IL CHECKSUM (metadata_csum). In fondo a ogni blocco di voci c'e' una finta voce da 12
 * byte (inode 0, lunghezza 12, nome lungo 0, tipo 0xDE) con il crc32c del blocco. E'
 * vuota, quindi la lettura la salta da sola; prima la si usa per controllare il blocco.
 *
 * LE CARTELLE IN LINEA (inline_data). Una cartella piccola sta nell'inode: nei 60 byte,
 * i primi 4 sono il numero della cartella di sopra ("..": "." non c'e', si sa chi e'),
 * poi voci normali fino al byte 60; e altre voci nell'attributo "system.data". Qui le
 * due parti si mettono una dopo l'altra in memoria e si leggono come un blocco solo.
 *
 * IL CURSORE della lettura (voce) e' il byte da cui ripartire: nelle cartelle normali il
 * byte nel file, in quelle in linea il byte nel pezzo unico. */
#include "ext4_interno.h"

#define TIPO_CODA_CHECKSUM 0xDE

/* Chi legge una cartella: un pezzo (un blocco, o tutta la cartella in linea) alla volta. */
struct lettore {
    struct ext4 *e;
    struct inode_ext4 *n;
    unsigned char *buf;
    unsigned long long caricato;      /* quale pezzo c'e' in buf (~0: nessuno) */
    unsigned int lung;                /* byte buoni del pezzo (0: e' un buco, si salta) */
    unsigned int dim_pezzo;
    unsigned long long fine;          /* byte di tutta la cartella */
    int in_linea;
    unsigned int superiore;           /* in linea: la cartella di sopra */
};

static int lettore_apri(struct lettore *l, struct ext4 *e, struct inode_ext4 *n) {
    l->e = e;
    l->n = n;
    l->caricato = ~0ull;
    l->in_linea = (n->flag & FL_INLINE) != 0;
    if (l->in_linea) {
        const unsigned char *valore;
        unsigned int lung;
        int r = ext4_dati_in_linea(e, n, &valore, &lung);
        if (r) return r;
        l->dim_pezzo = IN_LUNG_BLOCCO - 4 + lung;
        l->buf = kmalloc(l->dim_pezzo);
        if (!l->buf) return VFS_ERR_MEMORIA;
        memcpy(l->buf, n->grezzo + IN_BLOCCO + 4, IN_LUNG_BLOCCO - 4);
        memcpy(l->buf + IN_LUNG_BLOCCO - 4, valore, lung);
        l->superiore = le32(n->grezzo + IN_BLOCCO);
        l->fine = l->lung = l->dim_pezzo;
        l->caricato = 0;
        return 0;
    }
    l->dim_pezzo = e->blocco;
    l->fine = n->dimensione;
    l->buf = kmalloc(e->blocco);
    return l->buf ? 0 : VFS_ERR_MEMORIA;
}

static void lettore_chiudi(struct lettore *l) {
    kfree(l->buf);
    l->buf = 0;
}

/* Porta in buf il blocco 'quale' della cartella. */
static int carica(struct lettore *l, unsigned long long quale) {
    struct ext4 *e = l->e;
    unsigned long long fisico;
    unsigned int quanti;
    int r;

    l->caricato = ~0ull;
    if (quale >= 0xFFFFFFFFull) return VFS_ERR_FORMATO;
    r = ext4_mappa(e, l->n, (unsigned int)quale, l->buf, &fisico, &quanti);
    if (r) return r;
    if (fisico == 0) {
        l->lung = 0;   /* un buco in una cartella: non ci sono voci */
    } else {
        r = ext4_leggi_blocco(e, fisico, l->buf);
        if (r) return r;
        l->lung = e->blocco;
        /* la coda con il checksum, se c'e' (i nodi dell'indice non ce l'hanno) */
        const unsigned char *c = l->buf + e->blocco - 12;
        if (e->con_checksum && le32(c) == 0 && le16(c + 4) == 12 && c[6] == 0 &&
            c[7] == TIPO_CODA_CHECKSUM &&
            ext4_crc32c(l->n->seme, l->buf, e->blocco - 12) != le32(c + 8))
            ext4_avviso(e, "checksum sbagliato nel blocco %lu della cartella %lu\n",
                        (unsigned long)quale, (unsigned long)l->n->numero);
    }
    l->caricato = quale;
    return 0;
}

/* rec_len: con i blocchi da 64 KB la lunghezza 65536 non sta in 16 bit, e si scrive
 * cosi' (i due bit bassi, sempre 0 in una lunghezza vera, tengono i bit 16 e 17). */
static unsigned int lunghezza_voce(struct lettore *l, unsigned int v) {
    if (l->in_linea || l->e->blocco < 65536) return v;
    if (v == 65535 || v == 0) return l->e->blocco;
    return (v & 65532) | (v & 3) << 16;
}

/* La prossima voce non vuota dal byte *cursore: 1 (e *cursore dopo di lei), 0 finite,
 * o VFS_ERR_... */
static int prossima(struct lettore *l, unsigned long long *cursore, unsigned int *inode,
                    unsigned int *tipo, const unsigned char **nome, unsigned int *lung_nome) {
    for (;;) {
        unsigned long long c = *cursore;
        if (c >= l->fine) return 0;
        unsigned long long quale = c / l->dim_pezzo;
        unsigned int dentro = (unsigned int)(c % l->dim_pezzo);
        if (quale != l->caricato) {
            int r = carica(l, quale);
            if (r) return r;
        }
        if (dentro + 8 > l->lung) {   /* fine del pezzo (o un buco): si va al prossimo */
            *cursore = (quale + 1) * l->dim_pezzo;
            continue;
        }
        const unsigned char *v = l->buf + dentro;
        unsigned int lung = lunghezza_voce(l, le16(v + 4)), ln = v[6];
        if (lung < 8 || lung & 3 || lung > l->lung - dentro || 8 + ln > lung) {
            ext4_avviso(l->e, "voce rovinata nella cartella %lu (byte %lu)\n",
                        (unsigned long)l->n->numero, (unsigned long)c);
            return VFS_ERR_FORMATO;
        }
        *cursore = c + lung;
        if (le32(v) == 0 || ln == 0) continue;
        *inode = le32(v);
        *tipo = v[7];
        *nome = v + 8;
        *lung_nome = ln;
        return 1;
    }
}

/* Apre la cartella: l'inode, che sia davvero una cartella, il lettore. */
static int apri_cartella(struct ext4 *e, unsigned long long cartella, struct inode_ext4 *n,
                         struct lettore *l) {
    int r = ext4_inode_apri(e, cartella, n);
    if (r) return r;
    if ((n->modo & MODO_TIPO) != MODO_CARTELLA) r = VFS_ERR_NON_CARTELLA;
    else r = lettore_apri(l, e, n);
    if (r) ext4_inode_chiudi(n);
    return r;
}

int ext4_cerca(struct ext4 *e, unsigned long long cartella, const char *nome,
               unsigned int lunghezza, unsigned long long *trovato) {
    struct inode_ext4 n;
    struct lettore l;
    unsigned long long cursore = 0;
    const unsigned char *t;
    unsigned int inode, tipo, lt;
    int r;

    if (lunghezza == 0 || lunghezza > VFS_NOME_MAX) return VFS_ERR_NOME;
    r = apri_cartella(e, cartella, &n, &l);
    if (r) return r;

    /* nelle cartelle in linea "." e ".." non sono scritte come voci */
    if (l.in_linea && nome[0] == '.' && (lunghezza == 1 || (lunghezza == 2 && nome[1] == '.'))) {
        *trovato = lunghezza == 1 ? cartella : l.superiore;
        r = 0;
        goto fine;
    }
    while ((r = prossima(&l, &cursore, &inode, &tipo, &t, &lt)) == 1) {
        if (lt == lunghezza && !memcmp(t, nome, lunghezza)) {
            *trovato = inode;
            r = 0;
            goto fine;
        }
    }
    if (r == 0) r = VFS_ERR_NON_TROVATO;
fine:
    lettore_chiudi(&l);
    ext4_inode_chiudi(&n);
    return r;
}

static unsigned int tipo_voce(struct ext4 *e, unsigned int t) {
    if (!(e->incompat & INCOMPAT_TIPO_FILE)) return 0;
    switch (t) {
    case 1: return VFS_FILE;
    case 2: return VFS_CARTELLA;
    case 7: return VFS_COLLEGAMENTO;
    case 3: case 4: case 5: case 6: return VFS_ALTRO;
    default: return 0;
    }
}

int ext4_voce(struct ext4 *e, unsigned long long cartella, unsigned long long *cursore,
              struct voce_vfs *v) {
    struct inode_ext4 n;
    struct lettore l;
    const unsigned char *t;
    unsigned int inode, tipo, lt;
    int r = apri_cartella(e, cartella, &n, &l);
    if (r) return r;

    while ((r = prossima(&l, cursore, &inode, &tipo, &t, &lt)) == 1) {
        if (t[0] == '.' && (lt == 1 || (lt == 2 && t[1] == '.'))) continue;
        v->nodo = inode;
        v->tipo = tipo_voce(e, tipo);
        v->lunghezza = lt;
        memcpy(v->nome, t, lt);
        v->nome[lt] = 0;
        break;
    }
    lettore_chiudi(&l);
    ext4_inode_chiudi(&n);
    return r;
}
