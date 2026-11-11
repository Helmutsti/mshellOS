/* Il device tree: la descrizione della macchina.
 *
 * COS'E'. Una scheda ARM non ha un modo standard per dire "la RAM sta qui, la seriale sta
 * li'". Su PC il BIOS e i bus (PCI, ACPI) raccontano molto; su ARM no. Allora chi accende la
 * macchina (il firmware, o QEMU) lascia in memoria un foglio: il DEVICE TREE, un albero di
 * nodi che descrive la macchina. Dove sta la RAM, la seriale, il centralino delle
 * interruzioni (GIC), quante CPU ci sono... All'avvio l'indirizzo del foglio arriva al
 * kernel in un registro (x0), e noi lo leggiamo.
 *
 * PERCHE' SERVE. Senza, gli indirizzi (0x09000000 per la seriale, 0x08000000 per il GIC...)
 * dovrebbero essere scritti nel codice: il kernel andrebbe bene su QEMU "virt" e basta.
 * Con il device tree lo STESSO kernel parte su macchine diverse: chiede "dov'e' la
 * seriale?" e il foglio risponde. Linux fa esattamente cosi' (drivers/of/fdt.c, e la
 * libreria libfdt); questa e' la versione piccola.
 *
 * COM'E' FATTO IL FORMATO (FDT, il file si chiama DTB). Tutti i numeri sono BIG-ENDIAN
 * (il byte piu' importante per primo; ARM di solito e' little-endian: va girato con
 * __builtin_bswap32). Il file ha quattro parti:
 *   1. L'INTESTAZIONE: numero magico 0xd00dfeed (se non c'e', non e' un device tree),
 *      dimensione totale, dove stanno gli altri blocchi, versione.
 *   2. Il BLOCCO DELLA STRUTTURA: l'albero vero, scritto come una sequenza di "gettoni"
 *      da 32 bit, ognuno allineato a 4 byte:
 *        FDT_BEGIN_NODE = 1  segue il nome del nodo (testo terminato da 0): si entra
 *        FDT_END_NODE   = 2  si esce dal nodo
 *        FDT_PROP       = 3  una proprieta': lunghezza, posizione del nome, e il valore
 *        FDT_NOP        = 4  non fa niente, si salta
 *        FDT_END        = 9  fine dell'albero
 *   3. Il BLOCCO DELLE STRINGHE: i nomi delle proprieta' ("reg", "compatible"...), scritti
 *      una volta sola; ogni proprieta' dice solo "il mio nome sta all'offset tanto".
 *   4. (La mappa di memoria riservata: a noi non serve.)
 *
 * LE CELLE. Un indirizzo puo' stare in 1 o 2 numeri da 32 bit (le "celle"). Quanti, lo dice
 * il PADRE con #address-cells (per gli indirizzi) e #size-cells (per le dimensioni). La
 * proprieta' "reg" di un figlio e' una lista di coppie (indirizzo, dimensione) scritte con
 * quel numero di celle. Esempio sulla radice (2 e 2): reg = <0 0x40000000 0 0x8000000>
 * vuol dire "da 0x40000000, per 128 MB".
 *
 * "COMPATIBLE" e' la carta d'identita' di un dispositivo: una lista di nomi, dal piu'
 * preciso al piu' generico ("arm,pl011"). Il driver cerca il nodo che si dichiara
 * compatibile con lui.
 *
 * COME LEGGIAMO. Senza malloc e senza ricorsione: una sola passata sul blocco della
 * struttura, tenendo per ogni livello di profondita' le celle dichiarate. Ogni offset
 * viene controllato contro i limiti del blocco: un DTB rovinato non ci fa uscire dalla
 * memoria, al peggio non troviamo niente. */
#include "albero.h"

#define FDT_MAGICO      0xd00dfeedu
#define FDT_BEGIN_NODE  1u
#define FDT_END_NODE    2u
#define FDT_PROP        3u
#define FDT_NOP         4u
#define FDT_END         9u

#define PROFONDITA_MAX  16     /* quanti livelli di nodi teniamo a mente */
#define CELLE_IND_DEF   2      /* se il padre non lo dice, la specifica vuole 2 e 1 */
#define CELLE_DIM_DEF   1

/* Il device tree che abbiamo ricordato. */
static const unsigned char *g_dtb;
static unsigned long g_totale;
static unsigned long g_inizio_struttura, g_fine_struttura;
static unsigned long g_inizio_stringhe, g_fine_stringhe;

/* Un numero da 32 bit big-endian, a offset 'o' (multiplo di 4: il DTB e' allineato a 8).
 * Accesso a 32 bit allineato, poi si gira l'ordine dei byte. */
static unsigned int leggi32(unsigned long o) {
    return __builtin_bswap32(*(const unsigned int *)(g_dtb + o));
}

static unsigned long lunghezza_testo(const char *s, unsigned long massimo) {
    unsigned long n = 0;
    while (n < massimo && s[n]) n++;
    return n;      /* == massimo vuol dire: manca lo zero finale */
}

static int testo_uguale(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Il nome sta nel blocco delle stringhe, all'offset 'nome'? E' uguale a 'voluto'? */
static int nome_proprieta_e(unsigned long nome, const char *voluto) {
    unsigned long o = g_inizio_stringhe + nome;
    if (o >= g_fine_stringhe) return 0;
    unsigned long max = g_fine_stringhe - o;
    if (lunghezza_testo((const char *)g_dtb + o, max) == max) return 0;
    return testo_uguale((const char *)g_dtb + o, voluto);
}

/* Il nome del nodo e' "memory" oppure "memory@qualcosa"? */
static int nome_e_memoria(const char *nome) {
    const char *m = "memory";
    while (*m && *nome == *m) { nome++; m++; }
    return *m == 0 && (*nome == 0 || *nome == '@');
}

int albero_prepara(const void *dtb) {
    g_dtb = 0;
    g_totale = 0;
    if (!dtb || ((unsigned long)dtb & 3)) return 0;
    g_dtb = (const unsigned char *)dtb;
    if (leggi32(0) != FDT_MAGICO) { g_dtb = 0; return 0; }

    unsigned long totale      = leggi32(4);
    unsigned long off_strutt  = leggi32(8);
    unsigned long off_string  = leggi32(12);
    unsigned long versione    = leggi32(20);
    unsigned long dim_string  = leggi32(32);
    unsigned long dim_strutt  = leggi32(36);   /* c'e' dalla versione 17; la 16 e' compatibile */

    /* 40 byte: l'intestazione. Il resto deve stare tutto dentro 'totale'. */
    if (versione < 16 || totale < 40 ||
        (off_strutt & 3) || off_strutt > totale || dim_strutt > totale - off_strutt ||
        off_string > totale || dim_string > totale - off_string) {
        g_dtb = 0;
        return 0;
    }
    g_totale = totale;
    g_inizio_struttura = off_strutt;
    g_fine_struttura   = off_strutt + dim_strutt;
    g_inizio_stringhe  = off_string;
    g_fine_stringhe    = off_string + dim_string;
    return 1;
}

unsigned long albero_dimensione(void) {
    return g_dtb ? g_totale : 0;
}

/* Il valore e' una lista di stringhe separate da \0: 'voluto' ce l'ha dentro? */
static int lista_contiene(unsigned long o, unsigned long lunghezza, const char *voluto) {
    const char *p = (const char *)g_dtb + o;
    unsigned long i = 0;
    while (i < lunghezza) {
        unsigned long n = lunghezza_testo(p + i, lunghezza - i);
        if (n == lunghezza - i) break;               /* senza zero finale: ignora */
        if (testo_uguale(p + i, voluto)) return 1;
        i += n + 1;
    }
    return 0;
}

/* Legge l'intervallo numero 'quale' di un "reg": 'o' e' dove sta il valore,
 * 'lunghezza' i suoi byte. Ogni intervallo e' (ca + cd) celle da 32 bit. */
static int leggi_reg(unsigned long o, unsigned long lunghezza, int quale,
                     unsigned int ca, unsigned int cd,
                     unsigned long *indirizzo, unsigned long *dimensione) {
    unsigned long celle = (unsigned long)ca + cd;
    if (quale < 0 || ca == 0 || ca > 8 || cd > 8) return 0;
    unsigned long inizio = (unsigned long)quale * celle * 4;
    if (inizio + celle * 4 > lunghezza) return 0;
    o += inizio;

    /* Le celle si mettono in fila: la prima e' la parte alta. Con piu' di 2 celle
     * le parti piu' alte escono dai 64 bit e si perdono (da noi sono comunque zero). */
    unsigned long v = 0;
    for (unsigned int i = 0; i < ca; i++) v = (v << 32) | leggi32(o + 4ul * i);
    unsigned long d = 0;
    for (unsigned int i = 0; i < cd; i++) d = (d << 32) | leggi32(o + 4ul * (ca + i));
    *indirizzo = v;
    *dimensione = d;
    return 1;
}

/* La passata sull'albero. Se 'compatibile' e' 0 si cerca il nodo /memory, altrimenti il
 * primo nodo compatibile. Se 'nome_voluto' non e' 0, invece del "reg" si vuole quella
 * proprieta' del nodo: la sua posizione nel DTB in *indirizzo e i suoi byte in *dimensione. */
static int percorri_nome(const char *compatibile, int quale, const char *nome_voluto,
                         unsigned long *indirizzo, unsigned long *dimensione);

static int percorri(const char *compatibile, int quale,
                    unsigned long *indirizzo, unsigned long *dimensione) {
    return percorri_nome(compatibile, quale, 0, indirizzo, dimensione);
}

static int percorri_nome(const char *compatibile, int quale, const char *nome_voluto,
                         unsigned long *indirizzo, unsigned long *dimensione) {
    if (!g_dtb) return 0;

    unsigned int celle_ind[PROFONDITA_MAX], celle_dim[PROFONDITA_MAX];
    unsigned long pos = g_inizio_struttura;
    int profondita = -1;

    while (pos + 4 <= g_fine_struttura) {
        unsigned int gettone = leggi32(pos);
        pos += 4;

        if (gettone == FDT_NOP) continue;
        if (gettone == FDT_END) return 0;
        if (gettone == FDT_END_NODE) {
            if (--profondita < -1) return 0;
            continue;
        }
        if (gettone != FDT_BEGIN_NODE) return 0;   /* una PROP fuori posto: rovinato */

        /* Un nodo nuovo: prima il nome, poi (allineato a 4) le sue proprieta'. */
        const char *nome = (const char *)g_dtb + pos;
        if (pos >= g_fine_struttura) return 0;
        unsigned long n = lunghezza_testo(nome, g_fine_struttura - pos);
        if (n == g_fine_struttura - pos) return 0;
        pos = (pos + n + 1 + 3) & ~3ul;

        if (++profondita >= PROFONDITA_MAX) return 0;
        celle_ind[profondita] = CELLE_IND_DEF;
        celle_dim[profondita] = CELLE_DIM_DEF;

        int trovato_compatibile = 0;
        int ha_reg = 0, ha_voluta = 0;
        unsigned long reg_o = 0, reg_len = 0, voluta_o = 0, voluta_len = 0;

        /* Le proprieta' di questo nodo (stanno sempre prima dei figli). */
        while (pos + 4 <= g_fine_struttura) {
            unsigned int g = leggi32(pos);
            if (g == FDT_NOP) { pos += 4; continue; }
            if (g != FDT_PROP) break;
            if (pos + 12 > g_fine_struttura) return 0;
            unsigned long len = leggi32(pos + 4);
            unsigned long nome_p = leggi32(pos + 8);
            unsigned long valore = pos + 12;
            if (len > g_fine_struttura - valore) return 0;

            if (nome_proprieta_e(nome_p, "#address-cells") && len == 4)
                celle_ind[profondita] = leggi32(valore);
            else if (nome_proprieta_e(nome_p, "#size-cells") && len == 4)
                celle_dim[profondita] = leggi32(valore);
            else if (nome_proprieta_e(nome_p, "reg")) {
                ha_reg = 1; reg_o = valore; reg_len = len;
            } else if (nome_voluto && nome_proprieta_e(nome_p, nome_voluto)) {
                ha_voluta = 1; voluta_o = valore; voluta_len = len;
            } else if (compatibile && nome_proprieta_e(nome_p, "compatible") &&
                       lista_contiene(valore, len, compatibile))
                trovato_compatibile = 1;

            pos = (valore + len + 3) & ~3ul;
        }

        int voluto = compatibile ? trovato_compatibile
                                 : (profondita == 1 && nome_e_memoria(nome));
        if (voluto && nome_voluto) {
            if (!ha_voluta) return 0;
            *indirizzo = (unsigned long)(g_dtb + voluta_o);
            *dimensione = voluta_len;
            return 1;
        }
        if (voluto && ha_reg) {
            /* Le celle da usare sono quelle del PADRE (la radice, se non c'e'). */
            unsigned int ca = CELLE_IND_DEF, cd = CELLE_DIM_DEF;
            if (profondita > 0) { ca = celle_ind[profondita - 1]; cd = celle_dim[profondita - 1]; }
            if (leggi_reg(reg_o, reg_len, quale, ca, cd, indirizzo, dimensione)) return 1;
        }
    }
    return 0;
}

int albero_memoria(unsigned long *inizio, unsigned long *dimensione) {
    return percorri(0, 0, inizio, dimensione);
}

int albero_cerca_n(const char *compatibile, int quale,
                   unsigned long *indirizzo, unsigned long *dimensione) {
    if (!compatibile) return 0;
    return percorri(compatibile, quale, indirizzo, dimensione);
}

int albero_proprieta(const char *compatibile, const char *nome, const void **valore,
                     unsigned long *byte) {
    unsigned long dove;
    if (!compatibile || !nome || !percorri_nome(compatibile, 0, nome, &dove, byte)) return 0;
    *valore = (const void *)dove;
    return 1;
}

unsigned int albero_cella(const void *valore, unsigned long i) {
    return __builtin_bswap32(((const unsigned int *)valore)[i]);
}

int albero_cerca(const char *compatibile, unsigned long *indirizzo, unsigned long *dimensione) {
    return albero_cerca_n(compatibile, 0, indirizzo, dimensione);
}
