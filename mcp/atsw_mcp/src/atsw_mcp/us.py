"""atsw-us: el servidor de la clase US, univariantes (docs/ESTUDIO-terminal-mcp.md).

Fase 1b: las herramientas del proyecto y del camino en C. Cada una llama a una
orden de atsw-cli --las mismas funciones que las ventanas de la madre-- o abre
una ventana de la madre con atsw_gui --ver. Lo que queda en Python (la lambda
de Box-Cox, la escalera de intervenciones, el identificador de art) llegara
con el extra.
"""

from __future__ import annotations

from atsw_mcp import cli, comun

SERVIDOR = "atsw-us"

INSTRUCCIONES = """\
atsw-us. Eres el asistente de ATSW para modelos UNIVARIANTES (ARIMA con intervenciones),
dentro del proyecto de la madre: trabajas sobre su proyecto.yaml, sus series y
el linaje de sus modelos, con los motores en C del paquete (fue). Responde en el
idioma del usuario.

PRIMERO PREGUNTA: «¿Guiado o autónomo?»
- GUIADO: el analista decide cada nodo. Tú enseñas la evidencia, propones
  alternativas y paras.
- AUTÓNOMO: recorres los MISMOS nodos y decides cada uno, con su razón por
  escrito, y entregas el recorrido y el modelo. No es un ajuste automático de
  una llamada.

EL PROYECTO
- proyecto_resumen: las series, sus modelos, el linaje y los elegidos. Empieza
  por aquí. Los datos de cada serie son m00: no se tocan.
- Un modelo nuevo nace de los datos (derivar sin padre) o de otro estimado
  (derivar con padre: el .pre del padre como punto de partida). Siempre con una
  razón: la razón del nodo va a la razón del modelo en el manifiesto.
- Lo estimado NO se reescribe: para cambiarlo, se deriva un hijo.

EL CAMINO DE UN MODELO
0. identificar(serie, transformacion): lo que el identificador art propone
   sobre los datos, con λ (0 log, 1 niveles), d y D; o sobre los residuos de
   un modelo estimado (residuos_de), lo que les falta. Propone; no deriva.
1. derivar(serie, padre, razon) -> mNN
2. especificar(serie, mNN, ...): la transformación (λ,d,D), el ARMA
   (p,q,P,Q), las intervenciones ("step 10 2008", "impulse 3 2020", ...) y la
   media. Fue juzga el .inp: si no lo acepta, no se cambia.
3. estimar(serie, mNN) -> la log-verosimilitud y el dictamen.
4. La evidencia: dictamen (la diagnosis en veredictos), formales (los
   contrastes formales, por familia, uno a uno; no hay veredicto encadenado),
   anomalos (los episodios y si cambian la identificación), raices (la
   factorización del AR), ficha (la ficha de confianza).
5. razon / elegir: dejar escrito por qué, y cuál es el modelo de la serie.

LO QUE VE EL ANALISTA
- ver(que, serie, modelo) abre la ventana de la madre: serie (y ACF/PACF),
  media-dt, histograma, residuos, diagnosis. Es el mismo dibujo que la madre;
  devuelve además en texto lo que la ventana enseña.

REGLAS
- Solo trabajas dentro del proyecto, y solo con estas herramientas: no hay
  órdenes de sistema.
- Los contrastes suponen un modelo adecuado: mira antes el dictamen.
- Cada contraste dice su número y su crítico; qué hacer con ello lo decide el
  analista (guiado) o tú con la razón escrita (autónomo).
"""


def _hazlo(f, *a, **k) -> str:
    try:
        return f(*a, **k)
    except cli.NoSePuede as e:
        return f"No se pudo: {e}"


def crea():
    app = comun.crea_servidor(SERVIDOR, "us", "US", INSTRUCCIONES)
    ID = cli.comprueba_id

    @app.tool()
    def proyecto_resumen() -> str:
        """Las series del proyecto, sus modelos con su linaje y su razón, y los elegidos."""
        return _hazlo(cli.atsw_cli, "proyecto", "ver")

    @app.tool()
    def identificar(serie: str, transformacion: str = "", residuos_de: str = "",
                    muestra: str = "") -> str:
        """Lo que propone el identificador art: los candidatos ARMA en su orden (el
        primero es su propuesta), con sus pesos, la F estacional y ADF/KPSS. Sobre los
        datos con transformacion «λ,d,D» (λ 0 o 1; sin ella, la del nodo de datos), o
        sobre los residuos del modelo residuos_de. No escribe nada."""
        def f():
            a = ["identifica", ID("la serie", serie)]
            if transformacion: a += ["--transformacion", transformacion]
            if residuos_de:    a += ["--residuos", ID("el modelo", residuos_de)]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def derivar(serie: str, razon: str, padre: str = "", muestra: str = "") -> str:
        """Un modelo nuevo de la serie: de los datos (sin padre) o del .pre de un
        modelo estimado (con padre). Devuelve su nombre, mNN. La razón es obligatoria."""
        def f():
            s = ID("la serie", serie)
            if not razon.strip():
                raise cli.NoSePuede("la razón no puede ir vacía: es lo que guarda el linaje")
            if padre:
                return cli.atsw_cli("proyecto", "itera", s, ID("el padre", padre),
                                    "--razon", razon, "--por", "asistente", muestra=muestra)
            return cli.atsw_cli("proyecto", "nuevo", s, "--razon", razon,
                                "--por", "asistente", muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def especificar(serie: str, modelo: str, transformacion: str = "", arma: str = "",
                    anade_arma: str = "", intervenciones: list[str] | None = None,
                    media: str = "", muestra: str = "") -> str:
        """La especificación de un modelo SIN ESTIMAR. transformacion «λ,d,D»
        (0,1,0: log y una diferencia); arma «p,q,P,Q» (sustituye el ARMA);
        anade_arma «p,q,P,Q» (multiplica un factor más); intervenciones como el
        motor las lee («step 10 2008», «impulse 3 2020»); media «libre» o un
        número (fija). fue juzga el resultado: si no lo acepta, no se cambia nada."""
        def f():
            a = ["especifica", ID("la serie", serie), ID("el modelo", modelo)]
            if transformacion: a += ["--transformacion", transformacion]
            if arma:           a += ["--arma", arma]
            if anade_arma:     a += ["--anade-arma", anade_arma]
            for i in intervenciones or []:
                a += ["--intervencion", i]
            if media:          a += ["--media", media]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def estimar(serie: str, modelo: str, muestra: str = "") -> str:
        """Estima con fue un modelo sin estimar: la log-verosimilitud y el dictamen."""
        return _hazlo(lambda: cli.atsw_cli("estima", ID("la serie", serie), ID("el modelo", modelo),
                                           "--por", "asistente", muestra=muestra))

    @app.tool()
    def dictamen(serie: str, modelo: str, muestra: str = "") -> str:
        """La diagnosis del modelo estimado, en veredictos (la de la madre)."""
        return _hazlo(lambda: cli.atsw_cli("dictamen", ID("la serie", serie), ID("el modelo", modelo),
                                           muestra=muestra))

    @app.tool()
    def formales(serie: str, modelo: str, familia: str = "", sub: bool = False,
                 muestra: str = "") -> str:
        """Los contrastes formales del modelo estimado, uno a uno: familia
        «integracion» (Shin-Fuller, DCD, sobre y subdiferenciación) o
        «estacionalidad» (simplificación, DCD estacional, MEG); sin ella, las dos.
        sub: también d - 1, que se pide solo con motivo. Cada uno reestima con fue."""
        def f():
            a = ["formales", ID("la serie", serie), ID("el modelo", modelo)]
            if familia: a += ["--familia", familia]
            if sub:     a += ["--sub"]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def anomalos(serie: str, modelo: str, episodios: str = "", muestra: str = "") -> str:
        """Los episodios de los residuos y la ACF/PACF con y sin ellos: si quitarlos
        cambia la identificación. episodios «1,3» o «todos» (sin ellos, todos)."""
        def f():
            a = ["anomalos", ID("la serie", serie), ID("el modelo", modelo)]
            if episodios: a += ["--episodios", episodios]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def raices(serie: str, modelo: str, muestra: str = "") -> str:
        """La factorización del AR del modelo estimado: sus raíces y su periodo."""
        return _hazlo(lambda: cli.atsw_cli("raices", "factores", ID("la serie", serie),
                                           ID("el modelo", modelo), muestra=muestra))

    @app.tool()
    def ficha(serie: str, modelo: str, comprobar: bool = False, muestra: str = "") -> str:
        """La ficha de confianza del modelo, en Markdown: el recorrido, la adecuación,
        la reproducibilidad y el coste. comprobar: corre fue sobre el .pre."""
        def f():
            a = ["ficha", ID("la serie", serie), ID("el modelo", modelo)]
            if comprobar: a += ["--comprobar", "--por", "asistente"]
            return cli.atsw_cli(*a, muestra=muestra)
        return _hazlo(f)

    @app.tool()
    def razon(serie: str, modelo: str, texto: str, muestra: str = "") -> str:
        """La razón del modelo en el manifiesto: por qué se dio este paso."""
        return _hazlo(lambda: cli.atsw_cli("proyecto", "razon", ID("la serie", serie),
                                           ID("el modelo", modelo), texto, muestra=muestra))

    @app.tool()
    def elegir(serie: str, modelo: str, razon: str, muestra: str = "") -> str:
        """El modelo elegido de la serie, con su razón."""
        return _hazlo(lambda: cli.atsw_cli("proyecto", "elige", ID("la serie", serie),
                                           ID("el modelo", modelo), razon, "--por", "asistente",
                                           muestra=muestra))

    @app.tool()
    def ver(que: str, serie: str, modelo: str = "", muestra: str = "") -> str:
        """Abre la ventana de la madre, la misma que ve el analista: que es
        «serie», «media-dt», «histograma» (de la serie), «residuos» o «diagnosis»
        (del modelo; sin él, el elegido). Devuelve además en texto lo que enseña."""
        def f():
            s = ID("la serie", serie)
            m = ID("el modelo", modelo, opcional=True)
            dice = cli.ver(que, f"{s}/{m}" if m else s, muestra=muestra)
            if que == "residuos" and m:
                dice += "\n\n" + cli.atsw_cli("anomalos", s, m, muestra=muestra)
            elif que == "diagnosis" and m:
                dice += "\n\n" + cli.atsw_cli("dictamen", s, m, muestra=muestra)
            return dice
        return _hazlo(f)

    return app


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
