/* UIKit, il nucleo: IL CICLO DEGLI EVENTI, cioe' tutto quello che parla con il compositore.
 * Si apre la finestra (e l'altra, la mini finestra), si ridisegna e si mostrano le zone
 * cambiate, si ricevono le notizie (puntatore, rotella, tasti, fuoco, tema) e le si consegna
 * ai widget, si aspetta fino alla prima chiamata "ogni tanto". Usa la finestra di GKit e
 * CoreKit (aspetta, chiudi).
 *
 * E' in un file a parte apposta: l'anteprima (strumenti/anteprima) compila tutti gli altri
 * .c di UIKit ma non questo, e al posto di uikit_esegui mette la sua (che disegna una volta su
 * un PNG e torna). Le dichiarazioni sono in uikit.h. */
#include "uikit/uikit.h"
#include "uikit/interno/nucleo.h"

/* Ridisegna le zone cambiate e le mostra. L'immagine libera e' quella di due fotogrammi fa:
 * oltre alle zone cambiate adesso si rifanno quelle che le mancano (finestra_arretrate).
 * Ogni zona si ridisegna da capo, con il pennello ritagliato li': il fondo della finestra
 * comincia pulendo (pulisci), quindi rifare una zona da' gli stessi pixel di un ridisegno
 * intero. Al compositore si dicono solo le zone cambiate adesso. */
static int uikit_ridisegna(struct finestra_tk *f) {
    if (!f->tutta_cambiata && !f->quante_cambiate) { f->da_ridisegnare = 0; return 1; }
    unsigned int *pixel = finestra_tela(&f->finestra);
    if (!pixel) return 0;
    struct tela t = { pixel, f->base.l, f->base.a };
    struct pennello p;
    struct zona_finestra zone[MAX_ZONE_MOSTRA];
    int n = finestra_arretrate(&f->finestra, zone);
    int tutta = f->tutta_cambiata || n < 0;
    for (int k = 0; !tutta && k < f->quante_cambiate; k++)
        zone_aggiungi(zone, &n, MAX_ZONE_MOSTRA, f->cambiate[k]);
    if (tutta) {
        pennello_prepara(&p, &t);
        uikit_disegna_tutto(f, &p);
    } else {
        for (int k = 0; k < n; k++) {
            pennello_prepara(&p, &t);
            pennello_ritaglia(&p, zone[k].x, zone[k].y, zone[k].l, zone[k].a);
            uikit_disegna_tutto(f, &p);
        }
    }
    int fatto = (f->tutta_cambiata ? finestra_mostra(&f->finestra)
                                   : finestra_mostra_zone(&f->finestra, f->cambiate, f->quante_cambiate)) >= 0;
    f->da_ridisegnare = f->tutta_cambiata = f->quante_cambiate = 0;
    return fatto;
}

/* Il widget sotto il puntatore, con l'avviso: prima i suoi pulsanti; se e' bloccante, fuori
 * dal riquadro non c'e' niente. */
static struct widget *uikit_widget_sotto(struct finestra_tk *f, int x, int y) {
    struct avviso *v = finestra_tk_avviso_aperto(f);
    if (v) {
        struct widget *w = widget_sotto(&v->base, x, y);
        if (w || v->bloccante) return w;
    }
    return widget_sotto(&f->base, x, y);
}

/* Il widget piu' interno sotto (x, y) che vuole la rotella, o 0. Come widget_sotto: un widget
 * messo sopra agli altri che la rotella non la vuole (l'avviso in basso di Files, grande
 * quanto il pannello ma disegnato solo in fondo) non la ferma, passa a quelli sotto. */
static struct widget *widget_rotella_sotto(struct widget *w, int x, int y) {
    if (w->disabilitato || w->nascosto || !widget_contiene(w, x, y)) return 0;
    for (int i = w->quanti_figli - 1; i >= 0; i--) {
        struct widget *trovato = widget_rotella_sotto(w->figli[i], x, y);
        if (trovato) return trovato;
    }
    return w->tipo->rotella ? w : 0;
}

/* La rotella: al widget piu' interno sotto il puntatore che la vuole. */
static void uikit_rotella(struct finestra_tk *f, int x, int y, int sx, int sy) {
    if (y < testata_altezza(f) || finestra_tk_bloccata(f)) return;
    struct widget *w = widget_rotella_sotto(&f->base, x, y);
    if (w) w->tipo->rotella(w, sx, sy);
}

/* Il puntatore, una notizia PUNTATORE: cosa fa e' spiegato in uikit.h ("il puntatore"). */
static void uikit_puntatore(struct finestra_tk *f, int x, int y, unsigned int pulsanti) {
    const unsigned int cattura = PULSANTE_SINISTRO | PULSANTE_DESTRO;
    int giu = pulsanti & cattura;
    int appena_premuto = giu && !(f->pulsanti_prima & cattura);
    int sinistro = pulsanti & PULSANTE_SINISTRO;
    f->pulsanti_prima = pulsanti;

    /* riduci e chiudi si accendono sotto il puntatore (non mentre si tiene premuto
     * altrove, su un widget) */
    int bloccata = finestra_tk_bloccata(f);   /* un avviso bloccante: niente comandi ne' trascinami */
    int comando = f->catturato || bloccata ? 0 : su_comando(f, 1, x, y) ? 2 : su_comando(f, 0, x, y) ? 1 : 0;
    if (comando != f->comando_sopra) { f->comando_sopra = comando; uikit_cambiata_testata(f); }

    if (appena_premuto) {
        if (comando == 2 && sinistro) { f->chiudi_premuto = 1; uikit_cambiata_testata(f); return; }
        if (comando == 1 && sinistro) { f->riduci_premuto = 1; uikit_cambiata_testata(f); return; }
        /* la barra del titolo, e lo spazio vuoto della barra degli strumenti: "trascinami" */
        if (!bloccata && (y < testata_altezza(f) || barra_strumenti_vuota(f, x, y))) {
            if (sinistro) finestra_trascinami(&f->finestra);
            return;
        }
        f->catturato = uikit_widget_sotto(f, x, y);
        uikit_fuoco(f, f->catturato && f->catturato->tipo->tasto ? f->catturato : 0);
    }
    if (f->chiudi_premuto) {
        if (!sinistro) {
            f->chiudi_premuto = 0;
            uikit_cambiata_testata(f);
            if (su_comando(f, 1, x, y)) f->da_chiudere = 1;
        }
        return;
    }
    if (f->riduci_premuto) {                /* scatta quando si lascia, se si e' ancora sopra */
        if (!sinistro) {
            f->riduci_premuto = 0;
            uikit_cambiata_testata(f);
            if (su_comando(f, 0, x, y)) finestra_riducimi(&f->finestra);
        }
        return;
    }
    if (f->catturato) {
        f->catturato->tipo->puntatore(f->catturato, x, y, pulsanti);
        uikit_sotto(f, widget_contiene(f->catturato, x, y) ? f->catturato : 0);
        if (giu) return;
        f->catturato = 0;
    }
    uikit_sotto(f, uikit_widget_sotto(f, x, y));
    if (!giu && f->sotto && f->sotto->tipo->movimento) f->sotto->tipo->movimento(f->sotto, x, y);
}

/* L'avviso aperto che sente la tastiera: quello bloccante, o quello che ha il fuoco (lui o
 * uno dei suoi pulsanti). 0 se nessuno. */
static struct avviso *uikit_avviso_tastiera(const struct finestra_tk *f) {
    struct avviso *v = finestra_tk_avviso_aperto(f);
    if (!v || v->bloccante) return v;
    for (struct widget *w = f->con_fuoco; w; w = w->genitore)
        if (w == &v->base) return v;
    return 0;
}

/* Un tasto fisico premuto o lasciato: i modificatori si contano; Esc su un avviso fa come
 * Annulla; un tasto premuto va agli ascoltatori, dall'ultimo al primo, finche' uno lo usa. */
static void uikit_tasto_fisico(struct finestra_tk *f, unsigned int codice, int premuto) {
    unsigned int mod = codice == TASTO_CTRL || codice == TASTO_CTRL_DX ? MOD_CTRL
                     : codice == TASTO_MAIUSC || codice == TASTO_MAIUSC_DX ? MOD_MAIUSC
                     : codice == TASTO_ALT ? MOD_ALT : 0;
    if (mod) {
        if (premuto) f->modificatori |= mod; else f->modificatori &= ~mod;
        return;
    }
    if (!premuto) return;
    struct avviso *v = codice == TASTO_ESC && !f->modificatori ? uikit_avviso_tastiera(f) : 0;
    if (v) { avviso_rispondi(v, AVVISO_ANNULLA); return; }
    if (finestra_tk_bloccata(f)) return;
    for (int i = MAX_ASCOLTATORI - 1; i >= 0; i--) {
        struct ascoltatore_tk a = f->ascoltatori[i];
        if (a.funzione && a.funzione(f, codice, f->modificatori, a.dati)) return;
    }
}

int finestra_tk_apri_altra(struct finestra_tk *f, struct finestra_tk *altra) {
    if (f->altra) finestra_tk_chiudi_altra(f);
    int l, a;
    widget_misura(&altra->base, &l, &a);
    altra->base.x = altra->base.y = 0;
    altra->base.l = l;
    altra->base.a = a;
    widget_disponi(&altra->base);
    altra->da_chiudere = 0;
    altra->attiva = 1;
    altra->quante_cambiate = 0;
    uikit_cambiata_tutta(altra);
    if (finestra_apri_come(&altra->finestra, l, a, altra->titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_FINESTRA_NUOVO) < 0)
        return -1;
    finestra_tasti_fisici(&altra->finestra);
    altra->aperta = 1;
    f->altra = altra;
    return uikit_ridisegna(altra) ? 0 : -1;
}

void finestra_tk_chiudi_altra(struct finestra_tk *f) {
    struct finestra_tk *a = f->altra;
    if (!a) return;
    f->altra = 0;
    a->aperta = 0;
    chiudi(a->finestra.collegamento);
    if (a->finestra.immagini[0]) togli(a->finestra.immagini[0]);
    chiudi(a->finestra.foglio);
    a->catturato = a->sotto = a->con_fuoco = 0;
    a->modificatori = a->pulsanti_prima = 0;
    a->chiudi_premuto = a->riduci_premuto = a->comando_sopra = 0;
}

/* Una notizia del compositore per la finestra 'f' (la principale o l'altra). */
static void uikit_consegna(struct finestra_tk *f, const struct notizia *n) {
    if (n->tipo == PUNTATORE) {
        uikit_puntatore(f, n->x, n->y, n->pulsanti);
    } else if (n->tipo == USCITO) {
        /* il puntatore non e' piu' nostro (per esempio, il compositore ha cominciato
         * a spostare la finestra): una pressione in corso si annulla, senza scattare */
        if (f->catturato) f->catturato->tipo->puntatore(f->catturato, -1, -1, 0);
        f->catturato = 0;
        f->chiudi_premuto = 0;
        f->riduci_premuto = 0;
        f->pulsanti_prima = 0;
        if (f->comando_sopra) { f->comando_sopra = 0; uikit_cambiata_testata(f); }
        uikit_sotto(f, 0);
    } else if (n->tipo == ROTELLA) {
        uikit_rotella(f, n->x, n->y, n->scatti_x, n->scatti_y);
    } else if (n->tipo == TASTO_FISICO) {
        uikit_tasto_fisico(f, n->tasto, n->premuto);
    } else if (n->tipo == TASTO && (f->modificatori & (MOD_CTRL | MOD_ALT))) {
        /* Ctrl+D, Alt+...: e' una scorciatoia, non un carattere da scrivere */
    } else if (n->tipo == TASTO && finestra_tk_bloccata(f)) {
        avviso_tasto(&f->avviso->base, n->tasto);   /* sotto un avviso bloccante: solo lui */
    } else if (n->tipo == TASTO && f->con_fuoco) {
        f->con_fuoco->tipo->tasto(f->con_fuoco, n->tasto);
    } else if (n->tipo == FUOCO) {
        f->attiva = n->fuoco;
        if (!n->fuoco) f->modificatori = 0;    /* i tasti lasciati altrove non li sentiremo */
        uikit_cambiata_testata(f);             /* il fuoco cambia solo il titolo e i comandi */
    } else if (n->tipo == TEMA_CAMBIATO) {
        /* il tema e' cambiato (gkit/sistema/protocollo.h, IL TEMA): lo si rilegge e si ridisegna tutto. I colori
         * i widget li chiedono quando disegnano (tema.h), quindi basta questo */
        tema_leggi();
        uikit_cambiata_tutta(f);
    }
}

int uikit_cambia_tema(struct finestra_tk *f, const char *nome) {
    if (tema_salva(nome) < 0) return -1;
    return finestra_cambia_tema(&f->finestra) < 0 ? -1 : 0;
}

/* Consegna una notizia, ma i movimenti del puntatore li tiene da parte (in 'sospesa'): se
 * subito dopo ne arriva un altro con gli stessi pulsanti, quello prima non serve piu' (le
 * posizioni di mezzo non le vedrebbe nessuno). Le altre notizie passano dopo il movimento
 * tenuto da parte, nel loro ordine. */
struct sospesa_tk {
    struct finestra_tk *da;            /* 0: nessun movimento da parte */
    struct notizia n;
};

static void uikit_consegna_sospesa(struct sospesa_tk *s) {
    if (!s->da) return;
    struct finestra_tk *da = s->da;
    s->da = 0;
    uikit_consegna(da, &s->n);
}

static void uikit_consegna_unendo(struct sospesa_tk *s, struct finestra_tk *da, const struct notizia *n) {
    if (s->da && (n->tipo != PUNTATORE || s->da != da || s->n.pulsanti != n->pulsanti))
        uikit_consegna_sospesa(s);
    if (n->tipo == PUNTATORE) {
        s->da = da;
        s->n = *n;
    } else {
        uikit_consegna(da, n);
    }
}

int uikit_esegui(struct finestra_tk *f) {
    int l, a;
    widget_misura(&f->base, &l, &a);
    f->base.x = f->base.y = 0;
    f->base.l = l;
    f->base.a = a;
    widget_disponi(&f->base);
    if (finestra_tk_avviso_aperto(f)) avviso_sistema(f->avviso, l, a);   /* aperto prima di avere una misura */
    if (finestra_apri_come(&f->finestra, l, a, f->titolo, RUOLO_FINESTRA, 0, 0, RAGGIO_FINESTRA_NUOVO) < 0) return 1;
    finestra_tasti_fisici(&f->finestra);        /* per le scorciatoie (tasti.h) */
    f->aperta = 1;
    f->quante_cambiate = 0;
    uikit_cambiata_tutta(f);

    struct notizia n;
    for (;;) {
        if (f->da_ridisegnare && finestra_libera(&f->finestra) && !uikit_ridisegna(f)) return 1;
        if (f->altra && f->altra->da_ridisegnare && finestra_libera(&f->altra->finestra) &&
            !uikit_ridisegna(f->altra))
            finestra_tk_chiudi_altra(f);

        struct sospesa_tk sospesa = { 0 };
        for (int servite = 0; servite < 64; servite++) {
            /* prima le notizie messe da parte da finestra_tela, poi il compositore */
            struct finestra_tk *da = f->finestra.quante ? f : f->altra && f->altra->finestra.quante ? f->altra : 0;
            if (da) {
                finestra_notizia(&da->finestra, &n);
            } else {
                int resta = 0;                    /* gia' servito qualcosa: non si aspetta piu' */
                if (!servite) {
                    struct chiamata_tk *prima = uikit_prima_chiamata(f);
                    resta = prima ? (int)(prima->scadenza - tempo()) : -1;
                    if (prima && resta < 0) resta = 0;
                }
                /* la finestra, l'altra (se c'e') e il descrittore guardato (se c'e') */
                struct da_guardare g[3] = { { .descrittore = f->finestra.collegamento } };
                int quanti = 1, altra = 0, guardato = 0;
                if (f->altra) { altra = quanti; g[quanti++].descrittore = f->altra->finestra.collegamento; }
                if (f->al_pronto) { guardato = quanti; g[quanti++].descrittore = f->guardato; }
                int pronti = aspetta(g, quanti, resta);
                if (pronti == 0) break;           /* niente (o e' scaduta una chiamata) */
                if (pronti < 0) {                 /* interrotta: si riprova */
                    if (servite) break;
                    servite--;
                    continue;
                }
                if (guardato && g[guardato].pronto) {
                    f->al_pronto(f, f->dati_pronto);
                    if (f->da_chiudere) break;
                    if (!g[0].pronto && !(altra && g[altra].pronto)) continue;
                }
                da = g[0].pronto || !altra ? f : f->altra;
                if (!finestra_ricevi(&da->finestra, &n)) {
                    if (da == f) return 1;
                    if (sospesa.da == da) sospesa.da = 0;
                    finestra_tk_chiudi_altra(f);       /* il compositore ha chiuso l'altra */
                    continue;
                }
                if (finestra_di_servizio(&n)) continue;   /* un'immagine tornata, un "ci sei?" */
            }
            uikit_consegna_unendo(&sospesa, da, &n);
            if (f->da_chiudere || (f->altra && f->altra->da_chiudere)) break;
        }
        uikit_consegna_sospesa(&sospesa);
        uikit_chiamate_scadute(f);
        if (f->altra && f->altra->da_chiudere) finestra_tk_chiudi_altra(f);
        if (f->da_chiudere) return 0;
    }
}
