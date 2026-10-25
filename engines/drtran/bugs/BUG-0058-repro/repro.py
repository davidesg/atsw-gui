#!/usr/bin/env python3
"""BUG-0058 (drtran): con una media libre, drtran puede no acabar NUNCA.

La busqueda lineal prueba un paso enorme (el primer paso de cuasi-Newton, con
maxstep ~ 7000) y el parametro log(var_2/var_1) de la Q (tran_shootx.c, paso
5: var[i] = exp(x[idx++])) pasa de ~7 a ~2000-3000. exp() da inf, la Q lleva
inf y elf (elfvarma.c:77) la factoriza con choldcp (nlatools.c:131). Ahi
*d1 = inf y el reescalado del determinante

    while ( fabs( *d1 ) >= 1.0 ) { *d1 *= 0.0625; *d2 += 4.0; }   (:200-204)

no termina: inf * 0.0625 = inf. No hay tope de iteraciones que valga: el
bucle esta DENTRO de una sola evaluacion del objetivo. objcfunc ya rechaza un
objetivo no finito (drvmlest.c:332), pero aqui nunca se llega a tenerlo.

Los datos: el caso IPC <- WTI del manual (v0.2.0), con los .pre tal como los
dejan las escenas (docs/manual/capturas): IPC con mu libre (0.001545), WTI
con mu fija en 0 (wti_m02.pre) o libre (wti_m02_mu.pre, la regla intermedia
del 2026-10-24, ya retirada).

  A  estimacion conjunta con una sola omega (b=0 r=0 s=0, ganancia.dag):
     la que el analista estima al quitar la omega del retardo 1 que propone
     la CCF (b=0 r=0 s=1), o la del manual (r=1) sin el denominador.
  B  el baseline (-0, el modelo diagonal de «Calcular baseline») con la mu
     del WTI libre.

Los controles son los mismos con ipc_m01_sinmu.pre (la mu del IPC fija en 0,
flag 0): acaban en menos de un segundo, y en 162 combinaciones de (b, r, s)
y .pre del WTI ninguna cuelga sin la mu del IPC. Con ella cuelgan las de
r = s = 0 (b = 0, 1, 2; con wti_m01.pre tambien b = 3) y el baseline con la
mu del WTI libre. Fijar las medias en su valor del .pre (-M) NO lo evita (el
baseline -0 -M cuelga tambien con wti_m02.pre): lo que importa es arrancar
con la mu del IPC distinta de 0. Lo normal es menos de un segundo: el caso
del manual (r=1) tarda 0.2-0.9 s.

wti_m02_mu.pre es wti_m02.pre con el flag de mu a 1 (valor 0): no es un
optimo de fue, es un punto de partida como el que vio el baseline con la
regla intermedia.

Sale con 1 mientras alguno no acabe en LIMITE segundos (60 por defecto), con
0 cuando acaben todos.
"""
import os, pathlib, subprocess, sys, tempfile, time

AQUI = pathlib.Path(__file__).resolve().parent
DRTRAN = AQUI.parents[1] / "bin/drtran"
LIMITE = float(os.environ.get("LIMITE", "60"))

CASOS = [
    ("A: conjunta b=0 r=0 s=0 (mu del IPC libre)",
     ["ipc_m01.pre", "wti_m02.pre", "-n", "ganancia.dag"], True),
    ("A con el IPC sin mu (control)",
     ["ipc_m01_sinmu.pre", "wti_m02.pre", "-n", "ganancia.dag"], False),
    ("B: baseline -0 (mu del IPC y del WTI libres)",
     ["ipc_m01.pre", "wti_m02_mu.pre", "-0"], True),
    ("B con el IPC sin mu (control)",
     ["ipc_m01_sinmu.pre", "wti_m02_mu.pre", "-0"], False),
]


def corre(args, t):
    out = pathlib.Path(t) / "r.out"
    res = pathlib.Path(t) / "r_res.txt"
    a = [str(AQUI / x) if x.endswith((".pre", ".dag")) else x for x in args]
    t0 = time.monotonic()
    try:
        p = subprocess.run([str(DRTRAN), *a, "-o", str(out), "-e", str(res)],
                           cwd=t, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=LIMITE)
        return p.returncode, time.monotonic() - t0
    except subprocess.TimeoutExpired:          # subprocess lo mata (SIGKILL)
        return None, time.monotonic() - t0


def main():
    if not DRTRAN.exists():
        sys.exit(f"no encuentro {DRTRAN}: make en engines/drtran")
    cuelga = 0
    with tempfile.TemporaryDirectory() as t:
        for nombre, args, puede_colgar in CASOS:
            rc, s = corre(args, t)
            if rc is None:
                print(f"  {nombre}: NO ACABA (parado a los {s:.0f} s)")
                cuelga += 1
            else:
                print(f"  {nombre}: acaba con {rc} en {s:.2f} s")
                if not puede_colgar and rc not in (0, 3):
                    sys.exit(f"el control fallo con {rc}: la prueba no vale")
    if cuelga:
        print(f"ABIERTO: {cuelga} corrida(s) de drtran no acaban "
              "(choldcp con la Q infinita)")
        return 1
    print("cerrado")
    return 0


if __name__ == "__main__":
    sys.exit(main())
