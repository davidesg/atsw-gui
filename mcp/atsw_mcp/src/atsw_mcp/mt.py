"""atsw-mt: el servidor de la clase MT, transferencias (docs/ESTUDIO-terminal-mcp.md).

Fase 1b: identificar los enlaces, crear los casos, estimarlos (atsw-cli
caso-estima, lo que hace «Estimar» en drtran_gui) y abrirlos en drtran_gui.
"""

from __future__ import annotations

from atsw_mcp import cli, comun

SERVIDOR = "atsw-mt"

INSTRUCCIONES = """\
atsw-mt. Eres el asistente de ATSW para modelos de FUNCIÓN DE TRANSFERENCIA,
dentro del proyecto de la madre. Responde en el idioma del usuario.

DE DÓNDE PARTES: de los modelos univariantes ya estimados de cada serie (sus
.pre), que se hacen con atsw-us. Aquí no se identifican univariantes.

PRIMERO PREGUNTA: cuál es la SALIDA y cuáles las ENTRADAS, y «¿guiado o
autónomo?» (los mismos nodos en los dos; en autónomo decides tú, con la razón
por escrito).

EL CAMINO
1. guion_map: las series, sus modelos estimados y los casos que ya hay.
2. identify_link(output, inputs): la CCF preblanqueada de cada entrada
   con su propio ARMA (la salida con el mismo filtro). Se lee b (el retardo
   puro), s (el orden del numerador) y, sobre todo, los retardos NEGATIVOS
   fuera de banda: son el contraste de exogeneidad. Si la salida antecede a la
   entrada, no hay (b, s) que lo arregle y esa entrada no lo es.
3. load_pre(output, inputs, title, reason): el caso, con lo que se cruza y
   el sha256 de cada .pre. La salida va primero.
4. estimate(case, links, from_run, reason): una corrida del caso con
   drtran. links como «ENTRADA->SALIDA:b,r,s» con los nombres de serie del
   proyecto; sin ellos y con una entrada, drtran identifica b, r y s solo;
   from_run: la corrida de la que parte (hereda su red y sus restricciones);
   diagonal: el ajuste sin transferencias, la referencia. Devuelve la
   diagnosis: cuántos contrastes de adecuación y de exogeneidad no pasan.
   constraints: el .cns, «NOMBRE = VALOR» con los nombres de la tabla
   (q[2,1] = free libera una covarianza; omega1[1] = 0 fija; a = b
   comparte); se heredan de from_run y drtran las juzga.
   horizon H: la previsión; window W (con horizon): la evaluación fuera de
   muestra y, frente al diagonal con la misma ventana, el cociente de RMSE y
   Diebold-Mariano por horizonte. Es la prueba de que la transferencia sirve
   para prever: un cociente < 1 con DM significativo.
5. open_case(case, run): el caso en drtran_gui, para que el analista vea
   la diagnosis, la previsión y la respuesta al impulso.

LOS NOMBRES
- Las herramientas y sus argumentos se llaman como sus parejas de drtran
  (en Python): la misma orden hace lo mismo en las dos suites.

CADA CORRIDA devuelve la tabla de parámetros de drtran: preséntala TAL CUAL,
en un bloque de código; no la rehagas (los retardos del numerador RESTAN:
w(B) = w0 - w1 B - ...). De ella salen los nombres de las restricciones.

REGLAS
- Las series y los modelos se dicen como SERIE/MODELO («IPC/m03»).
- Solo trabajas dentro del proyecto y con estas herramientas.
"""


def _hazlo(f, *a, **k) -> str:
    try:
        return f(*a, **k)
    except cli.NoSePuede as e:
        return f"No se pudo: {e}"


def _clave(que: str, v: str) -> str:
    """SERIE/MODELO, comprobado pieza a pieza."""
    if "/" not in v:
        raise cli.NoSePuede(f"{que} «{v}»: es SERIE/MODELO («IPC/m03»)")
    s, m = v.split("/", 1)
    return f"{cli.comprueba_id('la serie', s)}/{cli.comprueba_id('el modelo', m)}"


def crea():
    app = comun.crea_servidor(SERVIDOR, "mt", "MT", INSTRUCCIONES)

    @app.tool()
    def guion_map() -> str:
        """El mapa del proyecto: las series, sus modelos y los casos que ya hay
        (atsw-cli proyecto ver)."""
        return _hazlo(cli.atsw_cli, "proyecto", "ver")

    @app.tool()
    def identify_link(output: str, inputs: list[str], lags: int = 0, sample: str = "") -> str:
        """La CCF preblanqueada de cada entrada contra la salida (SERIE/MODELO,
        modelos estimados): b, s, el pico, los retardos negativos fuera de banda
        (la exogeneidad) y el portmanteau de Hosking. lags: a cada lado (0: la regla
        del motor)."""
        def f():
            if not inputs:
                raise cli.NoSePuede("hace falta al menos una entrada")
            a = ["prewhiten", _clave("la salida", output)] + [_clave("la entrada", e) for e in inputs]
            if lags:
                a += ["--retardos", str(int(lags))]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def load_pre(output: str, inputs: list[str], reason: str, title: str = "",
                 sample: str = "") -> str:
        """Un caso de transferencia: la salida (la primera) y sus entradas
        (SERIE/MODELO, modelos estimados), con el sha256 de cada .pre. La razón es
        obligatoria."""
        def f():
            if not inputs:
                raise cli.NoSePuede("hace falta al menos una entrada")
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            a = ["proyecto", "caso-nuevo", _clave("la salida", output)]
            a += [_clave("la entrada", e) for e in inputs]
            a += ["--motor", "drtran", "--razon", reason]
            if title:
                a += ["--titulo", title]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def estimate(case: str, reason: str, links: list[str] | None = None, from_run: str = "",
                 diagonal: bool = False, horizon: int = 0, window: int = 0,
                 constraints: list[str] | None = None) -> str:
        """Una corrida del caso con drtran: con los links «ENTRADA->SALIDA:b,r,s»
        (nombres de serie del proyecto), con la red de la corrida from_run, o sin red
        (con una entrada, drtran identifica b, r y s). diagonal: sin transferencias.
        Si drtran no deja resultados, no queda la corrida. La razón es obligatoria.
        horizon H: la previsión a H pasos; window W (con horizon): la evaluación fuera
        de muestra con una ventana de estimación de W, y frente al diagonal (el cociente
        de RMSE y Diebold-Mariano por horizonte: ¿mejora la transferencia la previsión?).
        constraints: líneas del .cns, «NOMBRE = VALOR», con los nombres de la tabla de
        parámetros: «q[2,1] = free» (liberar una covarianza, que nacen fijas en cero),
        «omega1[1] = 0» (fijar), «delta1[1] = delta2[1]» (compartir), productos y
        combinaciones lineales. Van detrás de las heredadas de from_run, y la última
        sobre un slot manda. drtran las juzga: un nombre que no tiene no deja corrida.
        Devuelve la tabla de parámetros del .out, tal cual."""
        def f():
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            a = ["caso-estima", cli.comprueba_id("el caso", case)]
            for e in links or []:
                a += ["--enlace", e]
            if from_run:
                a += ["--desde", cli.comprueba_id("la corrida", from_run)]
            if diagonal:
                a += ["--diagonal"]
            if horizon:
                a += ["--prever", str(int(horizon))]
            if window:
                a += ["--evaluar", str(int(window))]
            for r in constraints or []:
                a += ["--restriccion", r]
            a += ["--razon", reason, "--por", "asistente"]
            return cli.atsw_cli(*a)
        return _hazlo(f)

    @app.tool()
    def open_case(case: str, run: str = "") -> str:
        """Abre el caso en drtran_gui, donde se define la red y se estima. run:
        una corrida del caso para cargar (sin ella, la elegida o ninguna)."""
        return _hazlo(cli.abre_caso, case, run)

    return app


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
