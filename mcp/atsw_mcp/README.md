# atsw-gui-mcp

Los MCP a medida de ATSW: el asistente sobre los motores en C de la madre.
El diseño está en `docs/ESTUDIO-terminal-mcp.md`.

**Fase 0.** Tres servidores, `atsw-us`, `atsw-mt` y `atsw-sis`, que arrancan
y dicen su versión (`version`) y dónde han encontrado el proyecto y los
motores (`estado`). Todavía no analizan ni estiman.

- `ATSW_PROYECTO`: el proyecto. Si no está, se busca `proyecto.yaml` desde la
  carpeta de arranque hacia arriba.
- `ATSW_BIN`: la carpeta de los motores del paquete. Si no está, la de
  `ATSW_GUI`; y sólo al final, el PATH.

Instalar, con las versiones de `uv.lock`:

    uv sync --frozen --no-dev --no-editable                 # el núcleo
    uv sync --frozen --no-dev --no-editable --extra python  # con lo que queda en Python

Prueba de humo, sin cliente de LLM: `python tests/humo.py .venv/bin`.
La prueba a mano con los clientes, en `docs/ENSAYO-clientes.md`.

## Fase 1b: atsw-us

`atsw-us` ya analiza, por el camino en C. Cada herramienta llama a una orden
de `atsw-cli` (las mismas funciones que las ventanas de la madre) o abre una
ventana de la madre con `atsw_gui --ver`. El MTM no escribe el manifiesto ni
los `.inp`: lo hace `atsw-cli`, con `lib/proyecto` y `lib/inpdet`.

| herramienta | qué hace | atsw-cli |
|---|---|---|
| `guion_map` | las series, sus modelos, el linaje y los elegidos | `proyecto ver` |
| `identification_analysis` | lo que propone el identificador art, sobre los datos o los residuos | `identifica` |
| `new_version` | un modelo nuevo, de los datos o de un estimado, con su razón | `proyecto nuevo` / `itera` |
| `write_inp` | la transformación, el ARMA, las intervenciones, los armónicos, Semana Santa y la media de un modelo sin estimar | `especifica` |
| `estimate_and_diagnose` | fue, con su coste, y el dictamen | `estima` |
| `model_equation_display` | el modelo estimado: el bloque del .out con sus t, σ y la log-verosimilitud | `ecuacion` |
| `diagnose` | la diagnosis en veredictos | `dictamen` |
| `formal_tests` | los contrastes formales, por familia | `formales` |
| `residual_outlier_scan` | los episodios y si cambian la identificación | `anomalos` |
| `boxcox_analysis` | [extra] la λ de Box-Cox de art, sobre los datos | art-python, aparte |
| `intervention_ladder` | [extra] la escalera de Ockham de art para un suceso | art-python, aparte |
| `ar_factorization` | la factorización del AR | `raices factores` |
| `factorized_model` | el hijo con el AR factorizado, con su razón | `raices factorizado` |
| `full_report` | la ficha de confianza | `ficha` |
| `generate_forecast` | la previsión, fue -f y fuf desde el .pre, sin tocar el modelo | `prever` |
| `guion_annotate`, `guion_adopt` | dejar escrito el porqué y el elegido | `proyecto razon` / `elige` |
| `plot` | la ventana de la madre, y en texto lo que enseña | `atsw_gui --ver` |

**El extra Python** (`atsw-gui-mcp[python]`) corre en un proceso aparte,
`python -m atsw_mcp.extra`: el canal MCP es la salida estándar del servidor y
art escribe por ella; importarlo tarda; y lo que estima con su fue en Python, si
revienta, revienta el hijo. Llama a la misma herramienta de art con la ruta que
da `atsw-cli proyecto ruta`, devuelve su texto sin las figuras, y apunta su línea
en `costes.csv` con `via: python`. Sin el extra, la herramienta lo dice.

Las instrucciones son el protocolo de nodos de art (los dos carriles, los diez
nodos, el AR que no se capa, los contrastes formales), recortado a estas
herramientas, en 11 KB; dicen también lo que todavía no hay.

Las herramientas y sus argumentos (`series`, `model`, `sample`, `reason`…)
se llaman como sus parejas de art, en Python: la misma orden hace lo mismo en
las dos suites. Los textos que devuelven siguen en español.

Las herramientas reciben series y modelos del proyecto, nunca rutas; lo que
escribe va con `--por asistente`. La prueba, `tests/us.py`, es una sesión MCP
a pelo sobre una copia del ejemplo de Jenkins y Alavi, y corre en la CI de
Linux con los motores del árbol.

## Fase 1b: atsw-mt

`atsw-mt` identifica los enlaces, crea los casos, los estima con drtran
(`atsw-cli caso-estima`, lo que hace «Estimar» en drtran_gui) y los abre en
drtran_gui.

| herramienta | qué hace | sobre |
|---|---|---|
| `guion_map` | las series, sus modelos y los casos | `atsw-cli proyecto ver` |
| `identify_link` | la CCF preblanqueada de cada entrada: b, s, la exogeneidad | `atsw-cli prewhiten` |
| `load_pre` | el caso: la salida primero, sus entradas, lo que se cruza y su razón | `atsw-cli proyecto caso-nuevo` |
| `estimate` | una corrida del caso: con `links`, con la red de otra corrida (`from_run`) o sin red; `constraints`, las líneas del `.cns`; `horizon` y `window`, la previsión y la evaluación frente al diagonal. Devuelve la tabla de parámetros | `atsw-cli caso-estima` |
| `open_case` | el caso en drtran_gui | `drtran_gui --caso` |

Como en atsw-us, los nombres son los de drtran en Python.

Prueba: `tests/mt.py`, también en la CI de Linux.

## atsw-sis: los sistemas (VARMA)

`atsw-sis` lleva los casos de SIM (motor drvarma) con lib/sistema —la misma
orden que arma sim_gui— y lib/outvarma. Primer paso del hito: la parte VARMA.

| herramienta | qué hace | sobre |
|---|---|---|
| `guion_map` | las series, sus modelos y los casos | `atsw-cli proyecto ver` |
| `load_pre` | el caso de SIM, con las series en el orden del sistema (el de Cholesky) | `atsw-cli proyecto caso-nuevo --motor drvarma` |
| `run_gate` | N1, la puerta del diagonal, y las CCF de sus residuos par a par (N2) | `atsw-cli caso-puerta` |
| `estimate` | N3/N4, un VARMA como corrida: `p`, `q`, `diagcov`, `links`, `zeros`, `from_run`; la LR, Hosking, JB y la tabla de drvarma | `atsw-cli caso-estima` |
| `rank_test` | C1, el rango (drvec -lrtest): sin `B`, la asintótica (orientación); con `B`, el bootstrap aparte | `atsw-cli caso-rango [--bootstrap B --aparte]` |
| `job_status`, `job_result` | el trabajo aparte, «CASO/rango»: cómo va, y su resultado final | `atsw-cli caso-rango --estado` |
| `estimate_vec` | C2, el VEC de rango r, con su normalización; se niega sin bootstrap salvo `theory` | `atsw-cli caso-estima --r` |
| `test_restriction` | C3, la exogeneidad débil de cada serie, por LR | `atsw-cli caso-exogeneidad` |
| `simplify` | N4s, los cruzados con \|t\| < 1 a cero, una corrida hija y la LR contra la madre | `atsw-cli caso-simplifica` |
| `structure` | N4t, cada par a cero y cada orden triangular; si es una red, se ofrece MT (`pass_to_mt` solo si lo pide el analista) | `atsw-cli caso-estructura [--pasar-mt]` |
| `open_case` | el caso en sim_gui | `sim_gui --caso` |

Los nombres son los de sima. La evaluación y la previsión llegan en el
paso siguiente. Prueba:
`tests/sis.py`, en la CI de Linux.

## Para la fase 1b: los procesos, con la entrada cerrada

La CI de la fase 0 lo ha medido. **En Windows, un proceso lanzado desde
dentro del servidor que hereda la entrada estándar se cuelga**: la entrada es
la tubería del cliente MCP. Con `stdin=subprocess.DEVNULL` no pasa. En Linux
y macOS no pasa de ninguna de las dos maneras.

Importar `art` dentro del servidor se cuelga igual en Windows, aunque fuera
del servidor tarda entre 1 y 19 s, también en un hilo y con una tubería
abierta. Todo apunta a que ese import lanza un proceso.

Así que en el MTM:

- toda llamada a un motor en C va con `stdin=subprocess.DEVNULL`;
- antes de importar el extra dentro del servidor hay que averiguar qué
  proceso lanza `art` al importarse, o importarlo en un proceso aparte.

Los pasos «diagnóstico» de `.github/workflows/asistente.yml`
(`ATSW_MCP_DIAG=import|sub|sub-devnull`) lo repiten en cada vuelta.
