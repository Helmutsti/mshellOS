/* I due temi dell'interfaccia, Chiaro e Scuro: tutti i colori SEMANTICI del nuovo design.
 *
 * Un colore semantico dice A COSA SERVE, non com'e' fatto (superficie_elevata, testo_primario,
 * azione_sfondo...): i due temi gli danno valori diversi, e il codice che disegna non
 * cambia. Qui sotto c'e' un campo per ogni colore semantico del file Figma "mshell -
 * interfaccia" (raccolta "Semantici"; i nomi sono quelli di Figma, con "/" e "-" trasformati
 * in "_"), e le due istanze con i valori dei modi Chiaro e Scuro. I valori sono copiati a
 * mano dal disegno: se il disegno cambia, si ricopiano qui.
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
 * chiaro; tema_leggi() (tema.c) lo cambia in base al file delle preferenze, all'avvio del
 * programma e ogni volta che il compositore dice che e' cambiato (TEMA_CAMBIATO).
 * Per questo un colore NON si copia in una variabile che dura (un campo di un widget):
 * si chiede alle macro quando si disegna, cosi' segue il tema anche quando cambia.
 *
 * 'tema_corrente' e' un simbolo DEBOLE definito qui nell'header: ogni file .c che lo include
 * ne porta una copia, e il linker le unisce in una sola. Cosi' l'header resta "header-only"
 * come gli altri, ma il puntatore e' uno per programma (e non uno per file .c, che sarebbe
 * un puntatore che UIKit non vede cambiare quando lo cambia tema.c). */
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

static const struct tema tema_chiaro = {
    .superficie_scena               = OPACO(0xE3E3E5),
    .superficie_elevata             = OPACO(0xFAFAFA),
    .superficie_vetro               = VELATO(0xFFFFFF, 184),
    .superficie_incassata           = OPACO(0xF2F2F3),
    .controllo_sfondo               = OPACO(0xEBEBEC),
    .controllo_sfondo_sopra         = OPACO(0xE3E3E5),
    .controllo_sfondo_premuto       = OPACO(0xD8D8DA),
    .controllo_sfondo_disabilitato  = OPACO(0xF2F2F3),
    .controllo_selezione_vetro      = VELATO(0x000000, 20),    /*  8% */
    .controllo_sopra_vetro          = VELATO(0x000000, 10),    /*  4% */
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
    .superficie_velo                = VELATO(0x000000, 51),    /* 20% */
    .successo_sfondo                = OPACO(0x166534),
    .successo_sfondo_sopra          = OPACO(0x15803D),
    .successo_sfondo_premuto        = OPACO(0x14532D),
    .successo_testo                 = OPACO(0xFFFFFF),
    .successo_tenue                 = OPACO(0xDCFCE7),
    .successo_testo_tenue           = OPACO(0x166534),
    .avviso_sfondo                  = OPACO(0xF59E0B),
    .avviso_sfondo_sopra            = OPACO(0xFBBF24),
    .avviso_sfondo_premuto          = OPACO(0xD97706),
    .avviso_testo                   = OPACO(0x1C1C1E),         /* scuro: sul giallo il bianco non si legge */
    .avviso_tenue                   = OPACO(0xFEF3C7),
    .avviso_testo_tenue             = OPACO(0x92400E),
    .informazione_sfondo            = OPACO(0x1D4ED8),
    .informazione_sfondo_sopra      = OPACO(0x2563EB),
    .informazione_sfondo_premuto    = OPACO(0x1E40AF),
    .informazione_testo             = OPACO(0xFFFFFF),
    .informazione_tenue             = OPACO(0xDBEAFE),
    .informazione_testo_tenue       = OPACO(0x1E40AF),
    .pericolo_sfondo                = OPACO(0xB91C1C),
    .pericolo_sfondo_sopra          = OPACO(0xDC2626),
    .pericolo_sfondo_premuto        = OPACO(0x991B1B),
    .pericolo_testo                 = OPACO(0xFFFFFF),
    .pericolo_tenue                 = OPACO(0xFEE2E2),
    .pericolo_testo_tenue           = OPACO(0x991B1B),
    .sistema = {
        [COLORE_ROSSO]   = { OPACO(0xFF3B30), OPACO(0xFFD4D1), OPACO(0x99231D) },
        [COLORE_ARANCIO] = { OPACO(0xFF9500), OPACO(0xFFE8C7), OPACO(0x8C5200) },
        [COLORE_GIALLO]  = { OPACO(0xFFCC00), OPACO(0xFFF4C7), OPACO(0x735C00) },
        [COLORE_VERDE]   = { OPACO(0x34C759), OPACO(0xD2F3DA), OPACO(0x1D6D31) },
        [COLORE_MENTA]   = { OPACO(0x00C7BE), OPACO(0xC7F3F1), OPACO(0x006D69) },
        [COLORE_AZZURRO] = { OPACO(0x32ADE6), OPACO(0xD2EDF9), OPACO(0x1E688A) },
        [COLORE_BLU]     = { OPACO(0x007AFF), OPACO(0xC7E2FF), OPACO(0x004999) },
        [COLORE_VIOLA]   = { OPACO(0xAF52DE), OPACO(0xEDD9F8), OPACO(0x693185) },
        [COLORE_ROSA]    = { OPACO(0xFF2D55), OPACO(0xFFD1DA), OPACO(0x991B33) },
        [COLORE_MARRONE] = { OPACO(0xA2845E), OPACO(0xEBE4DC), OPACO(0x614F38) },
        [COLORE_GRIGIO]  = { OPACO(0x8E8E93), OPACO(0xE6E6E7), OPACO(0x555558) },
    },
};

static const struct tema tema_scuro = {
    .superficie_scena               = OPACO(0x1C1C1E),
    .superficie_elevata             = OPACO(0x2B2B2E),
    .superficie_vetro               = VELATO(0x414146, 184),   /* piu' chiaro della scena, se no la finestra non si stacca */
    .superficie_incassata           = OPACO(0x242427),
    .controllo_sfondo               = OPACO(0x37373B),
    .controllo_sfondo_sopra         = OPACO(0x414146),
    .controllo_sfondo_premuto       = OPACO(0x2F2F33),
    .controllo_sfondo_disabilitato  = OPACO(0x242427),
    .controllo_selezione_vetro      = VELATO(0xFFFFFF, 31),    /* 12% */
    .controllo_sopra_vetro          = VELATO(0xFFFFFF, 15),    /*  6% */
    .azione_sfondo                  = OPACO(0xC4C4C7),   /* grigio chiaro, non bianco */
    .azione_sfondo_sopra            = OPACO(0xD8D8DA),
    .azione_sfondo_premuto          = OPACO(0xB8B8BB),
    .azione_testo                   = OPACO(0x242427),
    .testo_primario                 = OPACO(0xEBEBEC),
    .testo_secondario               = OPACO(0xB8B8BB),   /* 6:1 sul vetro */
    .testo_disabilitato             = OPACO(0x89898D),   /* 3,4:1 sul vetro */
    .testo_inverso                  = OPACO(0x242427),
    .bordo_controllo                = OPACO(0x4C4C52),
    .bordo_separatore               = OPACO(0x55555B),
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
    .superficie_velo                = VELATO(0x000000, 71),    /* 28% */
    .successo_sfondo                = OPACO(0x166534),
    .successo_sfondo_sopra          = OPACO(0x15803D),
    .successo_sfondo_premuto        = OPACO(0x14532D),
    .successo_testo                 = OPACO(0xFFFFFF),
    .successo_tenue                 = OPACO(0x052E16),
    .successo_testo_tenue           = OPACO(0x86EFAC),
    .avviso_sfondo                  = OPACO(0xF59E0B),
    .avviso_sfondo_sopra            = OPACO(0xFBBF24),
    .avviso_sfondo_premuto          = OPACO(0xD97706),
    .avviso_testo                   = OPACO(0x1C1C1E),
    .avviso_tenue                   = OPACO(0x451A03),
    .avviso_testo_tenue             = OPACO(0xFCD34D),
    .informazione_sfondo            = OPACO(0x1D4ED8),
    .informazione_sfondo_sopra      = OPACO(0x2563EB),
    .informazione_sfondo_premuto    = OPACO(0x1E40AF),
    .informazione_testo             = OPACO(0xFFFFFF),
    .informazione_tenue             = OPACO(0x172554),
    .informazione_testo_tenue       = OPACO(0x93C5FD),
    .pericolo_sfondo                = OPACO(0xB91C1C),
    .pericolo_sfondo_sopra          = OPACO(0xDC2626),
    .pericolo_sfondo_premuto        = OPACO(0x991B1B),
    .pericolo_testo                 = OPACO(0xFFFFFF),
    .pericolo_tenue                 = OPACO(0x450A0A),
    .pericolo_testo_tenue           = OPACO(0xFCA5A5),
    .sistema = {
        [COLORE_ROSSO]   = { OPACO(0xFF453A), OPACO(0x602826), OPACO(0xFF8F89) },
        [COLORE_ARANCIO] = { OPACO(0xFF9F0A), OPACO(0x604318), OPACO(0xFFC56C) },
        [COLORE_GIALLO]  = { OPACO(0xFFD60A), OPACO(0x605418), OPACO(0xFFE66C) },
        [COLORE_VERDE]   = { OPACO(0x30D158), OPACO(0x22522F), OPACO(0x83E39B) },
        [COLORE_MENTA]   = { OPACO(0x63E6E2), OPACO(0x315959), OPACO(0xA1F0EE) },
        [COLORE_AZZURRO] = { OPACO(0x64D2FF), OPACO(0x325362), OPACO(0xA2E4FF) },
        [COLORE_BLU]     = { OPACO(0x0A84FF), OPACO(0x173B62), OPACO(0x6CB5FF) },
        [COLORE_VIOLA]   = { OPACO(0xBF5AF2), OPACO(0x4D2F5E), OPACO(0xD99CF7) },
        [COLORE_ROSA]    = { OPACO(0xFF375F), OPACO(0x602432), OPACO(0xFF879F) },
        [COLORE_MARRONE] = { OPACO(0xAC8E68), OPACO(0x473E34), OPACO(0xCDBBA4) },
        [COLORE_GRIGIO]  = { OPACO(0x98989D), OPACO(0x414144), OPACO(0xC1C1C4) },
    },
};
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

static inline unsigned int colore_icona_di(const struct tema *t, int modo) {
    switch (modo) {
    case COLORE_ICONA_INVERSO:      return t->azione_testo;
    case COLORE_ICONA_ATTENUATO:    return t->testo_disabilitato;
    case COLORE_ICONA_SECONDARIO:   return t->testo_secondario;
    case COLORE_ICONA_PERICOLO:     return t->pericolo_testo;
    case COLORE_ICONA_SUCCESSO:     return t->successo_testo;
    case COLORE_ICONA_AVVISO:       return t->avviso_testo;
    case COLORE_ICONA_INFORMAZIONE: return t->informazione_testo;
    default:                        return t->testo_primario;
    }
}

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

/* Sceglie il tema leggendo /data/users/1000/preferences/tema (tema.c). La chiama da sola,
 * prima di main, ogni programma che lega la libreria; e la richiama quando il compositore
 * dice che il tema e' cambiato (TEMA_CAMBIATO, gkit.h: UIKit lo fa da sola). */
void tema_leggi(void);

/* Scrive il tema ("chiaro" o "scuro") nelle preferenze. Non cambia quello in uso: per farlo
 * cambiare a tutto il sistema c'e' uikit_cambia_tema (uikit.h). 0, o -1 (un nome che non si
 * conosce, o il file non si scrive). */
int tema_salva(const char *nome);

#endif
