/* LO SFONDO DELLA SCRIVANIA, scelto dalla persona tra le foto della sua cartella
 * /data/users/<uid>/wallpapers (fuori dalla Home: Files non la mostra). La scelta la tiene
 * catalog, come il tema (corekit/catalog.h): il nome del file della foto ("montagna.jpg");
 * vuoto vuol dire "Predefinito" (la sfumatura di sistema, wallpapers/default.image). Se la foto
 * non si legge si usa il Predefinito.
 *
 * Chi sceglie (Impostazioni) lo chiede a catalog (sfondo_salva), che avvisa chi osserva: la
 * scrivania ridisegna lo sfondo. Il codice e' in interno/sfondo.c. */
#ifndef SFONDO_H
#define SFONDO_H

#include "corekit/catalog.h"

#define SFONDI_MAX        24    /* quante foto si elencano */
#define SFONDO_FILE_MAX   64    /* byte di un nome di file, compreso lo 0 */
#define SFONDO_NOME_MAX   64    /* byte del nome mostrato, compreso lo 0 */

/* I file .jpg della cartella degli sfondi, in ordine alfabetico, in 'nomi'. Quanti sono. */
int sfondo_elenca(char nomi[][SFONDO_FILE_MAX], int max);

/* Il nome mostrato di un file: senza estensione e con la prima lettera maiuscola. */
void sfondo_nome_mostrato(const char *file, char *nome, int dimensione);

/* Il file scelto, chiesto a catalog ("" = Predefinito, anche se catalog non risponde). */
void sfondo_leggi(char *file, int dimensione);

/* Il file scelto, dalle impostazioni che ha dato catalog (una risposta o un avviso). */
void sfondo_da_catalog(const struct catalog_valori *v, char *file, int dimensione);

/* Chiede a catalog di cambiare la scelta ("" = Predefinito); catalog avvisa chi osserva. 0, o -1
 * se il nome non e' valido, manca il permesso "settings.system" o catalog non risponde. */
int sfondo_salva(const char *file);

/* Disegna la foto 'file' in 'pixel' (l x a, opaco), "riempi": scalata senza deformarla e
 * tagliata dove avanza, centrata. 0 se fatto, -1 se la foto non si legge ('pixel' non si tocca). */
int sfondo_disegna_foto(const char *file, unsigned int *pixel, int l, int a);

#endif
