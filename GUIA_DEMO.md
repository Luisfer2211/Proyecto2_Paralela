# Guía de demostración — Proyecto 2 (DES + MPI)

Guía rápida para **presentar y demostrar** cada método en la terminal **WSL (Ubuntu)**.
Texto de prueba: `Esta es una prueba de proyecto 2` · Frase clave: `es una prueba de`.

> ⚠️ **Regla de oro para la demo en vivo:** al enfoque **`aleatorio`** pasale SIEMPRE una **cota**
> (el 3er argumento, p. ej. `1048576`). Sin cota, con una llave pequeña **se cuelga** (recorre en
> orden al azar). Si algo se queda pegado, cortalo con **Ctrl + C**.

---

## 0. Preparar el entorno (hacer esto una vez al inicio)

```bash
# Abrir Ubuntu (WSL) y entrar a la carpeta del proyecto:
cd /mnt/c/Users/lpala/Documents/Paralela/Proyecto2_Paralela

# Compilar todo:
make all

# Crear el texto y cifrarlo con la llave 42 (nuestro caso de prueba):
printf "Esta es una prueba de proyecto 2" > entrada.txt
./bin/cifrar cifrar entrada.txt cifrado.bin 42
```

---

## 1. (Opcional) Mostrar que el DES propio es correcto

```bash
./bin/des_test
```
🗣️ *"Implementé DES desde cero y lo validé con vectores de prueba oficiales (NIST)."*
Debe decir: **TODOS LOS VECTORES PASARON**.

---

## 2. Cifrar / descifrar un archivo (Parte B1)

```bash
./bin/cifrar cifrar    entrada.txt cifrado.bin 42
./bin/cifrar descifrar cifrado.bin salida.txt  42
```
🗣️ *"Ciframos con la llave 42 y al descifrar recuperamos el texto original."*
Lo último que imprime: `Texto recuperado: "Esta es una prueba de proyecto 2"`.

---

## 3. Versión SECUENCIAL (la línea base)

```bash
./bin/secuencial cifrado.bin "es una prueba de"
```
🗣️ *"Una sola persona probando llaves. Es contra esto que comparamos el speedup."*
Imprime la llave encontrada (42) y el **Tiempo**.

---

## 4. Los métodos en PARALELO (todos encuentran la llave 42)

> Se corren con `mpirun`. `-np 4` = 4 procesos. `--oversubscribe` permite más procesos que núcleos.
> Le pasamos la cota `1048576` a todos para que el `aleatorio` también termine rápido y seguro.

```bash
# 1) Naive (por tramos contiguos)
mpirun --oversubscribe -np 4 ./bin/naive              cifrado.bin "es una prueba de" 1048576

# 2) Cíclico (intercalado)
mpirun --oversubscribe -np 4 ./bin/ciclico            cifrado.bin "es una prueba de" 1048576

# 3) Maestro-trabajador (reparto dinámico)
mpirun --oversubscribe -np 4 ./bin/maestro_trabajador cifrado.bin "es una prueba de" 1048576

# 4) Aleatorio (orden revuelto)  -- OJO: la cota es obligatoria aquí
mpirun --oversubscribe -np 4 ./bin/aleatorio          cifrado.bin "es una prueba de" 1048576

# 5) Híbrido MPI + OpenMP (2 procesos x 4 hilos)
OMP_NUM_THREADS=4 mpirun --oversubscribe -np 2 ./bin/hibrido cifrado.bin "es una prueba de" 1048576
```
🗣️ *"Los cinco encuentran la misma llave (42); cada uno reparte el trabajo de forma distinta."*
Cada uno imprime: `LLAVE ENCONTRADA: 42`, el número de procesos y el `tiempo`.

💡 Para variar el paralelismo: cambiá `-np 4` por `-np 1`, `-np 2` u `-np 8`.
Para el híbrido, cambiá `OMP_NUM_THREADS=4` por `1`, `2`, etc.

---

## 5. ⭐ La demo ESTRELLA: por qué el naive es engañoso

Esto es lo que más impresiona. Creamos dos cifrados con llaves **casi iguales** pero en
posiciones opuestas de los bloques (con 4 procesos y cota `1048576`):

```bash
# Llave al INICIO del bloque 1  -> el naive la halla al instante (speedup "falso")
./bin/cifrar cifrar entrada.txt mejor.bin 262145

# Llave al FINAL del bloque 0  -> el naive hace casi todo el trabajo en un solo proceso
./bin/cifrar cifrar entrada.txt peor.bin 262143
```

Ahora compará **naive vs cíclico** en ambos casos:

```bash
echo "--- NAIVE, mejor caso ---"
mpirun --oversubscribe -np 4 ./bin/naive   mejor.bin "es una prueba de" 1048576
echo "--- NAIVE, peor caso ---"
mpirun --oversubscribe -np 4 ./bin/naive   peor.bin  "es una prueba de" 1048576
echo "--- CICLICO, mejor caso ---"
mpirun --oversubscribe -np 4 ./bin/ciclico mejor.bin "es una prueba de" 1048576
echo "--- CICLICO, peor caso ---"
mpirun --oversubscribe -np 4 ./bin/ciclico peor.bin  "es una prueba de" 1048576
```
🗣️ *"Con el naive, el mejor caso es casi instantáneo y el peor caso es lentísimo (a veces peor que
secuencial). El cíclico da casi el mismo tiempo en ambos: es consistente. Esa es la conclusión central
del proyecto: el speedup del naive es engañoso porque depende de dónde cae la llave."*

Resultados típicos (pueden variar según la máquina):
| | Naive | Cíclico |
|---|---|---|
| Mejor caso | ~0.04 s (speedup "falso" enorme) | ~0.9 s |
| Peor caso | ~3.0 s (¡más lento que secuencial!) | ~0.8 s (consistente) |

---

## 6. (Opcional) Las llaves exactas del enunciado (Parte B2)

```bash
# a) Llave 123456: se encuentra rápido
./bin/cifrar cifrar entrada.txt b2.bin 123456
mpirun --oversubscribe -np 4 ./bin/naive b2.bin "es una prueba de"

# b) Llave 2^54-1 = 18014398509481983: NO termina (es el punto del enunciado). Cortar con Ctrl+C.
./bin/cifrar cifrar entrada.txt b2b.bin 18014398509481983
mpirun --oversubscribe -np 4 ./bin/naive b2b.bin "es una prueba de"   # <- Ctrl+C a los ~10 s

# c) Llave 2^54 = 18014398509481984: el naive la halla al instante (cae al inicio de un bloque)
./bin/cifrar cifrar entrada.txt b2c.bin 18014398509481984
mpirun --oversubscribe -np 4 ./bin/naive b2c.bin "es una prueba de"
```

---

## 7. (Opcional) Regenerar la bitácora, las gráficas y el informe

```bash
bash scripts/run_bench.sh      # genera resultados/bench.csv y evidencia.txt (~10 min)
python3 scripts/plot.py        # genera las gráficas en reporte/img/ y la tabla
cd reporte && latexmk -pdf reporte.tex && cd ..   # genera reporte/reporte.pdf
```

---

## 8. Guion sugerido de la demo (3–4 min)

1. `./bin/des_test` → *"mi DES es correcto"*.
2. `cifrar` + `descifrar` con 42 → *"ciframos y recuperamos"*.
3. `secuencial` → *"la línea base"*.
4. `naive`, `ciclico`, `aleatorio`, `hibrido` con cota → *"varias formas de repartir, todas hallan 42"*.
5. **La demo estrella (sección 5)** → *"el naive es engañoso; el cíclico es consistente"*.
6. Cerrar con la gráfica `reporte/img/speedup_dificil.png` del informe.

> Tené la terminal con letra grande y la carpeta ya abierta antes de empezar.
> Si una corrida tarda mucho, **Ctrl + C** y seguí. El aleatorio y las llaves grandes son los únicos
> que pueden colgarse: siempre con cota, o avisá que "esta no termina a propósito".
