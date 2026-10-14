/* ciclico.c - Enfoque 1: distribucion CICLICA (stride).
 *
 * En lugar de bloques contiguos, el proceso 'id' prueba las llaves
 *   id, id+N, id+2N, id+3N, ...
 * De este modo el espacio se recorre de forma globalmente uniforme: si la
 * llave esta en la posicion K, ALGUN proceso la alcanza tras ~K/N iteraciones
 * sin importar en que "zona" caiga. Esto elimina la enorme inconsistencia del
 * naive (donde el tiempo depende de en que bloque quedo la llave) y da un
 * "tiempo paralelo esperado" ~ K/N consistente.
 *
 * Uso: mpirun -np <N> ./ciclico <archivo_cifrado> <frase_clave> [cota_superior]
 */
#include "mpi_search.h"

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);
  MPI_Comm comm = MPI_COMM_WORLD;
  int N, id;
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  if (argc < 3 || argc > 4) {
    if (id == 0) fprintf(stderr, "Uso: %s <archivo_cifrado> <frase_clave> [cota_superior]\n", argv[0]);
    MPI_Finalize();
    return 1;
  }
  const char *ruta  = argv[1];
  const char *frase = argv[2];
  long upper = DES_KEYSPACE;
  if (argc == 4 && (parse_long(argv[3], &upper) != 0 || upper <= 0)) {
    if (id == 0) fprintf(stderr, "Error: cota superior invalida '%s'.\n", argv[3]);
    MPI_Finalize();
    return 1;
  }

  long len = 0;
  unsigned char *ciph = load_cipher_bcast(ruta, &len, id);
  if (ciph == NULL) { MPI_Finalize(); return 1; }

  long recv_key = -1;
  MPI_Request req;
  MPI_Irecv(&recv_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND, comm, &req);

  MPI_Barrier(comm);
  double t0 = MPI_Wtime();

  long found = -1;
  int recv_done = 0, flag = 0;
  long cnt = 0;
  /* Recorrido por zancada (stride) de tamano N, empezando en 'id'. */
  for (long i = id; i < upper; i += N) {
    if ((cnt++ % CHECK_INTERVAL) == 0) {
      MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
      if (flag) { found = recv_key; recv_done = 1; break; }
    }
    if (try_key(i, ciph, (int)len, frase)) {
      found = i;
      for (int n = 0; n < N; ++n)
        MPI_Send(&found, 1, MPI_LONG, n, TAG_FOUND, comm);
      break;
    }
  }

  long global = -1;
  MPI_Allreduce(&found, &global, 1, MPI_LONG, MPI_MAX, comm);
  double t1 = MPI_Wtime();

  if (!recv_done) {
    if (global >= 0) MPI_Wait(&req, MPI_STATUS_IGNORE);
    else { MPI_Cancel(&req); MPI_Wait(&req, MPI_STATUS_IGNORE); }
  }

  if (id == 0) print_result("ciclico", ruta, frase, global, ciph, len, N, t1 - t0);

  free(ciph);
  MPI_Finalize();
  return 0;
}
