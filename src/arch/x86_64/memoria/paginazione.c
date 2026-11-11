/* La paginazione sul PC a 64 bit.
 *
 * Con la paginazione accesa, ogni indirizzo usato dal codice e' VIRTUALE: a ogni
 * lettura e scrittura la CPU lo traduce in un indirizzo fisico (la RAM vera) usando
 * delle tabelle scritte dal kernel, e controlla i permessi. Se la pagina non c'e', o
 * il permesso manca, la CPU si ferma e chiama il kernel: eccezione 14, page fault.
 * A 64 bit la paginazione e' sempre accesa: il firmware l'ha gia' accesa, con le sue
 * tabelle; qui si fanno le nostre e le si da' alla CPU (il registro CR3).
 *
 * LE TABELLE sono su 4 livelli, come su ARM64: ognuna e' una pagina da 4 KB con 512
 * righe da 8 byte.
 *
 *   indirizzo: | PML4: 9 bit | PDPT: 9 bit | PD: 9 bit | PT: 9 bit | posizione: 12 bit |
 *   una riga di PML4 copre 512 GB, di PDPT 1 GB, di PD 2 MB, di PT 4 KB
 *
 * SCORCIATOIA: una riga della PD puo' essere una PAGINA GRANDE da 2 MB (bit 7, PS)
 * invece di puntare a una PT: "tutti questi 2 MB vanno li'". Per la RAM si usano quelle:
 * 8 GB sono 4096 righe, invece di 2 milioni.
 *
 * UNA RIGA (8 byte), i bit che usiamo:
 *   bit 0  P    presente
 *   bit 1  RW   si puo' scrivere (senza: sola lettura, anche per il kernel: CR0.WP)
 *   bit 2  US   la puo' usare anche il ring 3 (va acceso a TUTTI i livelli)
 *   bit 3  PWT, bit 4 PCD: senza cache (i dispositivi)
 *   bit 7  PS   (nella PD) pagina grande da 2 MB
 *   bit 9       uno dei bit lasciati al kernel: PAG_PRESTATA
 *   bit 12-51   l'indirizzo FISICO (della tabella sotto, o della pagina)
 * I bit 0, 1, 2 e 9 sono proprio PAG_PRESENTE, PAG_SCRITTURA, PAG_UTENTE e PAG_PRESTATA
 * di paginazione.h (che e' nata sul PC).
 *
 * LA MAPPA DEL KERNEL: la pianta di pagine.h. La PML4 ha 512 righe da 512 GB; le prime
 * 256 sono la meta' bassa, le altre la meta' alta:
 *   - righe 0-255: i PROGRAMMI. Nella mappa del kernel sono vuote;
 *   - riga 256 (MAPPA_DIRETTA): tutta la RAM, a MAPPA_DIRETTA + indirizzo fisico,
 *     costruita dalla mappa della memoria della consegna (la RAM libera, del kernel, delle
 *     tabelle ACPI) a pagine grandi da 2 MB; i pezzi da 2 MB che sono RAM solo in parte
 *     scendono a pagine da 4 KB, e solo le pagine di RAM ci sono. Sopra i 64 GB la RAM
 *     non si vede (MAX_GB). Anche qui il codice del kernel e' di sola lettura;
 *   - riga 384 (ZONA_DISPOSITIVI): i dispositivi (lo schermo, i registri delle schede
 *     PCI), aggiunti quando servono, senza cache: pag_mappa_dispositivo;
 *   - riga 511 (KERNEL_IMMAGINE): il kernel, a pagine da 4 KB: codice e costanti di sola
 *     lettura, il resto si scrive.
 * Nessuna riga della meta' alta ha il bit US: il ring 3 non puo' toccare niente del
 * kernel. Un indirizzo basso nella mappa del kernel (un puntatore nullo) fa page fault.
 * Le tabelle della mappa diretta e del kernel stanno nella .bss del kernel (non nelle
 * pagine libere: quando si costruiscono, il magazziniere non c'e' ancora).
 *
 * L'AVVIO. kernel_head.s parte con le tabelle del firmware (virtuale = fisico), ne fa
 * di provvisorie (il primo GB due volte: dov'e' e in alto, dove il kernel e' legato),
 * salta in alto e chiama il C. Qui mmu_prepara fa le tabelle vere: da li' gli
 * indirizzi bassi non portano piu' al kernel.
 *
 * LE MAPPE DEI PROGRAMMI: una per ogni programma in ring 3, come su ARM. Ogni mappa ha
 *   - le righe 256-511 della PML4 uguali a quelle del kernel: puntano alle STESSE
 *     tabelle, cosi' il kernel c'e' in ogni mappa (serve quando arriva un'interruzione) e
 *     un dispositivo mappato dopo compare in tutte;
 *   - le righe 0-255 solo sue (i programmi usano i primi 2 GB, PROGRAMMI_INIZIO -
 *     PROGRAMMI_FINE in chiamate.h), con tabelle create quando servono.
 * Lo scheduler, a ogni cambio di turno, mette in CR3 la mappa di chi parte.
 *
 * FISICO E VIRTUALE. Nelle righe c'e' l'indirizzo fisico (la CPU legge la RAM vera); il
 * kernel legge e scrive le tabelle passando dalla mappa diretta (virt_da_fisico). Una
 * mappa si indica con l'indirizzo fisico della sua PML4, quello che va in CR3. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "mmu.h"

#define RIGHE 512
#define INDICE_PML4(v) (((v) >> 39) & 0x1FF)
#define INDICE_PDPT(v) (((v) >> 30) & 0x1FF)
#define INDICE_PD(v)   (((v) >> 21) & 0x1FF)
#define INDICE_PT(v)   (((v) >> 12) & 0x1FF)
#define INDIRIZZO(riga) ((riga) & 0x000FFFFFFFFFF000ul)   /* la riga senza i bit dei permessi */

#define P        0x1ul
#define RW       0x2ul
#define US       0x4ul
#define PWT      0x8ul
#define PCD      0x10ul
#define PS       0x80ul
#define PERMESSI (P | RW | US | PAG_PRESTATA)

#define KB4 0x1000ul
#define MB2 0x200000ul
#define GB1 0x40000000ul
#define MAX_GB 64                 /* la RAM che si vede: i primi 64 GB */
#define MAX_PT 64                 /* le PT dei pezzi da 2 MB spezzati (di RAM) */
#define MAX_PT_KERNEL 4           /* il kernel: fino a 8 MB (linker.ld lo controlla) */

#define META_ALTA 256             /* la prima riga della PML4 della meta' alta */

#define CR0_PROTEGGI_SCRITTURA 0x10000 /* WP: anche il kernel rispetta la sola lettura */

/* Le tabelle della mappa del kernel, nella .bss (a zero = righe vuote). */
static unsigned long pml4[RIGHE] __attribute__((aligned(4096)));
static unsigned long pdpt_diretta[RIGHE] __attribute__((aligned(4096)));
static unsigned long scorta_pd[MAX_GB][RIGHE] __attribute__((aligned(4096)));
static unsigned long scorta_pt[MAX_PT][RIGHE] __attribute__((aligned(4096)));
static int pt_usate;
static unsigned long pdpt_dispositivi[RIGHE] __attribute__((aligned(4096)));
static unsigned long pdpt_kernel[RIGHE] __attribute__((aligned(4096)));
static unsigned long pd_kernel[RIGHE] __attribute__((aligned(4096)));
static unsigned long pt_kernel[MAX_PT_KERNEL][RIGHE] __attribute__((aligned(4096)));

extern char kernel_start[], sola_lettura_fine[], kernel_end[];   /* dal linker script */

static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Se si cambia una riga,
 * bisogna dirle di dimenticare quella pagina, se no continuerebbe a usare la vecchia. */
static void dimentica(indirizzo_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* La tabella che sta all'indirizzo fisico scritto nella riga: dalla mappa diretta. */
static unsigned long *tabella_di(unsigned long riga) {
    return (unsigned long *)virt_da_fisico(INDIRIZZO(riga));
}

static unsigned long cr3_attuale(void) {   /* l'indirizzo fisico della PML4 in uso */
    unsigned long cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return INDIRIZZO(cr3);
}

static unsigned long *attuale(void) { return tabella_di(cr3_attuale()); }

static int zona_programmi(indirizzo_t virt) { return virt >= PROGRAMMI_INIZIO && virt < PROGRAMMI_FINE; }

/* --- la mappa del kernel, all'avvio --- */

/* E' una zona di memoria da mappare come RAM? La RAM libera, il kernel e le tabelle ACPI
 * (e la memoria ACPI del firmware: e' RAM anche lei, la si lascia stare ma la si vede). */
static int e_ram(unsigned int tipo) {
    return tipo == ZONA_LIBERA || tipo == ZONA_KERNEL || tipo == ZONA_ACPI || tipo == ZONA_ACPI_NVS;
}

/* Mentre si costruisce, la mappa diretta non c'e' ancora: le tabelle della .bss si
 * scrivono dall'indirizzo del kernel. Nelle righe va il loro indirizzo fisico. */
static unsigned long fis(const void *tabella) { return fisico_da_virt(tabella); }

/* La PD del GB numero g della RAM (dalla scorta, la prima volta). La mappa diretta
 * comincia a un multiplo di 512 GB: il GB g della RAM e' la riga g della sua PDPT. */
static unsigned long *pd_del_gb(unsigned long g) {
    if (!(pdpt_diretta[g] & P))
        pdpt_diretta[g] = fis(scorta_pd[g]) | P | RW;
    return scorta_pd[g];
}

/* La PT del pezzo da 2 MB che comincia a 'base' (dalla scorta, la prima volta). */
static unsigned long *pt_del_pezzo(unsigned long base) {
    unsigned long *riga = &pd_del_gb(base / GB1)[INDICE_PD(base)];
    if (!(*riga & P)) {
        if (pt_usate == MAX_PT)
            panico("paginazione: la RAM e' spezzata in troppi pezzi (piu' di %d)", MAX_PT);
        *riga = fis(scorta_pt[pt_usate++]) | P | RW;
    }
    /* quale PT della scorta e': si conta da dove comincia la scorta */
    return scorta_pt[(INDIRIZZO(*riga) - fis(scorta_pt)) / KB4];
}

/* Il codice del kernel, in indirizzi fisici: anche nella mappa diretta e' di sola lettura. */
static int e_codice(unsigned long p) {
    return p >= fis(kernel_start) && p < fis(sola_lettura_fine);
}

/* Il pezzo da 2 MB che comincia a 'base' va spezzato in pagine da 4 KB anche se e' tutto
 * RAM? Si', se contiene il codice del kernel (che deve essere di sola lettura). */
static int da_spezzare(unsigned long base) {
    return base < fis(sola_lettura_fine) && base + MB2 > fis(kernel_start);
}

/* La RAM [da, a) (indirizzi fisici) nella mappa diretta: pagine grandi dove si puo',
 * pagine da 4 KB ai bordi. */
static void mappa_ram(unsigned long da, unsigned long a) {
    for (unsigned long base = da & ~(MB2 - 1); base < a; base += MB2) {
        if (!da_spezzare(base) && base >= da && base + MB2 <= a) {
            pd_del_gb(base / GB1)[INDICE_PD(base)] = base | P | RW | PS;
            continue;
        }
        unsigned long *pt = pt_del_pezzo(base);
        unsigned long p = base < da ? da : base, fine = base + MB2 < a ? base + MB2 : a;
        for (; p < fine; p += KB4)
            pt[INDICE_PT(p)] = p | P | (e_codice(p) ? 0 : RW);
    }
}

/* Il kernel in alto, pagina per pagina: codice e costanti di sola lettura. */
static void mappa_kernel(void) {
    pml4[INDICE_PML4(KERNEL_IMMAGINE)] = fis(pdpt_kernel) | P | RW;
    pdpt_kernel[INDICE_PDPT(KERNEL_IMMAGINE)] = fis(pd_kernel) | P | RW;
    int usate = 0;
    unsigned long *pt = 0;
    for (unsigned long v = (unsigned long)kernel_start; v < (unsigned long)kernel_end; v += KB4) {
        unsigned long *riga = &pd_kernel[INDICE_PD(v)];
        if (!(*riga & P)) {
            if (usate == MAX_PT_KERNEL) panico("paginazione: il kernel e' troppo grande");
            pt = pt_kernel[usate++];
            *riga = fis(pt) | P | RW;
        }
        pt[INDICE_PT(v)] = fis((void *)v) | P | (v < (unsigned long)sola_lettura_fine ? 0 : RW);
    }
}

unsigned long mmu_prepara(const struct consegna *c) {
    /* Le zone di RAM una dopo l'altra (la consegna le da' in ordine) si uniscono: un
     * pezzo da 2 MB fatto di due zone vicine resta una pagina grande. */
    unsigned long da = 0, a = 0, limite = (unsigned long)MAX_GB * GB1, ram = 0;
    for (unsigned int i = 0; i <= c->quante_zone; i++) {
        int fine = i == c->quante_zone;
        const struct zona_consegna *z = fine ? 0 : &c->zone[i];
        if (!fine && !e_ram(z->tipo)) continue;
        if (!fine && z->inizio == a && a > da) {     /* attaccata alla precedente */
            a = z->inizio + z->byte;
            continue;
        }
        if (a > da) {                                /* la precedente e' finita: si mappa */
            if (a > limite) a = limite;
            if (da < a) { mappa_ram(da, a); ram = a; }
        }
        if (!fine) { da = z->inizio; a = z->inizio + z->byte; }
    }

    /* La meta' alta: la mappa diretta, la zona dei dispositivi (ancora vuota, ma la sua
     * PDPT c'e' gia': le mappe dei programmi la condividono), il kernel. La meta' bassa
     * resta vuota: ogni programma ha la sua. */
    pml4[INDICE_PML4(MAPPA_DIRETTA)] = fis(pdpt_diretta) | P | RW;
    pml4[INDICE_PML4(ZONA_DISPOSITIVI)] = fis(pdpt_dispositivi) | P | RW;
    mappa_kernel();

    /* "la PML4 e' qui": da questa istruzione in poi le tabelle provvisorie di
     * kernel_head.s non servono piu', e gli indirizzi bassi non portano a niente. Se le
     * nostre fossero sbagliate, la prossima istruzione farebbe page fault. Poi WP: anche
     * il kernel rispetta la sola lettura (una scrittura nel suo codice e' un errore, non
     * passa in silenzio). */
    __asm__ volatile("mov %0, %%cr3" : : "r"(fis(pml4)) : "memory");
    unsigned long cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0 | CR0_PROTEGGI_SCRITTURA) : "memory");
    return ram;
}

unsigned int mmu_pezzi_spezzati(void) { return (unsigned int)pt_usate; }

/* --- scendere nelle tabelle --- */

/* Una tabella nuova (per i programmi e i dispositivi): una pagina del magazziniere,
 * tutta a zero (tutte le righe "assente"). Il suo indirizzo virtuale, e in *fisico
 * quello da scrivere nella riga sopra. */
static unsigned long *tabella_nuova(unsigned long *fisico) {
    indirizzo_t p = pagina_chiedi();
    if (!p) return 0;
    unsigned long *t = virt_da_fisico(p);
    memset(t, 0, PAGINA);
    tabelle++;
    *fisico = p;
    return t;
}

/* Nella tabella 't', la tabella sotto alla riga 'i'. Con crea=1, se non c'e' la si crea
 * con i permessi 'riga' (0 se il magazziniere non ha pagine). Una pagina grande non e'
 * una tabella: 0. */
static unsigned long *scendi(unsigned long *t, unsigned long i, int crea, unsigned long riga) {
    if (!(t[i] & P)) {
        if (!crea) return 0;
        unsigned long fisico;
        if (!tabella_nuova(&fisico)) return 0;
        t[i] = fisico | riga;
    }
    if (t[i] & PS) return 0;
    return tabella_di(t[i]);
}

/* La PT che copre 'virt' nella mappa 'radice'. Le righe dei livelli sopra sono
 * permissive (anche US): i permessi veri li decide la PT (la CPU guarda tutti i livelli
 * e applica il piu' severo). Si usa solo per la meta' bassa: quella dei programmi. */
static unsigned long *pt_di(unsigned long *radice, indirizzo_t virt, int crea) {
    unsigned long *pdpt = scendi(radice, INDICE_PML4(virt), crea, P | RW | US);
    if (!pdpt) return 0;
    unsigned long *pd = scendi(pdpt, INDICE_PDPT(virt), crea, P | RW | US);
    if (!pd) return 0;
    return scendi(pd, INDICE_PD(virt), crea, P | RW | US);
}

int pag_traduci(indirizzo_t virt, indirizzo_t *fis, unsigned int *permessi) {
    /* Fuori dalla zona dei programmi: 0. Il kernel usa questa funzione per controllare i
     * puntatori che arrivano dai programmi: un programma non puo' passare indirizzi del
     * kernel (la meta' alta). */
    if (!zona_programmi(virt)) return 0;
    unsigned long *pt = pt_di(attuale(), virt, 0);
    if (!pt) return 0;
    unsigned long riga = pt[INDICE_PT(virt)];
    if (!(riga & P)) return 0;
    *fis = INDIRIZZO(riga) | (virt & 0xFFF);   /* la pagina vera + la posizione dentro */
    *permessi = (unsigned int)(riga & PERMESSI);
    return 1;
}

unsigned int pag_tabelle(void) { return tabelle; }

/* --- le mappe dei programmi --- */

indirizzo_t pag_nuova_mappa(void) {
    unsigned long fisico;
    unsigned long *radice = tabella_nuova(&fisico);
    if (!radice) return 0;
    /* la meta' alta: le stesse righe, quindi le stesse tabelle (condivise). La meta'
     * bassa resta vuota: e' del programma. */
    for (int i = META_ALTA; i < RIGHE; i++) radice[i] = pml4[i];
    return fisico;
}

int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi) {
    if (!zona_programmi(virt)) return 0;   /* fuori: non e' dei programmi */
    unsigned long *pt = pt_di(virt_da_fisico(mappa), virt, 1);
    if (!pt) return 0;
    pt[INDICE_PT(virt)] = INDIRIZZO(fis) | (permessi & PERMESSI) | P;
    if (mappa == cr3_attuale()) dimentica(virt);
    return 1;
}

void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt) {
    if (!zona_programmi(virt)) return;
    unsigned long *pt = pt_di(virt_da_fisico(mappa), virt, 0);
    if (!pt) return;
    pt[INDICE_PT(virt)] = 0;
    if (mappa == cr3_attuale()) dimentica(virt);
}

/* La tabella sotto la riga 'riga' (0 se non c'e'). */
static unsigned long *sotto(unsigned long riga) {
    return (riga & P) && !(riga & PS) ? tabella_di(riga) : 0;
}

unsigned int pag_conta_programma(indirizzo_t mappa) {
    unsigned long *radice = virt_da_fisico(mappa);
    unsigned int n = 0;
    for (int r = 0; r < META_ALTA; r++) {               /* solo la meta' bassa */
        unsigned long *pdpt = sotto(radice[r]);
        if (!pdpt) continue;
        for (int i = 0; i < RIGHE; i++) {
            unsigned long *pd = sotto(pdpt[i]);
            if (!pd) continue;
            for (int j = 0; j < RIGHE; j++) {
                unsigned long *pt = sotto(pd[j]);
                if (!pt) continue;
                for (int k = 0; k < RIGHE; k++)
                    if ((pt[k] & P) && !(pt[k] & PAG_PRESTATA)) n++;   /* le sue pagine */
            }
        }
    }
    return n;
}

void pag_cancella_mappa(indirizzo_t mappa) {
    if (mappa == cr3_attuale()) panico("paginazione: cancello la mappa che sto usando");
    unsigned long *radice = virt_da_fisico(mappa);
    /* solo la meta' bassa: l'altra e' del kernel, condivisa, e resta */
    for (int r = 0; r < META_ALTA; r++) {
        unsigned long *pdpt = sotto(radice[r]);
        if (!pdpt) continue;
        for (int i = 0; i < RIGHE; i++) {
            unsigned long *pd = sotto(pdpt[i]);
            if (!pd) continue;
            for (int j = 0; j < RIGHE; j++) {
                unsigned long *pt = sotto(pd[j]);
                if (!pt) continue;
                for (int k = 0; k < RIGHE; k++)
                    if ((pt[k] & P) && !(pt[k] & PAG_PRESTATA))   /* le sue pagine */
                        pagina_restituisci(INDIRIZZO(pt[k]));
                pagina_restituisci(INDIRIZZO(pd[j]));   /* la PT */
                tabelle--;
            }
            pagina_restituisci(INDIRIZZO(pdpt[i]));     /* la PD */
            tabelle--;
        }
        pagina_restituisci(INDIRIZZO(radice[r]));       /* la PDPT */
        tabelle--;
    }
    pagina_restituisci(mappa);                          /* la PML4 */
    tabelle--;
}

void pag_usa_mappa(indirizzo_t mappa) {
    unsigned long radice = mappa ? mappa : fis(pml4);
    /* scrivere CR3 fa dimenticare alla CPU tutto il TLB: lo si fa solo se cambia davvero */
    if (radice != cr3_attuale()) __asm__ volatile("mov %0, %%cr3" : : "r"(radice) : "memory");
}

/* --- i dispositivi --- */

/* I pezzi gia' mappati: chi chiede di nuovo lo stesso pezzo (o un pezzo dentro) riceve
 * lo stesso indirizzo. */
#define MAX_PEZZI_DISPOSITIVI 32
static struct { indirizzo_t fisico, virtuale, byte; } pezzi[MAX_PEZZI_DISPOSITIVI];
static int quanti_pezzi;
static indirizzo_t prossimo = ZONA_DISPOSITIVI;   /* il primo indirizzo libero della zona */

/* La memoria di un dispositivo, senza cache (PCD e PWT): ogni lettura e scrittura arriva
 * al dispositivo. Ogni pezzo nuovo comincia in un posto da 2 MB tutto suo, alla stessa
 * distanza dall'inizio dei 2 MB che ha l'indirizzo fisico: cosi' dove il pezzo copre
 * 2 MB interi si usa una pagina grande (la finestra PCI di ARM e' di 256 MB), e pagine
 * da 4 KB solo ai bordi. Le tabelle nuove vengono dal magazziniere. */
indirizzo_t pag_mappa_dispositivo(indirizzo_t fisico, unsigned int byte) {
    if (byte == 0) return 0;
    for (int i = 0; i < quanti_pezzi; i++)
        if (fisico >= pezzi[i].fisico && fisico + byte <= pezzi[i].fisico + pezzi[i].byte)
            return pezzi[i].virtuale + (fisico - pezzi[i].fisico);

    indirizzo_t da = fisico & ~(KB4 - 1), a = (fisico + byte + KB4 - 1) & ~(KB4 - 1);
    if (a <= da) return 0;                                    /* fa il giro: non e' vero */
    indirizzo_t virt = ((prossimo + MB2 - 1) & ~(MB2 - 1)) + (da & (MB2 - 1));
    if (virt + (a - da) > ZONA_DISPOSITIVI_FINE) return 0;   /* la zona e' piena */

    for (indirizzo_t p = da; p < a; ) {
        indirizzo_t v = virt + (p - da);
        unsigned long *pd = scendi(pdpt_dispositivi, INDICE_PDPT(v), 1, P | RW);
        if (!pd) return 0;
        if (!(p & (MB2 - 1)) && p + MB2 <= a) {              /* 2 MB interi: pagina grande */
            pd[INDICE_PD(v)] = p | P | RW | PCD | PWT | PS;
            p += MB2;
            continue;
        }
        unsigned long *pt = scendi(pd, INDICE_PD(v), 1, P | RW);
        if (!pt) return 0;
        pt[INDICE_PT(v)] = p | P | RW | PCD | PWT;
        p += KB4;
    }
    /* Indirizzi mai usati prima: nel TLB non ci sono, non c'e' niente da dimenticare. */
    prossimo = virt + (a - da);
    if (quanti_pezzi < MAX_PEZZI_DISPOSITIVI)
        pezzi[quanti_pezzi++] = (typeof(pezzi[0])){ da, virt, a - da };
    return virt + (fisico - da);
}

void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo) {
    /* Niente da fare: la mappa del kernel e' gia' in uso da mmu_prepara, e lo schermo
     * l'ha gia' mappato l'avvio (avvio.c), per poterci scrivere subito. */
    (void)schermo; (void)byte_schermo;
}
