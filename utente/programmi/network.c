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
 *      E' solo questa, quella del sistema: il servizio non ne ha una della persona (e' in sola lettura, vedi LA PORTA DI CONTROLLO);
 *   2. mette il firewall, con le regole di /system/config/network/firewall.conf (vedi IL
 *      FIREWALL, sotto). PRIMA di accendere le schede: la rete non e' mai accesa senza.
 *      Se il kernel non accetta le regole, lo scrive e lascia la rete spenta;
 *   3. accende lo (la scheda "finta" del computer verso se stesso, 127.0.0.1: Linux le da'
 *      l'indirizzo da solo appena e' accesa) e la scheda configurata;
 *   4. con dhcp chiede l'indirizzo alla rete (vedi IL DHCP, sotto);
 *   5. da' l'indirizzo alla scheda e mette la strada predefinita verso il gateway, parlando
 *      con il kernel con rtnetlink (vedi IL KERNEL, sotto): e' quello che fa "ip" su Linux;
 *   6. scrive i DNS in /run/network/resolv.conf (vedi IL DNS, sotto);
 *   7. resta acceso: a meta' della durata del prestito dell'indirizzo lo rinnova; se il
 *      prestito scade senza rinnovo, toglie l'indirizzo e lo richiede da capo.
 *
 * IL FIREWALL. Il filtro dei pacchetti e' quello del kernel, nftables; il servizio e'
 * l'unico che lo tocca (SISTEMA.md). Mette una tabella "mshell" della famiglia inet (vale
 * per IPv4 e IPv6 insieme) con tre catene, come fanno Mac e Windows a casa:
 *   input    i pacchetti per noi. Con incoming=block la politica e' "scarta", e passano
 *            solo: quelli da noi a noi (lo), le risposte alle connessioni che abbiamo
 *            aperto noi (ct state established,related: il kernel tiene il conto delle
 *            connessioni, il "conntrack"), l'ICMPv6 senza cui IPv6 non funziona e, con
 *            ping=allow, il ping (ICMP echo-request). Chi prova a collegarsi da fuori non
 *            riceve niente, nemmeno un rifiuto. Con incoming=allow passa tutto;
 *   forward  i pacchetti che passerebbero da noi verso altri: con forward=block, nessuno;
 *   output   i pacchetti che mandiamo: con outgoing=allow, tutti. Con outgoing=block solo
 *            lo e le risposte (per chi vuole un computer che non apre connessioni).
 * Il file e' "chiave=valore" come network.conf: incoming, outgoing, forward, ping, ognuna
 * allow o block. Se manca il file o una chiave, si usa la scelta piu' sicura (ingresso,
 * inoltro e ping chiusi, uscita libera) e lo si scrive nel registro: e' l'unico caso di
 * valori non scritti in un file, perche' e' la via d'uscita sicura.
 * Al kernel si parla con netlink (NETLINK_NETFILTER, il sottosistema nf_tables), con
 * codice nostro e senza libnftnl ne' il comando nft: tutte le regole vanno in un solo
 * "lotto" (NFNL_MSG_BATCH_BEGIN ... BATCH_END) che il kernel applica tutto o niente.
 * Il DHCP non ne risente: usa prese AF_PACKET, che vedono i pacchetti prima del filtro,
 * anche quelli del rinnovo (l'ACK del server arriva alla porta 68, che il filtro
 * scarterebbe, ma la presa AF_PACKET l'ha gia' ricevuto).
 * Servono i moduli del kernel nf_tables, nft_ct e nf_conntrack (build.sh, MODULI).
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
 * LA PORTA DI CONTROLLO. Il servizio ascolta sul nome "network" (corekit/sistema/network.h,
 * dove c'e' il contratto): si puo' chiedere lo stato (status) e, con il permesso, riavviarlo
 * (restart) o spegnere la scheda (stop). E' in SOLA LETTURA: la configurazione non si cambia
 * da li', resta quella dei file. Per rispondere mentre fa il resto (aspettare il DHCP, il
 * rinnovo...) il servizio non dorme mai con dormi: ogni attesa passa da servi(), che
 * aspetta il momento dato (o che arrivi qualcosa dalla presa del DHCP) e intanto serve le
 * richieste della porta. Un restart risponde e poi si rilancia da solo (execv): rilegge i
 * file e rifa' tutto da capo, e l'indirizzo vecchio lo toglie prima. Uno stop toglie
 * indirizzo e strada e mette la scheda giu', lascia il firewall (un computer senza rete ma
 * protetto) e da li' non torna piu' al lavoro di prima: resta a servire la porta, in attesa
 * di un restart. Cosi' chi lo ferma non deve "srotolare" l'attesa in cui si trova.
 *
 * Scrive nel registro righe brevi, come gli altri servizi, per esempio:
 *   network: firewall attivo (ingresso bloccato, uscita libera, inoltro bloccato, ping ammesso)
 *   network: eth0 10.0.2.15/24, gateway 10.0.2.2, dns 10.0.2.3 */
#include "corekit/corekit.h"
#include "corekit/messaggi.h"
#include "corekit/sistema/servizi.h"     /* chi_chiede, ha_permesso */
#include "corekit/sistema/network.h"     /* il contratto della porta */
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
#include <netinet/icmp6.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/capability.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nf_tables.h>
#include <linux/netfilter/nf_conntrack_common.h>

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

/* Aspetta fino al momento 'fino' (ms, come adesso()) e intanto serve la porta di controllo;
 * se 'fd' non e' -1, torna 1 appena c'e' qualcosa da leggere su fd, altrimenti 0 allo
 * scadere. E' l'unico modo in cui il servizio aspetta (vedi LA PORTA DI CONTROLLO). */
static int servi(long long fino, int fd);
#define PER_SEMPRE 0x7fffffffffffffffLL

/* Lo stato per la porta: starting, up, stopped, failed (network.h) e, se failed, perche'. */
static const char *fase = RETE_STATO_AVVIO;
static char frase_guasto[160];

/* Resta fermo per sempre: quando non c'e' piu' niente da fare, ma il servizio non deve
 * finire (init lo farebbe ripartire). Risponde alla porta. */
static void __attribute__((noreturn)) resta_fermo(void) {
    for (;;) servi(PER_SEMPRE, -1);
}

/* Un errore da cui non si esce da soli: la riga in corso (senza il "network: " davanti)
 * diventa la frase di state=failed, si scrive nel registro e si resta fermi. */
static void __attribute__((noreturn)) guasto(void) {
    const int davanti = (int)strlen("network: ");
    int n = lunga - davanti;
    if (n < 0) n = 0;
    if (n > (int)sizeof frase_guasto - 1) n = (int)sizeof frase_guasto - 1;
    memcpy(frase_guasto, riga + davanti, (size_t)n);
    frase_guasto[n] = 0;
    fase = RETE_STATO_GUASTA;
    fine_riga();
    resta_fermo();
}

/* Come di(), ma e' un guasto: vedi guasto(). */
static void __attribute__((noreturn)) di_guasto(const char *s) {
    metti("network: ");
    metti(s);
    guasto();
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
static int leggi_riga(void *dati, char *chiave, char *valore) {
    struct configurazione *c = dati;
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

/* Legge un file "chiave=valore" (network.conf e firewall.conf): per ogni riga chiama
 * capisci(dati, chiave, valore). -1 se il file non c'e', 0 altrimenti; le righe che non
 * capisce le scrive nel registro e le salta. */
static int leggi_file(const char *percorso, int (*capisci)(void *, char *, char *), void *dati) {
    int d = open(percorso, O_RDONLY | O_CLOEXEC);
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
            capita = capisci(dati, senza_spazi(s), senza_spazi(uguale + 1));
            if (!capita) *uguale = '=';
        }
        if (!capita) {
            metti("network: in ");
            metti(percorso);
            metti(" non capisco la riga \"");
            metti(s);
            metti("\", la salto");
            fine_riga();
        }
    }
    return 0;
}

static int leggi_configurazione(struct configurazione *c) {
    memset(c, 0, sizeof *c);
    return leggi_file(CONFIGURAZIONE, leggi_riga, c);
}

/* --- 3. le schede --- */

/* Accende (IFF_UP) o spegne la scheda. 0, o -errno. */
static int imposta_su(const char *nome, int su) {
    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return -errno;
    struct ifreq r;
    memset(&r, 0, sizeof r);
    strncpy(r.ifr_name, nome, IF_NAMESIZE - 1);
    int esito = 0;
    if (ioctl(s, SIOCGIFFLAGS, &r) < 0) esito = -errno;
    else if (!!(r.ifr_flags & IFF_UP) != su) {
        if (su) r.ifr_flags |= IFF_UP;
        else r.ifr_flags &= ~IFF_UP;
        if (ioctl(s, SIOCSIFFLAGS, &r) < 0) esito = -errno;
    }
    close(s);
    return esito;
}

static int accendi(const char *nome) { return imposta_su(nome, 1); }

/* La scheda e' accesa e ha il collegamento (IFF_UP e IFF_RUNNING: quest'ultimo e' il
 * "carrier" del kernel, il cavo)? 0 anche se la scheda non c'e'. */
static int scheda_collegata(const char *nome) {
    if (!nome[0]) return 0;
    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return 0;
    struct ifreq r;
    memset(&r, 0, sizeof r);
    strncpy(r.ifr_name, nome, IF_NAMESIZE - 1);
    int si = ioctl(s, SIOCGIFFLAGS, &r) == 0 && (r.ifr_flags & IFF_UP) && (r.ifr_flags & IFF_RUNNING);
    close(s);
    return si;
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
    guasto();
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

/* --- 5. il kernel: rtnetlink --- */

static int presa_kernel = -1;
static unsigned int sequenza;

union msg_netlink {
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
    union msg_netlink risposta;
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
    union msg_netlink m;
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
    union msg_netlink m;
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

/* --- 2. il firewall: nf_tables ---
 *
 * Le regole stanno nel kernel (nftables); noi gliele diamo da una presa AF_NETLINK
 * NETLINK_NETFILTER, come fa il comando "nft", ma con codice nostro (vedi IL FIREWALL in
 * testa). Ogni messaggio e' un nlmsghdr con tipo (NFNL_SUBSYS_NFTABLES << 8) | NFT_MSG_...,
 * poi un nfgenmsg (la famiglia: NFPROTO_INET), poi gli attributi (struct nlattr), alcuni
 * "annidati" (un attributo che contiene altri attributi). I numeri dentro gli attributi
 * (registri, chiavi, ganci, politiche) vanno in big-endian; i dati da confrontare con il
 * pacchetto invece sono come li vede il kernel (l'indice di una scheda e lo stato della
 * connessione nell'ordine della CPU, i byte del pacchetto come sono). */

#define CONFIGURAZIONE_FIREWALL "/system/config/network/firewall.conf"
#define TABELLA "mshell"

struct firewall {
    int ingresso, uscita, inoltro, ping;   /* 1: allow, 0: block, -1: non scritto nel file */
};

static int leggi_riga_firewall(void *dati, char *chiave, char *valore) {
    struct firewall *f = dati;
    int *dove;
    if (!strcmp(chiave, "incoming")) dove = &f->ingresso;
    else if (!strcmp(chiave, "outgoing")) dove = &f->uscita;
    else if (!strcmp(chiave, "forward")) dove = &f->inoltro;
    else if (!strcmp(chiave, "ping")) dove = &f->ping;
    else return 0;
    if (!strcmp(valore, "allow")) *dove = 1;
    else if (!strcmp(valore, "block")) *dove = 0;
    else return 0;
    return 1;
}

/* Le regole del file. Se il file manca, o una chiave non c'e' (o non si capisce), si usa
 * la scelta PIU' SICURA che lascia comunque usare la rete: ingresso, inoltro e ping
 * chiusi, uscita libera. E' l'unico caso in cui il servizio usa valori non scritti in un
 * file: e' la via d'uscita sicura (meglio un computer chiuso che uno aperto per sbaglio),
 * e lo scrive nel registro. */
static void leggi_firewall(struct firewall *f) {
    f->ingresso = f->uscita = f->inoltro = f->ping = -1;
    if (leggi_file(CONFIGURAZIONE_FIREWALL, leggi_riga_firewall, f) < 0)
        di("manca " CONFIGURAZIONE_FIREWALL ", uso le regole piu' sicure");
    static const struct { const char *chiave, *valore; } sicuro[] = {
        { "incoming", "block" }, { "outgoing", "allow" }, { "forward", "block" }, { "ping", "block" },
    };
    int *chi[] = { &f->ingresso, &f->uscita, &f->inoltro, &f->ping };
    for (int i = 0; i < 4; i++) {
        if (*chi[i] >= 0) continue;
        *chi[i] = !strcmp(sicuro[i].valore, "allow");
        metti("network: nel firewall manca ");
        metti(sicuro[i].chiave);
        metti("=, uso ");
        metti(sicuro[i].valore);
        fine_riga();
    }
}

/* Il "lotto" (batch): tutti i messaggi, tra un BATCH_BEGIN e un BATCH_END, si mandano
 * insieme, e il kernel li applica tutti o nessuno. */
static char lotto[8192];
static int pieno;                   /* byte gia' usati dai messaggi chiusi */
static struct nlmsghdr *corrente;   /* il messaggio che si sta scrivendo */
static int da_confermare;           /* quanti messaggi chiedono la conferma (NLM_F_ACK) */
static int troppo;                  /* il lotto non ci sta in lotto[] */

static void chiudi_messaggio(void) {
    if (corrente) pieno += (int)NLMSG_ALIGN(corrente->nlmsg_len);
    corrente = 0;
}

static void messaggio_netlink(int tipo, int flag, int famiglia, int sottosistema) {
    chiudi_messaggio();
    if (pieno + (int)NLMSG_SPACE(sizeof(struct nfgenmsg)) > (int)sizeof lotto) { troppo = 1; pieno = 0; }
    corrente = (struct nlmsghdr *)(lotto + pieno);
    memset(corrente, 0, NLMSG_SPACE(sizeof(struct nfgenmsg)));
    corrente->nlmsg_len = NLMSG_LENGTH(sizeof(struct nfgenmsg));
    corrente->nlmsg_type = (unsigned short)tipo;
    corrente->nlmsg_flags = (unsigned short)(NLM_F_REQUEST | flag);
    corrente->nlmsg_seq = ++sequenza;
    if (flag & NLM_F_ACK) da_confermare++;
    struct nfgenmsg *g = NLMSG_DATA(corrente);
    g->nfgen_family = (unsigned char)famiglia;
    g->version = NFNETLINK_V0;
    g->res_id = htons((unsigned short)sottosistema);
}

/* un messaggio di nf_tables, nella nostra famiglia (inet: IPv4 e IPv6 insieme) */
static void messaggio_nft(int tipo, int flag) {
    messaggio_netlink(NFNL_SUBSYS_NFTABLES << 8 | tipo, flag | NLM_F_ACK, NFPROTO_INET, 0);
}

static struct nlattr *aggiungi(int tipo, const void *dati, int byte) {
    int dove = pieno + (int)NLMSG_ALIGN(corrente->nlmsg_len);
    if (dove + NLA_HDRLEN + NLA_ALIGN(byte) > (int)sizeof lotto) { troppo = 1; return (struct nlattr *)lotto; }
    struct nlattr *a = (struct nlattr *)(lotto + dove);
    a->nla_type = (unsigned short)tipo;
    a->nla_len = (unsigned short)(NLA_HDRLEN + byte);
    if (byte) memcpy((char *)a + NLA_HDRLEN, dati, (size_t)byte);
    corrente->nlmsg_len = NLMSG_ALIGN(corrente->nlmsg_len) + NLA_ALIGN(a->nla_len);
    return a;
}

static void aggiungi_numero(int tipo, uint32_t n) {
    n = htonl(n);
    aggiungi(tipo, &n, 4);
}

static void aggiungi_testo(int tipo, const char *s) {
    aggiungi(tipo, s, (int)strlen(s) + 1);
}

/* un attributo che ne contiene altri: si apre, si aggiungono i suoi, si chiude */
static struct nlattr *apri_gruppo(int tipo) {
    return aggiungi(tipo | NLA_F_NESTED, 0, 0);
}

static void chiudi_gruppo(struct nlattr *a) {
    if (!troppo) a->nla_len = (unsigned short)((char *)corrente + corrente->nlmsg_len - (char *)a);
}

/* Le "espressioni" di una regola: piccoli passi che il kernel fa su ogni pacchetto, uno
 * dopo l'altro. Leggono un dato del pacchetto in un registro (meta, ct, payload), lo
 * cambiano (bitwise) o lo confrontano (cmp: se il confronto non torna, la regola finisce
 * li'), e alla fine danno il verdetto (immediate). Usiamo un solo registro, NFT_REG_1. */
static struct nlattr *espressioni, *elemento, *dati_espressione;

static void espressione(const char *nome) {
    elemento = apri_gruppo(NFTA_LIST_ELEM);
    aggiungi_testo(NFTA_EXPR_NAME, nome);
    dati_espressione = apri_gruppo(NFTA_EXPR_DATA);
}

static void fine_espressione(void) {
    chiudi_gruppo(dati_espressione);
    chiudi_gruppo(elemento);
}

/* meta: un dato del pacchetto che non sta nei suoi byte (da quale scheda arriva, il
 * protocollo sopra IP...) */
static void e_meta(int chiave) {
    espressione("meta");
    aggiungi_numero(NFTA_META_DREG, NFT_REG_1);
    aggiungi_numero(NFTA_META_KEY, (uint32_t)chiave);
    fine_espressione();
}

/* ct: lo stato della connessione a cui appartiene il pacchetto (il "conntrack") */
static void e_stato_connessione(void) {
    espressione("ct");
    aggiungi_numero(NFTA_CT_DREG, NFT_REG_1);
    aggiungi_numero(NFTA_CT_KEY, NFT_CT_STATE);
    fine_espressione();
}

/* payload: byte del pacchetto, qui dall'inizio dell'intestazione del trasporto (ICMP) */
static void e_byte_trasporto(int da, int quanti) {
    espressione("payload");
    aggiungi_numero(NFTA_PAYLOAD_DREG, NFT_REG_1);
    aggiungi_numero(NFTA_PAYLOAD_BASE, NFT_PAYLOAD_TRANSPORT_HEADER);
    aggiungi_numero(NFTA_PAYLOAD_OFFSET, (uint32_t)da);
    aggiungi_numero(NFTA_PAYLOAD_LEN, (uint32_t)quanti);
    fine_espressione();
}

static void e_valore(int tipo, const void *dati, int byte) {
    struct nlattr *v = apri_gruppo(tipo);
    aggiungi(NFTA_DATA_VALUE, dati, byte);
    chiudi_gruppo(v);
}

/* bitwise: registro = (registro & maschera) ^ 0, su 4 byte */
static void e_maschera(uint32_t maschera) {
    uint32_t zero = 0;
    espressione("bitwise");
    aggiungi_numero(NFTA_BITWISE_SREG, NFT_REG_1);
    aggiungi_numero(NFTA_BITWISE_DREG, NFT_REG_1);
    aggiungi_numero(NFTA_BITWISE_LEN, 4);
    e_valore(NFTA_BITWISE_MASK, &maschera, 4);
    e_valore(NFTA_BITWISE_XOR, &zero, 4);
    fine_espressione();
}

static void e_confronta(int come, const void *dati, int byte) {
    espressione("cmp");
    aggiungi_numero(NFTA_CMP_SREG, NFT_REG_1);
    aggiungi_numero(NFTA_CMP_OP, (uint32_t)come);
    e_valore(NFTA_CMP_DATA, dati, byte);
    fine_espressione();
}

/* immediate: il verdetto, qui sempre "accetta" (quello che non e' accettato lo scarta la
 * politica della catena) */
static void e_accetta(void) {
    espressione("immediate");
    aggiungi_numero(NFTA_IMMEDIATE_DREG, NFT_REG_VERDICT);
    struct nlattr *dati = apri_gruppo(NFTA_IMMEDIATE_DATA);
    struct nlattr *verdetto = apri_gruppo(NFTA_DATA_VERDICT);
    aggiungi_numero(NFTA_VERDICT_CODE, NF_ACCEPT);
    chiudi_gruppo(verdetto);
    chiudi_gruppo(dati);
    fine_espressione();
}

/* Una regola, in fondo alla catena: regola(), le espressioni, fine_regola() */
static void regola(const char *catena) {
    messaggio_nft(NFT_MSG_NEWRULE, NLM_F_CREATE | NLM_F_APPEND);
    aggiungi_testo(NFTA_RULE_TABLE, TABELLA);
    aggiungi_testo(NFTA_RULE_CHAIN, catena);
    espressioni = apri_gruppo(NFTA_RULE_EXPRESSIONS);
}

static void fine_regola(void) {
    e_accetta();
    chiudi_gruppo(espressioni);
}

/* Una catena di base: si aggancia a un punto del kernel (gancio), con la politica per i
 * pacchetti che nessuna regola accetta. */
static void catena(const char *nome, int gancio, int aperta) {
    messaggio_nft(NFT_MSG_NEWCHAIN, NLM_F_CREATE);
    aggiungi_testo(NFTA_CHAIN_TABLE, TABELLA);
    aggiungi_testo(NFTA_CHAIN_NAME, nome);
    struct nlattr *g = apri_gruppo(NFTA_CHAIN_HOOK);
    aggiungi_numero(NFTA_HOOK_HOOKNUM, (uint32_t)gancio);
    aggiungi_numero(NFTA_HOOK_PRIORITY, 0);           /* "filter", come nft */
    chiudi_gruppo(g);
    aggiungi_numero(NFTA_CHAIN_POLICY, aperta ? NF_ACCEPT : NF_DROP);
    aggiungi_testo(NFTA_CHAIN_TYPE, "filter");
}

/* Le due regole di ogni catena chiusa: la scheda verso se stessi (lo), e i pacchetti
 * delle connessioni gia' aperte (le risposte) o collegate a una aperta (per esempio un
 * errore ICMP che parla di una nostra connessione). */
static void regole_di_base(const char *nome, int entrata) {
    uint32_t lo = if_nametoindex("lo"), zero = 0;
    regola(nome);
    e_meta(entrata ? NFT_META_IIF : NFT_META_OIF);
    e_confronta(NFT_CMP_EQ, &lo, 4);
    fine_regola();
    regola(nome);
    e_stato_connessione();
    e_maschera(NF_CT_STATE_BIT(IP_CT_ESTABLISHED) | NF_CT_STATE_BIT(IP_CT_RELATED));
    e_confronta(NFT_CMP_NEQ, &zero, 4);
    fine_regola();
}

/* accetta in entrata i messaggi ICMP (o ICMPv6) di un tipo */
static void accetta_icmp(int protocollo, int tipo) {
    uint8_t p = (uint8_t)protocollo, t = (uint8_t)tipo;
    regola("input");
    e_meta(NFT_META_L4PROTO);
    e_confronta(NFT_CMP_EQ, &p, 1);
    e_byte_trasporto(0, 1);                            /* il primo byte: il tipo */
    e_confronta(NFT_CMP_EQ, &t, 1);
    fine_regola();
}

/* Manda il lotto e aspetta le conferme. 0, o -errno. */
static int manda_lotto(void) {
    chiudi_messaggio();
    if (troppo) return -ENOBUFS;
    int s = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_NETFILTER);
    if (s < 0) return -errno;
    int si = 1;   /* negli errori il kernel non ci rimanda tutto il messaggio */
    setsockopt(s, SOL_NETLINK, NETLINK_CAP_ACK, &si, sizeof si);
    struct sockaddr_nl kernel = { .nl_family = AF_NETLINK };
    int esito = 0;
    if (sendto(s, lotto, (size_t)pieno, 0, (struct sockaddr *)&kernel, sizeof kernel) < 0) esito = -errno;
    static char risposta[8192];
    while (!esito && da_confermare > 0) {
        struct pollfd p = { .fd = s, .events = POLLIN };
        if (poll(&p, 1, 5000) <= 0) { esito = -ETIMEDOUT; break; }
        ssize_t letti = recv(s, risposta, sizeof risposta, 0);
        if (letti < 0) {
            if (errno == EINTR) continue;
            esito = -errno;
            break;
        }
        unsigned int n = (unsigned int)letti;
        for (struct nlmsghdr *r = (struct nlmsghdr *)risposta; NLMSG_OK(r, n); r = NLMSG_NEXT(r, n)) {
            if (r->nlmsg_type != NLMSG_ERROR) continue;
            int errore = ((struct nlmsgerr *)NLMSG_DATA(r))->error;
            if (errore) { esito = errore; break; }
            da_confermare--;
        }
    }
    close(s);
    return esito;
}

/* Mette tutte le regole, in un lotto solo. Se la tabella c'e' gia' (il servizio e'
 * ripartito) la si sostituisce: "crea, cancella, crea", come fa nft, cosi' la prima
 * creazione non fallisce se c'e' e la cancellazione non fallisce se non c'e'. Tutto nello
 * stesso lotto: il computer non resta mai, nemmeno per un attimo, senza regole. */
static int metti_firewall(const struct firewall *f) {
    pieno = 0;
    corrente = 0;
    da_confermare = 0;
    troppo = 0;
    messaggio_netlink(NFNL_MSG_BATCH_BEGIN, 0, AF_UNSPEC, NFNL_SUBSYS_NFTABLES);
    messaggio_nft(NFT_MSG_NEWTABLE, NLM_F_CREATE);
    aggiungi_testo(NFTA_TABLE_NAME, TABELLA);
    messaggio_nft(NFT_MSG_DELTABLE, 0);
    aggiungi_testo(NFTA_TABLE_NAME, TABELLA);
    messaggio_nft(NFT_MSG_NEWTABLE, NLM_F_CREATE);
    aggiungi_testo(NFTA_TABLE_NAME, TABELLA);

    catena("input", NF_INET_LOCAL_IN, f->ingresso);
    catena("forward", NF_INET_FORWARD, f->inoltro);
    catena("output", NF_INET_LOCAL_OUT, f->uscita);
    if (!f->ingresso) {
        regole_di_base("input", 1);
        /* I messaggi ICMPv6 senza cui IPv6 non funziona: chi ha quale indirizzo fisico
         * (neighbor solicitation e advertisement: per IPv6 sono quello che e' ARP per IPv4,
         * e ARP il firewall IP non lo vede), dove sta il router (router advertisement), e
         * gli errori (destinazione irraggiungibile, pacchetto troppo grande, tempo scaduto,
         * intestazione sbagliata: quelli delle nostre connessioni sono gia' "collegati",
         * ma servono anche prima che una connessione esista). Per IPv4 basta "collegati". */
        static const int icmp6[] = {
            ND_NEIGHBOR_SOLICIT, ND_NEIGHBOR_ADVERT, ND_ROUTER_ADVERT,
            ICMP6_DST_UNREACH, ICMP6_PACKET_TOO_BIG, ICMP6_TIME_EXCEEDED, ICMP6_PARAM_PROB,
        };
        for (unsigned int i = 0; i < sizeof icmp6 / sizeof icmp6[0]; i++)
            accetta_icmp(IPPROTO_ICMPV6, icmp6[i]);
        if (f->ping) {
            accetta_icmp(IPPROTO_ICMP, ICMP_ECHO);
            accetta_icmp(IPPROTO_ICMPV6, ICMP6_ECHO_REQUEST);
        }
    }
    /* l'inoltro non ha regole: il computer non fa da router, e con forward=block la
     * politica scarta tutto */
    if (!f->uscita) regole_di_base("output", 0);
    messaggio_netlink(NFNL_MSG_BATCH_END, 0, AF_UNSPEC, NFNL_SUBSYS_NFTABLES);
    return manda_lotto();
}

/* --- 6. il DNS --- */

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

/* --- 4. il DHCP --- */

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
        if (!servi(scadenza, presa_dhcp)) continue;   /* intanto serve la porta */
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

/* Quello che la scheda ha adesso, per la porta di controllo (status): l'indirizzo e il resto
 * (attuale, solo se ha_indirizzo), e quando scade il prestito del DHCP (scade_il, come
 * adesso(); solo con dhcp). */
static struct prestito attuale;
static int ha_indirizzo;
static long long scade_il;

/* Da' alla scheda indirizzo, strada e DNS, e lo scrive nel registro se e' cambiato
 * qualcosa rispetto a 'prima' (0: la prima volta). */
static void applica(const struct prestito *p, const struct prestito *prima, int da_dhcp) {
    attuale = *p;
    ha_indirizzo = 1;
    scade_il = da_dhcp && p->durata != 0xffffffffu ? adesso() + (long long)p->durata * 1000 : 0;
    fase = RETE_STATO_ACCESA;
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
    ha_indirizzo = 0;
    fase = RETE_STATO_AVVIO;
}

/* Aspetta fino al momento 'quando' (ms), servendo la porta. */
static void aspetta_fino(long long quando) {
    servi(quando, -1);
}

/* --- 7. la porta di controllo --- */

static struct configurazione cfg;   /* quella letta dal file */
static struct firewall fw;          /* idem */
static int firewall_attivo;         /* le regole sono nel kernel */
static int porta = -1;              /* il descrittore di ascolta("network") */

#define MAX_CLIENTI 8
#define SERVIZIO "/system/services/network"

/* Un valore di firewall.conf, come lo scrive il file. */
static const char *allow_block(int si) { return si ? "allow" : "block"; }

/* Un indirizzo con il prefisso, "10.0.2.15/24", in 'dove'. */
static void testo_indirizzo(char *dove, int byte, uint32_t ip, int prefisso) {
    char ip_testo[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip, ip_testo, sizeof ip_testo);
    snprintf(dove, (size_t)byte, "%s/%d", ip_testo, prefisso);
}

/* status: la risposta con tutto quello che si sa. */
static void stato(struct messaggio *r, const struct messaggio *q) {
    messaggio_risposta(r, q, ESITO_OK);
    messaggio_aggiungi(r, RETE_CHIAVE_SCHEDA, cfg.interfaccia);
    messaggio_aggiungi(r, RETE_CHIAVE_STATO, fase);
    messaggio_aggiungi(r, RETE_CHIAVE_COLLEGATA, scheda_collegata(cfg.interfaccia) ? "yes" : "no");
    messaggio_aggiungi(r, RETE_CHIAVE_MODO, cfg.statica ? "static" : "dhcp");
    if (ha_indirizzo) {
        char t[INET_ADDRSTRLEN + 8];
        testo_indirizzo(t, sizeof t, attuale.indirizzo, attuale.prefisso);
        messaggio_aggiungi(r, RETE_CHIAVE_INDIRIZZO, t);
        if (attuale.gateway) {
            inet_ntop(AF_INET, &attuale.gateway, t, sizeof t);
            messaggio_aggiungi(r, RETE_CHIAVE_GATEWAY, t);
        }
        for (int i = 0; i < attuale.quanti_dns; i++) {
            inet_ntop(AF_INET, &attuale.dns[i], t, sizeof t);
            messaggio_aggiungi(r, RETE_CHIAVE_DNS, t);
        }
        if (!cfg.statica) {
            if (attuale.durata == 0xffffffffu) messaggio_aggiungi(r, RETE_CHIAVE_PRESTITO, "infinite");
            else {
                long long resta = (scade_il - adesso()) / 1000;
                messaggio_aggiungi_numero(r, RETE_CHIAVE_PRESTITO, resta > 0 ? (long)resta : 0);
            }
        }
    }
    messaggio_aggiungi(r, RETE_CHIAVE_FIREWALL, firewall_attivo ? "active" : "inactive");
    if (firewall_attivo) {
        messaggio_aggiungi(r, RETE_CHIAVE_F_ENTRATA, allow_block(fw.ingresso));
        messaggio_aggiungi(r, RETE_CHIAVE_F_INOLTRO, allow_block(fw.inoltro));
        messaggio_aggiungi(r, RETE_CHIAVE_F_USCITA, allow_block(fw.uscita));
        messaggio_aggiungi(r, RETE_CHIAVE_F_PING, allow_block(fw.ping || fw.ingresso));
    }
    if (fase == RETE_STATO_GUASTA) messaggio_aggiungi(r, RETE_CHIAVE_FRASE, frase_guasto);
}

/* Cosa fare DOPO aver risposto (la risposta deve partire prima: restart si rilancia, e
 * il collegamento con chi chiede morirebbe senza risposta). */
enum azione { NIENTE, RIAVVIA, FERMA };

/* Una richiesta e la sua risposta, sul collegamento 'd'. */
static enum azione rispondi(int d) {
    struct messaggio q, r;
    enum azione dopo = NIENTE;
    int e = messaggio_ricevi(d, &q);
    if (e == 0 || e == -1) return NIENTE;
    if (e == MESSAGGIO_NON_VALIDO) {
        messaggio_risposta(&r, 0, ESITO_NON_VALIDO);
    } else {
        struct chi_chiede c;
        if (chi_chiede(d, &c) < 0) {   /* il kernel non lo sa: nessuno */
            c.uid = -1;
            c.programma[0] = 0;
        }
        const char *richiesta = messaggio_valore(&q, CHIAVE_RICHIESTA);
        if (!richiesta) richiesta = "";
        if (!strcmp(richiesta, RETE_RICHIESTA_STATO)) {
            if (ha_permesso(&c, PERMESSO_RETE_USARE)) stato(&r, &q);
            else messaggio_risposta(&r, &q, ESITO_NEGATO);
        } else if (!strcmp(richiesta, RETE_RICHIESTA_RIAVVIA) || !strcmp(richiesta, RETE_RICHIESTA_FERMA)) {
            if (ha_permesso(&c, PERMESSO_RETE_IMPOSTAZIONI)) {
                messaggio_risposta(&r, &q, ESITO_OK);
                dopo = !strcmp(richiesta, RETE_RICHIESTA_RIAVVIA) ? RIAVVIA : FERMA;
            } else messaggio_risposta(&r, &q, ESITO_NEGATO);
        } else {
            messaggio_risposta(&r, &q, ESITO_NON_VALIDO);   /* una richiesta che non conosco */
        }
        if (q.allegato >= 0) chiudi(q.allegato);
    }
    /* il collegamento e' nuovo e porta solo questa richiesta: la risposta ci sta nella
     * casella del kernel, e messaggio_manda non dorme */
    messaggio_manda(d, &r);
    if (r.allegato >= 0) chiudi(r.allegato);
    return dopo;
}

/* restart: toglie l'indirizzo (se la nuova configurazione ne da' un altro, il vecchio non
 * deve restare) e si rilancia: rilegge i file e rifa' tutto da capo, firewall compreso.
 * Il nome "network" lo libera la chiusura dei descrittori (sono tutti CLOEXEC), e il nuovo
 * lo riprende. Se execv non riesce lo dice e il servizio continua com'e'. */
static void riavvia(void) {
    di("riavvio richiesto, rileggo la configurazione");
    if (ha_indirizzo) togli_indirizzo(&attuale);
    char *argomenti[] = { (char *)SERVIZIO, 0 };
    execv(SERVIZIO, argomenti);
    metti("network: non riesco a rilanciarmi: ");
    metti(strerror(errno));
    fine_riga();
}

/* stop: indirizzo e DNS via, scheda giu'; il firewall resta (vedi network.h). Il servizio
 * poi serve solo la porta: non torna al lavoro di prima. */
static void __attribute__((noreturn)) ferma(void) {
    if (fase == RETE_STATO_FERMA) resta_fermo();
    if (ha_indirizzo) togli_indirizzo(&attuale);
    if (presa_dhcp >= 0) { close(presa_dhcp); presa_dhcp = -1; }
    if (scheda) imposta_su(nome_scheda, 0);
    fase = RETE_STATO_FERMA;
    di("scheda spenta su richiesta, il firewall resta");
    resta_fermo();
}

static int servi(long long fino, int fd) {
    static int clienti[MAX_CLIENTI];
    static int quanti;
    for (;;) {
        long long resta = fino - adesso();
        if (resta <= 0) return 0;
        struct da_guardare g[2 + MAX_CLIENTI];
        int n = 0;
        int i_fd = -1, i_porta = -1;
        if (fd >= 0) { i_fd = n; g[n++].descrittore = fd; }
        if (porta >= 0) { i_porta = n; g[n++].descrittore = porta; }
        int primo_cliente = n;
        for (int i = 0; i < quanti; i++) g[n++].descrittore = clienti[i];
        int ms = resta > 60 * 60 * 1000 ? 60 * 60 * 1000 : (int)resta;
        if (aspetta(g, n, ms) <= 0) continue;

        /* le richieste: una per collegamento, poi lo si chiude */
        enum azione dopo = NIENTE;
        for (int i = quanti - 1; i >= 0; i--) {
            if (!g[primo_cliente + i].pronto) continue;
            enum azione a = rispondi(clienti[i]);
            if (a != NIENTE) dopo = a;
            chiudi(clienti[i]);
            clienti[i] = clienti[--quanti];
        }
        if (i_porta >= 0 && g[i_porta].pronto) {
            int d = accetta(porta);
            if (d >= 0 && quanti < MAX_CLIENTI) clienti[quanti++] = d;
            else if (d >= 0) chiudi(d);
        }
        if (dopo == FERMA) ferma();
        if (dopo == RIAVVIA) riavvia();
        if (i_fd >= 0 && g[i_fd].pronto) return 1;
    }
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
            aspetta_fino(adesso() + 10 * 1000);
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
    /* la porta SUBITO, prima di tutto il resto: anche se poi il servizio resta fermo per
     * un guasto, si puo' chiedere come sta (state=failed) */
    porta = ascolta(NOME_NETWORK);
    if (porta < 0) di("non riesco a mettermi in ascolto sul nome " NOME_NETWORK ", niente porta di controllo");

    if (leggi_configurazione(&cfg) < 0) di_guasto("manca " CONFIGURAZIONE ", resto fermo");
    if (!cfg.interfaccia[0]) di_guasto("in " CONFIGURAZIONE " manca interface=, resto fermo");
    if (cfg.statica && !cfg.indirizzo) di_guasto("con mode=static serve address=, resto fermo");
    nome_scheda = cfg.interfaccia;
    if (!permessi_di_rete())
        di_guasto("Linux non mi lascia gestire le schede di rete (CAP_NET_ADMIN, CAP_NET_RAW), resto fermo");

    /* il firewall, PRIMA di accendere le schede: la rete non e' mai accesa senza */
    leggi_firewall(&fw);
    int e = metti_firewall(&fw);
    if (e < 0) {
        metti("network: non riesco a mettere il firewall: ");
        metti(strerror(-e));
        metti(", lascio la rete spenta");
        guasto();
    }
    firewall_attivo = 1;
    metti("network: firewall attivo (ingresso ");
    metti(fw.ingresso ? "libero" : "bloccato");
    metti(", uscita ");
    metti(fw.uscita ? "libera" : "bloccata");
    metti(", inoltro ");
    metti(fw.inoltro ? "libero" : "bloccato");
    metti(", ping ");
    metti(fw.ping || fw.ingresso ? "ammesso" : "bloccato");
    metti(")");
    fine_riga();

    e = accendi("lo");
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
        aspetta_fino(adesso() + 5000);
    }
    if ((e = accendi(nome_scheda)) < 0) fermo("non riesco ad accendere", nome_scheda, e);
    if ((e = indirizzo_fisico(nome_scheda, mac)) < 0) fermo("non trovo l'indirizzo fisico di", nome_scheda, e);

    if (!cfg.statica) con_dhcp();

    struct prestito p;
    memset(&p, 0, sizeof p);
    p.indirizzo = cfg.indirizzo;
    p.prefisso = cfg.prefisso;
    p.gateway = cfg.gateway;
    memcpy(p.dns, cfg.dns, sizeof p.dns);
    p.quanti_dns = cfg.quanti_dns;
    applica(&p, 0, 0);
    resta_fermo();
}
