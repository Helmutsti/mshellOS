/* Il nucleo VIRTIO: quello che hanno in comune tutti i dispositivi virtio (input, disco,
 * schermo...). La spiegazione lunga e' in virtio.c. Qui c'e' solo come si usa.
 *
 * Il giro tipico di un driver:
 *
 *     struct virtio_dispositivo d;
 *     struct virtq q;
 *     if (!virtio_prepara(&d, scheda, 0)) return 0;   // trova le parti, stretta di mano
 *     if (!virtio_coda(&d, 0, &q)) return 0;          // prepara la coda numero 0
 *     ... virtq_aggiungi(&q, pezzi, n) per dare i primi pacchi ...
 *     virtio_pronto(&d);                              // "sono pronto": DRIVER_OK
 *     virtq_notifica(&q);                             // "guarda nella cassetta"
 *     ... poi, a ogni tick: while ((id = virtq_raccogli(&q, &byte)) >= 0) { ... }
 */
#ifndef VIRTIO_H
#define VIRTIO_H

#include "kernel.h"   /* indirizzo_t */
#include "pci.h"

/* Il bit 32 delle caratteristiche: "parlo virtio 1.x" (quello "moderno"). */
#define VIRTIO_F_VERSION_1  (1ULL << 32)

/* Il dispositivo: la scheda PCI e i puntatori alle sue quattro parti. */
struct virtio_dispositivo {
    const struct pci_scheda *scheda;
    volatile unsigned char *comune;        /* la configurazione comune */
    volatile unsigned char *isr;           /* lo stato delle interruzioni (noi non le usiamo) */
    volatile unsigned char *config;        /* la configurazione propria del dispositivo */
    volatile unsigned char *notifica;      /* dove si "suona il campanello" */
    unsigned int moltiplicatore;           /* il campanello della coda q sta a notifica + q.off * questo */
    unsigned int config_byte;              /* quanto e' grande la configurazione del dispositivo */
    unsigned long long caratteristiche;    /* quelle che abbiamo accettato */
};

/* Un pezzo di una catena: un pezzo di memoria che il dispositivo legge o scrive. */
struct virtq_pezzo {
    indirizzo_t indirizzo;       /* dove sta: l'indirizzo FISICO (lo usa il dispositivo) */
    unsigned int byte;           /* quanto e' lungo */
    int scrive_dispositivo;      /* 1: ci scrive il dispositivo; 0: lo legge soltanto */
};

/* I tre pezzi della coda "split", come li vede il dispositivo (vedi virtio.c). */
struct virtq_descrittore {
    unsigned long long indirizzo;
    unsigned int byte;
    unsigned short flags;
    unsigned short prossimo;
};
struct virtq_disponibile {
    unsigned short flags;
    unsigned short idx;
    unsigned short anello[];
};
struct virtq_usato_voce {
    unsigned int id;
    unsigned int byte;
};
struct virtq_usato {
    unsigned short flags;
    unsigned short idx;
    struct virtq_usato_voce anello[];
};

/* Una coda, con i nostri appunti. */
struct virtq {
    unsigned short dimensione;                 /* quante voci */
    unsigned short numero;                     /* il numero della coda nel dispositivo */
    struct virtq_descrittore *descrittori;
    volatile struct virtq_disponibile *disponibile;
    volatile struct virtq_usato *usato;
    volatile unsigned short *campanello;       /* dove si scrive per avvisare il dispositivo */
    unsigned short disponibile_idx;            /* quanti pacchi abbiamo messo in tutto */
    unsigned short usato_visti;                /* quanti ne abbiamo gia' ritirati */
    unsigned short libero;                     /* il primo descrittore libero (lista) */
    unsigned short liberi;                     /* quanti descrittori liberi */
};

/* Trova le parti, azzera il dispositivo e fa la stretta di mano fino a FEATURES_OK.
 * 'volute' = le caratteristiche che vorremmo (oltre a VERSION_1, che e' sempre chiesta):
 * si accettano quelle che il dispositivo offre. Dopo, d->caratteristiche dice quali.
 * 1 se e' andata bene, 0 se no. */
int virtio_prepara(struct virtio_dispositivo *d, const struct pci_scheda *s,
                   unsigned long long volute);

/* Prepara la coda numero 'numero' (la sua memoria viene da una pagina). 1 / 0. */
int virtio_coda(struct virtio_dispositivo *d, unsigned short numero, struct virtq *q);

/* L'ultimo passo della stretta di mano: DRIVER_OK. Da qui il dispositivo lavora. */
void virtio_pronto(struct virtio_dispositivo *d);

/* Mette in coda una catena di 'n' pezzi. Ritorna il numero del primo descrittore
 * (la "testa": e' quello che ricomparira' in virtq_raccogli), o -1 se non c'e' posto.
 * Non avvisa il dispositivo: per quello c'e' virtq_notifica. */
int virtq_aggiungi(struct virtq *q, const struct virtq_pezzo *pezzi, int n);

/* Suona il campanello: "c'e' roba nuova nella cassetta". */
void virtq_notifica(struct virtq *q);

/* Ritira il prossimo pacco finito. Ritorna la sua testa (come da virtq_aggiungi) e in
 * *byte quanti byte il dispositivo ha scritto; i descrittori della catena tornano
 * liberi. -1 se non c'e' niente di nuovo. */
int virtq_raccogli(struct virtq *q, unsigned int *byte);

/* La configurazione propria del dispositivo (a 'posto' byte dall'inizio). */
unsigned char virtio_config_leggi8(const struct virtio_dispositivo *d, unsigned int posto);
unsigned short virtio_config_leggi16(const struct virtio_dispositivo *d, unsigned int posto);
unsigned int virtio_config_leggi32(const struct virtio_dispositivo *d, unsigned int posto);
void virtio_config_scrivi8(const struct virtio_dispositivo *d, unsigned int posto,
                           unsigned char valore);

/* Il "numero di versione" della configurazione: cambia se il dispositivo la modifica
 * mentre la leggiamo. Si legge prima e dopo; se e' diverso, si rilegge. */
unsigned char virtio_config_versione(const struct virtio_dispositivo *d);

#endif
