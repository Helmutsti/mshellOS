/* La seriale PL011: il primo modo di parlare col mondo su ARM. */
#ifndef SERIALE_H
#define SERIALE_H

/* Da qui la seriale sta a questo indirizzo (lo dice il device tree). Prima: 0x09000000,
 * quello della macchina "virt" di QEMU. */
void seriale_usa(unsigned long indirizzo);

void seriale_scrivi(char c);              /* una lettera ('\n' diventa "\r\n") */
void seriale_testo(const char *s);        /* una frase */
void seriale_esadecimale(unsigned long n);   /* un numero, in esadecimale: 0x... */

#endif
