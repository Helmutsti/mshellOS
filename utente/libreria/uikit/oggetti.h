/* GLI OGGETTI: le icone di file e cartelle del disegno nuovo (il componente "Oggetto" del file
 * Figma, pagina "Componenti base"). Due misure ottiche: GRANDE 48 px (tratto 2, per i tile
 * della griglia) e PICCOLA 24 px (tratto 1,5, per le righe delle tabelle e gli elenchi).
 *
 * Il tipo e' il piu' specifico che c'e' (la regola "la variante piu' specifica"):
 *   - una cartella con un ruolo ha il suo simbolo: la casa (Home), la Scrivania, i Documenti,
 *     le Immagini, i Download, le applicazioni; le altre sono la cartella semplice;
 *   - un file con un'estensione che il disegno conosce (TXT, PDF, PNG, ZIP, BIN) ha la sua
 *     variante: nella misura grande e' il foglio con la scritta, nella piccola un simbolo;
 *     gli altri sono il file semplice.
 *
 * I COLORI vengono dal tema (oggetto/riempimento-forte, oggetto/riempimento, oggetto/tratto):
 * i dati (i file in /system/resources/icons/objects, da strumenti/grafica/genera.py) dicono per ogni pixel quanto c'e'
 * di ognuno dei tre, e li si mescola qui con i colori del tema in uso. Livello piatto: niente
 * ombre. Lo stato (sopra, scelto) non e' dell'oggetto ma di chi lo contiene (il tile, la riga).
 *
 * C'e' anche il CAMPO DEL NOME NUOVO (lo stato "Rinomina" dell'icona, che il disegno elenca tra
 * quelli da aggiungere): un campo di testo incassato con il fuoco, come campo_disegna, che
 * prende il posto del nome nel tile o nella riga. Lo usano solo la tabella e la griglia: e'
 * dichiarato in interno/nucleo.h, come il testo tagliato con "..." (testo_tagliato) che usano le
 * righe e le voci dei componenti. Il codice e' in interno/oggetti.c. */
#ifndef OGGETTI_H
#define OGGETTI_H

enum {
    OGGETTO_FILE, OGGETTO_CARTELLA,
    OGGETTO_TXT, OGGETTO_PDF, OGGETTO_PNG, OGGETTO_ZIP, OGGETTO_BIN,
    OGGETTO_HOME, OGGETTO_SCRIVANIA, OGGETTO_DOCUMENTI, OGGETTO_IMMAGINI, OGGETTO_DOWNLOAD,
    OGGETTO_APPLICAZIONI,
    OGGETTI                            /* quanti sono */
};
enum { OGGETTO_GRANDE, OGGETTO_PICCOLO };

#define OGGETTO_LATO_GRANDE  ICONA_OGGETTO            /* 48 */
#define OGGETTO_LATO_PICCOLO ICONA_OGGETTO_PICCOLA    /* 24 */
#define OGGETTO_ETICHETTA_Y  23    /* la scritta (TXT...) sul foglio grande: la cima, dal disegno */

/* Disegna l'oggetto 'quale' (OGGETTO_...; uno fuori elenco diventa il file semplice) nella
 * misura OGGETTO_GRANDE o OGGETTO_PICCOLO, con l'angolo in (x, y), nei colori del tema. */
void oggetto(struct pennello *p, int quale, int misura, int x, int y);

/* 1 se l'oggetto 'quale' e' una cartella (la semplice o una con un ruolo), se no 0. */
int oggetto_e_cartella(int quale);

/* L'elemento di un file (elemento_file.h) con un oggetto come icona: l'oggetto grande (48)
 * centrato nel posto dell'icona (56). Lo usa la scrivania per le sue icone. */
void elemento_disegna_oggetto(struct pennello *p, int x, int y, int alto, int quale,
                              const char *nome, int stato);

/* --- quale oggetto: dal nome di un file, dal percorso di una cartella --- */

/* Un file, dal nome: la variante della sua estensione (OGGETTO_TXT...), se il disegno ce l'ha;
 * se no OGGETTO_FILE. */
int oggetto_di_file(const char *nome);

/* La casa della persona, come la vedono le app (vedi STATO.md, "Le cartelle nel sistema"). */
#define OGGETTO_CASA "/home"

/* Una cartella, dal suo percorso intero: le cartelle con un ruolo (la casa e le sue
 * Scrivania, Documenti, Immagini, Download; le cartelle delle applicazioni) hanno il loro
 * simbolo, le altre sono la cartella semplice. */
int oggetto_di_cartella(const char *percorso);

#endif
