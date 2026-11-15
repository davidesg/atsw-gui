"""atsw-sis: el servidor de la clase SIM, sistemas simultaneos (docs/ESTUDIO-terminal-mcp.md §2.1).

Primer paso del hito: la parte VARMA (drvarma). Los casos de SIM del
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
   CON DOS O MÁS SERIES CON d = 1, antes del VARMA en diferencias va la
   pregunta de la cointegración (el rango, el VEC): aún no está en atsw-sis.
   Dilo, y ofrece hacerla en sim_gui (open_case) antes de seguir.
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
Un empate (LR con p cerca de 0,05, o dos candidatos que leen lo mismo) se
resuelve ESTIMANDO LOS DOS, no eligiendo sobre el papel. Nunca decidas nodos
en lote: elegir candidatos antes de ver las CCF es decidir sin la evidencia.

══ LA PRESENTACIÓN ══
Cada corrida devuelve la tabla de parámetros de drvarma: preséntala TAL CUAL,
en un bloque de código; nunca la rehagas. Los cruzados se llaman AR1[A<-B],
MA1[A<-B]; Q[A,B] es la covarianza de las innovaciones.

══ LO QUE AQUÍ NO HAY (todavía) ══
El VEC y el rango de cointegración, la simplificación de Tiao y Box, la
pregunta de si el sistema es una red, la evaluación fuera de muestra (el
patrón de medida), la previsión y la respuesta al impulso con sus bandas.
Llegan en los pasos siguientes. Mientras, open_case abre el caso en sim_gui,
donde el analista los tiene. No improvises sus resultados.

══ REGLAS ══
1. El modelo univariante es el patrón: un VARMA que no prevé mejor que las
   univariantes fuera de muestra no tiene razón de ser, por significativos
   que sean sus cruzados en muestra. Es un hallazgo, no un fracaso.
2. La puerta no es opcional.
3. Las herramientas no deciden: enseñan evidencia y un menú.
4. No podes un enlace ni quites una serie para mejorar un número.
5. Solo dentro del proyecto y con estas herramientas; lo que escribes queda
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
