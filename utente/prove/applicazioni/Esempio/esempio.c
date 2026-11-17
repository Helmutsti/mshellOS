/* esempio: la prima applicazione con una finestra vera.
 *
 * Disegna da sola TUTTA la sua finestra, con gli strumenti di disegno di GKit, nella tela
 * che le da' la finestra di GKit: la barra con il titolo (blu quando ha il
 * fuoco, grigia quando no) e il contenuto. Non usa UIKit, quindi si fa tutto da
 * sola: anche accorgersi che si e' premuto sulla barra e chiedere al compositore di
 * trascinarla.
 * Un clic nel contenuto mette un quadretto dove si e' cliccato; un tasto cambia il
 * colore di sfondo (e i tasti o, s, m chiedono anche al picker un file: tasto_picker).
 *
 * OGNI VOLTA SI RIDISEGNA TUTTO. Le tele sono due e si alternano: quella che si riceve
 * contiene il disegno di DUE volte fa, non dell'ultima. Per questo l'applicazione tiene
 * il suo stato (i quadretti, il colore, il fuoco) e ridisegna tutto da capo. */
/* Solo GKit (la finestra e gli strumenti di disegno), piu' il font del titolo letto con GKit
 * dalle risorse del sistema (/system/resources/fonts): niente stile, temi, materiali o
 * componenti di UIKit. */
#include "gkit/gkit.h"
#include "gkit/gkit_disegno.h"
#include "corekit/catalog.h"
#include "corekit/sistema/network.h"   /* rete_stato: la porta verso la rete (D_PORTA_NETWORK) */
#include "corekit/tasks.h"             /* TASKS_NEGATO: elenco_processi senza il permesso */
#include "corekit/picker.h"            /* picker_apri, picker_salva: la finestra Apri/Salva */
#include <stdio.h>

#define LARGHEZZA 320
#define ALTEZZA   220
#define BARRA     30      /* altezza della barra */
#define LATO      10
#define MAX_QUADRETTI 64
#define TITOLO    "esempio"

static const unsigned int sfondi[] = { 0xF4E8C8, 0xC8E8F4, 0xD8F4C8, 0xF4C8D8 };
static int sfondo, fuoco;
static int qx[MAX_QUADRETTI], qy[MAX_QUADRETTI], quadretti;
static struct font font_titolo;   /* se il file manca: il font di riserva di GKit */

/* Il suo aspetto lo decide lei, e non e' quello del sistema: carta opaca, barra
 * colorata, titolo a sinistra. (Gli angoli arrotondati come quelli del sistema, perche'
 * l'ombra intorno la disegna il compositore con quella forma.) */
static void disegna(unsigned int *pixel) {
    struct tela t = { pixel, LARGHEZZA, ALTEZZA };
    struct pennello p;
    pennello_prepara(&p, &t);
    pulisci(&p);

    struct tondo tutta = tondo(0, 0, LARGHEZZA, ALTEZZA, 12);
    struct tondo barra = { 0, 0, LARGHEZZA, BARRA, { 12, 12, 0, 0 } };
    pennello_colore(&p, OPACO(sfondi[sfondo]));
    riempi_forma(&p, &tutta);
    pennello_colore(&p, OPACO(fuoco ? 0x3A6EA5 : 0x707880));
    riempi_forma(&p, &barra);
    pennello_colore(&p, OPACO(0xFFFFFF));
    testo(&p, &font_titolo, 14, (BARRA - font_titolo.altezza) / 2, TITOLO);

    /* il contenuto: ritagliato, cosi' i quadretti vicino al bordo non lo coprono */
    pennello_ritaglia(&p, 0, BARRA, LARGHEZZA, ALTEZZA - BARRA);
    pennello_colore(&p, OPACO(0x803030));
    for (int q = 0; q < quadretti; q++) riempi_tondo(&p, qx[q], qy[q], LATO, LATO, 3);
}

/* I TASTI DEL PICKER (la prova picker, test/prove/picker.py): Esempio non ha nessun permesso sui
 * file, e chiede al servizio picker (dalla sua porta, D_PORTA_PICKER) di far scegliere un file
 * alla persona. Ogni esito lo scrive nel registro:
 *   o  Apri: legge tutto il file ricevuto e scrive "aperto <nome>, <byte> byte"
 *   s  Salva, proponendo "prova.txt": scrive "ciao" nel file ricevuto, "salvato <nome>"
 *   m  Salva proponendo "../fuori.txt": il picker deve rifiutarlo (PICKER_NON_VALIDO), senza
 *      mostrare niente e senza creare niente
 * Finche' la persona sceglie, Esempio e' fermo dentro la funzione (come un'app vera che aspetta). */
static void tasto_picker(unsigned int tasto) {
    char nome[256], riga[320];
    if (tasto == 'o') {
        int d = picker_apri(nome, sizeof nome);
        if (d == PICKER_ANNULLATO) { scrivi_riga("esempio", "apertura annullata", 0); return; }
        if (d < 0) { scrivi_riga("esempio", "apertura non riuscita", 0); return; }
        long byte = 0;
        char pezzo[512];
        int n;
        while ((n = leggi(d, pezzo, sizeof pezzo)) > 0) byte += n;
        chiudi(d);
        snprintf(riga, sizeof riga, "%s, %ld byte", nome, byte);
        scrivi_riga("esempio", "aperto ", riga);
    } else if (tasto == 's') {
        int d = picker_salva("prova.txt", nome, sizeof nome);
        if (d == PICKER_ANNULLATO) { scrivi_riga("esempio", "salvataggio annullato", 0); return; }
        if (d < 0) { scrivi_riga("esempio", "salvataggio non riuscito", 0); return; }
        int scritti = scrivi_in(d, "ciao", 4);
        chiudi(d);
        scrivi_riga("esempio", scritti == 4 ? "salvato " : "scrittura non riuscita in ", nome);
    } else if (tasto == 'm') {
        int d = picker_salva("../fuori.txt", nome, sizeof nome);
        if (d >= 0) chiudi(d);
        scrivi_riga("esempio", "salva ../fuori.txt: ", d == PICKER_NON_VALIDO ? "non valido" : "NON rifiutato");
    }
}

static int ridisegna(struct finestra *f) {
    unsigned int *tela = finestra_tela(f);
    if (!tela) return 0;
    disegna(tela);
    return finestra_mostra(f) >= 0;
}

int main(void) {
    char percorso[PERCORSO_RISORSA_MAX];
    percorso_risorsa(percorso, sizeof percorso, "fonts/inter-semibold-13.font");
    font_da_file(percorso, &font_titolo);

    struct finestra f;
    if (finestra_apri(&f, LARGHEZZA, ALTEZZA, TITOLO) < 0) {
        scrivi_testo("esempio: non riesco ad aprire la finestra\n");
        return 0;
    }
    if (!ridisegna(&f)) return 0;

    /* il tema da catalog, solo per scriverlo nel registro: la prova applicazioni controlla che
     * arrivi dalla porta che il launcher le ha dato (catalog.h, D_PORTA_CATALOG) */
    struct catalog_valori impostazioni;
    if (catalog_leggi(&impostazioni) == 0) scrivi_riga("esempio", "tema da catalog: ", impostazioni.tema);

    /* la rete, dalla porta che il launcher le ha dato solo perche' il manifesto ha network.use
     * (network.h, D_PORTA_NETWORK): se il servizio risponde, una riga sola nel registro */
    struct stato_rete rete;
    if (rete_stato(&rete) == RETE_FATTO) scrivi_riga("esempio", "rete risponde", 0);

    /* I PERMESSI CHE NON HA (la prova applicazioni controlla i rifiuti): cambiare il tema chiede
     * settings.system, riavviare la rete network.settings, e Esempio non li dichiara (e sono di
     * sistema: non li avrebbe comunque, non e' un'app del sistema) */
    scrivi_riga("esempio", "cambiare il tema: ", catalog_cambia(CATALOG_TEMA_SCURO, 0) == CATALOG_NEGATO ? "negato" : "NON negato");
    scrivi_riga("esempio", "riavviare la rete: ", rete_riavvia() == RETE_NON_PERMESSO ? "negato" : "NON negato");

    /* Anche l'elenco dei processi chiede un permesso (processes.view), che Esempio non ha: il launcher
     * non le da' la porta verso tasks, quindi collega_servizio prova a collegarsi per NOME. Ma
     * Esempio e' nel suo recinto, dove gli indirizzi dei servizi (/run/mshell) non ci sono: tasks
     * non si raggiunge proprio ("irraggiungibile"). Senza recinto tasks avrebbe risposto
     * "negato" (un programma della persona non ha permessi di sistema): quello si scrive a parte,
     * e un elenco ricevuto sarebbe un errore ("NON negato"). */
    static struct info_processo processi[8];
    int elenco = elenco_processi(processi, 8);
    scrivi_riga("esempio", "elenco dei processi: ",
                elenco == TASKS_NEGATO ? "negato" : elenco < 0 ? "irraggiungibile" : "NON negato");

    unsigned int pulsanti_prima = 0;
    struct notizia n;
    while (finestra_notizia(&f, &n)) {
        int cambiato = 0;
        if (n.tipo == PUNTATORE) {
            int clic = (n.pulsanti & PULSANTE_SINISTRO) && !(pulsanti_prima & PULSANTE_SINISTRO);
            pulsanti_prima = n.pulsanti;
            if (clic && n.y < BARRA) {
                finestra_trascinami(&f);   /* sulla barra: ci sposta il compositore */
            } else if (clic && quadretti < MAX_QUADRETTI) {
                qx[quadretti] = n.x - LATO / 2;
                qy[quadretti] = n.y - LATO / 2;
                quadretti++;
                cambiato = 1;
            }
        } else if (n.tipo == USCITO) {
            pulsanti_prima = 0;   /* i pulsanti si sapranno di nuovo quando rientra */
        } else if (n.tipo == TASTO) {
            sfondo = (sfondo + 1) % (int)(sizeof sfondi / sizeof sfondi[0]);
            cambiato = 1;
            tasto_picker(n.tasto);
        } else if (n.tipo == FUOCO) {
            fuoco = n.fuoco;
            cambiato = 1;
        }
        if (cambiato && !ridisegna(&f)) return 0;
    }
    return 0;   /* il compositore ha chiuso */
}
