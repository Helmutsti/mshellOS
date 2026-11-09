/* ELF: il formato dei file eseguibili, lo stesso di Linux (la versione a 32 bit).
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

struct elf_intestazione {
    unsigned char  magico[4];      /* 0x7F 'E' 'L' 'F' */
    unsigned char  classe;         /* 1 = 32 bit */
    unsigned char  ordine;         /* 1 = little endian, come l'x86 */
    unsigned char  versione_id;
    unsigned char  resto_id[9];
    unsigned short tipo;           /* 2 = eseguibile */
    unsigned short macchina;       /* 3 = Intel 386 */
    unsigned int   versione;
    unsigned int   inizio;         /* l'indirizzo da cui parte il programma */
    unsigned int   tabella_pezzi;  /* dove sta, nel file, la tabella dei pezzi */
    unsigned int   tabella_sezioni;
    unsigned int   opzioni;
    unsigned short dim_intestazione;
    unsigned short dim_pezzo;      /* quanto e' grande una riga della tabella dei pezzi */
    unsigned short numero_pezzi;
    unsigned short dim_sezione, numero_sezioni, nomi_sezioni;
} __attribute__((packed));

struct elf_pezzo {
    unsigned int tipo;             /* 1 = PT_LOAD: da caricare in memoria */
    unsigned int nel_file;         /* da dove leggerlo nel file */
    unsigned int virtuale;         /* a che indirizzo metterlo */
    unsigned int fisico;           /* (non usato) */
    unsigned int byte_file;        /* quanti byte leggere dal file */
    unsigned int byte_memoria;     /* quanti byte occupa in memoria (>= byte_file) */
    unsigned int permessi;         /* PF_X, PF_W, PF_R */
    unsigned int allineamento;
} __attribute__((packed));

#define ELF_ESEGUIBILE 2
#define ELF_386        3
#define PT_LOAD        1
#define PF_W           0x2         /* il pezzo si puo' scrivere */

#endif
