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

/* Su tutte e due le CPU il formato e' ELF a 64 bit: indirizzi e misure di 8 byte. Cambia
 * solo il numero della macchina scritto nell'intestazione. */
typedef unsigned long elf_parola;      /* indirizzi e misure: 8 byte */
#define ELF_CLASSE      2              /* 64 bit */
#ifdef __aarch64__
#define ELF_MACCHINA    183            /* AArch64 */
#define ELF_DESCRIZIONE "un eseguibile ELF a 64 bit per ARM64"
#else
#define ELF_MACCHINA    62             /* x86-64 */
#define ELF_DESCRIZIONE "un eseguibile ELF a 64 bit per x86-64"
#endif

struct elf_intestazione {
    unsigned char  magico[4];      /* 0x7F 'E' 'L' 'F' */
    unsigned char  classe;         /* 1 = 32 bit, 2 = 64 bit */
    unsigned char  ordine;         /* 1 = little endian, come x86 e ARM */
    unsigned char  versione_id;
    unsigned char  resto_id[9];
    unsigned short tipo;           /* 2 = eseguibile */
    unsigned short macchina;       /* 62 = x86-64, 183 = AArch64 */
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
    unsigned int permessi;         /* PF_X, PF_W, PF_R */
    elf_parola   nel_file;         /* da dove leggerlo nel file */
    elf_parola   virtuale;         /* a che indirizzo metterlo */
    elf_parola   fisico;           /* (non usato) */
    elf_parola   byte_file;        /* quanti byte leggere dal file */
    elf_parola   byte_memoria;     /* quanti byte occupa in memoria (>= byte_file) */
    elf_parola   allineamento;
} __attribute__((packed));

#define ELF_ESEGUIBILE 2
#define PT_LOAD        1
#define PF_W           0x2         /* il pezzo si puo' scrivere */

#endif
