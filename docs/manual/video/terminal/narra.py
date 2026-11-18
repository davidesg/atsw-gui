#!/usr/bin/env python3
"""LA VOZ DE LA TERMINAL, sobre la toma ya grabada:

    python3 narra.py TOMA.mp4 narracion.txt VOZ_DIR SALIDA.mp4 [--tapa Y]

TOMA es el video de graba.sh (la madre y la terminal juntas). Las frases de
narracion.txt ya estan sintetizadas en VOZ_DIR (voz.py, como las escenas).

Hace lo que escenas.c hace al grabar, pero despues: cada frase empieza cuando
acaba la anterior (mas 0.45 s), y la de un ancla («a SEGUNDO»), no antes de
ese segundo de la toma. Si la voz va por detras del ancla, la toma se congela
en ese cuadro hasta que la voz la alcanza. Los rotulos salen en un .ass con
el estilo de las escenas, y la voz se mezcla en su segundo.

--tapa Y pinta con el fondo del lienzo la franja de abajo desde Y: borra los
rotulos que la toma ya llevara quemados.

Avisa de cada silencio de mas de 1.5 s: es lo que hay que llenar con frases.
"""

import hashlib
import os
import subprocess
import sys

HUECO = 0.45            # entre frase y frase, como escenas.c
FONDO = "0x1e3a66"      # el lienzo de graba.sh

CABECERA = """[Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080
WrapStyle: 0

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Abajo,Sans,44,&H00FFFFFF,&H00FFFFFF,&H00000000,&H90201A10,0,0,0,0,100,100,0,0,3,14,0,2,120,120,40,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""


def hms(t):
    h, t = divmod(t, 3600)
    m, s = divmod(t, 60)
    return "%d:%02d:%05.2f" % (h, m, s)


def lee(narracion):
    """[(ancla, [rotulo...])...]"""
    trozos = []
    with open(narracion, encoding="utf-8") as f:
        for l in f:
            l = l.strip()
            if l.startswith("a "):
                trozos.append((float(l[2:]), []))
            elif l.startswith("rotulo "):
                if not trozos:
                    sys.exit("narra: un rotulo antes del primer ancla")
                trozos[-1][1].append(l[len("rotulo "):].strip())
    return trozos


def duracion(voz_dir, arg):
    sha = hashlib.sha1(arg.encode("utf-8")).hexdigest()
    with open(os.path.join(voz_dir, sha + ".ms")) as f:
        return int(f.read()) / 1000.0, os.path.join(voz_dir, sha + ".wav")


def main():
    a = sys.argv[1:]
    tapa = None
    if "--tapa" in a:
        i = a.index("--tapa")
        tapa = int(a[i + 1])
        del a[i:i + 2]
    if len(a) != 4:
        sys.exit(__doc__)
    toma, narracion, voz_dir, salida = a
    fin = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                                "-of", "csv=p=0", toma], capture_output=True, text=True,
                               check=True).stdout)

    # EL CALENDARIO. Tramos de la toma [desde, hasta) con lo que se congela
    # al final de cada uno; frases con su segundo en el video de salida.
    tramos, frases = [], []
    src = out = libre = 0.0
    for ancla, rotulos in lee(narracion):
        if ancla < src:
            sys.exit("narra: el ancla %g va antes que la anterior" % ancla)
        llega = out + (ancla - src)
        para = max(0.0, libre - llega)
        tramos.append((src, ancla, para))
        if para > 0:
            print("narra: en %g s de la toma se congela %.1f s" % (ancla, para))
        out = llega + para
        src = ancla
        t = out
        for r in rotulos:
            ms, wav = duracion(voz_dir, r)
            if t - libre > 1.5 and frases:
                print("narra: %.1f s de silencio antes de «%s»" % (t - libre, r[:50]))
            frases.append((t, ms, wav, r.split(" || ", 1)[0]))
            libre = t + ms + HUECO
            t = libre
    para = max(0.0, libre - (out + fin - src)) + 0.5
    tramos.append((src, fin, para))
    total = out + (fin - src) + para
    if total - libre > 1.5:
        print("narra: %.1f s de silencio al final" % (total - libre))

    # LOS ROTULOS: cada uno hasta que empieza el siguiente, o acaba su voz.
    ass = salida + ".ass"
    with open(ass, "w", encoding="utf-8") as f:
        f.write(CABECERA)
        for i, (t, ms, _, txt) in enumerate(frases):
            hasta = frases[i + 1][0] if i + 1 < len(frases) else min(total, t + ms + 1.0)
            f.write("Dialogue: 0,%s,%s,Abajo,,0,0,0,,%s\n" % (hms(t), hms(hasta), txt))

    # LA IMAGEN: los tramos, cada uno con su congelado, uno detras de otro.
    v = "[0:v]" + ("drawbox=x=0:y=%d:w=iw:h=ih-%d:color=%s:t=fill," % (tapa, tapa, FONDO) if tapa else "")
    v += "split=%d" % len(tramos) + "".join("[s%d]" % i for i in range(len(tramos))) + ";"
    for i, (d, h, p) in enumerate(tramos):
        v += "[s%d]trim=%.3f:%.3f,setpts=PTS-STARTPTS" % (i, d, h)
        if p > 0:
            v += ",tpad=stop_mode=clone:stop_duration=%.3f" % p
        v += "[t%d];" % i
    v += "".join("[t%d]" % i for i in range(len(tramos)))
    v += "concat=n=%d:v=1:a=0,ass=%s[v]" % (len(tramos), os.path.basename(ass))

    # EL SONIDO: cada frase en su segundo.
    ent, aud = [], ""
    for n, (t, _, wav, _) in enumerate(frases, 1):
        ent += ["-i", wav]
        aud += "[%d:a]aresample=48000,adelay=%d:all=1[a%d];" % (n, int(t * 1000), n)
    aud += "".join("[a%d]" % n for n in range(1, len(frases) + 1))
    aud += "amix=inputs=%d:normalize=0:dropout_transition=0,apad=whole_dur=%.3f,atrim=0:%.3f[a]" % (
        len(frases), total, total)

    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", os.path.abspath(toma)]
                   + [x if x == "-i" else os.path.abspath(x) for x in ent]
                   + ["-filter_complex", v + ";" + aud, "-map", "[v]", "-map", "[a]",
                      "-t", "%.3f" % total, "-c:v", "libx264", "-preset", "slow", "-crf", "22",
                      "-pix_fmt", "yuv420p", "-r", "25", "-c:a", "aac", "-b:a", "96k",
                      "-ar", "48000", "-ac", "1", os.path.abspath(salida)],
                   check=True, cwd=os.path.dirname(os.path.abspath(ass)))
    congelado = sum(p for _, _, p in tramos)
    print("narra: %s, %.0f s (la toma, %.0f s; congelado, %.0f s; %d frases)"
          % (salida, total, fin, congelado, len(frases)))


if __name__ == "__main__":
    main()
