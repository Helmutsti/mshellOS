// exit: chiude la shell. init se ne accorge e la riavvia.
#include "comandi.h"

int comando_exit(char *parole[]) {
    (void)parole;
    return ESCI;
}
