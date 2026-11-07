/* Le strutture che il bootloader (Multiboot 1) ci passa all'avvio. */
#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define MULTIBOOT_MAGIC_BOOT 0x2BADB002   /* in EAX se ci ha caricato un bootloader Multiboot */

#define MB_FLAG_MEM  (1 << 0)   /* mem_lower / mem_upper validi */
#define MB_FLAG_MMAP (1 << 6)   /* mmap_* validi */

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;     /* KB sotto 1 MB */
    unsigned int mem_upper;     /* KB sopra 1 MB */
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length;   /* byte della mappa */
    unsigned int mmap_addr;     /* dove sta la mappa */
} __attribute__((packed));

/* Una regione della mappa. 'size' NON conta se stesso: la voce dopo e' a size+4 byte. */
struct multiboot_mmap {
    unsigned int  size;
    unsigned long long base;
    unsigned long long len;
    unsigned int  type;         /* 1 = libera, altro = riservata */
} __attribute__((packed));

#define MB_MMAP_LIBERA 1

#endif
