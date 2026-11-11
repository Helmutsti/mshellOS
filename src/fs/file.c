/* I file e le cartelle aperti dai programmi.
 *
 * Come in Unix, per un programma un file aperto e' un DESCRITTORE come gli altri: si
 * legge con ricevi e si scrive con manda, le stesse chiamate dei socket (in Unix read e
 * write valgono per file, tubi e dispositivi). La differenza e' dentro: un socket ha una
 * casella di messaggi, un file ha una POSIZIONE, il byte da cui si legge o si scrive
 * la prossima volta, che va avanti da sola (a 64 bit: un file puo' superare i 4 GB).
 *
 * Una CARTELLA aperta si legge una voce alla volta: ogni ricevi da' la prossima
 * (struct voce_cartella), 0 quando sono finite. In Linux e' getdents.
 *
 * I PERMESSI si controllano qui, una volta, quando si apre (come in Unix): chi ha
 * aperto un file per leggerlo puo' poi leggerlo finche' lo tiene aperto. Le regole
 * sono quelle di vfs_puo (fs/vfs_operazioni.h).
 *
 * Il file aperto tiene in mano il suo NODO del file system virtuale (quale formato
 * montato, quale nodo dentro di lui: l'inode di Unix), non il percorso: ogni lettura va
 * dritta al formato, senza ripercorrere le cartelle. */
#include "kernel.h"
#include "kmalloc.h"
#include "descrittori.h"
#include "vfs_operazioni.h"
#include "file.h"
#include "compiti.h"

struct file_aperto {
    struct aperto aperto;            /* per primo: un file aperto E' una cosa aperta */
    struct nodo_vfs nodo;
    unsigned int modo;               /* APRI_... */
    int cartella;
    unsigned long long posizione;    /* il prossimo byte, o il numero della prossima voce */
    unsigned long long cursore;      /* cartella: dove il formato e' arrivato tra le voci */
};

/* La prossima voce di una cartella aperta, come la vede il programma. 1, 0 se le voci
 * sono finite, o un errore. */
static int prossima_voce(struct file_aperto *f, struct voce_cartella *vc) {
    struct voce_vfs v;
    struct nodo_vfs sotto;
    struct info_vfs i;
    int r = vfs_voce(&f->nodo, &f->cursore, &v, &sotto);
    if (r <= 0) return r;
    unsigned int l = v.lunghezza > VFS_NOME_MAX ? VFS_NOME_MAX : v.lunghezza;
    memset(vc, 0, sizeof *vc);
    memcpy(vc->nome, v.nome, l);                      /* VFS_NOME_MAX + 1 = MAX_NOME_FILE */
    vc->tipo = v.tipo;
    /* tipo (se il formato non lo sapeva), dimensione e data stanno nel nodo */
    if (vfs_info(&sotto, &i) == 0) {
        vc->tipo = i.tipo;
        vc->permessi = i.permessi;
        vc->dimensione = i.dimensione;
        vc->modificato = i.modificato;
    }
    f->posizione++;
    return 1;
}

static int ricevi(struct aperto *a, void *dove, unsigned int byte, struct aperto **allegato) {
    struct file_aperto *f = (struct file_aperto *)a;
    *allegato = 0;
    if (f->cartella) {
        if (byte < sizeof(struct voce_cartella)) return -1;   /* una voce non si spezza */
        struct voce_cartella v;
        int r = prossima_voce(f, &v);
        if (r <= 0) return r;                                 /* 0: le voci sono finite */
        memcpy(dove, &v, sizeof v);
        return sizeof v;
    }
    if (!(f->modo & APRI_LEGGERE)) return -1;
    long long n = vfs_leggi(&f->nodo, f->posizione, dove, byte);
    if (n < 0) return (int)n;
    f->posizione += n;
    return (int)n;                                            /* 0: alla fine del file */
}

static int manda(struct aperto *a, const void *da, unsigned int byte, struct aperto *allegato,
                 int subito) {
    struct file_aperto *f = (struct file_aperto *)a;
    (void)subito;
    if (allegato || f->cartella || !(f->modo & APRI_SCRIVERE)) return -1;
    long long n = vfs_scrivi(&f->nodo, f->posizione, da, byte);
    if (n < 0) return (int)n;
    f->posizione += n;
    return (int)n;
}

/* Un file ha sempre qualcosa da dire (anche solo "sono finito"): non si aspetta mai. */
static int pronto(struct aperto *a) { (void)a; return 1; }

static void chiudi(struct aperto *a) { kfree(a); }

static const struct operazioni operazioni = { ricevi, manda, pronto, chiudi };

int file_puo(const char *percorso, unsigned int cosa) {
    struct nodo_vfs n;
    struct info_vfs i;
    if (vfs_cerca(percorso, 1, &n) < 0 || vfs_info(&n, &i) < 0) return 0;
    return vfs_puo(&i, compito_attuale()->uid, cosa);
}

int file_apri(const char *percorso, unsigned int modo) {
    unsigned int uid = compito_attuale()->uid;
    struct nodo_vfs n;
    struct info_vfs i;
    int r = vfs_cerca(percorso, 1, &n);
    /* non c'e': lo si crea vuoto, se e' stato chiesto (il file system virtuale controlla
     * che si possa scrivere nella cartella). E' di chi lo crea: lui legge e scrive, gli
     * altri solo leggono */
    if (r == VFS_ERR_NON_TROVATO && (modo & APRI_CREARE))
        r = vfs_crea(percorso, VFS_FILE, 0644, uid, &n);
    if (r < 0 || (r = vfs_info(&n, &i)) < 0) return r;
    if (i.tipo != VFS_FILE && i.tipo != VFS_CARTELLA) return VFS_ERR_NON_SUPPORTATO;

    int cartella = i.tipo == VFS_CARTELLA;
    int scrive = (modo & (APRI_SCRIVERE | APRI_SVUOTARE)) != 0;
    if ((modo & APRI_SVUOTARE) && !(modo & APRI_SCRIVERE)) return -1;   /* richiesta sbagliata */
    if (cartella && scrive) return VFS_ERR_E_CARTELLA;      /* le cartelle si leggono */
    /* per scrivere: prima il disco (un disco in sola lettura non lo scrive nessuno),
     * poi i permessi, per leggere (anche una cartella) e per scrivere o svuotare */
    if (scrive && (r = vfs_si_scrive(&n)) < 0) return r;
    if ((modo & APRI_LEGGERE || cartella) && !vfs_puo(&i, uid, PERMESSO_LEGGERE))
        return VFS_ERR_PERMESSO;
    if (scrive && !vfs_puo(&i, uid, PERMESSO_SCRIVERE)) return VFS_ERR_PERMESSO;
    if ((modo & APRI_SVUOTARE) && (r = vfs_tronca(&n, 0)) < 0) return r;

    struct file_aperto *f = kmalloc(sizeof *f);
    if (!f) return VFS_ERR_MEMORIA;
    memset(f, 0, sizeof *f);
    f->aperto.op = &operazioni;
    f->nodo = n;
    f->modo = modo;
    f->cartella = cartella;
    int d = descrittore_nuovo(&f->aperto);
    if (d < 0) kfree(f);                                  /* la tabella e' piena: -1 */
    return d;
}

long long file_posiziona(int d, unsigned long long posizione) {
    struct aperto *a = descrittore(d);
    if (!a || a->op != &operazioni) return -1;
    struct file_aperto *f = (struct file_aperto *)a;
    if (f->cartella) {
        /* la voce numero 'posizione': il formato non sa contare le voci, sa solo dare la
         * prossima. Si ricomincia da capo e se ne saltano tante (fino alla fine, al massimo) */
        struct voce_cartella v;
        f->cursore = 0;
        f->posizione = 0;
        while (f->posizione < posizione) {
            int r = prossima_voce(f, &v);
            if (r < 0) return r;
            if (r == 0) break;
        }
        return f->posizione;
    }
    struct info_vfs i;
    int r = vfs_info(&f->nodo, &i);
    if (r < 0) return r;
    if (posizione > i.dimensione) return -1;              /* non oltre la fine */
    f->posizione = posizione;
    return posizione;
}
