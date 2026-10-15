# Proyecto 2 — Fuerza bruta DES con Open MPI

Curso: **CC3069 Computación Paralela y Distribuida** — Universidad del Valle de Guatemala.
Integrante: **Luis Palacios #23933**.

Este proyecto encuentra, por **fuerza bruta** y con **Open MPI**, la llave privada **DES** usada para
cifrar un texto. Se sabe que una llave es correcta porque el texto descifrado contiene una **frase clave**
conocida como substring (p. ej. `es una prueba de`).

## Contenido

| Programa | Descripción |
|----------|-------------|
| `bin/cifrar` | Cifra/descifra un archivo de texto con una llave arbitraria (Parte B1). |
| `bin/secuencial` | Búsqueda de llave por fuerza bruta, versión secuencial (Parte A3). |
| `bin/naive` | Versión paralela MPI "naive" (bloques contiguos), corregida (Parte A4). |
| `bin/ciclico` | Enfoque 1: distribución cíclica (stride). |
| `bin/maestro_trabajador` | Enfoque 2: reparto dinámico de bloques (master-worker). |
| `bin/aleatorio` | Enfoque 3: recorrido pseudoaleatorio del subespacio. |
| `bin/hibrido` | Extra: MPI + OpenMP. |
| `bin/des_test` | Pruebas de correctitud del DES propio (vectores NIST). |

## Requisitos

- **WSL2 Ubuntu** (o Linux). Compiladores/herramientas:
  ```bash
  sudo apt update
  sudo apt install -y openmpi-bin libopenmpi-dev make gcc \
    texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended \
    texlive-lang-spanish texlive-pictures latexmk python3-matplotlib
  ```
- No se usa `rpc/des_crypt.h` ni OpenSSL: el DES es **propio y auto-contenido** (`src/des.c`).

## Compilación

```bash
make           # todo (requiere Open MPI)
make seq       # solo lo que no usa MPI (des_test, cifrar, secuencial)
make mpi       # binarios MPI + hibrido
make clean
```

## Uso

```bash
# 1) Cifrar un texto con una llave arbitraria (Parte B1)
echo -n "Esta es una prueba de proyecto 2" > entrada.txt
./bin/cifrar cifrar entrada.txt cifrado.bin 42
./bin/cifrar descifrar cifrado.bin salida.txt 42   # verifica round-trip

# 2) Buscar la llave (secuencial)
./bin/secuencial cifrado.bin "es una prueba de"

# 3) Buscar la llave (paralelo, 4 procesos)
mpirun -np 4 ./bin/naive   cifrado.bin "es una prueba de"
mpirun -np 4 ./bin/ciclico cifrado.bin "es una prueba de"
mpirun -np 4 ./bin/maestro_trabajador cifrado.bin "es una prueba de"
mpirun -np 4 ./bin/aleatorio cifrado.bin "es una prueba de"
mpirun -np 4 ./bin/hibrido cifrado.bin "es una prueba de"   # usa OMP_NUM_THREADS
```

Todos los buscadores aceptan opcionalmente una cota superior de llave para pruebas acotadas:
`... cifrado.bin "frase" <cota_superior>`.

## Benchmark y reporte

```bash
bash scripts/run_bench.sh      # genera resultados/*.csv
python3 scripts/plot.py        # genera graficas en reporte/img/
cd reporte && latexmk -pdf reporte.tex
```

El informe completo está en [`reporte/reporte.pdf`](reporte/reporte.pdf).
