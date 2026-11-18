"""GRABA LA TERMINAL del video del asistente: un cliente de LLM DE VERDAD.

La parte de la terminal (LEEME.md) no se puede escribir como una escena:
lo que contesta el modelo cambia de una vez a otra. Este programa abre una
terminal (VTE), lanza en ella el cliente en la carpeta del proyecto, le
escribe las preguntas como las escribiria el analista y graba lo que se ve,
un cuadro cada 1/10 s de reloj, por una tuberia a ffmpeg. Corre en el mismo
broadwayd que la madre (lo lanza su escena con «lanza»), y graba.sh
junta los dos videos.

    python3 graba.py GUION PROYECTO SALIDA.mp4 -- CLIENTE [ARGS...]

GUION, una orden por linea (# comenta):

    espera MS            nada, MS milisegundos
    si TEXTO | TECLA     si la terminal ensena TEXTO, pulsa TECLA
                         (Enter, Escape, Down, Up, o un caracter)
    aguarda TEXTO | S    hasta que la terminal ensene TEXTO (falla a los S s)
    quieto S | MAX       hasta que la terminal lleve S s sin cambiar: el
                         cliente ha acabado de contestar (falla a los MAX s)
    envia TEXTO          TEXTO e Intro, de golpe (antes de grabar)
    teclea TEXTO         TEXTO e Intro, a mano: se tiene que poder leer
    graba                desde aqui se graba (lo de antes no sale: el saludo
                         del cliente, que puede ensenar la cuenta)
    rotulo TEXTO | -     subtitulo, como en las escenas (SALIDA.ass)

LO QUE NO DEBE SALIR: VIGILA (en el entorno, separado por comas) son textos
que no pueden aparecer en la grabacion --la cuenta, la carpeta del usuario--.
Si la terminal ensena uno mientras se graba, se para y se borra el video. Va
en el entorno y no en el guion: el guion esta en el repositorio.

La hora de reloj en que empezo a grabar queda en SALIDA.t0: graba.sh
alinea con ella el video de la madre. Al acabar, bien o mal, escribe
SALIDA.fin: la escena de la madre lo espera («existe») para salir.
"""

from __future__ import annotations

import os
import subprocess
import sys
import time

# Gtk.init se come el «--» de sys.argv: se guarda antes.
ARGV = list(sys.argv)

import gi  # noqa: E402

gi.require_version("Gtk", "3.0")
gi.require_version("Vte", "2.91")
import cairo  # noqa: E402
from gi.repository import GLib, Gtk, Pango, Vte  # noqa: E402

FPS = 10
ANCHO, ALTO = 880, 880
TECLAS = {"Enter": "\r", "Escape": "\x1b", "Down": "\x1b[B", "Up": "\x1b[A"}
ESTILO = (
    "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n"
    "WrapStyle: 0\n\n[V4+ Styles]\n"
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
    "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
    "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
    "MarginL, MarginR, MarginV, Encoding\n"
    "Style: Abajo,Sans,44,&H00FFFFFF,&H00FFFFFF,&H00000000,&H90201A10,0,0,0,0,"
    "100,100,0,0,3,14,0,2,120,120,40,1\n\n"
    "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, "
    "MarginV, Effect, Text\n")


def hms(s: float) -> str:
    h, m = int(s // 3600), int(s // 60) % 60
    return f"{h}:{m:02d}:{s - h * 3600 - m * 60:05.2f}"


class Grabador:
    def __init__(self, guion: str, proyecto: str, salida: str, cliente: list[str]):
        self.ordenes = [l.rstrip("\n") for l in open(guion, encoding="utf-8")
                        if l.strip() and not l.startswith("#")]
        self.salida = salida
        self.vigila = [v for v in os.environ.get("VIGILA", "").split(",") if v]
        self.ffmpeg = None
        self.t0 = None
        self.ncuadro = 0
        self.ass = [ESTILO]
        self.rot = None          # (desde, texto)
        self.log = open(salida + ".log", "w", encoding="utf-8")
        self.error = None

        self.win = Gtk.Window(title="Terminal")
        self.win.set_default_size(ANCHO, ALTO)
        self.win.set_resizable(False)
        self.term = Vte.Terminal()
        self.term.set_font(Pango.FontDescription("Monospace 15"))
        # Las columnas que caben: la terminal no pide mas ancho que el de la
        # ventana (si no, el texto se sale por la derecha y no se ve).
        cw, ch = self.term.get_char_width(), self.term.get_char_height()
        self.term.set_size(max(40, (ANCHO - 24) // cw), max(10, (ALTO - 60) // ch))
        self.term.set_scrollback_lines(2000)
        self.win.add(self.term)
        self.win.connect("destroy", lambda *_: Gtk.main_quit())
        self.win.show_all()

        # El cliente, con el entorno de quien lo usa y no el de las escenas:
        # ni el PATH recortado ni las XDG temporales; y sin las variables de
        # una sesion de cliente que lo lance (se negaria a anidarse).
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(("CLAUDE", "XDG_", "GDK_", "BROADWAY", "GTK_"))
               and k not in ("AI_AGENT", "VIGILA", "PATH")}
        env.update(PATH="/usr/local/bin:/usr/bin:/bin", TERM="xterm-256color",
                   COLORTERM="truecolor", LANG="es_ES.UTF-8")
        # VTE SUMA el entorno que se le da al del proceso: env -i lo deja
        # solo en el que se le da.
        self.term.spawn_async(Vte.PtyFlags.DEFAULT, proyecto,
                              ["/usr/bin/env", "-i", *[f"{k}={v}" for k, v in env.items()],
                               *cliente], None,
                              GLib.SpawnFlags.DEFAULT, None, None, -1, None, None, None)
        self.term.connect("child-exited", self.salio)

        self.i = 0
        self.espera_hasta = 0.0
        self.paso = None          # la orden en curso que espera algo
        self.ultimo_texto = ""
        self.cambio = time.monotonic()
        GLib.timeout_add(1000 // FPS, self.tic)

    # ---- lo que ensena la terminal -------------------------------------
    def texto(self) -> str:
        try:
            t = self.term.get_text_format(Vte.Format.TEXT)
        except (AttributeError, TypeError):
            t = self.term.get_text(None, None)
            t = t[0] if isinstance(t, tuple) else t
        return t or ""

    def tecla(self, k: str) -> None:
        self.term.feed_child(TECLAS.get(k, k).encode())

    def salio(self, *_):
        if self.i < len(self.ordenes):
            self.falla("el cliente ha salido antes de acabar el guion")

    def falla(self, por: str) -> None:
        self.error = por
        print(f"terminal_graba: {por}", file=sys.stderr)
        print(f"!! {por}\n{self.texto()}", file=self.log, flush=True)
        Gtk.main_quit()

    # ---- la grabacion --------------------------------------------------
    def cuadro(self) -> None:
        if self.ffmpeg is None:
            return
        # La ventana se dibuja a su tamano y se pone en el cuadro de ANCHO x
        # ALTO, escalada si no cabe.
        w, h = self.win.get_allocated_width(), self.win.get_allocated_height()
        s = cairo.ImageSurface(cairo.FORMAT_RGB24, ANCHO, ALTO)
        cr = cairo.Context(s)
        e = min(1.0, ANCHO / max(w, 1), ALTO / max(h, 1))
        cr.scale(e, e)
        self.win.draw(cr)
        s.flush()
        datos = bytes(s.get_data())
        # De reloj: si un cuadro se retrasa, se repite el anterior.
        debidos = int((time.monotonic() - self.t0) * FPS) + 1
        while self.ncuadro < debidos:
            self.ffmpeg.stdin.write(datos)
            self.ncuadro += 1

    def empieza(self) -> None:
        self.ffmpeg = subprocess.Popen(
            ["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "bgr0",
             "-s", f"{ANCHO}x{ALTO}", "-r", str(FPS), "-i", "-",
             "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p",
             self.salida], stdin=subprocess.PIPE)
        self.t0 = time.monotonic()
        with open(self.salida + ".t0", "w") as f:
            f.write(f"{time.time():.3f}\n")

    def rotulo(self, txt: str | None) -> None:
        ahora = self.ncuadro / FPS
        if self.rot and ahora > self.rot[0]:
            self.ass.append(f"Dialogue: 0,{hms(self.rot[0])},{hms(ahora)},Abajo,,0,0,0,,"
                            f"{self.rot[1]}\n")
        self.rot = (ahora, txt) if txt and txt != "-" else None

    def acaba(self) -> None:
        self.rotulo(None)
        if self.ffmpeg:
            self.ffmpeg.stdin.close()
            self.ffmpeg.wait()
            with open(self.salida + ".ass", "w", encoding="utf-8") as f:
                f.writelines(self.ass)
        if self.error and os.path.exists(self.salida):
            os.unlink(self.salida)
        # La escena de la madre espera a este fichero («existe») para salir.
        with open(self.salida + ".fin", "w") as f:
            f.write("error\n" if self.error else "ok\n")

    # ---- el guion ------------------------------------------------------
    def tic(self) -> bool:
        if self.error:
            return False
        t = self.texto()
        ahora = time.monotonic()
        if t != self.ultimo_texto:
            self.ultimo_texto, self.cambio = t, ahora
        if self.ffmpeg:
            for v in self.vigila:
                if v in t:
                    self.falla("la terminal ensena algo que no debe salir")
                    return False
        self.cuadro()
        if ahora < self.espera_hasta:
            return True
        if self.paso:
            orden, arg, hasta = self.paso
            if ahora > hasta:
                self.falla(f"{orden} {arg}: no llega")
                return False
            if orden == "aguarda" and arg not in t:
                return True
            if orden == "quieto" and ahora - self.cambio < float(arg):
                return True
            if orden == "teclea" and arg:
                self.tecla(arg[0])
                self.paso = (orden, arg[1:], hasta)
                return True
            if orden == "teclea":
                self.tecla("Enter")
            self.paso = None
        while self.i < len(self.ordenes):
            linea = self.ordenes[self.i]
            self.i += 1
            orden, _, resto = linea.partition(" ")
            arg, _, opc = resto.partition(" | ")
            print(f"== {linea}", file=self.log, flush=True)
            if orden == "espera":
                self.espera_hasta = ahora + int(arg) / 1000
                return True
            if orden == "si":
                if arg in t:
                    self.tecla(opc or "Enter")
                    self.espera_hasta = ahora + 1.5
                    return True
                continue
            if orden == "aguarda":
                self.paso = ("aguarda", arg, ahora + float(opc or 60))
                return True
            if orden == "quieto":
                self.paso = ("quieto", arg, ahora + float(opc or 300))
                return True
            if orden == "envia":
                self.term.feed_child((arg + "\r").encode())
                self.espera_hasta = ahora + 1
                return True
            if orden == "teclea":
                self.paso = ("teclea", arg, ahora + 600)
                return True
            if orden == "graba":
                self.empieza()
                continue
            if orden == "rotulo":
                self.rotulo(resto)
                continue
            self.falla(f"orden desconocida: {linea}")
            return False
        print(f"== fin\n{t}", file=self.log, flush=True)
        Gtk.main_quit()
        return False


def main() -> int:
    if "--" not in ARGV or ARGV.index("--") != 4:
        print(__doc__, file=sys.stderr)
        return 2
    g = Grabador(ARGV[1], ARGV[2], ARGV[3], ARGV[5:])
    Gtk.main()
    g.acaba()
    return 1 if g.error else 0


if __name__ == "__main__":
    sys.exit(main())
