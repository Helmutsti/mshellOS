/* I due temi dell'interfaccia, Chiaro e Scuro: tutti i colori SEMANTICI del nuovo design.
 *
 * Un colore semantico dice A COSA SERVE, non com'e' fatto (superficie_elevata, testo_primario,
 * azione_sfondo...): i due temi gli danno valori diversi, e il codice che disegna non
 * cambia. Qui sotto c'e' un campo per ogni colore semantico del file Figma "mshell -
 * interfaccia" (raccolta "Semantici"; i nomi sono quelli di Figma, con "/" e "-" trasformati
 * in "_"). Le due istanze con i valori dei modi Chiaro e Scuro sono in interno/tema.c: i
 * valori sono copiati a mano dal disegno, e se il disegno cambia si ricopiano li'.
 *
 * Tre famiglie di colori:
 *   - le superfici, i testi, i bordi e i controlli (la scala di grigi);
 *   - i colori di RUOLO (successo, avviso, informazione, pericolo): dicono cosa significa
 *     una cosa. Ognuno ha il pieno per i pulsanti (sfondo, sopra, premuto, testo) e il
 *     tenue per i fondi leggeri (tenue, testo_tenue);
 *   - i colori di SISTEMA (rosso, arancio, giallo, verde, menta, azzurro, blu, viola, rosa,
 *     marrone, grigio): servono a distinguere le cose (etichette, grafici, icone). Ognuno ha
 *     saturo, pastello e il testo da mettere sul pastello.
 *
 * I colori sono 0xAARRGGBB come in GKit (gkit_disegno.h): OPACO(rgb) e
 * VELATO(rgb, alfa). Attenzione: il pennello li vuole NON premoltiplicati (e' lui, quando
 * disegna, a premoltiplicare: vedi il commento di struct tela), quindi VELATO tiene l'rgb
 * pieno e mette solo l'alfa (0.72 -> 184). Premoltiplicarli qui li scurirebbe due volte.
 *
 * LA SCELTA DEL TEMA e' a tempo di esecuzione: 'tema_corrente' punta a uno dei due, e le
 * macro di stile.h (SUPERFICIE_ELEVATA, TESTO_PRIMARIO...) leggono sempre da lui. Parte sul
 * chiaro; tema_leggi() lo cambia in base al file delle preferenze, all'avvio del
 * programma e ogni volta che il compositore dice che e' cambiato (TEMA_CAMBIATO).
 * Per questo un colore NON si copia in una variabile che dura (un campo di un widget):
 * si chiede alle macro quando si disegna, cosi' segue il tema anche quando cambia.
 * 'tema_corrente' e i due temi sono definiti una volta sola, in interno/tema.c (nella
 * libreria compilata, libuikit.a): uno per programma. */
#ifndef TEMA_H
#define TEMA_H

#include "gkit/gkit_disegno.h"

/* I colori di sistema, nell'ordine del disegno. Si usano come indice di tema->sistema[]
 * (per esempio il colore di un'etichetta salvato come numero). */
enum colore_sistema {
    COLORE_ROSSO, COLORE_ARANCIO, COLORE_GIALLO, COLORE_VERDE, COLORE_MENTA, COLORE_AZZURRO,
    COLORE_BLU, COLORE_VIOLA, COLORE_ROSA, COLORE_MARRONE, COLORE_GRIGIO,
    COLORI_SISTEMA   /* quanti sono */
};

struct colore_di_sistema {
    unsigned int saturo;           /* etichette, grafici, icone, accenti */
    unsigned int pastello;         /* fondi leggeri, evidenziazioni */
    unsigned int testo_pastello;   /* testo e icone sul pastello (almeno 5:1) */
};

struct tema {
    unsigned int superficie_scena;
    unsigned int superficie_elevata;
    unsigned int superficie_vetro;
    unsigned int superficie_incassata;
    unsigned int controllo_sfondo;
    unsigned int controllo_sfondo_sopra;
    unsigned int controllo_sfondo_premuto;
    unsigned int controllo_sfondo_disabilitato;
    unsigned int controllo_selezione_vetro;   /* la voce scelta della barra laterale, sul vetro */
    unsigned int controllo_sopra_vetro;       /* la voce sotto il puntatore, sul vetro */
    unsigned int azione_sfondo;
    unsigned int azione_sfondo_sopra;
    unsigned int azione_sfondo_premuto;
    unsigned int azione_testo;
    unsigned int testo_primario;
    unsigned int testo_secondario;
    unsigned int testo_disabilitato;
    unsigned int testo_inverso;
    unsigned int bordo_controllo;
    unsigned int bordo_separatore;
    unsigned int bordo_focus;
    unsigned int cursore_riempimento;
    unsigned int cursore_contorno;
    unsigned int oggetto_riempimento;
    unsigned int oggetto_riempimento_forte;
    unsigned int oggetto_tratto;
    unsigned int scorrimento_cursore;
    unsigned int scorrimento_cursore_sopra;
    unsigned int scorrimento_cursore_trascinato;
    unsigned int suggerimento_sfondo;
    unsigned int suggerimento_testo;
    unsigned int suggerimento_testo_secondario;
    unsigned int superficie_velo;   /* sopra una finestra bloccata da un avviso bloccante */
    /* i colori di ruolo */
    unsigned int successo_sfondo, successo_sfondo_sopra, successo_sfondo_premuto, successo_testo;
    unsigned int successo_tenue, successo_testo_tenue;
    unsigned int avviso_sfondo, avviso_sfondo_sopra, avviso_sfondo_premuto, avviso_testo;
    unsigned int avviso_tenue, avviso_testo_tenue;
    unsigned int informazione_sfondo, informazione_sfondo_sopra, informazione_sfondo_premuto;
    unsigned int informazione_testo, informazione_tenue, informazione_testo_tenue;
    unsigned int pericolo_sfondo, pericolo_sfondo_sopra, pericolo_sfondo_premuto, pericolo_testo;
    unsigned int pericolo_tenue, pericolo_testo_tenue;
    /* i colori di sistema, indicizzati da enum colore_sistema */
    struct colore_di_sistema sistema[COLORI_SISTEMA];
};

/* I due temi. I valori (copiati a mano dal disegno) sono in interno/tema.c: chi disegna non li
 * legge direttamente, ma attraverso 'tema_corrente' (le macro di stile.h). */
extern const struct tema tema_chiaro;
extern const struct tema tema_scuro;

/* Il tema in uso: punta a tema_chiaro o a tema_scuro, uno per programma. Parte sul chiaro; si
 * cambia con tema_leggi() o tema_scegli() (o, nelle prove, a mano). */
extern const struct tema *tema_corrente;

/* IL COLORE DI UN'ICONA: la raccolta "Colore icona" del file Figma. Ha una sola variabile
 * (icona/colore) con otto modi, e ogni modo punta a un colore semantico qui sopra: e' per
 * questo che non ha valori suoi e vale da sola nei due temi. Nei componenti del disegno ogni
 * icona prende il colore da qui, con il modo scelto dal componente (il pulsante Primario usa
 * Inverso, l'intestazione di colonna Secondario...): il codice fa lo stesso, sceglie il modo
 * e chiede il colore a colore_icona_di (o alla macro COLORE_ICONA di stile.h). L'ordine e'
 * quello dei modi in Figma. */
enum colore_icona {
    COLORE_ICONA_PRIMARIO,      /* testo/primario */
    COLORE_ICONA_INVERSO,       /* azione/testo: sopra il fondo scuro di un'azione */
    COLORE_ICONA_ATTENUATO,     /* testo/disabilitato */
    COLORE_ICONA_SECONDARIO,    /* testo/secondario */
    COLORE_ICONA_PERICOLO,      /* pericolo/testo */
    COLORE_ICONA_SUCCESSO,      /* successo/testo */
    COLORE_ICONA_AVVISO,        /* avviso/testo */
    COLORE_ICONA_INFORMAZIONE,  /* informazione/testo */
    COLORI_ICONA                /* quanti sono */
};

/* Il colore del modo 'modo' (COLORE_ICONA_...) nel tema 't'. Un modo che non si conosce da'
 * il primario. */
unsigned int colore_icona_di(const struct tema *t, int modo);

/* Sceglie il tema per nome ("chiaro" o "scuro"); un nome che non si conosce non cambia niente. */
void tema_scegli(const char *nome);

/* Sceglie il tema leggendo /data/users/1000/preferences/tema. La chiama da sola, prima di
 * main, ogni programma che usa i colori del tema (interno/tema.c dice come); e la richiama
 * quando il compositore dice che il tema e' cambiato (TEMA_CAMBIATO, gkit.h: UIKit lo fa da
 * sola). */
void tema_leggi(void);

/* Scrive il tema ("chiaro" o "scuro") nelle preferenze. Non cambia quello in uso: per farlo
 * cambiare a tutto il sistema c'e' uikit_cambia_tema (uikit.h). 0, o -1 (un nome che non si
 * conosce, o il file non si scrive). */
int tema_salva(const char *nome);

#endif
