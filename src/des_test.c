/* des_test.c - Verifica la correctitud del DES propio con vectores conocidos
 * (FIPS 46-3 / ejemplos clasicos). Imprime PASS/FAIL y retorna 0 si todo pasa. */
#include "des.h"
#include <stdio.h>
#include <inttypes.h>

static int check(const char *nombre, uint64_t got, uint64_t want) {
  int ok = (got == want);
  printf("[%s] %s  esperado=%016" PRIX64 "  obtenido=%016" PRIX64 "\n",
         ok ? "PASS" : "FAIL", nombre, want, got);
  return ok;
}

int main(void) {
  int ok = 1;

  /* Vector clasico (Wikipedia / tutorial DES). */
  {
    uint64_t key = 0x133457799BBCDFF1ULL;
    uint64_t pt  = 0x0123456789ABCDEFULL;
    uint64_t ct  = 0x85E813540F0AB405ULL;
    uint64_t enc = des_crypt_block(pt, key, 0);
    uint64_t dec = des_crypt_block(ct, key, 1);
    ok &= check("vector1 cifrado", enc, ct);
    ok &= check("vector1 descifrado", dec, pt);
  }

  /* Llave y texto en cero (la paridad se descarta en PC-1). */
  {
    uint64_t key = 0x0101010101010101ULL;
    uint64_t pt  = 0x0000000000000000ULL;
    uint64_t ct  = 0x8CA64DE9C1B123A7ULL;
    uint64_t enc = des_crypt_block(pt, key, 0);
    uint64_t dec = des_crypt_block(ct, key, 1);
    ok &= check("vector2 cifrado", enc, ct);
    ok &= check("vector2 descifrado", dec, pt);
  }

  /* Round-trip aleatorio: descifrar(cifrar(x)) == x. */
  {
    uint64_t key = 0xAABB09182736CCDDULL;
    uint64_t x   = 0xDEADBEEFCAFEBABEULL;
    uint64_t rt  = des_crypt_block(des_crypt_block(x, key, 0), key, 1);
    ok &= check("round-trip", rt, x);
  }

  /* Prueba de la API de alto nivel ecb_crypt + des_setparity sobre 16 bytes. */
  {
    char key[8] = {0x13,0x34,0x57,0x79,(char)0x9B,(char)0xBC,(char)0xDF,(char)0xF1};
    des_setparity(key);
    char buf[17] = "HOLA MUNDO DES!!"; /* 16 bytes utiles */
    char orig[17];
    for (int i = 0; i < 17; ++i) orig[i] = buf[i];
    ecb_crypt(key, buf, 16, DES_ENCRYPT);
    int diff = 0;
    for (int i = 0; i < 16; ++i) if (buf[i] != orig[i]) diff = 1;
    ecb_crypt(key, buf, 16, DES_DECRYPT);
    int same = 1;
    for (int i = 0; i < 16; ++i) if (buf[i] != orig[i]) same = 0;
    int pass = diff && same;
    printf("[%s] ecb_crypt round-trip (16 bytes)\n", pass ? "PASS" : "FAIL");
    ok &= pass;
  }

  printf("\n== Resultado: %s ==\n", ok ? "TODOS LOS VECTORES PASARON" : "HAY FALLOS");
  return ok ? 0 : 1;
}
