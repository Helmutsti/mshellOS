/* Le strutture che il bootloader (Multiboot 1) ci passa all'avvio. */
#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define MULTIBOOT_MAGIC_BOOT 0x2BADB002   /* in EAX se ci ha caricato un bootloader Multiboot */

#define MB_FLAG_MEM     (1 << 0)    /* mem_lower / mem_upper validi */
#define MB_FLAG_MMAP    (1 << 6)    /* mmap_* validi */
#define MB_FLAG_SCHERMO (1 << 12)   /* framebuffer_* validi: la modalita' pixel e' accesa */

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
    unsigned int drives_length, drives_addr, config_table, boot_loader_name, apm_table;
    unsigned int vbe_control_info, vbe_mode_info;
    unsigned short vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;

    /* LO SCHERMO A PIXEL (se c'e' MB_FLAG_SCHERMO) */
    unsigned long long framebuffer_addr;   /* dove sta la memoria dello schermo */
    unsigned int framebuffer_pitch;        /* byte da una riga di pixel alla successiva */
    unsigned int framebuffer_width, framebuffer_height;
    unsigned char framebuffer_bpp;         /* bit per pixel */
    unsigned char framebuffer_type;        /* 1 = colori RGB diretti */
    /* Due byte vuoti: in GRUB quello che segue e' un'unione che contiene un numero da
     * 4 byte, quindi parte a un indirizzo multiplo di 4 (offset 112, non 110). */
    unsigned char vuoto[2];
    unsigned char rosso_posizione, rosso_bit, verde_posizione, verde_bit, blu_posizione, blu_bit;
} __attribute__((packed));

#define MB_SCHERMO_RGB 1

/* Una regione della mappa. 'size' NON conta se stesso: la voce dopo e' a size+4 byte. */
struct multiboot_mmap {
    unsigned int  size;
    unsigned long long base;
    unsigned long long len;
    unsigned int  type;         /* 1 = libera, altro = riservata */
} __attribute__((packed));

#define MB_MMAP_LIBERA 1

#endif
