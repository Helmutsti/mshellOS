/* Al posto di src/memoria/kmalloc.h: kmalloc e kfree le scrive prova_ext4.c (con malloc
 * e free, contando i pezzi per scoprire quelli mai restituiti). */
#ifndef KMALLOC_H
#define KMALLOC_H
void *kmalloc(unsigned int byte);
void kfree(void *p);
#endif
