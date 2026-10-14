/* hibrido.c - Extra: MPI + OpenMP (paralelismo de memoria distribuida y
 * compartida combinados).
 *
 * Cada proceso MPI toma su "carril" ciclico (llaves == id mod N) y, dentro del
 * proceso, reparte ese carril entre varios hilos OpenMP: el hilo 't' de un
 * proceso con 'nt' hilos recorre  id + t*N, id + t*N + nt*N, ...  Asi se cubre
 * el carril del proceso sin traslape, aprovechando todos los nucleos de cada
 * maquina ademas de todos los procesos.
 *
 * MPI se inicializa en modo FUNNELED: solo el hilo principal hace llamadas MPI
 * (MPI_Test / MPI_Send), dentro de regiones 'omp master'.
 *
 * El numero de hilos se controla con la variable de entorno OMP_NUM_THREADS.
 * Uso: OMP_NUM_THREADS=4 mpirun -np <N> ./hibrido <cifrado> <frase> [cota]
 */
#include "mpi_search.h"
#include <omp.h>

int main(int argc, char *argv[]) {
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
  MPI_Comm comm = MPI_COMM_WORLD;
  int N, id;
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  if (argc < 3 || argc > 4) {
    if (id == 0) fprintf(stderr, "Uso: %s <archivo_cifrado> <frase_clave> [cota_superior]\n", argv[0]);
    MPI_Finalize();
    return 1;
  }
  if (id == 0 && provided < MPI_THREAD_FUNNELED)
    fprintf(stderr, "Aviso: la implementacion MPI no garantiza FUNNELED.\n");

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

  long found = -1;         /* compartida entre hilos */
  volatile int stop = 0;   /* bandera de parada compartida */
  int recv_done = 0;

  #pragma omp parallel
  {
    int nt  = omp_get_num_threads();
    int tid = omp_get_thread_num();
    long cnt = 0;
    for (long i = id + (long)tid * N; i < upper && !stop; i += (long)nt * N) {
      if ((cnt++ % CHECK_INTERVAL) == 0) {
        #pragma omp master
        {
          int flag = 0;
          MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
          if (flag) {
            #pragma omp critical
            { if (found < 0) found = recv_key; recv_done = 1; }
            stop = 1;
          }
        }
        if (stop) break;
      }
      if (try_key(i, ciph, (int)len, frase)) {
        #pragma omp critical
        { if (found < 0) found = i; }
        stop = 1;
        break;
      }
    }
  }

  /* Si esta maquina encontro la llave (no por aviso externo), avisa a todos. */
  if (found >= 0 && !recv_done)
    for (int n = 0; n < N; ++n)
      MPI_Send(&found, 1, MPI_LONG, n, TAG_FOUND, comm);

  long global = -1;
  MPI_Allreduce(&found, &global, 1, MPI_LONG, MPI_MAX, comm);
  double t1 = MPI_Wtime();

  if (!recv_done) {
    if (global >= 0) MPI_Wait(&req, MPI_STATUS_IGNORE);
    else { MPI_Cancel(&req); MPI_Wait(&req, MPI_STATUS_IGNORE); }
  }

  if (id == 0) {
    int nthreads = omp_get_max_threads();
    char etiqueta[64];
    snprintf(etiqueta, sizeof etiqueta, "hibrido(%d hilos/proc)", nthreads);
    print_result(etiqueta, ruta, frase, global, ciph, len, N, t1 - t0);
  }

  free(ciph);
  MPI_Finalize();
  return 0;
}
