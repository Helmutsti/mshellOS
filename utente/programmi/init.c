/* init: il primo programma del sistema, il processo numero 1 di Linux.
 *
 * Lo fa partire avvio (utente/avvio/avvio.c), il programma dell'archivio di avvio, dopo
 * aver montato il disco: init prende il suo posto, con lo stesso numero 1, come sistema
 * (uid 0). E' lui a far nascere i servizi del sistema: eventi (che legge tastiera e
 * mouse), il compositore, il launcher (che fa partire le applicazioni) e la scrivania.
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
#include "libreria.h"
#include "protocollo.h"
#include "launcher.h"

/* I SERVIZI: i programmi del sistema che init fa partire e tiene in vita. Eventi riceve
 * in prestito il capo del tubo su cui scrive; il compositore lo schermo, l'altro capo del
 * tubo e il suo nome; il launcher il suo nome; la scrivania niente (si collega al
 * compositore come le applicazioni). L'ordine conta: prima eventi e il compositore. */
struct servizio {
    const char *percorso;
    const int *risorse;      /* i descrittori da prestargli, ai posti 0, 1, 2... */
    int quante;
    int processo;            /* il descrittore del suo processo: pronto quando finisce */
};

static int risorse_eventi[1];
static int risorse_compositore[3];
static int risorse_launcher[1];
static struct servizio servizi[] = {
    { "/system/services/eventi", risorse_eventi, 1, -1 },
    { "/system/services/compositore", risorse_compositore, 3, -1 },
    { "/system/services/launcher", risorse_launcher, 1, -1 },
    { "/system/services/scrivania", 0, 0, -1 },
};
#define QUANTI_SERVIZI ((int)(sizeof servizi / sizeof servizi[0]))

/* Lo fa partire e si tiene il descrittore del suo processo. */
static void avvia_servizio(struct servizio *s) {
    int n = avvia_con(s->percorso, s->risorse, s->quante);
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
    if (risorse_compositore[0] < 0 || tubo[0] < 0 || risorse_compositore[2] < 0 ||
        risorse_launcher[0] < 0) {
        scrivi_testo("init: non riesco a prendere schermo, tastiera o i nomi dei servizi\n");
        for (;;) dormi(60 * 1000);
    }

    for (int i = 0; i < QUANTI_SERVIZI; i++) avvia_servizio(&servizi[i]);
    avvia_prove();

    /* Le applicazioni non le fa partire init: le apre il launcher, quando la scrivania
     * glielo chiede (un clic sull'isola). */

    /* LA SORVEGLIANZA: si aspetta che uno dei servizi finisca, e lo si fa ripartire. Se
     * non si e' riusciti ad avviarlo, si riprova ogni 5 secondi. */
    for (;;) {
        struct da_guardare g[QUANTI_SERVIZI];
        int n = 0, mancanti = 0;
        int chi_e[QUANTI_SERVIZI];
        for (int i = 0; i < QUANTI_SERVIZI; i++) {
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
