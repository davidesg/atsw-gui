#!/usr/bin/env python3
"""comparar.py -- dos ficheros del formato univariante, leidos y comparados.

No compara bytes: compara el MODELO. Dos implementaciones del formato no van a
escribir los mismos bytes nunca -- espaciado, decimales, el orden de las lineas
de comentario -- y eso no importa. Lo que importa es que quien lo lea despues
vea lo mismo.

    comparar.py A.inp B.inp        dice en que se diferencian
    comparar.py --estructura A B   igual, pero sin los valores que A declaraba
                                   LIBRES -- para comparar un .inp con el .pre
                                   que sale de estimarlo
    comparar.py --campos A.inp     ensena lo que se lee de A

El lector es fue.load(), que es el unico lector del formato en Python y el que
usa toda la suite.
"""

import sys
import math
import re
import dataclasses

import fue

TOL = 1e-9

# Campos privados que SI son parte del fichero. fue.load() lee los dos
# dialectos y guarda el horizonte y la varianza de fuf en atributos con guion
# bajo; sin esto, un fichero de fuf que pierde su horizonte al reescribirlo
# pasaba la bateria como si nada.
VISIBLES = ("_fuf_horizon", "_fuf_sigma2")


def _es_secuencia(v):
    if isinstance(v, (list, tuple)):
        return True
    return hasattr(v, "shape") and hasattr(v, "tolist")     # un array de numpy


def _plano(prefijo, v, salida):
    """Aplana lo que devuelve fue.load() a pares nombre -> valor."""
    if v is None or isinstance(v, (int, float, str, bool)):
        salida[prefijo] = v
    elif _es_secuencia(v):
        if not isinstance(v, (list, tuple)):
            v = v.tolist()
        salida[prefijo + ".n"] = len(v)
        for i, x in enumerate(v):
            _plano("%s[%d]" % (prefijo, i), x, salida)
    elif isinstance(v, (list, tuple)):
        salida[prefijo + ".n"] = len(v)
        for i, x in enumerate(v):
            _plano("%s[%d]" % (prefijo, i), x, salida)
    elif dataclasses.is_dataclass(v):
        for f in dataclasses.fields(v):
            _plano("%s.%s" % (prefijo, f.name), getattr(v, f.name), salida)
    elif hasattr(v, "__dict__"):
        for k in sorted(vars(v)):
            if not k.startswith("_") or k in VISIBLES:
                _plano("%s.%s" % (prefijo, k), getattr(v, k), salida)
    else:
        salida[prefijo] = repr(v)


def campos(path):
    ts, m = fue.load(path)
    out = {}
    _plano("ts", ts, out)
    _plano("model", m, out)
    return out


# ---------------------------------------------------------------------------
# Modo estructura. Los actores que ESTIMAN -- el motor en C, y write_pre --
# producen un .pre cuyos valores son otros por definicion: son las
# estimaciones. Compararlos con los del .inp seria comparar la semilla con el
# optimo. Lo que si tiene que sobrevivir intacto es todo lo demas: la serie,
# las fechas, los tipos y nombres de los deterministas, los ordenes, las
# banderas, lambda, d, D, los ifadf.
#
# Y una cosa mas, que es la que hace que este modo valga la pena: un parametro
# FIJO no es una estimacion. Si la bandera dice 0, ese valor tiene que salir
# igual que entro. Por eso no se saltan los valores en bloque, sino solo los
# que el fichero de partida declaraba libres.

_VALOR = (
    (re.compile(r"^model\.(ar|ma|ar_s|ma_s)\[(\d+)\]\[(\d+)\]$"),
     lambda m: "model.%s_free[%s][%s]" % (m.group(1), m.group(2), m.group(3))),
    (re.compile(r"^model\.interventions\[(\d+)\]\.(omega|delta)\[(\d+)\]$"),
     lambda m: "model.interventions[%s].%s_free[%s]" % (m.group(1), m.group(2),
                                                        m.group(3))),
    (re.compile(r"^model\.(ar_f|ma_f)\[(\d+)\]\.coef$"),
     lambda m: "model.%s[%s].free" % (m.group(1), m.group(2))),
    (re.compile(r"^model\.mu0$"), lambda m: "model.estimate_mu"),
)


def es_estimable(clave, origen):
    """True si `clave` es un valor que el fichero de partida declaraba LIBRE."""
    for patron, bandera in _VALOR:
        m = patron.match(clave)
        if m:
            return bool(origen.get(bandera(m), False))
    return False


def igual(a, b):
    if isinstance(a, float) or isinstance(b, float):
        try:
            a, b = float(a), float(b)
        except (TypeError, ValueError):
            return a == b
        if math.isnan(a) and math.isnan(b):
            return True
        return abs(a - b) <= TOL * max(1.0, abs(a), abs(b))
    return a == b


def compara(pa, pb, callar_datos=True, estructura=False):
    a, b = campos(pa), campos(pb)
    difs = []
    for k in sorted(set(a) | set(b)):
        if callar_datos and (k.startswith("ts.data[") or
                             k.startswith("model.series.data[")):
            continue
        if estructura and es_estimable(k, a):
            continue
        va, vb = a.get(k, "<falta>"), b.get(k, "<falta>")
        if not igual(va, vb):
            difs.append((k, va, vb))
    # los datos, resumidos: si difieren, una linea, no mil
    if callar_datos:
        for donde in ("ts.data[", "model.series.data["):
            da = [v for k, v in sorted(a.items()) if k.startswith(donde)]
            db = [v for k, v in sorted(b.items()) if k.startswith(donde)]
            if len(da) != len(db):
                difs.append((donde[:-1] + " (cuantos)", len(da), len(db)))
            else:
                malos = [i for i, (x, y) in enumerate(zip(da, db)) if not igual(x, y)]
                if malos:
                    difs.append((donde[:-1] + " (cuantos distintos)", len(malos),
                                 "el primero en %d" % malos[0]))
    return difs


def main(argv):
    estructura = "--estructura" in argv
    argv = [x for x in argv if x != "--estructura"]
    if len(argv) >= 3 and argv[1] == "--campos":
        for k, v in sorted(campos(argv[2]).items()):
            if not (k.startswith("ts.data[") or k.startswith("model.series.data[")):
                print("%-40s %s" % (k, v))
        return 0
    if len(argv) < 3:
        print(__doc__)
        return 2
    difs = compara(argv[1], argv[2], estructura=estructura)
    if not difs:
        print("iguales")
        return 0
    print("%d diferencias" % len(difs))
    for k, va, vb in difs:
        print("  %-36s %-24s %s" % (k, va, vb))
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
