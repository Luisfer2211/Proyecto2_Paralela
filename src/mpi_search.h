/* mpi_search.h - Andamiaje compartido por los buscadores paralelos (MPI):
 *  - load_cipher_bcast: el rank 0 lee y valida el archivo cifrado y lo difunde.
 *  - print_result     : imprime la llave, archivo, frase y texto descifrado.
 * El lazo de busqueda y la estrategia de terminacion viven en cada programa,
 * porque son la parte que distingue a cada enfoque. */
#ifndef MPI_SEARCH_H
#define MPI_SEARCH_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

/* Etiqueta del mensaje que anuncia "llave encontrada". */
#define TAG_FOUND 1

/* Cada cuantas iteraciones un proceso revisa si otro ya encontro la llave.
 * Valor pequeno => menor latencia de parada (y piso de tiempo mas bajo). */
#define CHECK_INTERVAL 4096

/* El rank 0 lee y valida el archivo cifrado; luego se difunde a todos.
 * Devuelve un buffer malloc'd de 'len' bytes (igual en todos los ranks) o NULL
 * si hubo un error (todos los ranks obtienen NULL de forma consistente). */
static inline unsigned char *load_cipher_bcast(const char *path, long *len, int rank) {
  long n = 0;
  unsigned char *buf = NULL;
  if (rank == 0) {
    buf = read_file(path, &n);
    if (buf == NULL) {
      fprintf(stderr, "Error: no se pudo leer el archivo cifrado '%s'.\n", path);
      n = -1;
    } else if (n < 8 || n % 8 != 0) {
      fprintf(stderr, "Error: '%s' debe ser multiplo de 8 bytes (tiene %ld).\n", path, n);
      free(buf); buf = NULL; n = -1;
    }
  }
  MPI_Bcast(&n, 1, MPI_LONG, 0, MPI_COMM_WORLD);
  if (n <= 0) { if (buf) free(buf); return NULL; }
  if (rank != 0) {
    buf = (unsigned char *)malloc((size_t)n + 1);
    if (buf) buf[n] = '\0';
  }
  if (buf == NULL) { MPI_Abort(MPI_COMM_WORLD, 1); }
  MPI_Bcast(buf, (int)n, MPI_BYTE, 0, MPI_COMM_WORLD);
  *len = n;
  return buf;
}

/* Imprime el resultado desde el rank 0. 'key' < 0 significa "no encontrada". */
static inline void print_result(const char *algo, const char *path, const char *frase,
                                long key, const unsigned char *ciph, long len,
                                int nprocs, double secs) {
  if (key < 0) {
    printf("[%s] No se encontro llave.  procesos=%d  tiempo=%.6f s\n", algo, nprocs, secs);
    return;
  }
  char *claro = (char *)malloc((size_t)len + 1);
  memcpy(claro, ciph, (size_t)len);
  decrypt_key(key, claro, (int)len);
  claro[len] = '\0';
  printf("[%s] LLAVE ENCONTRADA: %ld   procesos=%d   tiempo=%.6f s\n", algo, key, nprocs, secs);
  printf("  archivo cifrado : %s\n", path);
  printf("  frase clave     : \"%s\"\n", frase);
  printf("  texto descifrado: \"%s\"\n", claro);
  free(claro);
}

#endif /* MPI_SEARCH_H */
