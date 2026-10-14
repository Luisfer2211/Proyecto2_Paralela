# Makefile - Proyecto 2: Fuerza bruta DES con Open MPI
# Objetivos:
#   make            -> compila todo (requiere Open MPI para los binarios paralelos)
#   make seq        -> solo binarios que no requieren MPI (des_test, cifrar, secuencial)
#   make mpi        -> binarios MPI (naive, ciclico, maestro_trabajador, aleatorio) + hibrido
#   make clean      -> borra binarios y objetos

CC      = gcc
MPICC   = mpicc
CFLAGS  = -O2 -Wall -Wextra -std=c11
OMPFLAG = -fopenmp

SRC = src
BIN = bin

# Objetos comunes (C puro, sin MPI): se compilan con gcc y se enlazan en todos.
COMMON_OBJ = $(BIN)/des.o $(BIN)/common.o

SEQ_BINS = $(BIN)/des_test $(BIN)/cifrar $(BIN)/secuencial
MPI_BINS = $(BIN)/naive $(BIN)/ciclico $(BIN)/maestro_trabajador $(BIN)/aleatorio
OMP_BINS = $(BIN)/hibrido

.PHONY: all seq mpi clean
all: seq mpi
seq: $(SEQ_BINS)
mpi: $(MPI_BINS) $(OMP_BINS)

# --- Objetos comunes ---
$(BIN)/des.o: $(SRC)/des.c $(SRC)/des.h | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN)/common.o: $(SRC)/common.c $(SRC)/common.h $(SRC)/des.h | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

# --- Binarios sin MPI (gcc) ---
$(BIN)/des_test: $(SRC)/des_test.c $(BIN)/des.o | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/cifrar: $(SRC)/cifrar.c $(COMMON_OBJ) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/secuencial: $(SRC)/secuencial.c $(COMMON_OBJ) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

# --- Binarios MPI (mpicc) ---
$(BIN)/naive: $(SRC)/naive.c $(COMMON_OBJ) | $(BIN)
	$(MPICC) $(CFLAGS) $^ -o $@

$(BIN)/ciclico: $(SRC)/ciclico.c $(COMMON_OBJ) | $(BIN)
	$(MPICC) $(CFLAGS) $^ -o $@

$(BIN)/maestro_trabajador: $(SRC)/maestro_trabajador.c $(COMMON_OBJ) | $(BIN)
	$(MPICC) $(CFLAGS) $^ -o $@

$(BIN)/aleatorio: $(SRC)/aleatorio.c $(COMMON_OBJ) | $(BIN)
	$(MPICC) $(CFLAGS) $^ -o $@

# --- Hibrido MPI + OpenMP ---
$(BIN)/hibrido: $(SRC)/hibrido.c $(COMMON_OBJ) | $(BIN)
	$(MPICC) $(CFLAGS) $(OMPFLAG) $^ -o $@

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)
