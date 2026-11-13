/* CoreKit: i messaggi tra i servizi e i loro clienti (messaggi.h).
 *
 * PERCHE'. Un servizio (network, domani il firewall, gli account...) riceve richieste dai
 * suoi clienti (le app, la shell, la scrivania) sulla sua PORTA: un socket con un nome in
 * /run/mshell/<nome> (comunicazione.c). Il compositore e il launcher hanno ognuno un
 * formato dei messaggi suo, fatto di struct C (chiamate.h, launcher.h). Per non farne uno
 * nuovo per ogni servizio, da qui in poi c'e' uno standard solo (SISTEMA.md, sezione 3),
 * piccolissimo, e lo stesso dei file di configurazione:
 *
 *   - un messaggio e' TESTO: righe "chiave=valore", separate da a capo;
 *   - una chiave puo' ripetersi (es. due righe dns=...): l'ordine resta quello in cui si
 *     sono scritte;
 *   - le chiavi sono in inglese minuscolo: lettere a-z, cifre e _ (messaggi.h);
 *   - un valore e' qualsiasi testo senza a capo (anche con dentro un =: conta solo il
 *     primo, quello dopo la chiave);
 *   - la BUSTA, uguale per tutti: request (quale richiesta), id (un numero scelto da chi
 *     chiede, che la risposta ripete), result (l'esito, nella risposta: ok, denied,
 *     invalid, error; e timeout, che non manda il servizio ma mette chiedi quando il
 *     servizio non risponde in tempo);
 *   - un descrittore aperto (un file, un foglio di memoria) puo' viaggiare ACCANTO al
 *     messaggio, uno al massimo, come fa gia' manda_foglio;
 *   - un messaggio = un pacchetto: le porte sono SOCK_SEQPACKET, ogni messaggio arriva
 *     intero o non arriva, e non serve dire dove finisce. Al massimo MAX_MESSAGGIO byte.
 *
 * Perche' testo e non struct: si legge a occhio (nel registro, in una prova), un servizio
 * puo' aggiungere una chiave senza rompere i clienti vecchi (chi non la conosce la
 * ignora), e non ci sono misure di campi da tenere uguali tra chi manda e chi riceve.
 *
 * UN ESEMPIO DI SCAMBIO. La scrivania chiede al servizio network di mettere due server dei
 * nomi; nel pacchetto viaggiano queste righe (qui una per riga):
 *
 *     request=set-dns              la richiesta
 *     dns=1.1.1.1                  i dati: una chiave che si ripete
 *     dns=9.9.9.9
 *     id=7                         lo mette chiedi, da solo
 *
 * e il servizio risponde, sullo stesso collegamento:
 *
 *     id=7                         lo stesso della richiesta
 *     result=ok
 *
 * oppure "result=denied" (non ha il permesso) o "result=invalid" (es. un dns che non e' un
 * indirizzo), magari con altre chiavi che il servizio documenta (per esempio una frase per
 * la persona). In codice, dalla parte di chi chiede:
 *
 *     struct messaggio q, r;
 *     messaggio_inizia(&q);
 *     messaggio_aggiungi(&q, CHIAVE_RICHIESTA, "set-dns");
 *     messaggio_aggiungi(&q, "dns", "1.1.1.1");
 *     messaggio_aggiungi(&q, "dns", "9.9.9.9");
 *     int d = collega("network");
 *     if (d >= 0 && chiedi(d, &q, &r) == 0 && messaggio_vale(&r, CHIAVE_ESITO, ESITO_OK)) ...
 *
 * e dalla parte del servizio, per ogni collegamento accettato:
 *
 *     struct messaggio q, r;
 *     int e;
 *     while ((e = messaggio_ricevi(d, &q)) != 0 && e != -1) {
 *         if (e == MESSAGGIO_NON_VALIDO) messaggio_risposta(&r, 0, ESITO_NON_VALIDO);
 *         else if (messaggio_vale(&q, CHIAVE_RICHIESTA, "set-dns")) {
 *             for (const char *v = 0; (v = messaggio_prossimo(&q, "dns", v));) ...
 *             messaggio_risposta(&r, &q, ESITO_OK);
 *         } else messaggio_risposta(&r, &q, ESITO_NON_VALIDO);
 *         messaggio_manda(d, &r);
 *     }
 *
 * DENTRO. Una struct messaggio tiene le righe una dopo l'altra, ognuna finita da uno 0 al
 * posto dell'a capo: "request=set-dns\0dns=1.1.1.1\0". Cosi' messaggio_valore da' un
 * puntatore dentro il messaggio, gia' finito da 0, senza copiare niente. Mandando, gli 0
 * tornano a capo; ricevendo, gli a capo diventano 0, dopo aver controllato riga per riga
 * che il pacchetto segua lo standard: chi riceve non deve mai fidarsi di chi manda.
 *
 * Un messaggio vuoto non si manda: su SOCK_SEQPACKET un pacchetto di 0 byte, ricevuto,
 * sembra un collegamento chiuso (recvmsg da' 0 in tutti e due i casi).
 *
 * IL TEMPO MASSIMO. Chi chiede non aspetta per sempre: se il servizio e' vivo ma non
 * risponde (bloccato, o molto lento), chiedi smette dopo TEMPO_CHIEDI (5 secondi, uguale
 * per tutti), mette nella risposta result=timeout e da' CHIEDI_SCADUTO. Per una richiesta
 * che si sa lenta c'e' chiedi_entro, con un tempo suo. Se invece il servizio cade, il
 * kernel chiude il collegamento e chiedi lo sa subito (-1), senza aspettare. Una risposta
 * che arriva dopo il tempo massimo resta nel collegamento: la butta la chiedi successiva,
 * perche' ha l'id vecchio, come ogni risposta con un id che non e' il suo.
 *
 * CHI CHIEDE. Il servizio sa chi c'e' dall'altra parte da chi_chiede: l'uid e il programma,
 * che dice il kernel (SO_PEERCRED e /proc/<numero>/exe: chi e programma_di in
 * comunicazione.c), e quindi non si possono fingere. Poi chiede "ha il permesso X?" con
 * ha_permesso, e se no risponde ESITO_NEGATO. Oggi ha_permesso e' PROVVISORIA: risponde
 * sempre si' (vedi sotto, accanto al suo codice). */
#include <string.h>

#include "../messaggi.h"
#include "../sistema/servizi.h"

/* --- costruire --- */

void messaggio_inizia(struct messaggio *m) {
    m->byte = 0;
    m->allegato = -1;
    m->guasto = 0;
    m->testo[0] = 0;
}

/* La lunghezza della chiave, se va bene (a-z, 0-9, _, da 1 a MAX_CHIAVE - 1 lettere), o -1.
 * Finisce allo 0 o, se fine non e' 0, al primo carattere uguale a fine (ricevendo: '='). */
static int chiave_giusta(const char *chiave, char fine) {
    int n = 0;
    for (; chiave[n] && chiave[n] != fine; n++) {
        char c = chiave[n];
        if (n >= MAX_CHIAVE - 1 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return -1;
    }
    return n == 0 ? -1 : n;
}

int messaggio_aggiungi(struct messaggio *m, const char *chiave, const char *valore) {
    int lc = chiave_giusta(chiave, 0);
    size_t lv = strlen(valore);
    /* la riga intera: chiave, =, valore, e l'a capo (qui uno 0) */
    if (lc < 0 || memchr(valore, '\n', lv) || (size_t)lc + 1 + lv + 1 > MAX_MESSAGGIO - m->byte) {
        m->guasto = 1;
        return -1;
    }
    char *p = m->testo + m->byte;
    memcpy(p, chiave, (size_t)lc);
    p[lc] = '=';
    memcpy(p + lc + 1, valore, lv + 1);     /* con lo 0 finale */
    m->byte += (unsigned int)lc + 1 + (unsigned int)lv + 1;
    return 0;
}

int messaggio_aggiungi_numero(struct messaggio *m, const char *chiave, long numero) {
    char cifre[24];
    int k = sizeof cifre - 1;
    cifre[k] = 0;
    /* in unsigned long, per scrivere anche il numero piu' negativo (che, cambiato di
     * segno, non sta in un long) */
    unsigned long n = numero < 0 ? 0UL - (unsigned long)numero : (unsigned long)numero;
    do cifre[--k] = (char)('0' + n % 10); while (n /= 10);
    if (numero < 0) cifre[--k] = '-';
    return messaggio_aggiungi(m, chiave, cifre + k);
}

void messaggio_risposta(struct messaggio *risposta, const struct messaggio *richiesta, const char *esito) {
    messaggio_inizia(risposta);
    const char *id = richiesta ? messaggio_valore(richiesta, CHIAVE_ID) : 0;
    if (id) messaggio_aggiungi(risposta, CHIAVE_ID, id);
    messaggio_aggiungi(risposta, CHIAVE_ESITO, esito);
}

/* --- leggere --- */

const char *messaggio_prossimo(const struct messaggio *m, const char *chiave, const char *prima) {
    size_t lc = strlen(chiave);
    const char *p = m->testo, *fine = m->testo + m->byte;
    /* si riparte dalla riga dopo quella di 'prima' */
    if (prima) {
        if (prima < m->testo || prima >= fine) return 0;
        p = prima + strlen(prima) + 1;
    }
    for (; p < fine; p += strlen(p) + 1)
        if (!strncmp(p, chiave, lc) && p[lc] == '=') return p + lc + 1;
    return 0;
}

const char *messaggio_valore(const struct messaggio *m, const char *chiave) {
    return messaggio_prossimo(m, chiave, 0);
}

int messaggio_numero(const struct messaggio *m, const char *chiave, long *numero) {
    const char *v = messaggio_valore(m, chiave);
    if (!v) return -1;
    int meno = *v == '-';
    if (meno) v++;
    /* al massimo 18 cifre: un numero cosi' sta sempre in un long, senza controlli */
    long n = 0;
    int k = 0;
    for (; v[k]; k++) {
        if (v[k] < '0' || v[k] > '9' || k >= 18) return -1;
        n = n * 10 + (v[k] - '0');
    }
    if (k == 0) return -1;
    *numero = meno ? -n : n;
    return 0;
}

int messaggio_vale(const struct messaggio *m, const char *chiave, const char *valore) {
    const char *v = messaggio_valore(m, chiave);
    return v && !strcmp(v, valore);
}

/* --- mandare e ricevere --- */

int messaggio_manda(int d, const struct messaggio *m) {
    if (m->guasto || m->byte == 0 || m->byte > MAX_MESSAGGIO) return -1;
    char pacchetto[MAX_MESSAGGIO];
    memcpy(pacchetto, m->testo, m->byte);
    for (unsigned int i = 0; i < m->byte; i++)
        if (!pacchetto[i]) pacchetto[i] = '\n';
    int n = m->allegato >= 0 ? manda_foglio(d, pacchetto, m->byte, m->allegato) : manda(d, pacchetto, m->byte);
    return n == (int)m->byte ? 0 : -1;
}

/* Controlla il pacchetto appena ricevuto (m->byte byte in m->testo) e lo mette nella forma
 * di dentro: ogni a capo diventa 0. 0, o -1 se non segue lo standard. */
static int controlla(struct messaggio *m) {
    if (m->byte == 0 || m->byte > MAX_MESSAGGIO) return -1;
    /* l'ultima riga puo' anche non avere l'a capo: lo si aggiunge (c'e' il posto in piu') */
    if (m->testo[m->byte - 1] != '\n') m->testo[m->byte++] = '\n';
    if (m->byte > MAX_MESSAGGIO) return -1;
    char *p = m->testo, *fine = m->testo + m->byte;
    while (p < fine) {
        char *a_capo = memchr(p, '\n', (size_t)(fine - p));
        /* niente 0 nel testo (taglierebbe un valore), e la chiave buona seguita da = */
        if (memchr(p, 0, (size_t)(a_capo - p))) return -1;
        int lc = chiave_giusta(p, '=');
        if (lc < 0 || p[lc] != '=') return -1;
        *a_capo = 0;
        p = a_capo + 1;
    }
    return 0;
}

int messaggio_ricevi(int d, struct messaggio *m) {
    messaggio_inizia(m);
    /* si riceve in MAX_MESSAGGIO + 1 byte: se ne arrivano tanti, il pacchetto era piu'
     * lungo del limite (il kernel taglia il resto) */
    int n = ricevi_foglio(d, m->testo, MAX_MESSAGGIO + 1, &m->allegato);
    if (n <= 0) {
        if (m->allegato >= 0) chiudi(m->allegato);
        messaggio_inizia(m);
        return n;
    }
    m->byte = (unsigned int)n;
    if (controlla(m) < 0) {
        if (m->allegato >= 0) chiudi(m->allegato);
        messaggio_inizia(m);
        return MESSAGGIO_NON_VALIDO;
    }
    return 1;
}

/* --- chi chiede --- */

int chiedi_entro(int d, struct messaggio *richiesta, struct messaggio *risposta, int ms) {
    /* l'id: un numero che cresce, diverso per ogni richiesta di questo programma */
    static long ultimo;
    long id = ++ultimo;
    if (messaggio_valore(richiesta, CHIAVE_ID) || messaggio_aggiungi_numero(richiesta, CHIAVE_ID, id) < 0 ||
        messaggio_manda(d, richiesta) < 0)
        return -1;
    /* il tempo massimo vale per tutta l'attesa: ogni risposta buttata (id sbagliato) ha gia'
     * consumato una parte del tempo, e il giro dopo aspetta solo quello che resta */
    unsigned int partenza = tempo();
    if (ms < 0) ms = 0;
    for (;;) {
        unsigned int passati = tempo() - partenza;
        int resta = passati >= (unsigned int)ms ? 0 : ms - (int)passati;
        struct da_guardare g = { d, 0 };
        int pronti = aspetta(&g, 1, resta);
        if (pronti < 0) return -1;
        if (pronti == 0) {
            /* il servizio non ha risposto in tempo: la busta della risposta la fa chiedi */
            messaggio_risposta(risposta, richiesta, ESITO_SCADUTO);
            return CHIEDI_SCADUTO;
        }
        int e = messaggio_ricevi(d, risposta);
        if (e == 0 || e == -1) return -1;
        long ricevuto;
        if (e == 1 && messaggio_numero(risposta, CHIAVE_ID, &ricevuto) == 0 && ricevuto == id) return 0;
        /* una risposta che non e' per questa richiesta (o non si legge, o arrivata tardi a
         * una richiesta di prima): si butta */
        if (risposta->allegato >= 0) chiudi(risposta->allegato);
    }
}

int chiedi(int d, struct messaggio *richiesta, struct messaggio *risposta) {
    return chiedi_entro(d, richiesta, risposta, TEMPO_CHIEDI);
}

/* --- il servizio: chi c'e' dall'altra parte --- */

int chi_chiede(int d, struct chi_chiede *c) {
    c->uid = chi(d);
    if (c->uid < 0) return -1;
    if (programma_di(d, c->programma) < 0) c->programma[0] = 0;
    return 0;
}

/* I PERMESSI, PROVVISORI. Oggi questa funzione NON CONTROLLA NIENTE: risponde sempre si'.
 * Scrive pero' ogni domanda nel registro, con la parola "provvisorio", es.
 *     permesso network.configure a /system/apps/settings/code (uid 1000): concesso (provvisorio)
 * cosi' si vede chi chiede cosa, e quando si accendera' si sapra' gia' quali permessi
 * servono e a chi.
 *
 * QUANDO SARA' VERA, dal programma (che dice il kernel, non si puo' fingere) e dall'uid
 * risalira' a chi e':
 *   - un programma del SISTEMA (/system/services/..., /system/bin/... come uid 0): ha
 *     tutti i permessi;
 *   - un'APP (/system/apps/<id>/ o /data/apps/<id>/): ha il permesso solo se il suo
 *     manifesto lo dichiara E la persona gliel'ha concesso (i permessi concessi, salvati
 *     per ogni app: APPLICAZIONI.md, le sezioni sui permessi);
 *   - un COMANDO della persona (/data/users/<uid>/bin): parte dai permessi della persona
 *     che lo lancia (SISTEMA.md, domande aperte).
 * E rispondera' 0 quando il permesso non c'e': il servizio allora manda ESITO_NEGATO. */
int ha_permesso(const struct chi_chiede *c, const char *nome) {
    /* la riga intera in un pezzo solo, con una sola scrivi: cosi' non si mescola con le
     * righe degli altri programmi */
    char riga[64 + MAX_CHIAVE + MAX_PERCORSO_PROGRAMMA + 64];
    int n = 0;
    const char *pezzi[] = { "permesso ", nome, " a ", c->programma[0] ? c->programma : "un programma sconosciuto",
                            " (uid ", 0, "): concesso (provvisorio)\n" };
    for (unsigned int i = 0; i < sizeof pezzi / sizeof pezzi[0]; i++) {
        if (!pezzi[i]) {
            /* l'uid in cifre */
            char cifre[12];
            int k = sizeof cifre;
            unsigned int u = c->uid < 0 ? 0 : (unsigned int)c->uid;
            do cifre[--k] = (char)('0' + u % 10); while (u /= 10);
            while (k < (int)sizeof cifre && n < (int)sizeof riga - 1) riga[n++] = cifre[k++];
            continue;
        }
        /* un nome troppo lungo si taglia: la riga e' solo per il registro */
        for (const char *p = pezzi[i]; *p && n < (int)sizeof riga - 1; p++) riga[n++] = *p;
    }
    riga[n - 1] = '\n';   /* anche se si e' tagliata, la riga finisce con l'a capo */
    scrivi(riga, n);
    return 1;
}
