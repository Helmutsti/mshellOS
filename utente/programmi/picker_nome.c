/* La regola del nome valido del picker (vedi picker_finestra.h). */
#include <string.h>
#include "picker_finestra.h"

int picker_nome_valido(const char *nome) {
    size_t n = nome ? strlen(nome) : 0;
    if (n == 0 || n > 255) return 0;
    if (strcmp(nome, ".") == 0 || strcmp(nome, "..") == 0) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)nome[i];
        if (c == '/' || c < 0x20 || c == 0x7f) return 0;
    }
    return 1;
}
