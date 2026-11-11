/* EXT4: il montaggio, gli inode e le operazioni per il file system virtuale.
 *
 * SOLO LETTURA. Si legge un disco fatto da Linux o da mkfs.ext4 e non ci si scrive mai
 * niente: le operazioni di scrittura di tipo_ext4 sono 0.
 *
 * IL MONTAGGIO (ext4_monta):
 *   1. si legge il superblocco (byte 1024) e si controlla il numero magico 0xEF53;
 *   2. si guardano le funzioni INCOMPAT: se il disco ne usa una che non sappiamo
 *      leggere ci si ferma e si dice quale (VFS_ERR_NON_SUPPORTATO);
 *   3. con metadata_csum si controlla il checksum del superblocco e si calcola il SEME
 *      dei checksum (dal nome unico del disco, o scritto nel superblocco);
 *   4. si controllano i descrittori dei gruppi (il loro checksum, se c'e');
 *   5. si legge la cartella principale (inode 2) per vedere che c'e' davvero.
 *
 * I CHECKSUM SBAGLIATI non fermano niente, per ora: si stampa un avviso (i primi
 * AVVISI_MAX, poi si tace) e si va avanti. Fermarsi vorra' dire non poter piu' leggere
 * un disco appena un po' rovinato; decidere cosa fare e' per una tappa successiva.
 *
 * IL JOURNAL. ext4 scrive prima le modifiche in un diario (journal) e poi al loro
 * posto: se il computer si spegne a meta', al montaggio dopo si "rifanno" dal diario.
 * Noi non lo sappiamo ancora fare: se il disco dice che c'e' lavoro da rifare
 * (needs_recovery) si monta lo stesso, ma con un avviso chiaro: le ultime modifiche
 * potrebbero mancare.
 *
 * NIENTE CACHE QUI. Ogni operazione rilegge dal disco quello che le serve (il
 * descrittore del gruppo, l'inode, i blocchi): sotto c'e' la cache dei blocchi
 * (cache.h), che rende veloce rileggere. Per lo stesso motivo le operazioni non
 * condividono niente e possono girare insieme. La memoria di appoggio si chiede a
 * kmalloc e si restituisce prima di tornare: la pila del kernel e' piccola. */
#include <stdarg.h>
#include "ext4_interno.h"

#define AVVISI_MAX 8

void ext4_avviso(struct ext4 *e, const char *fmt, ...) {
    va_list ap;
    if (e->avvisi >= AVVISI_MAX) return;
    e->avvisi++;
    kprintf("ext4 (%s): ", e->disco->nome ? e->disco->nome : "?");
    va_start(ap, fmt);
    vkprintf(fmt, ap);
    va_end(ap);
    if (e->avvisi == AVVISI_MAX) kprintf("ext4: troppi avvisi, i prossimi non si stampano\n");
}

/* --- leggere il disco --- */

/* 'quanti' settori dal settore 'primo', controllando che stiano nel disco. */
static int leggi_settori(struct ext4 *e, unsigned long long primo, unsigned int quanti,
                         void *dove) {
    struct dispositivo_blocchi *d = e->disco;
    if (primo >= d->settori || quanti > d->settori - primo) return VFS_ERR_FORMATO;
    if (d->leggi(d, primo, quanti, dove)) return VFS_ERR_DISCO;
    return 0;
}

int ext4_leggi_blocchi(struct ext4 *e, unsigned long long primo, unsigned int quanti,
                       void *dove) {
    if (primo >= e->blocchi || quanti > e->blocchi - primo) return VFS_ERR_FORMATO;
    return leggi_settori(e, primo * e->settori_blocco, quanti * e->settori_blocco, dove);
}

int ext4_leggi_blocco(struct ext4 *e, unsigned long long numero, void *dove) {
    return ext4_leggi_blocchi(e, numero, 1, dove);
}

/* Un pezzo piccolo che non comincia per forza all'inizio di un settore (un descrittore,
 * un inode): si leggono i settori che lo contengono e se ne copia la parte giusta. */
static int leggi_pezzo(struct ext4 *e, unsigned long long posizione, unsigned int byte,
                       void *dove) {
    unsigned int s = e->byte_settore;
    unsigned long long primo = posizione / s;
    unsigned int quanti = (unsigned int)((posizione + byte - 1) / s - primo + 1);
    unsigned char *buf = kmalloc(quanti * s);
    int r;
    if (!buf) return VFS_ERR_MEMORIA;
    r = leggi_settori(e, primo, quanti, buf);
    if (!r) memcpy(dove, buf + (posizione - primo * s), byte);
    kfree(buf);
    return r;
}

/* --- i gruppi --- */

static int potenza_di(unsigned int n, unsigned int base) {
    while (n > 1 && n % base == 0) n /= base;
    return n == 1;
}

/* Il gruppo g ha una copia del superblocco (e dei descrittori)? Con sparse_super solo i
 * gruppi 0, 1 e le potenze di 3, 5 e 7; con sparse_super2 solo i due scritti nel
 * superblocco; senza nessuna delle due, tutti. */
static int ha_superblocco(struct ext4 *e, unsigned int g) {
    if (g == 0) return 1;
    if (e->compat & COMPAT_SPARSE_SUPER2) return g == e->copie_bg[0] || g == e->copie_bg[1];
    if (!(e->ro_compat & RO_SPARSE_SUPER) || g == 1) return 1;
    if (!(g & 1)) return 0;
    return potenza_di(g, 3) || potenza_di(g, 5) || potenza_di(g, 7);
}

/* Dove sta il descrittore del gruppo g (byte dall'inizio del disco).
 *   - di solito tutti i descrittori stanno uno dopo l'altro nei blocchi subito dopo il
 *     superblocco;
 *   - con meta_bg i gruppi sono riuniti in "meta-gruppi" di tanti gruppi quanti
 *     descrittori stanno in un blocco, e il blocco dei loro descrittori sta all'inizio
 *     del primo gruppo del meta-gruppo (dopo il superblocco, se quel gruppo ne ha uno).
 *     Cosi' il disco puo' crescere senza spostare una tabella sempre piu' lunga. I
 *     primi s_first_meta_bg meta-gruppi restano al posto di prima. */
static unsigned long long posto_descrittore(struct ext4 *e, unsigned int g) {
    unsigned int per_blocco = e->blocco / e->dim_descrittore;
    unsigned int mg = g / per_blocco;
    unsigned long long blocco;
    if (!(e->incompat & INCOMPAT_META_BG) || mg < e->primo_meta_bg) {
        blocco = (unsigned long long)e->primo_blocco + 1 + mg;
    } else {
        unsigned int primo = mg * per_blocco;
        blocco = (unsigned long long)e->primo_blocco +
                 (unsigned long long)primo * e->blocchi_gruppo + ha_superblocco(e, primo);
    }
    return blocco * e->blocco + (unsigned long long)(g % per_blocco) * e->dim_descrittore;
}

/* Il checksum di un descrittore: 1 giusto (o non c'e'), 0 sbagliato. */
static int descrittore_giusto(struct ext4 *e, unsigned int g, const unsigned char *d) {
    unsigned char g_le[4] = { g & 0xFF, (g >> 8) & 0xFF, (g >> 16) & 0xFF, g >> 24 };
    static const unsigned char zero[2] = { 0, 0 };
    unsigned int crc, dopo = GD_CHECKSUM + 2;

    if (e->con_checksum) {
        crc = ext4_crc32c(e->seme, g_le, 4);
        crc = ext4_crc32c(crc, d, GD_CHECKSUM);
        crc = ext4_crc32c(crc, zero, 2);
        crc = ext4_crc32c(crc, d + dopo, e->dim_descrittore - dopo);
    } else if (e->ro_compat & RO_GDT_CSUM) {
        crc = ext4_crc16(0xFFFF, e->uuid, 16);
        crc = ext4_crc16(crc, g_le, 4);
        crc = ext4_crc16(crc, d, GD_CHECKSUM);
        if (e->dim_descrittore > dopo)
            crc = ext4_crc16(crc, d + dopo, e->dim_descrittore - dopo);
    } else {
        return 1;
    }
    return (crc & 0xFFFF) == le16(d + GD_CHECKSUM);
}

static int leggi_descrittore(struct ext4 *e, unsigned int g, unsigned char *d) {
    return leggi_pezzo(e, posto_descrittore(e, g), e->dim_descrittore, d);
}

static unsigned long long tabella_inode(struct ext4 *e, const unsigned char *d) {
    unsigned long long t = le32(d + GD_TABELLA_INODE_LO);
    if (e->dim_descrittore >= 64) t |= (unsigned long long)le32(d + GD_TABELLA_INODE_HI) << 32;
    return t;
}

/* --- gli inode --- */

/* Il checksum di un inode: crc32c(seme del disco, numero, generazione, tutto l'inode con
 * i due pezzi del checksum a zero). La parte alta (i_checksum_hi) c'e' solo se lo
 * spazio "extra" la contiene; se no si confrontano solo i 16 bit bassi. */
static int inode_giusto(struct ext4 *e, struct inode_ext4 *n) {
    unsigned char *p = n->grezzo;
    int con_alto = n->extra >= 4;
    unsigned int atteso = le16(p + IN_CHECKSUM_LO), crc;
    unsigned char lo0 = p[IN_CHECKSUM_LO], lo1 = p[IN_CHECKSUM_LO + 1], hi0 = 0, hi1 = 0;

    if (con_alto) {
        atteso |= le16(p + IN_CHECKSUM_HI) << 16;
        hi0 = p[IN_CHECKSUM_HI];
        hi1 = p[IN_CHECKSUM_HI + 1];
        p[IN_CHECKSUM_HI] = p[IN_CHECKSUM_HI + 1] = 0;
    }
    p[IN_CHECKSUM_LO] = p[IN_CHECKSUM_LO + 1] = 0;
    crc = ext4_crc32c(n->seme, p, e->dim_inode);
    p[IN_CHECKSUM_LO] = lo0;
    p[IN_CHECKSUM_LO + 1] = lo1;
    if (con_alto) {
        p[IN_CHECKSUM_HI] = hi0;
        p[IN_CHECKSUM_HI + 1] = hi1;
    } else {
        crc &= 0xFFFF;
    }
    return crc == atteso;
}

int ext4_inode_apri(struct ext4 *e, unsigned long long numero, struct inode_ext4 *n) {
    unsigned char *d;
    unsigned long long posto;
    unsigned int g, i;
    int r;

    n->grezzo = 0;
    if (numero < 1 || numero > e->inode_totali) return VFS_ERR_FORMATO;
    g = (unsigned int)((numero - 1) / e->inode_gruppo);
    i = (unsigned int)((numero - 1) % e->inode_gruppo);

    d = kmalloc(e->dim_descrittore);
    if (!d) return VFS_ERR_MEMORIA;
    r = leggi_descrittore(e, g, d);
    posto = tabella_inode(e, d);
    kfree(d);
    if (r) return r;
    if (posto == 0 || posto >= e->blocchi) return VFS_ERR_FORMATO;
    posto = posto * e->blocco + (unsigned long long)i * e->dim_inode;

    n->grezzo = kmalloc(e->dim_inode);
    if (!n->grezzo) return VFS_ERR_MEMORIA;
    r = leggi_pezzo(e, posto, e->dim_inode, n->grezzo);
    if (r) {
        ext4_inode_chiudi(n);
        return r;
    }

    const unsigned char *p = n->grezzo;
    n->numero = numero;
    n->extra = 0;
    if (e->dim_inode > 128) {
        n->extra = le16(p + IN_EXTRA);
        /* un valore impossibile si ignora: si usano solo i primi 128 byte */
        if (n->extra & 3 || 128 + n->extra > e->dim_inode) n->extra = 0;
    }
    n->modo = le16(p + IN_MODO);
    n->flag = le32(p + IN_FLAG);
    n->dimensione = le32(p + IN_DIM_LO) | (unsigned long long)le32(p + IN_DIM_HI) << 32;
    n->seme = 0;
    if (e->con_checksum) {
        unsigned char num_le[4] = { numero & 0xFF, (numero >> 8) & 0xFF,
                                    (numero >> 16) & 0xFF, (numero >> 24) & 0xFF };
        n->seme = ext4_crc32c(e->seme, num_le, 4);
        n->seme = ext4_crc32c(n->seme, p + IN_GENERAZIONE, 4);
        if (!inode_giusto(e, n))
            ext4_avviso(e, "checksum sbagliato nell'inode %lu\n", (unsigned long)numero);
    }
    return 0;
}

void ext4_inode_chiudi(struct inode_ext4 *n) {
    kfree(n->grezzo);
    n->grezzo = 0;
}

/* --- il montaggio --- */

/* Le funzioni INCOMPAT che sappiamo leggere. Le altre fermano il montaggio. */
#define INCOMPAT_CONOSCIUTE (INCOMPAT_TIPO_FILE | INCOMPAT_RECUPERO | INCOMPAT_META_BG | \
                             INCOMPAT_EXTENT | INCOMPAT_64BIT | INCOMPAT_MMP |            \
                             INCOMPAT_FLEX_BG | INCOMPAT_EA_INODE | INCOMPAT_SEME_CSUM |  \
                             INCOMPAT_CARTELLE_GRANDI | INCOMPAT_INLINE_DATA)

static const struct { unsigned int bit; const char *nome; } incompat_nomi[] = {
    { INCOMPAT_COMPRESSIONE, "compression" },
    { INCOMPAT_DISCO_JOURNAL, "journal_dev (e' il journal di un altro disco)" },
    { INCOMPAT_DIRDATA, "dirdata" },
    { INCOMPAT_CIFRATURA, "encrypt" },
    { INCOMPAT_CASEFOLD, "casefold" },
};

static int potenza_di_2(unsigned int n) { return n && !(n & (n - 1)); }

/* Legge e controlla il superblocco e riempie 'e'. */
static int leggi_superblocco(struct ext4 *e) {
    unsigned int s = e->byte_settore;
    unsigned int quanti = (SB_POSIZIONE + SB_DIMENSIONE + s - 1) / s;
    unsigned char *buf = kmalloc(quanti * s), *sb;
    int r = VFS_ERR_FORMATO;
    if (!buf) return VFS_ERR_MEMORIA;
    if (leggi_settori(e, 0, quanti, buf)) {
        kfree(buf);
        return VFS_ERR_DISCO;
    }
    sb = buf + SB_POSIZIONE;

    if (le16(sb + SB_MAGICO) != EXT4_MAGICO) goto fine;   /* non e' ext2/3/4: zitti */

    unsigned int log = le32(sb + SB_LOG_BLOCCO);
    if (log > 6) {
        kprintf("ext4: blocchi da 1024 << %u byte: troppo grandi\n", log);
        goto fine;
    }
    e->blocco = 1024u << log;
    if (e->blocco % s) {
        kprintf("ext4: blocchi da %u byte piu' piccoli dei settori (%u): non supportato\n",
                e->blocco, s);
        r = VFS_ERR_NON_SUPPORTATO;
        goto fine;
    }
    e->settori_blocco = e->blocco / s;
    e->blocchi = le32(sb + SB_BLOCCHI_LO);
    e->primo_blocco = le32(sb + SB_PRIMO_BLOCCO);
    e->blocchi_gruppo = le32(sb + SB_BLOCCHI_GRUPPO);
    e->inode_gruppo = le32(sb + SB_INODE_GRUPPO);
    e->inode_totali = le32(sb + SB_INODE_TOTALI);
    memcpy(e->uuid, sb + SB_UUID, 16);

    if (le32(sb + SB_REVISIONE) == 0) {
        /* il primo ext2: niente funzioni, inode da 128 byte */
        e->dim_inode = 128;
        e->compat = e->incompat = e->ro_compat = 0;
    } else {
        e->dim_inode = le16(sb + SB_DIM_INODE);
        e->compat = le32(sb + SB_COMPAT);
        e->incompat = le32(sb + SB_INCOMPAT);
        e->ro_compat = le32(sb + SB_RO_COMPAT);
    }

    /* le funzioni che non sappiamo leggere */
    unsigned int ignote = e->incompat & ~INCOMPAT_CONOSCIUTE;
    if (ignote) {
        kprintf("ext4: il disco usa funzioni che non sappiamo leggere:");
        for (unsigned int i = 0; i < sizeof incompat_nomi / sizeof incompat_nomi[0]; i++)
            if (ignote & incompat_nomi[i].bit) {
                kprintf(" %s", incompat_nomi[i].nome);
                ignote &= ~incompat_nomi[i].bit;
            }
        if (ignote) kprintf(" (altre: 0x%x)", ignote);
        kprintf("\n");
        r = VFS_ERR_NON_SUPPORTATO;
        goto fine;
    }

    if (e->incompat & INCOMPAT_64BIT) {
        e->blocchi |= (unsigned long long)le32(sb + SB_BLOCCHI_HI) << 32;
        e->dim_descrittore = le16(sb + SB_DIM_DESCRITTORE);
        if (e->dim_descrittore < 64 || e->dim_descrittore > 1024 ||
            !potenza_di_2(e->dim_descrittore))
            goto rovinato;
    } else {
        e->dim_descrittore = 32;
    }
    e->primo_meta_bg = le32(sb + SB_PRIMO_META_BG);
    e->copie_bg[0] = le32(sb + SB_COPIE_BG);
    e->copie_bg[1] = le32(sb + SB_COPIE_BG + 4);

    /* i numeri devono avere senso, se no i conti dopo vanno fuori strada */
    if (e->dim_inode < 128 || e->dim_inode > e->blocco || !potenza_di_2(e->dim_inode) ||
        e->blocchi_gruppo == 0 || e->inode_gruppo == 0 || e->inode_totali == 0 ||
        e->primo_blocco >= e->blocchi || e->primo_blocco > 1 ||
        e->blocchi > e->disco->settori / e->settori_blocco)
        goto rovinato;
    unsigned long long gruppi = (e->blocchi - e->primo_blocco + e->blocchi_gruppo - 1) /
                                e->blocchi_gruppo;
    if (gruppi == 0 || gruppi > 0xFFFFFFFFull ||
        (unsigned long long)e->inode_gruppo * gruppi < e->inode_totali)
        goto rovinato;
    e->gruppi = (unsigned int)gruppi;

    /* i checksum */
    e->con_checksum = (e->ro_compat & RO_METADATA_CSUM) != 0;
    if (e->con_checksum) {
        if (sb[SB_TIPO_CHECKSUM] != 1)
            ext4_avviso(e, "tipo di checksum sconosciuto (%u), si prova crc32c\n",
                        sb[SB_TIPO_CHECKSUM]);
        if (ext4_crc32c(~0u, sb, SB_CHECKSUM) != le32(sb + SB_CHECKSUM))
            ext4_avviso(e, "checksum del superblocco sbagliato\n");
        if (e->incompat & INCOMPAT_SEME_CSUM)
            e->seme = le32(sb + SB_SEME_CHECKSUM);
        else
            e->seme = ext4_crc32c(~0u, e->uuid, 16);
    }

    if (e->incompat & INCOMPAT_RECUPERO)
        kprintf("ext4 (%s): ATTENZIONE: il journal non e' stato rifatto (il disco non e' "
                "stato smontato bene). Lo montiamo in sola lettura cosi' com'e': le ultime "
                "modifiche potrebbero mancare\n", e->disco->nome ? e->disco->nome : "?");
    else if (!(le16(sb + SB_STATO) & 1))
        kprintf("ext4 (%s): il disco non risulta smontato bene (serve un controllo con "
                "e2fsck)\n", e->disco->nome ? e->disco->nome : "?");
    r = 0;
    goto fine;

rovinato:
    kprintf("ext4: superblocco con valori impossibili\n");
    r = VFS_ERR_FORMATO;
fine:
    kfree(buf);
    return r;
}

/* Tutti i descrittori: si controlla il checksum e che la tabella degli inode stia nel
 * disco. Si legge un blocco di descrittori alla volta. */
static int controlla_descrittori(struct ext4 *e) {
    unsigned char *buf = kmalloc(e->blocco);
    unsigned long long caricato = ~0ull;
    int r = 0, sbagliati = 0;
    if (!buf) return VFS_ERR_MEMORIA;
    for (unsigned int g = 0; g < e->gruppi; g++) {
        unsigned long long posto = posto_descrittore(e, g);
        if (posto / e->blocco != caricato) {
            caricato = posto / e->blocco;
            r = ext4_leggi_blocco(e, caricato, buf);
            if (r) break;
        }
        const unsigned char *d = buf + posto % e->blocco;
        unsigned long long t = tabella_inode(e, d);
        if (t == 0 || t >= e->blocchi) {
            kprintf("ext4: il gruppo %u ha la tabella degli inode fuori dal disco\n", g);
            r = VFS_ERR_FORMATO;
            break;
        }
        if (!descrittore_giusto(e, g, d)) sbagliati++;
    }
    if (sbagliati)
        ext4_avviso(e, "checksum sbagliato in %d descrittori di gruppo\n", sbagliati);
    kfree(buf);
    return r;
}

static int ext4_monta(struct montato *m, struct dispositivo_blocchi *d, int sola_lettura) {
    struct ext4 *e;
    struct inode_ext4 radice;
    int r;
    (void)sola_lettura;   /* non scriviamo mai: e' sempre sola lettura */

    if (d->byte_settore < 512 || !potenza_di_2(d->byte_settore) || d->byte_settore > 65536)
        return VFS_ERR_NON_SUPPORTATO;
    e = kmalloc(sizeof *e);
    if (!e) return VFS_ERR_MEMORIA;
    memset(e, 0, sizeof *e);
    e->disco = d;
    e->byte_settore = d->byte_settore;

    r = leggi_superblocco(e);
    if (!r) r = controlla_descrittori(e);
    if (!r) r = ext4_inode_apri(e, 2, &radice);
    if (!r) {
        if ((radice.modo & MODO_TIPO) != MODO_CARTELLA) {
            kprintf("ext4: l'inode 2 non e' una cartella\n");
            r = VFS_ERR_FORMATO;
        }
        ext4_inode_chiudi(&radice);
    }
    if (r) {
        kfree(e);
        return r;
    }
    m->privato = e;
    m->radice = 2;
    return 0;
}

static void ext4_smonta(struct montato *m) {
    kfree(m->privato);
    m->privato = 0;
}

/* --- le operazioni sui nodi --- */

static unsigned int tipo_vfs(unsigned int modo) {
    switch (modo & MODO_TIPO) {
    case MODO_FILE: return VFS_FILE;
    case MODO_CARTELLA: return VFS_CARTELLA;
    case MODO_COLLEGAMENTO: return VFS_COLLEGAMENTO;
    default: return VFS_ALTRO;
    }
}

static int ext4_info(struct montato *m, unsigned long long nodo, struct info_vfs *i) {
    struct ext4 *e = m->privato;
    struct inode_ext4 n;
    int r = ext4_inode_apri(e, nodo, &n);
    if (r) return r;
    const unsigned char *p = n.grezzo;

    i->tipo = tipo_vfs(n.modo);
    i->permessi = n.modo & 07777;
    i->proprietario = le16(p + IN_UID) | le16(p + IN_UID_HI) << 16;
    i->gruppo = le16(p + IN_GID) | le16(p + IN_GID_HI) << 16;
    i->collegamenti = le16(p + IN_COLLEGAMENTI);
    i->dimensione = n.dimensione;
    /* i secondi sono 32 bit con il segno (dal 1901 al 2038). Con l'inode grande ci sono
     * 2 bit in piu' (i_mtime_extra, i due bassi: gli altri sono i nanosecondi) che
     * spostano la data di 2^32 secondi alla volta: si arriva al 2446. */
    i->modificato = (int)le32(p + IN_MTIME);
    if (n.extra >= IN_MTIME_EXTRA + 4 - 128)
        i->modificato += (long long)(le32(p + IN_MTIME_EXTRA) & 3) << 32;
    ext4_inode_chiudi(&n);
    return 0;
}

static long long ext4_leggi(struct montato *m, unsigned long long nodo, unsigned long long da,
                            void *dove, unsigned int byte) {
    struct ext4 *e = m->privato;
    struct inode_ext4 n;
    long long r = ext4_inode_apri(e, nodo, &n);
    if (r) return r;
    switch (n.modo & MODO_TIPO) {
    case MODO_FILE:
    case MODO_COLLEGAMENTO: r = ext4_leggi_dati(e, &n, da, dove, byte); break;
    case MODO_CARTELLA: r = VFS_ERR_E_CARTELLA; break;
    default: r = VFS_ERR_NON_SUPPORTATO; break;   /* dispositivi, code: niente dati */
    }
    ext4_inode_chiudi(&n);
    return r;
}

static int ext4_cerca_op(struct montato *m, unsigned long long cartella, const char *nome,
                         unsigned int lunghezza, unsigned long long *trovato) {
    return ext4_cerca(m->privato, cartella, nome, lunghezza, trovato);
}

static int ext4_voce_op(struct montato *m, unsigned long long cartella,
                        unsigned long long *cursore, struct voce_vfs *v) {
    return ext4_voce(m->privato, cartella, cursore, v);
}

const struct tipo_fs tipo_ext4 = {
    .nome = "ext4",
    .monta = ext4_monta,
    .smonta = ext4_smonta,
    .cerca = ext4_cerca_op,
    .info = ext4_info,
    .leggi = ext4_leggi,
    .voce = ext4_voce_op,
    /* la scrittura: non ancora */
};
