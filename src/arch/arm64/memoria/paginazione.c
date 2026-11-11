/* La paginazione dei programmi su ARM64.
 *
 * Il kernel ha gia' la sua mappa (mmu.c): la meta' alta degli indirizzi, con la sua
 * radice in TTBR1, uguale per tutti e mai cambiata. Qui facciamo le mappe dei
 * PROGRAMMI, la meta' bassa, con la stessa interfaccia di x86 (paginazione.h); e la zona
 * dei dispositivi del kernel (pag_mappa_dispositivo).
 *
 *   indirizzo virtuale: | L0: 9 bit | L1: 9 bit | L2: 9 bit | L3: 9 bit | posizione: 12 bit |
 *   una voce di L0 copre 512 GB, di L1 1 GB, di L2 2 MB, di L3 4 KB
 *
 * LA MAPPA DI UN PROGRAMMA e' una sua tabella L0, che va in TTBR0: tutta sua, perche' il
 * kernel sta nell'altra radice (TTBR1). Non serve copiarci niente del kernel, come
 * invece sul PC (dove la radice e' una sola per tutte e due le meta'). I programmi usano i
 * primi 2 GB (chiamate.h), quindi la voce 0 di L0; sotto ci sono L1, L2 e L3 create
 * quando servono, con le sue pagine. Lo scheduler, a ogni cambio di turno, cambia solo
 * TTBR0. La "mappa del kernel" (la mappa 0) e' una L0 vuota: la meta' bassa non porta a
 * niente, e un puntatore nullo fa errore.
 *
 * LE VOCI DELLE PAGINE (L3), i bit che usiamo:
 *   bit 1-0 = 11     e' una pagina
 *   bit 4-2 = 1      memoria normale, con la cache
 *   bit 6   AP[1]    1 = la puo' usare anche EL0 (PAG_UTENTE)
 *   bit 7   AP[2]    1 = sola lettura (senza PAG_SCRITTURA)
 *   bit 9-8 = 11     condivisa fra le CPU
 *   bit 10  AF       "e' stata usata": a 1, se no la CPU da' errore al primo uso
 *   bit 53  PXN      SEMPRE 1: il kernel non esegue mai il codice dei programmi
 *   bit 54  UXN      0: il programma puo' eseguire. Con WXN acceso (kernel_head.s) cio'
 *                    che e' scrivibile non si esegue comunque: il codice, di sola lettura, si'.
 *   bit 55           uno dei bit "software", che la CPU ignora: PAG_PRESTATA
 * Le voci di tabella (L0-L2) sono solo: bit 1-0 = 11 e l'indirizzo della tabella sotto.
 *
 * Le tabelle sono pagine prese dal magazziniere: nelle voci c'e' il loro indirizzo
 * FISICO (la CPU legge la RAM vera); il kernel le scrive dalla mappa diretta
 * (virt_da_fisico). Una mappa si indica con l'indirizzo fisico della sua L0, quello che
 * va in TTBR0. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"
#include "mmu.h"

#define VOCI 512
#define INDICE_L0(v) (((v) >> 39) & 0x1FF)
#define INDICE_L1(v) (((v) >> 30) & 0x1FF)
#define INDICE_L2(v) (((v) >> 21) & 0x1FF)
#define INDICE_L3(v) (((v) >> 12) & 0x1FF)
#define INDIRIZZO(voce) ((voce) & 0x0000FFFFFFFFF000ul)   /* la voce senza i bit dei permessi */

#define VALIDA     0x3ul          /* bit 1-0 = 11: tabella (L0-L2) o pagina (L3) */
#define BLOCCO     0x1ul          /* bit 1-0 = 01: un blocco (L1, L2) */
#define TIPO_DISPOSITIVO (0ul << 2)
#define TIPO_NORMALE (1ul << 2)
#define AP_UTENTE  (1ul << 6)
#define AP_SOLA_LETTURA (1ul << 7)
#define CONDIVISA  (3ul << 8)
#define USATA      (1ul << 10)
#define NO_ESEGUI_KERNEL (1ul << 53)
#define NO_ESEGUI_PROG   (1ul << 54)
#define VOCE_PRESTATA (1ul << 55)

#define KB4 0x1000ul
#define MB2 0x200000ul

/* La mappa del kernel per la meta' bassa: vuota. */
static unsigned long vuota[VOCI] __attribute__((aligned(4096)));
static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Dopo aver cambiato una
 * voce, prima si aspetta che la scrittura sia arrivata in memoria (dsb ishst), poi si
 * dice alla CPU di dimenticare quella pagina (in tutti gli ASID: non li usiamo). */
static void dimentica(indirizzo_t virt) {
    __asm__ volatile("dsb ishst\n tlbi vaae1is, %0\n dsb ish\n isb" : : "r"(virt >> 12) : "memory");
}

static indirizzo_t ttbr0_attuale(void) {   /* l'indirizzo fisico della L0 in uso */
    unsigned long ttbr;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr));
    return INDIRIZZO(ttbr);
}

static unsigned long *attuale(void) { return virt_da_fisico(ttbr0_attuale()); }

static int zona_programmi(indirizzo_t virt) { return virt >= PROGRAMMI_INIZIO && virt < PROGRAMMI_FINE; }

/* Una tabella nuova: una pagina, tutta a zero (tutte le voci "assenti"). Il suo
 * indirizzo virtuale, e in *fisico quello da scrivere nella voce sopra. */
static unsigned long *tabella_nuova(indirizzo_t *fisico) {
    indirizzo_t p = pagina_chiedi();
    if (!p) return 0;
    unsigned long *t = virt_da_fisico(p);
    memset(t, 0, PAGINA);
    __asm__ volatile("dsb ishst" : : : "memory");   /* e' scritta davvero, prima che la CPU la legga */
    tabelle++;
    *fisico = p;
    return t;
}

/* Nella tabella 't', la tabella sotto alla voce 'i'. Con crea=1, se non c'e' la si crea
 * (0 se il magazziniere non ha pagine). */
static unsigned long *scendi(unsigned long *t, unsigned long i, int crea) {
    if ((t[i] & VALIDA) != VALIDA) {
        if (!crea) return 0;
        indirizzo_t fisico;
        if (!tabella_nuova(&fisico)) return 0;
        t[i] = fisico | VALIDA;
    }
    return virt_da_fisico(INDIRIZZO(t[i]));
}

/* La L3 che copre 'virt' nella mappa 'l0'. */
static unsigned long *l3_di(unsigned long *l0, indirizzo_t virt, int crea) {
    unsigned long *l1 = scendi(l0, INDICE_L0(virt), crea);
    if (!l1) return 0;
    unsigned long *l2 = scendi(l1, INDICE_L1(virt), crea);
    if (!l2) return 0;
    return scendi(l2, INDICE_L2(virt), crea);
}

int pag_traduci(indirizzo_t virt, indirizzo_t *fis, unsigned int *permessi) {
    /* Fuori dalla zona dei programmi: 0. Il kernel usa questa funzione per controllare i
     * puntatori che arrivano dai programmi: un programma non puo' passare indirizzi del
     * kernel (la meta' alta). */
    if (!zona_programmi(virt)) return 0;
    unsigned long *l3 = l3_di(attuale(), virt, 0);
    if (!l3) return 0;
    unsigned long voce = l3[INDICE_L3(virt)];
    if ((voce & VALIDA) != VALIDA) return 0;
    *fis = INDIRIZZO(voce) | (virt & 0xFFF);   /* la pagina vera + la posizione dentro */
    unsigned int p = PAG_PRESENTE;
    if (voce & AP_UTENTE) p |= PAG_UTENTE;
    if (!(voce & AP_SOLA_LETTURA)) p |= PAG_SCRITTURA;
    if (voce & VOCE_PRESTATA) p |= PAG_PRESTATA;
    *permessi = p;
    return 1;
}

unsigned int pag_tabelle(void) { return tabelle; }

/* --- le mappe dei programmi --- */

indirizzo_t pag_nuova_mappa(void) {
    indirizzo_t fisico;   /* tutta a zero: il kernel sta nell'altra radice (TTBR1) */
    if (!tabella_nuova(&fisico)) return 0;
    return fisico;
}

int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi) {
    if (!zona_programmi(virt)) return 0;   /* fuori: non e' dei programmi */
    unsigned long *l3 = l3_di(virt_da_fisico(mappa), virt, 1);
    if (!l3) return 0;
    unsigned long voce = INDIRIZZO(fis) | VALIDA | TIPO_NORMALE | CONDIVISA | USATA | NO_ESEGUI_KERNEL;
    if (permessi & PAG_UTENTE) voce |= AP_UTENTE;
    if (!(permessi & PAG_SCRITTURA)) voce |= AP_SOLA_LETTURA;
    if (permessi & PAG_PRESTATA) voce |= VOCE_PRESTATA;
    l3[INDICE_L3(virt)] = voce;
    if (mappa == ttbr0_attuale()) dimentica(virt);
    else __asm__ volatile("dsb ishst" : : : "memory");
    return 1;
}

void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt) {
    if (!zona_programmi(virt)) return;
    unsigned long *l3 = l3_di(virt_da_fisico(mappa), virt, 0);
    if (!l3) return;
    l3[INDICE_L3(virt)] = 0;
    if (mappa == ttbr0_attuale()) dimentica(virt);
    else __asm__ volatile("dsb ishst" : : : "memory");
}

/* La tabella sotto la voce 'v' (0 se non c'e'). */
static unsigned long *sotto(unsigned long v) {
    return (v & VALIDA) == VALIDA ? virt_da_fisico(INDIRIZZO(v)) : 0;
}

unsigned int pag_conta_programma(indirizzo_t mappa) {
    unsigned long *l0 = virt_da_fisico(mappa);
    unsigned int n = 0;
    for (int r = 0; r < VOCI; r++) {          /* tutta la L0: e' tutta del programma */
        unsigned long *l1 = sotto(l0[r]);
        if (!l1) continue;
        for (int i = 0; i < VOCI; i++) {
            unsigned long *l2 = sotto(l1[i]);
            if (!l2) continue;
            for (int j = 0; j < VOCI; j++) {
                unsigned long *l3 = sotto(l2[j]);
                if (!l3) continue;
                for (int k = 0; k < VOCI; k++)
                    if ((l3[k] & VALIDA) == VALIDA && !(l3[k] & VOCE_PRESTATA)) n++;   /* le sue pagine */
            }
        }
    }
    return n;
}

void pag_cancella_mappa(indirizzo_t mappa) {
    if (mappa == ttbr0_attuale()) panico("paginazione: cancello la mappa che sto usando");
    unsigned long *l0 = virt_da_fisico(mappa);
    for (int r = 0; r < VOCI; r++) {
        unsigned long *l1 = sotto(l0[r]);
        if (!l1) continue;
        for (int i = 0; i < VOCI; i++) {
            unsigned long *l2 = sotto(l1[i]);
            if (!l2) continue;
            for (int j = 0; j < VOCI; j++) {
                unsigned long *l3 = sotto(l2[j]);
                if (!l3) continue;
                for (int k = 0; k < VOCI; k++)
                    if ((l3[k] & VALIDA) == VALIDA && !(l3[k] & VOCE_PRESTATA))   /* le sue pagine */
                        pagina_restituisci(INDIRIZZO(l3[k]));
                pagina_restituisci(INDIRIZZO(l2[j]));   /* la L3 */
                tabelle--;
            }
            pagina_restituisci(INDIRIZZO(l1[i]));       /* la L2 */
            tabelle--;
        }
        pagina_restituisci(INDIRIZZO(l0[r]));           /* la L1 */
        tabelle--;
    }
    pagina_restituisci(mappa);                          /* la L0 */
    tabelle--;
}

void pag_usa_mappa(indirizzo_t mappa) {
    indirizzo_t l0 = mappa ? mappa : fisico_da_virt(vuota);
    /* Non usiamo gli ASID (i numeri che distinguono le mappe nel TLB): cambiando mappa
     * si dimentica tutto il TLB. Lo si fa solo se la mappa cambia davvero. La meta' alta
     * (TTBR1) non si tocca mai. */
    if (l0 == ttbr0_attuale()) return;
    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "isb\n"
        "tlbi vmalle1\n"
        "dsb ish\n"
        "isb"
        : : "r"(l0) : "memory");
}

/* --- i dispositivi --- */

/* I pezzi gia' mappati: chi chiede di nuovo lo stesso pezzo (o un pezzo dentro) riceve
 * lo stesso indirizzo. */
#define MAX_PEZZI_DISPOSITIVI 32
static struct { indirizzo_t fisico, virtuale, byte; } pezzi[MAX_PEZZI_DISPOSITIVI];
static int quanti_pezzi;
static indirizzo_t prossimo = ZONA_DISPOSITIVI;   /* il primo indirizzo libero della zona */

/* Come sul PC: ogni pezzo nuovo comincia in un posto da 2 MB tutto suo, alla stessa
 * distanza dall'inizio dei 2 MB che ha l'indirizzo fisico, cosi' dove copre 2 MB interi
 * si usa un blocco (la finestra ECAM del PCI e' di 256 MB) e pagine da 4 KB solo ai
 * bordi. Tipo DISPOSITIVO, solo per il kernel, non si esegue. La L1 della zona e' nella
 * meta' alta (TTBR1), uguale per tutti: un pezzo aggiunto si vede da ogni programma. */
indirizzo_t pag_mappa_dispositivo(indirizzo_t fisico, unsigned int byte) {
    if (byte == 0) return 0;
    for (int i = 0; i < quanti_pezzi; i++)
        if (fisico >= pezzi[i].fisico && fisico + byte <= pezzi[i].fisico + pezzi[i].byte)
            return pezzi[i].virtuale + (fisico - pezzi[i].fisico);

    indirizzo_t da = fisico & ~(KB4 - 1), a = (fisico + byte + KB4 - 1) & ~(KB4 - 1);
    if (a <= da) return 0;                                    /* fa il giro: non e' vero */
    indirizzo_t virt = ((prossimo + MB2 - 1) & ~(MB2 - 1)) + (da & (MB2 - 1));
    if (virt + (a - da) > ZONA_DISPOSITIVI_FINE) return 0;   /* la zona e' piena */

    unsigned long *l1 = mmu_l1_dispositivi();
    const unsigned long voce = TIPO_DISPOSITIVO | USATA | NO_ESEGUI_KERNEL | NO_ESEGUI_PROG;
    for (indirizzo_t p = da; p < a; ) {
        indirizzo_t v = virt + (p - da);
        unsigned long *l2 = scendi(l1, INDICE_L1(v), 1);
        if (!l2) return 0;
        if (!(p & (MB2 - 1)) && p + MB2 <= a) {              /* 2 MB interi: un blocco */
            l2[INDICE_L2(v)] = p | BLOCCO | voce;
            p += MB2;
            continue;
        }
        unsigned long *l3 = scendi(l2, INDICE_L2(v), 1);
        if (!l3) return 0;
        l3[INDICE_L3(v)] = p | VALIDA | voce;
        p += KB4;
    }
    /* Voci che prima erano vuote: nel TLB non c'erano, basta che siano scritte. */
    __asm__ volatile("dsb ishst\n isb" : : : "memory");
    prossimo = virt + (a - da);
    if (quanti_pezzi < MAX_PEZZI_DISPOSITIVI)
        pezzi[quanti_pezzi++] = (typeof(pezzi[0])){ da, virt, a - da };
    return virt + (fisico - da);
}

void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo) {
    /* Niente da fare: la MMU e' gia' accesa da kernel_head.s e mmu_prepara, e la mappa
     * provvisoria della meta' bassa l'ha gia' tolta avvio.c. */
    (void)schermo; (void)byte_schermo;
}
