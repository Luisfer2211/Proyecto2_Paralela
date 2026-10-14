/* aleatorio.c - Enfoque 3 (extra): recorrido PSEUDOALEATORIO del espacio.
 *
 * Idea: el tiempo del naive y del ciclico aun depende de "donde" cae la llave
 * dentro del orden incremental. Si en cambio recorremos el espacio en un orden
 * pseudoaleatorio -pero cubriendo cada llave EXACTAMENTE UNA VEZ- el tiempo
 * esperado para hallar la llave se vuelve insensible a su valor numerico (deja
 * de haber llaves "adversarias" sistematicamente faciles o dificiles).
 *
 * Mecanismo: se construye una BIYECCION pseudoaleatoria 'perm' sobre [0, M)
 * con una red de Feistel, donde M = 2^b es la menor potencia de 2 >= cota.
 * El proceso 'id' recorre indices j = id, id+N, id+2N, ... y prueba perm(j)
 * cuando perm(j) < cota (cycle-walking). Como 'perm' es biyectiva y los indices
 * se reparten sin traslape, se cubre [0, cota) una sola vez, en orden revuelto.
 *
 * Uso: mpirun -np <N> ./aleatorio <archivo_cifrado> <frase_clave> [cota_superior]
 */
#include "mpi_search.h"
#include <stdint.h>

/* Funcion de ronda (mezcla no lineal); cualquier funcion sirve para que la
 * red de Feistel sea biyectiva. Usa constantes tipo splitmix64. */
static uint64_t round_fn(uint64_t r, int round) {
  uint64_t x = r + 0x9E3779B97F4A7C15ULL * (uint64_t)(round + 1);
  x ^= x >> 33; x *= 0xFF51AFD7ED558CCDULL;
  x ^= x >> 33; x *= 0xC4CEB9FE1A85EC53ULL;
  x ^= x >> 33;
  return x;
}

/* Biyeccion pseudoaleatoria sobre [0, 2^(2*half)) via Feistel de 'rounds' rondas. */
static uint64_t feistel_perm(uint64_t x, int half, int rounds) {
  uint64_t mask = (1ULL << half) - 1ULL;
  uint64_t L = (x >> half) & mask;
  uint64_t R = x & mask;
  for (int r = 0; r < rounds; ++r) {
    uint64_t f = round_fn(R, r) & mask;
    uint64_t nL = R;
    uint64_t nR = L ^ f;
    L = nL; R = nR;
  }
  return (L << half) | R;
}

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

  /* Dimensiona la biyeccion: b par con 2^b >= upper; M = 2^b. */
  int bits = 0;
  while ((1ULL << bits) < (uint64_t)upper) bits++;
  if (bits % 2) bits++;
  if (bits < 2) bits = 2;
  const int half = bits / 2;
  const int rounds = 4;
  const uint64_t M = (bits >= 63) ? (1ULL << 63) : (1ULL << bits);

  long recv_key = -1;
  MPI_Request req;
  MPI_Irecv(&recv_key, 1, MPI_LONG, MPI_ANY_SOURCE, TAG_FOUND, comm, &req);

  MPI_Barrier(comm);
  double t0 = MPI_Wtime();

  long found = -1;
  int recv_done = 0, flag = 0;
  long cnt = 0;
  for (uint64_t j = (uint64_t)id; j < M; j += (uint64_t)N) {
    if ((cnt++ % CHECK_INTERVAL) == 0) {
      MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
      if (flag) { found = recv_key; recv_done = 1; break; }
    }
    uint64_t key = feistel_perm(j, half, rounds);
    if (key >= (uint64_t)upper) continue;           /* cycle-walking */
    if (try_key((long)key, ciph, (int)len, frase)) {
      found = (long)key;
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

  if (id == 0) print_result("aleatorio", ruta, frase, global, ciph, len, N, t1 - t0);

  free(ciph);
  MPI_Finalize();
  return 0;
}
