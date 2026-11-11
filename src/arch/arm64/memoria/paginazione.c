/* La paginazione dei programmi su ARM64.
 *
 * Il kernel ha gia' la sua mappa (mmu.c): quattro livelli di tabelle, L0, L1, L2, L3,
 * ognuna di 512 voci da 8 byte. Qui facciamo le mappe dei PROGRAMMI, con la stessa
 * interfaccia di x86 (paginazione.h).
 *
 *   indirizzo virtuale: | L0: 9 bit | L1: 9 bit | L2: 9 bit | L3: 9 bit | posizione: 12 bit |
 *   una voce di L0 copre 512 GB, di L1 1 GB, di L2 2 MB, di L3 4 KB
 *
 * LA MAPPA DI UN PROGRAMMA e' una sua tabella L0, che ha solo due voci:
 *   - la voce 0 (i primi 512 GB) e' UGUALE a quella del kernel: punta alla STESSA L1,
 *     condivisa. Cosi' il kernel c'e' in ogni mappa (serve quando arriva un'interruzione
 *     o una chiamata), ma le sue pagine non hanno il permesso per EL0, i programmi:
 *     se le toccano, la CPU li ferma;
 *   - la voce 1 (da 512 GB a 1 TB) e' solo del programma: sotto ci sono L1, L2 e L3
 *     create quando servono, con le sue pagine.
 *
 * PERCHE' I PROGRAMMI STANNO A 512 GB. Su x86 stanno a 1-2 GB, dove il kernel non c'e'.
 * Su ARM, nella macchina "virt" di QEMU, la RAM sta proprio fra 1 e 2 GB: li' c'e' il
 * kernel. Per non pestarci i piedi mettiamo i programmi nella voce 1 di L0, oltre i
 * primi 512 GB: una voce intera tutta per loro, e la voce 0 resta quella del kernel.
 * (PROGRAMMI_INIZIO e' 512 GB + 1 GB, PROGRAMMI_FINE 512 GB + 2 GB: vedi chiamate.h.)
 *
 * LE VOCI DELLE PAGINE (L3), i bit che usiamo:
 *   bit 1-0 = 11     e' una pagina
 *   bit 4-2 = 1      memoria normale, con la cache
 *   bit 6   AP[1]    1 = la puo' usare anche EL0 (PAG_UTENTE)
 *   bit 7   AP[2]    1 = sola lettura (senza PAG_SCRITTURA)
 *   bit 9-8 = 11     condivisa fra le CPU
 *   bit 10  AF       "e' stata usata": a 1, se no la CPU da' errore al primo uso
 *   bit 53  PXN      SEMPRE 1: il kernel non esegue mai il codice dei programmi
 *   bit 54  UXN      0: il programma puo' eseguire. Con WXN acceso (mmu.c) cio' che e'
 *                    scrivibile non si esegue comunque: il codice, di sola lettura, si'.
 *   bit 55           uno dei bit "software", che la CPU ignora: PAG_PRESTATA
 * Le voci di tabella (L0-L2) sono solo: bit 1-0 = 11 e l'indirizzo della tabella sotto.
 *
 * Le tabelle sono pagine prese dal magazziniere. Siccome la mappa del kernel e'
 * identica (virtuale = fisico), il loro indirizzo fisico si usa direttamente come puntatore. */
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
#define TIPO_NORMALE (1ul << 2)
#define AP_UTENTE  (1ul << 6)
#define AP_SOLA_LETTURA (1ul << 7)
#define CONDIVISA  (3ul << 8)
#define USATA      (1ul << 10)
#define NO_ESEGUI_KERNEL (1ul << 53)
#define VOCE_PRESTATA (1ul << 55)

#define VOCE_PROGRAMMI 1          /* la voce di L0 dei programmi (512 GB) */

static unsigned long *kernel;     /* la L0 del kernel (la mappa 0) */
static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Dopo aver cambiato una
 * voce, prima si aspetta che la scrittura sia arrivata in memoria (dsb ishst), poi si
 * dice alla CPU di dimenticare quella pagina (in tutti gli ASID: non li usiamo). */
static void dimentica(indirizzo_t virt) {
    __asm__ volatile("dsb ishst\n tlbi vaae1is, %0\n dsb ish\n isb" : : "r"(virt >> 12) : "memory");
}

static unsigned long *attuale(void) {   /* la L0 che la CPU sta usando */
    unsigned long ttbr;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr));
    return (unsigned long *)INDIRIZZO(ttbr);
}

static int zona_programmi(indirizzo_t virt) { return virt >= PROGRAMMI_INIZIO && virt < PROGRAMMI_FINE; }

/* Una tabella nuova: una pagina, tutta a zero (tutte le voci "assenti"). */
static unsigned long *tabella_nuova(void) {
    unsigned long *t = (unsigned long *)pagina_chiedi();
    if (!t) return 0;
    memset(t, 0, PAGINA);
    __asm__ volatile("dsb ishst" : : : "memory");   /* e' scritta davvero, prima che la CPU la legga */
    tabelle++;
    return t;
}

/* Nella tabella 't', la tabella sotto alla voce 'i'. Con crea=1, se non c'e' la si crea
 * (0 se il magazziniere non ha pagine). */
static unsigned long *scendi(unsigned long *t, unsigned long i, int crea) {
    if ((t[i] & VALIDA) != VALIDA) {
        if (!crea) return 0;
        unsigned long *nuova = tabella_nuova();
        if (!nuova) return 0;
        t[i] = (unsigned long)nuova | VALIDA;
    }
    return (unsigned long *)INDIRIZZO(t[i]);
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
     * puntatori che arrivano dai programmi: un programma non puo' passare indirizzi del kernel. */
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
    unsigned long *l0 = tabella_nuova();   /* tutta a zero: anche la voce dei programmi e' vuota */
    if (!l0) return 0;
    l0[0] = kernel[0];   /* la parte del kernel: la stessa L1, condivisa */
    __asm__ volatile("dsb ishst" : : : "memory");
    return (indirizzo_t)l0;
}

int pag_mappa_programma(indirizzo_t mappa, indirizzo_t virt, indirizzo_t fis, unsigned int permessi) {
    if (!zona_programmi(virt)) return 0;   /* fuori: e' del kernel */
    unsigned long *l0 = (unsigned long *)mappa;
    unsigned long *l3 = l3_di(l0, virt, 1);
    if (!l3) return 0;
    unsigned long voce = INDIRIZZO(fis) | VALIDA | TIPO_NORMALE | CONDIVISA | USATA | NO_ESEGUI_KERNEL;
    if (permessi & PAG_UTENTE) voce |= AP_UTENTE;
    if (!(permessi & PAG_SCRITTURA)) voce |= AP_SOLA_LETTURA;
    if (permessi & PAG_PRESTATA) voce |= VOCE_PRESTATA;
    l3[INDICE_L3(virt)] = voce;
    if (l0 == attuale()) dimentica(virt);
    else __asm__ volatile("dsb ishst" : : : "memory");
    return 1;
}

void pag_togli_programma(indirizzo_t mappa, indirizzo_t virt) {
    if (!zona_programmi(virt)) return;
    unsigned long *l0 = (unsigned long *)mappa;
    unsigned long *l3 = l3_di(l0, virt, 0);
    if (!l3) return;
    l3[INDICE_L3(virt)] = 0;
    if (l0 == attuale()) dimentica(virt);
    else __asm__ volatile("dsb ishst" : : : "memory");
}

unsigned int pag_conta_programma(indirizzo_t mappa) {
    unsigned long *l0 = (unsigned long *)mappa;
    unsigned int n = 0;
    unsigned long *l1 = (l0[VOCE_PROGRAMMI] & VALIDA) == VALIDA ? (unsigned long *)INDIRIZZO(l0[VOCE_PROGRAMMI]) : 0;
    if (!l1) return 0;
    for (int i = 0; i < VOCI; i++) {
        if ((l1[i] & VALIDA) != VALIDA) continue;
        unsigned long *l2 = (unsigned long *)INDIRIZZO(l1[i]);
        for (int j = 0; j < VOCI; j++) {
            if ((l2[j] & VALIDA) != VALIDA) continue;
            unsigned long *l3 = (unsigned long *)INDIRIZZO(l2[j]);
            for (int k = 0; k < VOCI; k++)
                if ((l3[k] & VALIDA) == VALIDA && !(l3[k] & VOCE_PRESTATA)) n++;   /* le sue pagine */
        }
    }
    return n;
}

void pag_cancella_mappa(indirizzo_t mappa) {
    unsigned long *l0 = (unsigned long *)mappa;
    if (l0 == attuale()) panico("paginazione: cancello la mappa che sto usando");
    /* solo la voce dei programmi: la voce 0 e' la L1 del kernel, condivisa, e resta */
    if ((l0[VOCE_PROGRAMMI] & VALIDA) == VALIDA) {
        unsigned long *l1 = (unsigned long *)INDIRIZZO(l0[VOCE_PROGRAMMI]);
        for (int i = 0; i < VOCI; i++) {
            if ((l1[i] & VALIDA) != VALIDA) continue;
            unsigned long *l2 = (unsigned long *)INDIRIZZO(l1[i]);
            for (int j = 0; j < VOCI; j++) {
                if ((l2[j] & VALIDA) != VALIDA) continue;
                unsigned long *l3 = (unsigned long *)INDIRIZZO(l2[j]);
                for (int k = 0; k < VOCI; k++)
                    if ((l3[k] & VALIDA) == VALIDA && !(l3[k] & VOCE_PRESTATA))   /* le sue pagine */
                        pagina_restituisci(INDIRIZZO(l3[k]));
                pagina_restituisci((indirizzo_t)l3);   /* la L3 */
                tabelle--;
            }
            pagina_restituisci((indirizzo_t)l2);       /* la L2 */
            tabelle--;
        }
        pagina_restituisci((indirizzo_t)l1);           /* la L1 */
        tabelle--;
    }
    pagina_restituisci(mappa);                         /* la L0 */
    tabelle--;
}

void pag_usa_mappa(indirizzo_t mappa) {
    unsigned long *l0 = mappa ? (unsigned long *)mappa : kernel;
    /* Non usiamo gli ASID (i numeri che distinguono le mappe nel TLB): cambiando mappa
     * si dimentica tutto il TLB. Lo si fa solo se la mappa cambia davvero. */
    if (l0 == attuale()) return;
    __asm__ volatile(
        "msr ttbr0_el1, %0\n"
        "isb\n"
        "tlbi vmalle1\n"
        "dsb ish\n"
        "isb"
        : : "r"(l0) : "memory");
}

/* --- l'avvio --- */

/* La mappa del kernel (mmu.c) copre gia' tutti i dispositivi dei primi 512 GB: basta
 * controllare che l'intervallo ci stia. */
int pag_mappa_dispositivo(indirizzo_t fisico, unsigned int byte) {
    indirizzo_t fine = fisico + byte;
    return fine >= fisico && fine <= 0x8000000000ul;
}

void paginazione_prepara(indirizzo_t schermo, unsigned int byte_schermo) {
    /* Niente da fare: la MMU e' gia' accesa da mmu_prepara. Ci si ricorda solo dove sta
     * la mappa del kernel. Lo schermo e' coperto dalla mappa dei dispositivi. */
    (void)schermo; (void)byte_schermo;
    kernel = (unsigned long *)mmu_tabella_l0();
}

/* --- la prova --- */
#define PROVA_MAPPE 0
#if PROVA_MAPPE
__attribute__((unused)) static void prova_mappe(void) {
    unsigned int libere = pagine_libere();
    indirizzo_t m = pag_nuova_mappa();
    indirizzo_t a = pagina_chiedi(), b = pagina_chiedi(), c = pagina_chiedi();
    int ok = m && a && b && c;
    indirizzo_t va = PROGRAMMI_INIZIO, vb = va + PAGINA, vc = vb + PAGINA;
    ok &= pag_mappa_programma(m, va, a, PAG_UTENTE | PAG_SCRITTURA);
    ok &= pag_mappa_programma(m, vb, b, PAG_UTENTE);
    ok &= pag_mappa_programma(m, vc, c, PAG_UTENTE | PAG_PRESTATA);
    ok &= !pag_mappa_programma(m, 0x40000000ul, a, PAG_UTENTE);   /* fuori zona: no */
    kprintf("prova mappe: mappa %p, tre pagine mappate: %s\n", (void *)m, ok ? "ok" : "ERRORE");
    pag_usa_mappa(m);
    volatile unsigned int *p = (volatile unsigned int *)va;
    *p = 0xABCD1234;
    kprintf("prova mappe: scritto/riletto: %s\n", (*p == 0xABCD1234 && *(volatile unsigned int *)a == 0xABCD1234) ? "ok" : "ERRORE");
    indirizzo_t f; unsigned int perm;
    int t1 = pag_traduci(va + 8, &f, &perm);
    kprintf("prova mappe: traduci A: %s (perm %x)\n", (t1 && f == a + 8 && perm == (PAG_PRESENTE | PAG_UTENTE | PAG_SCRITTURA)) ? "ok" : "ERRORE", perm);
    int t2 = pag_traduci(vb, &f, &perm);
    kprintf("prova mappe: traduci B: %s (perm %x)\n", (t2 && f == b && perm == (PAG_PRESENTE | PAG_UTENTE)) ? "ok" : "ERRORE", perm);
    int t3 = pag_traduci(vc, &f, &perm);
    kprintf("prova mappe: traduci C: %s (perm %x)\n", (t3 && f == c && perm == (PAG_PRESENTE | PAG_UTENTE | PAG_PRESTATA)) ? "ok" : "ERRORE", perm);
    kprintf("prova mappe: non mappato e fuori zona danno 0: %s\n",
            (!pag_traduci(vc + PAGINA, &f, &perm) && !pag_traduci(a, &f, &perm)) ? "ok" : "ERRORE");
    kprintf("prova mappe: conta == 2: %s (%u)\n", pag_conta_programma(m) == 2 ? "ok" : "ERRORE", pag_conta_programma(m));
    pag_togli_programma(m, vb);
    kprintf("prova mappe: dopo togli, conta == 1 e B non c'e': %s\n",
            (pag_conta_programma(m) == 1 && !pag_traduci(vb, &f, &perm)) ? "ok" : "ERRORE");
    pagina_restituisci(b);
    pag_usa_mappa(0);
    pag_cancella_mappa(m);
    pagina_restituisci(c);
    kprintf("prova mappe: pagine libere %u -> %u, tabelle %u: %s\n", libere, pagine_libere(), tabelle,
            (libere == pagine_libere() && tabelle == 0) ? "ok" : "ERRORE");
}
#endif
