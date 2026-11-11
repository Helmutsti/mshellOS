/* Quello che i file di ext4 si dicono tra loro (non serve a nessun altro: fuori basta
 * ext4.h).
 *
 * COM'E' FATTO UN DISCO EXT4 (riassunto della documentazione pubblica del kernel Linux,
 * "ext4 Data Structures and Algorithms", Documentation/filesystems/ext4/):
 *
 *   - il disco e' una fila di BLOCCHI tutti uguali (da 1 a 64 KB; quasi sempre 4 KB);
 *   - i blocchi sono divisi in GRUPPI (con blocchi da 4 KB, 32768 blocchi = 128 MB);
 *   - al byte 1024 del disco c'e' il SUPERBLOCCO: la descrizione di tutto (quanti
 *     blocchi, quanti gruppi, quali funzioni il disco usa). Alcuni gruppi ne tengono una
 *     copia di riserva;
 *   - subito dopo, la tabella dei DESCRITTORI DEI GRUPPI: per ogni gruppo, dove stanno
 *     le sue mappe dei posti liberi e la sua TABELLA DEGLI INODE;
 *   - un INODE descrive un file (o una cartella, un collegamento...): tipo, permessi,
 *     proprietario, dimensione, date, e dove sta il contenuto. Gli inode sono numerati
 *     da 1; l'inode N sta nel gruppo (N-1) / inode_per_gruppo, al posto
 *     (N-1) % inode_per_gruppo della sua tabella. La cartella principale e' l'inode 2;
 *   - il NOME di un file non sta nell'inode: sta nella cartella che lo contiene (una
 *     cartella e' un file pieno di voci "nome -> numero di inode").
 *
 * Tutti i numeri sul disco sono little-endian (il byte basso per primo). Li leggiamo
 * byte per byte (le16, le32): cosi' non contano ne' l'allineamento (ARM non vuole
 * letture "storte") ne' come il compilatore disporrebbe una struct.
 *
 * LE FUNZIONI DEL DISCO. Il superblocco ha tre gruppi di bit:
 *   - COMPAT: chi non le conosce puo' leggere e scrivere lo stesso (il journal...);
 *   - RO_COMPAT: chi non le conosce puo' solo leggere (noi leggiamo soltanto: le
 *     accettiamo tutte);
 *   - INCOMPAT: chi non le conosce non deve nemmeno leggere, perche' cambiano il modo
 *     in cui i dati stanno sul disco. Qui accettiamo solo quelle che sappiamo leggere. */
#ifndef EXT4_INTERNO_H
#define EXT4_INTERNO_H

#include "kernel.h"
#include "kmalloc.h"
#include "vfs.h"
#include "ext4.h"

/* --- leggere i numeri little-endian --- */
static inline unsigned int le16(const unsigned char *p) {
    return (unsigned int)p[0] | (unsigned int)p[1] << 8;
}
static inline unsigned int le32(const unsigned char *p) {
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 |
           (unsigned int)p[3] << 24;
}

/* --- il superblocco: dove sta ogni campo (byte dall'inizio del superblocco) --- */
#define SB_POSIZIONE          1024    /* byte dall'inizio del disco */
#define SB_DIMENSIONE         1024
#define SB_INODE_TOTALI       0x00
#define SB_BLOCCHI_LO         0x04
#define SB_PRIMO_BLOCCO       0x14    /* 1 con blocchi da 1 KB, se no 0 */
#define SB_LOG_BLOCCO         0x18    /* blocco = 1024 << questo */
#define SB_BLOCCHI_GRUPPO     0x20
#define SB_INODE_GRUPPO       0x28
#define SB_MAGICO             0x38    /* 0xEF53 */
#define SB_STATO              0x3A    /* bit 0: smontato bene */
#define SB_REVISIONE          0x4C    /* 0: il primo ext2, inode da 128 byte e niente funzioni */
#define SB_DIM_INODE          0x58
#define SB_COMPAT             0x5C
#define SB_INCOMPAT           0x60
#define SB_RO_COMPAT          0x64
#define SB_UUID               0x68    /* 16 byte: il nome unico del disco */
#define SB_DIM_DESCRITTORE    0xFE    /* con 64bit: byte di un descrittore di gruppo */
#define SB_PRIMO_META_BG      0x104
#define SB_BLOCCHI_HI         0x150
#define SB_COPIE_BG           0x24C   /* sparse_super2: i due gruppi con la copia */
#define SB_TIPO_CHECKSUM      0x175   /* 1 = crc32c */
#define SB_SEME_CHECKSUM      0x270   /* con metadata_csum_seed */
#define SB_CHECKSUM           0x3FC

#define EXT4_MAGICO           0xEF53

/* COMPAT */
#define COMPAT_SPARSE_SUPER2  0x0200
/* INCOMPAT */
#define INCOMPAT_COMPRESSIONE 0x00001 /* mai finita in Linux */
#define INCOMPAT_TIPO_FILE    0x00002 /* filetype: il tipo anche nelle voci delle cartelle */
#define INCOMPAT_RECUPERO     0x00004 /* needs_recovery: il journal ha lavoro da rifare */
#define INCOMPAT_DISCO_JOURNAL 0x00008 /* questo disco e' solo il journal di un altro */
#define INCOMPAT_META_BG      0x00010 /* i descrittori sparsi per i gruppi */
#define INCOMPAT_EXTENT       0x00040
#define INCOMPAT_64BIT        0x00080
#define INCOMPAT_MMP          0x00100 /* protezione da due computer che montano insieme */
#define INCOMPAT_FLEX_BG      0x00200 /* le tabelle dei gruppi raccolte insieme */
#define INCOMPAT_EA_INODE     0x00400 /* attributi estesi grandi in un inode a se' */
#define INCOMPAT_DIRDATA      0x01000
#define INCOMPAT_SEME_CSUM    0x02000 /* metadata_csum_seed */
#define INCOMPAT_CARTELLE_GRANDI 0x04000 /* large_dir */
#define INCOMPAT_INLINE_DATA  0x08000
#define INCOMPAT_CIFRATURA    0x10000 /* encrypt */
#define INCOMPAT_CASEFOLD     0x20000
/* RO_COMPAT */
#define RO_SPARSE_SUPER       0x0001
#define RO_GDT_CSUM           0x0010  /* il vecchio checksum crc16 dei descrittori */
#define RO_METADATA_CSUM      0x0400

/* --- il descrittore di un gruppo (32 byte, o 64 con la funzione 64bit) --- */
#define GD_TABELLA_INODE_LO   0x08
#define GD_CHECKSUM           0x1E
#define GD_TABELLA_INODE_HI   0x28

/* --- l'inode --- */
#define IN_MODO               0x00
#define IN_UID                0x02
#define IN_DIM_LO             0x04
#define IN_MTIME              0x10
#define IN_GID                0x18
#define IN_COLLEGAMENTI       0x1A
#define IN_BLOCCHI_LO         0x1C    /* in settori da 512 byte (di solito) */
#define IN_FLAG               0x20
#define IN_BLOCCO             0x28    /* i_block: 60 byte, il "dove sta il contenuto" */
#define IN_GENERAZIONE        0x64
#define IN_ACL_LO             0x68    /* il blocco degli attributi estesi */
#define IN_DIM_HI             0x6C
#define IN_UID_HI             0x78
#define IN_GID_HI             0x7A
#define IN_CHECKSUM_LO        0x7C
#define IN_EXTRA              0x80    /* i_extra_isize: quanti byte dopo i primi 128 */
#define IN_CHECKSUM_HI        0x82
#define IN_MTIME_EXTRA        0x88

#define IN_LUNG_BLOCCO        60

/* i_flags */
#define FL_INDICE             0x00001000  /* cartella con l'indice htree */
#define FL_EXTENT             0x00080000
#define FL_INLINE             0x10000000

/* il tipo nel modo Unix */
#define MODO_TIPO             0xF000
#define MODO_FILE             0x8000
#define MODO_CARTELLA         0x4000
#define MODO_COLLEGAMENTO     0xA000

/* --- un ext4 montato: i suoi dati (m->privato) --- */
struct ext4 {
    struct dispositivo_blocchi *disco;
    unsigned int byte_settore;
    unsigned int blocco;              /* byte di un blocco */
    unsigned int settori_blocco;
    unsigned long long blocchi;       /* quanti blocchi ha il disco */
    unsigned int primo_blocco;        /* s_first_data_block */
    unsigned int blocchi_gruppo, inode_gruppo, inode_totali, gruppi;
    unsigned int dim_inode, dim_descrittore;
    unsigned int primo_meta_bg;
    unsigned int copie_bg[2];
    unsigned int compat, incompat, ro_compat;
    int con_checksum;                 /* metadata_csum: crc32c su inode, extent, cartelle */
    unsigned int seme;                /* il punto di partenza di quei crc32c */
    unsigned char uuid[16];
    unsigned int avvisi;              /* quanti avvisi abbiamo stampato (poi si tace) */
};

/* --- un inode letto dal disco --- */
struct inode_ext4 {
    unsigned long long numero;
    unsigned char *grezzo;            /* i suoi dim_inode byte (kmalloc) */
    unsigned int extra;               /* i_extra_isize valido (0 se non c'e') */
    unsigned int modo, flag;
    unsigned long long dimensione;
    unsigned int seme;                /* seme dei checksum dei blocchi di questo inode */
};

/* ext4_crc.c. Il crc32c "grezzo" come lo usa ext4: niente inversione ne' all'inizio
 * ne' alla fine (il valore di partenza lo sceglie chi chiama, di solito ~0). */
unsigned int ext4_crc32c(unsigned int crc, const void *dati, unsigned int byte);
unsigned int ext4_crc16(unsigned int crc, const void *dati, unsigned int byte);

/* ext4.c */
void ext4_avviso(struct ext4 *e, const char *fmt, ...);
int ext4_leggi_blocco(struct ext4 *e, unsigned long long numero, void *dove);
int ext4_leggi_blocchi(struct ext4 *e, unsigned long long primo, unsigned int quanti,
                       void *dove);
int ext4_inode_apri(struct ext4 *e, unsigned long long numero, struct inode_ext4 *n);
void ext4_inode_chiudi(struct inode_ext4 *n);

/* ext4_dati.c */
/* Dove sta il blocco 'logico' del file: *fisico (0 = buco o extent non scritto: si
 * legge a zeri) e quanti blocchi da li' in poi continuano allo stesso modo. 'appoggio'
 * e' un blocco di memoria per leggere i nodi dell'albero. 0 bene, o VFS_ERR_... */
int ext4_mappa(struct ext4 *e, struct inode_ext4 *n, unsigned int logico,
               unsigned char *appoggio, unsigned long long *fisico, unsigned int *quanti);
/* Il valore dell'attributo "system.data" (la seconda parte dei dati in linea), se c'e'
 * dentro l'inode. 0 bene (lunghezza 0 se non c'e'), o VFS_ERR_FORMATO. */
int ext4_dati_in_linea(struct ext4 *e, struct inode_ext4 *n, const unsigned char **valore,
                       unsigned int *lunghezza);
/* Legge fino a 'byte' dal byte 'da' del contenuto: byte letti, o VFS_ERR_... */
long long ext4_leggi_dati(struct ext4 *e, struct inode_ext4 *n, unsigned long long da,
                          void *dove, unsigned int byte);

/* ext4_cartelle.c */
int ext4_cerca(struct ext4 *e, unsigned long long cartella, const char *nome,
               unsigned int lunghezza, unsigned long long *trovato);
int ext4_voce(struct ext4 *e, unsigned long long cartella, unsigned long long *cursore,
              struct voce_vfs *v);

#endif
