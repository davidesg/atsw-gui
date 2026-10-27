#!/usr/bin/env python3
"""LOS MOTORES, con la secuencia de modelos de los dos videos.

    python3 motores.py --bin DIR --salida OUT

Con los programas instalados de DIR, sobre las especificaciones que dejan los
GUIs en las escenas de los videos (casos/: los .inp tal cual los escribieron
la madre y fue_gui, con sus semillas, y la red del caso de drtran):

  art     la identificacion de cada video: el IPC subyacente con D = 1 y con
          D = 0, y el IPC y el WTI con D = 0, con las mismas opciones que
          pasa la madre (lib/analisis/an_identifica.c);
  fue     m01..m04 del IPC subyacente; m01 del IPC; m01 y m02 del WTI;
  atsw-cli  el dictamen de la madre (la ventana Diagnosis) sobre esos .out;
  drtran  el caso IPC <- WTI con la red del manual (b=0, r=1, s=0): el
          baseline diagonal (-0), la estimacion conjunta y la prevision a 8;
  la regla de la media de la madre (media_libre), que atsw-cli no expone: su
          t se calcula aqui con la misma cuenta, y en las escenas se mira si
          el .inp derivado la deja libre.

Cada corrida tiene 30 s: la que pasa se da por colgada, se mata por su PID y
se apunta. Deja OUT/numeros.json (los numeros con nombre y su tipo, para
informe.py), los .out en OUT/trabajo y un registro de cada corrida.
"""
import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
import threading
import time

import numeros

AQUI = os.path.dirname(os.path.abspath(__file__))
R = os.path.dirname(os.path.dirname(AQUI))
DATOS = os.path.join(R, "docs", "manual", "datos")
WIN = sys.platform.startswith("win")
COLGADO = 30


class Motores:
    def __init__(self, bindir, salida):
        self.bin = bindir
        self.sal = salida
        self.trabajo = os.path.join(salida, "trabajo")
        self.corridas = []
        self.v = {}            # clave -> {"v": valor, "tipo": est|et|stat|tiempo|texto, "et": ...}

    def exe(self, p):
        return os.path.join(self.bin, p + (".exe" if WIN else ""))

    def corre(self, nombre, prog, args, cwd):
        """Una corrida con su limite; se mata por su PID si se pasa."""
        argv = [self.exe(prog)] + args
        log = os.path.join(self.sal, "logs", nombre.replace("/", "_") + ".txt")
        os.makedirs(os.path.dirname(log), exist_ok=True)
        kw = {"creationflags": 0x08000000} if WIN else {"start_new_session": True}
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            f.write("$ %s\n(en %s)\n\n" % (" ".join(argv), cwd))
            f.flush()
            t0 = time.perf_counter()
            p = subprocess.Popen(argv, cwd=cwd, stdout=f, stderr=subprocess.STDOUT,
                                 stdin=subprocess.DEVNULL, **kw)
            colgado = []

            def mata():
                colgado.append(1)
                if WIN:
                    subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)], capture_output=True)
                else:
                    p.kill()
            reloj = threading.Timer(COLGADO, mata)
            reloj.start()
            p.wait()
            t = time.perf_counter() - t0
            reloj.cancel()
            colgado = bool(colgado)
        c = {"nombre": nombre, "argv": [prog] + args, "rc": p.returncode,
             "segundos": round(t, 3), "colgado": colgado, "pid": p.pid}
        self.corridas.append(c)
        self.v["tiempo/" + nombre] = {"v": round(t, 3), "tipo": "tiempo"}
        print("%-28s rc=%-4s %7.3f s%s" % (nombre, p.returncode, t,
                                           "  COLGADO: matado por su PID %d" % p.pid if colgado else ""),
              flush=True)
        return c

    def pon(self, clave, v, tipo, et=None):
        if v is None:
            return
        self.v[clave] = {"v": v, "tipo": tipo}
        if et is not None:
            self.v[clave]["et"] = et

    # ---- art --------------------------------------------------------------

    def art(self):
        d = os.path.join(self.trabajo, "art")
        os.makedirs(d, exist_ok=True)
        series = {"subyacente": ("es_core.csv", "subyacente"), "ipc": ("ipc_wti.csv", "ipc"),
                  "wti": ("ipc_wti.csv", "wti")}
        corridas = [("es-core/D1", "subyacente", 1, False), ("es-core/D0", "subyacente", 0, False),
                    ("ipc-wti/ipc", "ipc", 0, False), ("ipc-wti/wti", "wti", 0, False),
                    # la madre vuelve a identificar el WTI sin armonicos: el F
                    # de art no le ve estacionalidad (an_identifica.c, corre_art)
                    ("ipc-wti/wti-sin-armonicos", "wti", 0, True)]
        for nombre, s, D, off in corridas:
            fich, col = series[s]
            x = self.serie(fich, col)
            base = nombre.replace("/", "_")
            with open(os.path.join(d, base + ".txt"), "w", newline="\n") as f:
                f.write("# freq 12\n# start 1/2002\n" + "".join("%.10g\n" % v for v in x))
            a = [base + ".txt", "-s", "12", "-l", "-d", "1", "-D", str(D)]
            if off:
                a += ["--harmonics", "off"]
            c = self.corre("art/" + nombre, "art", a, d)
            cand = os.path.join(d, base + "_art.cand")
            if c["rc"] != 0 or not os.path.exists(cand):
                self.pon("art/%s/ok" % nombre, "no", "texto")
                continue
            cs = numeros.art_cand(cand)
            prop = [k for k, v in cs.items() if v["propuesto"]]
            self.pon("art/%s/propuesto" % nombre, prop[0] if prop else "-", "texto")
            if cs:
                self.pon("art/%s/mas_peso" % nombre, max(cs, key=lambda k: cs[k]["peso"]), "texto")
            self.pon("art/%s/candidatos" % nombre, len(cs), "texto")
            for k, v in cs.items():
                self.pon("art/%s/%s/sim" % (nombre, k), v["sim"], "stat")
                self.pon("art/%s/%s/peso" % (nombre, k), v["peso"], "peso")

    @staticmethod
    def serie(fich, col, desde="2002-01", hasta="2019-12"):
        with open(os.path.join(DATOS, fich), encoding="utf-8") as f:
            return [float(r[col]) for r in csv.DictReader(f) if desde <= r["date"][:7] <= hasta]

    # ---- fue ------------------------------------------------------------

    def fue(self):
        for caso, modelos in (("es-core", ["subyacente_m01", "subyacente_m02", "subyacente_m03",
                                           "subyacente_m04"]),
                              ("ipc-wti", ["ipc_m01", "wti_m01", "wti_m02"])):
            d = os.path.join(self.trabajo, caso)
            for m in modelos:
                c = self.corre("fue/%s/%s" % (caso, m), "fue", [m], d)
                out = os.path.join(d, m + ".out")
                if c["rc"] != 0 or not os.path.exists(out):
                    self.pon("fue/%s/%s/ok" % (caso, m), "no", "texto")
                    continue
                self.lee_fue("fue/%s/%s" % (caso, m), out)

    def lee_fue(self, pre, out):
        r = numeros.fue_out(out)
        self.pon(pre + "/convergio", "si" if r["convergio"] else "no", "texto")
        for p in r["par"]:
            self.pon(pre + "/" + p["nombre"], p["valor"], "est", p["et"])
            self.pon(pre + "/" + p["nombre"] + "/et", p["et"], "et")
        for k in ("sigma", "logelf", "jb"):
            self.pon(pre + "/" + k, r[k], "stat")
        if r["media_res"] is not None and r["et_media_res"]:
            self.pon(pre + "/t_media_res", r["media_res"] / r["et_media_res"], "t")
        for q in r["q"]:
            self.pon(pre + "/Q%d" % q["k"], q["q"], "stat")
        for v, g in r["ganancias"].items():
            self.pon(pre + "/ganancia_" + v, g["valor"], "est", g["et"])
            self.pon(pre + "/ganancia_" + v + "/et", g["et"], "et")
            self.pon(pre + "/ganancia_" + v + "/W", g["w"], "t")
        mu = [p for p in r["par"] if p["nombre"] == "mu"]
        if mu:
            self.pon(pre + "/t_mu", mu[0]["valor"] / mu[0]["et"], "t")

    # ---- la madre, sin ventana: atsw-cli dictamen ----------------------

    def dictamen(self):
        """Un proyecto minimo con los .out de fue (como la bateria de atsw-cli)
        y el dictamen de cada modelo: la ventana Diagnosis, linea a linea."""
        p = os.path.join(self.trabajo, "proyecto")
        modelos = [("es-core", "subyacente", "m03"), ("es-core", "subyacente", "m04"),
                   ("ipc-wti", "ipc", "m01"), ("ipc-wti", "wti", "m02")]
        ym = ["schema_version: 2", "id: revision", 'titulo: "revision"', 'creado: "2026-10-27"',
              'analista: ""', "herramienta: fue", "raiz: .", "", "series:"]
        for s in sorted(set(m[1] for m in modelos)):
            ym.append("  %s:" % s)
        ym += ["", "modelos:"]
        for caso, s, m in modelos:
            w = os.path.join(p, s, "work")
            os.makedirs(w, exist_ok=True)
            for ext in (".out", ".pre"):
                src = os.path.join(self.trabajo, caso, "%s_%s%s" % (s, m, ext))
                if os.path.exists(src):
                    shutil.copy(src, os.path.join(w, "%s_%s%s" % (s, m, ext)))
            ym += ["  %s/%s:" % (s, m), "    version: %d" % int(m[1:]), '    padre: ""', '    razon: "revision"']
        y = os.path.join(p, "proyecto.yaml")
        with open(y, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(ym) + "\n")
        for caso, s, m in modelos:
            nombre = "atsw-cli/dictamen/%s/%s" % (s, m)
            c = self.corre(nombre, "atsw-cli", ["--json", "dictamen", y, s, m], p)
            log = os.path.join(self.sal, "logs", nombre.replace("/", "_") + ".txt")
            with open(log, encoding="utf-8", errors="replace") as f:
                txt = f.read()
            try:
                j = json.loads(txt[txt.index("{"):])
            except Exception:
                self.pon(nombre + "/ok", "no", "texto")
                continue
            for ln in j.get("lineas", []):
                self.pon("%s/%s" % (nombre, ln["titulo"]), ln["estado"], "texto")

    # ---- drtran -----------------------------------------------------------

    def drtran(self):
        d = os.path.join(self.trabajo, "ipc-wti")
        pres = ["ipc_m01.pre", "wti_m02.pre"]
        if not all(os.path.exists(os.path.join(d, x)) for x in pres):
            self.pon("drtran/ok", "no: faltan los .pre de fue", "texto")
            return
        runs = [("baseline", ["-o", "C1_base.out", "-e", "C1_base_res.txt", "-0"]),
                ("conjunta", ["-o", "C1_tf.out", "-n", "C1.dag", "-e", "C1_tf_res.txt"]),
                ("prevision", ["-o", "C1_prev.out", "-n", "C1.dag", "-e", "C1_prev_res.txt", "-f", "8"])]
        res = {}
        for nombre, a in runs:
            c = self.corre("drtran/" + nombre, "drtran", pres + a, d)
            out = os.path.join(d, a[1])
            if c["rc"] != 0 or not os.path.exists(out):
                self.pon("drtran/%s/ok" % nombre, "no", "texto")
                continue
            r = res[nombre] = numeros.drtran_out(out)
            pre = "drtran/" + nombre
            self.pon(pre + "/convergio", "si" if r["convergio"] else "no", "texto")
            self.pon(pre + "/loglik", r["loglik"], "stat")
            for k, p in r["par"].items():
                self.pon(pre + "/" + k, p["valor"], "est", p["et"])
                self.pon(pre + "/" + k + "/et", p["et"], "et")
            if "ganancia" in r:
                self.pon(pre + "/ganancia", r["ganancia"]["valor"], "est", r["ganancia"]["et"])
                self.pon(pre + "/ganancia/et", r["ganancia"]["et"], "et")
            if "retardo_medio" in r:
                self.pon(pre + "/retardo_medio", r["retardo_medio"]["valor"], "est", r["retardo_medio"]["et"])
            if "q_ruido" in r:
                self.pon(pre + "/Q_ruido", r["q_ruido"]["q"], "stat")
            for s, f in r["prevision"].items():
                self.pon("%s/prev_%s_%s" % (pre, s, f["fecha"]), f["valor"], "est", f["et"])
                self.pon("%s/prev_%s_%s/et" % (pre, s, f["fecha"]), f["et"], "et")
        if "baseline" in res and "conjunta" in res and res["baseline"]["loglik"] and res["conjunta"]["loglik"]:
            self.pon("drtran/LR", 2 * (res["conjunta"]["loglik"] - res["baseline"]["loglik"]), "stat")

    # ---- lo que dejaron los GUIs en los proyectos de las escenas -------

    def proyectos(self, base):
        """Los mismos numeros, de los ficheros que escribieron los GUIs al
        correr las escenas (claves gui/...): lo que el usuario ve."""
        ficheros = [("es-core", "es-core/subyacente/2002-2019/work", m)
                    for m in ("subyacente_m01", "subyacente_m02", "subyacente_m03", "subyacente_m04")]
        ficheros += [("ipc-wti", "ipc-wti/ipc/Pre-Covid/work", "ipc_m01"),
                     ("ipc-wti", "ipc-wti/wti/Pre-Covid/work", "wti_m01"),
                     ("ipc-wti", "ipc-wti/wti/Pre-Covid/work", "wti_m02")]
        for caso, d, m in ficheros:
            out = os.path.join(base, d, m + ".out")
            if os.path.exists(out):
                self.lee_fue("gui/fue/%s/%s" % (caso, m), out)
            else:
                self.pon("gui/fue/%s/%s/ok" % (caso, m), "no", "texto")
        # la regla de la media, en los .inp que derivo la madre
        for clave, inp in (("es-core/subyacente_m04", "es-core/subyacente/2002-2019/work/subyacente_m04.inp"),
                           ("ipc-wti/ipc_m01", "ipc-wti/ipc/Pre-Covid/work/ipc_m01.inp"),
                           ("ipc-wti/wti_m01", "ipc-wti/wti/Pre-Covid/work/wti_m01.inp")):
            f = os.path.join(base, inp)
            if os.path.exists(f):
                _, libre = numeros.inp_media(f)
                self.pon("gui/madre/media/" + clave, "libre" if libre else "fija", "texto")
        # drtran: el baseline es la corrida sin red; la conjunta, la ultima con
        # red que no es la de la prevision
        w = os.path.join(base, "ipc-wti", "_casos", "C1", "work")
        res = {}
        if os.path.isdir(w):
            for f in sorted(os.listdir(w)):
                if not f.endswith(".out"):
                    continue
                with open(os.path.join(w, f), encoding="latin-1") as g:
                    sin_red = "Transfer network" not in g.read()
                tipo = "prevision" if f.endswith("_prev.out") else "baseline" if sin_red else "conjunta"
                res[tipo] = numeros.drtran_out(os.path.join(w, f))
        for tipo, r in res.items():
            pre = "gui/drtran/" + tipo
            self.pon(pre + "/loglik", r["loglik"], "stat")
            for k in ("omega1[0]", "delta1[1]"):
                if k in r["par"]:
                    self.pon(pre + "/" + k, r["par"][k]["valor"], "est", r["par"][k]["et"])
            if "ganancia" in r:
                self.pon(pre + "/ganancia", r["ganancia"]["valor"], "est", r["ganancia"]["et"])
            for s, f in r["prevision"].items():
                self.pon("%s/prev_%s_%s" % (pre, s, f["fecha"]), f["valor"], "est", f["et"])
                self.pon("%s/prev_%s_%s/et" % (pre, s, f["fecha"]), f["et"], "et")
        if "baseline" in res and "conjunta" in res:
            self.pon("gui/drtran/LR", 2 * (res["conjunta"]["loglik"] - res["baseline"]["loglik"]), "stat")
        # los tiempos de cada corrida, de costes.csv (lib/coste: los apunta la madre)
        for caso in ("es-core", "ipc-wti"):
            c = os.path.join(base, caso, "costes.csv")
            if not os.path.exists(c):
                continue
            with open(c, encoding="utf-8") as g:
                for fila in csv.DictReader(g):
                    try:
                        t = float(fila["pared_s"])
                    except (KeyError, ValueError):
                        continue
                    self.pon("gui/tiempo/%s/%s/%s" % (fila["motor"].split()[0], fila["objeto"], fila["paso"]),
                             t, "tiempo")

    # ---- la regla de la media de la madre ------------------------------

    def media(self):
        for clave, fich, col in (("es-core/subyacente", "es_core.csv", "subyacente"),
                                 ("ipc-wti/ipc", "ipc_wti.csv", "ipc"),
                                 ("ipc-wti/wti", "ipc_wti.csv", "wti")):
            t = numeros.t_media(self.serie(fich, col), 0, 1, 0, 12)
            self.pon("madre/t_media/" + clave, t, "t")


def main():
    a = argparse.ArgumentParser()
    a.add_argument("--bin", required=True)
    a.add_argument("--salida", required=True)
    a.add_argument("--proyectos", help="solo leer los proyectos de las escenas (escenas.py) y salir")
    args = a.parse_args()
    m = Motores(os.path.abspath(args.bin), os.path.abspath(args.salida))
    if args.proyectos:
        m.proyectos(os.path.abspath(args.proyectos))
        with open(os.path.join(m.sal, "numeros-gui.json"), "w", encoding="utf-8") as f:
            json.dump({"valores": m.v}, f, ensure_ascii=False, indent=1, sort_keys=True)
        print("numeros de los GUIs: %d" % len(m.v))
        return 0
    if os.path.isdir(m.trabajo):
        shutil.rmtree(m.trabajo)
    shutil.copytree(os.path.join(AQUI, "casos"), m.trabajo)
    m.art()
    m.fue()
    m.dictamen()
    m.drtran()
    m.media()
    with open(os.path.join(m.sal, "numeros.json"), "w", encoding="utf-8") as f:
        json.dump({"valores": m.v, "corridas": m.corridas}, f, ensure_ascii=False, indent=1, sort_keys=True)
    malas = [c for c in m.corridas if c["rc"] != 0 or c["colgado"]]
    print("motores: %d corridas, %d mal" % (len(m.corridas), len(malas)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
