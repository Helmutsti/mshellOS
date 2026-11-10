/* contatore: la prima applicazione fatta con il toolkit.
 *
 * Un numero e due pulsanti, - e +. Confrontala con esempio, che si disegna tutto a
 * mano: qui non c'e' un pixel, non c'e' la barra, non c'e' il ciclo degli eventi. Si
 * dice COSA c'e' nella finestra e cosa fare quando premono i pulsanti; il resto (stile,
 * barra, trascinare, ridisegnare) lo fa il toolkit. */
#include "toolkit.h"

static struct finestra_tk finestra;
static struct contenitore colonna, riga;
static struct etichetta numero;
static struct pulsante meno, piu;
static int conto;

/* Il numero in lettere: senza libreria C, anche questo va fatto da soli. */
static void mostra_conto(void) {
    char c[12];
    int i = 11, n = conto < 0 ? -conto : conto;
    c[i] = 0;
    do { c[--i] = '0' + n % 10; n /= 10; } while (n);
    if (conto < 0) c[--i] = '-';
    etichetta_testo(&numero, &c[i]);
}

static void al_clic(struct pulsante *p, void *dati) {
    (void)p;
    conto += (int)dati;   /* -1 per il meno, +1 per il piu' */
    mostra_conto();
}

int main(void) {
    colonna_prepara(&colonna);
    riga_prepara(&riga);
    etichetta_prepara(&numero, "0");
    pulsante_prepara(&meno, "-", al_clic, (void *)-1);
    pulsante_prepara(&piu, "+", al_clic, (void *)1);

    widget_aggiungi(&riga.base, &meno.base);
    widget_aggiungi(&riga.base, &piu.base);
    widget_aggiungi(&colonna.base, &numero.base);
    widget_aggiungi(&colonna.base, &riga.base);

    finestra_tk_prepara(&finestra, "contatore", &colonna.base);
    return toolkit_esegui(&finestra);
}
