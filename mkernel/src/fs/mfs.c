/* mfs: il file system di mkernel.
 *
 * Il disco e' diviso in quattro zone:
 *
 *   settore 0        SUPERBLOCCO  la copertina: "MFS1" e dove inizia ogni zona
 *   settori 1-8      REGISTRO     un bit per settore del disco: 0 libero, 1 occupato
 *   settori 9-24     SCHEDE       128 schede (inode) da 64 byte, 8 per settore
 *   settori 25-...   DATI         il contenuto di file e cartelle
 *
 * Una SCHEDA descrive un file o una cartella: tipo, dimensione, e in quali settori
 * sta il contenuto (12 settori diretti + 1 indiretto che elenca altri 128 settori).
 * Il NOME non sta nella scheda: sta nella cartella che la contiene.
 *
 * Una CARTELLA e' una scheda il cui contenuto e' un elenco di voci da 32 byte:
 * nome (28 byte) -> numero di scheda (4 byte). Una voce col nome vuoto e' un posto libero.
 * La radice "/" e' sempre la scheda 0.
 *
 * Niente cache: registro, schede e cartelle si leggono dal disco ogni volta che servono
 * e si riscrivono subito quando cambiano.
 *
 * UN COMPITO ALLA VOLTA: ogni operazione pubblica prende il lucchetto di mfs all'inizio
 * e lo lascia alla fine (vedi in fondo al file). Senza, due compiti potrebbero trovare
 * libero lo stesso settore o la stessa scheda, o mescolare i comandi al disco. */
#include "kernel.h"
#include "disco.h"
#include "lucchetto.h"
#include "mfs.h"

typedef unsigned int u32;

#define MAGICO        "MFS1"
#define NUMERO_SCHEDE 128
#define DIRETTI       12
#define PER_INDIRETTO (SETTORE / 4)            /* 128 numeri di settore in un settore */
#define MAX_SETTORI   (DIRETTI + PER_INDIRETTO) /* 140 settori per file */
#define BIT_PER_SETTORE (SETTORE * 8)           /* 4096 bit di registro in un settore */
#define RADICE        0
#define LIBERA        0                         /* tipo di una scheda non usata */

struct superblocco {
    char magico[4];
    u32 settori_totali;
    u32 inizio_registro, settori_registro;
    u32 inizio_schede, numero_schede;
    u32 inizio_dati;
};

struct scheda {
    u32 tipo;               /* LIBERA, MFS_FILE o MFS_CARTELLA */
    u32 dimensione;         /* byte di contenuto */
    u32 diretti[DIRETTI];   /* i primi 12 settori del contenuto (0 = nessuno) */
    u32 indiretto;          /* un settore con altri 128 numeri di settore (0 = nessuno) */
    u32 riempimento;        /* per arrivare a 64 byte */
};
_Static_assert(sizeof(struct scheda) == 64, "una scheda deve essere di 64 byte");
#define SCHEDE_PER_SETTORE (SETTORE / sizeof(struct scheda))   /* 8 */

struct voce {
    char nome[MFS_NOME_MAX + 1];   /* 27 lettere + lo 0 finale */
    u32 scheda;
};
_Static_assert(sizeof(struct voce) == 32, "una voce deve essere di 32 byte");
#define VOCI_PER_SETTORE (SETTORE / sizeof(struct voce))       /* 16 */

static struct superblocco sb;   /* la copertina, letta una volta e tenuta in memoria */
static int montato;

/* ======================= il disco ======================= */

static int leggi(u32 n, void *dati)        { return disco_leggi(n, dati)  < 0 ? MFS_ERR_DISCO : 0; }
static int scrivi(u32 n, const void *dati) { return disco_scrivi(n, dati) < 0 ? MFS_ERR_DISCO : 0; }

/* ======================= il registro dei settori ======================= */

/* Mette il bit del settore 'n' a 1 (occupato) o a 0 (libero), e lo riscrive sul disco. */
static int segna(u32 n, int occupato) {
    unsigned char buf[SETTORE];
    u32 s = sb.inizio_registro + n / BIT_PER_SETTORE;   /* quale settore del registro */
    u32 bit = n % BIT_PER_SETTORE;                      /* quale bit dentro quel settore */
    int r = leggi(s, buf);
    if (r < 0) return r;
    if (occupato) buf[bit / 8] |=  (1 << (bit % 8));
    else          buf[bit / 8] &= ~(1 << (bit % 8));
    return scrivi(s, buf);
}

/* Trova un settore libero, lo segna occupato e lo riempie di zeri.
 * Restituisce il suo numero (sempre >= inizio_dati), o un errore. */
static int alloca_settore(void) {
    unsigned char buf[SETTORE];
    for (u32 i = 0; i < sb.settori_registro; i++) {
        int r = leggi(sb.inizio_registro + i, buf);
        if (r < 0) return r;
        for (u32 b = 0; b < SETTORE; b++) {
            if (buf[b] == 0xFF) continue;               /* 8 settori pieni: avanti */
            for (int k = 0; k < 8; k++) {
                if (buf[b] & (1 << k)) continue;
                u32 n = i * BIT_PER_SETTORE + b * 8 + k;
                if (n >= sb.settori_totali) return MFS_ERR_PIENO;
                if ((r = segna(n, 1)) < 0) return r;
                unsigned char zeri[SETTORE];
                memset(zeri, 0, SETTORE);
                if ((r = scrivi(n, zeri)) < 0) return r;
                return n;
            }
        }
    }
    return MFS_ERR_PIENO;
}

/* ======================= le schede ======================= */

static int leggi_scheda(u32 n, struct scheda *sc) {
    struct scheda buf[SCHEDE_PER_SETTORE];
    int r = leggi(sb.inizio_schede + n / SCHEDE_PER_SETTORE, buf);
    if (r < 0) return r;
    *sc = buf[n % SCHEDE_PER_SETTORE];
    return 0;
}

/* Le schede stanno 8 per settore: si legge il settore, si cambia la scheda, si riscrive. */
static int scrivi_scheda(u32 n, const struct scheda *sc) {
    struct scheda buf[SCHEDE_PER_SETTORE];
    u32 s = sb.inizio_schede + n / SCHEDE_PER_SETTORE;
    int r = leggi(s, buf);
    if (r < 0) return r;
    buf[n % SCHEDE_PER_SETTORE] = *sc;
    return scrivi(s, buf);
}

/* La prima scheda libera (la 0 e' sempre la radice). */
static int scheda_libera(void) {
    struct scheda sc;
    for (u32 n = 1; n < sb.numero_schede; n++) {
        int r = leggi_scheda(n, &sc);
        if (r < 0) return r;
        if (sc.tipo == LIBERA) return n;
    }
    return MFS_ERR_SCHEDE;
}

/* ======================= il contenuto di una scheda ======================= */

/* Il settore che contiene il blocco numero 'i' del contenuto (byte i*512 ... i*512+511).
 * Con crea=1, se il settore non c'e' lo alloca (e allora la scheda va riscritta dopo).
 * Restituisce il numero del settore, 0 se non c'e', o un errore. */
static int settore_del_blocco(struct scheda *sc, u32 i, int crea) {
    int r;
    if (i < DIRETTI) {                                   /* uno dei 12 diretti */
        if (!sc->diretti[i] && crea) {
            if ((r = alloca_settore()) < 0) return r;
            sc->diretti[i] = r;
        }
        return sc->diretti[i];
    }

    i -= DIRETTI;                                        /* altrimenti: nell'indiretto */
    if (i >= PER_INDIRETTO) return MFS_ERR_TROPPO_GRANDE;
    if (!sc->indiretto) {
        if (!crea) return 0;
        if ((r = alloca_settore()) < 0) return r;
        sc->indiretto = r;
    }
    u32 elenco[PER_INDIRETTO];
    if ((r = leggi(sc->indiretto, elenco)) < 0) return r;
    if (!elenco[i] && crea) {
        if ((r = alloca_settore()) < 0) return r;
        elenco[i] = r;
        if ((r = scrivi(sc->indiretto, elenco)) < 0) return r;
    }
    return elenco[i];
}

/* Libera tutti i settori del contenuto, indiretto compreso. */
static int libera_contenuto(struct scheda *sc) {
    int r;
    for (int i = 0; i < DIRETTI; i++)
        if (sc->diretti[i] && (r = segna(sc->diretti[i], 0)) < 0) return r;
    if (sc->indiretto) {
        u32 elenco[PER_INDIRETTO];
        if ((r = leggi(sc->indiretto, elenco)) < 0) return r;
        for (int i = 0; i < PER_INDIRETTO; i++)
            if (elenco[i] && (r = segna(elenco[i], 0)) < 0) return r;
        if ((r = segna(sc->indiretto, 0)) < 0) return r;
    }
    return 0;
}

/* ======================= le cartelle ======================= */

/* Per ogni voce della cartella si chiama 'visita', che risponde:
 *   CONTINUA   vai avanti
 *   FERMATI    basta cosi'
 *   RISCRIVI   basta cosi', e riscrivi sul disco il settore (la voce e' stata cambiata) */
#define CONTINUA 0
#define FERMATI  1
#define RISCRIVI 2
typedef int (*visita_voce)(struct voce *v, void *ctx);

/* Restituisce 1 se 'visita' si e' fermata, 0 se ha visto tutte le voci, o un errore. */
static int scorri_cartella(struct scheda *dir, visita_voce visita, void *ctx) {
    struct voce voci[VOCI_PER_SETTORE];
    u32 totale = dir->dimensione / sizeof(struct voce);
    for (u32 i = 0; i < totale; i += VOCI_PER_SETTORE) {
        int s = settore_del_blocco(dir, i / VOCI_PER_SETTORE, 0);
        if (s <= 0) return s < 0 ? s : MFS_ERR_DISCO;
        int r = leggi(s, voci);
        if (r < 0) return r;
        for (u32 j = 0; j < VOCI_PER_SETTORE && i + j < totale; j++) {
            int risposta = visita(&voci[j], ctx);
            if (risposta == CONTINUA) continue;
            if (risposta == RISCRIVI && (r = scrivi(s, voci)) < 0) return r;
            return 1;
        }
    }
    return 0;
}

static int uguali(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void copia_nome(char *dest, const char *src) {
    int i = 0;
    for (; src[i] && i < MFS_NOME_MAX; i++) dest[i] = src[i];
    for (; i <= MFS_NOME_MAX; i++) dest[i] = 0;
}

/* --- cercare un nome --- */
struct ricerca { const char *nome; u32 trovato; };

static int visita_cerca(struct voce *v, void *ctx) {
    struct ricerca *c = ctx;
    if (v->nome[0] && uguali(v->nome, c->nome)) { c->trovato = v->scheda; return FERMATI; }
    return CONTINUA;
}

/* Cerca 'nome' nella cartella 'n'. Mette in *trovato il numero della sua scheda. */
static int cerca(u32 n, const char *nome, u32 *trovato) {
    struct scheda dir;
    int r = leggi_scheda(n, &dir);
    if (r < 0) return r;
    if (dir.tipo != MFS_CARTELLA) return MFS_ERR_NON_CARTELLA;
    struct ricerca c = { nome, 0 };
    if ((r = scorri_cartella(&dir, visita_cerca, &c)) < 0) return r;
    if (!r) return MFS_ERR_NON_TROVATO;
    *trovato = c.trovato;
    return 0;
}

/* --- aggiungere una voce --- */
struct aggiunta { const char *nome; u32 scheda; };

static int visita_posto_libero(struct voce *v, void *ctx) {
    struct aggiunta *a = ctx;
    if (v->nome[0]) return CONTINUA;
    copia_nome(v->nome, a->nome);
    v->scheda = a->scheda;
    return RISCRIVI;
}

/* Aggiunge 'nome -> scheda' alla cartella 'n': in un posto libero se c'e', se no in fondo. */
static int aggiungi_voce(u32 n, const char *nome, u32 scheda) {
    struct scheda dir;
    int r = leggi_scheda(n, &dir);
    if (r < 0) return r;
    struct aggiunta a = { nome, scheda };
    if ((r = scorri_cartella(&dir, visita_posto_libero, &a)) != 0) return r < 0 ? r : 0;

    /* nessun posto libero: la voce va in fondo, magari in un settore nuovo */
    u32 indice = dir.dimensione / sizeof(struct voce);
    int s = settore_del_blocco(&dir, indice / VOCI_PER_SETTORE, 1);
    if (s < 0) return s;
    struct voce voci[VOCI_PER_SETTORE];
    if ((r = leggi(s, voci)) < 0) return r;
    copia_nome(voci[indice % VOCI_PER_SETTORE].nome, nome);
    voci[indice % VOCI_PER_SETTORE].scheda = scheda;
    if ((r = scrivi(s, voci)) < 0) return r;
    dir.dimensione += sizeof(struct voce);
    return scrivi_scheda(n, &dir);   /* dimensione cambiata (e forse un settore nuovo) */
}

/* --- togliere una voce --- */
static int visita_togli(struct voce *v, void *ctx) {
    if (!v->nome[0] || !uguali(v->nome, ctx)) return CONTINUA;
    v->nome[0] = 0;              /* il posto resta, vuoto: si riusera' */
    return RISCRIVI;
}

static int togli_voce(u32 n, const char *nome) {
    struct scheda dir;
    int r = leggi_scheda(n, &dir);
    if (r < 0) return r;
    return scorri_cartella(&dir, visita_togli, (void *)nome) < 0 ? MFS_ERR_DISCO : 0;
}

/* --- la cartella e' vuota? --- */
static int visita_qualcuno(struct voce *v, void *ctx) {
    (void)ctx;
    return v->nome[0] ? FERMATI : CONTINUA;
}

/* ======================= i percorsi ======================= */

/* Prende il prossimo nome da un percorso ("/doc/note.txt" -> "doc", poi "note.txt").
 * Restituisce la sua lunghezza, 0 se il percorso e' finito, o un errore se e' troppo lungo. */
static int prossimo_nome(const char **p, char *nome) {
    while (**p == '/') (*p)++;
    int l = 0;
    while (**p && **p != '/') {
        if (l == MFS_NOME_MAX) return MFS_ERR_NOME;
        nome[l++] = *(*p)++;
    }
    nome[l] = 0;
    return l;
}

/* Segue il percorso dalla radice, una cartella alla volta. Mette in *n la scheda trovata. */
static int trova(const char *percorso, u32 *n) {
    if (percorso[0] != '/') return MFS_ERR_NOME;
    char nome[MFS_NOME_MAX + 1];
    *n = RADICE;
    for (;;) {
        int l = prossimo_nome(&percorso, nome);
        if (l < 0) return l;
        if (l == 0) return 0;           /* percorso finito: *n e' la scheda cercata */
        int r = cerca(*n, nome, n);
        if (r < 0) return r;
    }
}

/* Divide "/doc/note.txt" in cartella genitore ("/doc") e nome ("note.txt"). */
static int separa(const char *percorso, char *genitore, char *nome) {
    if (percorso[0] != '/') return MFS_ERR_NOME;
    int l = 0, ultima = 0;
    while (percorso[l]) l++;
    while (l > 1 && percorso[l - 1] == '/') l--;        /* toglie le '/' in fondo */
    for (int i = 0; i < l; i++) if (percorso[i] == '/') ultima = i;

    int lung_nome = l - ultima - 1;
    if (lung_nome <= 0) return MFS_ERR_NOME;            /* "/" da solo non ha un nome */
    if (lung_nome > MFS_NOME_MAX) return MFS_ERR_NOME;
    for (int i = 0; i < lung_nome; i++) nome[i] = percorso[ultima + 1 + i];
    nome[lung_nome] = 0;

    if (ultima == 0) { genitore[0] = '/'; genitore[1] = 0; }
    else { for (int i = 0; i < ultima; i++) genitore[i] = percorso[i]; genitore[ultima] = 0; }
    return 0;
}

/* ======================= le operazioni ======================= */

static int monta_dentro(void) {
    unsigned char buf[SETTORE];
    montato = 0;
    if (leggi(0, buf) < 0) return 0;
    memcpy(&sb, buf, sizeof sb);
    if (memcmp(sb.magico, MAGICO, 4) != 0) return 0;
    montato = 1;
    return 1;
}

static int formatta_dentro(void) {
    unsigned char buf[SETTORE];
    int r;

    /* 1. il superblocco: le misure delle zone */
    memset(&sb, 0, sizeof sb);
    memcpy(sb.magico, MAGICO, 4);
    sb.settori_totali   = disco_settori();
    sb.inizio_registro  = 1;
    sb.settori_registro = (sb.settori_totali + BIT_PER_SETTORE - 1) / BIT_PER_SETTORE;
    sb.inizio_schede    = sb.inizio_registro + sb.settori_registro;
    sb.numero_schede    = NUMERO_SCHEDE;
    sb.inizio_dati      = sb.inizio_schede + NUMERO_SCHEDE / SCHEDE_PER_SETTORE;
    if (sb.settori_totali <= sb.inizio_dati) return MFS_ERR_PIENO;   /* disco troppo piccolo */

    memset(buf, 0, SETTORE);
    memcpy(buf, &sb, sizeof sb);
    if ((r = scrivi(0, buf)) < 0) return r;

    /* 2. il registro: occupati i settori delle zone di servizio (0 ... inizio_dati-1)
     *    e quelli oltre la fine del disco (se l'ultimo settore di registro avanza) */
    for (u32 i = 0; i < sb.settori_registro; i++) {
        memset(buf, 0, SETTORE);
        for (u32 bit = 0; bit < BIT_PER_SETTORE; bit++) {
            u32 n = i * BIT_PER_SETTORE + bit;
            if (n < sb.inizio_dati || n >= sb.settori_totali)
                buf[bit / 8] |= 1 << (bit % 8);
        }
        if ((r = scrivi(sb.inizio_registro + i, buf)) < 0) return r;
    }

    /* 3. le schede: tutte libere... */
    memset(buf, 0, SETTORE);
    for (u32 i = 0; i < NUMERO_SCHEDE / SCHEDE_PER_SETTORE; i++)
        if ((r = scrivi(sb.inizio_schede + i, buf)) < 0) return r;

    /* ...tranne la 0: la radice, una cartella vuota */
    montato = 1;
    struct scheda radice;
    memset(&radice, 0, sizeof radice);
    radice.tipo = MFS_CARTELLA;
    return scrivi_scheda(RADICE, &radice);
}

/* La parte comune di mkdir e crea: controlla il percorso e prepara una scheda nuova. */
static int nuova_scheda(const char *percorso, u32 *genitore, char *nome) {
    char cartella[128];
    u32 esistente;
    int r;
    if (!montato) return MFS_ERR_NON_FORMATTATO;
    if ((r = separa(percorso, cartella, nome)) < 0) return r;
    if ((r = trova(cartella, genitore)) < 0) return r;
    r = cerca(*genitore, nome, &esistente);
    if (r == 0) return MFS_ERR_ESISTE;
    if (r != MFS_ERR_NON_TROVATO) return r;     /* errore vero, o genitore non e' una cartella */
    return scheda_libera();
}

static int mkdir_dentro(const char *percorso) {
    u32 genitore;
    char nome[MFS_NOME_MAX + 1];
    int n = nuova_scheda(percorso, &genitore, nome);
    if (n < 0) return n;

    struct scheda sc;
    memset(&sc, 0, sizeof sc);
    sc.tipo = MFS_CARTELLA;
    int r = scrivi_scheda(n, &sc);
    if (r < 0) return r;
    return aggiungi_voce(genitore, nome, n);
}

static int crea_dentro(const char *percorso, const void *dati, unsigned int byte) {
    if (byte > MAX_SETTORI * SETTORE) return MFS_ERR_TROPPO_GRANDE;
    u32 genitore;
    char nome[MFS_NOME_MAX + 1];
    int n = nuova_scheda(percorso, &genitore, nome);
    if (n < 0) return n;

    struct scheda sc;
    memset(&sc, 0, sizeof sc);
    sc.tipo = MFS_FILE;
    sc.dimensione = byte;

    /* il contenuto, un settore alla volta: l'ultimo pezzo si completa con zeri */
    const unsigned char *d = dati;
    unsigned char buf[SETTORE];
    for (u32 i = 0; i * SETTORE < byte; i++) {
        int s = settore_del_blocco(&sc, i, 1);
        if (s < 0) { libera_contenuto(&sc); return s; }
        u32 pezzo = byte - i * SETTORE < SETTORE ? byte - i * SETTORE : SETTORE;
        memset(buf, 0, SETTORE);
        memcpy(buf, d + i * SETTORE, pezzo);
        int r = scrivi(s, buf);
        if (r < 0) return r;
    }

    /* prima la scheda, poi il nome nella cartella: se si spegne a meta', resta al massimo
     * una scheda orfana, mai un nome che punta a una scheda senza contenuto */
    int r = scrivi_scheda(n, &sc);
    if (r < 0) return r;
    return aggiungi_voce(genitore, nome, n);
}

static int leggi_dentro(const char *percorso, unsigned int da, void *dove, unsigned int n) {
    u32 num;
    int r;
    if (!montato) return MFS_ERR_NON_FORMATTATO;
    if ((r = trova(percorso, &num)) < 0) return r;
    struct scheda sc;
    if ((r = leggi_scheda(num, &sc)) < 0) return r;
    if (sc.tipo == MFS_CARTELLA) return MFS_ERR_E_CARTELLA;

    if (da >= sc.dimensione) return 0;
    if (n > sc.dimensione - da) n = sc.dimensione - da;   /* non oltre la fine del file */

    unsigned char buf[SETTORE];
    unsigned char *d = dove;
    u32 letti = 0;
    while (letti < n) {
        u32 pos = da + letti;
        int s = settore_del_blocco(&sc, pos / SETTORE, 0);
        if (s <= 0) return s < 0 ? s : MFS_ERR_DISCO;
        if ((r = leggi(s, buf)) < 0) return r;
        u32 dentro = pos % SETTORE;                        /* da dove, dentro il settore */
        u32 pezzo = SETTORE - dentro;
        if (pezzo > n - letti) pezzo = n - letti;
        memcpy(d + letti, buf + dentro, pezzo);
        letti += pezzo;
    }
    return letti;
}

struct elenco { mfs_visita f; };

static int visita_elenca(struct voce *v, void *ctx) {
    struct elenco *e = ctx;
    if (!v->nome[0]) return CONTINUA;
    struct scheda sc;
    if (leggi_scheda(v->scheda, &sc) < 0) return CONTINUA;
    e->f(v->nome, sc.tipo, sc.dimensione);
    return CONTINUA;
}

static int elenca_dentro(const char *percorso, mfs_visita f) {
    u32 num;
    int r;
    if (!montato) return MFS_ERR_NON_FORMATTATO;
    if ((r = trova(percorso, &num)) < 0) return r;
    struct scheda dir;
    if ((r = leggi_scheda(num, &dir)) < 0) return r;
    if (dir.tipo != MFS_CARTELLA) return MFS_ERR_NON_CARTELLA;
    struct elenco e = { f };
    r = scorri_cartella(&dir, visita_elenca, &e);
    return r < 0 ? r : 0;
}

static int rm_dentro(const char *percorso) {
    char cartella[128], nome[MFS_NOME_MAX + 1];
    u32 genitore, num;
    int r;
    if (!montato) return MFS_ERR_NON_FORMATTATO;
    if ((r = separa(percorso, cartella, nome)) < 0) return r;   /* rifiuta anche "/" */
    if ((r = trova(cartella, &genitore)) < 0) return r;
    if ((r = cerca(genitore, nome, &num)) < 0) return r;

    struct scheda sc;
    if ((r = leggi_scheda(num, &sc)) < 0) return r;
    if (sc.tipo == MFS_CARTELLA) {
        if ((r = scorri_cartella(&sc, visita_qualcuno, 0)) < 0) return r;
        if (r) return MFS_ERR_NON_VUOTA;
    }

    /* prima il nome (cosi' nessuno la trova piu'), poi i settori, poi la scheda */
    if ((r = togli_voce(genitore, nome)) < 0) return r;
    if ((r = libera_contenuto(&sc)) < 0) return r;
    memset(&sc, 0, sizeof sc);                  /* tipo = LIBERA */
    return scrivi_scheda(num, &sc);
}

static int spazio_dentro(unsigned int *schede_libere, unsigned int *settori_liberi) {
    if (!montato) return MFS_ERR_NON_FORMATTATO;
    struct scheda sc;
    int r;
    *schede_libere = 0;
    for (u32 n = 0; n < sb.numero_schede; n++) {
        if ((r = leggi_scheda(n, &sc)) < 0) return r;
        if (sc.tipo == LIBERA) (*schede_libere)++;
    }
    unsigned char buf[SETTORE];
    *settori_liberi = 0;
    for (u32 i = 0; i < sb.settori_registro; i++) {
        if ((r = leggi(sb.inizio_registro + i, buf)) < 0) return r;
        for (u32 bit = 0; bit < BIT_PER_SETTORE; bit++)
            if (!(buf[bit / 8] & (1 << (bit % 8)))) (*settori_liberi)++;
    }
    return 0;
}

const char *mfs_messaggio(int errore) {
    switch (errore) {
    case MFS_ERR_DISCO:          return "errore del disco";
    case MFS_ERR_NON_FORMATTATO: return "il disco non ha mfs";
    case MFS_ERR_NOME:           return "percorso non valido (deve iniziare con /, nomi fino a 27 lettere)";
    case MFS_ERR_NON_TROVATO:    return "non esiste";
    case MFS_ERR_ESISTE:         return "esiste gia'";
    case MFS_ERR_NON_CARTELLA:   return "non e' una cartella";
    case MFS_ERR_E_CARTELLA:     return "e' una cartella";
    case MFS_ERR_NON_VUOTA:      return "la cartella non e' vuota";
    case MFS_ERR_PIENO:          return "disco pieno";
    case MFS_ERR_SCHEDE:         return "schede finite (massimo 128 tra file e cartelle)";
    case MFS_ERR_TROPPO_GRANDE:  return "file troppo grande (massimo 70 KB)";
    default:                     return "errore sconosciuto";
    }
}

/* ======================= le operazioni pubbliche, col lucchetto ======================= */

static struct lucchetto lucchetto_mfs = LUCCHETTO("mfs");

/* Prende il lucchetto, esegue l'operazione, lo lascia, restituisce il risultato. */
#define CON_LUCCHETTO(operazione) do {                          \
        lucchetto_prendi(&lucchetto_mfs);                       \
        int r = (operazione);                                   \
        lucchetto_lascia(&lucchetto_mfs);                       \
        return r;                                               \
    } while (0)

int mfs_monta(void)                         { CON_LUCCHETTO(monta_dentro()); }
int mfs_formatta(void)                      { CON_LUCCHETTO(formatta_dentro()); }
int mfs_mkdir(const char *p)                { CON_LUCCHETTO(mkdir_dentro(p)); }
int mfs_crea(const char *p, const void *d, unsigned int b) { CON_LUCCHETTO(crea_dentro(p, d, b)); }
int mfs_leggi(const char *p, unsigned int da, void *dove, unsigned int n) {
    CON_LUCCHETTO(leggi_dentro(p, da, dove, n));
}
int mfs_elenca(const char *p, mfs_visita f) { CON_LUCCHETTO(elenca_dentro(p, f)); }
int mfs_rm(const char *p)                   { CON_LUCCHETTO(rm_dentro(p)); }
int mfs_spazio(unsigned int *schede, unsigned int *settori) {
    CON_LUCCHETTO(spazio_dentro(schede, settori));
}
