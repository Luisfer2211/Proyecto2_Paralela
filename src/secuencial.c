/* secuencial.c - Busqueda de la llave DES por fuerza bruta, SECUENCIAL.
 * Sirve de linea base (speedup = T_secuencial / T_paralelo) y para medir
 * el tiempo de busqueda con distintas llaves (Parte A3).
 *
 * Uso:
 *   secuencial <archivo_cifrado> <frase_clave> [cota_superior]
 * Si se da 'cota_superior', la busqueda se limita a [0, cota_superior). */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Uso: %s <archivo_cifrado> <frase_clave> [cota_superior]\n", argv[0]);
    return 1;
  }

  const char *ruta  = argv[1];
  const char *frase = argv[2];

  long upper = DES_KEYSPACE; /* 2^56 por defecto */
  if (argc == 4) {
    if (parse_long(argv[3], &upper) != 0 || upper <= 0) {
      fprintf(stderr, "Error: cota superior invalida '%s'.\n", argv[3]);
      return 1;
    }
  }

  if (frase[0] == '\0') {
    fprintf(stderr, "Error: la frase clave no puede estar vacia.\n");
    return 1;
  }

  long len = 0;
  unsigned char *ciph = read_file(ruta, &len);
  if (ciph == NULL) {
    fprintf(stderr, "Error: no se pudo leer el archivo cifrado '%s'.\n", ruta);
    return 1;
  }
  if (len < 8 || len % 8 != 0) {
    fprintf(stderr, "Error: '%s' debe ser multiplo de 8 bytes (tiene %ld).\n", ruta, len);
    free(ciph);
    return 1;
  }

  printf("Busqueda SECUENCIAL  archivo='%s'  frase=\"%s\"  rango=[0, %ld)\n",
         ruta, frase, upper);

  double t0 = now_seconds();
  long found = -1;
  for (long i = 0; i < upper; ++i) {
    if (try_key(i, ciph, (int)len, frase)) { found = i; break; }
  }
  double t1 = now_seconds();

  if (found >= 0) {
    char *claro = (char *)malloc((size_t)len + 1);
    memcpy(claro, ciph, (size_t)len);
    decrypt_key(found, claro, (int)len);
    claro[len] = '\0';
    printf("LLAVE ENCONTRADA: %ld\n", found);
    printf("  archivo cifrado : %s\n", ruta);
    printf("  frase clave     : \"%s\"\n", frase);
    printf("  texto descifrado: \"%s\"\n", claro);
    free(claro);
  } else {
    printf("No se encontro ninguna llave en [0, %ld).\n", upper);
  }
  printf("Tiempo: %.6f s\n", t1 - t0);

  free(ciph);
  return 0;
}
