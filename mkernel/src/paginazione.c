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
 * QUESTO PRIMO PASSO: una sola mappa, per il kernel, con indirizzo virtuale = fisico
 * ("mappatura identica"), cosi' dopo l'accensione tutto funziona come prima. In piu':
 *   - la pagina 0 non c'e': un puntatore nullo fa page fault invece di fare danni
 *   - codice e costanti del kernel sono di sola lettura
 *
 * Le tabelle sono pagine prese dal magazziniere: qui contabilita' (il magazziniere)
 * e hardware (la MMU) si incontrano. */
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

static unsigned int *directory;   /* la directory del kernel */
static int accesa;
static unsigned int tabelle;

/* La CPU tiene le traduzioni recenti in un taccuino (il TLB). Se si cambia una riga,
 * bisogna dirle di dimenticare quella pagina, se no continuerebbe a usare la vecchia. */
static void dimentica(unsigned int virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* La tabella che copre l'indirizzo 'virt'. Con crea=1, se non c'e' la si crea. */
static unsigned int *tabella_di(unsigned int virt, int crea) {
    unsigned int *riga = &directory[INDICE_DIR(virt)];
    if (!(*riga & PAG_PRESENTE)) {
        if (!crea) return 0;
        unsigned int nuova = pagina_chiedi();   /* una tabella e' una pagina */
        if (!nuova) panico("paginazione: memoria finita per le tabelle");
        memset((void *)nuova, 0, PAGINA);       /* tutte le righe "assente" */
        /* la riga della directory e' permissiva: i permessi veri li decide la tabella
         * (la CPU guarda tutti e due i livelli e applica il piu' severo) */
        *riga = nuova | PAG_PRESENTE | PAG_SCRITTURA | PAG_UTENTE;
        tabelle++;
    }
    return (unsigned int *)INDIRIZZO(*riga);
}

void pag_mappa(unsigned int virt, unsigned int fis, unsigned int permessi) {
    unsigned int *tab = tabella_di(virt, 1);
    tab[INDICE_TAB(virt)] = INDIRIZZO(fis) | permessi | PAG_PRESENTE;
    if (accesa) dimentica(virt);
}

int pag_traduci(unsigned int virt, unsigned int *fis, unsigned int *permessi) {
    unsigned int *tab = tabella_di(virt, 0);
    if (!tab) return 0;
    unsigned int riga = tab[INDICE_TAB(virt)];
    if (!(riga & PAG_PRESENTE)) return 0;
    *fis = INDIRIZZO(riga) | (virt & 0xFFF);   /* la pagina vera + la posizione dentro */
    *permessi = riga & 0xFFF;
    return 1;
}

unsigned int pag_tabelle(void) { return tabelle; }

void paginazione_prepara(void) {
    directory = (unsigned int *)pagina_chiedi();
    if (!directory) panico("paginazione: memoria finita per la directory");
    memset(directory, 0, PAGINA);

    /* Mappatura identica di tutta la RAM, tranne la pagina 0.
     * Il codice e le costanti del kernel: sola lettura. Il resto: lettura e scrittura.
     * Nessuna pagina e' PAG_UTENTE: per ora il ring 3 non vede niente. */
    unsigned int fine = memoria_fine();
    for (unsigned int p = PAGINA; p < fine; p += PAGINA) {
        int codice = p >= (unsigned int)kernel_start && p < (unsigned int)sola_lettura_fine;
        pag_mappa(p, p, codice ? 0 : PAG_SCRITTURA);
    }

    /* "la directory e' qui", poi si accende. Da questa istruzione in poi ogni
     * indirizzo passa dalla traduzione: se le tabelle fossero sbagliate, la prossima
     * istruzione (che sta in una pagina del kernel) farebbe page fault. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(directory));
    unsigned int cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PAGINAZIONE | CR0_PROTEGGI_SCRITTURA;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
    accesa = 1;
}
