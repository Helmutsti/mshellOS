/* network: il servizio della rete. All'avvio accende la scheda di rete, le da' un
 * indirizzo, la strada verso il resto di Internet e chi traduce i nomi (il DNS), e poi
 * resta acceso per tenerla cosi'.
 *
 * LO FA PARTIRE INIT, come sistema (uid 0): per cambiare le schede di rete Linux vuole il
 * permesso CAP_NET_ADMIN, e per i pacchetti "grezzi" del DHCP CAP_NET_RAW. Se il servizio
 * cade, init lo fa ripartire, e lui rifa' tutto da capo (le stesse richieste danno lo
 * stesso risultato: si puo' ripartire quante volte si vuole).
 *
 * COSA FA, IN ORDINE:
 *   1. legge la configurazione, /system/config/network/network.conf (il sorgente e'
 *      utente/config/network/network.conf; build.sh lo mette nel disco). Una cosa per riga,
 *      "chiave=valore": quale scheda (interface=eth0) e come avere l'indirizzo (mode=dhcp,
 *      chiesto alla rete, oppure mode=static, con address, gateway e dns scritti li').
 *      Oggi c'e' solo questa, del sistema; quella della persona in /data verra' dopo;
 *   2. accende lo (la scheda "finta" del computer verso se stesso, 127.0.0.1: Linux le da'
 *      l'indirizzo da solo appena e' accesa) e la scheda configurata;
 *   3. con dhcp chiede l'indirizzo alla rete (vedi IL DHCP, sotto);
 *   4. da' l'indirizzo alla scheda e mette la strada predefinita verso il gateway, parlando
 *      con il kernel con rtnetlink (vedi IL KERNEL, sotto): e' quello che fa "ip" su Linux;
 *   5. scrive i DNS in /run/network/resolv.conf (vedi IL DNS, sotto);
 *   6. resta acceso: a meta' della durata del prestito dell'indirizzo lo rinnova; se il
 *      prestito scade senza rinnovo, toglie l'indirizzo e lo richiede da capo.
 *
 * SE LA SCHEDA NON C'E' (per esempio un computer senza rete) lo scrive una volta e
 * aspetta che compaia, guardando ogni 5 secondi: non esce, altrimenti init lo farebbe
 * ripartire in continuazione. Allo stesso modo, se Linux non gli da' i permessi per
 * cambiare le schede (nella sandbox Docker: la rete del container la prepara Docker, che
 * a chi sta dentro toglie CAP_NET_ADMIN), lo scrive una volta e resta fermo. La shell
 * sulla seriale, in init, fa una cosa simile ("dispositivo"), ma li' e' init a guardare un
 * file di /dev; qui la scheda la conosce solo il servizio, che ha letto la configurazione,
 * e quindi e' lui ad aspettare.
 *
 * IL DHCP. Chi entra in una rete non ha ancora un indirizzo: lo chiede gridando a tutti
 * (broadcast). Quattro messaggi, sulle porte UDP 68 (noi) e 67 (il server):
 *   DISCOVER  noi a tutti: "c'e' un server? mi serve un indirizzo"
 *   OFFER     il server: "ti propongo 10.0.2.15, con questa maschera, gateway e DNS"
 *   REQUEST   noi a tutti: "prendo quello di quel server" (cosi' gli altri server sanno)
 *   ACK       il server: "e' tuo per N secondi" (il prestito, "lease"); oppure NAK: "no"
 * Per rinnovare basta un REQUEST con l'indirizzo che si ha gia' (ciaddr) e un ACK.
 * I messaggi si mandano e si ricevono con una presa AF_PACKET (il modulo af_packet): si
 * scrivono a mano anche le intestazioni IP e UDP. Con una presa UDP normale non si puo':
 * finche' la scheda non ha un indirizzo, Linux non consegna le risposte, e per mandarle
 * "a tutti" da 0.0.0.0 servirebbero altri trucchi. E' come fanno udhcpc di busybox e
 * dhcpcd. Le risposte, il server le manda a tutti (gli chiediamo cosi', con il bit
 * BROADCAST) o alla nostra scheda: la presa AF_PACKET le vede in tutti e due i casi.
 *
 * IL KERNEL. Indirizzi e strade si danno al kernel con rtnetlink: una presa AF_NETLINK su
 * cui si mandano messaggi RTM_NEWADDR (un indirizzo a una scheda), RTM_DELADDR (toglierlo),
 * RTM_NEWROUTE (una strada); il kernel risponde a ognuno con un "fatto" o un errore.
 * L'accensione delle schede invece si fa con ioctl SIOCSIFFLAGS (IFF_UP), la via piu' corta.
 *
 * IL DNS. musl legge i server dei nomi da /etc/resolv.conf, ma "/" e' in sola lettura. Come
 * fa systemd-resolved: nel disco /etc/resolv.conf e' un collegamento simbolico a
 * /run/network/resolv.conf (build.sh), e il servizio scrive li' (/run e' in memoria: lo
 * monta avvio). Il file si scrive accanto e poi si rinomina, cosi' chi lo legge non lo
 * trova mai a meta'.
 *
 * Scrive nel registro righe brevi, come gli altri servizi, per esempio:
 *   network: eth0 10.0.2.15/24, gateway 10.0.2.2, dns 10.0.2.3 */
#include "libreria.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define CONFIGURAZIONE "/system/config/network/network.conf"
#define CARTELLA_DNS   "/run/network"
#define FILE_DNS       CARTELLA_DNS "/resolv.conf"
#define MAX_DNS        3          /* musl ne usa al massimo 3 (MAXNS) */
#define ETH_P_IP_      0x0800     /* i pacchetti IPv4 */

/* --- il registro: una riga alla volta --- */

static char riga[256];
static int lunga;

static void metti(const char *s) {
    while (*s && lunga < (int)sizeof riga - 1) riga[lunga++] = *s++;
}

static void metti_numero(unsigned long n) {
    char cifre[24];
    int k = 0;
    do { cifre[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k && lunga < (int)sizeof riga - 1) riga[lunga++] = cifre[--k];
}

/* un indirizzo IPv4, come lo tiene la rete (il primo byte e' il primo numero) */
static void metti_ip(uint32_t ip) {
    const unsigned char *b = (const unsigned char *)&ip;
    for (int i = 0; i < 4; i++) {
        if (i) metti(".");
        metti_numero(b[i]);
    }
}

static void fine_riga(void) {
    riga[lunga++] = '\n';
    scrivi(riga, lunga);
    lunga = 0;
}

static void di(const char *s) { metti("network: "); metti(s); fine_riga(); }

/* --- il tempo, in millisecondi da quando il computer e' acceso --- */

static long long adesso(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* Resta fermo per sempre: quando non c'e' piu' niente da fare, ma il servizio non deve
 * finire (init lo farebbe ripartire). */
static void __attribute__((noreturn)) resta_fermo(void) {
    for (;;) dormi(60 * 60 * 1000);
}

/* --- 1. la configurazione --- */

struct configurazione {
    char interfaccia[IF_NAMESIZE];
    int statica;                  /* 0: dhcp, 1: static */
    uint32_t indirizzo;           /* solo static */
    int prefisso;
    uint32_t gateway;             /* 0: nessuno */
    uint32_t dns[MAX_DNS];
    int quanti_dns;
};

/* "10.0.2.15" in un indirizzo; 0 se non lo e' */
static int leggi_ip(const char *testo, uint32_t *ip) {
    struct in_addr a;
    if (inet_pton(AF_INET, testo, &a) != 1) return 0;
    *ip = a.s_addr;
    return 1;
}

/* Una riga "chiave=valore" (gia' senza spazi ai lati). 0 se non si capisce. */
static int leggi_riga(struct configurazione *c, char *chiave, char *valore) {
    if (!strcmp(chiave, "interface")) {
        if (!*valore || strlen(valore) >= sizeof c->interfaccia) return 0;
        strcpy(c->interfaccia, valore);
        return 1;
    }
    if (!strcmp(chiave, "mode")) {
        if (!strcmp(valore, "dhcp")) c->statica = 0;
        else if (!strcmp(valore, "static")) c->statica = 1;
        else return 0;
        return 1;
    }
    if (!strcmp(chiave, "address")) {
        char *barra = strchr(valore, '/');
        if (!barra) return 0;
        *barra = 0;
        char *fine;
        long p = strtol(barra + 1, &fine, 10);
        if (*fine || barra[1] == 0 || p < 0 || p > 32) return 0;
        c->prefisso = (int)p;
        return leggi_ip(valore, &c->indirizzo);
    }
    if (!strcmp(chiave, "gateway")) return leggi_ip(valore, &c->gateway);
    if (!strcmp(chiave, "dns")) {
        if (c->quanti_dns == MAX_DNS) return 0;
        return leggi_ip(valore, &c->dns[c->quanti_dns++]);
    }
    return 0;
}

static char *senza_spazi(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *f = s + strlen(s);
    while (f > s && (f[-1] == ' ' || f[-1] == '\t' || f[-1] == '\r')) *--f = 0;
    return s;
}

/* Legge il file. 0 se e' tutto a posto; le righe che non capisce le scrive nel registro
 * e le salta. */
static int leggi_configurazione(struct configurazione *c) {
    memset(c, 0, sizeof *c);
    int d = open(CONFIGURAZIONE, O_RDONLY | O_CLOEXEC);
    if (d < 0) return -1;
    static char testo[4096];
    int n = 0, letti;
    while (n < (int)sizeof testo - 1 && (letti = (int)read(d, testo + n, sizeof testo - 1 - (size_t)n)) > 0)
        n += letti;
    close(d);
    testo[n] = 0;
    for (char *r = testo, *dopo; r; r = dopo) {
        dopo = strchr(r, '\n');
        if (dopo) *dopo++ = 0;
        char *s = senza_spazi(r);
        if (!*s || *s == '#') continue;
        char *uguale = strchr(s, '=');
        int capita = 0;
        if (uguale) {
            *uguale = 0;
            capita = leggi_riga(c, senza_spazi(s), senza_spazi(uguale + 1));
            if (!capita) *uguale = '=';
        }
        if (!capita) {
            metti("network: in " CONFIGURAZIONE " non capisco la riga \"");
            metti(s);
            metti("\", la salto");
            fine_riga();
        }
    }
    return 0;
}

/* --- 2. le schede --- */

/* Accende la scheda (IFF_UP). 0, o -errno. */
static int accendi(const char *nome) {
    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return -errno;
    struct ifreq r;
    memset(&r, 0, sizeof r);
    strncpy(r.ifr_name, nome, IF_NAMESIZE - 1);
    int esito = 0;
    if (ioctl(s, SIOCGIFFLAGS, &r) < 0) esito = -errno;
    else if (!(r.ifr_flags & IFF_UP)) {
        r.ifr_flags |= IFF_UP;
        if (ioctl(s, SIOCSIFFLAGS, &r) < 0) esito = -errno;
    }
    close(s);
    return esito;
}

/* L'indirizzo fisico (MAC) della scheda, 6 byte. 0, o -errno. */
static int indirizzo_fisico(const char *nome, unsigned char mac[6]) {
    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return -errno;
    struct ifreq r;
    memset(&r, 0, sizeof r);
    strncpy(r.ifr_name, nome, IF_NAMESIZE - 1);
    int esito = ioctl(s, SIOCGIFHWADDR, &r) < 0 ? -errno : 0;
    close(s);
    if (!esito) memcpy(mac, r.ifr_hwaddr.sa_data, 6);
    return esito;
}

/* Un errore da cui non si esce da soli: lo si scrive e si resta fermi. */
static void __attribute__((noreturn)) fermo(const char *cosa, const char *nome, int errore) {
    metti("network: ");
    metti(cosa);
    metti(" ");
    metti(nome);
    metti(": ");
    metti(strerror(-errore));
    metti(", resto fermo");
    fine_riga();
    resta_fermo();
}

/* Linux ci lascia cambiare le schede (CAP_NET_ADMIN) e mandare pacchetti grezzi
 * (CAP_NET_RAW)? Si guarda prima di toccare qualcosa: senza, non si potrebbe fare niente. */
static int permessi_di_rete(void) {
    struct __user_cap_header_struct h = { _LINUX_CAPABILITY_VERSION_3, 0 };
    struct __user_cap_data_struct d[2];
    if (syscall(SYS_capget, &h, d) < 0) return 0;
    unsigned int servono = 1u << CAP_NET_ADMIN | 1u << CAP_NET_RAW;
    return (d[0].effective & servono) == servono;
}

/* --- 4. il kernel: rtnetlink --- */

static int presa_kernel = -1;
static unsigned int sequenza;

union messaggio {
    struct nlmsghdr h;
    char byte[512];
};

static void attributo(struct nlmsghdr *h, int tipo, const void *dati, int byte) {
    struct rtattr *a = (struct rtattr *)((char *)h + NLMSG_ALIGN(h->nlmsg_len));
    a->rta_type = (unsigned short)tipo;
    a->rta_len = (unsigned short)RTA_LENGTH(byte);
    memcpy(RTA_DATA(a), dati, (size_t)byte);
    h->nlmsg_len = NLMSG_ALIGN(h->nlmsg_len) + RTA_ALIGN(a->rta_len);
}

/* Manda la richiesta e aspetta la risposta del kernel. 0, o -errno. */
static int chiedi_al_kernel(struct nlmsghdr *h) {
    if (presa_kernel < 0) {
        presa_kernel = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
        if (presa_kernel < 0) return -errno;
    }
    h->nlmsg_flags |= NLM_F_REQUEST | NLM_F_ACK;
    h->nlmsg_seq = ++sequenza;
    struct sockaddr_nl kernel = { .nl_family = AF_NETLINK };
    if (sendto(presa_kernel, h, h->nlmsg_len, 0, (struct sockaddr *)&kernel, sizeof kernel) < 0)
        return -errno;
    union messaggio risposta;
    for (;;) {
        ssize_t letti = recv(presa_kernel, risposta.byte, sizeof risposta.byte, 0);
        if (letti < 0) {
            if (errno == EINTR) continue;
            return -errno;
        }
        unsigned int n = (unsigned int)letti;
        for (struct nlmsghdr *r = &risposta.h; NLMSG_OK(r, n); r = NLMSG_NEXT(r, n))
            if (r->nlmsg_seq == sequenza && r->nlmsg_type == NLMSG_ERROR)
                return ((struct nlmsgerr *)NLMSG_DATA(r))->error;
    }
}

static uint32_t maschera(int prefisso) {
    return prefisso ? htonl(0xffffffffu << (32 - prefisso)) : 0;
}

/* Da' (RTM_NEWADDR) o toglie (RTM_DELADDR) un indirizzo alla scheda. 0, o -errno. */
static int indirizzo(int tipo, int scheda, uint32_t ip, int prefisso) {
    union messaggio m;
    memset(&m, 0, sizeof m);
    m.h.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    m.h.nlmsg_type = (unsigned short)tipo;
    if (tipo == RTM_NEWADDR) m.h.nlmsg_flags = NLM_F_CREATE | NLM_F_REPLACE;
    struct ifaddrmsg *a = NLMSG_DATA(&m.h);
    a->ifa_family = AF_INET;
    a->ifa_prefixlen = (unsigned char)prefisso;
    a->ifa_scope = RT_SCOPE_UNIVERSE;
    a->ifa_index = (unsigned int)scheda;
    attributo(&m.h, IFA_LOCAL, &ip, 4);
    attributo(&m.h, IFA_ADDRESS, &ip, 4);
    if (tipo == RTM_NEWADDR && prefisso < 31) {
        uint32_t tutti = ip | ~maschera(prefisso);   /* l'indirizzo "a tutti" della rete */
        attributo(&m.h, IFA_BROADCAST, &tutti, 4);
    }
    return chiedi_al_kernel(&m.h);
}

/* La strada predefinita (0.0.0.0/0) verso il gateway, dalla scheda. 0, o -errno. */
static int strada_predefinita(int scheda, uint32_t gateway, int da_dhcp) {
    union messaggio m;
    memset(&m, 0, sizeof m);
    m.h.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    m.h.nlmsg_type = RTM_NEWROUTE;
    m.h.nlmsg_flags = NLM_F_CREATE | NLM_F_REPLACE;
    struct rtmsg *r = NLMSG_DATA(&m.h);
    r->rtm_family = AF_INET;
    r->rtm_dst_len = 0;
    r->rtm_table = RT_TABLE_MAIN;
    r->rtm_protocol = da_dhcp ? RTPROT_DHCP : RTPROT_STATIC;
    r->rtm_scope = RT_SCOPE_UNIVERSE;
    r->rtm_type = RTN_UNICAST;
    attributo(&m.h, RTA_GATEWAY, &gateway, 4);
    attributo(&m.h, RTA_OIF, &scheda, 4);
    return chiedi_al_kernel(&m.h);
}

/* --- 5. il DNS --- */

static void scrivi_dns(const uint32_t *dns, int quanti) {
    if (mkdir(CARTELLA_DNS, 0755) < 0 && errno != EEXIST) {
        metti("network: non riesco a creare " CARTELLA_DNS ": ");
        metti(strerror(errno));
        fine_riga();
        return;
    }
    lunga = 0;
    metti("# scritto dal servizio network\n");
    for (int i = 0; i < quanti; i++) {
        metti("nameserver ");
        metti_ip(dns[i]);
        metti("\n");
    }
    int d = open(FILE_DNS ".nuovo", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    int bene = d >= 0 && fchmod(d, 0644) == 0 && write(d, riga, (size_t)lunga) == lunga;
    if (d >= 0) close(d);
    lunga = 0;
    if (!bene || rename(FILE_DNS ".nuovo", FILE_DNS) < 0) {
        metti("network: non riesco a scrivere " FILE_DNS ": ");
        metti(strerror(errno));
        fine_riga();
    }
}

/* --- 3. il DHCP --- */

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5
#define DHCP_NAK      6

struct dhcp {
    uint8_t op, htype, hlen, hops;
    uint32_t xid;
    uint16_t secs, flags;
    uint32_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic;
    uint8_t opzioni[308];
} __attribute__((packed));

struct intestazione_ip {
    uint8_t versione, servizio;
    uint16_t lunghezza, numero, frammento;
    uint8_t ttl, protocollo;
    uint16_t somma;
    uint32_t da, a;
} __attribute__((packed));

struct intestazione_udp {
    uint16_t da, a, lunghezza, somma;
} __attribute__((packed));

struct pacchetto {
    struct intestazione_ip ip;
    struct intestazione_udp udp;
    struct dhcp d;
} __attribute__((packed));

#define MAGICO 0x63825363u   /* i 4 byte prima delle opzioni */

/* Quello che il server ci ha dato. */
struct prestito {
    uint32_t indirizzo;
    int prefisso;
    uint32_t gateway;
    uint32_t dns[MAX_DNS];
    int quanti_dns;
    uint32_t server;
    uint32_t durata, t1;          /* secondi; durata 0xffffffff = per sempre */
};

static int presa_dhcp = -1;
static int scheda;                 /* il numero della scheda (ifindex) */
static unsigned char mac[6];
static uint32_t xid;

/* La somma di controllo dell'intestazione IP. */
static uint16_t somma_ip(const void *dati, int byte) {
    const uint16_t *p = dati;
    uint32_t s = 0;
    for (; byte > 1; byte -= 2) s += *p++;
    while (s >> 16) s = (s & 0xffff) + (s >> 16);
    return (uint16_t)~s;
}

/* Prepara e manda un messaggio DHCP "a tutti". ciaddr: il nostro indirizzo, se ne abbiamo
 * uno (rinnovo); chiesto e server: quelli dell'OFFER (REQUEST dopo un DISCOVER). */
static int manda_dhcp(int tipo, uint32_t ciaddr, uint32_t chiesto, uint32_t server) {
    static struct pacchetto p;
    memset(&p, 0, sizeof p);
    struct dhcp *d = &p.d;
    d->op = 1;                     /* richiesta */
    d->htype = 1;                  /* Ethernet */
    d->hlen = 6;
    d->xid = xid;
    d->flags = ciaddr ? 0 : htons(0x8000);   /* BROADCAST: rispondi a tutti, non ho un indirizzo */
    d->ciaddr = ciaddr;
    memcpy(d->chaddr, mac, 6);
    d->magic = htonl(MAGICO);
    uint8_t *o = d->opzioni;
    *o++ = 53; *o++ = 1; *o++ = (uint8_t)tipo;          /* che messaggio e' */
    if (chiesto) { *o++ = 50; *o++ = 4; memcpy(o, &chiesto, 4); o += 4; }
    if (server)  { *o++ = 54; *o++ = 4; memcpy(o, &server, 4); o += 4; }
    /* cosa ci interessa: maschera, gateway, DNS, durata, server, T1 */
    *o++ = 55; *o++ = 6; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 51; *o++ = 54; *o++ = 58;
    *o++ = 255;                    /* fine */

    p.udp.da = htons(68);
    p.udp.a = htons(67);
    p.udp.lunghezza = htons(sizeof p.udp + sizeof p.d);
    p.udp.somma = 0;               /* facoltativa in IPv4 */
    p.ip.versione = 0x45;          /* IPv4, intestazione di 5 parole da 4 byte */
    p.ip.lunghezza = htons(sizeof p);
    p.ip.ttl = 64;
    p.ip.protocollo = 17;          /* UDP */
    p.ip.da = ciaddr;
    p.ip.a = 0xffffffffu;          /* 255.255.255.255 */
    p.ip.somma = somma_ip(&p.ip, sizeof p.ip);

    struct sockaddr_ll dove;
    memset(&dove, 0, sizeof dove);
    dove.sll_family = AF_PACKET;
    dove.sll_protocol = htons(ETH_P_IP_);
    dove.sll_ifindex = scheda;
    dove.sll_halen = 6;
    memset(dove.sll_addr, 0xff, 6);   /* a tutte le schede della rete */
    if (sendto(presa_dhcp, &p, sizeof p, 0, (struct sockaddr *)&dove, sizeof dove) < 0)
        return -errno;
    return 0;
}

/* Legge le opzioni di una risposta. Restituisce il tipo di messaggio (OFFER, ACK, NAK). */
static int leggi_opzioni(const struct dhcp *d, int byte, struct prestito *p) {
    memset(p, 0, sizeof *p);
    p->indirizzo = d->yiaddr;
    p->durata = 0xffffffffu;
    int tipo = 0;
    const uint8_t *o = d->opzioni, *fine = (const uint8_t *)d + byte;
    while (o < fine && *o != 255) {
        if (*o == 0) { o++; continue; }
        if (o + 2 > fine || o + 2 + o[1] > fine) break;
        int codice = o[0], n = o[1];
        const uint8_t *v = o + 2;
        uint32_t x = 0;
        if (n >= 4) memcpy(&x, v, 4);
        switch (codice) {
        case 53: if (n >= 1) tipo = v[0]; break;
        case 1:  if (n == 4) { uint32_t m = ntohl(x); int k = 0; while (m & 0x80000000u) { k++; m <<= 1; } p->prefisso = k; } break;
        case 3:  if (n >= 4) p->gateway = x; break;
        case 6:  for (int i = 0; i + 4 <= n && p->quanti_dns < MAX_DNS; i += 4) memcpy(&p->dns[p->quanti_dns++], v + i, 4); break;
        case 51: if (n == 4) p->durata = ntohl(x); break;
        case 54: if (n == 4) p->server = x; break;
        case 58: if (n == 4) p->t1 = ntohl(x); break;
        }
        o += 2 + n;
    }
    if (!p->prefisso) p->prefisso = 24;   /* senza maschera: quella piu' comune */
    return tipo;
}

/* Aspetta, fino a 'scadenza' (ms), una risposta al nostro xid. Il tipo, o 0 se non arriva. */
static int aspetta_risposta(long long scadenza, struct prestito *p) {
    static unsigned char b[2048];
    for (;;) {
        long long resta = scadenza - adesso();
        if (resta <= 0) return 0;
        struct pollfd q = { presa_dhcp, POLLIN, 0 };
        if (poll(&q, 1, (int)resta) <= 0) continue;
        int n = (int)recv(presa_dhcp, b, sizeof b, 0);
        if (n < (int)(sizeof(struct intestazione_ip) + sizeof(struct intestazione_udp))) continue;
        int lung_ip = (b[0] & 15) * 4;
        if ((b[0] >> 4) != 4 || b[9] != 17 || lung_ip < 20) continue;      /* IPv4 e UDP */
        if (n < lung_ip + 8 + 240) continue;
        const struct intestazione_udp *u = (const void *)(b + lung_ip);
        if (u->a != htons(68)) continue;                                  /* a noi */
        static struct dhcp d;
        int byte = n - lung_ip - 8;
        if (byte > (int)sizeof d) byte = sizeof d;
        memset(&d, 0, sizeof d);
        memcpy(&d, b + lung_ip + 8, (size_t)byte);
        if (d.op != 2 || d.xid != xid || d.magic != htonl(MAGICO)) continue;
        int tipo = leggi_opzioni(&d, byte, p);
        if (tipo) return tipo;
    }
}

static void nuovo_xid(void) {
    if (getrandom(&xid, sizeof xid, 0) != sizeof xid) xid = (uint32_t)adesso() ^ (uint32_t)mac[5] << 24;
}

/* DISCOVER, OFFER, REQUEST, ACK. 1 se si e' avuto un indirizzo. */
static int chiedi_indirizzo(struct prestito *p) {
    nuovo_xid();
    struct prestito offerta;
    int offerto = 0;
    for (int volta = 0; volta < 4 && !offerto; volta++) {   /* aspetta 2, 4, 8, 16 secondi */
        manda_dhcp(DHCP_DISCOVER, 0, 0, 0);
        long long scadenza = adesso() + (2000LL << volta);
        int tipo;
        while ((tipo = aspetta_risposta(scadenza, &offerta)) != 0)
            if (tipo == DHCP_OFFER && offerta.indirizzo) { offerto = 1; break; }
    }
    if (!offerto) return 0;
    for (int volta = 0; volta < 3; volta++) {
        manda_dhcp(DHCP_REQUEST, 0, offerta.indirizzo, offerta.server);
        long long scadenza = adesso() + 4000;
        int tipo;
        while ((tipo = aspetta_risposta(scadenza, p)) != 0) {
            if (tipo == DHCP_ACK && p->indirizzo == offerta.indirizzo) return 1;
            if (tipo == DHCP_NAK) return 0;
        }
    }
    return 0;
}

/* Il rinnovo: REQUEST con il nostro indirizzo. 1: ACK; -1: NAK; 0: nessuna risposta. */
static int rinnova(const struct prestito *vecchio, struct prestito *p) {
    nuovo_xid();
    manda_dhcp(DHCP_REQUEST, vecchio->indirizzo, 0, 0);
    long long scadenza = adesso() + 4000;
    int tipo;
    while ((tipo = aspetta_risposta(scadenza, p)) != 0) {
        if (tipo == DHCP_ACK && p->indirizzo == vecchio->indirizzo) return 1;
        if (tipo == DHCP_NAK) return -1;
    }
    return 0;
}

/* --- mettere tutto al suo posto --- */

static const char *nome_scheda;

/* Da' alla scheda indirizzo, strada e DNS, e lo scrive nel registro se e' cambiato
 * qualcosa rispetto a 'prima' (0: la prima volta). */
static void applica(const struct prestito *p, const struct prestito *prima, int da_dhcp) {
    int cambiato = !prima || prima->indirizzo != p->indirizzo || prima->prefisso != p->prefisso ||
                   prima->gateway != p->gateway || prima->quanti_dns != p->quanti_dns ||
                   memcmp(prima->dns, p->dns, sizeof p->dns) != 0;
    if (prima && prima->indirizzo &&
        (prima->indirizzo != p->indirizzo || prima->prefisso != p->prefisso))
        indirizzo(RTM_DELADDR, scheda, prima->indirizzo, prima->prefisso);
    int e = indirizzo(RTM_NEWADDR, scheda, p->indirizzo, p->prefisso);
    if (e < 0) {
        metti("network: non riesco a dare l'indirizzo a ");
        metti(nome_scheda);
        metti(": ");
        metti(strerror(-e));
        fine_riga();
    }
    if (p->gateway && (e = strada_predefinita(scheda, p->gateway, da_dhcp)) < 0) {
        metti("network: non riesco a mettere la strada verso ");
        metti_ip(p->gateway);
        metti(": ");
        metti(strerror(-e));
        fine_riga();
    }
    if (cambiato) scrivi_dns(p->dns, p->quanti_dns);
    if (!cambiato) return;
    metti("network: ");
    metti(nome_scheda);
    metti(" ");
    metti_ip(p->indirizzo);
    metti("/");
    metti_numero((unsigned long)p->prefisso);
    if (p->gateway) { metti(", gateway "); metti_ip(p->gateway); }
    for (int i = 0; i < p->quanti_dns; i++) { metti(i ? " " : ", dns "); metti_ip(p->dns[i]); }
    fine_riga();
}

/* Toglie l'indirizzo (le strade che passano di li' le toglie il kernel da solo). */
static void togli_indirizzo(const struct prestito *p) {
    indirizzo(RTM_DELADDR, scheda, p->indirizzo, p->prefisso);
    scrivi_dns(0, 0);
}

/* Aspetta fino al momento 'quando' (ms). */
static void aspetta_fino(long long quando) {
    for (long long resta; (resta = quando - adesso()) > 0;)
        dormi(resta > 60 * 60 * 1000 ? 60 * 60 * 1000 : (unsigned int)resta);
}

/* Con dhcp: chiede, mette, rinnova, per sempre. */
static void __attribute__((noreturn)) con_dhcp(void) {
    presa_dhcp = socket(AF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, htons(ETH_P_IP_));
    if (presa_dhcp < 0) fermo("non riesco ad aprire la presa per il DHCP su", nome_scheda, -errno);
    struct sockaddr_ll qui;
    memset(&qui, 0, sizeof qui);
    qui.sll_family = AF_PACKET;
    qui.sll_protocol = htons(ETH_P_IP_);
    qui.sll_ifindex = scheda;
    if (bind(presa_dhcp, (struct sockaddr *)&qui, sizeof qui) < 0)
        fermo("non riesco ad ascoltare il DHCP su", nome_scheda, -errno);

    struct prestito p, nuovo;
    int detto = 0;
    for (;;) {
        if (!chiedi_indirizzo(&p)) {
            if (!detto) {
                metti("network: ");
                metti(nome_scheda);
                metti(": nessuna risposta dal DHCP, riprovo");
                fine_riga();
                detto = 1;
            }
            dormi(10 * 1000);
            continue;
        }
        detto = 0;
        applica(&p, 0, 1);
        /* il prestito: rinnovo a T1 (se il server non lo dice, a meta'); se nessuno
         * risponde si riprova ogni tanto (a meta' del tempo che resta, almeno un minuto)
         * finche' non scade */
        for (;;) {
            if (p.durata == 0xffffffffu) resta_fermo();      /* per sempre */
            long long inizio = adesso();
            long long scade = inizio + (long long)p.durata * 1000;
            long long t1 = p.t1 && p.t1 < p.durata ? p.t1 : p.durata / 2;
            aspetta_fino(inizio + t1 * 1000);
            int esito = 0;
            while (adesso() < scade && (esito = rinnova(&p, &nuovo)) == 0) {
                long long resta = scade - adesso(), pausa = resta / 2;
                if (pausa < 60000) pausa = 60000;
                if (pausa > resta) pausa = resta;
                aspetta_fino(adesso() + pausa);
            }
            if (esito == 1) {
                applica(&nuovo, &p, 1);
                p = nuovo;
                continue;
            }
            metti("network: ");
            metti(nome_scheda);
            metti(esito < 0 ? ": il DHCP non rinnova l'indirizzo, lo richiedo"
                            : ": l'indirizzo e' scaduto, lo richiedo");
            fine_riga();
            togli_indirizzo(&p);
            break;
        }
    }
}

int main(void) {
    struct configurazione c;
    if (leggi_configurazione(&c) < 0) {
        di("manca " CONFIGURAZIONE ", resto fermo");
        resta_fermo();
    }
    if (!c.interfaccia[0]) {
        di("in " CONFIGURAZIONE " manca interface=, resto fermo");
        resta_fermo();
    }
    if (c.statica && !c.indirizzo) {
        di("con mode=static serve address=, resto fermo");
        resta_fermo();
    }
    nome_scheda = c.interfaccia;
    if (!permessi_di_rete()) {
        di("Linux non mi lascia gestire le schede di rete (CAP_NET_ADMIN, CAP_NET_RAW), resto fermo");
        resta_fermo();
    }

    int e = accendi("lo");
    if (e < 0) fermo("non riesco ad accendere", "lo", e);

    /* la scheda: se non c'e', si aspetta che compaia */
    int detto = 0;
    while ((scheda = (int)if_nametoindex(nome_scheda)) == 0) {
        if (!detto) {
            metti("network: manca la scheda ");
            metti(nome_scheda);
            metti(", aspetto che compaia");
            fine_riga();
            detto = 1;
        }
        dormi(5000);
    }
    if ((e = accendi(nome_scheda)) < 0) fermo("non riesco ad accendere", nome_scheda, e);
    if ((e = indirizzo_fisico(nome_scheda, mac)) < 0) fermo("non trovo l'indirizzo fisico di", nome_scheda, e);

    if (!c.statica) con_dhcp();

    struct prestito p;
    memset(&p, 0, sizeof p);
    p.indirizzo = c.indirizzo;
    p.prefisso = c.prefisso;
    p.gateway = c.gateway;
    memcpy(p.dns, c.dns, sizeof p.dns);
    p.quanti_dns = c.quanti_dns;
    applica(&p, 0, 0);
    resta_fermo();
}
