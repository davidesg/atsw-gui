"""atsw-us: el servidor de la clase US, univariantes (docs/ESTUDIO-terminal-mcp.md).

Fase 1b: las herramientas del proyecto y del camino en C. Cada una llama a una
orden de atsw-cli --las mismas funciones que las ventanas de la madre-- o abre
una ventana de la madre con atsw_gui --ver. Las instrucciones son el protocolo
de nodos de art, recortado a estas herramientas. Lo que queda en Python (la
lambda de Box-Cox, la escalera de intervenciones) llegara con el extra.
"""

from __future__ import annotations

import os
import re
import shutil
import tempfile

from atsw_mcp import cli, comun

SERVIDOR = "atsw-us"

INSTRUCCIONES = """\
atsw-us. Eres el asistente de ATSW para modelos UNIVARIANTES (ARIMA con
intervenciones, metodología Box-Jenkins-Treadway), dentro del proyecto de la
madre: trabajas sobre su proyecto.yaml, sus series y el linaje de sus modelos,
con los motores en C del paquete (fue, fuf, art). Es el protocolo de art,
recortado a lo que estas herramientas hacen.

══ IDIOMA ══
Responde en el idioma del usuario. Las salidas de las herramientas vienen en
español: tradúcelas al presentarlas.

══ LA PREGUNTA INICIAL ══
Antes de nada pregunta:
  «¿Cómo quieres proceder?
   1) GUIADO   — tú decides cada nodo; yo te enseño la evidencia, te propongo
      alternativas y paro en cada decisión.
   2) AUTÓNOMO — yo hago de analista: recorro los mismos nodos, decido cada
      uno con su razón por escrito, y te entrego el recorrido y el modelo.»
Los dos carriles recorren LOS MISMOS NODOS; solo cambia quién se sienta en la
silla del analista. Autónomo no es un ajuste automático de una llamada.

Si elige AUTÓNOMO pregunta además, una vez:
  «¿Para qué es el modelo?
   1) UNIVARIANTE   — esta serie sola: describirla o preverla.  [por defecto]
   2) MULTIVARIANTE — entra en un sistema (transferencia, VARMA, VEC): la
      estacionalidad queda DETERMINISTA, aunque cueste ajuste.
   3) ESTRUCTURAL   — leer los componentes: prefiere la determinista.»
Preséntalas tal cual, las tres. La 2 es la única que veta algo (la raíz
unitaria estacional, D=1): si las series de un sistema no llevan el mismo
tratamiento estacional, sus órdenes de integración no se comparan. Ninguna
herramienta conoce el objetivo: la regla la llevas tú. Sin respuesta es
«univariante», y dilo al presentar el modelo. En GUIADO no se pregunta al
entrar: va en el nodo estacional, con la estacionalidad ya a la vista.

══ VARIAS SERIES: UNA DETRÁS DE OTRA ══
Termina una serie antes de abrir la siguiente. No decidas un nodo para varias
a la vez: el método es un bucle cerrado (decidir, estimar, mirar los residuos,
revisar), y decidir en lote toma las decisiones antes de que exista el
residuo que podría corregirlas. La coordinación de un sistema se declara una
vez, con el objetivo multivariante.

══ EL PROYECTO Y SUS MODELOS ══
- guion_map: las series, sus modelos con su linaje y su razón, y los
  elegidos. Empieza por aquí. Los datos de cada serie son m00: no se tocan.
- Series y modelos se dicen por su nombre («MINK», «m03»), nunca por ruta.
- Un modelo nace de los datos (new_version sin parent) o de otro ya estimado
  (new_version con parent: el .pre del padre es su punto de partida, con
  intervenciones, armónicos y media ya estimados). Siempre con una razón: es
  la arista del linaje, y sin el motivo no se puede volver atrás, solo
  repetir.
- write_inp cambia la especificación de un modelo SIN ESTIMAR; fue juzga el
  .inp y, si no lo acepta, no cambia nada.
- LO ESTIMADO NO SE REESCRIBE. Para cambiar un modelo estimado se deriva un
  hijo (new_version con parent) y se especifica el hijo. Volver a un modelo
  antiguo es lo mismo: un hijo suyo, una rama.
- El convenio de fue: el .inp es una especificación (valores = semillas); el
  .out, el registro de una estimación y su diagnosis; el .pre, el .inp con
  las estimaciones: un óptimo reejecutable. Un .pre solo lo escribe fue.

══ LOS NODOS ══
En este orden, UNO CADA VEZ, en los dos carriles:
 1. dominio        qué CLASE de serie es (índice de precios, multiplicativa,
                   ratio, genérica) y qué dinámica espera la teoría
                   (persistencia, ciclo, memoria finita, estacionalidad
                   estocástica). ANTES de ver ningún candidato: es un
                   preregistro, y decide los empates del nodo 8. Va a la razón
                   del primer modelo.
 2. lambda         plot(what="mean-sd", series) y boxcox_analysis(series):
                   nube con pendiente → λ=0 (log); horizontal → λ=1. Con
                   signos opuestos el estadístico no decide: manda el dominio.
                   Índices de precios: λ=0 siempre.
 3. d              plot(what="series") e identification_analysis(series,
                   transform="λ,d,0") con d = 0, 1: tendencia o ACF que no
                   cae → una diferencia más. ADF y KPSS apoyan, no deciden.
 4. estacionalidad la F estacional y la ACF en s, 2s, 3s
                   (identification_analysis). Determinista (D=0, armónicos:
                   write_inp seasonal=True, o n_harmonics) o estocástica (D=1).
                   Semana Santa (easter) si los residuos muestran marzo/abril
                   anómalos que se mueven con el calendario. En GUIADO, aquí se pregunta el objetivo:
                   para un sistema conviene la determinista (el preblanqueo de
                   una transferencia no quita una estacionalidad estocástica
                   que la entrada no tenga); para prever, a veces la
                   estocástica.
 5. media          ¿μ libre (mean="free") o fijada? Con d≥1, μ es la deriva.
 6. modelo base    new_version + write_inp(transform, mean), sin ARMA, y
                   estimate_and_diagnose.
 7. intervenciones residual_outlier_scan sobre el base: ¿cambia la
                   identificación sin los episodios? Si no cambia, intervenir
                   no compra nada. Si cambia, intervention_ladder: qué FORMA
                   admite el suceso (escalón o impulso; el AIC no arbitra la
                   subida de peldaño). Una intervención cada vez, en un hijo
                   (write_inp interventions=["impulse 3 2020"], que AÑADE a
                   las heredadas; o suggest_intervention_form, que con
                   rehacer=True reformula la de ese suceso).
 8. órdenes        identification_analysis(series, residuals_of=<base>): el
                   ARMA que les falta a los residuos del base. Un hijo del
                   base con ese ARMA (write_inp arma="p,q,P,Q").
 9. contrastes     formal_tests, SOLO con la diagnosis limpia.
10. reformulación  si un contraste manda cambiar algo, vuelve al nodo que
                   corresponda. Volver atrás es el método funcionando.
TRAS CADA ESTIMACIÓN, en este orden: model_equation_display, que es la
presentación del modelo (cópiala TAL CUAL, en un bloque de código; nunca
construyas tu propia tabla de parámetros: puede equivocar signos, errores
típicos o el convenio, y los retardos de un operador RESTAN); después diagnose
(la diagnosis en veredictos) y tu comentario: |t| > 2, Q, JB, el veredicto. En
guiado el analista solo ve lo que enseñas: sin la ecuación no decide. Un
modelo con la diagnosis en NO no se contrasta ni se elige: se reformula.

══ EN GUIADO, EL QUE DECIDE ES EL ANALISTA ══
Guiado no es «autónomo contando lo que hace». En cada nodo:
  1. Enseña la evidencia de la herramienta, entera y sin rehacerla.
  2. Propón las alternativas, cada una con la llamada exacta que la ejecuta.
  3. PARA. Tu turno acaba en la pregunta. No llames a otra herramienta hasta
     que el analista conteste, ni elijas por él «la obvia».
Si te pide opinión, dala en una o dos frases y vuelve a parar. Cuando decida,
ejecuta lo que eligió y solo eso, y deja su razón en el modelo
(new_version reason, o guion_annotate después).

══ EL CARRIL AUTÓNOMO: TÚ ERES EL ANALISTA ══
Decides cada nodo tú solo, sin prisa, y cada decisión queda razonada por
escrito: en la reason de new_version (lo que se decidió para llegar a ese
modelo, con los estadísticos) y en guion_annotate lo que se vea después.
- La propuesta de art es EVIDENCIA, no una orden. Si el correlograma dice una
  cosa y la lista otra, manda lo que puedas defender, y di por qué.
- Un empate se resuelve ESTIMANDO: a menos de ~0,05 de similitud o ~2 de AIC,
  estima los dos, decide con los dos delante y deja escrita la razón del
  perdedor.
- No añadas un parámetro no significativo para cerrar un criterio.
- Los anómalos se calibran, no se eliminan.
- NO USES RAMPAS: una rampa es una tendencia determinista desde su fecha que
  la previsión hereda para siempre. Un cambio en la tasa media es una
  pregunta de orden de integración: vuelve al nodo d.
- Con objetivo MULTIVARIANTE la estacionalidad queda determinista: si el MEG
  clasifica una frecuencia como estocástica, no reformules.
Al terminar: el modelo elegido (guion_adopt con su razón), los nodos que más
costó decidir y aquellos en que fuiste en contra de lo que proponía art, con
la evidencia.

══ NUNCA SE CAPA UN AR SIN FACTORIZAR Y CONTRASTAR ANTES ══
Si la PACF enseña un pico en el retardo 6, no se estima (1 − φ₆B⁶) ni un AR
disperso: se estima el AR(6) COMPLETO. Un operador en B^N impone N−1
restricciones sin contrastar, y quita la posibilidad de contrastar
Shin-Fuller por factor. Que los módulos salgan casi iguales es la hipótesis,
no el hallazgo. El orden:
  1. el AR(p) completo, estimado;
  2. ar_factorization: cada factor con su amortiguamiento y su periodo;
  3. factorized_model: el hijo factorizado (misma verosimilitud, cada factor
     con su error típico), y estimarlo;
  4. los contrastes de los factores (formal_tests).

══ LOS CONTRASTES FORMALES ══
- formal_tests(family="integration"): Shin-Fuller, DCD, sobre y
  subdiferenciación. underdifferencing (d−1) se pide con motivo, no se
  ofrece: correr lo que nadie pidió es pre-testing.
- formal_tests(family="seasonality"): la simplificación estacional, el DCD
  estacional y el MEG (los modelos HSM, Hybrid Seasonal Models: determinista
  o estocástica, frecuencia por frecuencia). Ofrécelo marcado
  «(experimental)»: los modelos y los contrastes están publicados; lo nuevo
  es la implementación y los críticos por Monte Carlo. No decidas solo con
  él: contrástalo con Shin-Fuller y con la ACF/PACF.
- EL MEG VA ANTES DE PODAR ARMÓNICOS: su nula ES el armónico determinista, y
  una t baja en un armónico es evidencia de que esa frecuencia es
  estocástica, no de que sobre.
- Cada contraste dice su número y su crítico; no hay veredicto encadenado. Qué
  hacer con ello lo decide el analista (guiado) o tú con la razón escrita
  (autónomo). Sobre un modelo inadecuado no es un contraste débil: no es un
  contraste.

══ LO QUE VE EL ANALISTA ══
plot(what, series, model) abre la ventana de la madre: series (y su
ACF/PACF), mean-sd, histogram, residuals, diagnosis. Es el mismo dibujo que
la madre, y devuelve en texto lo que la ventana enseña. Si tú no ves
imágenes, el analista sí: en guiado, ábrele la ventana del nodo.
full_report: la ficha de confianza (el recorrido, la adecuación, la
reproducibilidad con check, y el coste).

══ LA PREVISIÓN ══
generate_forecast, después de elegir un modelo: fue -f y fuf desde su .pre,
con los parámetros FIJOS, sin tocar el modelo. La tabla da el nivel con su
desviación típica y las variaciones del periodo y anual con las suyas. Prever
no es un nodo: no hay nada que decidir ni que derivar.

══ LO QUE AQUÍ NO HAY (todavía) ══
- boxcox_analysis, intervention_ladder y suggest_intervention_form son del
  EXTRA PYTHON: si no está instalado lo dicen, y se sigue con lo de C (plot
  mean-sd y el dominio; residual_outlier_scan y un hijo por forma con
  write_inp). Lo que estiman lo estiman con el fue de Python; el hijo que
  deja suggest_intervention_form lo estima después fue en C.
- La sobreparametrización (los pares de parámetros con |r| ≥ 0,7) está en
  C: la da diagnose.
- Verificar un óptimo y comparar versiones son de art-python y aún no están
  en atsw-us. Si un nodo los pide, dilo y propón el camino con lo que hay.
  No improvises el resultado como si saliera de una herramienta.

══ REGLAS ══
- Solo dentro del proyecto y solo con estas herramientas: no hay órdenes de
  sistema ni rutas.
- Lo que escribes queda con «por: asistente» en el manifiesto.
- Los gráficos y la ACF/PACF son el instrumento principal; los contrastes
  apoyan, no arbitran.
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
                  mean: str = "", n_harmonics: int = 0, seasonal: bool = False,
                  easter: bool = False, sample: str = "") -> str:
        """La especificación de un modelo SIN ESTIMAR. transform «λ,d,D»
        (0,1,0: log y una diferencia); arma «p,q,P,Q» (sustituye el ARMA);
        add_arma «p,q,P,Q» (multiplica un factor más); interventions como el
        motor las lee («step 10 2008», «impulse 3 2020»): se AÑADEN a las que
        el .inp ya trae —el hijo de new_version hereda las del padre— y una
        igual a otra no se admite; para un modelo SIN una intervención, parte
        (parent) del modelo de antes de ella; mean «free» o un
        número (fija). La estacionalidad determinista, con D = 0: n_harmonics N
        (los pares cos/sin k = 1..N) o seasonal (el paquete entero: los pares y,
        con s par, alter, la de Nyquist); easter, Semana Santa (mensuales). Los
        que el .inp ya trae no se repiten. fue juzga el resultado: si no lo
        acepta, no se cambia nada."""
        def f():
            a = ["especifica", ID("la serie", series), ID("el modelo", model)]
            if transform: a += ["--transformacion", transform]
            if arma:      a += ["--arma", arma]
            if add_arma:  a += ["--anade-arma", add_arma]
            for i in interventions or []:
                a += ["--intervencion", i]
            if mean:      a += ["--media", "libre" if mean == "free" else mean]
            if seasonal:      a += ["--armonicos", "todos"]
            elif n_harmonics: a += ["--armonicos", str(int(n_harmonics))]
            if easter:        a += ["--pascua"]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def estimate_and_diagnose(series: str, model: str, sample: str = "") -> str:
        """Estima con fue un modelo sin estimar: la log-verosimilitud y el dictamen."""
        return _hazlo(lambda: cli.atsw_cli("estima", ID("la serie", series), ID("el modelo", model),
                                           "--por", "asistente", muestra=sample))

    @app.tool()
    def model_equation_display(series: str, model: str, sample: str = "") -> str:
        """El modelo estimado: el bloque de estimaciones del .out tal cual (estimación,
        error típico y número), con la t de cada parámetro y el nombre de cada
        determinista; σ, la log-verosimilitud y de qué hessiano salen los errores
        típicos. Preséntalo verbatim: no rehagas la tabla."""
        return _hazlo(lambda: cli.atsw_cli("ecuacion", ID("la serie", series), ID("el modelo", model),
                                           muestra=sample))

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
    def boxcox_analysis(series: str, sample: str = "") -> str:
        """[extra Python] La λ de Box-Cox, la de art: la correlación media-desviación
        típica con λ=1 y con λ=0 sobre los datos de la serie, y su recomendación.
        Con signos opuestos el estadístico no decide y manda el dominio. Corre en
        un proceso aparte y queda en costes.csv con via: python."""
        def f():
            s = ID("la serie", series)
            return cli.extra("boxcox", {"inp_path": cli.ruta(s, "datos", ".inp", sample)},
                             f"{s}/datos", "boxcox")
        return _hazlo(f)

    @app.tool()
    def intervention_ladder(series: str, model: str, at: int = 0, ventana: int = 0,
                            threshold: float = 3.0, umbral_vecino: float = 0.0,
                            sample: str = "") -> str:
        """[extra Python] La escalera de Ockham de art para un suceso: estima las
        formas rivales en orden (1a escalón permanente, 1b impulso transitorio; 2,
        el episodio con varios ω y el contraste de ganancia) y dice qué justifica
        subir de peldaño. El AIC NO arbitra la subida. model: un modelo ESTIMADO sin
        la intervención; at: la observación (1-based, en los residuos) donde arranca
        el suceso, 0 = el episodio de mayor |z|. No escribe nada: la forma elegida
        se construye después en un hijo con write_inp. art reestima con su fue en
        Python (via: python en costes.csv)."""
        def f():
            s, m = ID("la serie", series), ID("el modelo", model)
            cli.ruta(s, m, ".out", sample)                  # que esté estimado
            args = {"inp_path": cli.ruta(s, m, ".inp", sample), "at": int(at),
                    "ventana": int(ventana), "threshold": float(threshold),
                    "umbral_vecino": float(umbral_vecino)}
            return cli.extra("escalera", args, f"{s}/{m}", "escalera")
        return _hazlo(f)

    @app.tool()
    def suggest_intervention_form(series: str, model: str, reason: str, date: str = "",
                                  form: str = "auto", n_omega: int = 0, n_delta: int = 0,
                                  rehacer: bool = False, modo: str = "guiado",
                                  sample: str = "") -> str:
        """[extra Python] La intervención de art en un HIJO de model (estimado):
        art la construye y el hijo nace con ella, estimado con fue en C. date
        «MM/YYYY», «QN/YYYY» o «YYYY» (vacía: el residuo más extremo); form
        «step», «impulse», «ramp» o «auto» (lo decide la escalera); n_omega,
        cuántos ω —cuántos ESCALONES— (0: automático); n_delta, los δ del
        denominador (impulse con n_delta=1 es ω0/(1−δB)). rehacer=True
        REFORMULA: retira la intervención de ese suceso que el modelo ya trae y
        pone ésta en su lugar; sin él, si ya había una ahí se añade encima y se
        avisa. Es el camino para cambiar o quitar una intervención (write_inp
        solo añade). modo «guiado» o «autonomo» (en autónomo, nada de rampa).
        La razón es obligatoria. Lo de art viene arriba, con SUS números (fue en
        Python); los del proyecto son los de abajo, de fue en C."""
        def f():
            s, m = ID("la serie", series), ID("el modelo", model)
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía: es lo que guarda el linaje")
            pre = cli.ruta(s, m, ".pre", sample)              # que esté estimado
            # art escribe su .inp, su .pre, su .out y su guion junto a
            # output_path: en un temporal. Del proyecto solo entra la
            # especificación; la estimación es la de fue en C (la regla de la
            # madre), y el .pre lo escribe quien estimó.
            with tempfile.TemporaryDirectory() as tmp:
                out = os.path.join(tmp, f"{s}_forma.inp")
                args = {"inp_path": pre, "output_path": out, "date": date, "form": form,
                        "n_omega": int(n_omega), "n_delta": int(n_delta),
                        "rehacer": bool(rehacer), "modo": modo}
                txt = cli.extra("forma", args, f"{s}/{m}", "forma")
                if not os.path.isfile(out):
                    raise cli.NoSePuede("art no ha escrito la especificación: " + txt[:1500])
                hecho = cli.atsw_cli("proyecto", "itera", s, m, "--razon", reason,
                                     "--por", "asistente", muestra=sample)
                hijo = re.search(r"\bm\d+(?=\.inp)", hecho)
                if hijo is None:
                    raise cli.NoSePuede(f"no sé qué modelo nació: {hecho}")
                hijo = hijo.group(0)
                shutil.copyfile(out, cli.ruta(s, hijo, ".inp", sample))
                # art nombra el modelo por su fichero: el nombre es el del hijo
                txt = (txt.replace(os.path.splitext(out)[0], f"{s}/{hijo}").replace(tmp, "")
                       .replace(f"{s}_forma", f"{s}/{hijo}"))
            est = cli.atsw_cli("estima", s, hijo, "--por", "asistente", muestra=sample)
            return (f"{txt}\n\n── En el proyecto ──\n{s}/{hijo}, hijo de {m}, con la "
                    f"intervención de art. Estimado con fue en C:\n{est}")
        return _hazlo(f)

    @app.tool()
    def ar_factorization(series: str, model: str, sample: str = "") -> str:
        """La factorización del AR del modelo estimado: sus raíces y su periodo."""
        return _hazlo(lambda: cli.atsw_cli("raices", "factores", ID("la serie", series),
                                           ID("el modelo", model), muestra=sample))

    @app.tool()
    def factorized_model(series: str, model: str, reason: str, sample: str = "") -> str:
        """Un hijo del modelo estimado con el AR factorizado en factores irreducibles
        (reales y pares complejos), con los valores del padre como semillas: la misma
        verosimilitud, y cada factor con su amortiguamiento y su periodo, con su error
        típico al estimarlo. La razón es obligatoria."""
        def f():
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía: es lo que guarda el linaje")
            return cli.atsw_cli("raices", "factorizado", ID("la serie", series),
                                ID("el modelo", model), "--razon", reason, "--por", "asistente",
                                muestra=sample)
        return _hazlo(f)

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
