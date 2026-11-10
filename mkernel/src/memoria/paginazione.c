/* La paginazione.
 *
 * Con la paginazione accesa, ogni indirizzo usato dal codice e' VIRTUALE: a ogni
 * lettura e scrittura la CPU lo traduce in un indirizzo fisico (la RAM vera) usando
 * delle tabelle scritte dal kernel, e controlla i permessi. Se la pagina non c'e', o
 * il permesso manca, la CPU si ferma e chiama il kernel: eccezione 14, page fault.
 *
 * Le tabelle, sull'x86 a 32 bit, sono su due livelli, come l'indice di un libro:
 *
 *   indirizzo virtuale:  [ 10 bit directory | 10 bit tabella | 12 bit posizione ]
 *
 *   DIRECTORY (1 pagina, 1024 righe): ogni riga punta a una TABELLA
 *   TABELLA   (1 pagina, 1024 righe): ogni riga punta a una PAGINA vera, con i permessi
 *   una riga della directory copre 4 MB, una riga di tabella 4 KB
 *
 * Il registro CR3 dice alla CPU dove sta la directory. Il bit 31 di CR0 accende tutto.
 *
 * LA MAPPA DEL KERNEL: indirizzo virtuale = fisico ("mappatura identica") per tutta la
 * RAM e per la memoria dello schermo, solo ring 0. In piu':
 *   - la pagina 0 non c'e': un puntatore nullo fa page fault invece di fare danni
 *   - codice e costanti del kernel sono di sola lettura
 *
 * LE MAPPE DEI PROGRAMMI: una per ogni programma in ring 3. Ogni mappa ha
 *   - le stesse righe della directory del kernel, che puntano alle STESSE tabelle del
 *     kernel: il kernel c'e' in ogni mappa (serve quando arriva un'interruzione), ma
 *     le sue pagine non hanno il bit utente, quindi il programma non le puo' toccare;
 *   - da 1 a 2 GB (PROGRAMMI_INIZIO - PROGRAMMI_FINE) tabelle solo sue, con le sue pagine.
 * Lo scheduler, a ogni cambio di turno, mette in CR3 la mappa di chi parte.
 *
 * Le directory e le tabelle sono pagine prese dal magazziniere. Siccome il kernel ha la
 * mappatura identica in ogni mappa, il loro indirizzo fisico si puo' usare direttamente
 * come puntatore. */
#include "kernel.h"
#include "pagine.h"
#include "paginazione.h"

#define RIGHE_TABELLA 1024
#define INDICE_DIR(v) ((v) >> 22)             /* i primi 10 bit */
#define INDICE_TAB(v) (((v) >> 12) & 0x3FF)   /* i 10 bit di mezzo */
#define INDIRIZZO(riga) ((riga) & ~0xFFFu)    /* la riga senza i bit dei permessi */

#define CR0_PAGINAZIONE  0x80000000   /* PG: accende la paginazione */
#define CR0_PROTEGGI_SCRITTURA 0x10000 /* WP: anche il kernel rispetta la sola lettura */

extern char kernel_start[], sola_lettura_fine[];   /* dal linker script */

static unsigned int *kernel;   /* la directory del kernel */
static int accesa;
static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Se si cambia una riga,
 * bisogna dirle di dimenticare quella pagina, se no continuerebbe a usare la vecchia. */
static void dimentica(unsigned int virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static unsigned int *attuale(void) {   /* la directory che la CPU sta usando */
    unsigned int cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return (unsigned int *)cr3;
}

/* Nella directory 'dir', la tabella che copre l'indirizzo 'virt'.
 * Con crea=1, se non c'e' la si crea (0 se il magazziniere non ha pagine). */
static unsigned int *tabella_di(unsigned int *dir, unsigned int virt, int crea) {
    unsigned int *riga = &dir[INDICE_DIR(virt)];
    if (!(*riga & PAG_PRESENTE)) {
        if (!crea) return 0;
        unsigned int nuova = pagina_chiedi();   /* una tabella e' una pagina */
        if (!nuova) return 0;
        memset((void *)nuova, 0, PAGINA);       /* tutte le righe "assente" */
        /* la riga della directory e' permissiva: i permessi veri li decide la tabella
         * (la CPU guarda tutti e due i livelli e applica il piu' severo) */
        *riga = nuova | PAG_PRESENTE | PAG_SCRITTURA | PAG_UTENTE;
        tabelle++;
    }
    return (unsigned int *)INDIRIZZO(*riga);
}

static int mappa_in(unsigned int *dir, unsigned int virt, unsigned int fis, unsigned int permessi) {
    unsigned int *tab = tabella_di(dir, virt, 1);
    if (!tab) return 0;
    tab[INDICE_TAB(virt)] = INDIRIZZO(fis) | permessi | PAG_PRESENTE;
    if (accesa && dir == attuale()) dimentica(virt);
    return 1;
}

/* Nella mappa del kernel. */
static void pag_mappa(unsigned int virt, unsigned int fis, unsigned int permessi) {
    if (!mappa_in(kernel, virt, fis, permessi))
        panico("paginazione: memoria finita per le tabelle");
}

int pag_traduci(unsigned int virt, unsigned int *fis, unsigned int *permessi) {
    unsigned int *tab = tabella_di(accesa ? attuale() : kernel, virt, 0);
    if (!tab) return 0;
    unsigned int riga = tab[INDICE_TAB(virt)];
    if (!(riga & PAG_PRESENTE)) return 0;
    *fis = INDIRIZZO(riga) | (virt & 0xFFF);   /* la pagina vera + la posizione dentro */
    *permessi = riga & 0xFFF;
    return 1;
}

unsigned int pag_tabelle(void) { return tabelle; }

/* --- le mappe dei programmi --- */

unsigned int pag_nuova_mappa(void) {
    unsigned int *dir = (unsigned int *)pagina_chiedi();
    if (!dir) return 0;
    /* la parte del kernel: le stesse righe, quindi le stesse tabelle (condivise).
     * Da 1 a 2 GB la directory del kernel e' vuota: li' andranno le tabelle del programma. */
    for (int i = 0; i < RIGHE_TABELLA; i++) dir[i] = kernel[i];
    tabelle++;
    return (unsigned int)dir;
}

int pag_mappa_programma(unsigned int mappa, unsigned int virt, unsigned int fis, unsigned int permessi) {
    if (virt < PROGRAMMI_INIZIO || virt >= PROGRAMMI_FINE) return 0;   /* fuori: e' del kernel */
    return mappa_in((unsigned int *)mappa, virt, fis, permessi);
}

void pag_cancella_mappa(unsigned int mappa) {
    unsigned int *dir = (unsigned int *)mappa;
    if (dir == attuale()) panico("paginazione: cancello la mappa che sto usando");
    /* solo le righe del programma: quelle del kernel sono condivise e restano */
    for (unsigned int i = INDICE_DIR(PROGRAMMI_INIZIO); i < INDICE_DIR(PROGRAMMI_FINE); i++) {
        if (!(dir[i] & PAG_PRESENTE)) continue;
        unsigned int *tab = (unsigned int *)INDIRIZZO(dir[i]);
        for (int j = 0; j < RIGHE_TABELLA; j++)
            if ((tab[j] & PAG_PRESENTE) && !(tab[j] & PAG_PRESTATA))         /* le sue pagine */
                pagina_restituisci(INDIRIZZO(tab[j]));
        pagina_restituisci((unsigned int)tab);                                   /* la tabella */
        tabelle--;
    }
    pagina_restituisci(mappa);                                                   /* la directory */
    tabelle--;
}

void pag_usa_mappa(unsigned int mappa) {
    unsigned int *dir = mappa ? (unsigned int *)mappa : kernel;
    /* scrivere CR3 fa dimenticare alla CPU tutto il TLB: lo si fa solo se cambia davvero */
    if (dir != attuale()) __asm__ volatile("mov %0, %%cr3" : : "r"(dir) : "memory");
}

/* --- l'avvio --- */

void paginazione_prepara(unsigned int schermo, unsigned int byte_schermo) {
    kernel = (unsigned int *)pagina_chiedi();
    if (!kernel) panico("paginazione: memoria finita per la directory");
    memset(kernel, 0, PAGINA);

    /* I programmi vivono dal 1 GB in su: la RAM deve stare sotto, se no le due zone
     * si sovrapporrebbero. Con QEMU (128 MB di serie) c'e' molto margine. */
    unsigned int fine = memoria_fine();
    if (fine > PROGRAMMI_INIZIO)
        panico("paginazione: piu' di %u MB di RAM, la zona dei programmi si sovrappone",
               PROGRAMMI_INIZIO >> 20);

    /* Mappatura identica di tutta la RAM, tranne la pagina 0. Solo ring 0.
     * Il codice e le costanti del kernel: sola lettura. Il resto: lettura e scrittura. */
    for (unsigned int p = PAGINA; p < fine; p += PAGINA) {
        int codice = p >= (unsigned int)kernel_start && p < (unsigned int)sola_lettura_fine;
        pag_mappa(p, p, codice ? 0 : PAG_SCRITTURA);
    }

    /* La memoria dello schermo: non e' RAM, e' la memoria della scheda video, che il
     * bootloader ci ha detto dove sta (di solito sopra i 3 GB). Anche lei con indirizzo
     * virtuale = fisico, lettura e scrittura, solo kernel. Non deve cadere nella RAM ne'
     * nella zona dei programmi. */
    unsigned int schermo_fine = schermo + byte_schermo;
    if (schermo < fine || schermo_fine < schermo ||
        (schermo < PROGRAMMI_FINE && schermo_fine > PROGRAMMI_INIZIO))
        panico("paginazione: lo schermo a %p e' in una zona gia' occupata", (void *)schermo);
    for (unsigned int p = schermo & ~(PAGINA - 1); p < schermo_fine; p += PAGINA)
        pag_mappa(p, p, PAG_SCRITTURA);

    /* "la directory e' qui", poi si accende. Da questa istruzione in poi ogni
     * indirizzo passa dalla traduzione: se le tabelle fossero sbagliate, la prossima
     * istruzione (che sta in una pagina del kernel) farebbe page fault. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel));
    unsigned int cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PAGINAZIONE | CR0_PROTEGGI_SCRITTURA;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    accesa = 1;
}
