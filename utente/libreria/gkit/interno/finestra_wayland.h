/* GKit, interno: la finestra normale come CLIENT WAYLAND (il lavoro e' in finestra_wayland.c;
 * finestra.c sceglie tra questa strada e il protocollo nostro). Niente tipi di Wayland qui: lo
 * include solo GKit. Lo stato di una finestra e' in finestra.wl (gkit.h). */
#ifndef GKIT_INTERNO_FINESTRA_WAYLAND_H
#define GKIT_INTERNO_FINESTRA_WAYLAND_H

#include "gkit/gkit.h"

/* 1 se il programma ha la connessione Wayland del launcher (WAYLAND_SOCKET) e il compositore ha
 * i globali che servono. Si prova una volta sola (la prima volta si connette). */
int fw_attivo(void);

/* Apre la finestra: finestra.c ha gia' creato il foglio con le due immagini (f->foglio,
 * f->immagini). Dopo il configure del compositore i due buffer sono pronti. 0, o -1. */
int fw_apri(struct finestra *f, int larghezza, int altezza, const char *titolo);

/* Toglie la finestra dal compositore (il foglio e le immagini restano a finestra.c). */
void fw_chiudi(struct finestra *f);

/* Aspetta eventi dal compositore per al massimo 'ms' millisecondi (-1: finche' ne arrivano;
 * si ferma comunque alla scadenza di una ripetizione di tasto), li smista nelle code delle
 * finestre e serve le scadenze. 0, o -1 se il compositore ha chiuso. */
int fw_leggi(int ms);

/* Il descrittore della connessione. */
int fw_descrittore(void);

/* Spedisce quello che e' in attesa di partire. */
void fw_scarica(void);

/* Tra quanti ms va servita la prossima ripetizione di tasto (0: adesso; -1: niente). */
int fw_scadenza_ms(void);

/* Serve le ripetizioni arrivate a scadenza. */
void fw_servi_scadenze(void);

/* Mostra l'immagine f->in_uso: 'zone' sono le 'n' zone cambiate (gia' dentro la finestra, una
 * vuota non conta); 'tutta' = 1: tutta la finestra. 0, o -1. */
int fw_mostra(struct finestra *f, const struct zona_finestra *zone, int n, int tutta);

int fw_trascinami(struct finestra *f);
int fw_riducimi(struct finestra *f);
int fw_tasti_fisici(struct finestra *f);
int fw_forma(struct finestra *f, unsigned int forma);

/* Lo schermo (la misura corrente del wl_output). 0, o -1 se il compositore non lo dice. */
int fw_schermo(int *larghezza, int *altezza);

#endif
