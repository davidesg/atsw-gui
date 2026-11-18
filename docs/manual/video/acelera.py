#!/usr/bin/env python3
"""SIN PAUSAS: acelera lo que pasa entre frase y frase.

    python3 acelera.py TROZO.mp4 TROZO.voz SALIDA.mp4

TROZO es un trozo ya montado (con su voz y sus rotulos, video.sh); TROZO.voz
dice en que segundo empieza cada frase (escenas.c), y la duracion de cada
una esta en el .ms de su .wav. Entre una frase y la siguiente, la escena
hace lo que tiene que hacer: abrir un menu, esperar a una ventana. Si eso
dura mas de MAX segundos, el tramo se acelera hasta durar QUEDA: la accion
se ve igual, mas deprisa, y no hay silencio. Las frases no se tocan.
Lo mismo antes de la primera frase y despues de la ultima.
"""

import os
import subprocess
import sys

HUECO = 0.45            # lo que escenas.c deja tras cada frase
MAX = 1.0               # un hueco de mas de esto se acelera...
QUEDA = 0.7             # ...hasta durar esto


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    ent, voz, sal = sys.argv[1:]
    fin = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                                "-of", "csv=p=0", ent], capture_output=True, text=True,
                               check=True).stdout)
    habla = []                                  # [desde, hasta) con voz
    with open(voz) as f:
        for l in f:
            t, wav = l.split()
            with open(wav[:-4] + ".ms") as h:
                ms = int(h.read()) / 1000.0
            habla.append((float(t), min(fin, float(t) + ms + HUECO)))
    # Los tramos: (desde, hasta, factor). Con voz, 1; un hueco largo, mas.
    tramos, t = [], 0.0
    for a, b in habla + [(fin, fin)]:
        if a - t > MAX:
            tramos.append((t, a, (a - t) / QUEDA))
        elif a > t:
            tramos.append((t, a, 1.0))
        if b > max(a, t):
            tramos.append((max(a, t), b, 1.0))
        t = max(t, b)
    # Los tramos seguidos a velocidad normal, juntos: menos cortes.
    junta = []
    for d, h, f in tramos:
        if junta and f == 1.0 and junta[-1][2] == 1.0:
            junta[-1] = (junta[-1][0], h, 1.0)
        else:
            junta.append((d, h, f))
    ganado = sum((h - d) - (h - d) / f for d, h, f in junta)
    if ganado < 0.05:
        subprocess.run(["cp", ent, sal], check=True)
        print("acelera: %s, sin huecos" % os.path.basename(ent))
        return
    # UNA SOLA PASADA (partir el video en trozos con trim lo guarda entero en
    # memoria). La imagen: cada cuadro, a su nueva hora, con una expresion
    # por tramos; fps quita los que sobran. El sonido: de cada hueco
    # acelerado se queda el principio, QUEDA segundos (en un hueco no
    # suena nada).
    expr, o = "", 0.0
    for d, h, f in junta:
        expr += "if(lt(T,%.3f),%.4f+(T-%.3f)/%.4f," % (h, o, d, f)
        o += (h - d) / f
    expr += "%.4f" % o + ")" * len(junta)
    quita = "+".join("between(t,%.3f,%.3f)" % (d + (h - d) / f, h)
                     for d, h, f in junta if f > 1.0)
    fil = ("[0:v]setpts='(%s)/TB',fps=25[v];"
           "[0:a]aselect='not(%s)',asetpts=N/SR/TB[a]" % (expr, quita))
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", ent, "-filter_complex", fil,
                    "-map", "[v]", "-map", "[a]", "-t", "%.3f" % o, "-c:v", "libx264",
                    "-preset", "slow", "-crf", "22", "-pix_fmt", "yuv420p", "-c:a", "aac",
                    "-b:a", "96k", "-ar", "48000", "-ac", "1", sal], check=True)
    print("acelera: %s, %.1f s menos" % (os.path.basename(ent), ganado))


if __name__ == "__main__":
    main()
