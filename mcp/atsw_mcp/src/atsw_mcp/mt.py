"""atsw-mt: el servidor de la clase MT, transferencias (docs/ESTUDIO-terminal-mcp.md).

Fase 1b, primera parte: identificar los enlaces, crear los casos y abrirlos en
drtran_gui. Estimar un caso desde aqui llega despues, cuando la corrida de
drtran_gui este en atsw-cli (caso-red, caso-estima): mientras, se estima en
drtran_gui, que es la ventana del caso.
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
1. proyecto_resumen: las series, sus modelos estimados y los casos que ya hay.
2. identificar_enlace(salida, entradas): la CCF preblanqueada de cada entrada
   con su propio ARMA (la salida con el mismo filtro). Se lee b (el retardo
   puro), s (el orden del numerador) y, sobre todo, los retardos NEGATIVOS
   fuera de banda: son el contraste de exogeneidad. Si la salida antecede a la
   entrada, no hay (b, s) que lo arregle y esa entrada no lo es.
3. caso_nuevo(salida, entradas, titulo, razon): el caso, con lo que se cruza y
   el sha256 de cada .pre. La salida va primero.
4. abrir_caso(caso): drtran_gui con el caso, donde se define la red y se
   estima. Desde aquí todavía no se estima: díselo al analista.

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
    def proyecto_resumen() -> str:
        """Las series del proyecto, sus modelos y los casos que ya hay."""
        return _hazlo(cli.atsw_cli, "proyecto", "ver")

    @app.tool()
    def identificar_enlace(salida: str, entradas: list[str], retardos: int = 0,
                           muestra: str = "") -> str:
        """La CCF preblanqueada de cada entrada contra la salida (SERIE/MODELO,
        modelos estimados): b, s, el pico, los retardos negativos fuera de banda
        (la exogeneidad) y el portmanteau de Hosking. retardos: a cada lado (0:
        la regla del motor)."""
        def f():
            if not entradas:
                raise cli.NoSePuede("hace falta al menos una entrada")
            a = ["prewhiten", _clave("la salida", salida)] + [_clave("la entrada", e) for e in entradas]
            if retardos:
                a += ["--retardos", str(int(retardos))]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def caso_nuevo(salida: str, entradas: list[str], razon: str, titulo: str = "",
                   muestra: str = "") -> str:
        """Un caso de transferencia: la salida y sus entradas (SERIE/MODELO, modelos
        estimados), con el sha256 de cada .pre. La razón es obligatoria."""
        def f():
            if not entradas:
                raise cli.NoSePuede("hace falta al menos una entrada")
            if not razon.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            a = ["proyecto", "caso-nuevo", _clave("la salida", salida)]
            a += [_clave("la entrada", e) for e in entradas]
            a += ["--motor", "drtran", "--razon", razon]
            if titulo:
                a += ["--titulo", titulo]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def abrir_caso(caso: str, modelo: str = "") -> str:
        """Abre el caso en drtran_gui, donde se define la red y se estima. modelo:
        una corrida del caso para cargar (sin ella, la elegida o ninguna)."""
        return _hazlo(cli.abre_caso, caso, modelo)

    return app


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
