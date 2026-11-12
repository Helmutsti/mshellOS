/* eventi, nella SANDBOX: al posto di /dev/input, una presa TCP.
 *
 * Nel container non ci sono tastiera e mouse (ne' /dev/input, ne' uinput per inventarli).
 * Questo programma prende il posto di eventi (/system/services/eventi: init lo fa partire
 * e gli presta il tubo verso il compositore, come sempre) e USA IL CODICE DI eventi.c,
 * incluso qui sotto cosi' com'e': la tastiera italiana, la scala della tavoletta, i
 * pacchetti (SYN_REPORT), la rotella. Cambia solo da dove arrivano gli eventi di Linux:
 *
 *   - ci sono due dispositivi finti, una TASTIERA e una TAVOLETTA (assi da 0 a 32767 e
 *     rotella, come la usb-tablet di QEMU). Il "file" di ognuno e' un tubo: ci si scrivono
 *     gli eventi (struct input_event, quelli che darebbe Linux) e leggi_dispositivo di
 *     eventi.c li legge come da /dev/input;
 *   - gli eventi arrivano da chi fa le prove (strumenti/sandbox/sandbox.py) sulla presa TCP
 *     SANDBOX_PORTA. Un tasto tenuto giu' lo ripete questo programma, come farebbe Linux
 *     (dopo RIPETI_DOPO ms, poi ogni RIPETI_OGNI: i valori chiesti da eventi.c).
 *
 * LA PRESA (un solo collegamento alla volta; uno nuovo prende il posto del vecchio).
 * Chi si collega manda comandi di un byte:
 *   'e' + dispositivo (1 byte: 0 tastiera, 1 tavoletta) + tipo (2) + codice (2) + valore (4,
 *       con segno): un evento di Linux, i numeri in little endian. Nessuna risposta.
 *   'f' la foto dello schermo: risponde 'F', larghezza e altezza (4 byte l'una), poi i pixel
 *       come li ha il compositore (4 byte per pixel, 0x00RRGGBB in little endian: B G R X).
 *   'p' "ci sei?": risponde 'P' quando tutto quello che e' arrivato prima e' stato passato
 *       al compositore. */
#define main main_di_eventi_vero
#include "../../utente/programmi/eventi.c"
#undef main

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>

#include "sandbox.h"

#define TASTIERA 0
#define TAVOLETTA 1

static int scrive_in[2];               /* il capo da cui si scrive nel "file" di ognuno */
static unsigned int tasto_ripetuto;    /* 0: nessuno */
static long long prossima_ripetizione; /* in ms (CLOCK_MONOTONIC) */

static long long adesso_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* Un dispositivo finto: il suo "file" e' un tubo, lui legge il capo di lettura. */
static void prepara(int quale, int tavoletta) {
    int tubo[2];
    if (pipe2(tubo, O_NONBLOCK | O_CLOEXEC) < 0) esci();
    struct dispositivo *p = &dispositivi[quale];
    *p = (struct dispositivo){ .d = tubo[0], .numero = (dev_t)(quale + 1) };
    if (tavoletta) {
        p->assoluto = p->relativo = 1;
        p->massimo[0] = p->massimo[1] = 32767;
    }
    scrive_in[quale] = tubo[1];
}

/* Un evento di Linux sul dispositivo: lo si scrive nel suo "file" e lo si fa leggere a
 * eventi.c (che, alla fine di un pacchetto, lo manda al compositore). */
static void evento_linux(int quale, unsigned int tipo, unsigned int codice, int valore) {
    struct input_event e = { .type = (unsigned short)tipo, .code = (unsigned short)codice, .value = valore };
    if (write(scrive_in[quale], &e, sizeof e) != (ssize_t)sizeof e) return;
    leggi_dispositivo(&dispositivi[quale]);
    if (dispositivi[quale].d < 0) esci();                 /* non dovrebbe succedere mai */
    /* la ripetizione dei tasti, come la fa Linux: l'ultimo tasto premuto, finche' e' giu' */
    if (quale == TASTIERA && tipo == EV_KEY) {
        if (valore == 1) {
            tasto_ripetuto = codice;
            prossima_ripetizione = adesso_ms() + RIPETI_DOPO;
        } else if (valore == 0 && codice == tasto_ripetuto) {
            tasto_ripetuto = 0;
        }
    }
}

static void ripeti_se_serve(void) {
    if (!tasto_ripetuto || adesso_ms() < prossima_ripetizione) return;
    unsigned int codice = tasto_ripetuto;
    evento_linux(TASTIERA, EV_KEY, codice, 2);
    evento_linux(TASTIERA, EV_SYN, SYN_REPORT, 0);
    tasto_ripetuto = codice;
    prossima_ripetizione = adesso_ms() + RIPETI_OGNI;
}

/* --- la presa --- */

static int manda_tutto(int d, const void *dati, size_t byte) {
    const char *p = dati;
    while (byte) {
        ssize_t n = send(d, p, byte, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        p += n;
        byte -= (size_t)n;
    }
    return 0;
}

static const unsigned char *schermo;   /* il file dello schermo, nella nostra memoria */

static int manda_foto(int d) {
    if (!schermo) {
        int f = open(SANDBOX_SCHERMO, O_RDONLY | O_CLOEXEC);
        if (f >= 0) {
            void *m = mmap(0, SANDBOX_BYTE, PROT_READ, MAP_SHARED, f, 0);
            if (m != MAP_FAILED) schermo = m;
            close(f);
        }
    }
    unsigned char testa[9] = { 'F' };
    unsigned int misure[2] = { SANDBOX_LARGHEZZA, SANDBOX_ALTEZZA };
    if (!schermo) misure[0] = misure[1] = 0;            /* non c'e' ancora: una foto vuota */
    memcpy(testa + 1, misure, sizeof misure);
    if (manda_tutto(d, testa, sizeof testa) < 0) return -1;
    return schermo ? manda_tutto(d, schermo, SANDBOX_BYTE) : 0;
}

static int16_t numero16(const unsigned char *b) { return (int16_t)(b[0] | b[1] << 8); }
static int32_t numero32(const unsigned char *b) {
    return (int32_t)((uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
}

/* Serve i comandi interi che ci sono in 'b' (n byte). Torna quanti byte ha usato, o -1
 * se il collegamento va chiuso. */
static int servi_comandi(int d, unsigned char *b, int n) {
    int k = 0;
    while (k < n) {
        if (b[k] == 'e') {
            if (n - k < 10) break;                       /* il resto arrivera' */
            int quale = b[k + 1] == TAVOLETTA ? TAVOLETTA : TASTIERA;
            evento_linux(quale, (unsigned short)numero16(b + k + 2), (unsigned short)numero16(b + k + 4),
                         numero32(b + k + 6));
            k += 10;
        } else if (b[k] == 'f') {
            if (manda_foto(d) < 0) return -1;
            k++;
        } else if (b[k] == 'p') {
            if (manda_tutto(d, "P", 1) < 0) return -1;
            k++;
        } else {
            return -1;                                   /* non si capisce: si chiude */
        }
    }
    return k;
}

int main(void) {
    prepara(TASTIERA, 0);
    prepara(TAVOLETTA, 1);
    for (int i = 2; i < MAX_DISPOSITIVI; i++) dispositivi[i].d = -1;
    scrivi_dispositivo("tastiera", "sandbox", "presa TCP");
    scrivi_dispositivo("tavoletta", "sandbox", "presa TCP");

    int ascolto = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int si = 1;
    setsockopt(ascolto, SOL_SOCKET, SO_REUSEADDR, &si, sizeof si);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(SANDBOX_PORTA),
                             .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (ascolto < 0 || bind(ascolto, (struct sockaddr *)&a, sizeof a) < 0 || listen(ascolto, 4) < 0) {
        scrivi_testo("eventi: non riesco ad ascoltare sulla presa della sandbox\n");
        return 1;
    }

    int cliente = -1;
    static unsigned char b[65536];
    int pieni = 0;
    for (;;) {
        struct pollfd g[2] = { { ascolto, POLLIN, 0 }, { cliente, POLLIN, 0 } };
        int attesa = -1;
        if (tasto_ripetuto) {
            long long manca = prossima_ripetizione - adesso_ms();
            attesa = manca < 0 ? 0 : (int)manca;
        }
        if (poll(g, cliente >= 0 ? 2 : 1, attesa) < 0 && errno != EINTR) return 1;
        ripeti_se_serve();
        if (g[0].revents & POLLIN) {
            int nuovo = accept4(ascolto, 0, 0, SOCK_CLOEXEC);
            if (nuovo >= 0) {
                if (cliente >= 0) close(cliente);
                cliente = nuovo;
                pieni = 0;
                setsockopt(cliente, IPPROTO_TCP, TCP_NODELAY, &si, sizeof si);
            }
            continue;
        }
        if (cliente >= 0 && g[1].revents) {
            ssize_t n = recv(cliente, b + pieni, sizeof b - (size_t)pieni, 0);
            if (n < 0 && errno == EINTR) continue;
            int usati = n > 0 ? servi_comandi(cliente, b, pieni + (int)n) : -1;
            if (usati < 0) {
                close(cliente);
                cliente = -1;
                pieni = 0;
                continue;
            }
            pieni += (int)n - usati;
            memmove(b, b + usati, (size_t)pieni);
        }
    }
}
