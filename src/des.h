/* des.h - Implementacion propia y auto-contenida de DES (ECB).
 *
 * Reemplaza a <rpc/des_crypt.h> (removida de glibc) sin depender de OpenSSL.
 * Expone la MISMA API que usa el codigo base bruteforce.c, de modo que el
 * resto del programa queda intacto y explicable:
 *
 *   - ecb_crypt(key, data, datalen, mode): cifra/descifra en modo ECB.
 *   - des_setparity(key): fija paridad impar en cada byte de la llave de 8 bytes.
 *   - DES_ENCRYPT / DES_DECRYPT: modos de operacion.
 */
#ifndef DES_H
#define DES_H

#include <stdint.h>

/* Modos compatibles con la API original de SunRPC. */
#define DES_ENCRYPT 0x0000
#define DES_DECRYPT 0x0001

/* Codigo de retorno de exito (compatible con DESERR_NONE). */
#define DES_SUCCESS 0

/* Cifra o descifra 'datalen' bytes (multiplo de 8) EN SITIO, en modo ECB.
 *   key     : 8 bytes de llave (con bits de paridad).
 *   data    : buffer de entrada/salida.
 *   datalen : longitud en bytes, debe ser multiplo de 8.
 *   mode    : DES_ENCRYPT o DES_DECRYPT.
 * Retorna DES_SUCCESS (0) en exito, valor distinto de 0 si hay error. */
int ecb_crypt(char *key, char *data, unsigned datalen, unsigned mode);

/* Ajusta el bit de paridad (LSB) de cada uno de los 8 bytes de 'key' para
 * que cada byte tenga un numero impar de bits en 1 (paridad impar de DES). */
void des_setparity(char *key);

/* --- Rutinas de bajo nivel expuestas para pruebas (des_test.c) --- */

/* Cifra/descifra un unico bloque de 64 bits usando la llave de 64 bits dada
 * (los bits de paridad se ignoran en el calendario de llaves, como en DES). */
uint64_t des_crypt_block(uint64_t block, uint64_t key64, int decrypt);

#endif /* DES_H */
