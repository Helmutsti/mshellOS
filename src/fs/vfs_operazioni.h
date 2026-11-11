/* Il file system virtuale visto dal resto del kernel (i file aperti, le chiamate di
 * sistema, il caricamento dei programmi, l'avvio).
 *
 * vfs.h e' il contratto con i FORMATI (ext4...); qui ci sono le operazioni che il
 * file system virtuale offre sopra di loro, su un unico albero di cartelle:
 *   - i MONTAGGI: "/" e' un formato su un disco; domani altre cartelle potranno essere
 *     altri formati su altri dischi (come una chiavetta in /Volumi/...);
 *   - i PERCORSI: da "/Sistema/../Sistema/init" al nodo giusto del formato giusto,
 *     seguendo i collegamenti simbolici;
 *   - i PERMESSI: chi puo' leggere, scrivere, eseguire (le regole sono in vfs_puo);
 *   - le operazioni, che passano al formato. Quelle di scrittura rispondono
 *     VFS_ERR_SOLA_LETTURA se il montaggio e' in sola lettura o il formato non le sa fare.
 *
 * Tutte restituiscono 0 (o un numero di byte) se va bene, o un VFS_ERR_... (negativo).
 * Ognuna prende il lucchetto del file system virtuale: un compito alla volta. */
#ifndef VFS_OPERAZIONI_H
#define VFS_OPERAZIONI_H

#include "vfs.h"

/* Un nodo dell'albero: quale formato montato, e quale nodo dentro di lui. */
struct nodo_vfs {
    struct montato *m;
    unsigned long long nodo;
};

#define VFS_PERCORSO_MAX 4096   /* byte di un percorso, anche dopo aver seguito i collegamenti */
#define VFS_MAX_LINK 8          /* collegamenti simbolici di fila, poi VFS_ERR_TROPPI_LINK */

/* Monta il formato 'tipo' del disco 'd' sulla cartella 'dove' ("/" la prima volta). */
int vfs_monta(const char *dove, const struct tipo_fs *tipo, struct dispositivo_blocchi *d,
              int sola_lettura);

/* Il nodo del percorso 'percorso' (assoluto: comincia con /). Se l'ultimo pezzo e' un
 * collegamento simbolico, con 'segui' = 1 si va dove punta, con 0 si da' il
 * collegamento stesso. I collegamenti nel mezzo si seguono sempre. */
int vfs_cerca(const char *percorso, int segui, struct nodo_vfs *n);

int vfs_info(const struct nodo_vfs *n, struct info_vfs *i);

/* Chi ha l'uid 'uid' puo' fare 'cosa' (PERMESSO_..., anche sommati) sul nodo con queste
 * informazioni? 1 si', 0 no. Le regole sono quelle di Unix, semplificate:
 *   - il sistema (uid 0) puo' tutto;
 *   - il proprietario guarda la prima cifra dei permessi, tutti gli altri l'ultima
 *     (quella di mezzo, il gruppo, per ora non si usa).
 * (Attraversare una cartella non chiede ancora il permesso di eseguirla.) */
int vfs_puo(const struct info_vfs *i, unsigned int uid, unsigned int cosa);

/* Legge fino a 'byte' dal byte 'da' del file: i byte letti (meno solo alla fine). */
long long vfs_leggi(const struct nodo_vfs *n, unsigned long long da, void *dove,
                    unsigned int byte);

/* La prossima voce della cartella (vedi tipo_fs.voce): 1, 0 alla fine, o un errore.
 * In '*sotto' (se non e' 0) il nodo della voce, da passare a vfs_info: se sulla voce e'
 * montato un altro formato, e' gia' la radice di quello. */
int vfs_voce(const struct nodo_vfs *cartella, unsigned long long *cursore,
             struct voce_vfs *v, struct nodo_vfs *sotto);

/* --- la scrittura ---
 * Su un nodo (un file gia' aperto) non si controllano i permessi: si sono controllati
 * quando lo si e' aperto, come in Unix. Le operazioni con un percorso li controllano,
 * per chi ha l'uid 'uid'. */

/* 0 se il nodo si puo' scrivere (il montaggio non e' in sola lettura e il formato sa
 * scrivere), se no VFS_ERR_SOLA_LETTURA. Per chi apre un file per scriverci. */
int vfs_si_scrive(const struct nodo_vfs *n);

long long vfs_scrivi(const struct nodo_vfs *n, unsigned long long da, const void *dati,
                     unsigned int byte);
int vfs_tronca(const struct nodo_vfs *n, unsigned long long dimensione);

/* Crea un file vuoto o una cartella (tipo VFS_FILE o VFS_CARTELLA), di 'uid', con
 * 'permessi'. Serve poter scrivere nella cartella che lo conterra'. */
int vfs_crea(const char *percorso, unsigned int tipo, unsigned int permessi, unsigned int uid,
             struct nodo_vfs *creato);
/* Cancella un file o una cartella vuota (si deve poter scrivere nella cartella). */
int vfs_cancella(const char *percorso, unsigned int uid);
/* Rinomina o sposta, dentro lo stesso montaggio (si deve poter scrivere in tutte e due
 * le cartelle). */
int vfs_rinomina(const char *da, const char *a, unsigned int uid);
/* Cambia proprietario e permessi: il sistema puo' tutto; il proprietario solo i
 * permessi, e il file resta suo. */
int vfs_cambia(const char *percorso, unsigned int proprietario, unsigned int permessi,
               unsigned int uid);

/* Il messaggio per un codice di errore. */
const char *vfs_messaggio(int errore);

#endif
