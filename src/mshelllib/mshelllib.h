// mshelllib: libreria di mshell, codice condiviso tra i programmi del sistema.
// Ogni funzione sta in un suo file f_<nome>.c in questa cartella; qui ci sono le loro dichiarazioni.
// Per usarla: #include "mshelllib.h" (build.sh compila con -I src/mshelllib e unisce src/mshelllib/*.c).

#ifndef MSHELLLIB_H
#define MSHELLLIB_H

#include <stddef.h>
#include <sys/types.h>

// Scrive un file in modo che, anche se la corrente manca a meta',
// dopo il riavvio il file sia o intero (nuovo) o com'era prima: mai a meta'.
// Restituisce 0 se va tutto bene, -1 in caso di errore (errno indica il motivo).
int scrivi_file_sicuro(const char *percorso, const void *dati, size_t lunghezza, mode_t permessi);

#endif
