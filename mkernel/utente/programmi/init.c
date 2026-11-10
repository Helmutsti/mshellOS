/* init: il primo programma, come in Linux.
 *
 * Il kernel, finito di prepararsi, lo legge dal disco (/Sistema/init) e lo fa partire in
 * ring 3, come sistema (uid 0). E' lui a far nascere gli altri programmi: i servizi del
 * sistema (il compositore e la scrivania) e, finche' non c'e' il launcher, tre
 * applicazioni: esempio, che si disegna tutto a mano, contatore e impostazioni, fatte con
 * il toolkit.
 *
 * INIT TIENE LE RISORSE DEL COMPOSITORE, e gliele presta. E' lui ad aprire lo schermo,
 * la tastiera e il mouse, e a mettersi in ascolto con il nome "compositore"; poi fa
 * partire il compositore passandogli queste tre cose gia' aperte (avvia_con), come
 * launchd sul Mac e systemd su Linux. Init non le usa: le tiene soltanto in mano.
 *
 * E LI SORVEGLIA. Se un servizio cade, init se ne accorge (il descrittore del processo
 * diventa pronto) e lo fa ripartire; il compositore, con le stesse risorse. Siccome init non
 * le ha mai lasciate, nel frattempo nessun altro ha potuto prenderle: lo schermo non
 * torna alla console, il nome resta occupato, e le applicazioni che si collegano proprio
 * in quel momento aspettano in fila, invece di trovare la porta chiusa. */
#include "libreria.h"
#include "protocollo.h"

/* I SERVIZI: i programmi del sistema che init fa partire e tiene in vita. Il compositore
 * riceve in prestito schermo, tastiera e il suo nome; la scrivania niente (si collega al
 * compositore come le applicazioni). L'ordine conta: prima il compositore. */
struct servizio {
    const char *percorso;
    const int *risorse;      /* i descrittori da prestargli, ai posti 0, 1, 2... */
    int quante;
    int processo;            /* il descrittore del suo processo: pronto quando finisce */
};

static int risorse_compositore[3];
static struct servizio servizi[] = {
    { "/Sistema/Servizi/compositore", risorse_compositore, 3, -1 },
    { "/Sistema/Servizi/scrivania", 0, 0, -1 },
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

int main(void) {
    scrivi_testo("ciao come va\n");

    /* le risorse del compositore, nell'ordine in cui le trovera': 0, 1, 2 */
    risorse_compositore[0] = apri_schermo();
    risorse_compositore[1] = apri_eventi();
    risorse_compositore[2] = ascolta(NOME_COMPOSITORE);
    if (risorse_compositore[0] < 0 || risorse_compositore[1] < 0 || risorse_compositore[2] < 0) {
        scrivi_testo("init: non riesco a prendere schermo, tastiera o il nome del compositore\n");
        for (;;) dormi(60 * 1000);
    }

    for (int i = 0; i < QUANTI_SERVIZI; i++) avvia_servizio(&servizi[i]);

    /* le applicazioni non sono del sistema: partono come UID_APPLICAZIONI (1000). Il nome
     * "compositore" c'e' gia' (lo tiene init): si collegano subito. (Finche' non c'e' il
     * launcher, le fa partire init, una volta.) */
    avvia_come("/Applicazioni/Esempio/Esempio", UID_APPLICAZIONI);
    avvia_come("/Applicazioni/Contatore/Contatore", UID_APPLICAZIONI);
    avvia_come("/Applicazioni/Impostazioni/Impostazioni", UID_APPLICAZIONI);

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
