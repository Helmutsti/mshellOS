/* stb_image (v2.30, di Sean Barrett, pubblico dominio o MIT: LICENSE), compilato una volta
 * per tutta la libreria. Solo le PNG, e senza le funzioni sui FILE della libreria C: i file
 * li legge GKit (gkit/immagine_file.c) con le funzioni della libreria dei programmi, e
 * passa a stb solo i byte. Il codice di stb e' quello originale, non toccato:
 * https://github.com/nothings/stb, commit f58f558 (stb_image.h). */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"
