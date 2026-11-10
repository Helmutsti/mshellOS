/* La seriale PL011: il primo modo di parlare col mondo su ARM. */
#ifndef SERIALE_H
#define SERIALE_H

void seriale_scrivi(char c);              /* una lettera ('\n' diventa "\r\n") */
void seriale_testo(const char *s);        /* una frase */
void seriale_esadecimale(unsigned long n);   /* un numero, in esadecimale: 0x... */

#endif
