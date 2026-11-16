/* init: il primo programma del sistema, il processo numero 1 di Linux.
 *
 * Lo fa partire avvio (utente/avvio/avvio.c), il programma dell'archivio di avvio, dopo
 * aver montato il disco: init prende il suo posto, con lo stesso numero 1, come sistema
 * (uid 0). E' lui a far nascere i servizi del sistema: eventi (che legge tastiera e
 * mouse), il compositore, il launcher (che fa partire le applicazioni), la scrivania e
 * network (che accende la rete e la tiene accesa: vedi network.c).
 * Le applicazioni non le fa partire lui: le apre il launcher, quando la scrivania lo
 * chiede.
 *
 * INIT TIENE LE RISORSE DEI SERVIZI, e gliele presta. E' lui ad aprire lo schermo, a
 * preparare il tubo degli eventi (un capo al servizio eventi, che ci scrive, e l'altro al
 * compositore, che ci legge) e a mettersi in ascolto con i nomi "compositore" e
 * "launcher"; poi fa partire i servizi passandogli queste cose gia' aperte (avvia_con),
 * come launchd sul Mac e systemd su Linux. Init non le usa: le tiene soltanto in mano.
 *
 * E LI SORVEGLIA. Se un servizio cade, init se ne accorge (il descrittore del processo
 * diventa pronto) e lo fa ripartire; il compositore, con le stesse risorse. Siccome init non
 * le ha mai lasciate, nel frattempo nessun altro ha potuto prenderle: lo schermo non
 * torna alla console, il nome resta occupato, e le applicazioni che si collegano proprio
 * in quel momento aspettano in fila, invece di trovare la porta chiusa.
 *
 * E CUSTODISCE LE PORTE DELLE APP (come il "file descriptor store" di systemd). Catalog, launcher
 * e network tengono il capo "servizio" della porta di ogni app (SISTEMA.md, sezione 3): se
 * cadono, quei capi si chiudono e le app restano con una porta morta. Allora ognuno di loro ha
 * con init un canale (una coppia di socket: init tiene un capo, e presta l'altro al servizio) su
 * cui, a ogni porta nuova, manda una copia del capo (request=keep, con programma e permessi,
 * servizi.h). Init le tiene in una tabella per servizio, SENZA MAI LEGGERE dalle porte: le guarda
 * solo per sapere quando l'app dall'altra parte le ha chiuse (POLLRDHUP), e allora le butta. Ogni
 * volta che fa partire uno di quei servizi (la prima e dopo una caduta) gli riconsegna le porte
 * sullo stesso canale (request=restore, e alla fine restored) PRIMA di farlo partire: il servizio
 * le ritrova all'avvio, e le app non si accorgono di niente.
 * Il canale e' una coppia: i messaggi che il servizio non ha fatto in tempo a leggere prima di
 * cadere restano nel suo capo, che init tiene (presta una copia): prima di riconsegnare init li
 * butta (chiudendo i descrittori allegati), cosi' le porte non arrivano due volte. E legge anche
 * i keep che il servizio aveva mandato e init non ancora letto, cosi' non se ne perde nessuno. */
#define _GNU_SOURCE   /* POLLRDHUP */
#include "corekit/corekit.h"
#include "gkit/sistema/protocollo.h"   /* NOME_COMPOSITORE */
#include "gkit/sistema/schermo.h"      /* apri_schermo */
#include "corekit/sistema/launcher.h"
#include "corekit/sistema/privilegi.h"    /* avvia_come_con_descrittori: catalog come la persona */
#include "corekit/catalog.h"              /* NOME_CATALOG */
#include "corekit/sistema/servizi.h"      /* le porte in custodia: keep, restore, chi_chiede */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

/* I SERVIZI: i programmi del sistema che init fa partire e tiene in vita. Eventi riceve
 * in prestito il capo del tubo su cui scrive; il compositore lo schermo, l'altro capo del
 * tubo e il suo nome; il launcher il suo nome (e il canale del custode, come catalog); la scrivania niente (si collega al
 * compositore come le applicazioni); network solo il canale del custode (la configurazione se la legge da
 * solo, e gira come sistema perche' Linux gli lasci cambiare le schede di rete); catalog il nome e il canale
 * del custode, e gira come la persona (uid 1000: tocca solo le sue preferenze). L'ordine
 * conta: prima eventi e il compositore. */
#define MAX_PORTE_CUSTODITE 32   /* per servizio: come MAX_PORTE dei servizi */
struct servizio {
    const char *percorso;
    int *risorse;            /* i descrittori da prestargli, ai posti 0, 1, 2... */
    int quante;
    int processo;            /* il descrittore del suo processo: pronto quando finisce */
    const char *dispositivo; /* se non e' 0, il servizio parte solo se questo si apre */
    int non_qui;             /* 1: il dispositivo manca, su questo computer non parte */
    unsigned int uid;        /* chi e': 0 il sistema (quasi tutti), UID_APPLICAZIONI la persona */
    int custodisce;          /* 1: init custodisce le porte delle app di questo servizio */
    int custode;             /* il capo del canale che tiene init (resta lo stesso per sempre) */
    int custode_servizio;    /* l'altro capo, quello prestato al servizio (in ultimo tra le risorse) */
    struct {                 /* le porte in custodia: -1 il posto e' libero */
        int d;
        struct chi_chiede chi;
    } porte[MAX_PORTE_CUSTODITE];
};

static int risorse_eventi[1];
static int risorse_compositore[3];
static int risorse_launcher[2];   /* il nome, poi il canale del custode */
static int risorse_catalog[2];    /* il nome, poi il canale del custode */
static int risorse_network[1];    /* il canale del custode */
static struct servizio servizi[] = {
    /* le impostazioni della persona: prima delle app e della scrivania, che le chiedono */
    { "/system/services/catalog/catalog", risorse_catalog, 2, -1, 0, 0, UID_APPLICAZIONI, 1 },
    { "/system/services/eventi/eventi", risorse_eventi, 1, -1 },
    { "/system/services/compositore/compositore", risorse_compositore, 3, -1 },
    { "/system/services/launcher/launcher", risorse_launcher, 2, -1, 0, 0, UID_SISTEMA, 1 },
    { "/system/services/scrivania/scrivania", 0, 0, -1 },
    /* la rete. Se la sua scheda non c'e', non e' init a saperlo (la scheda e' scritta
     * nella configurazione della rete): il servizio lo dice e aspetta, senza finire */
    { "/system/services/network/network", risorse_network, 1, -1, 0, 0, UID_SISTEMA, 1 },
    /* la shell di prova sulla seriale (/dev/ttyS0), come sistema: provvisoria, da rivedere.
     * Parte solo se la seriale c'e' (come la getty sulla seriale di Linux): nella sandbox
     * Docker non c'e', e la shell ripartirebbe ogni 5 secondi senza poter fare niente. */
    { "/system/bin/shell", 0, 0, -1, "/dev/ttyS0", 0 },
};
#define QUANTI_SERVIZI ((int)(sizeof servizi / sizeof servizi[0]))

/* Il dispositivo si apre? Si prova senza prenderlo come terminale di init (O_NOCTTY) e
 * senza aspettare (O_NONBLOCK), e lo si richiude subito. Una seriale senza hardware dietro
 * esiste come file ma non si apre: come se non ci fosse. */
static int dispositivo_presente(const char *percorso) {
    int d = open(percorso, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (d < 0) return 0;
    close(d);
    return 1;
}

/* --- LE PORTE IN CUSTODIA --- */

/* C'e' qualcosa da leggere su 'd' (senza aspettare)? */
static int da_leggere(int d) {
    struct pollfd p = { d, POLLIN, 0 };
    return poll(&p, 1, 0) > 0 && (p.revents & POLLIN);
}

/* Un keep dal servizio: la porta va in tabella. Un messaggio sbagliato, o la tabella piena: il
 * descrittore allegato si chiude (e il servizio non ne sa niente: nessuna risposta). */
static void tieni_porta(struct servizio *s, const struct messaggio *m) {
    struct chi_chiede app;
    int p = 0;
    while (p < MAX_PORTE_CUSTODITE && s->porte[p].d >= 0) p++;
    if (m->allegato >= 0 && messaggio_vale(m, CHIAVE_RICHIESTA, RICHIESTA_CUSTODISCI) &&
        p < MAX_PORTE_CUSTODITE && app_da_attach(m, &app) == 0) {
        s->porte[p].d = m->allegato;
        s->porte[p].chi = app;
        return;
    }
    if (m->allegato >= 0) close(m->allegato);
}

/* Legge dal canale del custode tutto quello che c'e' (senza aspettare). */
static void leggi_custode(struct servizio *s) {
    static struct messaggio m;
    while (da_leggere(s->custode)) {
        int n = messaggio_ricevi(s->custode, &m);
        if (n == 1) tieni_porta(s, &m);
        else if (n != MESSAGGIO_NON_VALIDO) return;   /* (un non valido ha gia' chiuso l'allegato) */
    }
}

/* Butta tutto quello che il servizio non ha fatto in tempo a leggere (nel suo capo, che init
 * tiene): le porte allegate si chiudono, init le ha ancora tutte nella sua tabella. */
static void svuota_servizio(struct servizio *s) {
    static struct messaggio m;
    while (da_leggere(s->custode_servizio)) {
        int n = messaggio_ricevi(s->custode_servizio, &m);
        if (n == 1 && m.allegato >= 0) close(m.allegato);
        else if (n != 1 && n != MESSAGGIO_NON_VALIDO) return;
    }
}

/* Il servizio sta per partire: gli si riconsegnano le porte, una per messaggio, e alla fine
 * restored (anche se non ce n'e' nessuna). Prima init legge i keep arrivati e non ancora letti
 * (una caduta subito dopo), e butta i messaggi rimasti non letti nel capo del servizio. */
static void riconsegna_porte(struct servizio *s) {
    leggi_custode(s);
    svuota_servizio(s);
    int date = 0;
    for (int p = 0; p < MAX_PORTE_CUSTODITE; p++)
        if (s->porte[p].d >= 0 &&
            manda_porta_app(s->custode, RICHIESTA_RICONSEGNA, &s->porte[p].chi, s->porte[p].d) == 0)
            date++;
    static struct messaggio fine;
    messaggio_inizia(&fine);
    messaggio_aggiungi(&fine, CHIAVE_RICHIESTA, RICHIESTA_RICONSEGNATE);
    messaggio_manda(s->custode, &fine);
    if (date > 0) {
        char riga[512];
        int n = snprintf(riga, sizeof riga, "init: riconsegnate %d porte a %s\n", date, s->percorso);
        if (n >= (int)sizeof riga) n = (int)sizeof riga - 1;
        scrivi(riga, n);
    }
}

/* Lo fa partire e si tiene il descrittore del suo processo. */
static void avvia_servizio(struct servizio *s) {
    if (s->non_qui) return;
    if (s->custodisce) riconsegna_porte(s);
    int n = s->uid == UID_SISTEMA ? avvia_con(s->percorso, s->risorse, s->quante)
                                  : avvia_come_con_descrittori(s->percorso, s->uid, s->risorse, s->quante);
    s->processo = n < 0 ? -1 : processo(n);
    if (s->processo < 0) {
        scrivi_riga("init", "non riesco ad avviare ", s->percorso);
    }
}

/* I PROGRAMMI DELLE PROVE. Se sul disco c'e' la cartella /data/tests, init fa partire
 * una volta ogni programma che ci trova dentro. C'e' solo nel disco delle prove
 * automatiche (disco_prove.img, vedi build.sh: la sua partizione data ha questa cartella
 * in piu'): nel disco normale non c'e', e qui non succede niente. Questi programmi non
 * sono servizi: init non li sorveglia, e se finiscono non li fa ripartire. */
#define CARTELLA_PROVE "/data/tests"

static void avvia_prove(void) {
    int d = apri(CARTELLA_PROVE, APRI_LEGGERE);
    if (d < 0) return;                       /* il disco normale: niente prove */
    struct voce_cartella v;
    while (leggi(d, &v, sizeof v) == sizeof v) {
        if (v.tipo != TIPO_FILE) continue;
        v.nome[MAX_NOME_FILE - 1] = 0;
        char percorso[MAX_PERCORSO];
        int n = 0;
        for (const char *s = CARTELLA_PROVE "/"; *s; s++) percorso[n++] = *s;
        for (const char *s = v.nome; *s && n < MAX_PERCORSO - 1; s++) percorso[n++] = *s;
        percorso[n] = 0;
        if (avvia(percorso) < 0) {
            scrivi_riga("init", "non riesco ad avviare ", percorso);
        }
    }
    chiudi(d);
}

int main(void) {
    scrivi_testo("ciao come va\n");

    /* le risorse del compositore, nell'ordine in cui le trovera': 0, 1, 2. Il tubo degli
     * eventi resta a init anche se eventi o il compositore cadono: ripartono con lo stesso */
    int tubo[2] = { -1, -1 };
    coppia(tubo);
    risorse_eventi[0] = tubo[1];
    risorse_compositore[0] = apri_schermo();
    risorse_compositore[1] = tubo[0];
    risorse_compositore[2] = ascolta(NOME_COMPOSITORE);
    risorse_launcher[0] = ascolta(NOME_LAUNCHER);
    risorse_catalog[0] = ascolta(NOME_CATALOG);
    if (risorse_compositore[0] < 0 || tubo[0] < 0 || risorse_compositore[2] < 0 ||
        risorse_launcher[0] < 0 || risorse_catalog[0] < 0) {
        scrivi_testo("init: non riesco a prendere schermo, tastiera o i nomi dei servizi\n");
        for (;;) dormi(60 * 1000);
    }

    /* il canale del custode di ogni servizio che ne ha uno: init tiene un capo (per sempre), e
     * presta l'altro al servizio, in fondo alle sue risorse. Anche l'altro capo init se lo
     * tiene: serve a buttare i messaggi che il servizio non ha letto prima di cadere. */
    for (int i = 0; i < QUANTI_SERVIZI; i++) {
        struct servizio *s = &servizi[i];
        for (int p = 0; p < MAX_PORTE_CUSTODITE; p++) s->porte[p].d = -1;
        if (!s->custodisce) continue;
        int canale[2] = { -1, -1 };
        if (coppia(canale) < 0) {
            scrivi_testo("init: non riesco a preparare il canale delle porte delle app\n");
            for (;;) dormi(60 * 1000);
        }
        s->custode = canale[0];
        s->custode_servizio = canale[1];
        s->risorse[s->quante - 1] = canale[1];
    }

    for (int i = 0; i < QUANTI_SERVIZI; i++) {
        struct servizio *s = &servizi[i];
        if (s->dispositivo && !dispositivo_presente(s->dispositivo)) {
            s->non_qui = 1;
            /* quattro pezzi: si compone la riga e si scrive con una sola write */
            char riga[512];
            int n = snprintf(riga, sizeof riga, "init: manca %s, non avvio %s\n", s->dispositivo, s->percorso);
            if (n >= (int)sizeof riga) n = (int)sizeof riga - 1;
            scrivi(riga, n);
        }
        avvia_servizio(s);
    }
    avvia_prove();

    /* Le applicazioni non le fa partire init: le apre il launcher, quando la scrivania
     * glielo chiede (un clic sulla userbar). */

    /* LA SORVEGLIANZA: si aspetta che uno dei servizi finisca, e lo si fa ripartire. Se
     * non si e' riusciti ad avviarlo, si riprova ogni 5 secondi. */
    for (;;) {
        /* si guarda tutto con poll: i processi dei servizi (POLLIN: finito), i canali dei custodi
         * (POLLIN: un keep da leggere) e le porte in custodia (solo POLLRDHUP: chiusa dall'app,
         * mai i dati: una porta su cui l'app ha mandato connect e' leggibile, e non e' chiusa) */
        struct pollfd g[QUANTI_SERVIZI * (2 + MAX_PORTE_CUSTODITE)];
        int servizio_di[QUANTI_SERVIZI * (2 + MAX_PORTE_CUSTODITE)];   /* quale servizio */
        int porta_di[QUANTI_SERVIZI * (2 + MAX_PORTE_CUSTODITE)];      /* -1 processo, -2 custode, o la porta */
        int n = 0, mancanti = 0;
        for (int i = 0; i < QUANTI_SERVIZI; i++) {
            struct servizio *s = &servizi[i];
            if (s->non_qui) continue;
            if (s->processo < 0) mancanti = 1;
            else { g[n] = (struct pollfd){ s->processo, POLLIN, 0 }; servizio_di[n] = i; porta_di[n++] = -1; }
            if (!s->custodisce) continue;
            g[n] = (struct pollfd){ s->custode, POLLIN, 0 }; servizio_di[n] = i; porta_di[n++] = -2;
            for (int p = 0; p < MAX_PORTE_CUSTODITE; p++)
                if (s->porte[p].d >= 0) {
                    g[n] = (struct pollfd){ s->porte[p].d, POLLRDHUP, 0 };
                    servizio_di[n] = i;
                    porta_di[n++] = p;
                }
        }
        if (n) { while (poll(g, (nfds_t)n, mancanti ? 5000 : -1) < 0 && errno == EINTR) {} }
        else dormi(5000);
        /* prima le porte (una chiusa dall'app si butta), poi i canali, poi i processi: un keep
         * puo' riempire un posto liberato in questo giro */
        for (int k = 0; k < n; k++) {
            if (porta_di[k] < 0 || !(g[k].revents & (POLLHUP | POLLRDHUP | POLLERR))) continue;
            struct servizio *s = &servizi[servizio_di[k]];
            if (s->porte[porta_di[k]].d != g[k].fd) continue;
            close(g[k].fd);
            s->porte[porta_di[k]].d = -1;
        }
        for (int k = 0; k < n; k++)
            if (porta_di[k] == -2 && (g[k].revents & POLLIN)) leggi_custode(&servizi[servizio_di[k]]);
        for (int k = 0; k < n; k++) {
            if (porta_di[k] != -1 || !(g[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            struct servizio *s = &servizi[servizio_di[k]];
            chiudi(s->processo);
            s->processo = -1;
            scrivi_riga("init", s->percorso, " e' finito, lo faccio ripartire");
            dormi(500);   /* se cade subito ogni volta, almeno non si gira a vuoto */
        }
        for (int i = 0; i < QUANTI_SERVIZI; i++)
            if (servizi[i].processo < 0) avvia_servizio(&servizi[i]);
    }
}
