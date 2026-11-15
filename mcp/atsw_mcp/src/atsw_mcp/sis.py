"""atsw-sis: el servidor de la clase SIM, sistemas simultaneos (docs/ESTUDIO-terminal-mcp.md §2.1).

La parte VARMA (drvarma) y la del VEC (drvec): el rango, con su bootstrap
aparte (caso-rango --aparte), el VEC y la exogeneidad debil. Los casos de SIM del
proyecto, la puerta del sistema diagonal con las CCF de sus residuos (atsw-cli
caso-puerta), y el VARMA como corrida del caso (atsw-cli caso-estima, lo que
hace la pagina 4 de sim_gui), con lib/sistema y lib/outvarma. Las
herramientas y sus argumentos se llaman como las de sima. El VEC, la
simplificacion, la estructura y la prevision llegan en los pasos siguientes;
mientras, el caso se abre en sim_gui.
"""

from __future__ import annotations

from atsw_mcp import cli, comun

SERVIDOR = "atsw-sis"

INSTRUCCIONES = """\
atsw-sis. Eres el asistente de ATSW para SISTEMAS SIMULTÁNEOS: el VARMA por
máxima verosimilitud exacta (motor drvarma, en C), dentro del proyecto de la
madre. Es el tercer peldaño: art (una serie) → mtram (redes de transferencia)
→ sistemas. Es el protocolo de sima, recortado a lo que estas herramientas
hacen. Responde en el idioma del usuario; las salidas del motor vienen en
inglés y las de atsw-cli en español: preséntalas en uno solo.

══ DE DÓNDE PARTES ══
De los modelos univariantes ya estimados de cada serie (sus .pre), hechos con
atsw-us. Cada .pre lleva el modelo entero de su serie (Box-Cox, deterministas,
diferencias, media, ARMA); el VARMA lo deja en su DIAGONAL y lo que añade es
la dinámica CRUZADA. Aquí no se identifican univariantes. Llegas de atsw-us
con varias series, o de atsw-mt cuando la red tiene un CICLO (dos series que
se alimentan una a otra): un ciclo es donde mtram termina.

══ LA PREGUNTA INICIAL ══
«¿Guiado o autónomo?» Los dos carriles recorren LOS MISMOS NODOS; solo cambia
quién decide. En guiado: enseñas la evidencia, propones las alternativas con
su llamada exacta, y PARAS hasta que el analista decide. En autónomo decides
tú, cada nodo con su razón por escrito (la reason de cada corrida).

══ LOS NODOS ══
N0 guion_map, y load_pre(inputs, reason): el caso de SIM con las series en
   el ORDEN del sistema, que es también el de Cholesky de la respuesta al
   impulso: es un supuesto de identificación, dilo.
N1 run_gate(case): la puerta. El sistema diagonal (p = q = 0, Σ diagonal)
   tiene que reproducir la suma de las univariantes, exactamente. SI NO PASA,
   PARA: nada de lo que se estime encima se puede creer. Si una serie «se
   movió» sin recorte de ventana, su fichero era una especificación, no un
   óptimo: dilo, y ofrece volver a atsw-us.
N1c CON DOS O MÁS SERIES CON d = 1, antes del VARMA en diferencias va la
   pregunta de la cointegración. Se ofrece SIEMPRE; decide el analista (en
   autónomo, tú, con la razón escrita y el PROPÓSITO: prever o largo plazo).
   C1 rank_test(case, p, q): la prueba del rango de drvec (r -> r+1). Sin B,
      la asintótica: ORIENTACIÓN, nunca veredicto (en el caso «pre» o 3, el
      habitual, ni siquiera hay valores críticos). EL BOOTSTRAP SE OFRECE
      SIEMPRE (B = 199 a 499; 999 si importa el 1 %), y es lo RECOMENDADO
      cuando hay que decidir el rango: caso sin valores tabulados, n/(M p)
      pequeño, una LR cerca del crítico, o el carril autónomo. Tarda minutos:
      va APARTE; rank_test devuelve el id del trabajo y job_status /
      job_result lo siguen. NADA PARCIAL ES UN RESULTADO: no decidas el rango
      antes de job_result. El analista también puede lanzarlo en sim_gui:
      deja el resultado en el mismo fichero, y rank_test lo lee.
      r = 0: las diferencias están bien, el VARMA (N2). 0 < r < M: el VEC.
      Todas rechazadas: «al menos M-1», una pregunta de raíz unitaria.
   C2 estimate_vec(case, r, p, q, normalise_on): el VEC adoptado. Se niega
      sin un bootstrap terminado que proponga ese r, salvo que el analista lo
      adopte por TEORÍA (theory, que queda escrito). normalise_on nombra las
      r series sobre las que se normaliza beta: otra normalización es OTRO
      ajuste, no un reordenamiento, y la logL del VEC cambia con ella (dos
      VEC solo se comparan si la comparten).
   C3 test_restriction(case, run): la exogeneidad débil de cada serie, alfa_i
      = 0 por LR (nunca el Wald del .out, que depende de la normalización). Una
      serie débilmente exógena no corrige hacia el equilibrio: candidata a
      ENTRADA de un modelo condicional, que es atsw-mt. Ofrécelo; no vayas.
N2 La identificación: run_gate devuelve también las CCF de los residuos del
   diagonal —las innovaciones de cada serie, preblanqueadas por
   construcción—, par a par, con los retardos fuera de banda: k > 0, «A<-B»
   (B entra en la ecuación de A); k < 0, «B<-A»; k = 0, contemporánea (Σ
   completa). Elige los candidatos DE ESTA EVIDENCIA, no de una rejilla.
N3 estimate(case, reason, p, q, diagcov, links, zeros, from_run): un
   candidato, como corrida del caso. links «A<-B, C<-A» restringe la dinámica
   cruzada a los pares que la evidencia señala (sin ellos, todos); zeros
   «AR1[A<-B], MA2[B<-A]», coeficientes a cero por su nombre de la tabla.
N4 La diagnosis viene con la corrida: la LR frente a las univariantes (en
   muestra: necesaria, no suficiente), Hosking y Jarque-Bera multivariantes,
   la raíz inversa mayor y cómo paró el optimizador. Un ajuste PARADO EN LA
   PARED DE INVERTIBILIDAD MA no es un óptimo: nunca lo presentes como tal.
N4s simplify(case, run, t): Tiao y Box. Los cruzados con |t| < t (1 por
   defecto) a cero, una corrida hija, y la LR (y el AIC) contra su padre.
   Varias rondas son normales. Un ajuste en la pared MA no se simplifica.
N4t structure(case, run): ¿es el sistema simultáneo? Cada par a cero y,
   con hasta cuatro series, cada orden triangular, por LR. Si un orden se
   sostiene, es una RED DE TRANSFERENCIA: OFRECE atsw-mt; no vayas solo.
   Solo si el analista lo pide, pass_to_mt=True da de alta el caso de MT (la
   salida primero). El veredicto vale lo que valga el orden del ajuste: con
   orden bajo puede salir una retroalimentación espuria.
Un empate (LR con p cerca de 0,05, o dos candidatos que leen lo mismo) se
resuelve ESTIMANDO LOS DOS, no eligiendo sobre el papel. Nunca decidas nodos
en lote: elegir candidatos antes de ver las CCF es decidir sin la evidencia.

══ LA PRESENTACIÓN ══
Cada corrida devuelve la tabla de parámetros de drvarma: preséntala TAL CUAL,
en un bloque de código; nunca la rehagas. Los cruzados se llaman AR1[A<-B],
MA1[A<-B]; Q[A,B] es la covarianza de las innovaciones.

══ LO QUE AQUÍ NO HAY (todavía) ══
La evaluación fuera de muestra (el patrón de medida), la previsión y la respuesta al impulso con sus bandas.
Llegan en los pasos siguientes. Mientras, open_case abre el caso en sim_gui,
donde el analista los tiene. No improvises sus resultados.

══ REGLAS ══
1. El modelo univariante es el patrón: un VARMA que no prevé mejor que las
   univariantes fuera de muestra no tiene razón de ser, por significativos
   que sean sus cruzados en muestra. Es un hallazgo, no un fracaso.
2. La puerta no es opcional.
3. Las herramientas no deciden: enseñan evidencia y un menú.
4. No podes un enlace ni quites una serie para mejorar un número.
5. SIN BOOTSTRAP TERMINADO NO HAY RANGO, ni beta, ni equilibrio, salvo un r
   adoptado por teoría y escrito como tal.
6. Solo dentro del proyecto y con estas herramientas; lo que escribes queda
   con «por: asistente».
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


def _lista(txt: str) -> list[str]:
    return [x.strip() for x in (txt or "").split(",") if x.strip()]


def crea():
    app = comun.crea_servidor(SERVIDOR, "sis", "SIM", INSTRUCCIONES)

    @app.tool()
    def guion_map() -> str:
        """El mapa del proyecto: las series, sus modelos estimados y los casos que ya
        hay, con su motor (drvarma: SIM) y sus corridas (atsw-cli proyecto ver)."""
        return _hazlo(cli.atsw_cli, "proyecto", "ver")

    @app.tool()
    def load_pre(inputs: list[str], reason: str, title: str = "", sample: str = "") -> str:
        """Un caso de SIM: las series (SERIE/MODELO, modelos estimados) en el ORDEN del
        sistema, que es el de Cholesky de la respuesta al impulso, con el sha256 de
        cada .pre. La razón es obligatoria."""
        def f():
            if len(inputs) < 2:
                raise cli.NoSePuede("un sistema cruza dos series o más")
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            a = ["proyecto", "caso-nuevo"] + [_clave("la serie", e) for e in inputs]
            a += ["--motor", "drvarma", "--razon", reason]
            if title:
                a += ["--titulo", title]
            return cli.atsw_cli(*a, muestra=sample)
        return _hazlo(f)

    @app.tool()
    def run_gate(case: str) -> str:
        """N1, la puerta: el sistema diagonal tiene que reproducir la suma de las
        univariantes; si no, se para. Devuelve el bloque de la puerta de drvarma tal
        cual (cada serie, si se movió y por qué) y las CCF de los residuos del diagonal
        par a par, con los retardos fuera de banda: la evidencia de N2. Es un artefacto
        del caso, no una corrida."""
        return _hazlo(lambda: cli.atsw_cli("caso-puerta", cli.comprueba_id("el caso", case),
                                           "--por", "asistente"))

    @app.tool()
    def estimate(case: str, reason: str, p: int = 1, q: int = 0, diagcov: bool = False,
                 links: str = "", zeros: str = "", from_run: str = "") -> str:
        """N3/N4, un candidato VARMA como corrida del caso (drvarma), tras la puerta.
        p, q: la dinámica cruzada; diagcov: Σ diagonal; links «A<-B, C<-A»: los pares
        con dinámica cruzada (sin ellos, todos); zeros «AR1[A<-B], MA2[B<-A]»: a cero
        por su nombre; from_run: la corrida de la que sale. Devuelve el optimizador, la
        log-verosimilitud, la LR frente a las univariantes, Hosking y Jarque-Bera, la
        raíz inversa mayor y la tabla de parámetros de drvarma. Si drvarma no deja
        resultados, no queda la corrida. La razón es obligatoria."""
        def f():
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            a = ["caso-estima", cli.comprueba_id("el caso", case),
                 "--p", str(int(p)), "--q", str(int(q))]
            if diagcov:
                a += ["--diagcov"]
            for l in _lista(links):
                a += ["--enlace", l.replace(" ", "")]
            for z in _lista(zeros):
                a += ["--cero", z.replace(" ", "")]
            if from_run:
                a += ["--desde", cli.comprueba_id("la corrida", from_run)]
            a += ["--razon", reason, "--por", "asistente"]
            return cli.atsw_cli(*a)
        return _hazlo(f)

    def _rango_json(case: str) -> dict:
        import json
        try:
            return json.loads(cli.atsw_cli("caso-rango", case, "--estado", "--json"))
        except (cli.NoSePuede, ValueError):
            return {}

    @app.tool()
    def rank_test(case: str, p: int = 1, q: int = 0, case_det: str = "", B: int = 0,
                  redo: bool = False) -> str:
        """C1, el rango de cointegración (drvec -lrtest, r -> r+1) de un caso de SIM con
        todas sus series I(1). Sin B, la asintótica: orientación, nunca veredicto. Con B
        (199 a 499; el bootstrap es lo que DECIDE el rango, y se ofrece siempre), lanza
        el bootstrap APARTE y devuelve el id del trabajo, «CASO/rango»: síguelo con
        job_status y recoge el resultado con job_result. Un bootstrap ya terminado con la
        misma configuración (hecho aquí o en sim_gui) no se repite sin redo.
        case_det: 1, 2, 3 o pre (el de cada .pre, por defecto)."""
        def f():
            c = cli.comprueba_id("el caso", case)
            a = ["caso-rango", c, "--p", str(int(p)), "--q", str(int(q))]
            if case_det:
                a += ["--caso-det", cli.comprueba_id("el caso determinista", case_det)]
            if B:
                a += ["--bootstrap", str(int(B)), "--aparte"]
            if redo:
                a += ["--rehacer"]
            a += ["--por", "asistente"]
            dice = cli.atsw_cli(*a)
            if B and "lanzado" in dice:
                dice += f"\n\nid del trabajo: {c}/rango"
            return dice
        return _hazlo(f)

    def _caso_del_trabajo(job_id: str) -> str:
        if not job_id.endswith("/rango"):
            raise cli.NoSePuede(f"«{job_id}» no es un trabajo: es CASO/rango («C1/rango»)")
        return cli.comprueba_id("el caso", job_id.rsplit("/", 1)[0])

    @app.tool()
    def job_status(job_id: str) -> str:
        """El estado de un trabajo aparte («C1/rango», el bootstrap del rango): en marcha,
        con cuántas réplicas van y cuánto falta, o terminado (el resultado, con
        job_result). Nunca un número provisional."""
        def f():
            c = _caso_del_trabajo(job_id)
            d = _rango_json(c)
            if d.get("terminado"):
                return f"{job_id}: terminado. El resultado, con job_result."
            return cli.atsw_cli("caso-rango", c, "--estado")
        return _hazlo(f)

    @app.tool()
    def job_result(job_id: str) -> str:
        """El resultado FINAL de un trabajo terminado: la tabla del rango con el
        bootstrap y el rango propuesto. Si no ha terminado, lo dice y no da nada
        parcial: una decisión espera a esto."""
        def f():
            c = _caso_del_trabajo(job_id)
            d = _rango_json(c)
            if not d.get("terminado"):
                return f"{job_id} no ha terminado: no hay resultado. job_status dice cómo va."
            return cli.atsw_cli("caso-rango", c, "--estado")
        return _hazlo(f)

    @app.tool()
    def estimate_vec(case: str, r: int, reason: str, p: int = 2, q: int = 0,
                     normalise_on: str = "", case_det: str = "", theory: str = "",
                     from_run: str = "") -> str:
        """C2, el VEC de rango r como corrida del caso (drvec, con el hessiano en el
        óptimo), tras la puerta. Se NIEGA sin un bootstrap terminado (rank_test) que
        proponga ese r, salvo que el analista lo adopte por TEORÍA: theory dice por
        qué, y queda escrito en la razón. p, q: el VARMA sobre las series EN NIVELES (p
        >= 1). normalise_on: las r series sobre las que se normaliza beta («LRM» o «LRM,
        IDE»); sin ella, las r últimas del caso. Devuelve el optimizador, la logL, AIC,
        BIC, Hosking, JB y la tabla de drvec entera (alfa, beta, Gamma, Q)."""
        def f():
            c = cli.comprueba_id("el caso", case)
            if not reason.strip():
                raise cli.NoSePuede("la razón no puede ir vacía")
            d = _rango_json(c)
            rp = d.get("propuesto") if d.get("terminado") and d.get("bootstrap") else None
            razon = reason
            if rp != int(r):
                if not theory.strip():
                    hay = (f"el bootstrap propone r = {rp}" if rp is not None
                           else "no hay un bootstrap del rango terminado")
                    raise cli.NoSePuede(f"{hay}: un VEC con r = {r} necesita el bootstrap (rank_test con B) "
                                        "o que el analista lo adopte por teoría (theory)")
                razon = f"{reason} [r = {r} adoptado por TEORÍA: {theory}; bootstrap: " + \
                        (f"r = {rp}]" if rp is not None else "no hecho]")
            a = ["caso-estima", c, "--r", str(int(r)), "--p", str(int(p)), "--q", str(int(q))]
            if normalise_on:
                a += ["--normaliza", normalise_on]
            if case_det:
                a += ["--caso-det", cli.comprueba_id("el caso determinista", case_det)]
            if from_run:
                a += ["--desde", cli.comprueba_id("la corrida", from_run)]
            a += ["--razon", razon, "--por", "asistente"]
            return cli.atsw_cli(*a)
        return _hazlo(f)

    @app.tool()
    def test_restriction(case: str, run: str) -> str:
        """C3, la exogeneidad débil: para cada serie, alfa_i = 0 contra el VEC libre de
        la corrida run, con su normalización, por LR (drvec -weakex). p > 0,05: la serie
        no corrige hacia el equilibrio, candidata a entrada de un modelo condicional
        (atsw-mt)."""
        return _hazlo(lambda: cli.atsw_cli("caso-exogeneidad", cli.comprueba_id("el caso", case),
                                           cli.comprueba_id("la corrida", run), "--por", "asistente"))

    @app.tool()
    def simplify(case: str, run: str, reason: str = "", t: float = 1.0) -> str:
        """N4s, la simplificación de Tiao y Box (1981, §4) sobre el VARMA de la corrida
        run: los coeficientes cruzados con |t| < t a cero (además de los que ya
        estaban), una corrida hija, y la LR contra run, con el AIC. Varias rondas son
        normales. Devuelve lo mismo que estimate, con la línea «contra run»."""
        def f():
            a = ["caso-simplifica", cli.comprueba_id("el caso", case), cli.comprueba_id("la corrida", run),
                 "--t", repr(float(t)), "--por", "asistente"]
            if reason.strip():
                a += ["--razon", reason]
            return cli.atsw_cli(*a)
        return _hazlo(f)

    @app.tool()
    def structure(case: str, run: str, pass_to_mt: bool = False) -> str:
        """N4t, ¿es el sistema simultáneo? (Tiao y Box, §3.1, §5.2.) Sobre el VARMA de
        la corrida run: la LR de cada par a cero (B no entra en la ecuación de A) y, con
        hasta cuatro series, la de cada orden triangular. Si un orden se sostiene, es una
        red de transferencia: se OFRECE atsw-mt. pass_to_mt=True, solo si el analista lo
        pide, da de alta el caso de MT con la salida primero."""
        def f():
            a = ["caso-estructura", cli.comprueba_id("el caso", case), cli.comprueba_id("la corrida", run),
                 "--por", "asistente"]
            if pass_to_mt:
                a += ["--pasar-mt"]
            return cli.atsw_cli(*a)
        return _hazlo(f)

    @app.tool()
    def open_case(case: str, run: str = "") -> str:
        """Abre el caso en sim_gui, donde están el rango y el VEC, la simplificación,
        la estructura, la evaluación y la previsión. run: una corrida para cargar."""
        return _hazlo(cli.abre_sim, case, run)

    return app


def main() -> None:
    comun.principal(crea, INSTRUCCIONES)


if __name__ == "__main__":
    main()
