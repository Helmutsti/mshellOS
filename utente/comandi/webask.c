/* webask: chiede una pagina a un server web e ne scrive il contenuto. Il primo comando del
 * sistema (SISTEMA.md: un comando e' un programma senza grafica, in /system/bin, che gira
 * con i permessi di chi lo lancia). Un "curl" essenziale: HTTP e HTTPS. Per HTTPS usa
 * mbedTLS (il pacchetto di Alpine, legato dentro il programma) e i certificati radice di
 * Alpine (ca-certificates-bundle) in CERTIFICATI.
 *
 *     webask [-i] http[s]://nome[:porta][/percorso]
 *     webask -h
 *
 *     -i   scrive anche la riga di stato e le intestazioni della risposta, poi una riga
 *          vuota, poi il contenuto (come "curl -i")
 *     -h   questo aiuto, con i codici di uscita
 *
 * COSA FA, in ordine:
 *   1. legge l'indirizzo: lo schema deve essere "http://" o "https://" (maiuscole o
 *      minuscole). Dopo vengono il nome (o un indirizzo numerico; uno IPv6 tra parentesi
 *      quadre, come [::1]), la porta facoltativa (se manca: 80, o 443 con https) e il
 *      percorso (se manca: "/"). Quello che sta dopo un '#' resta a noi e non si manda (e'
 *      cosi' per tutti i programmi web). Ogni altro schema e' un errore.
 *   2. traduce il nome in indirizzi (getaddrinfo della libreria C musl: legge i server dei
 *      nomi in /etc/resolv.conf, che punta al file scritto dal servizio network). Il tempo
 *      massimo di questo passo lo decide musl (5 secondi per tentativo, 2 tentativi).
 *   3. si collega (TCP) al primo indirizzo che risponde, provandoli in ordine. Per tutti
 *      insieme ha al massimo TEMPO_COLLEGAMENTO millisecondi.
 *   4. manda la richiesta: GET, HTTP/1.1, con Host (obbligatorio in HTTP/1.1),
 *      "Connection: close" (il server chiude quando ha finito: una richiesta sola per
 *      collegamento) e "User-Agent: webask" (un nome generico: niente dati della persona).
 *   5. legge la risposta. Ogni lettura aspetta al massimo TEMPO_LETTURA millisecondi: un
 *      server che resta zitto piu' a lungo e' "tempo scaduto". La risposta e' fatta cosi':
 *      la riga di stato ("HTTP/1.1 200 OK"), le intestazioni ("Nome: valore", una per riga),
 *      una riga vuota, il contenuto. Le righe di stato 1xx (per esempio "100 Continue")
 *      sono annunci provvisori: si saltano e si legge la risposta vera che segue.
 *   6. scrive il CONTENUTO sullo standard output, cosi' com'e' (anche se non e' testo).
 *      Dove finisce il contenuto lo dice il server in uno di tre modi, e si capiscono tutti:
 *        - "Transfer-Encoding: chunked": a pezzi, ognuno preceduto dalla sua misura in
 *          esadecimale su una riga sua; un pezzo di misura 0 e' la fine (dopo possono
 *          venire altre intestazioni, che si leggono e si buttano);
 *        - "Content-Length: N": esattamente N byte; se il collegamento si chiude prima, la
 *          risposta e' troncata (errore);
 *        - nessuno dei due: il contenuto finisce quando il server chiude il collegamento.
 *      Le risposte 204 e 304 non hanno contenuto, per regola di HTTP.
 *
 * I REINDIRIZZAMENTI (le risposte 3xx: la pagina sta altrove) NON si seguono: webask
 * scrive sullo standard error dove porterebbero (l'intestazione Location) ed esce con
 * USCITA_REINDIRIZZAMENTO. Il contenuto di queste risposte (di solito una paginetta che
 * dice "spostato qui") non si scrive.
 *
 * HTTPS. Con https://, appena collegati, si apre la connessione sicura (TLS 1.2 o 1.3) e
 * tutto il resto passa da li'. Il certificato del server si controlla SEMPRE: deve essere
 * firmato da una delle autorita' in CERTIFICATI, valido oggi (serve l'orologio giusto) e
 * per il nome chiesto. Se non va, webask si ferma e dice perche' (USCITA_SICUREZZA): non
 * c'e' un modo per saltare il controllo.
 *
 * Le risposte 4xx e 5xx (pagina che non c'e', errore del server) sono risposte vere:
 * si scrive il contenuto e si esce con 0, come fa curl. La riga di stato si vede con -i.
 *
 * I CODICI DI USCITA (li dice anche "webask -h"):
 *     0  fatto: la risposta e' arrivata tutta (qualunque codice HTTP che non sia 3xx)
 *     1  uso sbagliato: argomenti, indirizzo non valido, schema che non sia http o https
 *     2  il nome non si traduce in un indirizzo (non esiste, o i server dei nomi non
 *        rispondono)
 *     3  collegamento non riuscito (rifiutato, rete irraggiungibile) o caduto a meta'
 *     4  tempo scaduto: nel collegarsi o nell'aspettare la risposta
 *     5  risposta non valida o troncata (non e' HTTP, o finisce prima del dovuto)
 *     6  reindirizzamento (3xx): non seguito, il posto nuovo e' sullo standard error
 *     7  non si riesce a scrivere sullo standard output
 *     8  connessione sicura non riuscita: certificato non valido, o errore di TLS
 *
 * COME E' FATTO: le chiamate di Linux dirette per la rete (socket, connect, poll, recv,
 * send: CoreKit non ha la rete, e non serve), CoreKit per scrivere (scrivi_in, lunghezza)
 * e per il tempo (tempo). Tutto quello che si legge passa da un recipiente solo (struct
 * lettore), da cui si prendono righe (le intestazioni, le misure dei pezzi) o blocchi di
 * byte (il contenuto). Nessuna memoria chiesta al sistema: tutto in recipienti fissi
 * (tranne quella che chiede mbedTLS per la connessione sicura e i certificati). */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <signal.h>

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include "corekit/corekit.h"

/* i certificati radice: le autorita' di cui ci si fida (il file di ca-certificates-bundle) */
#define CERTIFICATI "/etc/ssl/certs/ca-certificates.crt"

#define TEMPO_COLLEGAMENTO 10000   /* ms, per collegarsi (tutti gli indirizzi insieme) */
#define TEMPO_LETTURA      15000   /* ms, al massimo, tra un pezzo di risposta e il successivo */

#define USCITA_FATTO            0
#define USCITA_USO              1
#define USCITA_NOME             2
#define USCITA_COLLEGAMENTO     3
#define USCITA_TEMPO            4
#define USCITA_RISPOSTA         5
#define USCITA_REINDIRIZZAMENTO 6
#define USCITA_SCRITTURA        7
#define USCITA_SICUREZZA        8

#define MAX_SERVER      256
#define MAX_PERCORSO_WEB 4096
#define MAX_RIGA      8192          /* una riga di stato o di intestazione */
#define MAX_POSTO     2048          /* il valore di Location che si ricorda */

/* --- scrivere --- */

/* Scrive tutto 'byte' (write puo' scriverne meno di quanti chiesti). Se non riesce
 * sull'uscita, non c'e' piu' niente da fare: si esce. */
static void scrivi_tutto(int d, const void *dati, unsigned int byte) {
    const char *p = dati;
    while (byte > 0) {
        int n = scrivi_in(d, p, byte);
        if (n <= 0) _exit(USCITA_SCRITTURA);
        p += n;
        byte -= (unsigned int)n;
    }
}

static void fuori(const char *s) { scrivi_tutto(1, s, (unsigned int)lunghezza(s)); }

/* Un messaggio d'errore sullo standard error: "webask: " e i pezzi dati (fino a tre;
 * quelli a 0 si saltano), poi a capo. */
static void errore(const char *a, const char *b, const char *c) {
    const char *pezzi[] = { "webask: ", a, b, c, "\n" };
    for (int i = 0; i < 5; i++)
        if (pezzi[i]) scrivi_in(2, pezzi[i], (unsigned int)lunghezza(pezzi[i]));
}

static void __attribute__((noreturn)) fallisci(int uscita, const char *a, const char *b, const char *c) {
    errore(a, b, c);
    _exit(uscita);
}

static const char aiuto[] =
    "uso: webask [-i] http[s]://nome[:porta][/percorso]\n"
    "Chiede la pagina al server web (HTTP o HTTPS) e ne scrive il contenuto.\n"
    "  -i  scrive anche la riga di stato e le intestazioni\n"
    "  -h  questo aiuto\n"
    "I reindirizzamenti (3xx) non si seguono: il posto nuovo si scrive come errore.\n"
    "Codici di uscita:\n"
    "  0  fatto (qualunque risposta che non sia un reindirizzamento)\n"
    "  1  uso sbagliato, indirizzo non valido o schema non http/https\n"
    "  2  il nome non si traduce in un indirizzo\n"
    "  3  collegamento non riuscito o caduto\n"
    "  4  tempo scaduto\n"
    "  5  risposta non valida o troncata\n"
    "  6  reindirizzamento, non seguito\n"
    "  7  non si riesce a scrivere l'uscita\n"
    "  8  connessione sicura non riuscita (certificato non valido o errore TLS)\n";

/* --- piccoli aiuti sul testo --- */

static int minuscola(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* 1 se 's' comincia con 'inizio', senza badare a maiuscole e minuscole. */
static int comincia(const char *s, const char *inizio) {
    for (; *inizio; s++, inizio++)
        if (minuscola((unsigned char)*s) != minuscola((unsigned char)*inizio)) return 0;
    return 1;
}

/* 1 se 'a' e 'b' sono la stessa parola, senza badare a maiuscole e minuscole. */
static int uguali(const char *a, const char *b) {
    for (; *a && *b; a++, b++)
        if (minuscola((unsigned char)*a) != minuscola((unsigned char)*b)) return 0;
    return *a == *b;
}

/* Copia al massimo max-1 byte e chiude con lo 0. 0, o -1 se non ci stava. */
static int copia(char *dove, int max, const char *da, int byte) {
    if (byte < 0 || byte >= max) return -1;
    memcpy(dove, da, (size_t)byte);
    dove[byte] = 0;
    return 0;
}

/* --- 1. l'indirizzo --- */

struct indirizzo {
    char nome[MAX_SERVER];             /* senza parentesi quadre, anche se IPv6 */
    int ipv6;                        /* 1: il nome era tra parentesi quadre */
    char porta[8];                   /* in cifre, come la vuole getaddrinfo */
    int porta_data;                  /* 1: scritta nell'indirizzo (allora va anche in Host) */
    char percorso[MAX_PERCORSO_WEB]; /* comincia sempre con '/' */
    int sicuro;                      /* 1: https:// (TLS) */
};

static void leggi_indirizzo(const char *testo, struct indirizzo *u) {
    const char *dopo = strstr(testo, "://");
    if (!dopo) fallisci(USCITA_USO, "manca lo schema: scrivi http://", testo, 0);
    u->sicuro = comincia(testo, "https://");
    if (!u->sicuro && !comincia(testo, "http://")) {
        char schema[32];
        if (copia(schema, sizeof schema, testo, (int)(dopo - testo)) < 0) schema[0] = 0;
        fallisci(USCITA_USO, "schema non supportato: ", schema, " (solo http:// e https://)");
    }
    const char *p = dopo + 3;
    const char *predefinita = u->sicuro ? "443" : "80";

    /* il nome (e la porta) finiscono al primo '/', '?' o '#' */
    const char *fine = p;
    while (*fine && *fine != '/' && *fine != '?' && *fine != '#') fine++;
    for (const char *q = p; q < fine; q++)
        if (*q == '@') fallisci(USCITA_USO, "nome e password nell'indirizzo non sono supportati", 0, 0);

    const char *fine_nome, *due_punti = 0;
    u->ipv6 = 0;
    if (*p == '[') {                                     /* [indirizzo IPv6] */
        const char *chiusa = memchr(p, ']', (size_t)(fine - p));
        if (!chiusa) fallisci(USCITA_USO, "indirizzo non valido (manca ']'): ", testo, 0);
        if (copia(u->nome, sizeof u->nome, p + 1, (int)(chiusa - p - 1)) < 0)
            fallisci(USCITA_USO, "nome troppo lungo o vuoto: ", testo, 0);
        u->ipv6 = 1;
        fine_nome = chiusa + 1;
        if (fine_nome < fine) {
            if (*fine_nome != ':') fallisci(USCITA_USO, "indirizzo non valido: ", testo, 0);
            due_punti = fine_nome;
        }
    } else {
        due_punti = memchr(p, ':', (size_t)(fine - p));
        fine_nome = due_punti ? due_punti : fine;
        if (copia(u->nome, sizeof u->nome, p, (int)(fine_nome - p)) < 0)
            fallisci(USCITA_USO, "nome troppo lungo: ", testo, 0);
    }
    if (!u->nome[0]) fallisci(USCITA_USO, "manca il nome del server: ", testo, 0);

    u->porta_data = 0;
    strcpy(u->porta, predefinita);
    if (due_punti) {
        const char *c = due_punti + 1;
        int n = 0, cifre = 0;
        for (; c < fine; c++, cifre++) {
            if (*c < '0' || *c > '9' || cifre >= 5) fallisci(USCITA_USO, "porta non valida: ", testo, 0);
            n = n * 10 + (*c - '0');
        }
        if (cifre > 0) {                                  /* "nome:" senza cifre: la predefinita */
            if (n < 1 || n > 65535) fallisci(USCITA_USO, "porta non valida: ", testo, 0);
            if (copia(u->porta, sizeof u->porta, due_punti + 1, cifre) < 0)
                fallisci(USCITA_USO, "porta non valida: ", testo, 0);
            u->porta_data = !uguali(u->porta, predefinita);
        }
    }

    /* il percorso, con la domanda (?...) ma senza il '#...' */
    const char *cancelletto = strchr(fine, '#');
    int byte = cancelletto ? (int)(cancelletto - fine) : lunghezza(fine);
    if (byte == 0 || *fine != '/') {                      /* "" o "?a=b": si mette '/' davanti */
        u->percorso[0] = '/';
        if (copia(u->percorso + 1, sizeof u->percorso - 1, fine, byte) < 0)
            fallisci(USCITA_USO, "percorso troppo lungo", 0, 0);
    } else if (copia(u->percorso, sizeof u->percorso, fine, byte) < 0)
        fallisci(USCITA_USO, "percorso troppo lungo", 0, 0);
    for (const char *q = u->percorso; *q; q++)            /* niente spazi o a capo: romperebbero la richiesta */
        if ((unsigned char)*q <= ' ') fallisci(USCITA_USO, "spazi o caratteri di controllo nel percorso (scrivili come %20)", 0, 0);
}

/* --- 2. e 3. il nome e il collegamento --- */

/* Il collegamento con un indirizzo, entro 'ms' millisecondi: connect "senza aspettare"
 * (O_NONBLOCK) e poi poll, che aspetta al massimo quel tempo. Il descrittore (rimesso ad
 * aspettare normalmente), -1 se non riesce (in *perche' errno), -2 se il tempo e' scaduto. */
static int collegati_a(const struct addrinfo *a, int ms, int *perche) {
    int s = socket(a->ai_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (s < 0) { *perche = errno; return -1; }
    if (connect(s, a->ai_addr, a->ai_addrlen) < 0) {
        if (errno != EINPROGRESS) { *perche = errno; close(s); return -1; }
        struct pollfd pf = { s, POLLOUT, 0 };
        int n;
        do n = poll(&pf, 1, ms); while (n < 0 && errno == EINTR);
        if (n == 0) { close(s); return -2; }
        int e = 0;
        socklen_t l = sizeof e;
        if (n < 0 || getsockopt(s, SOL_SOCKET, SO_ERROR, &e, &l) < 0 || e) {
            *perche = n < 0 ? errno : e;
            close(s);
            return -1;
        }
    }
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) & ~O_NONBLOCK);
    return s;
}

static int collegati(const struct indirizzo *u) {
    struct addrinfo chiedo, *trovati = 0;
    memset(&chiedo, 0, sizeof chiedo);
    chiedo.ai_family = AF_UNSPEC;
    chiedo.ai_socktype = SOCK_STREAM;
    chiedo.ai_flags = AI_NUMERICSERV;
    int e = getaddrinfo(u->nome, u->porta, &chiedo, &trovati);
    if (e == EAI_NONAME || e == EAI_NODATA)
        fallisci(USCITA_NOME, "il nome ", u->nome, " non esiste");
    if (e == EAI_AGAIN)
        fallisci(USCITA_NOME, "il nome ", u->nome, " non si traduce: i server dei nomi non rispondono (la rete c'e'?)");
    if (e != 0) fallisci(USCITA_NOME, "il nome ", u->nome, " non si traduce");

    unsigned int scadenza = tempo() + TEMPO_COLLEGAMENTO;
    int perche = 0, scaduto = 0;
    for (struct addrinfo *a = trovati; a; a = a->ai_next) {
        int resta = (int)(scadenza - tempo());
        if (resta <= 0) { scaduto = 1; break; }
        int s = collegati_a(a, resta, &perche);
        if (s >= 0) { freeaddrinfo(trovati); return s; }
        if (s == -2) scaduto = 1;
    }
    freeaddrinfo(trovati);
    if (scaduto) fallisci(USCITA_TEMPO, "tempo scaduto: ", u->nome, " non risponde");
    errore("non riesco a collegarmi a ", u->nome, 0);
    fallisci(USCITA_COLLEGAMENTO, "  ", strerror(perche), 0);
}

/* --- 3 bis. la connessione sicura (https) ---
 * Con https tutto passa da 'tls' invece che direttamente dal socket: si manda con
 * mbedtls_ssl_write e si riceve con mbedtls_ssl_read, che cifrano e decifrano. */

static int sicuro;                       /* 1: si parla attraverso tls */
static mbedtls_ssl_context tls;
static mbedtls_ssl_config tls_regole;
static mbedtls_x509_crt autorita;
static mbedtls_net_context tls_presa;

static void __attribute__((noreturn)) fallisci_tls(int uscita, const char *cosa, int codice) {
    char perche[200];
    mbedtls_strerror(codice, perche, sizeof perche);
    fallisci(uscita, cosa, perche, 0);
}

/* Apre la connessione sicura sul socket gia' collegato 's' e controlla il certificato. */
static void apri_tls(int s, const struct indirizzo *u) {
    signal(SIGPIPE, SIG_IGN);            /* mbedTLS scrive con write: un server chiuso non deve ucciderci */
    if (psa_crypto_init() != PSA_SUCCESS) fallisci(USCITA_SICUREZZA, "la crittografia non parte (manca il caso?)", 0, 0);
    mbedtls_x509_crt_init(&autorita);
    int e = mbedtls_x509_crt_parse_file(&autorita, CERTIFICATI);
    if (e < 0) fallisci_tls(USCITA_SICUREZZA, "non riesco a leggere i certificati in " CERTIFICATI ": ", e);
    mbedtls_ssl_config_init(&tls_regole);
    e = mbedtls_ssl_config_defaults(&tls_regole, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (e) fallisci_tls(USCITA_SICUREZZA, "TLS: ", e);
    mbedtls_ssl_conf_authmode(&tls_regole, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls_regole, &autorita, 0);
    mbedtls_ssl_conf_read_timeout(&tls_regole, TEMPO_LETTURA);
    mbedtls_ssl_init(&tls);
    if ((e = mbedtls_ssl_setup(&tls, &tls_regole))) fallisci_tls(USCITA_SICUREZZA, "TLS: ", e);
    /* il nome: va al server (SNI, per scegliere il certificato) e si confronta con il certificato */
    if ((e = mbedtls_ssl_set_hostname(&tls, u->nome))) fallisci_tls(USCITA_SICUREZZA, "TLS: ", e);
    tls_presa.fd = s;
    mbedtls_ssl_set_bio(&tls, &tls_presa, mbedtls_net_send, 0, mbedtls_net_recv_timeout);

    while ((e = mbedtls_ssl_handshake(&tls)) != 0) {
        if (e == MBEDTLS_ERR_SSL_WANT_READ || e == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (e == MBEDTLS_ERR_SSL_TIMEOUT) fallisci(USCITA_TEMPO, "tempo scaduto: ", u->nome, " non completa la connessione sicura");
        if (e == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
            char perche[512];
            mbedtls_x509_crt_verify_info(perche, sizeof perche, "  ", mbedtls_ssl_get_verify_result(&tls));
            errore("il certificato di ", u->nome, " non e' valido:");
            scrivi_in(2, perche, (unsigned int)lunghezza(perche));
            _exit(USCITA_SICUREZZA);
        }
        fallisci_tls(USCITA_SICUREZZA, "connessione sicura non riuscita: ", e);
    }
    sicuro = 1;
}

/* Manda tutto 'n' byte, in chiaro o attraverso tls. */
static void manda_tutto(int s, const char *r, int n) {
    for (int fatti = 0; fatti < n;) {
        long k;
        if (sicuro) {
            k = mbedtls_ssl_write(&tls, (const unsigned char *)r + fatti, (size_t)(n - fatti));
            if (k == MBEDTLS_ERR_SSL_WANT_READ || k == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        } else {
            k = send(s, r + fatti, (size_t)(n - fatti), MSG_NOSIGNAL);
            if (k < 0 && errno == EINTR) continue;
        }
        if (k <= 0) fallisci(USCITA_COLLEGAMENTO, "collegamento caduto mentre mandavo la richiesta", 0, 0);
        fatti += (int)k;
    }
}

/* --- 4. la richiesta --- */

static void manda_richiesta(int s, const struct indirizzo *u) {
    static char r[MAX_PERCORSO_WEB + MAX_SERVER + 256];
    int n = 0;
    const char *pezzi[] = {
        "GET ", u->percorso, " HTTP/1.1\r\nHost: ",
        u->ipv6 ? "[" : "", u->nome, u->ipv6 ? "]" : "",
        u->porta_data ? ":" : "", u->porta_data ? u->porta : "",
        "\r\nUser-Agent: webask\r\nAccept: */*\r\nConnection: close\r\n\r\n",
    };
    for (unsigned int i = 0; i < sizeof pezzi / sizeof pezzi[0]; i++) {
        int l = lunghezza(pezzi[i]);
        memcpy(r + n, pezzi[i], (size_t)l);
        n += l;
    }
    manda_tutto(s, r, n);
}

/* --- 5. leggere la risposta --- */

/* Il recipiente da cui si legge: quello che e' arrivato dal collegamento e non e' ancora
 * stato preso sta tra 'inizio' e 'fine'. */
struct lettore {
    int s;
    int chiuso;                      /* il server ha chiuso: non arrivera' altro */
    int inizio, fine;
    char dati[16384];
};

/* Riempie il recipiente (se c'e' spazio, dopo averlo compattato). 1 se e' arrivato
 * qualcosa, 0 se il server ha chiuso. Tempo scaduto o errore: esce. */
static int riempi(struct lettore *l) {
    if (l->chiuso) return 0;
    if (l->inizio > 0) {
        memmove(l->dati, l->dati + l->inizio, (size_t)(l->fine - l->inizio));
        l->fine -= l->inizio;
        l->inizio = 0;
    }
    if (l->fine == (int)sizeof l->dati) return 1;     /* pieno: c'e' gia' da prendere */
    if (sicuro) {
        /* il tempo massimo lo conta mbedTLS (mbedtls_ssl_conf_read_timeout) */
        int k;
        do k = mbedtls_ssl_read(&tls, (unsigned char *)l->dati + l->fine, sizeof l->dati - (size_t)l->fine);
        while (k == MBEDTLS_ERR_SSL_WANT_READ || k == MBEDTLS_ERR_SSL_WANT_WRITE ||
               k == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET);
        if (k == MBEDTLS_ERR_SSL_TIMEOUT) fallisci(USCITA_TEMPO, "tempo scaduto: il server non manda piu' niente", 0, 0);
        /* chiusura: quella in regola (close_notify) o il socket chiuso senza avvisare (lo
         * fanno molti server; se manca dei byte, lo dice Content-Length o la fine dei pezzi) */
        if (k == 0 || k == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || k == MBEDTLS_ERR_NET_CONN_RESET) { l->chiuso = 1; return 0; }
        if (k < 0) fallisci_tls(USCITA_COLLEGAMENTO, "collegamento sicuro caduto: ", k);
        l->fine += k;
        return 1;
    }
    struct pollfd pf = { l->s, POLLIN, 0 };
    int n;
    do n = poll(&pf, 1, TEMPO_LETTURA); while (n < 0 && errno == EINTR);
    if (n == 0) fallisci(USCITA_TEMPO, "tempo scaduto: il server non manda piu' niente", 0, 0);
    ssize_t k;
    do k = recv(l->s, l->dati + l->fine, sizeof l->dati - (size_t)l->fine, 0); while (k < 0 && errno == EINTR);
    if (k < 0) fallisci(USCITA_COLLEGAMENTO, "collegamento caduto: ", strerror(errno), 0);
    if (k == 0) { l->chiuso = 1; return 0; }
    l->fine += (int)k;
    return 1;
}

/* Una riga (fino a "\n", senza "\r\n") in 'riga'. 1, o 0 se il server ha chiuso prima
 * della fine della riga. Una riga troppo lunga e' una risposta non valida. */
static int prendi_riga(struct lettore *l, char *riga) {
    for (;;) {
        char *a_capo = memchr(l->dati + l->inizio, '\n', (size_t)(l->fine - l->inizio));
        if (a_capo) {
            int byte = (int)(a_capo - (l->dati + l->inizio));
            if (byte > 0 && a_capo[-1] == '\r') byte--;
            if (copia(riga, MAX_RIGA, l->dati + l->inizio, byte) < 0)
                fallisci(USCITA_RISPOSTA, "risposta non valida: riga troppo lunga", 0, 0);
            l->inizio = (int)(a_capo + 1 - l->dati);
            return 1;
        }
        if (l->fine - l->inizio >= MAX_RIGA)
            fallisci(USCITA_RISPOSTA, "risposta non valida: riga troppo lunga", 0, 0);
        if (!riempi(l)) return 0;
    }
}

/* Passa allo standard output al massimo 'quanti' byte del contenuto (-1: fino a quando il
 * server chiude). Quanti ne ha passati: meno di 'quanti' vuol dire che il server ha chiuso. */
static long long passa(struct lettore *l, long long quanti) {
    long long passati = 0;
    while (quanti < 0 || passati < quanti) {
        if (l->fine == l->inizio && !riempi(l)) break;
        long long c = l->fine - l->inizio;
        if (quanti >= 0 && c > quanti - passati) c = quanti - passati;
        scrivi_tutto(1, l->dati + l->inizio, (unsigned int)c);
        l->inizio += (int)c;
        passati += c;
    }
    return passati;
}

/* Il contenuto a pezzi (Transfer-Encoding: chunked). Ogni pezzo: la misura in
 * esadecimale (con dopo, facoltativi, ";estensioni" che si ignorano), a capo, i byte, a
 * capo. Il pezzo di misura 0 e' l'ultimo; poi intestazioni finali fino a una riga vuota. */
static void passa_a_pezzi(struct lettore *l) {
    static char riga[MAX_RIGA];
    for (;;) {
        if (!prendi_riga(l, riga)) fallisci(USCITA_RISPOSTA, "risposta troncata (a pezzi)", 0, 0);
        long long misura = 0;
        int cifre = 0;
        for (const char *c = riga; ; c++, cifre++) {
            int v = *c >= '0' && *c <= '9' ? *c - '0'
                  : minuscola(*c) >= 'a' && minuscola(*c) <= 'f' ? minuscola(*c) - 'a' + 10 : -1;
            if (v < 0) break;
            if (cifre >= 15) fallisci(USCITA_RISPOSTA, "risposta non valida: pezzo troppo grande", 0, 0);
            misura = misura * 16 + v;
        }
        if (cifre == 0) fallisci(USCITA_RISPOSTA, "risposta non valida: misura del pezzo \"", riga, "\"");
        if (misura == 0) break;
        if (passa(l, misura) < misura) fallisci(USCITA_RISPOSTA, "risposta troncata (a pezzi)", 0, 0);
        if (!prendi_riga(l, riga) || riga[0])
            fallisci(USCITA_RISPOSTA, "risposta non valida: manca l'a capo dopo un pezzo", 0, 0);
    }
    /* le intestazioni finali: si buttano. Se il server chiude qui, il contenuto c'e' tutto. */
    while (prendi_riga(l, riga) && riga[0]) ;
}

/* --- tutto insieme --- */

int main(int argc, char **argv) {
    int intestazioni = 0;
    const char *testo = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { fuori(aiuto); return USCITA_FATTO; }
        else if (!strcmp(argv[i], "-i")) intestazioni = 1;
        else if (argv[i][0] == '-') fallisci(USCITA_USO, "opzione sconosciuta: ", argv[i], " (webask -h per l'aiuto)");
        else if (testo) fallisci(USCITA_USO, "un indirizzo solo, per favore (webask -h per l'aiuto)", 0, 0);
        else testo = argv[i];
    }
    if (!testo) fallisci(USCITA_USO, "manca l'indirizzo. uso: webask [-i] http[s]://nome[:porta][/percorso]", 0, 0);

    static struct indirizzo u;
    leggi_indirizzo(testo, &u);
    static struct lettore l;
    l.s = collegati(&u);
    if (u.sicuro) apri_tls(l.s, &u);
    manda_richiesta(l.s, &u);

    /* la riga di stato e le intestazioni */
    static char riga[MAX_RIGA], posto[MAX_POSTO];
    int codice;
    long long misura = -1;
    int a_pezzi = 0, c_e_posto = 0;
    for (;;) {
        if (!prendi_riga(&l, riga)) fallisci(USCITA_RISPOSTA, "il server ha chiuso senza rispondere", 0, 0);
        /* "HTTP/1.x NNN motivo" */
        if (!comincia(riga, "HTTP/") || !strchr(riga, ' '))
            fallisci(USCITA_RISPOSTA, "risposta non valida (non e' HTTP): ", riga, 0);
        const char *c = strchr(riga, ' ') + 1;
        if (c[0] < '1' || c[0] > '5' || c[1] < '0' || c[1] > '9' || c[2] < '0' || c[2] > '9' || (c[3] && c[3] != ' '))
            fallisci(USCITA_RISPOSTA, "risposta non valida: ", riga, 0);
        codice = (c[0] - '0') * 100 + (c[1] - '0') * 10 + (c[2] - '0');
        if (intestazioni) { fuori(riga); fuori("\n"); }

        for (;;) {
            if (!prendi_riga(&l, riga)) fallisci(USCITA_RISPOSTA, "risposta troncata nelle intestazioni", 0, 0);
            if (!riga[0]) break;
            if (intestazioni) { fuori(riga); fuori("\n"); }
            char *dp = strchr(riga, ':');
            if (!dp) continue;                              /* riga strana: si salta */
            *dp = 0;
            const char *valore = dp + 1;
            while (*valore == ' ' || *valore == '\t') valore++;
            int lv = lunghezza(valore);                     /* niente spazi in fondo */
            while (lv > 0 && (valore[lv - 1] == ' ' || valore[lv - 1] == '\t')) ((char *)valore)[--lv] = 0;
            if (uguali(riga, "content-length")) {
                misura = 0;
                int cifre = 0;
                for (const char *q = valore; *q >= '0' && *q <= '9'; q++, cifre++) {
                    if (cifre >= 18) fallisci(USCITA_RISPOSTA, "risposta non valida: Content-Length troppo grande", 0, 0);
                    misura = misura * 10 + (*q - '0');
                }
                if (cifre == 0 || valore[cifre]) fallisci(USCITA_RISPOSTA, "risposta non valida: Content-Length \"", valore, "\"");
            } else if (uguali(riga, "transfer-encoding")) {
                /* chunked conta solo se e' l'ultima codifica dell'elenco */
                int ll = lunghezza(valore);
                a_pezzi = ll >= 7 && uguali(valore + ll - 7, "chunked");
            } else if (uguali(riga, "location")) {
                c_e_posto = copia(posto, sizeof posto, valore, lv) == 0;
            }
        }
        if (intestazioni) fuori("\n");
        if (codice >= 200 || codice == 101) break;          /* 1xx: annuncio provvisorio, si va avanti */
        misura = -1; a_pezzi = 0; c_e_posto = 0;
    }

    if (codice >= 300 && codice <= 399 && codice != 304) {
        char numero[4] = { (char)('0' + codice / 100), (char)('0' + codice / 10 % 10), (char)('0' + codice % 10), 0 };
        if (!c_e_posto) fallisci(USCITA_REINDIRIZZAMENTO, "reindirizzamento (", numero, ") senza dire dove: non seguito");
        errore("reindirizzamento (", numero, "), non seguito");
        fallisci(USCITA_REINDIRIZZAMENTO, "la pagina e' a ", posto, 0);
    }

    if (codice == 204 || codice == 304) return USCITA_FATTO;
    if (a_pezzi) passa_a_pezzi(&l);
    else if (misura >= 0) {
        if (passa(&l, misura) < misura) fallisci(USCITA_RISPOSTA, "risposta troncata: il server ha chiuso prima della fine", 0, 0);
    } else passa(&l, -1);
    close(l.s);
    return USCITA_FATTO;
}
