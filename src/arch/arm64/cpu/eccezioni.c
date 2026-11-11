/* Le eccezioni su ARM64: il C chiamato da vettori.s (l'idt.c di x86).
 *
 * Il numero della casella dice che cosa e' successo (vedi vettori.s): 0-3 dal kernel con
 * SP_EL0 (non succede), 4-7 dal kernel, 8-11 da un programma, 12-15 da un programma a 32
 * bit. Dentro ogni gruppo: +0 sincrona, +1 IRQ, +2 FIQ, +3 SError.
 *
 * Restituisce i registri DA RIPRENDERE: quelli di chi e' stato interrotto, o quelli di
 * un altro compito scelto dallo scheduler (come su x86, dove si restituisce un esp).
 *
 * LE SINCRONE. Il motivo sta in ESR_EL1 ("Exception Syndrome Register"): i bit 31-26 sono
 * la CLASSE (0x15 = svc, 0x24/0x25 = errore di memoria dei dati, da un programma / dal
 * kernel, 0x20/0x21 = errore eseguendo codice...), i bit sotto i dettagli. Per gli errori
 * di memoria FAR_EL1 dice l'indirizzo (come cr2 su x86), e i bit 5-0 di ESR il tipo:
 * 0b0001xx pagina assente ("translation fault"), 0b0011xx permesso negato. */
#include "kernel.h"
#include "eccezioni.h"
#include "gic.h"
#include "compiti.h"
#include "dinamica.h"

#define CLASSE_SVC              0x15
#define CLASSE_CODICE_PROGRAMMA 0x20
#define CLASSE_CODICE_KERNEL    0x21
#define CLASSE_DATI_PROGRAMMA   0x24
#define CLASSE_DATI_KERNEL      0x25

indirizzo_t chiamata_di_sistema(struct registri *r);   /* compiti/chiamate.c */

void vettori_prepara(void) {
    extern char tabella_vettori[];
    __asm__ volatile("msr vbar_el1, %0; isb" : : "r"(tabella_vettori));
}

/* --- le prove (vedi eccezioni.h) --- */
static volatile int prova_attesa, prova_avvenuta;
static unsigned long prova_indirizzo, prova_classe;

void prova_errore_atteso(void) { prova_attesa = 1; prova_avvenuta = 0; }

int prova_esito(unsigned long *indirizzo, unsigned long *classe) {
    prova_attesa = 0;
    *indirizzo = prova_indirizzo;
    *classe = prova_classe;
    return prova_avvenuta;
}

static const char *const nomi_tipo[] = { "sincrona", "IRQ", "FIQ", "SError" };
static const char *const nomi_da_dove[] = {
    "dal kernel (SP_EL0)", "dal kernel", "da un programma", "da un programma a 32 bit"
};

static unsigned long leggi_esr(void) { unsigned long v; __asm__ volatile("mrs %0, esr_el1" : "=r"(v)); return v; }
static unsigned long leggi_far(void) { unsigned long v; __asm__ volatile("mrs %0, far_el1" : "=r"(v)); return v; }

static int e_errore_di_memoria(unsigned long classe) {
    return classe == CLASSE_DATI_PROGRAMMA || classe == CLASSE_DATI_KERNEL ||
           classe == CLASSE_CODICE_PROGRAMMA || classe == CLASSE_CODICE_KERNEL;
}

/* Un errore: lo si racconta. Se l'ha fatto un compito con una pila sua, si chiude solo
 * lui e si va avanti (come su x86: "Segmentation fault"); se l'ha fatto il kernel, panico. */
static struct registri *errore(struct registri *r, unsigned long tipo) {
    unsigned long esr = leggi_esr(), classe = esr >> 26;
    console_colore(ROSSO);
    kprintf("\nECCEZIONE %s %s: classe %x (ESR %x)\n", nomi_tipo[tipo % 4],
            nomi_da_dove[tipo / 4], (unsigned int)classe, (unsigned int)esr);
    console_colore(GRIGIO);
    if (e_errore_di_memoria(classe)) {
        unsigned long far = leggi_far();
        kprintf("  all'indirizzo %p: %s\n", (void *)far,
                ((esr & 0x3C) >> 2) == 1 ? "pagina assente" :
                ((esr & 0x3C) >> 2) == 3 ? "permesso negato" : "errore");
        if (far < 4096) kprintf("  (e' la pagina 0: quasi certamente un puntatore nullo)\n");
    }
    kprintf("  dove : ELR=%p  SPSR=%x\n", (void *)r->elr, (unsigned int)r->spsr);

    struct compito *c = compito_attuale();
    if (c && c->pila && c->lucchetti)
        panico("il compito %d ha sbagliato mentre teneva un lucchetto", c->numero);
    if (c && c->pila) {
        console_colore(GIALLO);
        kprintf("  Il compito %d (%s) e' stato chiuso. Il sistema continua.\n", c->numero, c->nome);
        console_colore(GRIGIO);
        c->stato = FINITO;
        return (struct registri *)scheduler((indirizzo_t)r);
    }
    panico("sistema fermato dopo l'eccezione");
}

static struct registri *sincrona(struct registri *r, unsigned long tipo) {
    unsigned long esr = leggi_esr(), classe = esr >> 26;

    /* svc: da un programma e' una chiamata di sistema; dal kernel, cedi_turno */
    if (classe == CLASSE_SVC) {
        if (tipo / 4 == 2) return (struct registri *)chiamata_di_sistema(r);
        if ((esr & 0xFFFF) == SVC_CEDI) return (struct registri *)scheduler((indirizzo_t)r);
    }

    /* Un errore di memoria su una pagina che non c'e' potrebbe non essere un errore: se
     * l'indirizzo e' nella zona dinamica, la pagina era solo promessa. La si da' adesso e
     * si torna all'istruzione che aveva fallito (ELR punta ancora a lei). Vale anche per
     * il kernel che tocca la memoria di un programma durante una chiamata di sistema. */
    if ((classe == CLASSE_DATI_PROGRAMMA || classe == CLASSE_DATI_KERNEL) &&
        ((esr & 0x3C) >> 2) == 1 && memoria_promessa(leggi_far()))
        return r;

    /* un errore atteso da una prova: lo si annota e si salta l'istruzione (4 byte) */
    if (prova_attesa && classe == CLASSE_DATI_KERNEL) {
        prova_indirizzo = leggi_far();
        prova_classe = esr;
        prova_avvenuta = 1;
        prova_attesa = 0;
        r->elr += 4;
        return r;
    }
    return errore(r, tipo);
}

struct registri *eccezione(struct registri *r, unsigned long tipo) {
    switch (tipo % 4) {
    case 0:
        return sincrona(r, tipo);
    case 1: {
        /* Un'interruzione: il centralino sa chi e'. Il timer decide i turni; per le altre
         * si cambia compito solo se stava girando l'inattivo (come su x86). */
        unsigned int linea = gic_interruzione();
        if (linea == GIC_LINEA_TIMER || scheduler_sta_riposando())
            return (struct registri *)scheduler((indirizzo_t)r);
        return r;
    }
    default:
        return errore(r, tipo);
    }
}
