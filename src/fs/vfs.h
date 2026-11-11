/* IL FILE SYSTEM VIRTUALE: lo strato tra i programmi e i formati dei dischi.
 *
 * I programmi conoscono solo le operazioni sui file (apri, leggi, elenca, informazioni...)
 * e un unico albero di cartelle. Sotto, ogni cartella "montata" ha il suo FORMATO
 * (ext4 oggi; FAT32 e altri domani) su un suo DISCO. Il file system virtuale (vfs.c)
 * tiene la tabella dei montaggi, percorre i percorsi, controlla i permessi e passa ogni
 * richiesta al formato giusto. Un formato non sa niente di percorsi, descrittori o
 * programmi: risponde solo a domande su NODI (file, cartelle, collegamenti) indicati
 * da un numero, come gli inode di Unix. (Linux, macOS e i BSD sono fatti cosi'.)
 *
 * Tre pezzi:
 *   - struct dispositivo_blocchi: un disco, visto come fila di settori (lo da' il driver:
 *     ATA, virtio-blk, domani NVMe...). La memoria dei blocchi letti di recente (cache)
 *     sta sopra il dispositivo, trasparente per i formati;
 *   - struct tipo_fs: un formato, con le sue operazioni;
 *   - struct montato: un formato montato su un disco, con i dati privati del formato.
 *
 * TAPPE. Prima la lettura: le operazioni di scrittura ci sono gia' nella struttura, ma
 * un formato che non le sa fare le lascia a 0, e il file system virtuale risponde
 * VFS_ERR_SOLA_LETTURA. */
#ifndef VFS_H
#define VFS_H

/* --- gli errori: sempre negativi --- */
#define VFS_ERR_DISCO          -1    /* il disco ha dato errore */
#define VFS_ERR_FORMATO        -2    /* il disco non ha questo formato, o e' rovinato */
#define VFS_ERR_NON_SUPPORTATO -3    /* il formato usa una funzione che non sappiamo gestire */
#define VFS_ERR_NON_TROVATO    -4
#define VFS_ERR_ESISTE         -5
#define VFS_ERR_NON_CARTELLA   -6
#define VFS_ERR_E_CARTELLA     -7
#define VFS_ERR_NON_VUOTA      -8
#define VFS_ERR_PIENO          -9
#define VFS_ERR_NOME          -10    /* nome troppo lungo o scritto male */
#define VFS_ERR_PERMESSO      -11
#define VFS_ERR_SOLA_LETTURA  -12
#define VFS_ERR_MEMORIA       -13    /* il kernel ha finito la memoria */
#define VFS_ERR_TROPPI_LINK   -14    /* troppi collegamenti simbolici di fila */

/* --- un disco --- */
struct dispositivo_blocchi {
    const char *nome;                 /* es. "ata0", "virtio0" */
    unsigned int byte_settore;        /* quasi sempre 512 */
    unsigned long long settori;       /* quanti */
    /* Leggono/scrivono 'quanti' settori consecutivi dal numero 'primo'. 0 bene, -1 errore.
     * 'dove'/'da' e' memoria del kernel (indirizzo virtuale). */
    int (*leggi)(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                 void *dove);
    int (*scrivi)(struct dispositivo_blocchi *d, unsigned long long primo, unsigned int quanti,
                  const void *da);
    void *privato;                    /* del driver */
};

/* --- che cos'e' un nodo, e le sue informazioni --- */
#define VFS_FILE        1
#define VFS_CARTELLA    2
#define VFS_COLLEGAMENTO 3            /* collegamento simbolico: il contenuto e' un percorso */
#define VFS_ALTRO       4             /* dispositivi, code, socket del disco: non li apriamo */

#define VFS_NOME_MAX 255              /* byte di un nome (UTF-8), senza lo 0 finale */

struct info_vfs {
    unsigned int tipo;                /* VFS_... */
    unsigned int permessi;            /* i 12 bit bassi del modo Unix (es. 0755) */
    unsigned int proprietario, gruppo;
    unsigned int collegamenti;        /* quanti nomi puntano al nodo */
    unsigned long long dimensione;    /* byte */
    long long modificato;             /* secondi dal 1/1/1970 (UTC) */
};

struct voce_vfs {
    unsigned long long nodo;
    unsigned int tipo;                /* VFS_... (0 se il formato non lo sa senza leggere il nodo) */
    unsigned int lunghezza;           /* byte del nome */
    char nome[VFS_NOME_MAX + 1];      /* finisce con 0 */
};

struct montato;

/* --- un formato --- */
struct tipo_fs {
    const char *nome;                 /* "ext4" */

    /* Monta il formato sul disco 'd': legge e controlla la descrizione del disco, mette i
     * suoi dati in m->privato e il numero della cartella principale in m->radice.
     * 'sola_lettura': non scrivere niente sul disco. 0 bene, o VFS_ERR_... */
    int (*monta)(struct montato *m, struct dispositivo_blocchi *d, int sola_lettura);
    void (*smonta)(struct montato *m);

    /* Cerca 'nome' ('lunghezza' byte, senza '/') nella cartella 'cartella'. */
    int (*cerca)(struct montato *m, unsigned long long cartella, const char *nome,
                 unsigned int lunghezza, unsigned long long *trovato);
    int (*info)(struct montato *m, unsigned long long nodo, struct info_vfs *i);
    /* Legge fino a 'byte' dal byte 'da' del file (o del collegamento: il percorso). Byte
     * letti (0 alla fine del file), o VFS_ERR_... I buchi dei file sparsi si leggono a 0. */
    long long (*leggi)(struct montato *m, unsigned long long nodo, unsigned long long da,
                       void *dove, unsigned int byte);
    /* Le voci di una cartella, una alla volta. '*cursore' parte da 0 e lo aggiorna il
     * formato. 1 = una voce in *v, 0 = finite, o VFS_ERR_... Salta "." e "..". */
    int (*voce)(struct montato *m, unsigned long long cartella, unsigned long long *cursore,
                struct voce_vfs *v);

    /* --- scrittura (tappa 2): 0 se il formato non la sa fare --- */
    long long (*scrivi)(struct montato *m, unsigned long long nodo, unsigned long long da,
                        const void *dati, unsigned int byte);
    int (*tronca)(struct montato *m, unsigned long long nodo, unsigned long long dimensione);
    int (*crea)(struct montato *m, unsigned long long cartella, const char *nome,
                unsigned int lunghezza, unsigned int tipo, unsigned int permessi,
                unsigned int proprietario, unsigned long long *creato);
    int (*cancella)(struct montato *m, unsigned long long cartella, const char *nome,
                    unsigned int lunghezza);
    int (*rinomina)(struct montato *m, unsigned long long da_cartella, const char *da_nome,
                    unsigned int da_lunghezza, unsigned long long a_cartella,
                    const char *a_nome, unsigned int a_lunghezza);
    int (*cambia)(struct montato *m, unsigned long long nodo, unsigned int permessi,
                  unsigned int proprietario);
    int (*sincronizza)(struct montato *m);   /* scrive sul disco quello che e' in sospeso */
};

/* --- un formato montato --- */
struct montato {
    const struct tipo_fs *tipo;
    struct dispositivo_blocchi *disco;
    unsigned long long radice;        /* il nodo della cartella principale */
    int sola_lettura;
    void *privato;                    /* del formato */
};

#endif
