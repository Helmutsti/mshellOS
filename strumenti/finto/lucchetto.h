/* Al posto di src/compiti/lucchetto.h, per compilare src/fs/mfs.c sul computer che
 * compila (strumenti/crea_disco.c): li' c'e' un solo programma che usa il disco,
 * quindi il lucchetto non serve e non fa niente. */
#ifndef LUCCHETTO_H
#define LUCCHETTO_H

struct lucchetto { const char *nome; };
#define LUCCHETTO(n) { (n) }

static inline void lucchetto_prendi(struct lucchetto *l) { (void)l; }
static inline void lucchetto_lascia(struct lucchetto *l) { (void)l; }

#endif
