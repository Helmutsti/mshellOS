/* I compiti e lo scheduler.
 *
 * Un COMPITO e' un programma in ring 3 (o l'inattivo, il kernel stesso) con una
 * pila sua e una scheda. I compiti stanno in una fila circolare; lo scheduler, a ogni tick del timer (e a ogni cedi), passa al
 * prossimo compito PRONTO: e' il "giro" (round robin).
 *
 * IL TRUCCO DEL CAMBIO DI CONTESTO
 * Quando arriva un'interruzione, il kernel (su x86: isr_comune) salva TUTTI i registri
 * del compito interrotto sulla SUA pila. Lo scheduler si annota quell'ESP nella scheda del
 * compito, e restituisce l'ESP di un altro compito: isr_comune ripesca i registri
 * da li' e con iret riparte... dentro l'altro compito, dove si era fermato.
 *
 * IL PRIMO AVVIO
 * Un compito nuovo non e' mai stato interrotto: sulla sua pila prepariamo a mano
 * i registri "come se" lo fosse stato, con EIP = inizio del programma. */
#include "kernel.h"
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
#include "processo.h"

#define DIM_PILA 8192


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
static indirizzo_t carica_elf(indirizzo_t mappa, const char *percorso, const char **errore,
                              indirizzo_t *fine) {
    indirizzo_t inizio = 0;
    unsigned char *file = kmalloc(MAX_PROGRAMMA);
    if (!file) { *errore = "memoria finita"; return 0; }

    int byte = mfs_leggi(percorso, 0, file, MAX_PROGRAMMA);
    if (byte < 0) { *errore = mfs_messaggio(byte); goto fine; }

    /* 1. l'intestazione: e' davvero un eseguibile ELF per questa CPU? */
    struct elf_intestazione *e = (struct elf_intestazione *)file;
    *errore = "non e' " ELF_DESCRIZIONE;
    if ((unsigned int)byte < sizeof *e || memcmp(e->magico, "\x7F" "ELF", 4) || e->classe != ELF_CLASSE ||
        e->tipo != ELF_ESEGUIBILE || e->macchina != ELF_MACCHINA ||
        e->dim_pezzo != sizeof(struct elf_pezzo) ||
        e->tabella_pezzi + e->numero_pezzi * sizeof(struct elf_pezzo) > (unsigned int)byte)
        goto fine;

    /* 2. i pezzi da caricare, in ordine di indirizzo, ognuno a pagina nuova */
    struct elf_pezzo *pezzi = (struct elf_pezzo *)(file + e->tabella_pezzi);
    indirizzo_t fine_precedente = PROGRAMMI_INIZIO;
    for (int i = 0; i < e->numero_pezzi; i++) {
        struct elf_pezzo *p = &pezzi[i];
        if (p->tipo != PT_LOAD) continue;
        indirizzo_t fine_pezzo = (indirizzo_t)p->virtuale + p->byte_memoria;

        *errore = "un pezzo del file e' fuori posto";
        if (p->virtuale % PAGINA || p->virtuale < fine_precedente ||
            fine_pezzo < p->virtuale || fine_pezzo > FOGLI_INIZIO ||
            p->byte_file > p->byte_memoria || p->nel_file + p->byte_file > (unsigned int)byte ||
            p->nel_file + p->byte_file < p->nel_file)
            goto fine;

        unsigned int permessi = PAG_UTENTE | (p->permessi & PF_W ? PAG_SCRITTURA : 0);
        for (indirizzo_t v = p->virtuale; v < fine_pezzo; v += PAGINA) {
            indirizzo_t pagina = pagina_chiedi();
            if (!pagina) { *errore = "memoria finita"; goto fine; }
            memset((void *)pagina, 0, PAGINA);   /* cio' che non sta nel file resta a zero */
            unsigned int dentro = (unsigned int)(v - p->virtuale);   /* dentro il pezzo: piccolo */
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

/* Il nome di un compito: l'ultimo pezzo del percorso ("/Sistema/init" -> "init"). */
static const char *nome_da_percorso(const char *percorso) {
    const char *nome = percorso;
    for (const char *c = percorso; *c; c++)
        if (*c == '/') nome = c + 1;
    return nome;
}

struct compito *compito_crea_programma(const char *percorso, unsigned int uid, const char **errore) {
    struct compito *c = kmalloc(sizeof *c);
    unsigned char *pila = kmalloc(DIM_PILA);    /* la pila del kernel per questo programma */
    indirizzo_t mappa = pag_nuova_mappa();       /* la sua mappa: il kernel gia' dentro */
    indirizzo_t pila_programma = 0;
    *errore = "memoria finita";
    if (!c || !pila || !mappa) goto non_va;

    /* 1. le sue pagine: i pezzi del file ELF, ognuno al suo indirizzo virtuale */
    indirizzo_t fine;
    indirizzo_t inizio = carica_elf(mappa, percorso, errore, &fine);
    if (!inizio) goto non_va;

    /* 2. la sua pila: una pagina nuova, nella sua mappa in cima alla zona dei programmi.
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
    for (int i = 0; i < MAX_PERCORSO - 1 && percorso[i]; i++) c->percorso[i] = percorso[i];
    c->pila = pila;
    c->mappa = mappa;
    c->dati_inizio = c->rottura = fine;   /* la zona dinamica, vuota */
    c->uid = uid;

    /* 3. LA PILA FINTA, sulla pila del kernel: i registri "come se" fosse stato
     *    interrotto mentre girava il programma, all'inizio. Com'e' fatta dipende dalla
     *    CPU (arch: contesto.c). */
    c->sp = programma_prepara_pila((indirizzo_t)pila + DIM_PILA, inizio, PILA_PROGRAMMA_CIMA);

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

/* --- lo scheduler: gira dentro l'interruzione, a interruzioni spente ---
 *
 * A giro (round robin): ogni compito pronto ha un tick, poi tocca al successivo; tutti
 * lo stesso tempo, nessuno davanti agli altri.
 *
 * DA IMPLEMENTARE: le priorita'. Chi deve rispondere subito (il compositore, chi viene
 * svegliato da un evento) aspetta il suo turno come chi fa un lavoro lungo: quando
 * un'applicazione lavora molto, il compositore perde fotogrammi. I sistemi veri fanno
 * passare prima chi si e' appena svegliato (Linux), o danno piu' priorita' a chi
 * disegna lo schermo (macOS) o alla finestra in primo piano (Windows). */

indirizzo_t scheduler(indirizzo_t sp) {
    if (!acceso) return sp;

    /* 1. il compito interrotto: ci si annota dove era arrivata la sua pila */
    attuale->sp = sp;
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
            era_un_programma((struct registri *)c->sp))
            c->stato = FINITO;
        if (c->stato == FINITO && c != attuale) {
            prec->prossimo = dopo;
            if (c == primo) primo = dopo;
            if (c == ultimo) ultimo = prec;
            numero_compiti--;
            descrittori_chiudi_tutti(c);   /* socket, schermo...: chi li usava lo sapra' */
            fogli_lascia_tutti(c);         /* i fogli nella sua memoria */
            processo_finito(c->numero);    /* chi lo aspettava (init) si sveglia */
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
    pila_kernel_imposta(scelto->pila ? (indirizzo_t)scelto->pila + DIM_PILA : pila_avvio_cima());

    /* 5. la mappa: da qui la CPU traduce gli indirizzi con quella di chi parte
     *    (0 = la mappa del kernel, per i compiti del kernel). La pila del kernel su
     *    cui stiamo lavorando c'e' in tutte le mappe, quindi si puo' cambiare adesso. */
    pag_usa_mappa(scelto->mappa);

    /* 6. si restituisce la SUA pila (sp): da li' si ripescano i suoi registri */
    scelto->stato = IN_ESECUZIONE;
    attuale = scelto;
    return scelto->sp;
}

int scheduler_sta_riposando(void) { return acceso && attuale == inattivo; }

/* --- cedere, dormire, aspettare --- */

void cedi(void) {
    /* i registri si salvano e lo scheduler sceglie un altro compito (arch.h) */
    cedi_turno();
}

void dormi(unsigned int ms) {
    unsigned int s = interruzioni_spegni();
    unsigned int tick = ms_in_tick(ms);
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

struct compito *compito_numero(int numero) {
    struct compito *c = primo;
    for (int i = 0; i < numero_compiti; i++, c = c->prossimo)
        if (c->numero == numero) return c;
    return 0;
}

int compiti_elenco(struct info_processo *v, int max) {
    unsigned int s = interruzioni_spegni();
    struct compito *c = primo;
    int n = 0;
    for (int i = 0; i < numero_compiti && n < max; i++, c = c->prossimo) {
        struct info_processo *p = &v[n++];
        memset(p, 0, sizeof *p);
        p->numero = c->numero;
        p->uid = c->uid;
        p->stato = c->stato;   /* l'enum ha gli stessi numeri di STATO_... */
        /* le sue pagine, piu' la sua pila del kernel */
        p->memoria = c->mappa ? pag_conta_programma(c->mappa) * PAGINA + DIM_PILA : 0;
        copia_nome(p->nome, c->nome);
    }
    interruzioni_ripristina(s);
    return n;
}

int compito_esiste(int numero) {
    struct compito *c = primo;
    for (int i = 0; i < numero_compiti; i++, c = c->prossimo)
        if (c->numero == numero) return 1;
    return 0;
}

int compito_prossimo_numero(void) { return prossimo_numero; }
