/* I file e le cartelle aperti dai programmi.
 *
 * Come in Unix, per un programma un file aperto e' un DESCRITTORE come gli altri: si
 * legge con ricevi e si scrive con manda, le stesse chiamate dei socket (in Unix read e
 * write valgono per file, tubi e dispositivi). La differenza e' dentro: un socket ha una
 * casella di messaggi, un file ha una POSIZIONE, il byte da cui si legge o si scrive
 * la prossima volta, che va avanti da sola.
 *
 * Una CARTELLA aperta si legge una voce alla volta: ogni ricevi da' la prossima
 * (struct voce_cartella), 0 quando sono finite. In Linux e' getdents.
 *
 * Il file aperto si ricorda il suo percorso, e ogni lettura o scrittura lo ritrova su
 * mfs. (Un kernel vero tiene in mano la scheda del file, l'inode, per non ricercarlo
 * ogni volta.) */
#include "kernel.h"
#include "kmalloc.h"
#include "descrittori.h"
#include "mfs.h"
#include "file.h"

struct file_aperto {
    struct aperto aperto;            /* per primo: un file aperto E' una cosa aperta */
    char percorso[MAX_PERCORSO];
    unsigned int modo;               /* APRI_... */
    int cartella;
    unsigned int posizione;          /* il prossimo byte, o la prossima voce della cartella */
};

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    struct file_aperto *f = (struct file_aperto *)a;
    *allegato = 0;
    if (f->cartella) {
        if (byte < sizeof(struct voce_cartella)) return -1;   /* una voce non si spezza */
        struct voce_cartella v;
        int tipo;
        memset(&v, 0, sizeof v);
        int r = mfs_voce(f->percorso, f->posizione, v.nome, &tipo, &v.dimensione);
        if (r <= 0) return r < 0 ? -1 : 0;                    /* 0: le voci sono finite */
        v.tipo = tipo;
        f->posizione++;
        memcpy(dove, &v, sizeof v);
        return sizeof v;
    }
    if (!(f->modo & APRI_LEGGERE)) return -1;
    int n = mfs_leggi(f->percorso, f->posizione, dove, byte);
    if (n < 0) return -1;
    f->posizione += n;
    return n;                                                 /* 0: alla fine del file */
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                 int subito) {
    struct file_aperto *f = (struct file_aperto *)a;
    (void)subito;
    if (allegato || f->cartella || !(f->modo & APRI_SCRIVERE)) return -1;
    int n = mfs_scrivi(f->percorso, f->posizione, da, byte);
    if (n < 0) return -1;
    f->posizione += n;
    return n;
}

/* Un file ha sempre qualcosa da dire (anche solo "sono finito"): non si aspetta mai. */
static int pronto(struct aperto *a) { (void)a; return 1; }

static void chiudi(struct aperto *a) { kfree(a); }

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

int file_apri(const char *percorso, unsigned int modo) {
    int tipo;
    unsigned int dimensione;
    if (mfs_info(percorso, &tipo, &dimensione) < 0) {
        /* non c'e': lo si crea vuoto, se e' stato chiesto */
        if (!(modo & APRI_CREARE) || mfs_crea(percorso, 0, 0) < 0) return -1;
        tipo = MFS_FILE;
    }
    int cartella = tipo == MFS_CARTELLA;
    if (cartella && (modo & (APRI_SCRIVERE | APRI_SVUOTARE))) return -1;   /* le cartelle si leggono */
    if (!cartella && (modo & APRI_SVUOTARE)) {
        if (!(modo & APRI_SCRIVERE) || mfs_svuota(percorso) < 0) return -1;
    }

    struct file_aperto *f = kmalloc(sizeof *f);
    if (!f) return -1;
    memset(f, 0, sizeof *f);
    f->aperto.op = &operazioni;
    memcpy(f->percorso, percorso, MAX_PERCORSO);
    f->modo = modo;
    f->cartella = cartella;
    int d = descrittore_nuovo(&f->aperto);
    if (d < 0) kfree(f);
    return d;
}

int file_posiziona(int d, unsigned int posizione) {
    struct aperto *a = descrittore(d);
    if (!a || a->op != &operazioni) return -1;
    struct file_aperto *f = (struct file_aperto *)a;
    int tipo;
    unsigned int dimensione;
    if (f->cartella) { f->posizione = posizione; return posizione; }   /* la voce numero ... */
    if (mfs_info(f->percorso, &tipo, &dimensione) < 0 || posizione > dimensione) return -1;
    f->posizione = posizione;
    return posizione;
}
