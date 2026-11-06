/* Il kernel: scrive una riga sullo schermo.
 * In modalita' testo lo schermo e' una zona di memoria all'indirizzo 0xB8000:
 * 80x25 caselle da 2 byte, il primo e' il carattere, il secondo il colore. */

void kernel_main(void) {
    volatile unsigned short *schermo = (unsigned short *)0xB8000;
    const char *msg = "Hello, world!";

    for (int i = 0; msg[i]; i++)
        schermo[i] = (unsigned short)msg[i] | 0x0F00;   /* 0x0F = bianco su nero */
}
