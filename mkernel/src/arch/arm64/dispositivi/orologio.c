/* L'orologio su ARM: il PL031 ("Real Time Clock"), sulla macchina "virt" di QEMU. Dove
 * sta lo dice il device tree ("arm,pl031"). Ha un solo registro che ci serve:
 *   +0x00 DR: i secondi passati dal 1 gennaio 1970 (il "tempo Unix")
 * Da li' si ricavano data e ora, contando gli anni e i mesi. QEMU lo mette all'ora del
 * Mac se gli si dice -rtc base=localtime. */
#include "kernel.h"
#include "chiamate.h"
#include "orologio.h"
#include "albero.h"

static int bisestile(unsigned int a) { return (a % 4 == 0 && a % 100 != 0) || a % 400 == 0; }

void orologio_leggi(struct data_ora *d) {
    unsigned long dove = 0x09010000UL, quanto;
    albero_cerca("arm,pl031", &dove, &quanto);
    unsigned long secondi = *(volatile unsigned int *)dove;

    unsigned long giorni = secondi / 86400, resto = secondi % 86400;
    d->ore = (unsigned int)(resto / 3600);
    d->minuti = (unsigned int)(resto % 3600 / 60);
    d->secondi = (unsigned int)(resto % 60);
    d->giorno_settimana = (unsigned int)((giorni + 4) % 7);   /* il 1/1/1970 era giovedi' */

    unsigned int anno = 1970;
    while (giorni >= (bisestile(anno) ? 366u : 365u)) giorni -= bisestile(anno) ? 366 : 365, anno++;
    static const unsigned char lunghi[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    unsigned int mese = 0;
    while (mese < 12) {
        unsigned int g = lunghi[mese] + (mese == 1 && bisestile(anno));
        if (giorni < g) break;
        giorni -= g;
        mese++;
    }
    d->anno = anno;
    d->mese = mese + 1;
    d->giorno = (unsigned int)giorni + 1;
}
