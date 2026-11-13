/* La libreria dei programmi: i pacchetti .app (pacchetto.h).
 *
 * Lo ZIP lo legge miniz (esterni/miniz: release 3.0.2, licenza MIT), direttamente dal file
 * aperto con pread: si legge l'indice (il "central directory", in fondo all'archivio) e, di
 * ogni file, solo quello che serve. Il manifesto lo legge descrizione.c (tomlc99).
 *
 * Usa solo la libreria C e le chiamate di Linux, non libreria.h: cosi' si compila anche
 * sul computer che compila, per le prove (test/pacchetti/).
 *
 * SICUREZZA DELLO SCOMPATTAMENTO ("zip slip"). Un archivio puo' contenere nomi come
 * "../../system/x" o "/data/system/x", o collegamenti simbolici che puntano fuori: chi
 * scompatta senza guardare scrive dove non dovrebbe. Qui un nome si accetta solo se e'
 * relativo, fatto di pezzi normali (niente "", ".", "..", "\", caratteri di controllo) e
 * non e' un collegamento (i permessi Unix che lo ZIP si porta dietro): altrimenti tutto il
 * pacchetto e' rifiutato, gia' quando lo si descrive. I file si creano con O_EXCL e
 * O_NOFOLLOW dentro una cartella appena fatta, che solo il sistema puo' scrivere. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

#include "pacchetto.h"
/* miniz senza le funzioni sui FILE della libreria C e senza i nomi di zlib: le stesse
 * scelte con cui build.sh compila esterni/miniz/miniz.c */
#define MINIZ_NO_STDIO
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "esterni/miniz/miniz.h"

const char *cartella_persone = "/data/users";

/* La persona: per ora una sola (chiamate.h, UID_APPLICAZIONI). */
#define UID_PERSONA 1000

#define MAX_FILE_PACCHETTO   4096
#define MAX_BYTE_PACCHETTO   (512ull << 20)    /* scompattato: 512 MB */
#define MAX_BYTE_MANIFESTO   16384
#define MAX_NOME_IN_PACCHETTO 200
#define MARGINE_SPAZIO       (1ull << 20)      /* 1 MB di disco da lasciare libero */

/* --- i nomi che il sistema conosce (APPLICAZIONI.md, sezione 9) --- */

static const char *const permessi_normali[] = { "file.leggere", "file.scrivere", "notifiche", "rete", "audio", 0 };
static const struct { const char *nome, *cosa; } permessi_sistema[] = {
    { "processi", "vedere i programmi in esecuzione e fermarli" },
    { "impostazioni.sistema", "cambiare le impostazioni del sistema" },
    { "spegnimento", "spegnere e riavviare il computer" },
    { "app.gestire", "installare, aggiornare e togliere app" },
    { 0, 0 },
};
static const struct { const char *nome, *avviso; } deviazioni_note[] = {
    { "senza_finestra", "Quest'app non mostra nessuna finestra: mentre lavora la trovi tra quelle attive in Attivit\xc3\xa0." },
    { "sottofondo", "Quest'app resta accesa anche dopo che chiudi la sua finestra: finch\xc3\xa9 \xc3\xa8 accesa vedrai un segno." },
    { "avvio_automatico", "Quest'app parte da sola quando entri: la puoi spegnere dalle Impostazioni." },
    { "schermo_intero", "Quest'app pu\xc3\xb2 coprire tutto lo schermo, barra compresa: c'\xc3\xa8 sempre un tasto per uscire." },
    { 0, 0 },
};
static const char *const funzioni_note[] = {
    "file.apri_cartella", "file.mostra", "web.apri", "mail.scrivi", "immagine.apri",
    "testo.apri", "testo.nuovo", "terminale.apri", 0,
};

static int in_elenco(const char *const *elenco, const char *nome) {
    for (; *elenco; elenco++) if (!strcmp(*elenco, nome)) return 1;
    return 0;
}

static const char *permesso_di_sistema(const char *nome) {
    for (int i = 0; permessi_sistema[i].nome; i++)
        if (!strcmp(permessi_sistema[i].nome, nome)) return permessi_sistema[i].cosa;
    return 0;
}

/* --- il resoconto --- */

static void problema(struct resoconto *r, int blocca, const char *voce, const char *formato, ...) {
    if (blocca) r->quanti_errori++; else r->quanti_avvisi++;
    if (r->quanti >= MAX_PROBLEMI) return;
    struct problema *p = &r->problemi[r->quanti];
    if (r->quanti == MAX_PROBLEMI - 1) {          /* l'ultimo posto: si dice che ce ne sono altri */
        p->blocca = r->quanti_errori > 0;
        snprintf(p->voce, sizeof p->voce, "altri");
        snprintf(p->testo, sizeof p->testo, "Ci sono anche altri problemi, che qui non stanno.");
        r->quanti++;
        return;
    }
    p->blocca = blocca;
    snprintf(p->voce, sizeof p->voce, "%s", voce);
    va_list a;
    va_start(a, formato);
    vsnprintf(p->testo, sizeof p->testo, formato, a);
    va_end(a);
    r->quanti++;
}

#define SEGNA_ERRORE(r, voce, ...) problema(r, 1, voce, __VA_ARGS__)
#define SEGNA_AVVISO(r, voce, ...) problema(r, 0, voce, __VA_ARGS__)

/* --- lo ZIP, letto dal file aperto --- */

struct archivio {
    mz_zip_archive zip;
    int f;
    int quanti;                       /* i file nell'indice */
};

static size_t leggi_pezzo(void *dati, mz_uint64 dove, void *buffer, size_t n) {
    struct archivio *a = dati;
    size_t fatti = 0;
    while (fatti < n) {
        ssize_t l = pread(a->f, (char *)buffer + fatti, n - fatti, (off_t)(dove + fatti));
        if (l < 0 && errno == EINTR) continue;
        if (l <= 0) break;
        fatti += (size_t)l;
    }
    return fatti;
}

static int apri_archivio(struct archivio *a, int f) {
    struct stat s;
    memset(a, 0, sizeof *a);
    a->f = f;
    if (fstat(f, &s) < 0 || !S_ISREG(s.st_mode) || s.st_size <= 0) return -1;
    a->zip.m_pRead = leggi_pezzo;
    a->zip.m_pIO_opaque = a;
    if (!mz_zip_reader_init(&a->zip, (mz_uint64)s.st_size, 0)) return -1;
    a->quanti = (int)mz_zip_reader_get_num_files(&a->zip);
    return 0;
}

static void chiudi_archivio(struct archivio *a) { mz_zip_reader_end(&a->zip); }

/* L'indice del file 'nome' nell'archivio (confronto esatto), o -1. */
static int cerca(struct archivio *a, const char *nome) {
    mz_zip_archive_file_stat s;
    for (int i = 0; i < a->quanti; i++)
        if (mz_zip_reader_file_stat(&a->zip, (mz_uint)i, &s) && !s.m_is_directory && !strcmp(s.m_filename, nome))
            return i;
    return -1;
}

/* Un nome dell'archivio si puo' scompattare? 1 se si'. I nomi delle cartelle finiscono con
 * "/" (l'ultimo pezzo vuoto e' permesso solo li'). */
static int nome_sicuro(const char *nome) {
    size_t l = strlen(nome);
    if (l == 0 || l >= MAX_NOME_IN_PACCHETTO || nome[0] == '/') return 0;
    const char *pezzo = nome;
    for (const char *c = nome;; c++) {
        if (*c == '/' || *c == 0) {
            size_t n = (size_t)(c - pezzo);
            int ultimo_di_cartella = *c == 0 && c > nome && c[-1] == '/';
            if (!ultimo_di_cartella) {
                if (n == 0) return 0;                                  /* "a//b" */
                if (n == 1 && pezzo[0] == '.') return 0;               /* "." */
                if (n == 2 && pezzo[0] == '.' && pezzo[1] == '.') return 0;   /* ".." */
            }
            if (*c == 0) return 1;
            pezzo = c + 1;
        } else if (*c == '\\' || (unsigned char)*c < 0x20 || *c == 0x7f) {
            return 0;
        }
    }
}

/* Il file e' un collegamento simbolico? Lo dicono i permessi Unix che lo ZIP si porta
 * dietro (i 16 bit alti degli "attributi esterni", quando l'archivio e' fatto su Unix). */
static int e_collegamento(const mz_zip_archive_file_stat *s) {
    int sistema = (int)(s->m_version_made_by >> 8);
    return sistema == 3 /* Unix */ && ((s->m_external_attr >> 16) & 0170000) == 0120000;
}

/* --- i controlli --- */

static const char *spiega_processore(const char *p) {
    if (!strcmp(p, "x86_64")) return "per PC (x86_64)";
    if (!strcmp(p, "arm64")) return "per ARM (arm64)";
    return 0;
}

/* L'indice dell'archivio: nomi sicuri, niente collegamenti, niente cifrati, quanto pesa. */
static void controlla_indice(struct archivio *a, struct resoconto *r) {
    if (a->quanti > MAX_FILE_PACCHETTO) {
        SEGNA_ERRORE(r, "pacchetto", "Il pacchetto contiene troppi file (pi\xc3\xb9 di %d): non si pu\xc3\xb2 installare.",
               MAX_FILE_PACCHETTO);
        return;
    }
    mz_zip_archive_file_stat s;
    for (int i = 0; i < a->quanti; i++) {
        if (!mz_zip_reader_file_stat(&a->zip, (mz_uint)i, &s)) {
            SEGNA_ERRORE(r, "pacchetto", "L'indice del pacchetto \xc3\xa8 rovinato e non si riesce a leggere: l'app non si pu\xc3\xb2 installare.");
            return;
        }
        if (!nome_sicuro(s.m_filename)) {
            SEGNA_ERRORE(r, "sicurezza", "Il pacchetto contiene un file con un percorso che esce dalla cartella dell'app "
                   "(\xc2\xab%.80s\xc2\xbb). \xc3\x88 un trucco noto per scrivere file dove non si dovrebbe: per sicurezza "
                   "l'app non si installa.", s.m_filename);
            continue;
        }
        if (e_collegamento(&s)) {
            SEGNA_ERRORE(r, "sicurezza", "Il pacchetto contiene un collegamento (\xc2\xab%.80s\xc2\xbb). Le app non possono "
                   "contenere collegamenti, perch\xc3\xa9 potrebbero puntare a file fuori dalla loro cartella: per "
                   "sicurezza l'app non si installa.", s.m_filename);
            continue;
        }
        if (!s.m_is_directory && (s.m_is_encrypted || !s.m_is_supported)) {
            SEGNA_ERRORE(r, "pacchetto", "Il file \xc2\xab%.80s\xc2\xbb del pacchetto \xc3\xa8 cifrato o compresso in un modo che "
                   "il sistema non conosce: l'app non si pu\xc3\xb2 installare.", s.m_filename);
            continue;
        }
        r->byte += s.m_uncomp_size;
    }
    if (r->byte > MAX_BYTE_PACCHETTO)
        SEGNA_ERRORE(r, "pacchetto", "Il pacchetto, scompattato, sarebbe troppo grande (pi\xc3\xb9 di %llu MB): non si "
               "pu\xc3\xb2 installare.", MAX_BYTE_PACCHETTO >> 20);
}

/* Il manifesto: lo si prende dall'archivio e lo si legge. 0 se letto. */
static int leggi_manifesto(struct archivio *a, struct resoconto *r) {
    int i = cerca(a, NOME_MANIFESTO);
    if (i < 0) {
        SEGNA_ERRORE(r, "manifesto", "Nel pacchetto manca il manifesto (manifest.toml), la scheda che dice cos'\xc3\xa8 "
               "l'app e cosa le serve: senza, l'app non si pu\xc3\xb2 installare.");
        return -1;
    }
    mz_zip_archive_file_stat s;
    if (!mz_zip_reader_file_stat(&a->zip, (mz_uint)i, &s) || s.m_uncomp_size >= MAX_BYTE_MANIFESTO) {
        SEGNA_ERRORE(r, "manifesto", "Il manifesto \xc3\xa8 troppo lungo (pi\xc3\xb9 di 16 KB): l'app non si pu\xc3\xb2 installare.");
        return -1;
    }
    size_t n = 0;
    char *dati = mz_zip_reader_extract_to_heap(&a->zip, (mz_uint)i, &n, 0);
    char *testo = dati ? malloc(n + 1) : 0;
    if (!testo) {
        mz_free(dati);
        SEGNA_ERRORE(r, "manifesto", "Il manifesto \xc3\xa8 rovinato e non si riesce a scompattare: l'app non si pu\xc3\xb2 installare.");
        return -1;
    }
    memcpy(testo, dati, n);
    testo[n] = 0;
    mz_free(dati);
    char errore[120] = "";
    int esito = manifesto_leggi(testo, &r->m, errore, sizeof errore);
    free(testo);
    if (esito < 0) {
        SEGNA_ERRORE(r, "manifesto", "Il manifesto \xc3\xa8 scritto male e non si riesce a leggere (%s): l'app non si "
               "pu\xc3\xb2 installare. Va corretto da chi l'ha fatta.", errore);
        return -1;
    }
    r->manifesto_letto = 1;
    return 0;
}

/* Le voci obbligatorie e quelle scritte male. */
static void controlla_voci(struct resoconto *r) {
    static const struct { unsigned int voce; const char *nome, *cosa; } obbligatorie[] = {
        { VOCE_ID, "id", "l'identificativo fisso dell'app" },
        { VOCE_NOME, "nome", "il nome che vedi sotto l'icona" },
        { VOCE_VERSIONE, "versione", "la versione dell'app" },
        { VOCE_PROCESSORE, "processore", "per quale processore \xc3\xa8 fatta" },
        { VOCE_SISTEMA_MINIMO, "sistema_minimo", "la versione minima del sistema che le serve" },
        { VOCE_PROGRAMMA, "programma", "il programma da avviare" },
    };
    static const struct { unsigned int voce; const char *nome; } tutte[] = {
        { VOCE_ID, "id" }, { VOCE_NOME, "nome" }, { VOCE_VERSIONE, "versione" }, { VOCE_AUTORE, "autore" },
        { VOCE_DESCRIZIONE, "descrizione" }, { VOCE_PROCESSORE, "processore" },
        { VOCE_SISTEMA_MINIMO, "sistema_minimo" }, { VOCE_PROGRAMMA, "programma" },
        { VOCE_PERMESSI, "permessi" }, { VOCE_DEVIAZIONI, "deviazioni" }, { VOCE_FUNZIONI, "funzioni" },
        { VOCE_ICONE, "icone" },
    };
    struct manifesto *m = &r->m;
    for (unsigned int i = 0; i < sizeof obbligatorie / sizeof obbligatorie[0]; i++)
        if (!(m->presenti & obbligatorie[i].voce))
            SEGNA_ERRORE(r, obbligatorie[i].nome, "Nel manifesto manca la voce \xc2\xab%s\xc2\xbb (%s): l'app non si "
                   "pu\xc3\xb2 installare.", obbligatorie[i].nome, obbligatorie[i].cosa);
    for (unsigned int i = 0; i < sizeof tutte / sizeof tutte[0]; i++)
        if (m->sbagliate & tutte[i].voce)
            SEGNA_ERRORE(r, tutte[i].nome, "Nel manifesto la voce \xc2\xab%s\xc2\xbb \xc3\xa8 scritta male (del tipo "
                   "sbagliato, troppo lunga o con troppi elementi): l'app non si pu\xc3\xb2 installare.", tutte[i].nome);

    if ((m->presenti & VOCE_ID) && !(m->sbagliate & VOCE_ID) && !id_valido(m->id))
        SEGNA_ERRORE(r, "id", "L'identificativo \xc2\xab%.40s\xc2\xbb non va bene: pu\xc3\xb2 avere solo lettere minuscole, "
               "cifre e trattini (al massimo 32, senza trattino all'inizio). L'app non si pu\xc3\xb2 installare.", m->id);
    if ((m->presenti & VOCE_VERSIONE) && !(m->sbagliate & VOCE_VERSIONE) && !versione_valida(m->versione))
        SEGNA_ERRORE(r, "versione", "La versione \xc2\xab%s\xc2\xbb non va bene: deve essere fatta di numeri separati da "
               "punti, per esempio 1.0.2. L'app non si pu\xc3\xb2 installare.", m->versione);
    if ((m->presenti & VOCE_PROGRAMMA) && !(m->sbagliate & VOCE_PROGRAMMA) && !programma_valido(m->programma))
        SEGNA_ERRORE(r, "programma", "Il programma da avviare (\xc2\xab%s\xc2\xbb) deve essere un file nella cartella "
               "dell'app, con un nome semplice: l'app non si pu\xc3\xb2 installare.", m->programma);
}

/* Processore e versione del sistema. */
static void controlla_sistema(struct resoconto *r) {
    struct manifesto *m = &r->m;
    if (m->processore[0] && strcmp(m->processore, PROCESSORE_QUI)) {
        const char *suo = spiega_processore(m->processore);
        if (suo)
            SEGNA_ERRORE(r, "processore", "Quest'app \xc3\xa8 fatta %s, ma questo computer \xc3\xa8 %s. Qui non pu\xc3\xb2 "
                   "funzionare: serve il pacchetto fatto per questo computer.", suo, spiega_processore(PROCESSORE_QUI));
        else
            SEGNA_ERRORE(r, "processore", "Quest'app \xc3\xa8 fatta per un processore che il sistema non conosce "
                   "(\xc2\xab%s\xc2\xbb): qui non pu\xc3\xb2 funzionare.", m->processore);
    }
    if (m->sistema_minimo > VERSIONE_SISTEMA)
        SEGNA_ERRORE(r, "sistema_minimo", "Quest'app ha bisogno della versione %d del sistema, ma questo sistema \xc3\xa8 "
               "alla versione %d. Per installarla bisogna prima aggiornare il sistema.", m->sistema_minimo,
               VERSIONE_SISTEMA);
}

/* Permessi, deviazioni e funzioni: i nomi devono essere noti. */
static void controlla_elenchi(struct resoconto *r) {
    struct manifesto *m = &r->m;
    for (int i = 0; i < m->quanti_permessi; i++) {
        const char *p = m->permessi[i], *cosa = permesso_di_sistema(p);
        if (cosa)
            SEGNA_AVVISO(r, "permessi", "L'app chiede il permesso di sistema \xc2\xab%s\xc2\xbb (%s), che oggi \xc3\xa8 "
                   "riservato alle app del sistema: non lo ricever\xc3\xa0, e dovr\xc3\xa0 funzionare senza.", p, cosa);
        else if (!in_elenco(permessi_normali, p))
            SEGNA_ERRORE(r, "permessi", "L'app chiede il permesso \xc2\xab%s\xc2\xbb, che il sistema non conosce. Forse \xc3\xa8 "
                   "fatta per un sistema pi\xc3\xb9 nuovo, o il manifesto \xc3\xa8 sbagliato: per sicurezza non si "
                   "installa.", p);
    }
    for (int i = 0; i < m->quante_deviazioni; i++) {
        int k = 0;
        while (deviazioni_note[k].nome && strcmp(deviazioni_note[k].nome, m->deviazioni[i])) k++;
        if (deviazioni_note[k].nome) SEGNA_AVVISO(r, "deviazioni", "%s", deviazioni_note[k].avviso);
        else
            SEGNA_ERRORE(r, "deviazioni", "L'app dichiara la deviazione dallo standard \xc2\xab%s\xc2\xbb, che il sistema "
                   "non conosce: per sicurezza non si installa.", m->deviazioni[i]);
    }
    for (int i = 0; i < m->quante_funzioni; i++)
        if (!in_elenco(funzioni_note, m->funzioni[i]))
            SEGNA_ERRORE(r, "funzioni", "L'app dice di offrire la funzione \xc2\xab%s\xc2\xbb, che il sistema non conosce: "
                   "non si installa.", m->funzioni[i]);
}

/* Il programma da avviare c'e' nel pacchetto? */
static void controlla_programma(struct archivio *a, struct resoconto *r) {
    if (!programma_valido(r->m.programma)) return;     /* gia' detto */
    if (cerca(a, r->m.programma) < 0)
        SEGNA_ERRORE(r, "programma", "Il manifesto dice di avviare \xc2\xab%s\xc2\xbb, ma nel pacchetto questo file non "
               "c'\xc3\xa8: l'app non partirebbe, quindi non si installa.", r->m.programma);
}

static const char *const nomi_temi[2] = { "chiaro", "scuro" };

/* Le icone: quelle che mancano sono avvisi; il sistema usa la piu' adatta che c'e'. */
static void controlla_icone(struct archivio *a, struct resoconto *r) {
    struct manifesto *m = &r->m;
    for (int t = 0; t < 2; t++) {
        r->svg_presente[t] = m->icone[t].svg[0] && cerca(a, m->icone[t].svg) >= 0;
        for (int k = 0; k < m->icone[t].quante_png; k++)
            r->png_presente[t][k] = cerca(a, m->icone[t].png[k]) >= 0;
    }
    if (!r->svg_presente[0] && !r->svg_presente[1]) {
        SEGNA_AVVISO(r, "icone", "L'app non ha la sua icona (il disegno SVG per il tema chiaro e per quello scuro): "
               "vedrai un'icona generica.");
        return;
    }
    for (int t = 0; t < 2; t++) {
        if (r->svg_presente[t]) continue;
        if (m->icone[t].svg[0])
            SEGNA_AVVISO(r, "icone", "Il manifesto indica l'icona del tema %s (\xc2\xab%.60s\xc2\xbb), ma nel pacchetto non "
                   "c'\xc3\xa8: in tema %s vedrai l'icona del tema %s.", nomi_temi[t], m->icone[t].svg, nomi_temi[t],
                   nomi_temi[1 - t]);
        else
            SEGNA_AVVISO(r, "icone", "Manca l'icona del tema %s: in tema %s vedrai l'icona del tema %s.", nomi_temi[t],
                   nomi_temi[t], nomi_temi[1 - t]);
    }
    /* le PNG piccole: per ogni tema, le misure attese che mancano */
    static const int attese[] = MISURE_PNG_ATTESE;
    for (int t = 0; t < 2; t++) {
        char mancano[48] = "";
        int quante = 0;
        for (unsigned int i = 0; i < sizeof attese / sizeof attese[0]; i++) {
            int c = 0;
            for (int k = 0; k < m->icone[t].quante_png; k++)
                if (m->icone[t].png_misura[k] == attese[i] && r->png_presente[t][k]) c = 1;
            if (c) continue;
            snprintf(mancano + strlen(mancano), sizeof mancano - strlen(mancano), "%s%d", quante ? " e " : "", attese[i]);
            quante++;
        }
        if (quante)
            SEGNA_AVVISO(r, "icone", "Mancano le icone piccole (PNG) da %s pixel per il tema %s: a quelle misure il "
                   "sistema rimpicciolir\xc3\xa0 il disegno, che potrebbe vedersi meno nitido.", mancano, nomi_temi[t]);
    }
}

/* Il percorso di una cartella delle app con un nome dentro, in 'dove' (PATH_MAX). */
static int percorso_in(char *dove, size_t massimo, const char *cartella, const char *nome) {
    int n = snprintf(dove, massimo, "%s/%s", cartella, nome);
    return n > 0 && (size_t)n < massimo ? 0 : -1;
}

/* L'id e' di un'app del sistema? Quale versione e' gia' installata in /data/apps? */
static void controlla_installate(struct resoconto *r, int cosa) {
    struct manifesto *m = &r->m;
    if (!id_valido(m->id)) return;
    char cartella[512];
    struct stat s;
    if (percorso_in(cartella, sizeof cartella, cartelle_applicazioni[CARTELLA_APP_SISTEMA], m->id) == 0 &&
        lstat(cartella, &s) == 0) {
        SEGNA_ERRORE(r, "id", "L'identificativo \xc2\xab%s\xc2\xbb \xc3\xa8 gi\xc3\xa0 usato da un'app del sistema. Un'app "
               "non pu\xc3\xb2 prendere il posto di un'app del sistema: non si installa.", m->id);
        return;
    }
    struct manifesto *vecchio = malloc(sizeof *vecchio);
    if (!vecchio) return;
    if (percorso_in(cartella, sizeof cartella, cartelle_applicazioni[CARTELLA_APP_DATI], m->id) == 0 &&
        lstat(cartella, &s) == 0) {
        r->gia_installata = 1;
        if (manifesto_dell_app(cartella, vecchio) == 0 && versione_valida(vecchio->versione))
            strcpy(r->versione_installata, vecchio->versione);
    }
    free(vecchio);
    if (!r->gia_installata) {
        if (cosa == PACCHETTO_AGGIORNA)
            SEGNA_ERRORE(r, "versione", "Quest'app non \xc3\xa8 installata, quindi non si pu\xc3\xb2 aggiornare: va installata.");
        return;
    }
    if (cosa == PACCHETTO_INSTALLA) {
        SEGNA_ERRORE(r, "versione", "Quest'app \xc3\xa8 gi\xc3\xa0 installata (versione %s): per passare a questa va "
               "aggiornata, non installata di nuovo.", r->versione_installata[0] ? r->versione_installata : "sconosciuta");
        return;
    }
    if (versione_valida(m->versione) && r->versione_installata[0] &&
        confronta_versioni(m->versione, r->versione_installata) <= 0)
        SEGNA_ERRORE(r, "versione", "\xc3\x88 gi\xc3\xa0 installata la versione %s di quest'app, uguale o pi\xc3\xb9 nuova "
               "di questa (%s): non c'\xc3\xa8 niente da aggiornare.", r->versione_installata, m->versione);
}

static int descrivi(struct archivio *a, int cosa, struct resoconto *r) {
    controlla_indice(a, r);
    if (leggi_manifesto(a, r) == 0) {
        controlla_voci(r);
        controlla_sistema(r);
        controlla_elenchi(r);
        controlla_programma(a, r);
        controlla_installate(r, cosa);
        controlla_icone(a, r);
    }
    r->installabile = r->quanti_errori == 0;
    return r->installabile ? 0 : -1;
}

int pacchetto_descrivi(int f, int cosa, struct resoconto *r) {
    memset(r, 0, sizeof *r);
    struct archivio a;
    if (apri_archivio(&a, f) < 0) {
        SEGNA_ERRORE(r, "pacchetto", "Il file non \xc3\xa8 un pacchetto .app valido (non si apre come archivio zip): forse "
               "\xc3\xa8 rovinato, o non \xc3\xa8 un'app. Non si pu\xc3\xb2 installare.");
        return -1;
    }
    int esito = descrivi(&a, cosa, r);
    chiudi_archivio(&a);
    return esito;
}

/* --- installare --- */

static int togli_voce(const char *percorso, const struct stat *s, int tipo, struct FTW *f) {
    (void)s; (void)f;
    return tipo == FTW_DP ? rmdir(percorso) : unlink(percorso);
}

/* Cancella una cartella con tutto quello che c'e' dentro (senza seguire i collegamenti). */
static void togli_cartella(const char *percorso) {
    nftw(percorso, togli_voce, 16, FTW_DEPTH | FTW_PHYS);
}

/* Le cartelle lungo il percorso 'nome' (relativo a 'radice'), fino all'ultimo "/". */
static int cartelle_lungo(const char *radice, const char *nome) {
    char percorso[1024];
    for (const char *c = nome; *c; c++) {
        if (*c != '/') continue;
        if (snprintf(percorso, sizeof percorso, "%s/%.*s", radice, (int)(c - nome), nome) >= (int)sizeof percorso)
            return -1;
        if (mkdir(percorso, 0755) < 0) {
            struct stat s;
            if (errno != EEXIST || lstat(percorso, &s) < 0 || !S_ISDIR(s.st_mode)) return -1;
            continue;
        }
        if (chown(percorso, 0, 0) < 0 || chmod(percorso, 0755) < 0) return -1;
    }
    return 0;
}

static size_t scrivi_pezzo(void *dati, mz_uint64 dove, const void *buffer, size_t n) {
    int f = *(int *)dati;
    size_t fatti = 0;
    while (fatti < n) {
        ssize_t l = pwrite(f, (const char *)buffer + fatti, n - fatti, (off_t)(dove + fatti));
        if (l < 0 && errno == EINTR) continue;
        if (l <= 0) break;
        fatti += (size_t)l;
    }
    return fatti;
}

/* Scompatta tutto l'archivio nella cartella 'radice' (appena creata). 0, o -1 con il
 * perche' nel resoconto. */
static int scompatta(struct archivio *a, const char *radice, struct resoconto *r) {
    mz_zip_archive_file_stat s;
    char percorso[1024];
    for (int i = 0; i < a->quanti; i++) {
        if (!mz_zip_reader_file_stat(&a->zip, (mz_uint)i, &s) || !nome_sicuro(s.m_filename) || e_collegamento(&s))
            return -1;                                   /* gia' controllato: non succede */
        int cartella = s.m_is_directory;
        if (cartelle_lungo(radice, s.m_filename) < 0) goto disco;
        if (cartella) continue;                          /* le cartelle le ha fatte cartelle_lungo */
        if (snprintf(percorso, sizeof percorso, "%s/%s", radice, s.m_filename) >= (int)sizeof percorso) goto disco;
        int programma = !strcmp(s.m_filename, r->m.programma);
        int f = open(percorso, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (f < 0) goto disco;
        int ok = mz_zip_reader_extract_to_callback(&a->zip, (mz_uint)i, scrivi_pezzo, &f, 0);
        if (ok && (fchown(f, 0, 0) < 0 || fchmod(f, programma ? 0755 : 0644) < 0 || fsync(f) < 0)) ok = -1;
        close(f);
        if (ok == -1) goto disco;
        if (!ok) {
            SEGNA_ERRORE(r, "pacchetto", "Il file \xc2\xab%.80s\xc2\xbb del pacchetto \xc3\xa8 rovinato e non si riesce a "
                   "scompattare: l'app non si installa, e non \xc3\xa8 cambiato niente.", s.m_filename);
            return -1;
        }
    }
    return 0;
disco:
    SEGNA_ERRORE(r, "disco", "L'installazione non \xc3\xa8 riuscita mentre si scriveva sul disco (%s): non \xc3\xa8 cambiato "
           "niente.", strerror(errno));
    return -1;
}

/* C'e' abbastanza spazio sul disco? */
static int spazio(const char *cartella, struct resoconto *r) {
    struct statvfs v;
    if (statvfs(cartella, &v) < 0) return 0;          /* non si sa: si prova */
    unsigned long long libero = (unsigned long long)v.f_bavail * v.f_frsize;
    if (libero >= r->byte + MARGINE_SPAZIO) return 0;
    SEGNA_ERRORE(r, "disco", "Sul disco non c'\xc3\xa8 abbastanza spazio: l'app scompattata occupa %llu KB, e ne restano "
           "liberi %llu.", (r->byte + 1023) / 1024, libero / 1024);
    return -1;
}

/* La cartella dei dati della persona per l'app: se non c'e', vuota e sua. Quella che c'e'
 * gia' non si tocca. */
static void cartella_dati_persona(const char *id) {
    char percorso[512];
    if (snprintf(percorso, sizeof percorso, "%s/%d/appsdata/%s", cartella_persone, UID_PERSONA, id) >= (int)sizeof percorso)
        return;
    if (mkdir(percorso, 0700) == 0 && chown(percorso, UID_PERSONA, UID_PERSONA) < 0) rmdir(percorso);
}

/* Scambia due cartelle in un colpo solo (renameat2 con RENAME_EXCHANGE). Dove il file
 * system non lo sa fare, in due passi (con un attimo in cui l'app non c'e'). */
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif
static int scambia(const char *nuova, const char *vecchia, const char *parcheggio) {
    if (syscall(SYS_renameat2, AT_FDCWD, nuova, AT_FDCWD, vecchia, RENAME_EXCHANGE) == 0) return 0;
    if (errno != EINVAL && errno != ENOSYS) return -1;
    if (rename(vecchia, parcheggio) < 0) return -1;
    if (rename(nuova, vecchia) < 0) { rename(parcheggio, vecchia); return -1; }
    return rename(parcheggio, nuova) < 0 ? -1 : 0;     /* cosi' la vecchia sta in 'nuova', come con lo scambio */
}

int pacchetto_installa(int f, int cosa, struct resoconto *r) {
    memset(r, 0, sizeof *r);
    if (cosa != PACCHETTO_INSTALLA && cosa != PACCHETTO_AGGIORNA) return -1;
    struct archivio a;
    if (apri_archivio(&a, f) < 0) {
        SEGNA_ERRORE(r, "pacchetto", "Il file non \xc3\xa8 un pacchetto .app valido (non si apre come archivio zip): forse "
               "\xc3\xa8 rovinato, o non \xc3\xa8 un'app. Non si pu\xc3\xb2 installare.");
        return -1;
    }
    const char *apps = cartelle_applicazioni[CARTELLA_APP_DATI];
    char nuova[512], finale[512], parcheggio[512];
    if (descrivi(&a, cosa, r) < 0 || spazio(apps, r) < 0 ||
        snprintf(nuova, sizeof nuova, "%s/.nuova-%s-%d", apps, r->m.id, (int)getpid()) >= (int)sizeof nuova ||
        snprintf(parcheggio, sizeof parcheggio, "%s/.vecchia-%s-%d", apps, r->m.id, (int)getpid()) >= (int)sizeof parcheggio ||
        percorso_in(finale, sizeof finale, apps, r->m.id) < 0) {
        chiudi_archivio(&a);
        r->installabile = 0;
        return -1;
    }
    togli_cartella(nuova);                               /* rimasta da un tentativo interrotto */
    togli_cartella(parcheggio);
    int esito = -1;
    if (mkdir(nuova, 0700) < 0 || chown(nuova, 0, 0) < 0) {
        SEGNA_ERRORE(r, "disco", "Non si riesce a preparare la cartella dell'app (%s): non \xc3\xa8 cambiato niente.", strerror(errno));
    } else if (scompatta(&a, nuova, r) == 0) {
        sync();                                          /* tutto sul disco, prima di farla comparire */
        if (chmod(nuova, 0755) < 0) {
            SEGNA_ERRORE(r, "disco", "Non si riesce a preparare la cartella dell'app (%s): non \xc3\xa8 cambiato niente.", strerror(errno));
        } else if (cosa == PACCHETTO_INSTALLA) {
            if (rename(nuova, finale) == 0) esito = 0;
            else SEGNA_ERRORE(r, "disco", "Non si riesce a mettere l'app al suo posto (%s): non \xc3\xa8 cambiato niente.", strerror(errno));
        } else if (scambia(nuova, finale, parcheggio) == 0) {
            esito = 0;                                   /* la vecchia ora e' in 'nuova': si cancella sotto */
        } else {
            SEGNA_ERRORE(r, "disco", "Non si riesce a sostituire la versione installata (%s): \xc3\xa8 rimasta quella di prima.",
                   strerror(errno));
        }
    }
    togli_cartella(nuova);
    chiudi_archivio(&a);
    if (esito == 0) {
        sync();
        cartella_dati_persona(r->m.id);
    }
    r->installabile = esito == 0;
    return esito;
}

/* --- le app installate --- */

static int per_id(const void *a, const void *b) {
    const struct app_installata *x = a, *y = b;
    if (x->di_sistema != y->di_sistema) return y->di_sistema - x->di_sistema;
    return strcmp(x->m.id, y->m.id);
}

int pacchetto_elenco(struct elenco_app *e) {
    memset(e, 0, sizeof *e);
    for (int c = 0; c < QUANTE_CARTELLE_APPLICAZIONI; c++) {
        DIR *d = opendir(cartelle_applicazioni[c]);
        if (!d) continue;
        int inizio = e->quante;
        struct dirent *v;
        while ((v = readdir(d)) && e->quante < MAX_APP_ELENCO) {
            char cartella[512];
            if (!id_valido(v->d_name) || percorso_in(cartella, sizeof cartella, cartelle_applicazioni[c], v->d_name) < 0)
                continue;
            int gia = 0;                                 /* un id del sistema vale una volta sola */
            for (int k = 0; k < inizio; k++) if (!strcmp(e->app[k].m.id, v->d_name)) gia = 1;
            struct app_installata *a = &e->app[e->quante];
            if (gia || manifesto_dell_app(cartella, &a->m) < 0 || strcmp(a->m.id, v->d_name)) continue;
            a->di_sistema = c == CARTELLA_APP_SISTEMA;
            e->quante++;
        }
        closedir(d);
    }
    qsort(e->app, (size_t)e->quante, sizeof e->app[0], per_id);
    return e->quante;
}

/* --- togliere un'app e svuotarne i dati --- */

static void di(char *messaggio, int massimo, const char *formato, ...) {
    if (!messaggio || massimo <= 0) return;
    va_list a;
    va_start(a, formato);
    vsnprintf(messaggio, (size_t)massimo, formato, a);
    va_end(a);
}

int pacchetto_in_uso(const char *programma) {
    DIR *d = opendir("/proc");
    if (!d) return 0;
    size_t l = strlen(programma);
    int trovato = 0;
    struct dirent *v;
    while (!trovato && (v = readdir(d))) {
        char *fine;
        long n = strtol(v->d_name, &fine, 10);
        if (*fine || n <= 0) continue;
        char percorso[64], programma_qui[1024];
        snprintf(percorso, sizeof percorso, "/proc/%ld/exe", n);
        ssize_t k = readlink(percorso, programma_qui, sizeof programma_qui - 1);
        if (k <= 0) continue;
        programma_qui[k] = 0;
        /* Linux aggiunge " (deleted)" se il file non c'e' piu' */
        if (!strncmp(programma_qui, programma, l) && (!programma_qui[l] || !strcmp(programma_qui + l, " (deleted)")))
            trovato = 1;
    }
    closedir(d);
    return trovato;
}

/* L'app installata 'id': in quale cartella delle app sta (CARTELLA_APP_...), con la sua
 * cartella in 'cartella' e il manifesto in *m (vuoto, con il nome uguale all'id, se non si
 * legge: un'app rovinata si deve poter togliere lo stesso). -1 se non c'e'. */
static int trova_app(const char *id, struct manifesto *m, char *cartella, size_t massimo) {
    for (int c = 0; c < QUANTE_CARTELLE_APPLICAZIONI; c++) {
        struct stat s;
        if (percorso_in(cartella, massimo, cartelle_applicazioni[c], id) < 0 || lstat(cartella, &s) < 0) continue;
        if (!S_ISDIR(s.st_mode)) return -1;
        if (manifesto_dell_app(cartella, m) < 0) {
            memset(m, 0, sizeof *m);
            snprintf(m->programma, sizeof m->programma, "code");
        }
        if (!m->nome[0]) snprintf(m->nome, sizeof m->nome, "%s", id);
        return c;
    }
    return -1;
}

static int app_in_uso(const struct manifesto *m, const char *cartella) {
    char programma[600];
    snprintf(programma, sizeof programma, "%s/%s", cartella, programma_valido(m->programma) ? m->programma : "code");
    return pacchetto_in_uso(programma);
}

/* Svuotare: tutto tranne la cartella stessa (il primo livello di nftw). */
static int svuota_voce(const char *percorso, const struct stat *s, int tipo, struct FTW *f) {
    if (f->level == 0) return 0;
    return togli_voce(percorso, s, tipo, f);
}

/* Il lavoro sui dati di una persona, in un processo con la sua identita': cancella la
 * cartella 'percorso' (svuota 0) o tutto quello che c'e' dentro (svuota 1; e se al posto della
 * cartella c'e' altro, lo si toglie e la si rifa': sua, 700). 0 se riuscito. */
static int lavoro_figlio(unsigned int uid, const char *percorso, int svuota) {
    if (setgroups(0, 0) < 0 || setgid(uid) < 0 || setuid(uid) < 0) return -1;
    if (!svuota) return nftw(percorso, togli_voce, 16, FTW_DEPTH | FTW_PHYS) == 0 ? 0 : -1;
    struct stat s;
    if (lstat(percorso, &s) == 0 && !S_ISDIR(s.st_mode) && unlink(percorso) < 0) return -1;
    if (lstat(percorso, &s) < 0 && mkdir(percorso, 0700) < 0) return -1;
    if (nftw(percorso, svuota_voce, 16, FTW_DEPTH | FTW_PHYS) != 0) return -1;
    return chmod(percorso, 0700) == 0 ? 0 : -1;
}

/* Com'e' andata, il figlio lo dice con un byte in un tubo e non con il suo codice d'uscita:
 * chi usa la libreria (il launcher) ignora SIGCHLD perche' i figli finiti spariscano da
 * soli, e allora waitpid non lo puo' sapere. */
static int lavoro_come(unsigned int uid, const char *percorso, int svuota) {
    int tubo[2];
    if (pipe2(tubo, O_CLOEXEC) < 0) return -1;
    pid_t figlio = fork();
    if (figlio < 0) { close(tubo[0]); close(tubo[1]); return -1; }
    if (figlio == 0) {
        close(tubo[0]);
        char esito = lavoro_figlio(uid, percorso, svuota) == 0 ? 's' : 'n';
        if (write(tubo[1], &esito, 1) < 0) _exit(1);
        _exit(0);
    }
    close(tubo[1]);
    char esito = 'n';
    while (read(tubo[0], &esito, 1) < 0 && errno == EINTR) {}
    close(tubo[0]);
    while (waitpid(figlio, 0, 0) < 0 && errno == EINTR) {}   /* con SIGCHLD ignorato: ECHILD, va bene */
    return esito == 's' ? 0 : -1;
}

/* Per ogni persona (/data/users/<uid>) che ha la cartella dei dati dell'app: la cancella o
 * la svuota. 0 se tutto bene. */
static int dati_persone(const char *id, int svuota) {
    DIR *d = opendir(cartella_persone);
    if (!d) return 0;
    int esito = 0;
    struct dirent *v;
    while ((v = readdir(d))) {
        char *fine;
        unsigned long uid = strtoul(v->d_name, &fine, 10);
        if (*fine || !v->d_name[0] || uid == 0 || uid > 0x7fffffff) continue;   /* solo i numeri, non il sistema */
        char percorso[600];
        struct stat s;
        if (snprintf(percorso, sizeof percorso, "%s/%s/appsdata/%s", cartella_persone, v->d_name, id) >= (int)sizeof percorso)
            { esito = -1; continue; }
        if (lstat(percorso, &s) < 0) continue;                 /* questa persona non ha dati dell'app */
        if (lavoro_come((unsigned int)uid, percorso, svuota) < 0) esito = -1;
    }
    closedir(d);
    return esito;
}

int pacchetto_togli(const char *id, int tieni_dati, char *messaggio, int massimo) {
    if (!id || !id_valido(id)) {
        di(messaggio, massimo, "L'identificativo dell'app non \xc3\xa8 valido.");
        return TOGLI_ID_SBAGLIATO;
    }
    struct manifesto *m = malloc(sizeof *m);
    if (!m) { di(messaggio, massimo, "Non c'\xc3\xa8 abbastanza memoria."); return TOGLI_DISCO; }
    char cartella[512], nascosta[512];
    int dove = trova_app(id, m, cartella, sizeof cartella), esito;
    if (dove < 0) {
        di(messaggio, massimo, "Quest'app non \xc3\xa8 installata.");
        esito = TOGLI_NON_INSTALLATA;
    } else if (dove == CARTELLA_APP_SISTEMA) {
        di(messaggio, massimo, "%s \xc3\xa8 un'app del sistema: non si pu\xc3\xb2 togliere.", m->nome);
        esito = TOGLI_DEL_SISTEMA;
    } else if (app_in_uso(m, cartella)) {
        di(messaggio, massimo, "Chiudi %s prima di toglierla.", m->nome);
        esito = TOGLI_IN_USO;
    } else if (snprintf(nascosta, sizeof nascosta, "%s/.togliere-%s-%d", cartelle_applicazioni[CARTELLA_APP_DATI], id,
                        (int)getpid()) >= (int)sizeof nascosta) {
        di(messaggio, massimo, "Non si \xc3\xa8 riusciti a togliere %s.", m->nome);
        esito = TOGLI_DISCO;
    } else {
        togli_cartella(nascosta);                        /* rimasta da un tentativo interrotto */
        if (rename(cartella, nascosta) < 0) {            /* l'app sparisce tutta insieme */
            di(messaggio, massimo, "Non si \xc3\xa8 riusciti a togliere %s (%s).", m->nome, strerror(errno));
            esito = TOGLI_DISCO;
        } else {
            togli_cartella(nascosta);
            esito = TOGLI_FATTO;
            if (!tieni_dati && dati_persone(id, 0) < 0) {
                di(messaggio, massimo, "%s \xc3\xa8 stata tolta, ma non tutti i suoi dati si sono potuti cancellare.", m->nome);
                esito = TOGLI_DISCO;
            } else {
                di(messaggio, massimo, "%s \xc3\xa8 stata tolta.", m->nome);
            }
            sync();
        }
    }
    free(m);
    return esito;
}

int pacchetto_svuota_dati(const char *id, char *messaggio, int massimo) {
    if (!id || !id_valido(id)) {
        di(messaggio, massimo, "L'identificativo dell'app non \xc3\xa8 valido.");
        return TOGLI_ID_SBAGLIATO;
    }
    struct manifesto *m = malloc(sizeof *m);
    if (!m) { di(messaggio, massimo, "Non c'\xc3\xa8 abbastanza memoria."); return TOGLI_DISCO; }
    char cartella[512];
    int esito;
    if (trova_app(id, m, cartella, sizeof cartella) < 0) {
        di(messaggio, massimo, "Quest'app non \xc3\xa8 installata.");
        esito = TOGLI_NON_INSTALLATA;
    } else if (app_in_uso(m, cartella)) {
        di(messaggio, massimo, "Chiudi %s prima di svuotarne i dati.", m->nome);
        esito = TOGLI_IN_USO;
    } else if (dati_persone(id, 1) < 0) {
        di(messaggio, massimo, "Non si \xc3\xa8 riusciti a svuotare tutti i dati di %s.", m->nome);
        esito = TOGLI_DISCO;
    } else {
        di(messaggio, massimo, "Dati svuotati.");
        esito = TOGLI_FATTO;
        sync();
    }
    free(m);
    return esito;
}

int pacchetto_ha_permesso(const char *programma, const char *permesso) {
    const char *sistema = cartelle_applicazioni[CARTELLA_APP_SISTEMA];
    size_t l = strlen(sistema);
    if (strncmp(programma, sistema, l) || programma[l] != '/') return 0;
    char id[MAX_ID];
    const char *inizio = programma + l + 1, *fine = strchr(inizio, '/');
    if (!fine || fine - inizio >= MAX_ID || strchr(fine + 1, '/')) return 0;
    memcpy(id, inizio, (size_t)(fine - inizio));
    id[fine - inizio] = 0;
    struct descrizione d;
    if (leggi_descrizione(id, &d) < 0 || !d.di_sistema || strcmp(d.programma, programma)) return 0;
    struct manifesto *m = malloc(sizeof *m);
    char cartella[512];
    int puo = 0;
    if (m && percorso_in(cartella, sizeof cartella, sistema, id) == 0 && manifesto_dell_app(cartella, m) == 0)
        for (int i = 0; i < m->quanti_permessi; i++)
            if (!strcmp(m->permessi[i], permesso)) puo = 1;
    free(m);
    return puo;
}

int pacchetto_puo_gestire(const char *programma) { return pacchetto_ha_permesso(programma, "app.gestire"); }
