/* I materiali del sistema, di UIKit (vedi LIBRERIE.md): il font del sistema (IBM Plex Sans, e
 * Inter finche' non sparisce), le icone Lucide, gli oggetti (le icone di file e cartelle), le
 * immagini dei controlli e lo sfondo. E' COSA si disegna: il design di mshell. La scrivania e
 * tutte le app li prendono da qui, cosi' una cartella e' uguale ovunque.
 *
 * I dati stanno in file su disco, in /system/resources (fonts/, icons/, wallpapers/; nel
 * repository in risorse/), preparati da strumenti/grafica/genera.py. L'elenco (nomi e file)
 * e' materiali_elenco.h, generato anche lui. Ogni materiale si carica la PRIMA volta che
 * serve (uikit/interno/materiali.c, con le funzioni di GKit font_da_file, icona_da_file,
 * immagine_da_file), e poi resta: un programma tocca solo i file di quello che disegna.
 *
 * Per chi usa UIKit non cambia niente: un materiale si chiama con il suo nome come se fosse
 * una variabile (&plex_14, plex_14.altezza, &oggetto_cartella_grande), e un'icona per numero
 * con icona(). Se un file manca o e' rovinato, GKit lo scrive sullo standard error e da' un
 * materiale di riserva (vedi gkit_disegno.h): il programma va avanti. */
#ifndef MATERIALI_H
#define MATERIALI_H

#include "gkit/gkit_disegno.h"
#include "materiali_elenco.h"

#define MATERIALE_NUMERO(nome, file) MATERIALE_##nome,
enum { MATERIALI_FONT(MATERIALE_NUMERO) FONT_DEL_SISTEMA };
enum { MATERIALI_IMMAGINI(MATERIALE_NUMERO) IMMAGINI_DEL_SISTEMA };
#undef MATERIALE_NUMERO
#define MATERIALE_UNO(nome, file) + 1
enum { ICONE_DEL_SISTEMA = 0 MATERIALI_ICONE(MATERIALE_UNO) };
#undef MATERIALE_UNO

/* Il font, l'immagine o l'icona del sistema per numero (MATERIALE_..., ICONA_...), caricati
 * la prima volta. Le icone: le sue misure, e quante sono in *quante. */
const struct font *font_del_sistema(int quale);
const struct immagine *immagine_del_sistema(int quale);
const struct icona *icona_del_sistema(int quale, int *quante);

/* L'icona del sistema 'quale' (ICONA_...) di lato 'lato' pixel, nel colore del pennello.
 * Se quella misura non c'e', si usa la piu' vicina. */
void icona(struct pennello *p, int quale, int lato, int x, int y);

#endif
