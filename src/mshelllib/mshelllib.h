// mshelllib: libreria di mshell, codice condiviso tra i programmi del sistema.
// Ogni funzione sta in un suo file f_<nome>.c in questa cartella; qui ci sono le loro dichiarazioni.
// Per usarla: #include "mshelllib.h" (build.sh compila con -I src/mshelllib e unisce src/mshelllib/*.c).

#ifndef MSHELLLIB_H
#define MSHELLLIB_H

#include <stddef.h>
#include <sys/types.h>

// --- Posti fissi del sistema ------------------------------------------------
// Scritti una volta sola qui: chi li usa li prende da questa libreria.
#define FILE_UTENTI       "/system/userlist"         // elenco degli utenti
#define CARTELLA_UTENTI   "/system/users"            // le cartelle degli utenti: /system/users/<uid>
#define SOCKET_SERVICENTER "/system/servicenter.sock" // canale delle richieste a servicenter

// --- Utenti -----------------------------------------------------------------
// Un utente letto dal file degli utenti, in stile /etc/passwd di Linux. Una riga per utente:
//   nome:password:uid:gid:descrizione:cartella:programma_da_avviare
struct utente {
    char nome[32];
    char password[128];         // "x" = non impostata; altrimenti l'impronta prodotta da crypt()
    unsigned uid, gid;
    char descrizione[64];
    char cartella[128];         // radice del suo mondo (vuota per l'amministratore)
    char programma[128];        // punto d'ingresso, col percorso che vede lui
};

// Cerca un utente per nome nel file degli utenti.
// Restituisce 0 se lo trova, -1 se non c'e' o il file non si puo' leggere.
int leggi_utente(const char *nome, struct utente *u);

// --- Scrittura --------------------------------------------------------------
// Scrive tutti i byte su un descrittore: write() puo' scriverne meno di quelli chiesti,
// o essere interrotta da un segnale; questa ripete finche' ha finito.
// Restituisce 0, oppure -1 (errno indica il motivo).
int scrivi_tutto(int fd, const void *dati, size_t lunghezza);

// Scrive un file in modo che, anche se la corrente manca a meta',
// dopo il riavvio il file sia o intero (nuovo) o com'era prima: mai a meta'.
// Restituisce 0 se va tutto bene, -1 in caso di errore (errno indica il motivo).
int scrivi_file_sicuro(const char *percorso, const void *dati, size_t lunghezza, mode_t permessi);

// Una cartella (o un file) da far comparire nel mondo di un processo.
struct aggancio {
    const char *sorgente;       // dove sta davvero, per esempio /system/bin
    const char *destinazione;   // dove compare nel mondo, per esempio /bin
    int sola_lettura;           // 1 = nel mondo non si puo' modificare
};

// Chiude il processo nella cartella radice (che per lui diventa "/"),
// dopo averci agganciato le cose del sistema indicate.
// Da chiamare da root, prima di cambiare utente. Restituisce 0, oppure -1 (errno indica il motivo).
int crea_mondo(const char *radice, const struct aggancio *agganci, int n);

// --- Console ----------------------------------------------------------------
// Collega il processo a una console (es. /dev/tty1): nuova sessione con quella console
// come terminale (cosi' Ctrl+C arriva a lui) e la console come entrata, uscita ed errori.
// Restituisce 0, oppure -1 (errno indica il motivo).
int collega_console(const char *console);

#endif
