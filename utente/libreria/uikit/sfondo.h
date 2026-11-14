/* LO SFONDO DELLA SCRIVANIA, scelto dalla persona tra le foto della sua cartella
 * /data/users/<uid>/wallpapers (fuori dalla Home: Files non la mostra). La scelta sta nelle
 * preferenze, come il tema: il file /data/users/1000/preferences/sfondo contiene il nome del
 * file della foto ("montagna.jpg"); vuoto o assente vuol dire "Predefinito" (la sfumatura di
 * sistema, wallpapers/default.image). Se la foto non si legge si usa il Predefinito.
 *
 * Chi sceglie (Impostazioni) lo salva e chiede CAMBIA_TEMA al compositore (uikit_cambia_tema ->
 * finestra_cambia_tema): il compositore manda TEMA_CAMBIATO a tutti e la scrivania, che e' tra
 * questi, rilegge la preferenza e ridisegna lo sfondo. Il codice e' in interno/sfondo.c. */
#ifndef SFONDO_H
#define SFONDO_H

#define SFONDI_MAX        24    /* quante foto si elencano */
#define SFONDO_FILE_MAX   64    /* byte di un nome di file, compreso lo 0 */
#define SFONDO_NOME_MAX   64    /* byte del nome mostrato, compreso lo 0 */

/* I file .jpg della cartella degli sfondi, in ordine alfabetico, in 'nomi'. Quanti sono. */
int sfondo_elenca(char nomi[][SFONDO_FILE_MAX], int max);

/* Il nome mostrato di un file: senza estensione e con la prima lettera maiuscola. */
void sfondo_nome_mostrato(const char *file, char *nome, int dimensione);

/* Il file scelto nelle preferenze ("" = Predefinito). */
void sfondo_leggi(char *file, int dimensione);

/* Salva la scelta ("" = Predefinito). 0, o -1 se il nome non e' valido o il file non si scrive. */
int sfondo_salva(const char *file);

/* Disegna la foto 'file' in 'pixel' (l x a, opaco), "riempi": scalata senza deformarla e
 * tagliata dove avanza, centrata. 0 se fatto, -1 se la foto non si legge ('pixel' non si tocca). */
int sfondo_disegna_foto(const char *file, unsigned int *pixel, int l, int a);

#endif
