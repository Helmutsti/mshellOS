/* EXT4 IN SOLA LETTURA: il formato dei dischi di Linux, visto dal file system virtuale.
 *
 * Si monta con tipo_ext4 (vfs.h): i nodi sono i numeri degli inode, la radice e' il
 * nodo 2. Le operazioni di scrittura sono 0: il file system virtuale risponde
 * VFS_ERR_SOLA_LETTURA.
 *
 * Il codice e' in quattro file (tutti in src/fs/ext4/):
 *   ext4.c          il montaggio (superblocco, descrittori dei gruppi, funzioni del
 *                   disco), gli inode e le operazioni di tipo_ext4;
 *   ext4_dati.c     dove sta il contenuto di un file: alberi di extent, mappa dei
 *                   blocchi all'antica, dati "in linea" dentro l'inode;
 *   ext4_cartelle.c le voci delle cartelle;
 *   ext4_crc.c      i checksum (crc32c e crc16).
 * ext4_interno.h e' quello che si dicono tra loro. */
#ifndef EXT4_H
#define EXT4_H

#include "vfs.h"

extern const struct tipo_fs tipo_ext4;

#endif
