// Comandi interni della shell: sono scritti dentro la shell, non sono programmi su disco.
// Stanno qui perche' agiscono sulla shell stessa (la sua cartella corrente, la sua fine),
// cosa che un programma separato non potrebbe fare.
//
// Ogni comando riceve le parole della riga (parole[0] e' il nome del comando)
// e restituisce CONTINUA oppure ESCI.
#ifndef COMANDI_H
#define COMANDI_H

#define CONTINUA 0
#define ESCI     1

struct comando {
    const char *nome;
    int (*esegui)(char *parole[]);
};

int comando_cd(char *parole[]);
int comando_exit(char *parole[]);

#endif
