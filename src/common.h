/* common.h - Utilidades compartidas por todos los programas del proyecto:
 * expansion de llave, cifrado/descifrado por llave, prueba de llave (try_key),
 * E/S de archivos, medicion de tiempo y parseo defensivo de argumentos. */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* Espacio de llaves DES efectivo: 2^56. */
#define DES_KEYSPACE (1L << 56)

/* Expande una llave de hasta 56 bits (long) a 8 bytes con paridad para DES.
 * Coloca 7 bits de la llave en la parte alta de cada byte y deja el LSB para
 * paridad (equivalente, ya corregido, al lazo del bruteforce.c base). */
void spread_key(long key, unsigned char out8[8]);

/* Descifra / cifra 'len' bytes EN SITIO con la llave 'key' (ya expandida). */
void decrypt_key(long key, char *ciph, int len);
void encrypt_key(long key, char *ciph, int len);

/* Intenta la llave: descifra una copia de 'ciph' (len bytes) y revisa si
 * 'search' aparece como substring. Retorna 1 si parece correcta, 0 si no. */
int try_key(long key, const unsigned char *ciph, int len, const char *search);

/* Lee todo un archivo binario a un buffer malloc'd (no null-terminado).
 * Devuelve el buffer y escribe su tamano en *len. NULL en error. */
unsigned char *read_file(const char *path, long *len);

/* Escribe 'len' bytes a 'path'. Retorna 0 en exito, distinto de 0 en error. */
int write_file(const char *path, const unsigned char *data, long len);

/* Redondea 'len' al siguiente multiplo de 8 (tamano de bloque DES). */
long pad_len(long len);

/* Tiempo monotonico en segundos (para mediciones de wall-clock). */
double now_seconds(void);

/* Parseo defensivo de un entero largo. Acepta sufijo 'L'/'l' opcional.
 * Retorna 0 en exito y escribe *out; -1 si la cadena es invalida. */
int parse_long(const char *s, long *out);

#endif /* COMMON_H */
