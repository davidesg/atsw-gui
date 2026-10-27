#!/usr/bin/env python3
"""EL INFORME DE LA REVISION: una fila por comprobacion, una columna por
plataforma, con OK, DIFF (con los valores) o FAIL.

    python3 informe.py DIR [--resumen FICHERO]

DIR tiene un subdirectorio por plataforma (resultado-linux, resultado-windows,
resultado-macos-arm64...), cada uno con lo que dejaron instalar.json,
motores.py (numeros.json), escenas.py (escenas.json) y motores.py --proyectos
(numeros-gui.json).

Las varas son las de docs/PRUEBAS.md §3: en Linux, la referencia exacta
(referencia-linux.json, sacada con el paquete v0.2.2 en Linux); fuera de
Linux, una estimacion vale si se aparta menos de 2e-3 relativo o del 2 % de
su error tipico, un error tipico si se aparta menos del 5 %, y un estadistico
(sigma, logL, Q, JB, LR, similitudes...) si se aparta menos de 2e-3 relativo.
Ademas, los numeros que citan los textos (CASO-ES-CORE.md, el manual, los
videos), con la precision con que los citan.

Escribe DIR/informe.md y DIR/informe.json. Sale con 1 si hay algun FAIL que no
sea un fallo ya conocido (CONOCIDOS).
"""
import argparse
import json
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
COLGADO = 30.0

# Los fallos ya apuntados: la casilla dice FAIL y su BUG, y no tumba el trabajo.
CONOCIDOS = {
    "gui/vistazo-sin-path": "BUG-0002 de la madre",
}

# Lo que dicen los textos, con su precision: (clave, valor, tolerancia, de donde)
CITADO = {
    "art-es-d1": [("art/es-core/D1/propuesto", "p1q0P0Q1", None, "art propone AR(1) x SMA(1)"),
                  ("art/es-core/D1/p3q0P0Q1/sim", 0.873, 0.0005, "AR(3) x SMA(1), similitud 0.873")],
    "art-es-d0": [("art/es-core/D0/mas_peso", "p1q1P1Q0", None, "AR(1)+MA(1) x SAR(1), casi todo el peso"),
                  ("art/es-core/D0/p1q1P1Q0/peso", 0.999, 0.0005, "peso 0.999")],
    "art-ipc": [("art/ipc-wti/ipc/propuesto", "p0q1P0Q0", None, "art propone MA(1)"),
                ("art/ipc-wti/ipc/p1q0P0Q0/sim", 0.947, 0.0005, "AR(1), similitud 0.947")],
    "art-wti": [("art/ipc-wti/wti-sin-armonicos/propuesto", "p1q0P0Q0", None, "AR(1)"),
                ("art/ipc-wti/wti-sin-armonicos/p1q0P0Q0/sim", 0.961, 0.0005, "similitud 0.961")],
    "fue-es-m01": [("fue/es-core/subyacente_m01/jb", 127.5, 0.05, "Jarque-Bera 127.5")],
    "fue-es-m03": [("fue/es-core/subyacente_m03/sigma", 0.00123, 0.000005, "sigma 0.123 %"),
                   ("fue/es-core/subyacente_m03/sma1_1", 0.583, 0.0005, "Theta 0.583"),
                   ("fue/es-core/subyacente_m03/Q12", 6.5, 0.05, "Q(8) = 6.5"),
                   ("fue/es-core/subyacente_m03/jb", 12.1, 0.05, "JB 12.1"),
                   ("fue/es-core/subyacente_m03/logelf", None, None, "")],
    "ganancia-m03": [("fue/es-core/subyacente_m03/ganancia_omega1", 0.0129, 0.00005, "omega(1) = 0.0129"),
                     ("fue/es-core/subyacente_m03/ganancia_omega1/et", 0.0016, 0.00005, "e.t. 0.0016"),
                     ("fue/es-core/subyacente_m03/ganancia_omega1/W", 61.6, 0.05, "W = 61.6")],
    "fue-es-m04": [("fue/es-core/subyacente_m04/mu", 0.00131, 0.000005, "mu = 0.00131"),
                   ("fue/es-core/subyacente_m04/t_mu", 4.0, 0.05, "t = 4.0"),
                   ("fue/es-core/subyacente_m04/ganancia_omega12", 0.01246, 0.000005, "ganancia 0.01246")],
    "media-madre": [("madre/t_media/es-core/subyacente", 2.6, 0.05, "ES_CORE: t = 2.6"),
                    ("madre/t_media/ipc-wti/ipc", 3.0, 0.05, "IPC: t = 3.0"),
                    ("madre/t_media/ipc-wti/wti", 0.6, 0.05, "WTI: t = 0.6")],
    "drtran-conjunta": [("drtran/conjunta/omega1[0]", 0.018, 0.0005, "omega0 = 0.018"),
                        ("drtran/conjunta/delta1[1]", 0.50, 0.005, "delta = 0.50"),
                        ("drtran/conjunta/ganancia", 0.037, 0.0005, "ganancia 0.037")],
    "drtran-lr": [("drtran/LR", 86.9, 0.05, "LR = 86.9")],
    "drtran-prevision": [("drtran/prevision/prev_ipc_1/2020", 97.14, 0.005, "enero de 2020: 97.14"),
                         ("drtran/prevision/prev_ipc_1/2020/et", 0.25, 0.005, "± 0.25")],
    "gui-media": [("gui/madre/media/es-core/subyacente_m04", "libre", None, "m04: mu libre"),
                  ("gui/madre/media/ipc-wti/ipc_m01", "libre", None, "IPC m01: mu libre"),
                  ("gui/madre/media/ipc-wti/wti_m01", "fija", None, "WTI m01: mu fija")],
}

# Las filas: (id, texto, prefijos de claves que compara con la referencia)
FILAS_MOTORES = [
    ("art-es-d1", "art: ES_CORE con λ=0, d=1, D=1", ["art/es-core/D1/"]),
    ("art-es-d0", "art: ES_CORE con D=0", ["art/es-core/D0/"]),
    ("art-ipc", "art: IPC con D=0", ["art/ipc-wti/ipc/"]),
    ("art-wti", "art: WTI con D=0 (y sin armónicos)", ["art/ipc-wti/wti/", "art/ipc-wti/wti-sin-armonicos/"]),
    ("fue-es-m01", "fue: ES_CORE m01, AR(3)×SMA(1)", ["fue/es-core/subyacente_m01/"]),
    ("fue-es-m02", "fue: ES_CORE m02, con el escalón 9/2012", ["fue/es-core/subyacente_m02/"]),
    ("fue-es-m03", "fue: ES_CORE m03, ω(B)=ω0−ω1B y 10/2007 (D=1)", ["fue/es-core/subyacente_m03/"]),
    ("ganancia-m03", "m03: ganancia del IVA ω(1), e.t. y W", ["fue/es-core/subyacente_m03/ganancia"]),
    ("fue-es-m04", "fue: ES_CORE m04, armónicos y μ (D=0)", ["fue/es-core/subyacente_m04/"]),
    ("fue-ipc-m01", "fue: IPC m01, AR(1) con armónicos y μ", ["fue/ipc-wti/ipc_m01/"]),
    ("fue-wti-m01", "fue: WTI m01, AR(1)", ["fue/ipc-wti/wti_m01/"]),
    ("fue-wti-m02", "fue: WTI m02, tres escalones 10/2008", ["fue/ipc-wti/wti_m02/"]),
    ("media-madre", "madre: t de la media (regla de derivar μ)", ["madre/t_media/"]),
    ("dictamen", "atsw-cli dictamen (la Diagnosis de la madre)", ["atsw-cli/dictamen/"]),
    ("drtran-baseline", "drtran: baseline diagonal (-0)", ["drtran/baseline/"]),
    ("drtran-conjunta", "drtran: conjunta b=0, r=1, s=0", ["drtran/conjunta/"]),
    ("drtran-lr", "drtran: LR transferencia frente a baseline", ["drtran/LR"]),
    ("drtran-prevision", "drtran: previsión a 8 (enero de 2020)", ["drtran/prevision/"]),
]
FILAS_GUI_NUM = [
    ("gui-fue-es", "GUI: los .out de fue del ES_CORE (m01–m04)", ["gui/fue/es-core/"]),
    ("gui-fue-ipcwti", "GUI: los .out de fue del IPC y el WTI", ["gui/fue/ipc-wti/"]),
    ("gui-drtran", "GUI: drtran_gui (baseline, conjunta, LR, previsión)", ["gui/drtran/"]),
    ("gui-media", "GUI: la madre deriva μ libre/fija", ["gui/madre/media/"]),
]


def carga(d, f):
    p = os.path.join(d, f)
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as g:
        return json.load(g)


def fmt(v):
    if isinstance(v, float):
        return "%.6g" % v
    return str(v)


def compara(a, ref, linux):
    """None si vale; si no, el porque."""
    va, vr, tipo = a["v"], ref["v"], ref["tipo"]
    if tipo == "tiempo":
        return None
    if tipo == "texto" or isinstance(vr, str) or isinstance(va, str):
        return None if va == vr else "%s ≠ %s" % (fmt(va), fmt(vr))
    if linux:
        return None if abs(va - vr) <= 1e-12 * max(1.0, abs(vr)) else "%s ≠ %s" % (fmt(va), fmt(vr))
    if tipo == "est":
        tol = max(2e-3 * abs(vr), 0.02 * ref.get("et", 0.0))
    elif tipo == "et":
        tol = 0.05 * abs(vr)
    else:
        tol = max(2e-3 * abs(vr), 1e-9)
    return None if abs(va - vr) <= tol else "%s ≠ %s" % (fmt(va), fmt(vr))


def fila_numeros(fid, prefijos, val, ref, linux, citado, mapa=None):
    """OK / DIFF / FAIL para las claves de esos prefijos frente a la
    referencia, y los numeros citados por los textos."""
    if val is None:
        return "FAIL", "sin resultados"
    claves = sorted(k for k in val if any(k.startswith(p) for p in prefijos))
    fallos = [k for k in claves if k.endswith("/ok") and val[k]["v"] != "si"]
    if fallos:
        return "FAIL", "no corrió: " + ", ".join(fallos[:3])
    if not claves:
        return "FAIL", "sin números"
    difs, n = [], 0
    for k in claves:
        kr = mapa(k) if mapa else k
        if kr not in ref:
            continue
        n += 1
        x = compara(val[k], ref[kr], linux)
        if x:
            difs.append("%s: %s" % (k.split("/", 2)[-1], x))
    # lo citado por los textos (siempre con su propia vara)
    for k, v, tol, que in citado.get(fid, []):
        if v is None:
            continue
        kk = k if not mapa else k
        if kk not in val:
            difs.append("%s: falta" % que)
            continue
        x = val[kk]["v"]
        if isinstance(v, str):
            if x != v:
                difs.append("texto «%s»: %s" % (que, fmt(x)))
        elif abs(x - v) > tol:
            difs.append("texto «%s»: %s" % (que, fmt(x)))
    if difs:
        return "DIFF", "; ".join(difs[:4]) + (" (y %d más)" % (len(difs) - 4) if len(difs) > 4 else "")
    return "OK", "%d números" % n


def main():
    a = argparse.ArgumentParser()
    a.add_argument("dir")
    a.add_argument("--resumen", help="donde escribir el Markdown (GITHUB_STEP_SUMMARY)")
    args = a.parse_args()
    with open(os.path.join(AQUI, "referencia-linux.json"), encoding="utf-8") as f:
        ref = json.load(f)["valores"]
    plats = sorted(x[len("resultado-"):] for x in os.listdir(args.dir)
                   if x.startswith("resultado-") and os.path.isdir(os.path.join(args.dir, x)))
    orden = {"linux": 0, "windows": 1}
    plats.sort(key=lambda p: (orden.get(p, 2), p))
    tabla = []          # (seccion, id, texto, {plat: (estado, detalle)})

    def pon(sec, fid, texto, celdas):
        tabla.append((sec, fid, texto, celdas))

    datos = {}
    for p in plats:
        d = os.path.join(args.dir, "resultado-" + p)
        datos[p] = {"inst": carga(d, "instalar.json"), "num": carga(d, "numeros.json"),
                    "gui": carga(d, "numeros-gui.json"), "esc": carga(d, "escenas.json")}

    # A. la instalacion
    c = {}
    for p in plats:
        i = datos[p]["inst"]
        if not i:
            c[p] = ("FAIL", "sin registro")
        else:
            c[p] = ("OK" if i.get("ok") else "FAIL", i.get("como", ""))
    pon("A. Instalación", "instalar", "Instala el paquete v0.2.2 publicado", c)

    # B. los motores
    for fid, texto, pref in FILAS_MOTORES:
        c = {}
        for p in plats:
            n = datos[p]["num"]
            c[p] = fila_numeros(fid, pref, n["valores"] if n else None, ref, p == "linux", CITADO)
        pon("B. Motores", fid, texto, c)
    # los tiempos
    c, c2 = {}, {}
    for p in plats:
        n = datos[p]["num"]
        if not n:
            c[p] = c2[p] = ("FAIL", "sin resultados")
            continue
        malas = [x for x in n["corridas"] if x["colgado"] or x["rc"] != 0]
        lenta = max(n["corridas"], key=lambda x: x["segundos"]) if n["corridas"] else None
        if malas:
            c[p] = ("FAIL", "; ".join("%s rc=%s%s" % (x["nombre"], x["rc"],
                                                      " COLGADO (matado, PID %s)" % x["pid"] if x["colgado"] else "")
                                      for x in malas[:4]))
        else:
            c[p] = ("OK", "%d corridas; la más lenta %s, %.2f s" % (len(n["corridas"]), lenta["nombre"], lenta["segundos"]))
        t = n["valores"].get("tiempo/drtran/conjunta", {}).get("v")
        c2[p] = ("FAIL", "no corrió") if t is None else (("OK" if t < 1.0 else "DIFF"), "%.3f s" % t)
    pon("B. Motores", "corridas", "Todas las corridas acaban (≤ 30 s), con rc = 0", c)
    pon("B. Motores", "tiempo-drtran", "drtran conjunta en menos de 1 s", c2)

    # C. los GUIs: las escenas
    escenas = []
    for p in plats:
        for e in (datos[p]["esc"] or []):
            k = "%s/%s" % (e["caso"], e["escena"])
            if k not in escenas:
                escenas.append(k)
    for k in escenas:
        c = {}
        for p in plats:
            es = [e for e in (datos[p]["esc"] or []) if "%s/%s" % (e["caso"], e["escena"]) == k]
            if not es:
                c[p] = ("—", "no se corrió")
                continue
            e = es[0]
            fid = "gui/" + e["escena"] if e["caso"] == "extra" else None
            det = "%.0f s, %d órdenes, %d fotos" % (e["segundos"], e["ordenes"], len(e["fotos"]))
            if e["ok"]:
                c[p] = ("OK", det + (", %d avisos" % len(e["avisos"]) if e["avisos"] else ""))
            else:
                por = e["fallo_guion"] or ("colgada: matada por su PID %s" % e["pid"] if e["colgado"]
                                           else "rc=%s" % e["rc"])
                if e.get("consolas"):
                    por += "; CONSOLAS: " + ", ".join("%s (%s)" % (x["programa"], x["clase"]) for x in e["consolas"])
                conocido = CONOCIDOS.get(fid) if fid else None
                c[p] = ("FAIL", (conocido + ": " if conocido else "") + por + "; " + det)
        pon("C. GUIs (escenas)", "escena:" + k, "Escena " + k, c)
    # las consolas de Windows, juntas
    c = {}
    for p in plats:
        if p != "windows":
            c[p] = ("—", "solo Windows")
            continue
        vistas = [(e["escena"], x) for e in (datos[p]["esc"] or []) for x in e.get("consolas", [])]
        c[p] = ("OK", "ninguna ventana ConsoleWindowClass") if not vistas else \
               ("FAIL", "; ".join("%s: %s" % (s, x["programa"]) for s, x in vistas[:5]))
    pon("C. GUIs (escenas)", "consolas", "Windows: ninguna consola asoma durante las escenas", c)
    # los avisos de stderr
    c = {}
    for p in plats:
        es = datos[p]["esc"] or []
        n = sum(len(e["avisos"]) for e in es)
        ej = next((x for e in es for x in e["avisos"]), "")
        c[p] = ("OK", "ninguno") if n == 0 else ("AVISO", "%d; p. ej. %s" % (n, ej[:120]))
    pon("C. GUIs (escenas)", "avisos", "Avisos y errores en stderr de los GUIs", c)
    # los numeros que dejaron los GUIs
    mapa = lambda k: k[4:]      # gui/fue/... -> fue/...
    for fid, texto, pref in FILAS_GUI_NUM:
        c = {}
        for p in plats:
            g = datos[p]["gui"]
            c[p] = fila_numeros(fid, pref, g["valores"] if g else None, ref, p == "linux", CITADO, mapa)
        pon("C. GUIs (escenas)", fid, texto, c)

    # el Markdown
    sim = {"OK": "✅ OK", "DIFF": "🟡 DIFF", "FAIL": "❌ FAIL", "AVISO": "🟠 AVISO", "—": "—"}
    md = ["# Revisión de ATSW v0.2.2 con los casos de los vídeos", "",
          "Una fila por comprobación y una columna por plataforma. En Linux, la referencia "
          "exacta; fuera de Linux, las varas de `docs/PRUEBAS.md` §3. Los números que citan "
          "los textos se miden con la precisión con que se citan.", ""]
    sec = None
    for s, fid, texto, celdas in tabla:
        if s != sec:
            md += ["", "## " + s, "", "| comprobación | " + " | ".join(plats) + " |",
                   "|---|" + "---|" * len(plats)]
            sec = s
        md.append("| %s | %s |" % (texto, " | ".join(
            "%s<br><sub>%s</sub>" % (sim.get(celdas[p][0], celdas[p][0]), celdas[p][1].replace("|", "/"))
            for p in plats)))
    txt = "\n".join(md) + "\n"
    with open(os.path.join(args.dir, "informe.md"), "w", encoding="utf-8") as f:
        f.write(txt)
    with open(os.path.join(args.dir, "informe.json"), "w", encoding="utf-8") as f:
        json.dump([{"seccion": s, "id": fid, "texto": t, "celdas": c} for s, fid, t, c in tabla],
                  f, ensure_ascii=False, indent=1)
    if args.resumen:
        with open(args.resumen, "a", encoding="utf-8") as f:
            f.write(txt)
    print(txt)
    malos = [(fid, p) for s, fid, t, c in tabla for p in plats
             if c[p][0] == "FAIL" and not any(v in c[p][1] for v in CONOCIDOS.values())]
    if malos:
        print("FAIL sin fallo conocido: %s" % malos)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
