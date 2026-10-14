#!/usr/bin/env bash
# run_bench.sh - Genera la bitacora de pruebas (Anexo 2) del proyecto.
#
# Mide cuanto tarda cada buscador en encontrar la llave en tres fases:
#   Fase 1: llaves literales del enunciado (Parte B2), espacio completo 2^56.
#   Fase 2: speedups MEDIBLES en un espacio acotado (cota = 2^20).
#   Fase 3: extra MPI+OpenMP (hibrido) variando procesos e hilos.
# Escribe resultados/bench.csv (una fila por corrida) y resultados/evidencia.txt
# (capturas de consola para el reporte).
#
# Uso:  bash scripts/run_bench.sh      (variables: REPS, TIMEOUT, NPS, COTA)
set -u

cd "$(dirname "$0")/.." || exit 1
ROOT="$(pwd)"
OUT="$ROOT/resultados"
mkdir -p "$OUT"
CSV="$OUT/bench.csv"
EVID="$OUT/evidencia.txt"

TEXTO="Esta es una prueba de proyecto 2"
FRASE="es una prueba de"
ENTRADA="$ROOT/entrada_bench.txt"
REPS="${REPS:-3}"                 # repeticiones por punto medible (mediana)
TIMEOUT="${TIMEOUT:-20}"          # seg. maximos por corrida medible
TO_B2="${TO_B2:-12}"              # seg. maximos para las llaves del enunciado
NPS="${NPS:-1 2 4}"               # numeros de procesos
COTA="${COTA:-$((1 << 20))}"      # 1,048,576: espacio acotado medible
OVERSUB="--oversubscribe"
MPI_PROGS="naive ciclico maestro_trabajador aleatorio"

echo "== Compilando =="
make -s all || { echo "ERROR: fallo la compilacion (¿Open MPI instalado?)"; exit 1; }
printf '%s' "$TEXTO" > "$ENTRADA"

mediana() { sort -n | awk '{a[NR]=$1} END{ if(NR==0){print "NA"} else if(NR%2){print a[(NR+1)/2]} else {printf "%.6f",(a[NR/2]+a[NR/2+1])/2} }'; }
extrae_tiempo() { grep -iE 'tiempo' | grep -oE '[0-9]+\.[0-9]+' | head -1; }
extrae_llave()  { grep -iE 'LLAVE' | grep -oE '[0-9]+' | head -1; }

correr() { # $1=timeout  $2=comando  ->  "tiempo|estado|llave"
  local salida estado t k
  salida="$(timeout "$1" bash -c "$2 </dev/null 2>/dev/null")"; estado=$?
  if [ "$estado" -eq 124 ]; then echo "TIMEOUT|TIMEOUT|"; return; fi
  t="$(printf '%s\n' "$salida" | extrae_tiempo)"
  k="$(printf '%s\n' "$salida" | extrae_llave)"
  [ -z "$t" ] && { echo "NA|ERROR|"; return; }
  echo "$t|OK|$k"
}

cifrar_con() { ./bin/cifrar cifrar "$ENTRADA" "$2" "$1" >/dev/null 2>&1; }  # $1=llave $2=salida

echo "programa,escenario,llave,np,hilos,tiempo_s,estado,llave_hallada" > "$CSV"

# ---------------------------------------------------------------------------
# Evidencia de consola para el Anexo 2.
# ---------------------------------------------------------------------------
{
  echo "============================================================"
  echo " EVIDENCIA DE EJECUCION  -  $(date)"
  echo "============================================================"
  echo; echo "### Vectores de prueba DES ###"
  ./bin/des_test
  echo; echo "### Parte B1: cifrar/descifrar con llave 42 ###"
  ./bin/cifrar cifrar "$ENTRADA" "$OUT/demo42.bin" 42
  ./bin/cifrar descifrar "$OUT/demo42.bin" "$OUT/demo42.txt" 42
  echo; echo "### Una corrida de cada buscador (np=4, cota=$COTA, llave media) ###"
  KDEMO=$(( COTA/2 + COTA/8 )); cifrar_con "$KDEMO" "$OUT/demo_media.bin"
  ./bin/secuencial "$OUT/demo_media.bin" "$FRASE" "$COTA" </dev/null
  for p in $MPI_PROGS; do
    mpirun $OVERSUB -np 4 ./bin/$p "$OUT/demo_media.bin" "$FRASE" "$COTA" </dev/null 2>/dev/null
  done
  OMP_NUM_THREADS=4 mpirun $OVERSUB -np 2 ./bin/hibrido "$OUT/demo_media.bin" "$FRASE" "$COTA" </dev/null 2>/dev/null
} > "$EVID" 2>&1
echo "== Evidencia guardada en $EVID =="

# ---------------------------------------------------------------------------
# FASE 1: llaves literales del enunciado (Parte B2), espacio completo.
#   123456              -> se halla rapido
#   18014398509481983   -> 2^54-1 (final del bloque 0 con np=4): NO termina
#   18014398509481984   -> 2^54 (inicio del bloque 1 con np=4): instantaneo en naive
# ---------------------------------------------------------------------------
echo "== FASE 1: llaves del enunciado (np=4) =="
for kv in "123456" "18014398509481983" "18014398509481984"; do
  cifrar_con "$kv" "$OUT/cipher_b2_$kv.bin"
  r="$(correr "$TO_B2" "./bin/secuencial '$OUT/cipher_b2_$kv.bin' '$FRASE'")"
  echo "secuencial,b2_$kv,$kv,1,1,${r%%|*},$(echo "$r"|cut -d'|' -f2),$(echo "$r"|cut -d'|' -f3)" >> "$CSV"
  for p in $MPI_PROGS; do
    r="$(correr "$TO_B2" "mpirun $OVERSUB -np 4 ./bin/$p '$OUT/cipher_b2_$kv.bin' '$FRASE'")"
    echo "$p,b2_$kv,$kv,4,1,${r%%|*},$(echo "$r"|cut -d'|' -f2),$(echo "$r"|cut -d'|' -f3)" >> "$CSV"
  done
done

# ---------------------------------------------------------------------------
# FASE 2: speedups medibles (espacio acotado COTA). Analogos de las llaves
# facil/media/dificil del enunciado, mas los casos mejor/peor del naive.
# ---------------------------------------------------------------------------
echo "== FASE 2: speedups medibles (cota=$COTA) =="
declare -A ESC
ESC[naive_mejor]=$(( COTA/4 + 1 ))                 # inicio de bloque -> naive instantaneo
ESC[naive_peor]=$(( COTA/4 - 1 ))                  # final del bloque 0 -> naive speedup~1
ESC[facil]=$(( COTA/2 + 1 ))                       # analogo de 2^56/2 + 1
ESC[media]=$(( COTA/2 + COTA/8 ))                  # analogo de 2^56/2 + 2^56/8
ESC[dificil]=$(( (COTA+6)/7 + (COTA+12)/13 ))      # analogo de 2^56/7 + 2^56/13 (techo)

for nombre in naive_mejor naive_peor facil media dificil; do
  llave=${ESC[$nombre]}
  cipher="$OUT/cipher_$nombre.bin"; cifrar_con "$llave" "$cipher"
  echo "  escenario $nombre (llave=$llave)"
  for _ in $(seq 1 "$REPS"); do
    r="$(correr "$TIMEOUT" "./bin/secuencial '$cipher' '$FRASE' $COTA")"
    echo "secuencial,$nombre,$llave,1,1,${r%%|*},$(echo "$r"|cut -d'|' -f2),$(echo "$r"|cut -d'|' -f3)" >> "$CSV"
  done
  for p in $MPI_PROGS; do
    for np in $NPS; do
      for _ in $(seq 1 "$REPS"); do
        r="$(correr "$TIMEOUT" "mpirun $OVERSUB -np $np ./bin/$p '$cipher' '$FRASE' $COTA")"
        echo "$p,$nombre,$llave,$np,1,${r%%|*},$(echo "$r"|cut -d'|' -f2),$(echo "$r"|cut -d'|' -f3)" >> "$CSV"
      done
    done
  done
done

# ---------------------------------------------------------------------------
# FASE 3: extra MPI+OpenMP (hibrido) sobre el escenario 'media'.
# ---------------------------------------------------------------------------
echo "== FASE 3: hibrido MPI+OpenMP (escenario media) =="
cipher="$OUT/cipher_media.bin"
for np in $NPS; do
  for th in 1 2 4; do
    for _ in $(seq 1 "$REPS"); do
      r="$(correr "$TIMEOUT" "OMP_NUM_THREADS=$th mpirun $OVERSUB -np $np ./bin/hibrido '$cipher' '$FRASE' $COTA")"
      echo "hibrido,media,${ESC[media]},$np,$th,${r%%|*},$(echo "$r"|cut -d'|' -f2),$(echo "$r"|cut -d'|' -f3)" >> "$CSV"
    done
  done
done

echo "== Listo: $CSV =="
