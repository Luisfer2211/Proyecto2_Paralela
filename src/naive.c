/* naive.c - Busqueda paralela "naive" con Open MPI (Parte A4).
 *
 * Estrategia: se divide [0, upper) en N bloques CONTIGUOS e iguales; el proceso
 * 'id' recorre su bloque de forma incremental. Es la version del bruteforce.c
 * base, pero con tres correcciones:
 *   1) El indice del lazo es 'long' (el base usaba 'int' y desbordaba en 2^56).
 *   2) Terminacion temprana REAL: el base publicaba un MPI_Irecv pero nunca lo
 *      probaba, asi que los demas procesos jamas se detenian. Aqui se hace
 *      MPI_Test periodico para que todos paren al encontrarse la llave.
 *   3) Programacion defensiva y cierre limpio (sin Irecv colgados).
 *
 * Primitivas MPI usadas (Parte A6): MPI_Irecv, MPI_Test, MPI_Send, MPI_Wait,
 * mas MPI_Bcast (difusion del cifrado) y MPI_Allreduce (convergencia final).
 *
 * Uso: mpirun -np <N> ./naive <archivo_cifrado> <frase_clave> [cota_superior]
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

  /* Division en bloques contiguos (naive). */
  long range = upper / N;
  long mylower = range * id;
  long myupper = (id == N - 1) ? upper : range * (id + 1);

  /* Recepcion no bloqueante del anuncio "llave encontrada". */
  long recv_key = -1;
  MPI_Request req;
  MPI_Irecv(&recv_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND, comm, &req);

  MPI_Barrier(comm);
  double t0 = MPI_Wtime();

  long found = -1;
  int recv_done = 0, flag = 0;
  long cnt = 0;
  for (long i = mylower; i < myupper; ++i) {
    if ((cnt++ % CHECK_INTERVAL) == 0) {
      MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
      if (flag) { found = recv_key; recv_done = 1; break; }
    }
    if (try_key(i, ciph, (int)len, frase)) {
      found = i;
      for (int n = 0; n < N; ++n)
        MPI_Send(&found, 1, MPI_LONG, n, TAG_FOUND, comm); /* avisa a todos */
      break;
    }
  }

  /* Convergencia: todos acuerdan la llave global (MAX ignora los -1). */
  long global = -1;
  MPI_Allreduce(&found, &global, 1, MPI_LONG, MPI_MAX, comm);
  double t1 = MPI_Wtime();

  /* Cierre limpio del Irecv pendiente. */
  if (!recv_done) {
    if (global >= 0) MPI_Wait(&req, MPI_STATUS_IGNORE); /* consume el mensaje que ya fue enviado */
    else { MPI_Cancel(&req); MPI_Wait(&req, MPI_STATUS_IGNORE); }
  }

  if (id == 0) print_result("naive", ruta, frase, global, ciph, len, N, t1 - t0);

  free(ciph);
  MPI_Finalize();
  return 0;
}
