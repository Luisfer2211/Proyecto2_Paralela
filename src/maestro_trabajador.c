/* maestro_trabajador.c - Enfoque 2: reparto DINAMICO de trabajo (master-worker).
 *
 * El rank 0 actua como MAESTRO: no busca, solo reparte "chunks" del espacio de
 * llaves bajo demanda. Cada TRABAJADOR (ranks 1..N-1) pide un chunk, lo recorre,
 * y vuelve a pedir; si encuentra la llave avisa al maestro, que entonces ordena
 * detenerse a todos. Ventajas sobre el naive: balanceo de carga automatico y
 * robustez cuando los procesos avanzan a ritmos distintos (p. ej. maquinas
 * heterogeneas), porque el trabajo se asigna segun quien termina antes.
 *
 * Protocolo:  TRABAJADOR --TAG_REQUEST--> MAESTRO --TAG_WORK[ini,fin]--> TRABAJADOR
 *             TRABAJADOR --TAG_RESULT(llave)--> MAESTRO (al encontrar)
 *             chunk con ini<0  => "detente".
 *
 * Uso: mpirun -np <N> ./maestro_trabajador <archivo_cifrado> <frase_clave> [cota]
 */
#include "mpi_search.h"

#define TAG_REQUEST 2
#define TAG_WORK    3
#define TAG_RESULT  4
#define CHUNK_MW    (1L << 16)   /* 65536 llaves por bloque (granularidad fina) */

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

  MPI_Barrier(comm);
  double t0 = MPI_Wtime();
  long global = -1;

  if (N == 1) {
    /* Degenerado: sin trabajadores, el rank 0 busca el mismo (para -np 1). */
    for (long i = 0; i < upper; ++i)
      if (try_key(i, ciph, (int)len, frase)) { global = i; break; }
  } else if (id == 0) {
    /* MAESTRO: reparte chunks y recoge resultados. */
    long next = 0, foundkey = -1;
    int active = N - 1;
    while (active > 0) {
      long msg;
      MPI_Status st;
      MPI_Recv(&msg, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &st);
      if (st.MPI_TAG == TAG_RESULT) {
        if (msg >= 0) foundkey = msg;
        active--;                         /* ese trabajador ya se detuvo */
      } else { /* TAG_REQUEST */
        long chunk[2];
        if (foundkey >= 0 || next >= upper) {
          chunk[0] = -1; chunk[1] = -1;   /* detente */
          active--;
        } else {
          chunk[0] = next;
          chunk[1] = (next + CHUNK_MW < upper) ? next + CHUNK_MW : upper;
          next += CHUNK_MW;
        }
        MPI_Send(chunk, 2, MPI_LONG, st.MPI_SOURCE, TAG_WORK, comm);
      }
    }
    global = foundkey;
  } else {
    /* TRABAJADOR: pide, busca, repite. */
    long foundkey = -1;
    while (1) {
      long req = 0;
      MPI_Send(&req, 1, MPI_LONG, 0, TAG_REQUEST, comm);
      long chunk[2];
      MPI_Recv(chunk, 2, MPI_LONG, 0, TAG_WORK, comm, MPI_STATUS_IGNORE);
      if (chunk[0] < 0) break;            /* detente */
      for (long i = chunk[0]; i < chunk[1]; ++i)
        if (try_key(i, ciph, (int)len, frase)) { foundkey = i; break; }
      if (foundkey >= 0) {
        MPI_Send(&foundkey, 1, MPI_LONG, 0, TAG_RESULT, comm);
        break;
      }
    }
  }

  /* El maestro conoce la llave; se difunde para dejar el estado consistente. */
  MPI_Bcast(&global, 1, MPI_LONG, 0, comm);
  double t1 = MPI_Wtime();

  if (id == 0) print_result("maestro_trabajador", ruta, frase, global, ciph, len, N, t1 - t0);

  free(ciph);
  MPI_Finalize();
  return 0;
}
