/* L'orologio del PC (RTC, "real time clock").
 *
 * Il PC ha un orologio a batteria che va avanti anche da spento: sta nel chip CMOS,
 * insieme a qualche byte di impostazioni. Si legge con due porte: sulla 0x70 si scrive
 * QUALE registro si vuole, dalla 0x71 se ne legge il valore. I registri: 0 i secondi,
 * 2 i minuti, 4 le ore, 7 il giorno, 8 il mese, 9 l'anno (le ultime due cifre).
 *
 * DUE STRANEZZE, da ricordare:
 *   - l'orologio aggiorna i registri una volta al secondo, e se lo si legge proprio in
 *     quel momento si possono prendere meta' valori vecchi e meta' nuovi. Si aspetta che
 *     non stia aggiornando (bit 7 del registro 0x0A), e si legge due volte finche' le due
 *     letture sono uguali;
 *   - di solito i numeri sono in BCD: 0x59 vuol dire 59, una cifra decimale per ogni
 *     mezzo byte. Lo dice il bit 2 del registro 0x0B (acceso: numeri normali).
 * Linux fa lo stesso all'avvio, per sapere che ore sono. */
#include "kernel.h"
#include "io.h"
#include "chiamate.h"
#include "orologio.h"

static unsigned char registro(unsigned char n) {
    outb(0x70, n);
    return inb(0x71);
}

static void leggi_una_volta(unsigned char v[6]) {
    while (registro(0x0A) & 0x80) ;   /* sta aggiornando: si aspetta */
    v[0] = registro(0x00); v[1] = registro(0x02); v[2] = registro(0x04);
    v[3] = registro(0x07); v[4] = registro(0x08); v[5] = registro(0x09);
}

static unsigned int da_bcd(unsigned char x) { return (x >> 4) * 10 + (x & 0x0F); }

void orologio_leggi(struct data_ora *d) {
    unsigned char a[6], b[6];
    leggi_una_volta(a);
    for (;;) {                         /* finche' due letture di fila sono uguali */
        leggi_una_volta(b);
        int uguali = 1;
        for (int i = 0; i < 6; i++) if (a[i] != b[i]) uguali = 0;
        if (uguali) break;
        for (int i = 0; i < 6; i++) a[i] = b[i];
    }
    unsigned char formato = registro(0x0B);
    int bcd = !(formato & 0x04), dodici_ore = !(formato & 0x02);
    unsigned int pomeriggio = a[2] & 0x80;   /* nel formato a 12 ore, il bit 7 delle ore */
    a[2] &= 0x7F;
    unsigned int v[6];
    for (int i = 0; i < 6; i++) v[i] = bcd ? da_bcd(a[i]) : a[i];
    if (dodici_ore) v[2] = (v[2] % 12) + (pomeriggio ? 12 : 0);

    d->secondi = v[0];
    d->minuti = v[1];
    d->ore = v[2];
    d->giorno = v[3];
    d->mese = v[4];
    d->anno = 2000 + v[5];
    /* il giorno della settimana, con la formula di Zeller (0 = domenica) */
    int m = d->mese, y = d->anno;
    if (m < 3) { m += 12; y--; }
    int h = (d->giorno + 13 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;   /* 0 = sabato */
    d->giorno_settimana = (h + 6) % 7;
}
