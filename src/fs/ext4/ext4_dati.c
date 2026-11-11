/* EXT4: dove sta il contenuto di un file.
 *
 * Il file si vede come una fila di blocchi LOGICI (0, 1, 2...: il primo blocco del
 * file, il secondo...). Sul disco ognuno sta in un blocco FISICO qualsiasi. L'inode ha
 * 60 byte (i_block) per dire quale, in uno di quattro modi:
 *
 * 1. GLI EXTENT (ext4, il modo di oggi; flag EXTENT nell'inode). Un extent dice "i
 *    blocchi logici da L a L+n-1 stanno nei blocchi fisici da F a F+n-1": un file
 *    scritto tutto di fila e' un extent solo. Gli extent stanno in un ALBERO:
 *      - ogni nodo comincia con un'intestazione da 12 byte (numero magico 0xF30A,
 *        quante voci, quante ne stanno, PROFONDITA': 0 = foglia);
 *      - poi le voci, da 12 byte, in ordine di blocco logico. In una foglia sono
 *        extent (logico, lunghezza, fisico); negli altri nodi sono INDICI (dal blocco
 *        logico L in poi, guarda il nodo che sta nel blocco fisico B);
 *      - la radice sta nei 60 byte dell'inode (4 voci); gli altri nodi occupano un
 *        blocco ciascuno (340 voci con blocchi da 4 KB). Profondita' massima 5.
 *    Per trovare il blocco L si scende dalla radice prendendo ogni volta l'ultima voce
 *    che comincia a L o prima. Un extent con la lunghezza oltre 32768 e' "non scritto"
 *    (lo spazio e' riservato, per esempio con fallocate, ma il contenuto e' ancora
 *    zero): si legge a zeri. Un blocco logico che nessun extent copre e' un BUCO
 *    (file sparsi): si legge a zeri anche lui.
 *
 * 2. LA MAPPA DEI BLOCCHI (ext2 ed ext3; gli inode senza il flag EXTENT). I 60 byte
 *    sono 15 numeri di blocco: i primi 12 sono i primi 12 blocchi del file (DIRETTI);
 *    il 13esimo e' un blocco pieno di numeri di blocco (INDIRETTO); il 14esimo un
 *    blocco di indiretti (DOPPIO); il 15esimo un blocco di doppi (TRIPLO). Il numero 0
 *    e' un buco. (Come mfs, ma con un livello in piu'.)
 *
 * 3. I DATI IN LINEA (funzione inline_data, flag INLINE nell'inode). I file piccoli
 *    non hanno blocchi: il contenuto sta nei 60 byte stessi e, se non bastano, in un
 *    attributo esteso "system.data" nello spazio libero in fondo all'inode.
 *
 * 4. I COLLEGAMENTI SIMBOLICI CORTI ("veloci"): se il percorso ha meno di 60 byte sta
 *    nei 60 byte dell'inode, senza flag particolari.
 *
 * Nessuna ricorsione: si scende l'albero con un ciclo, e basta un blocco di memoria di
 * appoggio (il nodo attuale) perche' del nodo di sopra serve solo il numero del figlio. */
#include "ext4_interno.h"

#define EXTENT_MAGICO      0xF30A
#define EXTENT_PROF_MAX    5
#define EXTENT_LUNG_MAX    32768u      /* oltre: extent non scritto */
#define FINE_LOGICI        0x100000000ull   /* i blocchi logici sono numeri da 32 bit */

/* Il checksum di un nodo dell'albero (non della radice, che e' dentro l'inode): sta
 * subito dopo l'ultimo posto per le voci. */
static void controlla_nodo(struct ext4 *e, struct inode_ext4 *n, const unsigned char *nodo,
                           unsigned int max, unsigned long long blocco) {
    unsigned int coda = 12 + 12 * max;
    if (!e->con_checksum || coda + 4 > e->blocco) return;
    if (ext4_crc32c(n->seme, nodo, coda) != le32(nodo + coda))
        ext4_avviso(e, "checksum sbagliato nel nodo di extent %lu (inode %lu)\n",
                    (unsigned long)blocco, (unsigned long)n->numero);
}

static int mappa_extent(struct ext4 *e, struct inode_ext4 *n, unsigned int logico,
                        unsigned char *appoggio, unsigned long long *fisico,
                        unsigned int *quanti) {
    const unsigned char *nodo = n->grezzo + IN_BLOCCO;
    unsigned int lung_nodo = IN_LUNG_BLOCCO;
    unsigned long long confine = FINE_LOGICI;   /* dove finisce il ramo in cui siamo */
    int atteso = -1;                             /* la profondita' che deve avere il nodo */

    for (;;) {
        unsigned int voci = le16(nodo + 2), max = le16(nodo + 4), prof = le16(nodo + 6);
        int scelta = -1;
        if (le16(nodo) != EXTENT_MAGICO || voci > max || 12 + 12 * max > lung_nodo ||
            prof > EXTENT_PROF_MAX || (atteso >= 0 && prof != (unsigned int)atteso))
            return VFS_ERR_FORMATO;

        /* l'ultima voce che comincia a 'logico' o prima (le voci sono in ordine) */
        unsigned int basso = 0, alto = voci;
        while (basso < alto) {
            unsigned int mezzo = (basso + alto) / 2;
            if (le32(nodo + 12 + 12 * mezzo) <= logico) basso = mezzo + 1;
            else alto = mezzo;
        }
        scelta = (int)basso - 1;
        if (basso < voci) confine = le32(nodo + 12 + 12 * basso);

        if (prof == 0) {
            if (scelta >= 0) {
                const unsigned char *x = nodo + 12 + 12 * scelta;
                unsigned int inizio = le32(x), lung = le16(x + 4);
                int non_scritto = lung > EXTENT_LUNG_MAX;
                unsigned long long f = (unsigned long long)le16(x + 6) << 32 | le32(x + 8);
                if (non_scritto) lung -= EXTENT_LUNG_MAX;
                if (lung == 0) return VFS_ERR_FORMATO;
                if (logico - inizio < lung) {
                    if (f >= e->blocchi || lung > e->blocchi - f) return VFS_ERR_FORMATO;
                    *fisico = non_scritto ? 0 : f + (logico - inizio);
                    *quanti = lung - (logico - inizio);
                    return 0;
                }
            }
            /* un buco fino al prossimo extent (o alla fine del ramo) */
            *fisico = 0;
            *quanti = confine - logico > 0xFFFFFFFFull ? 0xFFFFFFFFu
                                                       : (unsigned int)(confine - logico);
            return 0;
        }

        if (scelta < 0) {   /* prima del primo indice: un buco */
            *fisico = 0;
            *quanti = (unsigned int)(confine - logico);
            return 0;
        }
        const unsigned char *x = nodo + 12 + 12 * scelta;
        unsigned long long figlio = (unsigned long long)le16(x + 8) << 32 | le32(x + 4);
        int r = ext4_leggi_blocco(e, figlio, appoggio);
        if (r) return r;
        nodo = appoggio;
        lung_nodo = e->blocco;
        atteso = (int)prof - 1;
        controlla_nodo(e, n, nodo, le16(nodo + 4), figlio);
    }
}

/* La mappa all'antica. 'resto' e' il blocco logico contato dall'inizio della zona
 * coperta dal numero 'blocco' (un indiretto copre 'per' blocchi, un doppio per*per...). */
static int mappa_blocchi(struct ext4 *e, struct inode_ext4 *n, unsigned int logico,
                         unsigned char *appoggio, unsigned long long *fisico,
                         unsigned int *quanti) {
    const unsigned char *radice = n->grezzo + IN_BLOCCO;
    unsigned long long per = e->blocco / 4, copre = 1, resto = logico;
    const unsigned char *elenco;
    unsigned int livelli, posto, voci;

    if (logico < 12) {
        elenco = radice;
        posto = logico;
        voci = 12;
    } else {
        resto -= 12;
        livelli = 1;
        copre = per;
        while (resto >= copre) {
            resto -= copre;
            if (++livelli > 3) {   /* oltre il triplo: non c'e' niente */
                *fisico = 0;
                *quanti = 0xFFFFFFFFu;
                return 0;
            }
            copre *= per;
        }
        unsigned long long blocco = le32(radice + 4 * (11 + livelli));
        while (livelli--) {
            if (blocco == 0) {   /* tutta la zona sotto questo numero e' un buco */
                unsigned long long b = copre - resto;
                *fisico = 0;
                *quanti = b > 0xFFFFFFFFull ? 0xFFFFFFFFu : (unsigned int)b;
                return 0;
            }
            int r = ext4_leggi_blocco(e, blocco, appoggio);
            if (r) return r;
            copre /= per;
            posto = (unsigned int)(resto / copre);
            resto %= copre;
            blocco = le32(appoggio + 4 * posto);
        }
        elenco = appoggio;
        voci = (unsigned int)per;
    }

    /* quanti blocchi dopo questo continuano di fila (o restano buco) nello stesso elenco */
    unsigned long long primo = le32(elenco + 4 * posto);
    unsigned int n_fila = 1;
    while (posto + n_fila < voci) {
        unsigned long long dopo = le32(elenco + 4 * (posto + n_fila));
        if (primo == 0 ? dopo != 0 : dopo != primo + n_fila) break;
        n_fila++;
    }
    if (primo != 0 && (primo >= e->blocchi || n_fila > e->blocchi - primo))
        return VFS_ERR_FORMATO;
    *fisico = primo;
    *quanti = n_fila;
    return 0;
}

int ext4_mappa(struct ext4 *e, struct inode_ext4 *n, unsigned int logico,
               unsigned char *appoggio, unsigned long long *fisico, unsigned int *quanti) {
    if (n->flag & FL_EXTENT) return mappa_extent(e, n, logico, appoggio, fisico, quanti);
    return mappa_blocchi(e, n, logico, appoggio, fisico, quanti);
}

/* GLI ATTRIBUTI ESTESI NELL'INODE. Dopo i 128 byte fissi e i_extra_isize byte di campi
 * in piu', lo spazio fino alla fine dell'inode puo' tenere attributi "nome = valore":
 * prima il numero magico 0xEA020000, poi le voci (16 byte + il nome, arrotondato a 4),
 * finite da 4 byte a zero; i valori stanno in fondo, a 'posto' byte dalla prima voce.
 * Ogni nome ha un prefisso numerico: 7 e' "system.", quindi "system.data" e' 7 + "data". */
#define XATTR_MAGICO 0xEA020000u

int ext4_dati_in_linea(struct ext4 *e, struct inode_ext4 *n, const unsigned char **valore,
                       unsigned int *lunghezza) {
    unsigned int inizio = 128 + n->extra, fine = e->dim_inode, p;
    const unsigned char *g = n->grezzo;

    *valore = 0;
    *lunghezza = 0;
    if (e->dim_inode <= 128 || inizio + 4 > fine || le32(g + inizio) != XATTR_MAGICO) return 0;
    inizio += 4;
    for (p = inizio; p + 16 <= fine && le32(g + p) != 0;) {
        unsigned int lung_nome = g[p], prefisso = g[p + 1], posto = le16(g + p + 2);
        unsigned int in_inode = le32(g + p + 4), lung = le32(g + p + 8);
        if (p + 16 + lung_nome > fine) return VFS_ERR_FORMATO;
        if (prefisso == 7 && lung_nome == 4 && !memcmp(g + p + 16, "data", 4)) {
            if (in_inode || posto > fine - inizio || lung > fine - inizio - posto)
                return VFS_ERR_FORMATO;
            *valore = g + inizio + posto;
            *lunghezza = lung;
            return 0;
        }
        p += (16 + lung_nome + 3) & ~3u;
    }
    return 0;
}

/* Un collegamento simbolico "veloce": il percorso sta nei 60 byte dell'inode. Lo e' se
 * non usa extent ne' dati in linea, e' corto, e non ha blocchi suoi (al massimo quello
 * degli attributi estesi, che i_blocks conta in settori da 512 byte). */
static int collegamento_veloce(struct ext4 *e, struct inode_ext4 *n) {
    const unsigned char *g = n->grezzo;
    unsigned int blocchi = le32(g + IN_BLOCCHI_LO);
    if ((n->modo & MODO_TIPO) != MODO_COLLEGAMENTO || n->flag & (FL_EXTENT | FL_INLINE) ||
        n->dimensione >= IN_LUNG_BLOCCO)
        return 0;
    return blocchi == 0 || (le32(g + IN_ACL_LO) != 0 && blocchi == e->blocco / 512);
}

/* Copia da un contenuto in due pezzi (i 60 byte, poi l'attributo) come se fosse uno. */
static long long copia_in_linea(const unsigned char *a, unsigned int lung_a,
                                const unsigned char *b, unsigned int lung_b,
                                unsigned long long da, unsigned char *dove, unsigned int byte) {
    for (unsigned int i = 0; i < byte; i++) {
        unsigned long long p = da + i;
        if (p < lung_a) dove[i] = a[p];
        else if (p - lung_a < lung_b) dove[i] = b[p - lung_a];
        else dove[i] = 0;
    }
    return byte;
}

long long ext4_leggi_dati(struct ext4 *e, struct inode_ext4 *n, unsigned long long da,
                          void *dove, unsigned int byte) {
    unsigned char *d = dove, *appoggio;
    unsigned int fatti = 0;
    int r = 0;

    if (da >= n->dimensione) return 0;
    if (byte > n->dimensione - da) byte = (unsigned int)(n->dimensione - da);
    if (byte == 0) return 0;

    if (n->flag & FL_INLINE) {
        const unsigned char *valore;
        unsigned int lung;
        r = ext4_dati_in_linea(e, n, &valore, &lung);
        if (r) return r;
        return copia_in_linea(n->grezzo + IN_BLOCCO, IN_LUNG_BLOCCO, valore, lung, da, d, byte);
    }
    if (collegamento_veloce(e, n))
        return copia_in_linea(n->grezzo + IN_BLOCCO, IN_LUNG_BLOCCO, 0, 0, da, d, byte);

    appoggio = kmalloc(e->blocco);
    if (!appoggio) return VFS_ERR_MEMORIA;
    while (fatti < byte) {
        unsigned long long pos = da + fatti, fisico;
        unsigned long long logico = pos / e->blocco;
        unsigned int dentro = (unsigned int)(pos % e->blocco), quanti, pezzo;
        unsigned int mancano = byte - fatti;

        if (logico >= FINE_LOGICI) {   /* oltre i blocchi che un file puo' avere: zeri */
            memset(d + fatti, 0, mancano);
            break;
        }
        r = ext4_mappa(e, n, (unsigned int)logico, appoggio, &fisico, &quanti);
        if (r) break;
        unsigned long long disponibili = (unsigned long long)quanti * e->blocco - dentro;
        pezzo = disponibili < mancano ? (unsigned int)disponibili : mancano;

        if (fisico == 0) {
            memset(d + fatti, 0, pezzo);
        } else if (dentro == 0 && pezzo >= e->blocco) {
            /* blocchi interi: dritti nella memoria di chi ha chiesto */
            unsigned int interi = pezzo / e->blocco;
            r = ext4_leggi_blocchi(e, fisico, interi, d + fatti);
            if (r) break;
            pezzo = interi * e->blocco;
        } else {
            /* un pezzo di blocco: si legge tutto il blocco e se ne copia la parte */
            r = ext4_leggi_blocco(e, fisico, appoggio);
            if (r) break;
            if (pezzo > e->blocco - dentro) pezzo = e->blocco - dentro;
            memcpy(d + fatti, appoggio + dentro, pezzo);
        }
        fatti += pezzo;
    }
    kfree(appoggio);
    return r ? r : (long long)fatti;
}
