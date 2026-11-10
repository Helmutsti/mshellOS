/* I compiti e lo scheduler.
 *
 * Un COMPITO e' un programma in ring 3 (o l'inattivo, il kernel stesso) con una
 * pila sua e una scheda. I compiti stanno in una fila circolare; lo scheduler, a ogni tick del timer (e a ogni cedi), passa al
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
 * i registri "come se" lo fosse stato, con EIP = inizio del programma. */
#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "kmalloc.h"
#include "timer.h"
#include "pagine.h"
#include "paginazione.h"
#include "mfs.h"
#include "elf.h"
#include "chiamate.h"
#include "compiti.h"
#include "descrittori.h"
#include "fogli.h"

#define DIM_PILA 8192

extern char stack_top[];   /* la cima della pila dell'avvio (kernel_head.s): e' dell'inattivo */
#define EFLAGS_INTERRUZIONI_ACCESE 0x202   /* bit IF acceso (+ il bit 1, sempre a 1) */

static struct compito *attuale;     /* chi sta girando */
static struct compito *primo, *ultimo;
static struct compito *inattivo;    /* il compito che fa hlt quando nessuno ha da fare */
static int numero_compiti;
static int prossimo_numero = 0;    /* 0 l'inattivo, 1 init: come in Linux */
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

/* --- creare un programma in ring 3, da un file ELF sul disco --- */

#define MAX_PROGRAMMA (1024 * 1024)   /* il programma piu' grande che si carica: 1 MB */

/* Legge il file ELF 'percorso' dal disco e mette i suoi pezzi nella mappa, ognuno in
 * pagine nuove al suo indirizzo virtuale. Restituisce l'indirizzo da cui il programma
 * parte, oppure 0 e in *errore il perche'.
 *
 * NON SI CREDE AL FILE: lo ha scritto chiunque. Ogni numero che contiene (dove sta un
 * pezzo, quanto e' grande, a che indirizzo va) si controlla prima di usarlo: un pezzo
 * fuori dal file, o fuori dalla zona dei programmi, e il file viene rifiutato. */
static unsigned int carica_elf(unsigned int mappa, const char *percorso, const char **errore,
                               unsigned int *fine) {
    unsigned int inizio = 0;
    unsigned char *file = kmalloc(MAX_PROGRAMMA);
    if (!file) { *errore = "memoria finita"; return 0; }

    int byte = mfs_leggi(percorso, 0, file, MAX_PROGRAMMA);
    if (byte < 0) { *errore = mfs_messaggio(byte); goto fine; }

    /* 1. l'intestazione: e' davvero un eseguibile ELF a 32 bit per questa CPU? */
    struct elf_intestazione *e = (struct elf_intestazione *)file;
    *errore = "non e' un eseguibile ELF a 32 bit per x86";
    if ((unsigned int)byte < sizeof *e || memcmp(e->magico, "\x7F" "ELF", 4) || e->classe != 1 ||
        e->tipo != ELF_ESEGUIBILE || e->macchina != ELF_386 ||
        e->dim_pezzo != sizeof(struct elf_pezzo) ||
        e->tabella_pezzi + e->numero_pezzi * sizeof(struct elf_pezzo) > (unsigned int)byte)
        goto fine;

    /* 2. i pezzi da caricare, in ordine di indirizzo, ognuno a pagina nuova */
    struct elf_pezzo *pezzi = (struct elf_pezzo *)(file + e->tabella_pezzi);
    unsigned int fine_precedente = PROGRAMMI_INIZIO;
    for (int i = 0; i < e->numero_pezzi; i++) {
        struct elf_pezzo *p = &pezzi[i];
        if (p->tipo != PT_LOAD) continue;
        unsigned int fine_pezzo = p->virtuale + p->byte_memoria;

        *errore = "un pezzo del file e' fuori posto";
        if (p->virtuale % PAGINA || p->virtuale < fine_precedente ||
            fine_pezzo < p->virtuale || fine_pezzo > FOGLI_INIZIO ||
            p->byte_file > p->byte_memoria || p->nel_file + p->byte_file > (unsigned int)byte ||
            p->nel_file + p->byte_file < p->nel_file)
            goto fine;

        unsigned int permessi = PAG_UTENTE | (p->permessi & PF_W ? PAG_SCRITTURA : 0);
        for (unsigned int v = p->virtuale; v < fine_pezzo; v += PAGINA) {
            unsigned int pagina = pagina_chiedi();
            if (!pagina) { *errore = "memoria finita"; goto fine; }
            memset((void *)pagina, 0, PAGINA);   /* cio' che non sta nel file resta a zero */
            unsigned int dentro = v - p->virtuale;
            if (dentro < p->byte_file) {
                unsigned int n = p->byte_file - dentro < PAGINA ? p->byte_file - dentro : PAGINA;
                memcpy((void *)pagina, file + p->nel_file + dentro, n);
            }
            if (!pag_mappa_programma(mappa, v, pagina, permessi)) {
                pagina_restituisci(pagina);
                *errore = "memoria finita";
                goto fine;
            }
        }
        fine_precedente = (fine_pezzo + PAGINA - 1) & ~(PAGINA - 1);
    }

    /* 3. da dove parte: deve stare dentro quello che si e' appena caricato */
    *errore = "l'indirizzo di partenza e' fuori dal programma";
    if (e->inizio < PROGRAMMI_INIZIO || e->inizio >= fine_precedente) goto fine;
    inizio = e->inizio;
    *fine = fine_precedente;   /* la prima pagina dopo il programma: li' parte la zona dinamica */
    *errore = 0;

fine:
    kfree(file);
    return inizio;
}

/* Il nome di un compito: l'ultimo pezzo del percorso ("/init" -> "init"). */
static const char *nome_da_percorso(const char *percorso) {
    const char *nome = percorso;
    for (const char *c = percorso; *c; c++)
        if (*c == '/') nome = c + 1;
    return nome;
}

struct compito *compito_crea_programma(const char *percorso, unsigned int uid, const char **errore) {
    struct compito *c = kmalloc(sizeof *c);
    unsigned char *pila = kmalloc(DIM_PILA);    /* la pila del kernel per questo programma */
    unsigned int mappa = pag_nuova_mappa();      /* la sua mappa: il kernel gia' dentro */
    unsigned int pila_programma = 0;
    *errore = "memoria finita";
    if (!c || !pila || !mappa) goto non_va;

    /* 1. le sue pagine: i pezzi del file ELF, ognuno al suo indirizzo virtuale */
    unsigned int fine;
    unsigned int inizio = carica_elf(mappa, percorso, errore, &fine);
    if (!inizio) goto non_va;

    /* 2. la sua pila: una pagina nuova, nella sua mappa appena sotto i 2 GB.
     *    In cima un indirizzo di ritorno 0: se il programma tornasse da _start invece di
     *    chiamare esci, salterebbe a 0 -> page fault, e il kernel chiuderebbe il compito.
     *    (La pagina si scrive dal suo indirizzo fisico: nella mappa del kernel, che e'
     *    quella che stiamo usando, il suo indirizzo virtuale non esiste.) */
    *errore = "memoria finita";
    pila_programma = pagina_chiedi();
    if (!pila_programma) goto non_va;
    memset((void *)pila_programma, 0, PAGINA);
    if (!pag_mappa_programma(mappa, PILA_PROGRAMMA_CIMA - PAGINA, pila_programma,
                             PAG_UTENTE | PAG_SCRITTURA)) goto non_va;
    pila_programma = 0;   /* ora e' della mappa: la restituira' pag_cancella_mappa */

    memset(c, 0, sizeof *c);
    copia_nome(c->nome, nome_da_percorso(percorso));
    c->pila = pila;
    c->mappa = mappa;
    c->dati_inizio = c->rottura = fine;   /* la zona dinamica, vuota */
    c->uid = uid;

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
    r->eip = inizio;
    r->cs = SEL_UTENTE_CODICE;
    r->eflags = EFLAGS_INTERRUZIONI_ACCESE;   /* il timer deve poterlo fermare */
    c->esp = (unsigned int)r;

    unsigned int s = interruzioni_spegni();
    c->numero = prossimo_numero++;
    c->stato = PRONTO;
    in_fila(c);
    interruzioni_ripristina(s);
    *errore = 0;
    return c;

non_va:
    if (pila_programma) pagina_restituisci(pila_programma);
    if (mappa) pag_cancella_mappa(mappa);
    kfree(pila);
    kfree(c);
    return 0;
}

void compiti_prepara(void) {
    /* Il codice che sta girando adesso (kernel_main, sulla pila di kernel_head.s)
     * diventa il compito 0, l'INATTIVO. Come in Linux: finito l'avvio il kernel non ha
     * altro da fare, e resta li' a fermare la CPU (hlt) quando nessuno ha lavoro.
     * Non serve una pila finta: e' gia' in esecuzione, e il suo ESP verra' salvato al
     * primo tick come per tutti. */
    struct compito *c = kmalloc(sizeof *c);
    memset(c, 0, sizeof *c);
    copia_nome(c->nome, "inattivo");
    c->stato = IN_ESECUZIONE;

    unsigned int s = interruzioni_spegni();
    c->numero = prossimo_numero++;
    in_fila(c);
    attuale = inattivo = c;
    acceso = 1;
    interruzioni_ripristina(s);
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
        /* segnato da chiudere e fermo in ring 3 (i registri salvati dicono che e' stato
         * interrotto mentre girava il suo codice): si chiude adesso */
        if (c->da_terminare && c->stato == PRONTO && c != attuale &&
            (((struct registri *)c->esp)->cs & 3) == 3)
            c->stato = FINITO;
        if (c->stato == FINITO && c != attuale) {
            prec->prossimo = dopo;
            if (c == primo) primo = dopo;
            if (c == ultimo) ultimo = prec;
            numero_compiti--;
            console_riprendi(c);   /* se lo schermo era suo, torna alla console */
            descrittori_chiudi_tutti(c);   /* i suoi socket: chi era collegato lo sapra' */
            fogli_lascia_tutti(c);         /* i fogli nella sua memoria */
            if (c->mappa) pag_cancella_mappa(c->mappa);   /* pagine, tabelle, directory */
            kfree(c->pila);
            kfree(c);
        } else {
            if (c->stato == DORME && adesso >= c->sveglia) c->stato = PRONTO;
            if (c->stato == ATTESA && c->scadenza && adesso >= c->scadenza) c->stato = PRONTO;
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
     *    stesso. L'inattivo usa la pila dell'avvio (kernel_head.s). */
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
    compito_fine_se_terminato();
    interruzioni_ripristina(s);
}

void compito_aspetta(void) { compito_aspetta_fino(0); }

void compito_aspetta_fino(unsigned int scadenza) {
    attuale->scadenza = scadenza;
    attuale->stato = ATTESA;
    cedi();
    attuale->scadenza = 0;
}

void compito_sveglia(struct compito *c) {
    if (c && c->stato == ATTESA) c->stato = PRONTO;
}

void compito_termina(struct compito *c) {
    unsigned int s = interruzioni_spegni();
    c->da_terminare = 1;
    if (c->stato == DORME || (c->stato == ATTESA && !c->aspetta)) c->stato = PRONTO;
    interruzioni_ripristina(s);
}

void compito_fine_se_terminato(void) {
    if (!attuale->da_terminare) return;
    attuale->stato = FINITO;
    cedi();                                  /* lo scheduler non lo fara' piu' ripartire */
    panico("un compito chiuso e' ripartito");
}

struct compito *compito_attuale(void) { return attuale; }
