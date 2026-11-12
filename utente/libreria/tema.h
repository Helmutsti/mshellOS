/* I due temi dell'interfaccia, Chiaro e Scuro: tutti i colori SEMANTICI del nuovo design.
 *
 * Un colore semantico dice A COSA SERVE, non com'e' fatto (superficie_elevata, testo_primario,
 * azione_sfondo...): i due temi gli danno valori diversi, e il codice che disegna non
 * cambia. Qui sotto c'e' un campo per ogni token di strumenti/grafica/token-figma.json (i
 * nomi sono quelli del file, con "/" e "-" trasformati in "_"), e le due istanze con i valori
 * della colonna 0 (Chiaro) e della colonna 1 (Scuro). Non scrivere i valori a mano: se il
 * disegno cambia, si rigenera da quel JSON.
 *
 * I colori sono 0xAARRGGBB come nel resto della libreria (disegno.h): OPACO(rgb) e
 * VELATO(rgb, alfa). Attenzione: il pennello li vuole NON premoltiplicati (e' lui, quando
 * disegna, a premoltiplicare: vedi il commento di struct tela), quindi VELATO tiene l'rgb
 * pieno e mette solo l'alfa (0.72 -> 184). Premoltiplicarli qui li scurirebbe due volte.
 *
 * LA SCELTA DEL TEMA e' a tempo di esecuzione: 'tema_corrente' punta a uno dei due, e le
 * macro di stile.h (SUPERFICIE_ELEVATA, TESTO_PRIMARIO...) leggono sempre da lui. Parte sul
 * chiaro; tema_leggi() (libreria.c) lo cambia in base al file delle preferenze, una volta
 * all'avvio del programma.
 *
 * 'tema_corrente' e' un simbolo DEBOLE definito qui nell'header: ogni file .c che lo include
 * ne porta una copia, e il linker le unisce in una sola. Cosi' l'header resta "header-only"
 * come gli altri, ma il puntatore e' uno per programma (e non uno per file .c, che sarebbe
 * un puntatore che il toolkit non vede cambiare quando lo cambia libreria.c). */
#ifndef TEMA_H
#define TEMA_H

#include "disegno.h"

struct tema {
    unsigned int superficie_scena;
    unsigned int superficie_elevata;
    unsigned int superficie_vetro;
    unsigned int superficie_incassata;
    unsigned int controllo_sfondo;
    unsigned int controllo_sfondo_sopra;
    unsigned int controllo_sfondo_premuto;
    unsigned int controllo_sfondo_disabilitato;
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
};

static const struct tema tema_chiaro = {
    .superficie_scena               = OPACO(0xE3E3E5),
    .superficie_elevata             = OPACO(0xFAFAFA),
    .superficie_vetro               = VELATO(0xFFFFFF, 184),
    .superficie_incassata           = OPACO(0xF2F2F3),
    .controllo_sfondo               = OPACO(0xEBEBEC),
    .controllo_sfondo_sopra         = OPACO(0xE3E3E5),
    .controllo_sfondo_premuto       = OPACO(0xD8D8DA),
    .controllo_sfondo_disabilitato  = OPACO(0xF2F2F3),
    .azione_sfondo                  = OPACO(0x4C4C52),
    .azione_sfondo_sopra            = OPACO(0x5F5F64),
    .azione_sfondo_premuto          = OPACO(0x414146),
    .azione_testo                   = OPACO(0xFAFAFA),
    .testo_primario                 = OPACO(0x414146),
    .testo_secondario               = OPACO(0x77777B),
    .testo_disabilitato             = OPACO(0xB8B8BB),
    .testo_inverso                  = OPACO(0xFAFAFA),
    .bordo_controllo                = OPACO(0xC4C4C7),
    .bordo_separatore               = OPACO(0xEBEBEC),
    .bordo_focus                    = OPACO(0x4C4C52),
    .cursore_riempimento            = OPACO(0x37373B),
    .cursore_contorno               = OPACO(0xFFFFFF),
    .oggetto_riempimento            = OPACO(0xF2F2F3),
    .oggetto_riempimento_forte      = OPACO(0xE3E3E5),
    .oggetto_tratto                 = OPACO(0x77777B),
    .scorrimento_cursore            = OPACO(0xB8B8BB),
    .scorrimento_cursore_sopra      = OPACO(0x9A9A9F),
    .scorrimento_cursore_trascinato = OPACO(0x77777B),
    .suggerimento_sfondo            = OPACO(0x4C4C52),
    .suggerimento_testo             = OPACO(0xFAFAFA),
    .suggerimento_testo_secondario  = OPACO(0xC4C4C7),
};

static const struct tema tema_scuro = {
    .superficie_scena               = OPACO(0x1C1C1E),
    .superficie_elevata             = OPACO(0x2B2B2E),
    .superficie_vetro               = VELATO(0x2B2B2E, 184),
    .superficie_incassata           = OPACO(0x242427),
    .controllo_sfondo               = OPACO(0x37373B),
    .controllo_sfondo_sopra         = OPACO(0x414146),
    .controllo_sfondo_premuto       = OPACO(0x2F2F33),
    .controllo_sfondo_disabilitato  = OPACO(0x242427),
    .azione_sfondo                  = OPACO(0xE3E3E5),
    .azione_sfondo_sopra            = OPACO(0xF2F2F3),
    .azione_sfondo_premuto          = OPACO(0xD8D8DA),
    .azione_testo                   = OPACO(0x242427),
    .testo_primario                 = OPACO(0xEBEBEC),
    .testo_secondario               = OPACO(0x9A9A9F),
    .testo_disabilitato             = OPACO(0x5F5F64),
    .testo_inverso                  = OPACO(0x242427),
    .bordo_controllo                = OPACO(0x4C4C52),
    .bordo_separatore               = OPACO(0x37373B),
    .bordo_focus                    = OPACO(0xE3E3E5),
    .cursore_riempimento            = OPACO(0xFAFAFA),
    .cursore_contorno               = OPACO(0x1C1C1E),
    .oggetto_riempimento            = OPACO(0x2F2F33),
    .oggetto_riempimento_forte      = OPACO(0x414146),
    .oggetto_tratto                 = OPACO(0x9A9A9F),
    .scorrimento_cursore            = OPACO(0x5F5F64),
    .scorrimento_cursore_sopra      = OPACO(0x77777B),
    .scorrimento_cursore_trascinato = OPACO(0x9A9A9F),
    .suggerimento_sfondo            = OPACO(0xE3E3E5),
    .suggerimento_testo             = OPACO(0x242427),
    .suggerimento_testo_secondario  = OPACO(0x77777B),
};
/* Il tema in uso. Debole (vedi sopra); si cambia con tema_leggi() o, nelle prove, a mano. */
__attribute__((weak)) const struct tema *tema_corrente = &tema_chiaro;

/* Sceglie il tema per nome ("chiaro" o "scuro"); un nome che non si conosce non cambia niente. */
static inline void tema_scegli(const char *nome) {
    if (nome && nome[0] == 's' && nome[1] == 'c' && nome[2] == 'u' && nome[3] == 'r' && nome[4] == 'o')
        tema_corrente = &tema_scuro;
    else if (nome && nome[0] == 'c' && nome[1] == 'h' && nome[2] == 'i' && nome[3] == 'a' &&
             nome[4] == 'r' && nome[5] == 'o')
        tema_corrente = &tema_chiaro;
}

/* Sceglie il tema leggendo /data/users/1000/preferences/tema (libreria.c). La chiama da sola,
 * prima di main, ogni programma che lega la libreria. */
void tema_leggi(void);

#endif
