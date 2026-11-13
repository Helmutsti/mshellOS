/* UIKit. testo437.h: da UTF-8 alla tabella di caratteri dei font del sistema (code page 437).
 * Sta in UIKit perche' riguarda i FONT DEL SISTEMA (uikit/materiali.h), cioe' cosa si
 * disegna: GKit sa scrivere con qualunque font gli si dia (gkit_disegno.h, glifo_di) e non
 * sa niente di quali caratteri hanno i font del sistema. Lo usano lo Store e la scrivania.
 *
 * I testi che arrivano dal launcher (le frasi dei problemi, i messaggi) e quelli dei
 * manifesti (nome, autore, descrizione) sono in UTF-8; i font del sistema oggi hanno solo
 * i caratteri della code page 437 (gkit_disegno.h, glifo_di). Qui si traduce: le lettere
 * accentate che la 437 ha diventano le sue, le virgolette e i trattini "tipografici"
 * diventano quelli semplici, il resto diventa "?". Le virgolette basse (U+00AB, U+00BB) la 437 le ha,
 * ma i font del sistema no: diventano ". Era dello Store; ora sta nella libreria perche' la
 * usa anche la scrivania (i nomi delle app nel vassoio). Il codice e' in interno/testo437.c. */
#ifndef TESTO437_H
#define TESTO437_H

/* Copia 'da' (UTF-8) in 'dove' (code page 437), al massimo 'massimo' byte compreso lo 0. */
void in_437(char *dove, const char *da, int massimo);

#endif
