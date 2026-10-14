/* cifrar.c - Herramienta para cifrar/descifrar un archivo de texto con una
 * llave DES arbitraria (Parte B1 del proyecto).
 *
 * Uso:
 *   cifrar cifrar    <entrada.txt>  <salida.bin>  <llave>
 *   cifrar descifrar <entrada.bin>  <salida.txt>  <llave>
 *
 * El texto se padea con ceros al siguiente multiplo de 8 (bloque DES). Al
 * descifrar se recortan los ceros finales para recuperar el texto original. */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void uso(const char *prog) {
  fprintf(stderr,
    "Uso:\n"
    "  %s cifrar    <entrada.txt> <salida.bin> <llave>\n"
    "  %s descifrar <entrada.bin> <salida.txt> <llave>\n",
    prog, prog);
}

int main(int argc, char *argv[]) {
  if (argc != 5) { uso(argv[0]); return 1; }

  const char *modo    = argv[1];
  const char *entrada = argv[2];
  const char *salida  = argv[3];

  long llave;
  if (parse_long(argv[4], &llave) != 0 || llave < 0) {
    fprintf(stderr, "Error: llave invalida '%s' (debe ser un entero >= 0).\n", argv[4]);
    return 1;
  }

  int cifrar;
  if      (strcmp(modo, "cifrar")    == 0) cifrar = 1;
  else if (strcmp(modo, "descifrar") == 0) cifrar = 0;
  else { fprintf(stderr, "Error: modo '%s' desconocido.\n", modo); uso(argv[0]); return 1; }

  long len = 0;
  unsigned char *data = read_file(entrada, &len);
  if (data == NULL) {
    fprintf(stderr, "Error: no se pudo leer el archivo '%s'.\n", entrada);
    return 1;
  }

  if (cifrar) {
    /* Padea con ceros al multiplo de 8 y cifra. */
    long plen = pad_len(len);
    unsigned char *buf = (unsigned char *)calloc((size_t)plen, 1);
    if (buf == NULL) { fprintf(stderr, "Error: sin memoria.\n"); free(data); return 1; }
    memcpy(buf, data, (size_t)len);
    encrypt_key(llave, (char *)buf, (int)plen);
    int rc = write_file(salida, buf, plen);
    if (rc != 0) { fprintf(stderr, "Error: no se pudo escribir '%s'.\n", salida); free(buf); free(data); return 1; }
    printf("Cifrado OK: '%s' (%ld bytes) -> '%s' (%ld bytes) con llave %ld.\n",
           entrada, len, salida, plen, llave);
    free(buf);
  } else {
    /* El archivo cifrado debe ser multiplo de 8. */
    if (len % 8 != 0) {
      fprintf(stderr, "Error: '%s' no es multiplo de 8 bytes (%ld); no es un cifrado DES valido.\n", entrada, len);
      free(data);
      return 1;
    }
    decrypt_key(llave, (char *)data, (int)len);
    /* Recorta ceros finales (padding) para recuperar el texto. */
    long real = len;
    while (real > 0 && data[real - 1] == '\0') real--;
    int rc = write_file(salida, data, real);
    if (rc != 0) { fprintf(stderr, "Error: no se pudo escribir '%s'.\n", salida); free(data); return 1; }
    printf("Descifrado OK: '%s' -> '%s' con llave %ld.\n", entrada, salida, llave);
    printf("Texto recuperado: \"%.*s\"\n", (int)real, (char *)data);
  }

  free(data);
  return 0;
}
