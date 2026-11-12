/* La descrizione di un'applicazione: il suo MANIFESTO, <cartella delle app>/<id>/manifest.toml.
 *
 * LE CARTELLE DELLE APPLICAZIONI sono due, e si guardano in quest'ordine:
 *     /system/apps   quelle del sistema (nella partizione system, in sola lettura)
 *     /data/apps     quelle installate dalla persona (nella partizione data)
 * Un'app installata non puo' avere l'identificativo di una del sistema (pacchetto.h lo
 * rifiuta); se capitasse lo stesso, vale quella del sistema.
 * Ogni applicazione e' una cartella che si chiama come il suo identificativo, con dentro
 * il programma (di solito "code"), manifest.toml e resources/ (le icone e i suoi dati).
 *
 * IL MANIFESTO e' scritto in TOML: la guida, voce per voce, e' manifest-guida.toml nella
 * cartella principale del progetto; le regole sono in APPLICAZIONI.md. Lo legge tomlc99
 * (esterni/tomlc99, licenza MIT) in descrizione.c. Le voci:
 *     id, nome, versione, autore, descrizione       chi e' l'app
 *     processore, sistema_minimo                     per quale computer e sistema e' fatta
 *     programma                                      il file da far partire (di solito "code")
 *     permessi, deviazioni, funzioni                 elenchi di nomi (APPLICAZIONI.md, sezione 9)
 *     [icone.chiaro], [icone.scuro]                  svg = "...", png = { 18 = "...", 32 = "..." }
 *
 * Chi lo usa: il launcher (per far partire l'app), la scrivania (per mostrarla) e
 * pacchetto.c (per controllare e installare un pacchetto .app).
 *
 * Questo header non tira dentro la libreria C: lo possono includere anche i programmi. */
#ifndef DESCRIZIONE_H
#define DESCRIZIONE_H

#include "chiamate.h"   /* MAX_PERCORSO_PROGRAMMA */

/* LA VERSIONE DEL SISTEMA. Un'app dice nel manifesto la versione minima che le serve
 * (sistema_minimo): se e' piu' alta di questa, non si installa. Si alza quando il sistema
 * offre alle app qualcosa che prima non c'era. */
#define VERSIONE_SISTEMA 1

/* IL PROCESSORE di questo computer, scritto come nel manifesto. */
#if defined(__x86_64__)
#define PROCESSORE_QUI "x86_64"
#elif defined(__aarch64__)
#define PROCESSORE_QUI "arm64"
#else
#error "processore sconosciuto: mshell e' fatto per x86_64 e arm64"
#endif

#define NOME_MANIFESTO "manifest.toml"

/* Le misure dei campi (compreso lo 0 finale). Un valore piu' lungo e' un errore del
 * manifesto, non si taglia. */
#define MAX_ID           33     /* 32 lettere: minuscole, cifre, trattini */
#define MAX_NOME_APP     64
#define MAX_VERSIONE     24     /* "1.0.0": numeri separati da punti */
#define MAX_AUTORE       64
#define MAX_DESCRIZIONE 256
#define MAX_PROCESSORE   16
#define MAX_PROGRAMMA    16     /* un file della cartella dell'app, senza "/" */
#define MAX_VOCI          8     /* permessi, deviazioni, funzioni: quanti al massimo */
#define MAX_VOCE         32     /* la lunghezza di ognuno */
#define MAX_PNG           4     /* le PNG di un tema */
#define MAX_PERCORSO_PACCHETTO 96   /* un file dentro il pacchetto (per esempio un'icona) */

#define TEMA_CHIARO 0
#define TEMA_SCURO  1

struct icone_tema {
    char svg[MAX_PERCORSO_PACCHETTO];        /* "" se non c'e' */
    int quante_png;
    int png_misura[MAX_PNG];                 /* il lato in pixel: 18, 32... */
    char png[MAX_PNG][MAX_PERCORSO_PACCHETTO];
};

/* Le voci, per dire quali ci sono (presenti) e quali sono scritte male (sbagliate). */
#define VOCE_ID              (1u << 0)
#define VOCE_NOME            (1u << 1)
#define VOCE_VERSIONE        (1u << 2)
#define VOCE_AUTORE          (1u << 3)
#define VOCE_DESCRIZIONE     (1u << 4)
#define VOCE_PROCESSORE      (1u << 5)
#define VOCE_SISTEMA_MINIMO  (1u << 6)
#define VOCE_PROGRAMMA       (1u << 7)
#define VOCE_PERMESSI        (1u << 8)
#define VOCE_DEVIAZIONI      (1u << 9)
#define VOCE_FUNZIONI        (1u << 10)
#define VOCE_ICONE           (1u << 11)

struct manifesto {
    char id[MAX_ID];
    char nome[MAX_NOME_APP];
    char versione[MAX_VERSIONE];
    char autore[MAX_AUTORE];
    char descrizione[MAX_DESCRIZIONE];
    char processore[MAX_PROCESSORE];
    int sistema_minimo;                      /* 0 se manca */
    char programma[MAX_PROGRAMMA];
    int quanti_permessi, quante_deviazioni, quante_funzioni;
    char permessi[MAX_VOCI][MAX_VOCE];
    char deviazioni[MAX_VOCI][MAX_VOCE];
    char funzioni[MAX_VOCI][MAX_VOCE];
    struct icone_tema icone[2];              /* TEMA_CHIARO, TEMA_SCURO */
    unsigned int presenti;                   /* VOCE_...: quelle scritte nel manifesto */
    unsigned int sbagliate;                  /* VOCE_...: scritte, ma del tipo sbagliato o troppo lunghe */
};

/* Legge un manifesto dal suo testo (finito da uno 0; tomlc99 lo modifica). 0 se e' TOML
 * valido (anche se mancano delle voci: lo dicono presenti e sbagliate), -1 se no, con
 * in 'errore' (se non e' 0) cosa non va, come lo dice tomlc99 (in inglese). */
int manifesto_leggi(char *testo, struct manifesto *m, char *errore, int max_errore);

/* Il manifesto dell'app installata nella cartella 'cartella_app' (il percorso completo,
 * per esempio /system/apps/files). 0, o -1 se non c'e' o non si legge. */
int manifesto_dell_app(const char *cartella_app, struct manifesto *m);

/* 1 se l'identificativo va bene: da 1 a 32 lettere, minuscole, cifre e trattini, che
 * non comincia con un trattino. */
int id_valido(const char *id);

/* 1 se la versione va bene: numeri separati da punti ("1", "1.0", "2.10.3"). */
int versione_valida(const char *v);

/* Confronta due versioni valide numero per numero ("1.10" e' dopo "1.9"; "1.0" e "1"
 * sono uguali): negativo se a viene prima, 0 se sono uguali, positivo se dopo. */
int confronta_versioni(const char *a, const char *b);

/* 1 se 'nome' va bene come programma: un file della cartella dell'app (niente "/"),
 * minuscole, cifre, '.', '_' e '-', che non comincia con un punto. */
int programma_valido(const char *nome);

/* LE CARTELLE DELLE APPLICAZIONI, nell'ordine in cui si guardano. Sono variabili (e non
 * costanti) solo perche' le prove sul computer che compila le spostano in una cartella
 * di prova (test/pacchetti/). */
#define QUANTE_CARTELLE_APPLICAZIONI 2
#define CARTELLA_APP_SISTEMA 0
#define CARTELLA_APP_DATI    1
extern const char *cartelle_applicazioni[QUANTE_CARTELLE_APPLICAZIONI];

/* QUELLO CHE SERVE A LAUNCHER E SCRIVANIA di un'app installata. */
struct descrizione {
    char id[MAX_ID];
    char nome[MAX_NOME_APP];
    char programma[MAX_PERCORSO_PROGRAMMA];   /* il percorso completo: /system/apps/<id>/code */
    int di_sistema;                 /* 1 se sta in /system/apps */
};

/* Legge il manifesto dell'applicazione 'app' (il suo identificativo, che e' anche il nome
 * della sua cartella): prima in /system/apps, poi in /data/apps. 0 se c'e', il suo id
 * corrisponde alla cartella e dice quale programma far partire; -1 se no. */
int leggi_descrizione(const char *app, struct descrizione *d);

/* --- piccole funzioni sui testi, senza la libreria C --- */

static inline int descrizione_uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static inline void descrizione_copia(char *dove, const char *da, int massimo) {
    int i = 0;
    for (; i < massimo - 1 && da[i]; i++) dove[i] = da[i];
    dove[i] = 0;
}

static inline void descrizione_aggiungi(char *dove, const char *da, int massimo) {
    int n = 0;
    while (dove[n]) n++;
    descrizione_copia(dove + n, da, massimo - n);
}

#endif
