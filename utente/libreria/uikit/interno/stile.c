/* UIKit: le ombre a strati del design, livello per livello e tema per tema (vedi "le ombre"
 * in stile.h). I valori sono copiati a mano dal file Figma "mshell - interfaccia" (gli
 * effetti "ombra/..."): se il disegno cambia, si ricopiano qui.
 *
 * Le tabelle si leggono solo attraverso OMBRA_TAB e OMBRA_N (stile.h), che scelgono quella
 * del tema in uso: per questo ogni livello ha una 'struct ombra_del_design' con le sue due
 * tabelle e il numero di righe di ognuna (OMBRA_STRATI, gkit_disegno.h, che qui si puo'
 * contare perche' le tabelle sono intere). */
#include "uikit/stile.h"

static const struct strato_ombra ombra_incassato_chiaro[] = {
    { VELATO(0x14141A, 18), OMBRA_INTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 10), OMBRA_ESTERNA, 0, 1, 0, 0 },
};
static const struct strato_ombra ombra_sollevato_chiaro[] = {
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x14141A, 18), OMBRA_ESTERNA, 0, 2, 4, -1 },
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 6, 12, -4 },
};
static const struct strato_ombra ombra_sollevato_pieno_chiaro[] = {
    { VELATO(0x1F1F26, 26), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x1F1F26, 41), OMBRA_ESTERNA, 0, 4, 8, -2 },
    { VELATO(0x1F1F26, 46), OMBRA_ESTERNA, 0, 10, 20, -6 },
};
static const struct strato_ombra ombra_sovrapposto_chiaro[] = {
    { VELATO(0x14141A, 13), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 20), OMBRA_ESTERNA, 0, 8, 16, -4 },
    { VELATO(0x14141A, 36), OMBRA_ESTERNA, 0, 20, 40, -12 },
};
static const struct strato_ombra ombra_barra_chiaro[] = {
    { VELATO(0x14141A, 15), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 26), OMBRA_ESTERNA, 0, 6, 14, -4 },
    { VELATO(0x14141A, 41), OMBRA_ESTERNA, 0, 16, 32, -10 },
};
static const struct strato_ombra ombra_finestra_chiaro[] = {
    { VELATO(0x14141A, 13), OMBRA_ESTERNA, 0, 2, 4, 0 },
    { VELATO(0x14141A, 26), OMBRA_ESTERNA, 0, 12, 24, -6 },
    { VELATO(0x14141A, 51), OMBRA_ESTERNA, 0, 32, 64, -16 },
};
static const struct strato_ombra ombra_focus_chiaro[] = {
    { VELATO(0x14141A, 18), OMBRA_INTERNA, 0, 1, 2, 0 },
    { VELATO(0x14141A, 36), OMBRA_ESTERNA, 0, 0, 0, 3 },
};
static const struct strato_ombra ombra_puntatore_chiaro[] = {
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 66), OMBRA_ESTERNA, 0, 3, 6, -1 },
};
static const struct strato_ombra ombra_incassato_scuro[] = {
    { VELATO(0x000000, 89), OMBRA_INTERNA, 0, 1, 3, 0 },
};
static const struct strato_ombra ombra_sollevato_scuro[] = {
    { VELATO(0xFFFFFF, 15), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 61), OMBRA_ESTERNA, 0, 3, 6, -1 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 8, 14, -4 },
};
static const struct strato_ombra ombra_sollevato_pieno_scuro[] = {
    { VELATO(0xFFFFFF, 77), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 4, 8, -2 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 12, 22, -6 },
};
static const struct strato_ombra ombra_sovrapposto_scuro[] = {
    { VELATO(0xFFFFFF, 13), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 51), OMBRA_ESTERNA, 0, 2, 3, 0 },
    { VELATO(0x000000, 77), OMBRA_ESTERNA, 0, 10, 20, -4 },
    { VELATO(0x000000, 97), OMBRA_ESTERNA, 0, 24, 44, -12 },
};
static const struct strato_ombra ombra_barra_scuro[] = {
    { VELATO(0xFFFFFF, 18), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 61), OMBRA_ESTERNA, 0, 1, 2, 0 },
    { VELATO(0x000000, 82), OMBRA_ESTERNA, 0, 8, 16, -4 },
    { VELATO(0x000000, 107), OMBRA_ESTERNA, 0, 20, 36, -10 },
};
static const struct strato_ombra ombra_finestra_scuro[] = {
    { VELATO(0xFFFFFF, 18), OMBRA_INTERNA, 0, 1, 0, 0 },
    { VELATO(0x000000, 56), OMBRA_ESTERNA, 0, 2, 4, 0 },
    { VELATO(0x000000, 87), OMBRA_ESTERNA, 0, 16, 28, -6 },
    { VELATO(0x000000, 128), OMBRA_ESTERNA, 0, 40, 72, -16 },
};
static const struct strato_ombra ombra_focus_scuro[] = {
    { VELATO(0x000000, 89), OMBRA_INTERNA, 0, 1, 3, 0 },
    { VELATO(0xFFFFFF, 36), OMBRA_ESTERNA, 0, 0, 0, 3 },
};
static const struct strato_ombra ombra_puntatore_scuro[] = {
    { VELATO(0x000000, 140), OMBRA_ESTERNA, 0, 1, 1, 0 },
    { VELATO(0x000000, 140), OMBRA_ESTERNA, 0, 3, 6, -1 },
};

/* un livello: le sue due tabelle, chiaro e scuro, e quante righe ha ognuna */
#define LIVELLO(nome) { { ombra_##nome##_chiaro, ombra_##nome##_scuro }, \
                        { OMBRA_STRATI(ombra_##nome##_chiaro), OMBRA_STRATI(ombra_##nome##_scuro) } }
const struct ombra_del_design ombre_incassato       = LIVELLO(incassato);
const struct ombra_del_design ombre_sollevato       = LIVELLO(sollevato);
const struct ombra_del_design ombre_sollevato_pieno = LIVELLO(sollevato_pieno);
const struct ombra_del_design ombre_sovrapposto     = LIVELLO(sovrapposto);
const struct ombra_del_design ombre_barra           = LIVELLO(barra);
const struct ombra_del_design ombre_finestra        = LIVELLO(finestra);
const struct ombra_del_design ombre_focus           = LIVELLO(focus);
const struct ombra_del_design ombre_puntatore       = LIVELLO(puntatore);
#undef LIVELLO
