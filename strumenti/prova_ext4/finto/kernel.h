/* Al posto di src/lib/kernel.h, per compilare src/fs/ext4/ sul computer che compila
 * (strumenti/prova_ext4): memset, memcpy e memcmp le da' la libreria C vera; kprintf e
 * vkprintf le scrive prova_ext4.c (sull'uscita degli errori). */
#ifndef KERNEL_H
#define KERNEL_H
#include <stdarg.h>
#include <string.h>
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list ap);
#endif
