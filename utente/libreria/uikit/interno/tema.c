/* UIKit: i due temi (Chiaro e Scuro), quello in uso, e la scelta letta dalle preferenze della
 * persona (vedi tema.h).
 *
 * I VALORI dei due temi sono qui sotto, copiati a mano dal file Figma "mshell - interfaccia"
 * (raccolta "Semantici", modi Chiaro e Scuro): se il disegno cambia, si ricopiano qui. Ce n'e'
 * UNA copia per programma, come di 'tema_corrente': cosi' "il tema in uso e' lo scuro?" si
 * chiede confrontando gli indirizzi (TEMA_SCURO_IN_USO, stile.h).
 *
 * IL TEMA (Chiaro o Scuro) si sceglie quando il programma parte, e di nuovo ogni volta che il
 * compositore dice che e' cambiato (TEMA_CAMBIATO, gkit.h): sta nel file
 * /data/users/1000/preferences/tema ("chiaro" o "scuro"), che scrive Impostazioni (tema_salva). Se il file
 * manca o non si legge (primo avvio, /data non ancora montato, permesso negato) resta il
 * chiaro, quello di partenza di tema_corrente.
 *
 * Il percorso e' quello della persona 1000 (UID_APPLICAZIONI) scritto per intero, non
 * "/home/..." : i servizi (compositore, scrivania) girano con un altro uid e per loro /home
 * sarebbe un'altra cartella, ma le preferenze sono sempre quelle della persona. Il sistema
 * (uid 0) puo' leggerle comunque.
 *
 * Perche' un "constructor": parte da solo prima di main, cosi' nessun programma deve
 * ricordarsi di chiamare tema_leggi() e tutti, applicazioni e servizi, hanno il tema giusto
 * fin dal primo disegno. (Si puo' richiamare a mano per rileggere il file.) Il constructor
 * sta in questo file insieme a 'tema_corrente', e non per caso: UIKit e' una libreria statica
 * (libuikit.a), da cui chi lega prende solo i file che usa. Ogni colore del tema legge
 * 'tema_corrente' (le macro di stile.h), quindi un programma che disegna con i colori del
 * tema prende anche questo file, e con lui il constructor. Un programma che non li usa (init,
 * i servizi senza grafica) non lo prende, e non legge le preferenze: non gli servono. */
#include <string.h>

#include "corekit/corekit.h"
#include "uikit/tema.h"

#define FILE_TEMA "/data/users/1000/preferences/tema"

const struct tema tema_chiaro = {
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

const struct tema tema_scuro = {
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

/* Il tema in uso: parte sul chiaro. */
const struct tema *tema_corrente = &tema_chiaro;

unsigned int colore_icona_di(const struct tema *t, int modo) {
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

void tema_scegli(const char *nome) {
    if (nome && nome[0] == 's' && nome[1] == 'c' && nome[2] == 'u' && nome[3] == 'r' && nome[4] == 'o')
        tema_corrente = &tema_scuro;
    else if (nome && nome[0] == 'c' && nome[1] == 'h' && nome[2] == 'i' && nome[3] == 'a' &&
             nome[4] == 'r' && nome[5] == 'o')
        tema_corrente = &tema_chiaro;
}

void tema_leggi(void) {
    char testo[16] = { 0 };
    int d = apri(FILE_TEMA, APRI_LEGGERE);
    if (d < 0) return;                       /* manca o illeggibile: resta com'e' */
    int n = leggi(d, testo, sizeof testo - 1);
    chiudi(d);
    if (n > 0) tema_scegli(testo);
}

int tema_salva(const char *nome) {
    if (!nome || (strcmp(nome, "chiaro") && strcmp(nome, "scuro"))) return -1;
    int l = (int)strlen(nome);
    int d = apri(FILE_TEMA, APRI_SCRIVERE | APRI_CREARE | APRI_SVUOTARE);
    if (d < 0) return -1;
    int n = scrivi_in(d, nome, (unsigned int)l);
    chiudi(d);
    return n == l ? 0 : -1;
}

__attribute__((constructor)) static void tema_all_avvio(void) { tema_leggi(); }
