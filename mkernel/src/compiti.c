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
#include "compiti.h"

#define DIM_PILA 8192
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

    /* 4. si restituisce il SUO esp: isr_comune ripeschera' i suoi registri */
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
        if (c != attuale && c != inattivo && c->pila) {
            c->stato = FINITO;     /* lo scheduler liberera' scheda e pila */
            risultato = 0;
        }
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
