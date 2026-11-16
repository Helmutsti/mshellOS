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
 * in quel momento aspettano in fila, invece di trovare la porta chiusa. */
#include "corekit/corekit.h"
#include "gkit/sistema/protocollo.h"   /* NOME_COMPOSITORE */
#include "gkit/sistema/schermo.h"      /* apri_schermo */
#include "corekit/sistema/launcher.h"
#include "corekit/sistema/privilegi.h"    /* avvia_come_con_descrittori: catalog come la persona */
#include "corekit/catalog.h"              /* NOME_CATALOG */
#include <fcntl.h>
#include <unistd.h>

/* I SERVIZI: i programmi del sistema che init fa partire e tiene in vita. Eventi riceve
 * in prestito il capo del tubo su cui scrive; il compositore lo schermo, l'altro capo del
 * tubo e il suo nome; il launcher il suo nome; la scrivania niente (si collega al
 * compositore come le applicazioni); network niente (la configurazione se la legge da
 * solo, e gira come sistema perche' Linux gli lasci cambiare le schede di rete); catalog il
 * suo nome, e gira come la persona (uid 1000: tocca solo le sue preferenze). L'ordine
 * conta: prima eventi e il compositore. */
struct servizio {
    const char *percorso;
    const int *risorse;      /* i descrittori da prestargli, ai posti 0, 1, 2... */
    int quante;
    int processo;            /* il descrittore del suo processo: pronto quando finisce */
    const char *dispositivo; /* se non e' 0, il servizio parte solo se questo si apre */
    int non_qui;             /* 1: il dispositivo manca, su questo computer non parte */
    unsigned int uid;        /* chi e': 0 il sistema (quasi tutti), UID_APPLICAZIONI la persona */
};

static int risorse_eventi[1];
static int risorse_compositore[3];
static int risorse_launcher[1];
static int risorse_catalog[1];
static struct servizio servizi[] = {
    /* le impostazioni della persona: prima delle app e della scrivania, che le chiedono */
    { "/system/services/catalog/catalog", risorse_catalog, 1, -1, 0, 0, UID_APPLICAZIONI },
    { "/system/services/eventi/eventi", risorse_eventi, 1, -1 },
    { "/system/services/compositore/compositore", risorse_compositore, 3, -1 },
    { "/system/services/launcher/launcher", risorse_launcher, 1, -1 },
    { "/system/services/scrivania/scrivania", 0, 0, -1 },
    /* la rete. Se la sua scheda non c'e', non e' init a saperlo (la scheda e' scritta
     * nella configurazione della rete): il servizio lo dice e aspetta, senza finire */
    { "/system/services/network/network", 0, 0, -1 },
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

/* Lo fa partire e si tiene il descrittore del suo processo. */
static void avvia_servizio(struct servizio *s) {
    if (s->non_qui) return;
    int n = s->uid == UID_SISTEMA ? avvia_con(s->percorso, s->risorse, s->quante)
                                  : avvia_come_con_descrittori(s->percorso, s->uid, s->risorse, s->quante);
    s->processo = n < 0 ? -1 : processo(n);
    if (s->processo < 0) {
        scrivi_testo("init: non riesco ad avviare ");
        scrivi_testo(s->percorso);
        scrivi_testo("\n");
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
            scrivi_testo("init: non riesco ad avviare ");
            scrivi_testo(percorso);
            scrivi_testo("\n");
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

    for (int i = 0; i < QUANTI_SERVIZI; i++) {
        struct servizio *s = &servizi[i];
        if (s->dispositivo && !dispositivo_presente(s->dispositivo)) {
            s->non_qui = 1;
            scrivi_testo("init: manca ");
            scrivi_testo(s->dispositivo);
            scrivi_testo(", non avvio ");
            scrivi_testo(s->percorso);
            scrivi_testo("\n");
        }
        avvia_servizio(s);
    }
    avvia_prove();

    /* Le applicazioni non le fa partire init: le apre il launcher, quando la scrivania
     * glielo chiede (un clic sulla userbar). */

    /* LA SORVEGLIANZA: si aspetta che uno dei servizi finisca, e lo si fa ripartire. Se
     * non si e' riusciti ad avviarlo, si riprova ogni 5 secondi. */
    for (;;) {
        struct da_guardare g[QUANTI_SERVIZI];
        int n = 0, mancanti = 0;
        int chi_e[QUANTI_SERVIZI];
        for (int i = 0; i < QUANTI_SERVIZI; i++) {
            if (servizi[i].non_qui) continue;
            if (servizi[i].processo < 0) { mancanti = 1; continue; }
            g[n].descrittore = servizi[i].processo;
            chi_e[n++] = i;
        }
        if (n) aspetta(g, n, mancanti ? 5000 : -1);
        else dormi(5000);
        for (int k = 0; k < n; k++) {
            if (!g[k].pronto) continue;
            struct servizio *s = &servizi[chi_e[k]];
            chiudi(s->processo);
            s->processo = -1;
            scrivi_testo("init: ");
            scrivi_testo(s->percorso);
            scrivi_testo(" e' finito, lo faccio ripartire\n");
            dormi(500);   /* se cade subito ogni volta, almeno non si gira a vuoto */
        }
        for (int i = 0; i < QUANTI_SERVIZI; i++)
            if (servizi[i].processo < 0) avvia_servizio(&servizi[i]);
    }
}
