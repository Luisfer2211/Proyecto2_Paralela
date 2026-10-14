/* common.c - Implementacion de utilidades compartidas. Ver common.h. */
#define _POSIX_C_SOURCE 199309L  /* requerido por clock_gettime con -std=c11 */
#include "common.h"
#include "des.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

void spread_key(long key, unsigned char out8[8]) {
  /* 7 bits de la llave por byte, en las posiciones altas; LSB para paridad.
   * Esta es la forma clara y portable del lazo original de bruteforce.c, que
   * usaba '0xFE << i*8' con 'int' y desbordaba para los bytes altos (i>=4).
   * Aqui usamos corrimientos sobre 'key' (long), valido para todo el rango. */
  for (int i = 0; i < 8; ++i) {
    unsigned char seven = (unsigned char)((key >> (7 * i)) & 0x7F);
    out8[i] = (unsigned char)(seven << 1);
  }
  des_setparity((char *)out8);
}

void decrypt_key(long key, char *ciph, int len) {
  unsigned char k[8];
  spread_key(key, k);
  ecb_crypt((char *)k, ciph, (unsigned)len, DES_DECRYPT);
}

void encrypt_key(long key, char *ciph, int len) {
  unsigned char k[8];
  spread_key(key, k);
  ecb_crypt((char *)k, ciph, (unsigned)len, DES_ENCRYPT);
}

int try_key(long key, const unsigned char *ciph, int len, const char *search) {
  /* Buffer local: el texto de prueba del proyecto es corto. Se usa un tope
   * fijo para no asignar memoria en el lazo caliente de busqueda. */
  enum { MAXLEN = 4096 };
  char temp[MAXLEN + 1];
  int n = len;
  if (n > MAXLEN) n = MAXLEN;      /* seguridad: nunca desbordar el buffer */
  n -= (n % 8);                    /* DES opera en bloques de 8 */
  memcpy(temp, ciph, (size_t)n);
  temp[n] = '\0';
  decrypt_key(key, temp, n);
  temp[n] = '\0';                  /* re-asegura terminador tras descifrar */
  return strstr(temp, search) != NULL;
}

unsigned char *read_file(const char *path, long *len) {
  if (path == NULL || len == NULL) return NULL;
  FILE *f = fopen(path, "rb");
  if (f == NULL) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long sz = ftell(f);
  if (sz < 0) { fclose(f); return NULL; }
  rewind(f);
  unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1);
  if (buf == NULL) { fclose(f); return NULL; }
  size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if ((long)got != sz) { free(buf); return NULL; }
  buf[sz] = '\0';
  *len = sz;
  return buf;
}

int write_file(const char *path, const unsigned char *data, long len) {
  if (path == NULL || data == NULL) return 1;
  FILE *f = fopen(path, "wb");
  if (f == NULL) return 2;
  size_t put = fwrite(data, 1, (size_t)len, f);
  int rc = (fclose(f) == 0 && (long)put == len) ? 0 : 3;
  return rc;
}

long pad_len(long len) {
  return ((len + 7) / 8) * 8;
}

double now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int parse_long(const char *s, long *out) {
  if (s == NULL || out == NULL || *s == '\0') return -1;
  errno = 0;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (errno == ERANGE) return -1;
  /* Permite un sufijo 'L' o 'l' opcional (como en 123456L). */
  if (end != NULL && (*end == 'L' || *end == 'l')) end++;
  if (end == NULL || *end != '\0') return -1;
  *out = v;
  return 0;
}
