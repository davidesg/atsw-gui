#!/usr/bin/env python3
"""LA VOZ DEL VIDEO: cada rotulo de los guiones, sintetizado con Piper.

    python3 docs/manual/video/voz.py DIR_SALIDA guion.txt [guion.txt...]

Para cada «rotulo TEXTO» deja DIR_SALIDA/<sha1 de TEXTO>.wav y su duracion
en milisegundos en <sha1>.ms: es lo que escenas.c busca al grabar (VOZ_DIR)
para esperar a que acabe cada frase. El sha1 es del argumento tal cual esta
en el guion, asi que cambiar un rotulo rehace solo su frase.

Lo que se dice sale del rotulo, leido para el oido: las siglas deletreadas,
«d = 1» como «de igual a uno», los decimales con coma. Si un rotulo necesita
decir otra cosa, el guion lo pone detras de « || ».

Necesita Piper (pip install piper-tts) y una voz en espanol:
    PIPER=/ruta/a/piper   (si no esta en el PATH)
    VOZ_MODELO=/ruta/a/es_ES-sharvard-medium.onnx  (la del video)
"""

import hashlib
import os
import re
import subprocess
import sys
import wave

# Las siglas y los nombres, como se pronuncian. Por palabras enteras.
SIGLAS = {
    "ATSW": "a te ese uve doble",
    "IPC": "i pe ce", "ipc": "i pe ce",
    "WTI": "uve doble te i", "wti": "uve doble te i",
    "ACF": "a ce efe", "PACF": "pe a ce efe", "CCF": "ce ce efe",
    "LR": "ele erre",
    "drtran": "de erre tran",
    "fug": "fug",
    "MANUAL.md": "el manual",
}

# Los modelos y los operadores. En orden: lo largo antes.
REGLAS = [
    (r"«|»", ""),
    (r"\bAR\((\d)\)", r"a erre \1"),
    (r"\bMA\((\d)\)", r"eme a \1"),
    (r"\bm0(\d)\b", r"eme cero \1"),
    (r"\.pre\b", " punto pre"),
    (r"\bb r s\b", "be erre ese"),
    (r"λ", "lambda"),
    (r"ω₀", "omega cero"),
    (r"δ", "delta"),
    (r"σ", "sigma"),
    (r"±", "más menos"),
    (r"\bD = ", "de estacional igual a "),
    (r"\bd = ", "de igual a "),
    (r"\bb = ", "be igual a "),
    (r"\br = ", "erre igual a "),
    (r"\bs = ", "ese igual a "),
    (r"\bk = ", "ka igual a "),
    (r"\br\b", "erre"),
    (r" = ", " igual a "),
    (r"(\d)\.(\d)", r"\1 coma \2"),
    (r"\s*%", " por ciento"),
    (r"…", ""),
]


def para_el_oido(txt):
    if " || " in txt:
        return txt.split(" || ", 1)[1]
    for a, b in REGLAS:
        txt = re.sub(a, b, txt)
    for k, v in SIGLAS.items():
        txt = re.sub(r"(?<![\w.])" + re.escape(k) + r"(?![\w])", v, txt)
    return txt


def rotulos(guiones):
    for g in guiones:
        with open(g, encoding="utf-8") as f:
            for l in f:
                l = l.strip()
                if l.startswith("rotulo "):
                    arg = l[len("rotulo "):].strip()
                    if arg and arg != "-":
                        yield arg


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    sal = sys.argv[1]
    os.makedirs(sal, exist_ok=True)
    piper = os.environ.get("PIPER", "piper")
    modelo = os.environ.get("VOZ_MODELO")
    if not modelo:
        sys.exit("falta VOZ_MODELO: la voz de Piper (.onnx)")
    hechos = 0
    for arg in dict.fromkeys(rotulos(sys.argv[2:])):
        sha = hashlib.sha1(arg.encode("utf-8")).hexdigest()
        wav = os.path.join(sal, sha + ".wav")
        if not os.path.exists(wav):
            dice = para_el_oido(arg)
            subprocess.run([piper, "-m", modelo, "-f", wav], input=dice.encode("utf-8"),
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            print("voz:", dice)
            hechos += 1
        with wave.open(wav) as w:
            ms = int(1000 * w.getnframes() / w.getframerate())
        with open(os.path.join(sal, sha + ".ms"), "w") as f:
            f.write("%d\n" % ms)
    print("voz: %d frases nuevas en %s" % (hechos, sal))


if __name__ == "__main__":
    main()
