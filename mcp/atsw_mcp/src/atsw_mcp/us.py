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
- guion_map: las series, sus modelos, el linaje y los elegidos. Empieza
  por aquí. Los datos de cada serie son m00: no se tocan.
- Un modelo nuevo nace de los datos (new_version sin parent) o de otro estimado
  (new_version con parent: el .pre del padre como punto de partida). Siempre con una
  razón: la razón del nodo va a la razón del modelo en el manifiesto.
- Lo estimado NO se reescribe: para cambiarlo, se deriva un hijo.

EL CAMINO DE UN MODELO
0. identification_analysis(series, transform): lo que el identificador art propone
   sobre los datos, con λ (0 log, 1 niveles), d y D; o sobre los residuos de
   un modelo estimado (residuals_of), lo que les falta. Propone; no deriva.
1. new_version(series, parent, reason) -> mNN
2. write_inp(series, mNN, ...): la transformación (λ,d,D), el ARMA
   (p,q,P,Q), las intervenciones ("step 10 2008", "impulse 3 2020", ...) y la
   media. Fue juzga el .inp: si no lo acepta, no se cambia.
3. estimate_and_diagnose(series, mNN) -> la log-verosimilitud y el dictamen.
4. La evidencia: diagnose (la diagnosis en veredictos), formal_tests (los
   contrastes formales, por familia, uno a uno; no hay veredicto encadenado),
   residual_outlier_scan (los episodios y si cambian la identificación),
   ar_factorization (la factorización del AR), full_report (la ficha de
   confianza).
5. generate_forecast(series, mNN): la previsión de un modelo estimado (fue -f y fuf), en
   la tabla de fuf: el nivel y su d.t., las variaciones y sus d.t.
6. guion_annotate / guion_adopt: dejar escrito por qué, y cuál es el modelo de la serie.

LO QUE VE EL ANALISTA
- plot(what, series, model) abre la ventana de la madre: series (y ACF/PACF),
  mean-sd, histogram, residuals, diagnosis. Es el mismo dibujo que la madre;
  devuelve además en texto lo que la ventana enseña.

LOS NOMBRES
- Las herramientas y sus argumentos se llaman como sus parejas de art (en
  Python): la misma orden hace lo mismo en las dos suites.

REGLAS
- Solo trabajas dentro del proyecto, y solo con estas herramientas: no hay
  órdenes de sistema.
- Los contrastes suponen un modelo adecuado: mira antes diagnose.
- Cada contraste dice su número y su crítico; qué hacer con ello lo decide el
  analista (guiado) o tú con la razón escrita (autónomo).
"""


def _hazlo(f, *a, **k) -> str:
    try:
        return f(*a, **k)
    except cli.NoSePuede as e:
        return f"No se pudo: {e}"


# Las palabras de las herramientas (las de art) -> las de atsw-cli y la madre.
_FAMILIAS = {"integration": "integracion", "seasonality": "estacionalidad"}
_VENTANAS = {"series": "serie", "mean-sd": "media-dt", "histogram": "histograma",
             "residuals": "residuos", "diagnosis": "diagnosis"}


def crea():
    app = comun.crea_servidor(SERVIDOR, "us", "US", INSTRUCCIONES)
    ID = cli.comprueba_id

    @app.tool()
    def guion_map() -> str:
        """El mapa del proyecto: las series, sus modelos con su linaje y su razón, y los
        elegidos (atsw-cli proyecto ver)."""
        return _hazlo(cli.atsw_cli, "proyecto", "ver")

    @app.tool()
    def identification_analysis(series: str, transform: str = "", residuals_of: str = "",
                                sample: str = "") -> str:
        """Lo que propone el identificador art: los candidatos ARMA en su orden (el
        primero es su propuesta), con sus pesos, la F estacional y ADF/KPSS. Sobre los
        datos con transform «λ,d,D» (λ 0 o 1; sin ella, la del nodo de datos), o
        sobre los residuos del modelo residuals_of. No escribe nada."""
        def f():
            a = ["identifica", ID("la serie", series)]
            if transform:    a += ["--transformacion", transform]
            if residuals_of: a += ["--residuos", ID("el modelo", residuals_of)]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def new_version(series: str, reason: str, parent: str = "", sample: str = "") -> str:
        """Un modelo nuevo de la serie: de los datos (sin parent) o del .pre de un
        modelo estimado (con parent). Devuelve su nombre, mNN. La razón es obligatoria."""
        def f():
            s = ID("la serie", series)
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía: es lo que guarda el linaje")
            if parent:
                return cli.atsw_cli("proyecto", "itera", s, ID("el padre", parent),
                                    "--razon", reason, "--por", "asistente", muestra=sample)
            return cli.atsw_cli("proyecto", "nuevo", s, "--razon", reason,
                                "--por", "asistente", muestra=sample)
        return _hazlo(f)

    @app.tool()
    def write_inp(series: str, model: str, transform: str = "", arma: str = "",
                  add_arma: str = "", interventions: list[str] | None = None,
                  mean: str = "", sample: str = "") -> str:
        """La especificación de un modelo SIN ESTIMAR. transform «λ,d,D»
        (0,1,0: log y una diferencia); arma «p,q,P,Q» (sustituye el ARMA);
        add_arma «p,q,P,Q» (multiplica un factor más); interventions como el
        motor las lee («step 10 2008», «impulse 3 2020»); mean «free» o un
        número (fija). fue juzga el resultado: si no lo acepta, no se cambia nada."""
        def f():
            a = ["especifica", ID("la serie", series), ID("el modelo", model)]
            if transform: a += ["--transformacion", transform]
            if arma:      a += ["--arma", arma]
            if add_arma:  a += ["--anade-arma", add_arma]
            for i in interventions or []:
                a += ["--intervencion", i]
            if mean:      a += ["--media", "libre" if mean == "free" else mean]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def estimate_and_diagnose(series: str, model: str, sample: str = "") -> str:
        """Estima con fue un modelo sin estimar: la log-verosimilitud y el dictamen."""
        return _hazlo(lambda: cli.atsw_cli("estima", ID("la serie", series), ID("el modelo", model),
                                           "--por", "asistente", muestra=sample))

    @app.tool()
    def diagnose(series: str, model: str, sample: str = "") -> str:
        """La diagnosis del modelo estimado, en veredictos (la de la madre)."""
        return _hazlo(lambda: cli.atsw_cli("dictamen", ID("la serie", series), ID("el modelo", model),
                                           muestra=sample))

    @app.tool()
    def formal_tests(series: str, model: str, family: str = "", underdifferencing: bool = False,
                     sample: str = "") -> str:
        """Los contrastes formales del modelo estimado, uno a uno: family
        «integration» (Shin-Fuller, DCD, sobre y subdiferenciación) o
        «seasonality» (simplificación, DCD estacional, MEG); sin ella, las dos.
        underdifferencing: también d - 1, que se pide solo con motivo. Cada uno
        reestima con fue."""
        def f():
            a = ["formales", ID("la serie", series), ID("el modelo", model)]
            if family:
                fam = _FAMILIAS.get(family)
                if fam is None:
                    raise cli.NoSePuede(f"family «{family}»: es integration o seasonality")
                a += ["--familia", fam]
            if underdifferencing:
                a += ["--sub"]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def residual_outlier_scan(series: str, model: str, episodes: str = "", sample: str = "") -> str:
        """Los episodios de los residuos y la ACF/PACF con y sin ellos: si quitarlos
        cambia la identificación. episodes «1,3» o «all» (sin ellos, todos)."""
        def f():
            a = ["anomalos", ID("la serie", series), ID("el modelo", model)]
            if episodes: a += ["--episodios", "todos" if episodes == "all" else episodes]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def ar_factorization(series: str, model: str, sample: str = "") -> str:
        """La factorización del AR del modelo estimado: sus raíces y su periodo."""
        return _hazlo(lambda: cli.atsw_cli("raices", "factores", ID("la serie", series),
                                           ID("el modelo", model), muestra=sample))

    @app.tool()
    def full_report(series: str, model: str, check: bool = False, sample: str = "") -> str:
        """La ficha de confianza del modelo, en Markdown: el recorrido, la adecuación,
        la reproducibilidad y el coste. check: corre fue sobre el .pre."""
        def f():
            a = ["ficha", ID("la serie", series), ID("el modelo", model)]
            if check: a += ["--comprobar", "--por", "asistente"]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def generate_forecast(series: str, model: str, sample: str = "") -> str:
        """La previsión de un modelo estimado: fue -f y fuf desde su .pre, sin tocar
        el modelo. La tabla de fuf: el nivel con su d.t. y las variaciones del periodo
        y anual con las suyas. Deja forecast_<modelo>.inp/.out junto al modelo."""
        return _hazlo(lambda: cli.atsw_cli("prever", ID("la serie", series), ID("el modelo", model),
                                           "--por", "asistente", muestra=sample))

    @app.tool()
    def guion_annotate(series: str, model: str, text: str, sample: str = "") -> str:
        """La razón del modelo en el manifiesto: por qué se dio este paso."""
        return _hazlo(lambda: cli.atsw_cli("proyecto", "razon", ID("la serie", series),
                                           ID("el modelo", model), text, muestra=sample))

    @app.tool()
    def guion_adopt(series: str, model: str, why: str, sample: str = "") -> str:
        """El modelo elegido de la serie, con su razón (why)."""
        return _hazlo(lambda: cli.atsw_cli("proyecto", "elige", ID("la serie", series),
                                           ID("el modelo", model), why, "--por", "asistente",
                                           muestra=sample))

    @app.tool()
    def plot(what: str, series: str, model: str = "", sample: str = "") -> str:
        """Abre la ventana de la madre, la misma que ve el analista: what es
        «series», «mean-sd», «histogram» (de la serie), «residuals» o «diagnosis»
        (del modelo; sin él, el elegido). Devuelve además en texto lo que enseña."""
        def f():
            que = _VENTANAS.get(what)
            if que is None:
                raise cli.NoSePuede(f"what «{what}»: es {', '.join(_VENTANAS)}")
            s = ID("la serie", series)
            m = ID("el modelo", model, opcional=True)
            dice = cli.ver(que, f"{s}/{m}" if m else s, muestra=sample)
            if que == "residuos" and m:
                dice += "\n\n" + cli.atsw_cli("anomalos", s, m, muestra=sample)
            elif que == "diagnosis" and m:
                dice += "\n\n" + cli.atsw_cli("dictamen", s, m, muestra=sample)
            return dice
        return _hazlo(f)

    return app


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
