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
 *   bit 12-51   l'indirizzo fisico (della tabella sotto, o della pagina)
 * I bit 0, 1, 2 e 9 sono proprio PAG_PRESENTE, PAG_SCRITTURA, PAG_UTENTE e PAG_PRESTATA
 * di paginazione.h (che e' nata sul PC).
 *
 * LA MAPPA DEL KERNEL: indirizzo virtuale = fisico ("mappatura identica"), come su ARM,
 * costruita dalla mappa della memoria della consegna, solo ring 0:
 *   - la RAM (libera, del kernel, delle tabelle ACPI) a pagine grandi da 2 MB; i pezzi
 *     da 2 MB che sono RAM solo in parte scendono a pagine da 4 KB, e solo le pagine di
 *     RAM ci sono. Sopra i 64 GB la RAM non si vede (MAX_GB);
 *   - la pagina 0 non c'e': un puntatore nullo fa page fault invece di fare danni;
 *   - codice e costanti del kernel sono di sola lettura (a pagine da 4 KB);
 *   - i dispositivi (lo schermo, i registri delle schede PCI) si aggiungono quando
 *     servono, senza cache: pag_mappa_dispositivo.
 * Le tabelle della RAM stanno nella .bss del kernel (non nelle pagine libere: quando si
 * costruiscono, le pagine libere sono ancora piene delle tabelle del firmware, che la
 * CPU sta usando).
 *
 * LE MAPPE DEI PROGRAMMI: una per ogni programma in ring 3, come su ARM. Ogni mappa ha
 *   - le stesse righe della PML4 del kernel, che puntano alle STESSE tabelle: il kernel
 *     c'e' in ogni mappa (serve quando arriva un'interruzione), ma le sue pagine non
 *     hanno il bit US, quindi il programma non le puo' toccare;
 *   - la riga 255 (da 127,5 a 128 TB, l'ultima degli indirizzi "bassi") solo sua: li'
 *     vivono i programmi (PROGRAMMI_INIZIO - PROGRAMMI_FINE, in chiamate.h), con tabelle
 *     create quando servono. Cosi' in alto perche' nessun indirizzo fisico ci arriva:
 *     il kernel vede la RAM e le schede al loro indirizzo vero, e il firmware puo'
 *     mettere le schede PCI a 64 bit molto in alto (QEMU: a 768 GB). Il kernel puo'
 *     usare tutte le altre righe (anche oltre i primi 512 GB, per i dispositivi).
 * Lo scheduler, a ogni cambio di turno, mette in CR3 la mappa di chi parte.
 *
 * Le tabelle dei programmi sono pagine prese dal magazziniere. Siccome il kernel ha la
 * mappatura identica in ogni mappa, il loro indirizzo fisico si usa come puntatore. */
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

#define RIGA_PROGRAMMI INDICE_PML4(ZONA_PROGRAMMI)   /* la riga dei programmi: 255 */

#define CR0_PROTEGGI_SCRITTURA 0x10000 /* WP: anche il kernel rispetta la sola lettura */

/* Le tabelle della mappa del kernel, nella .bss (a zero = righe vuote). */
static unsigned long pml4[RIGHE] __attribute__((aligned(4096)));
static unsigned long pdpt0[RIGHE] __attribute__((aligned(4096)));
static unsigned long scorta_pd[MAX_GB][RIGHE] __attribute__((aligned(4096)));
static unsigned long scorta_pt[MAX_PT][RIGHE] __attribute__((aligned(4096)));
static int pt_usate;

extern char kernel_start[], sola_lettura_fine[];   /* dal linker script */

static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Se si cambia una riga,
 * bisogna dirle di dimenticare quella pagina, se no continuerebbe a usare la vecchia. */
static void dimentica(indirizzo_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static unsigned long *attuale(void) {   /* la PML4 che la CPU sta usando */
    unsigned long cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return (unsigned long *)INDIRIZZO(cr3);
}

static int zona_programmi(indirizzo_t virt) { return virt >= PROGRAMMI_INIZIO && virt < PROGRAMMI_FINE; }

/* --- la mappa del kernel, all'avvio --- */

/* E' una zona di memoria da mappare come RAM? La RAM libera, il kernel e le tabelle ACPI
 * (e la memoria ACPI del firmware: e' RAM anche lei, la si lascia stare ma la si vede). */
static int e_ram(unsigned int tipo) {
    return tipo == ZONA_LIBERA || tipo == ZONA_KERNEL || tipo == ZONA_ACPI || tipo == ZONA_ACPI_NVS;
}

/* La PD del GB numero g (dalla scorta, la prima volta). */
static unsigned long *pd_del_gb(unsigned long g) {
    if (!(pdpt0[g] & P))
        pdpt0[g] = (unsigned long)scorta_pd[g] | P | RW;
    return scorta_pd[g];
}

/* La PT del pezzo da 2 MB che comincia a 'base' (dalla scorta, la prima volta). */
static unsigned long *pt_del_pezzo(unsigned long base) {
    unsigned long *pd = pd_del_gb(base / GB1);
    unsigned long *riga = &pd[INDICE_PD(base)];
    if (!(*riga & P)) {
        if (pt_usate == MAX_PT)
            panico("paginazione: la RAM e' spezzata in troppi pezzi (piu' di %d)", MAX_PT);
        *riga = (unsigned long)scorta_pt[pt_usate++] | P | RW;
    }
    return (unsigned long *)INDIRIZZO(*riga);
}

/* Il pezzo da 2 MB che comincia a 'base' va spezzato in pagine da 4 KB anche se e' tutto
 * RAM? Si', se contiene la pagina 0 (che non deve esserci) o il codice del kernel (che
 * deve essere di sola lettura). */
static int da_spezzare(unsigned long base) {
    return base == 0 ||
           (base < (unsigned long)sola_lettura_fine && base + MB2 > (unsigned long)kernel_start);
}

/* La RAM [da, a): pagine grandi dove si puo', pagine da 4 KB ai bordi. */
static void mappa_ram(unsigned long da, unsigned long a) {
    for (unsigned long base = da & ~(MB2 - 1); base < a; base += MB2) {
        if (!da_spezzare(base) && base >= da && base + MB2 <= a) {
            pd_del_gb(base / GB1)[INDICE_PD(base)] = base | P | RW | PS;
            continue;
        }
        unsigned long *pt = pt_del_pezzo(base);
        unsigned long p = base < da ? da : base, fine = base + MB2 < a ? base + MB2 : a;
        for (; p < fine; p += KB4) {
            if (p == 0) continue;                                    /* la pagina 0: no */
            int codice = p >= (unsigned long)kernel_start && p < (unsigned long)sola_lettura_fine;
            pt[INDICE_PT(p)] = p | P | (codice ? 0 : RW);
        }
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

    /* La riga 0 della PML4: i primi 512 GB, la RAM. La riga dei programmi resta vuota:
     * ognuno ha la sua. */
    pml4[0] = (unsigned long)pdpt0 | P | RW;

    /* "la PML4 e' qui": da questa istruzione in poi le tabelle del firmware non servono
     * piu'. Se le nostre fossero sbagliate, la prossima istruzione (che sta in una
     * pagina del kernel) farebbe page fault. Poi WP: anche il kernel rispetta la sola
     * lettura (una scrittura nel suo codice e' un errore, non passa in silenzio). */
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4) : "memory");
    unsigned long cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0 | CR0_PROTEGGI_SCRITTURA) : "memory");
    return ram;
}

unsigned int mmu_pezzi_spezzati(void) { return (unsigned int)pt_usate; }

/* --- scendere nelle tabelle --- */

/* Una tabella nuova (per i programmi e i dispositivi): una pagina del magazziniere,
 * tutta a zero (tutte le righe "assente"). */
static unsigned long *tabella_nuova(void) {
    unsigned long *t = (unsigned long *)pagina_chiedi();
    if (!t) return 0;
    memset(t, 0, PAGINA);
    tabelle++;
    return t;
}

/* Nella tabella 't', la tabella sotto alla riga 'i'. Con crea=1, se non c'e' la si crea
 * con i permessi 'riga' (0 se il magazziniere non ha pagine). Una pagina grande non e'
 * una tabella: 0. */
static unsigned long *scendi(unsigned long *t, unsigned long i, int crea, unsigned long riga) {
    if (!(t[i] & P)) {
        if (!crea) return 0;
        unsigned long *nuova = tabella_nuova();
        if (!nuova) return 0;
        t[i] = (unsigned long)nuova | riga;
    }
    if (t[i] & PS) return 0;
    return (unsigned long *)INDIRIZZO(t[i]);
}

/* La PT che copre 'virt' nella mappa 'radice'. Le righe dei livelli sopra sono
 * permissive (anche US): i permessi veri li decide la PT (la CPU guarda tutti i livelli
 * e applica il piu' severo). Per il kernel la riga della PML4 non ha US, e basta lei. */
static unsigned long *pt_di(unsigned long *radice, indirizzo_t virt, int crea) {
    unsigned long *pdpt = scendi(radice, INDICE_PML4(virt), crea, P | RW | US);
    if (!pdpt) return 0;
    unsigned long *pd = scendi(pdpt, INDICE_PDPT(virt), crea, P | RW | US);
    if (!pd) return 0;
    return scendi(pd, INDICE_PD(virt), crea, P | RW | US);
}

int pag_traduci(indirizzo_t virt, indirizzo_t *fis, unsigned int *permessi) {
    /* Fuori dalla zona dei programmi: 0. Il kernel usa questa funzione per controllare i
     * puntatori che arrivano dai programmi: un programma non puo' passare indirizzi del kernel. */
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
    unsigned long *radice = tabella_nuova();
    if (!radice) return 0;
    /* la parte del kernel: le stesse righe, quindi le stesse tabelle (condivise).
     * La riga dei programmi nella PML4 del kernel e' vuota: resta vuota anche qui. */
    for (int i = 0; i < RIGHE; i++) radice[i] = pml4[i];
    return (indirizzo_t)radice;
}

int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi) {
    if (!zona_programmi(virt)) return 0;   /* fuori: e' del kernel */
    unsigned long *radice = (unsigned long *)mappa;
    unsigned long *pt = pt_di(radice, virt, 1);
    if (!pt) return 0;
    pt[INDICE_PT(virt)] = INDIRIZZO(fis) | (permessi & PERMESSI) | P;
    if (radice == attuale()) dimentica(virt);
    return 1;
}

void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt) {
    if (!zona_programmi(virt)) return;
    unsigned long *radice = (unsigned long *)mappa;
    unsigned long *pt = pt_di(radice, virt, 0);
    if (!pt) return;
    pt[INDICE_PT(virt)] = 0;
    if (radice == attuale()) dimentica(virt);
}

/* La tabella sotto la riga 'riga' (0 se non c'e'). */
static unsigned long *sotto(unsigned long riga) {
    return (riga & P) && !(riga & PS) ? (unsigned long *)INDIRIZZO(riga) : 0;
}

unsigned int pag_conta_programma(indirizzo_t mappa) {
    unsigned long *pdpt = sotto(((unsigned long *)mappa)[RIGA_PROGRAMMI]);
    unsigned int n = 0;
    if (!pdpt) return 0;
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
    return n;
}

void pag_cancella_mappa(indirizzo_t mappa) {
    unsigned long *radice = (unsigned long *)mappa;
    if (radice == attuale()) panico("paginazione: cancello la mappa che sto usando");
    /* solo la riga dei programmi: le altre sono del kernel, condivise, e restano */
    unsigned long *pdpt = sotto(radice[RIGA_PROGRAMMI]);
    if (pdpt) {
        for (int i = 0; i < RIGHE; i++) {
            unsigned long *pd = sotto(pdpt[i]);
            if (!pd) continue;
            for (int j = 0; j < RIGHE; j++) {
                unsigned long *pt = sotto(pd[j]);
                if (!pt) continue;
                for (int k = 0; k < RIGHE; k++)
                    if ((pt[k] & P) && !(pt[k] & PAG_PRESTATA))   /* le sue pagine */
                        pagina_restituisci(INDIRIZZO(pt[k]));
                pagina_restituisci((indirizzo_t)pt);   /* la PT */
                tabelle--;
            }
            pagina_restituisci((indirizzo_t)pd);       /* la PD */
            tabelle--;
        }
        pagina_restituisci((indirizzo_t)pdpt);         /* la PDPT */
        tabelle--;
    }
    pagina_restituisci(mappa);                         /* la PML4 */
    tabelle--;
}

void pag_usa_mappa(indirizzo_t mappa) {
    unsigned long *radice = mappa ? (unsigned long *)mappa : pml4;
    /* scrivere CR3 fa dimenticare alla CPU tutto il TLB: lo si fa solo se cambia davvero */
    if (radice != attuale()) __asm__ volatile("mov %0, %%cr3" : : "r"(radice) : "memory");
}

/* --- i dispositivi --- */

/* La memoria di un dispositivo: pagine da 4 KB, senza cache (PCD e PWT): ogni lettura e
 * scrittura arriva al dispositivo. Non deve cadere nella RAM (le sue pagine ci sono gia',
 * con la cache) ne' nella riga dei programmi. Le tabelle nuove vengono dal magazziniere:
 * si chiama dopo pagine_prepara. Rimappare una zona gia' mappata come dispositivo va bene. */
int pag_mappa_dispositivo(indirizzo_t fisico, unsigned int byte) {
    indirizzo_t fine = fisico + byte;
    if (fine < fisico || fine > 0x800000000000ul) return 0;   /* oltre i 128 TB: no (non e' un indirizzo "basso") */
    for (indirizzo_t p = fisico & ~(KB4 - 1); p < fine; p += KB4) {
        if (INDICE_PML4(p) == RIGA_PROGRAMMI) return 0;
        unsigned long *pdpt = scendi(pml4, INDICE_PML4(p), 1, P | RW);
        if (!pdpt) return 0;
        unsigned long *pd = scendi(pdpt, INDICE_PDPT(p), 1, P | RW);
        if (!pd) return 0;
        if (pd[INDICE_PD(p)] & PS) return 0;                 /* una pagina grande: RAM */
        unsigned long *pt = scendi(pd, INDICE_PD(p), 1, P | RW);
        if (!pt) return 0;
        unsigned long riga = pt[INDICE_PT(p)];
        if (riga & P) {
            if (riga & PCD) continue;                        /* gia' mappata come dispositivo */
            return 0;                                        /* e' RAM */
        }
        pt[INDICE_PT(p)] = p | P | RW | PCD | PWT;
        dimentica(p);
    }
    return 1;
}

void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo) {
    /* Niente da fare: la mappa del kernel e' gia' in uso da mmu_prepara, e lo schermo
     * l'ha gia' mappato l'avvio (avvio.c), per poterci scrivere subito. */
    (void)schermo; (void)byte_schermo;
}
