/* L'orologio: data e ora. Su x86 e' l'orologio CMOS (arch/x86_64/dispositivi/orologio.c). */
#ifndef OROLOGIO_H
#define OROLOGIO_H

struct data_ora;

void orologio_leggi(struct data_ora *d);

#endif
