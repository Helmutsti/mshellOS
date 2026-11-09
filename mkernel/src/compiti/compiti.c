/* I compiti e lo scheduler.
 *
 * Un COMPITO e' una funzione con una pila sua e una scheda. I compiti stanno in una
 * fila circolare; lo scheduler, a ogni tick del timer (e a ogni cedi), passa al
 * prossimo compito PRONTO: e' il "giro" (round robin).
 *
 * IL TRUCCO DEL CAMBIO DI CONTESTO
 * Quando arriva un'interruzione, isr_comune salva TUTTI i registri del compito
 * interrotto sulla SUA pila. Lo scheduler si annota quell'ESP nella scheda del
 * compito, e restituisce l'ESP di un altro compito: isr_comune ripesca i registri
 * da li' e con iret riparte... dentro l'altro compito, dove si era fermato.
 *
 * IL PRIMO AVVIO
 * Un compito nuovo non e' mai stato interrotto: sulla sua pila prepariamo a mano
 * i registri "come se" lo fosse stato, con EIP = inizio della sua funzione. */
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "timer.h"
#include "pagine.h"
#include "paginazione.h"
#include "compiti.h"

#define DIM_PILA 8192

extern char stack_top[];   /* la cima della pila dell'avvio (kernel_head.s): e' della shell */
#define EFLAGS_INTERRUZIONI_ACCESE 0x202   /* bit IF acceso (+ il bit 1, sempre a 1) */

static struct compito *attuale;     /* chi sta girando */
static struct compito *primo, *ultimo;
static struct compito *inattivo;    /* il compito che fa hlt quando nessuno ha da fare */
static int numero_compiti;
static int prossimo_numero = 1;
static int acceso;                  /* lo scheduler parte solo dopo compiti_prepara */

static void copia_nome(char *dest, const char *src) {
    int i = 0;
    for (; src[i] && i < 15; i++) dest[i] = src[i];
    dest[i] = 0;
}

/* Aggiunge una scheda in fondo alla fila circolare. Va chiamata a interruzioni spente. */
static void in_fila(struct compito *c) {
    if (!primo) { primo = ultimo = c; c->prossimo = c; }
    else { c->prossimo = primo; ultimo->prossimo = c; ultimo = c; }
    numero_compiti++;
}

/* --- quando un compito finisce --- */

/* La funzione di ogni compito "ritorna" qui: la pila finta e' fatta apposta. */
static void fine_compito(void) {
    interruzioni_spegni();
    if (attuale->lucchetti)   /* nessuno lo lascerebbe piu': chi aspetta dormirebbe per sempre */
        panico("il compito %d e' finito tenendo un lucchetto", attuale->numero);
    attuale->stato = FINITO;
    /* non si puo' liberare la propria pila mentre ci si sta sopra:
     * lo fara' lo scheduler, quando staremo girando su un'altra */
    for (;;) cedi();
}

/* --- il compito inattivo --- */

static void ciclo_inattivo(unsigned int argomento) {
    (void)argomento;
    for (;;) __asm__ volatile("hlt");   /* dorme fino alla prossima interruzione */
}

/* --- creare un compito --- */

struct compito *compito_crea(const char *nome, void (*funzione)(unsigned int), unsigned int argomento) {
    struct compito *c = kmalloc(sizeof *c);
    unsigned char *pila = kmalloc(DIM_PILA);
    if (!c || !pila) { kfree(c); kfree(pila); return 0; }

    memset(c, 0, sizeof *c);
    copia_nome(c->nome, nome);
    c->pila = pila;

    /* LA PILA FINTA. Dall'alto (fine della pila) verso il basso:
     *
     *   argomento          \ come se qualcuno avesse fatto "call funzione(argomento)":
     *   fine_compito       / quando la funzione fa return, salta a fine_compito
     *   EFLAGS, CS, EIP    \
     *   errore, numero      |  quello che la CPU e isr_comune avrebbero salvato
     *   registri (pusha)    |  se il compito fosse stato interrotto
     *   DS                 /   <- qui punta l'ESP salvato nella scheda
     */
    unsigned int *cima = (unsigned int *)(pila + DIM_PILA);
    cima[-1] = argomento;
    cima[-2] = (unsigned int)fine_compito;

    struct registri *r = (struct registri *)((unsigned char *)&cima[-2] - sizeof(struct registri));
    memset(r, 0, sizeof *r);
    r->ds = SEL_KERNEL_DATI;
    r->eip = (unsigned int)funzione;        /* iret "tornera'" all'inizio della funzione */
    r->cs = SEL_KERNEL_CODICE;
    r->eflags = EFLAGS_INTERRUZIONI_ACCESE; /* senno' nessun tick lo fermerebbe piu' */
    c->esp = (unsigned int)r;

    unsigned int s = interruzioni_spegni();
    c->numero = prossimo_numero++;
    c->stato = PRONTO;
    in_fila(c);
    interruzioni_ripristina(s);
    return c;
}

/* --- creare un programma in ring 3 --- */

/* Dal linker script: la zona dei programmi. Gli indirizzi "inizio/fine" sono quelli
 * VIRTUALI (1 GB in su), utente_carica e' dove sta in RAM la copia originale. */
extern char utente_carica[], utente_codice_inizio[], utente_dati_inizio[],
            utente_bss_inizio[], utente_fine[];

#define PILA_PROGRAMMA_CIMA 0x80000000u   /* la pila del programma: l'ultima pagina sotto i 2 GB */

/* Copia la zona dei programmi in pagine nuove e le mette nella mappa al loro indirizzo
 * virtuale: codice e costanti in sola lettura, variabili in lettura e scrittura,
 * le variabili a zero (bss) azzerate. */
static int carica_programmi(unsigned int mappa) {
    unsigned int inizio = (unsigned int)utente_codice_inizio;
    unsigned int dati = (unsigned int)utente_dati_inizio;
    unsigned int bss = (unsigned int)utente_bss_inizio, fine = (unsigned int)utente_fine;

    for (unsigned int v = inizio; v < fine; v += PAGINA) {
        unsigned int pagina = pagina_chiedi();
        if (!pagina) return 0;
        memcpy((void *)pagina, utente_carica + (v - inizio), PAGINA);
        if (v + PAGINA > bss) {                          /* questa pagina ha variabili a zero */
            unsigned int da = v > bss ? v : bss;
            memset((void *)(pagina + (da - v)), 0, v + PAGINA - da);
        }
        unsigned int permessi = v < dati ? PAG_UTENTE : PAG_UTENTE | PAG_SCRITTURA;
        if (!pag_mappa_programma(mappa, v, pagina, permessi)) { pagina_restituisci(pagina); return 0; }
    }
    return 1;
}

struct compito *compito_crea_programma(const char *nome, void (*inizio)(void)) {
    struct compito *c = kmalloc(sizeof *c);
    unsigned char *pila = kmalloc(DIM_PILA);    /* la pila del kernel per questo programma */
    unsigned int mappa = pag_nuova_mappa();      /* la sua mappa: il kernel gia' dentro */
    unsigned int pila_programma = 0;
    if (!c || !pila || !mappa) goto memoria_finita;

    /* 1. le sue pagine: codice e variabili, copiate dalla copia originale */
    if (!carica_programmi(mappa)) goto memoria_finita;

    /* 2. la sua pila: una pagina nuova, nella sua mappa appena sotto i 2 GB.
     *    In cima un indirizzo di ritorno 0: se la funzione del programma facesse return
     *    invece di esci, salterebbe a 0 -> page fault, e il kernel chiuderebbe il compito.
     *    (La pagina si scrive dal suo indirizzo fisico: nella mappa del kernel, che e'
     *    quella che stiamo usando, il suo indirizzo virtuale non esiste.) */
    pila_programma = pagina_chiedi();
    if (!pila_programma) goto memoria_finita;
    ((unsigned int *)(pila_programma + PAGINA))[-1] = 0;
    if (!pag_mappa_programma(mappa, PILA_PROGRAMMA_CIMA - PAGINA, pila_programma,
                             PAG_UTENTE | PAG_SCRITTURA)) goto memoria_finita;
    pila_programma = 0;   /* ora e' della mappa: la restituira' pag_cancella_mappa */

    memset(c, 0, sizeof *c);
    copia_nome(c->nome, nome);
    c->pila = pila;
    c->mappa = mappa;

    /* 3. LA PILA FINTA, sulla pila del kernel. Come per un compito normale, ma "come se"
     *    fosse stato interrotto un programma in ring 3: in quel caso la CPU, oltre a
     *    EFLAGS, CS, EIP, mette anche la pila del programma (ESP e SS). E' iret a vedere
     *    che il ring di CS (3) e' diverso da quello attuale (0): allora riprende anche
     *    ESP e SS e scende in ring 3. Tutti indirizzi VIRTUALI della sua mappa.
     *
     *   SS  = dati dei programmi (riga 4 della GDT, ring 3)
     *   ESP = cima della pila del programma (sotto l'indirizzo di ritorno 0)
     *   EFLAGS, CS = codice dei programmi (riga 3, ring 3), EIP = inizio
     *   errore, numero, registri (pusha)
     *   DS  = dati dei programmi            <- qui punta l'ESP salvato nella scheda */
    unsigned int *cima = (unsigned int *)(pila + DIM_PILA);
    cima[-1] = SEL_UTENTE_DATI;
    cima[-2] = PILA_PROGRAMMA_CIMA - 4;

    struct registri *r = (struct registri *)((unsigned char *)&cima[-2] - sizeof(struct registri));
    memset(r, 0, sizeof *r);
    r->ds = SEL_UTENTE_DATI;
    r->eip = (unsigned int)inizio;
    r->cs = SEL_UTENTE_CODICE;
    r->eflags = EFLAGS_INTERRUZIONI_ACCESE;   /* il timer deve poterlo fermare */
    c->esp = (unsigned int)r;

    unsigned int s = interruzioni_spegni();
    c->numero = prossimo_numero++;
    c->stato = PRONTO;
    in_fila(c);
    interruzioni_ripristina(s);
    return c;

memoria_finita:
    if (pila_programma) pagina_restituisci(pila_programma);
    if (mappa) pag_cancella_mappa(mappa);
    kfree(pila);
    kfree(c);
    return 0;
}

void compiti_prepara(void) {
    /* Il codice che sta girando adesso (kernel_main, sulla pila di kernel_head.s)
     * diventa il compito 1. Non serve una pila finta: e' gia' in esecuzione, e il
     * suo ESP verra' salvato al primo tick come per tutti. */
    struct compito *shell = kmalloc(sizeof *shell);
    memset(shell, 0, sizeof *shell);
    copia_nome(shell->nome, "shell");
    shell->stato = IN_ESECUZIONE;

    unsigned int s = interruzioni_spegni();
    shell->numero = prossimo_numero++;
    in_fila(shell);
    attuale = shell;
    interruzioni_ripristina(s);

    inattivo = compito_crea("inattivo", ciclo_inattivo, 0);
    acceso = 1;
}

/* --- lo scheduler: gira dentro l'interruzione, a interruzioni spente --- */

unsigned int scheduler(unsigned int esp) {
    if (!acceso) return esp;

    /* 1. il compito interrotto: ci si annota dove era arrivata la sua pila */
    attuale->esp = esp;
    if (attuale->stato == IN_ESECUZIONE) attuale->stato = PRONTO;

    /* 2. un giro della fila: si sveglia chi ha dormito abbastanza e si libera chi
     *    ha finito (ma non chi sta girando adesso: siamo ancora sulla sua pila) */
    unsigned int adesso = timer_tick();
    struct compito *prec = ultimo, *c = primo;
    for (int giri = numero_compiti; giri > 0; giri--) {
        struct compito *dopo = c->prossimo;
        if (c->stato == FINITO && c != attuale) {
            prec->prossimo = dopo;
            if (c == primo) primo = dopo;
            if (c == ultimo) ultimo = prec;
            numero_compiti--;
            if (c->mappa) pag_cancella_mappa(c->mappa);   /* pagine, tabelle, directory */
            kfree(c->pila);
            kfree(c);
        } else {
            if (c->stato == DORME && adesso >= c->sveglia) c->stato = PRONTO;
            prec = c;
        }
        c = dopo;
    }

    /* 3. chi tocca: il primo PRONTO dopo quello attuale (l'attuale per ultimo).
     *    Il compito inattivo solo se non c'e' nessun altro. */
    struct compito *scelto = 0;
    c = attuale->prossimo;
    for (int i = 0; i < numero_compiti; i++, c = c->prossimo)
        if (c->stato == PRONTO && c != inattivo) { scelto = c; break; }
    if (!scelto) scelto = inattivo;

    /* 4. la TSS: se mentre gira il compito scelto arrivasse un'interruzione dal
     *    ring 3, la CPU salverebbe i registri sulla cima della SUA pila del kernel.
     *    Per i compiti del kernel (ring 0) la CPU non la legge, ma la si aggiorna lo
     *    stesso. La shell usa la pila dell'avvio (kernel_head.s). */
    tss_pila_kernel(scelto->pila ? (unsigned int)scelto->pila + DIM_PILA
                                 : (unsigned int)stack_top);

    /* 5. la mappa: da qui la CPU traduce gli indirizzi con quella di chi parte
     *    (0 = la mappa del kernel, per i compiti del kernel). La pila del kernel su
     *    cui stiamo lavorando c'e' in tutte le mappe, quindi si puo' cambiare adesso. */
    pag_usa_mappa(scelto->mappa);

    /* 6. si restituisce il SUO esp: isr_comune ripeschera' i suoi registri */
    scelto->stato = IN_ESECUZIONE;
    attuale = scelto;
    return scelto->esp;
}

int scheduler_sta_riposando(void) { return acceso && attuale == inattivo; }

/* --- cedere, dormire, aspettare --- */

void cedi(void) {
    /* un'interruzione software: passa da isr_comune come un tick, quindi i registri
     * vengono salvati e lo scheduler sceglie un altro compito */
    __asm__ volatile("int %0" : : "i"(INT_CEDI) : "memory");
}

void dormi(unsigned int ms) {
    unsigned int s = interruzioni_spegni();
    unsigned int tick = (ms + 9) / 10;            /* 1 tick = 10 ms, arrotondato in su */
    attuale->sveglia = timer_tick() + (tick ? tick : 1);
    attuale->stato = DORME;
    cedi();                                        /* si riparte da qui al risveglio */
    interruzioni_ripristina(s);
}

void compito_aspetta(void) {
    attuale->stato = ATTESA;
    cedi();
}

void compito_sveglia(struct compito *c) {
    if (c && c->stato == ATTESA) c->stato = PRONTO;
}

struct compito *compito_attuale(void) { return attuale; }

int compito_ferma(int numero) {
    int risultato = -1;
    unsigned int s = interruzioni_spegni();
    struct compito *c = primo;
    for (int i = 0; i < numero_compiti; i++, c = c->prossimo) {
        if (c->numero != numero) continue;
        /* la shell, l'inattivo e chi sta girando non si fermano */
        if (c == attuale || c == inattivo || !c->pila) break;
        /* chi tiene un lucchetto lascerebbe la risorsa a meta' e il lucchetto preso
         * per sempre; chi e' in fila lascerebbe nella fila una scheda liberata */
        if (c->lucchetti || c->aspetta) { risultato = -2; break; }
        c->stato = FINITO;         /* lo scheduler liberera' scheda e pila */
        risultato = 0;
        break;
    }
    interruzioni_ripristina(s);
    return risultato;
}

void compiti_elenca(void) {
    static const char *nomi_stato[] = { "pronto", "in esecuzione", "dorme", "attesa", "finito" };
    kprintf("  N.  nome          stato\n");
    unsigned int s = interruzioni_spegni();
    struct compito *c = primo;
    for (int i = 0; i < numero_compiti; i++, c = c->prossimo) {
        kprintf("  %2d  %s", c->numero, c->nome);
        int l = 0;
        while (c->nome[l]) l++;
        for (; l < 14; l++) kprintf(" ");
        kprintf("%s\n", nomi_stato[c->stato]);
    }
    interruzioni_ripristina(s);
}
