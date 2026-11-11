/* ELF: il formato dei file eseguibili, lo stesso di Linux.
 *
 * Un file ELF inizia con un'INTESTAZIONE: "sono un ELF, per questa CPU, il programma
 * parte da questo indirizzo, e i miei pezzi sono descritti in questa tabella".
 * La TABELLA DEI PEZZI (program header) elenca cosa va messo in memoria: per ogni
 * pezzo da caricare (PT_LOAD), da dove leggerlo nel file, a che indirizzo virtuale
 * metterlo, quanto e' grande nel file e quanto in memoria (la differenza sono le
 * variabili a zero, che nel file non stanno), e con quali permessi.
 *
 * Il kernel non ha bisogno d'altro: il resto del file (nomi delle funzioni, ecc.)
 * serve agli strumenti di sviluppo, non a far girare il programma. */
#ifndef ELF_H
#define ELF_H

/* Il formato dipende dalla CPU: ELF a 32 bit per x86 ("Intel 386"), ELF a 64 bit per
 * ARM64 ("AArch64"). I campi hanno gli stessi nomi; nella versione a 64 bit gli indirizzi
 * e le misure sono di 8 byte, e nella tabella dei pezzi i permessi stanno subito dopo il
 * tipo. Il caricatore (compiti.c) usa i nomi, e va bene per tutte e due. */
#ifdef __aarch64__
typedef unsigned long elf_parola;      /* indirizzi e misure: 8 byte */
#define ELF_CLASSE      2              /* 64 bit */
#define ELF_MACCHINA    183            /* AArch64 */
#define ELF_DESCRIZIONE "un eseguibile ELF a 64 bit per ARM64"
#else
typedef unsigned int elf_parola;       /* indirizzi e misure: 4 byte */
#define ELF_CLASSE      1              /* 32 bit */
#define ELF_MACCHINA    3              /* Intel 386 */
#define ELF_DESCRIZIONE "un eseguibile ELF a 32 bit per x86"
#endif

struct elf_intestazione {
    unsigned char  magico[4];      /* 0x7F 'E' 'L' 'F' */
    unsigned char  classe;         /* 1 = 32 bit, 2 = 64 bit */
    unsigned char  ordine;         /* 1 = little endian, come x86 e ARM */
    unsigned char  versione_id;
    unsigned char  resto_id[9];
    unsigned short tipo;           /* 2 = eseguibile */
    unsigned short macchina;       /* 3 = Intel 386, 183 = AArch64 */
    unsigned int   versione;
    elf_parola     inizio;         /* l'indirizzo da cui parte il programma */
    elf_parola     tabella_pezzi;  /* dove sta, nel file, la tabella dei pezzi */
    elf_parola     tabella_sezioni;
    unsigned int   opzioni;
    unsigned short dim_intestazione;
    unsigned short dim_pezzo;      /* quanto e' grande una riga della tabella dei pezzi */
    unsigned short numero_pezzi;
    unsigned short dim_sezione, numero_sezioni, nomi_sezioni;
} __attribute__((packed));

struct elf_pezzo {
    unsigned int tipo;             /* 1 = PT_LOAD: da caricare in memoria */
#ifdef __aarch64__
    unsigned int permessi;         /* PF_X, PF_W, PF_R (a 64 bit stanno qui) */
#endif
    elf_parola   nel_file;         /* da dove leggerlo nel file */
    elf_parola   virtuale;         /* a che indirizzo metterlo */
    elf_parola   fisico;           /* (non usato) */
    elf_parola   byte_file;        /* quanti byte leggere dal file */
    elf_parola   byte_memoria;     /* quanti byte occupa in memoria (>= byte_file) */
#ifndef __aarch64__
    unsigned int permessi;         /* PF_X, PF_W, PF_R (a 32 bit stanno qui) */
#endif
    elf_parola   allineamento;
} __attribute__((packed));

#define ELF_ESEGUIBILE 2
#define PT_LOAD        1
#define PF_W           0x2         /* il pezzo si puo' scrivere */

#endif
