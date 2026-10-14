/* des.c - Implementacion estandar de DES (FIPS 46-3), modo ECB.
 *
 * Convencion de bits: los bloques y llaves se tratan como enteros de 64 bits
 * donde el bit numerado "1" en las tablas FIPS es el MAS significativo (MSB).
 * Esto permite traducir las tablas de permutacion directamente.
 */
#include "des.h"
#include <string.h>

/* --- Tablas estandar de DES (FIPS 46-3) --- */

/* Permutacion inicial (IP): 64 -> 64. */
static const int IP[64] = {
  58,50,42,34,26,18,10,2, 60,52,44,36,28,20,12,4,
  62,54,46,38,30,22,14,6, 64,56,48,40,32,24,16,8,
  57,49,41,33,25,17, 9,1, 59,51,43,35,27,19,11,3,
  61,53,45,37,29,21,13,5, 63,55,47,39,31,23,15,7
};

/* Permutacion final (IP^-1): 64 -> 64. */
static const int FP[64] = {
  40,8,48,16,56,24,64,32, 39,7,47,15,55,23,63,31,
  38,6,46,14,54,22,62,30, 37,5,45,13,53,21,61,29,
  36,4,44,12,52,20,60,28, 35,3,43,11,51,19,59,27,
  34,2,42,10,50,18,58,26, 33,1,41, 9,49,17,57,25
};

/* Expansion (E): 32 -> 48. */
static const int E[48] = {
  32, 1, 2, 3, 4, 5,  4, 5, 6, 7, 8, 9,
   8, 9,10,11,12,13, 12,13,14,15,16,17,
  16,17,18,19,20,21, 20,21,22,23,24,25,
  24,25,26,27,28,29, 28,29,30,31,32, 1
};

/* Permutacion P (de la funcion f): 32 -> 32. */
static const int P[32] = {
  16, 7,20,21, 29,12,28,17,  1,15,23,26,  5,18,31,10,
   2, 8,24,14, 32,27, 3, 9, 19,13,30, 6, 22,11, 4,25
};

/* PC-1 (seleccion permutada 1): 64 -> 56. */
static const int PC1[56] = {
  57,49,41,33,25,17, 9,  1,58,50,42,34,26,18,
  10, 2,59,51,43,35,27, 19,11, 3,60,52,44,36,
  63,55,47,39,31,23,15,  7,62,54,46,38,30,22,
  14, 6,61,53,45,37,29, 21,13, 5,28,20,12, 4
};

/* PC-2 (seleccion permutada 2): 56 -> 48. */
static const int PC2[48] = {
  14,17,11,24, 1, 5,  3,28,15, 6,21,10,
  23,19,12, 4,26, 8, 16, 7,27,20,13, 2,
  41,52,31,37,47,55, 30,40,51,45,33,48,
  44,49,39,56,34,53, 46,42,50,36,29,32
};

/* Corrimientos por ronda. */
static const int SHIFTS[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};

/* 8 S-boxes, cada una 4 filas x 16 columnas. */
static const int SBOX[8][64] = {
  { /* S1 */
    14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,
    0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,
    4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,
    15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13 },
  { /* S2 */
    15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,
    3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,
    0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,
    13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9 },
  { /* S3 */
    10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,
    13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,
    13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,
    1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12 },
  { /* S4 */
    7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,
    13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,
    10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,
    3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14 },
  { /* S5 */
    2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,
    14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,
    4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,
    11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3 },
  { /* S6 */
    12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,
    10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,
    9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,
    4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13 },
  { /* S7 */
    4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,
    13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,
    1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,
    6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12 },
  { /* S8 */
    13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,
    1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,
    7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,
    2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11 }
};

/* Permuta 'nbits' tomados de 'in' (que tiene 'in_bits' bits, numerados 1=MSB)
 * segun 'table'. Devuelve un entero con 'nbits' bits (primero=MSB). */
static uint64_t permute(uint64_t in, const int *table, int nbits, int in_bits) {
  uint64_t out = 0;
  for (int i = 0; i < nbits; ++i) {
    out <<= 1;
    out |= (in >> (in_bits - table[i])) & 1ULL;
  }
  return out;
}

/* Rotacion a la izquierda de un valor de 28 bits. */
static uint32_t rotl28(uint32_t v, int n) {
  const uint32_t mask = 0x0FFFFFFFu; /* 28 bits */
  return ((v << n) | (v >> (28 - n))) & mask;
}

/* Calendario de llaves: produce 16 subllaves de 48 bits a partir de la
 * llave de 64 bits (los 8 bits de paridad se descartan en PC-1). */
static void des_key_schedule(uint64_t key64, uint64_t subkeys[16]) {
  uint64_t permuted = permute(key64, PC1, 56, 64); /* 56 bits */
  uint32_t C = (uint32_t)((permuted >> 28) & 0x0FFFFFFFu);
  uint32_t D = (uint32_t)(permuted & 0x0FFFFFFFu);
  for (int i = 0; i < 16; ++i) {
    C = rotl28(C, SHIFTS[i]);
    D = rotl28(D, SHIFTS[i]);
    uint64_t CD = ((uint64_t)C << 28) | (uint64_t)D; /* 56 bits */
    subkeys[i] = permute(CD, PC2, 48, 56);           /* 48 bits */
  }
}

/* Funcion f de Feistel: toma R (32 bits) y una subllave (48 bits). */
static uint32_t feistel(uint32_t R, uint64_t subkey) {
  uint64_t expanded = permute(R, E, 48, 32); /* 48 bits */
  uint64_t x = expanded ^ subkey;
  uint32_t out = 0;
  /* 8 grupos de 6 bits, del MSB al LSB. */
  for (int i = 0; i < 8; ++i) {
    uint32_t six = (uint32_t)((x >> (42 - 6 * i)) & 0x3F);
    int row = ((six & 0x20) >> 4) | (six & 0x01);  /* bits exteriores b1 b6 */
    int col = (six >> 1) & 0x0F;                    /* bits interiores b2..b5 */
    uint32_t val = (uint32_t)SBOX[i][row * 16 + col];
    out = (out << 4) | val;
  }
  return (uint32_t)permute(out, P, 32, 32);
}

uint64_t des_crypt_block(uint64_t block, uint64_t key64, int decrypt) {
  uint64_t subkeys[16];
  des_key_schedule(key64, subkeys);

  uint64_t ip = permute(block, IP, 64, 64);
  uint32_t L = (uint32_t)((ip >> 32) & 0xFFFFFFFFu);
  uint32_t R = (uint32_t)(ip & 0xFFFFFFFFu);

  for (int round = 0; round < 16; ++round) {
    int idx = decrypt ? (15 - round) : round; /* subllaves en orden inverso al descifrar */
    uint32_t newR = L ^ feistel(R, subkeys[idx]);
    L = R;
    R = newR;
  }

  /* Intercambio final: preoutput = R16 L16. */
  uint64_t preoutput = ((uint64_t)R << 32) | (uint64_t)L;
  return permute(preoutput, FP, 64, 64);
}

/* --- Utilidades de conversion bytes <-> uint64 (byte 0 = MSB) --- */
static uint64_t bytes_to_u64(const unsigned char *b) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v = (v << 8) | b[i];
  return v;
}
static void u64_to_bytes(uint64_t v, unsigned char *b) {
  for (int i = 7; i >= 0; --i) { b[i] = (unsigned char)(v & 0xFF); v >>= 8; }
}

int ecb_crypt(char *key, char *data, unsigned datalen, unsigned mode) {
  if (key == NULL || data == NULL) return 1;
  if (datalen % 8 != 0) return 2; /* ECB requiere multiplo de 8 */
  uint64_t key64 = bytes_to_u64((const unsigned char *)key);
  int decrypt = (mode & DES_DECRYPT) ? 1 : 0;
  for (unsigned off = 0; off + 8 <= datalen; off += 8) {
    uint64_t block = bytes_to_u64((const unsigned char *)data + off);
    uint64_t out = des_crypt_block(block, key64, decrypt);
    u64_to_bytes(out, (unsigned char *)data + off);
  }
  return DES_SUCCESS;
}

void des_setparity(char *key) {
  unsigned char *k = (unsigned char *)key;
  for (int i = 0; i < 8; ++i) {
    unsigned char b = k[i];
    int ones = 0;
    for (int j = 1; j < 8; ++j) if (b & (1u << j)) ones++; /* 7 bits de datos */
    if ((ones % 2) == 0) b |= 1u;   /* total impar -> set LSB */
    else                 b &= (unsigned char)~1u;
    k[i] = b;
  }
}
