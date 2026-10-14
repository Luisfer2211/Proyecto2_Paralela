#!/usr/bin/env python3
"""plot.py - Genera graficas de tiempo y speedup a partir de resultados/bench.csv.

Lee el CSV producido por run_bench.sh, calcula la mediana por
(programa, escenario, np, hilos) y produce figuras en reporte/img/:
  - speedup_<escenario>.png : speedup vs numero de procesos por programa.
  - tiempo_<escenario>.png  : tiempo vs numero de procesos por programa.
  - naive_inconsistencia.png: barras naive 'mejor' vs 'peor' (np=4).
No depende de pandas (solo csv + matplotlib).
"""
import csv
import os
import statistics
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV = os.path.join(ROOT, "resultados", "bench.csv")
IMG = os.path.join(ROOT, "reporte", "img")
os.makedirs(IMG, exist_ok=True)

# Escenarios con cota (medibles) sobre los que calculamos speedup.
MEDIBLES = ["naive_mejor", "naive_peor", "facil", "media", "dificil"]
PROGS = ["naive", "ciclico", "maestro_trabajador", "aleatorio"]


def cargar(path):
    """tiempos[(prog,esc,np,hilos)] = mediana de tiempos OK."""
    bruto = defaultdict(list)
    if not os.path.exists(path):
        print("No existe", path, "- corre primero scripts/run_bench.sh")
        return {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row["estado"] != "OK":
                continue
            try:
                t = float(row["tiempo_s"])
            except ValueError:
                continue
            key = (row["programa"], row["escenario"], int(row["np"]), int(row["hilos"]))
            bruto[key].append(t)
    return {k: statistics.median(v) for k, v in bruto.items()}


def nps_disponibles(med, esc):
    nps = sorted({k[2] for k in med if k[1] == esc and k[0] in PROGS})
    return nps


def grafica_por_escenario(med, esc):
    nps = nps_disponibles(med, esc)
    if not nps:
        return
    # Linea base secuencial (np=1); si falta, usa el menor np del naive.
    base = med.get(("secuencial", esc, 1, 1))
    if base is None:
        base = med.get(("naive", esc, 1, 1))
    # --- Speedup ---
    plt.figure(figsize=(7, 4.5))
    for prog in PROGS:
        xs, ys = [], []
        for np_ in nps:
            t = med.get((prog, esc, np_, 1))
            if t and base:
                xs.append(np_); ys.append(base / t)
        if xs:
            plt.plot(xs, ys, marker="o", label=prog)
    if base:
        plt.plot(nps, nps, "k--", linewidth=1, label="ideal")
    plt.xlabel("Numero de procesos (np)")
    plt.ylabel("Speedup  (T_secuencial / T_paralelo)")
    plt.title(f"Speedup - escenario '{esc}'")
    plt.legend(); plt.grid(True, alpha=0.3); plt.tight_layout()
    plt.savefig(os.path.join(IMG, f"speedup_{esc}.png"), dpi=130)
    plt.close()
    # --- Tiempo ---
    plt.figure(figsize=(7, 4.5))
    for prog in PROGS:
        xs, ys = [], []
        for np_ in nps:
            t = med.get((prog, esc, np_, 1))
            if t:
                xs.append(np_); ys.append(t)
        if xs:
            plt.plot(xs, ys, marker="o", label=prog)
    plt.xlabel("Numero de procesos (np)")
    plt.ylabel("Tiempo (s)")
    plt.title(f"Tiempo de busqueda - escenario '{esc}'")
    plt.legend(); plt.grid(True, alpha=0.3); plt.tight_layout()
    plt.savefig(os.path.join(IMG, f"tiempo_{esc}.png"), dpi=130)
    plt.close()


def grafica_inconsistencia(med):
    """Barras comparando naive en su mejor y peor caso (np=4) vs ciclico."""
    np_ = 4
    escs = ["naive_mejor", "naive_peor"]
    progs = ["naive", "ciclico"]
    datos = {p: [med.get((p, e, np_, 1)) for e in escs] for p in progs}
    if all(v is None for p in progs for v in datos[p]):
        return
    import numpy as np
    x = np.arange(len(escs)); w = 0.35
    plt.figure(figsize=(7, 4.5))
    for i, p in enumerate(progs):
        ys = [v if v is not None else 0 for v in datos[p]]
        plt.bar(x + (i - 0.5) * w, ys, w, label=p)
    plt.xticks(x, ["naive mejor caso", "naive peor caso"])
    plt.ylabel("Tiempo (s)")
    plt.title(f"Inconsistencia del naive segun la llave (np={np_})")
    plt.legend(); plt.grid(True, axis="y", alpha=0.3); plt.tight_layout()
    plt.savefig(os.path.join(IMG, "naive_inconsistencia.png"), dpi=130)
    plt.close()


def generar_tabla(path_csv):
    """Escribe reporte/tabla_bitacora.tex con la mediana de tiempo/estado por
    (escenario, programa, np, hilos). Incluye corridas OK y TIMEOUT."""
    bruto = defaultdict(list)
    estados = {}
    if not os.path.exists(path_csv):
        return
    with open(path_csv, newline="") as f:
        for row in csv.DictReader(f):
            key = (row["escenario"], row["programa"], int(row["np"]), int(row["hilos"]))
            estados.setdefault(key, row["estado"])
            if row["estado"] == "OK":
                try:
                    bruto[key].append(float(row["tiempo_s"]))
                except ValueError:
                    pass
            else:
                estados[key] = row["estado"]
    out = os.path.join(ROOT, "reporte", "tabla_bitacora.tex")
    with open(out, "w") as f:
        f.write("% Tabla generada automaticamente por scripts/plot.py\n")
        f.write("\\begin{longtable}{llrrr}\n\\toprule\n")
        f.write("Escenario & Programa & np & hilos & Tiempo (s) / estado \\\\\n\\midrule\n\\endhead\n")
        for key in sorted(estados):
            esc, prog, np_, th = key
            if key in bruto and bruto[key]:
                val = f"{statistics.median(bruto[key]):.6f}"
            else:
                val = estados[key]
            esc_e = esc.replace("_", "\\_"); prog_e = prog.replace("_", "\\_")
            f.write(f"{esc_e} & {prog_e} & {np_} & {th} & {val} \\\\\n")
        f.write("\\bottomrule\n\\end{longtable}\n")
    print("Tabla generada en", out)


def main():
    med = cargar(CSV)
    generar_tabla(CSV)
    if not med:
        return
    for esc in MEDIBLES:
        grafica_por_escenario(med, esc)
    grafica_inconsistencia(med)
    print("Figuras generadas en", IMG)


if __name__ == "__main__":
    main()
