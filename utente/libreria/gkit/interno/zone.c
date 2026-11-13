/* GKit: le zone cambiate, cioe' i rettangoli di una finestra (le dichiarazioni, con cosa fa
 * ogni funzione, sono in gkit.h). Solo conti: niente compositore e niente CoreKit, cosi' si
 * usano anche fuori da mshell (l'anteprima sul Mac, dove UIKit disegna senza finestra). */
#include "gkit/gkit.h"

int zona_vuota(struct zona_finestra z) { return z.l <= 0 || z.a <= 0; }

struct zona_finestra zona_unione(struct zona_finestra p, struct zona_finestra q) {
    int x0 = p.x < q.x ? p.x : q.x, y0 = p.y < q.y ? p.y : q.y;
    int x1 = p.x + p.l > q.x + q.l ? p.x + p.l : q.x + q.l;
    int y1 = p.y + p.a > q.y + q.a ? p.y + p.a : q.y + q.a;
    return (struct zona_finestra){ x0, y0, x1 - x0, y1 - y0 };
}

struct zona_finestra zona_dentro(struct zona_finestra p, struct zona_finestra q) {
    int x0 = p.x > q.x ? p.x : q.x, y0 = p.y > q.y ? p.y : q.y;
    int x1 = p.x + p.l < q.x + q.l ? p.x + p.l : q.x + q.l;
    int y1 = p.y + p.a < q.y + q.a ? p.y + p.a : q.y + q.a;
    if (x1 <= x0 || y1 <= y0) return (struct zona_finestra){ 0, 0, 0, 0 };
    return (struct zona_finestra){ x0, y0, x1 - x0, y1 - y0 };
}

void zone_aggiungi(struct zona_finestra *zone, int *quante, int massimo, struct zona_finestra z) {
    if (zona_vuota(z)) return;
    for (int k = 0; k < *quante; k++) {
        struct zona_finestra u = zona_unione(z, zone[k]);
        long long piu = (long long)u.l * u.a - (long long)z.l * z.a - (long long)zone[k].l * zone[k].a;
        int tocca = !zona_vuota(zona_dentro(z, zone[k]));
        if (tocca || piu * 4 <= (long long)z.l * z.a + (long long)zone[k].l * zone[k].a) {
            z = u;
            zone[k] = zone[--*quante];
            k = -1;                    /* la nuova, piu' grande, puo' toccarne altre */
        }
    }
    if (*quante == massimo) {
        for (int k = 0; k < *quante; k++) z = zona_unione(z, zone[k]);
        *quante = 0;
    }
    zone[(*quante)++] = z;
}
