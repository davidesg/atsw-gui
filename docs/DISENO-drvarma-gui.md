# DISEÑO — drvarma_gui, el GUI del VARMA, y el menú VARMA de la madre

Es el hito que viene después de drtran_gui (`RELEVO.md` §6) y de la
reorganización de la madre (`ESTUDIO-madre-organizacion.md` §6, «Hito
drvarma_gui»). drtran y drvarma son hermanos: el primero modela series **sin
retroalimentación** (transferencia, un grafo sin ciclos) y el segundo **con
ella** (VARMA: todas las series se explican entre sí). El alumno que sube por
la escalera llega a drvarma desde fue y desde drtran, así que lo que es común
—abrir, estimar, ver el modelo, diagnosticar y prever— tiene que estar en el
mismo sitio, con el mismo icono y el mismo atajo. Solo tiene que aprender lo
nuevo: la puerta diagonal, las CCF de los residuos univariantes, la dinámica
cruzada y la estructura del sistema.

**Lo que este documento NO cambia:**
- **El motor.** drvarma no se toca, salvo defectos, cada uno con su
  `BUG-NNNN-repro` y con el OK del analista (§1.4).
- **El VECM.** Va en el hito drvec_gui; aquí solo se dice qué comparte (§4.6).

Es solo un documento. El código va en las fases de §6, cada una en su PR.

---

## 0. La decisión de fondo: el GUI nuevo sobre la ESCALERA, no sobre el `.inp`

drvarma tiene hoy **dos entradas**, y el GUI actual usa la que está en
retirada:

| | el `.inp` multivariante (el GUI actual) | la escalera (`escalera.c`, drvarma 5.0) |
|---|---|---|
| qué recibe | una tabla de datos crudos; el GUI la transforma (log, escala, ∇, desestacionaliza) y escribe un `.inp` | un **`.pre` de fue por serie**, con su modelo univariante entero |
| la diagonal | se estima de cero, con los mismos órdenes p, q en todas | **es el `.pre` de cada serie**; p y q son solo la dinámica cruzada |
| la puerta | no existe | con p = q = 0 y Σ diagonal, la logL conjunta tiene que ser la suma de las univariantes; si no, **para** (`escalera.c:1251-1299`, sale con 5) |
| códigos de salida | **0 aunque la estimación falle** (§1.4, B1) | 1 uso, 2 fichero que no es de fue, 3 estimación fallida, 4 ventanas que no cuadran, 5 puerta fallida (`escalera.c:1051-1309`) |
| estado | «deprecated since 5.0» (el propio motor lo dice, `escalera.c:1000-1004`) | el camino de sima y de los casos |

Los casos de la madre son **una lista de `.pre` en orden** (`DISENO-casos.md`
§2.3), y el traspaso entre drtran y drvarma es «la misma lista de `.pre`».
**El GUI nuevo se construye sobre la escalera.** La entrada de datos crudos no
se pierde, pero cambia de sitio: la hace la madre (importar series, fue, art)
y, sin proyecto, la ruta U o V de sima (§2.2). drvarma_gui no transforma
datos.

---

## 1. El GUI actual: `engines/drvarma/gui`

### 1.1 Qué hay

| fichero | líneas | qué hace |
|---|---|---|
| `drvarma_gui.c` | 2379 | la ventana, las pestañas, la escritura del `.inp`, lanzar el motor, el visor |
| `seasonal_detection.c` | 1483 | la regresión armónica y el contraste de estacionalidad; **el motor CLI la compila también** (`engines/drvarma/Makefile:120-126`) |
| `vecm.c` / `vecm.h` | 759 / 114 | un VECM estimado dentro del GUI, con GSL |
| `johansen_test.c` / `.h` | 547 / 116 | el contraste de Johansen dentro del GUI, con GSL |

Todo está en inglés. Funciona, y tiene una prueba conducida con 45
comprobaciones (`tests/gui/test_drvarma_gui.c`, con `fake_drvarma.c` como
motor que falla) que corre en la CI (`Makefile` raíz, línea 62).

### 1.2 Pantalla a pantalla

Una ventana con una fila de arriba y un cuaderno de pestañas (`create_ui`,
`drvarma_gui.c:1986-2329`):

- **Arriba:** «Load Data File», la etiqueta de estado y la ruta del ejecutable
  con «Browse…» (`:2000-2018`).
- **Model:** p, q (0-10), «Include mean», «Two-step initialisation».
- **Matrix Structure:** AR diagonal, MA diagonal, covarianza diagonal.
- **Estimation:** método exacto (1) o aproximado (2).
- **Volatility:** `-volexp` (α y ventana) y `-volmov` (ventana).
- **Data Transformation:** log, factor de escala (100 por defecto), d y D.
- **Seasonal Adjustment:** forzar o solo las significativas, «Test
  Seasonality», el periodo s.
- **Forecast:** activar, horizonte L, periodo s, «Run & Forecast».
- **Abajo:** Run DRVARMA · View Output · View .inp · Johansen Test · Estimate
  VECM · Select VAR Order · Quit; el visor de texto debajo.
- **Diálogo de propiedades** al cargar: frecuencia y fecha de inicio
  (`show_data_properties_dialog`, `:1572`).

### 1.3 Qué se queda, qué se reescribe y qué sale

| | destino | por qué |
|---|---|---|
| lanzar el motor y esperar con su código (`run_drvarma`, `:933`; `run_drvarma_win32`, `:825`) | **se reescribe sobre `lib/engine`**, como drtran (`estima.c`) | `lib/engine` ya hace búsqueda del motor (`lib/sitio`), la orden visible, el código de salida y, con la fase 1a del asistente, el coste (`lib/coste`) |
| escribir el `.inp` multivariante (`generate_inp_file`, `:748`) | **sale** | la escalera recibe `.pre` (§0) |
| log, escala, d, D (`preprocess_series*`, `:506-747`) y la desestacionalización | **sale del GUI** | es del modelo univariante de cada serie: va en su `.pre` |
| `seasonal_detection.c` | **se queda donde está**, para el motor | lo compila el CLI; moverlo es tocar el motor (decisión 6) |
| Johansen y VECM dentro del GUI (`johansen_test.c`, `vecm.c`) | **sale de drvarma_gui** | el VEC es de drvec, con su contraste de rango por bootstrap (sima C1); un segundo VECM en el GUI daría otros números (decisión 7) |
| Select VAR Order (`on_select_var_order`, `:1318`) | **se rehace** como parte de la identificación (§2.3): la M(l) de Tiao y Box y los criterios sobre los residuos univariantes | hoy trabaja sobre los datos crudos preprocesados |
| volatilidad (`-volexp`, `-volmov`) | **se queda, en un menú aparte** (§4.2) | es un uso propio del motor que sima no cubre; decisión 8 |
| p, q, Σ diagonal, método | **se quedan**, como la especificación de la escalera | `-diagcov`, `-m` existen en la escalera; AR/MA diagonal se sustituyen por `-links` (§2.3) |
| «Include mean», «Two-step» | **salen** | la media va en cada `.pre`; `-twostep` no existe en la escalera |
| `-redet`, `-fixarma`, `-hessian`, `-lik` | **entran** como opciones avanzadas | son de la escalera y el GUI actual no los conoce |
| el visor de texto | **se sustituye** por `lib/preview` y las páginas de la diagnosis | regla 7 de mtram: lo que se ve es lo que va al papel |
| la prueba conducida | **se rehace** en el estilo de `gui/drtran/tests` | la ventana cambia entera |

### 1.4 Los fallos conocidos, comprobados en el código

| | fallo (`TODO.md`) | dónde | estado |
|---|---|---|---|
| B1 | el motor **sale con 0** cuando la estimación falla, y el GUI lo anuncia como éxito | `src/drvarma.c:457-473` guarda `est_fault` y escribe «ESTIMATION FAILED» en el `.out`, pero `main` acaba en `return 0` (`:725`) sin mirarlo. El GUI confía en el código (`gui/drvarma_gui.c`, `run_drvarma`, ~`:1002-1032`) y dice «DRVARMA finished successfully.» | **abierto, solo en la ruta del `.inp`**. La escalera sale con 3 (`escalera.c:1335`, `:1397`). Con el GUI nuevo sobre la escalera, el GUI deja de depender de ese camino; el defecto del motor sigue y pide su `BUG-NNNN-repro` |
| B2 | «Use differenced data» en Select VAR Order no hace nada | `gui/drvarma_gui.c:1359` lee la casilla; `:1365` llama a `preprocess_series` igual en los dos casos, y `use_diff` solo se usa en la etiqueta de `:1483` | **abierto**. Muere con la pantalla (§1.3) |
| B3 | la conclusión de Johansen se escribe con `strcat` en un búfer fijo | `gui/johansen_test.c:419` reserva 2048 bytes; las líneas de la traza (`:430`) y del máximo autovalor (`:443`) entran con `strcat` sin límite. Con unas 15 series desborda. El informe completo (`:462`) ya usa 16384, con el mismo patrón | **abierto**. Muere con el código (§1.3) |
| B4 | las recargas pierden `base_name`; `transform_data` no se usa | `load_data_file` ya sustituye fichero, `base_name` y nombres al final y solo si el fichero es bueno (`:207-211`, `:295-305`). **La primera mitad parece resuelta.** `transform_data` (`:314`) sigue sin llamarse | **a confirmar con un repro**; la segunda mitad es código muerto |

Ninguno se arregla en este documento. B1 es del motor y necesita OK; B2, B3
y B4 desaparecen con lo que sale del GUI. Hay trabajo en curso del analista
sobre la desestacionalización de drvarma en su worktree propio (rama
`drvarma-bugs-c`); este diseño no la toca y la desestacionalización sale del
GUI de todas formas.

---

## 2. El protocolo de sima, nodo a nodo, en el GUI

sima (sima-python, `~/Dropbox/SRC/sima-python`) es el asistente del VARMA, y su
protocolo está escrito en `src/sima/mcp_server.py:84-185` y en
`docs/TOOLS.md`. Como hizo drtran_gui con mtram (`DISENO-mtram.md`), el GUI se
diseña desde ese protocolo. El ejemplo de referencia es **Jenkins y Alavi**
(ratas almizcleras y visones; `examples/jenkins_alavi`,
`docs/MANUAL-jenkins-alavi.md`), y después Tiao y Box (horno de gas) y
Lütkepohl y Poskitt (precios de la harina).

### 2.1 La regla de mtram se hereda

**Nada se grisa dentro de las páginas; cada página pide por su nombre lo que
le falta y dice quién lo fabrica** (`DISENO-mtram.md`, reglas transversales).
En la barra, la regla de fue: apagado con globo que dice por qué
(`DISENO-barra-drtran.md`, D2).

### 2.2 Los nodos y su sitio

| nodo | sima | qué es | en drvarma_gui | motor |
|---|---|---|---|---|
| N0r | `load_data` | series crudas, sin modelos | **no**: lo hace la madre (Importar series) | — |
| N0c | `characterize` | λ, d, estacionalidad, anómalos de cada serie | **no**: fue y art desde la madre | — |
| N0v | `write_specs` | ruta V: el vector primero | **no** en la primera versión (decisión 4) | — |
| N0u | `build_univariate` | ruta U: los univariantes con art | **no**: es la madre (US) | — |
| N0 | `load_pre` | los ficheros, sus modelos, la ventana común | página **1 · Series** | `lib/fuepre` (lector y alineación) |
| N1 | `run_gate` | la puerta diagonal | página **2 · Puerta** | `drvarma A.pre B.pre … 0 0 -diagcov` |
| N1b | `canonical_analysis` | el análisis canónico de Box y Tiao de los niveles | página 2, como lectura | **no hay C** (Python) — §3.2 |
| N1c | `prepare_vec` | la pregunta de la cointegración | página 2, como **aviso con salida a VECM** | — (drvec) |
| N2 | `identify_cross` | CCF de los residuos del sistema diagonal | página **3 · Identificación** | `lib/prewhiten` + `lib/ccfplot` |
| N2 | `identify_matrices` | las dos identificaciones de Jenkins y Alavi, como matrices; la M(l) de Tiao y Box | página 3 | **parcial** — §3.2 |
| N2 | `identify_echelon` | la forma echelon de Lütkepohl y Poskitt | no en la primera versión | **no hay C** |
| N3 | `estimate` | un candidato: p, q, Σ, `links` | página **4 · Modelo** y **5 · Estimación** | `drvarma … p q [-links] [-diagcov]` |
| N4 | (en `estimate`) / `check_residuals` | LR contra los univariantes, residuos, portmanteau | página **6 · Diagnosis** | `.out` del motor + `lib/diagmv` |
| N4s | `simplify` | coeficientes con \|t\| < 1 a cero, reestimar, LR | página 6, botón «Simplificar» | **orquestación**: varias corridas del motor (§3.2) |
| N4t | `structure` | ¿es simultáneo? Cada par cruzado a cero, y los órdenes triangulares | página 6, «¿Es una red?» → **«Pasar a MT»** | orquestación sobre el motor |
| N4b | `study_estimation` | el ajuste se paró en la pared MA | página 5, aviso con menú | lectura del `.out` (el motor ya lo declara, BUGS.md «pinned to the MA invertibility wall») |
| N5 | `evaluate` | la vara de medir: previsiones desde cada origen contra los univariantes | página **7 · Previsión**, «Evaluar» | `-forecast H -estwin N` → `.recursive` |
| N5 | `forecast_uncertainty` | la tabla VIII de Jenkins y Alavi | página 7 | a comprobar |
| N6 | `forecast` | prever cada serie en su nivel | página 7 | `-forecast H` → `.forecast` |
| N6 | `impulse_response`, `variance_decomposition`, `reorder` | respuesta al impulso y descomposición | página 7 | el `.out` del motor ya las trae (`drvarma.c:486-488`) |
| — | `record_decision`, `export_guion` | la razón de cada paso | la razón de cada corrida del caso y el linaje de la madre | `lib/proyecto` |

**Siete páginas, como drtran_gui** — Series · Puerta · Identificación ·
Modelo · Estimación · Diagnosis · Previsión — y en el mismo orden donde son
hermanas (1, 3-7). La nueva es la **2 · Puerta**: en drtran no existe porque
la transferencia no estima de nuevo la diagonal.

### 2.3 Lo que cambia frente a drtran

- **La página 2, la puerta.** Si la puerta falla, el resto no se puede creer:
  la página lo dice y propone volver a fue con el `.pre` que se movió (sima
  N1, «a file MOVED»). El motor ya escribe la tabla de la puerta en el `.out`
  (`escalera.c:1261-1274`).
- **La identificación es una matriz, no una pareja.** La CCF de residuos
  univariantes se enseña **por par**, con la rejilla m × m de miniaturas y el
  par ampliado al pulsar, en el formato de `lib/ccfplot` (es el de
  GraphMaker y drvus, el mismo que la Figura de Jenkins y Alavi). Lo que se
  saca de la página son los **enlaces**: qué pares tienen dinámica cruzada.
- **El modelo son p, q, Σ y los enlaces.** `-links "A<-B,C<-A"` sustituye a
  «AR diagonal / MA diagonal»: es más fino y es lo que propone N2. Con un
  enlace de ida y vuelta (A←B y B←A) hay retroalimentación, que es lo que
  hace falta para estar aquí y no en MT.
- **La salida a MT.** Si N4t encuentra un orden triangular que se sostiene,
  el sistema es una red: la página lo dice y ofrece «Pasar a MT» (un caso
  hermano con la misma lista de `.pre`, `motor: drtran`). Es la vuelta de
  «Pasar a VARMA» (§4.4).

### 2.4 «Ver modelo»: cada ecuación, como una univariante

**La regla del analista:** «Ver modelo» de un modelo multivariante enseña
cada ecuación **exactamente** con el formato y la tipografía de fue, y con
**un solo dibujante compartido**. En la transferencia, los términos de
transferencia van **antes de los deterministas**. Ese trabajo se está haciendo
ya para MT, en su propia rama (`ver-modelo-mt-como-us`): el dibujante de MT
pasa a ser el de US.

drvarma_gui usa **ese mismo dibujante**, sin uno propio:

- **Una página por serie**, como drtran (`diagnosis_escribe_paginas`, una
  página por ecuación): la ecuación de la serie i escrita como su univariante.
  Lleva su transformación (Box-Cox, ∇), su media, sus deterministas y sus
  factores AR y MA propios, con la misma tipografía y en el mismo orden que
  fue.
- **Los términos cruzados van donde van los de transferencia**, antes de los
  deterministas. En la ecuación de A, lo que B le aporta (φ_AB(B) w_B,t en la
  parte AR y θ_AB(B) a_B,t en la MA) se escribe en ese hueco, con el nombre de
  la serie de la que viene, como una entrada de MT.
- «Ver modelo ▾» lista una entrada por serie, en el orden del caso, que es el
  de Cholesky. El clic abre la de la serie que se está mirando, como en
  drtran.
- La madre ofrece lo mismo («Ver modelo…» y «Ver la ecuación de X…»). Las
  páginas salen del mismo dibujante, así que el GUI, la madre y `atsw-cli`
  dan la misma ecuación.

**Lo que no encaja en una ecuación univariante**, y queda como decisión
abierta (§8, decisión 12):

- **La covarianza contemporánea Σ.** Una ecuación univariante tiene una σ²;
  el sistema tiene una matriz. La propuesta es que cada página lleve su σ_ii
  donde fue pone σ, y que la matriz Σ (o las correlaciones) vaya en una página
  propia del sistema, «Σ y correlaciones», al final de «Ver modelo ▾».
- **El término MA cruzado.** En la transferencia la entrada es una serie
  observada; en el VARMA, θ_AB(B) multiplica una **innovación** de otra serie,
  a_B,t, que no se observa. El hueco de transferencia admite la serie, pero
  hay que decidir cómo se nombra la innovación ajena sin confundirla con la
  propia: por ejemplo a_MINK,t frente a a_t.
- **La forma echelon** (Lütkepohl y Poskitt), si llega: tiene un término
  contemporáneo en el lado izquierdo (Φ_0 ≠ I), que una ecuación univariante
  no tiene. Queda fuera de la primera versión, pero si entra necesitará una
  línea propia en el dibujante.
- **Los factores estacionales de la ruta U** que multiplican la fila entera
  (sima, `identify_echelon`): en la escalera la diagonal es el `.pre` y no se
  da el caso, pero conviene dejarlo apuntado.

---

## 3. Qué se reutiliza de `lib/` y qué falta

### 3.1 Lo que hay

| librería | para qué en drvarma_gui |
|---|---|
| `lib/barra` | la barra y los iconos de fue y drtran (`at_icono`, `at_menu_item`): Estimar · Ver modelo ▾ │ Diagnosis ▾ · Prever ▾ |
| `lib/engine`, `lib/sitio` | lanzar `drvarma`: el del árbol propio primero, el PATH de respaldo; la orden visible; el código de salida |
| `lib/fuepre` | leer cada `.pre` y comprobar la alineación de ventanas (el motor usa la misma, `escalera.c:1081`) |
| `lib/prewhiten` | los residuos univariantes de cada serie, para la CCF de N2 (el motor usa el mismo código) |
| `lib/ccfplot` | la CCF bidireccional, par a par |
| `lib/diagmv` | Hosking y Jarque-Bera multivariantes: ya es de drvarma, drtran y drvec |
| `lib/vuelta` | devolver al univariante el bloque diagonal estimado con la dinámica cruzada, como `.inp` (no `.pre`) |
| `lib/fugplot` (`fp_forecast`), `lib/fugdraw` | el gráfico de la previsión en nivel y variación anual, con bandas teóricas |
| `lib/preview` | el visor de páginas que van al papel |
| `lib/equation` | la ecuación de cada serie, con **el dibujante único** de US y MT (§2.4): los términos cruzados en el hueco de la transferencia |
| `lib/proyecto` | el caso, sus corridas, la razón y el linaje |
| `lib/analisis`, `lib/cuaderno` | las ventanas de análisis con «Al cuaderno» |
| `lib/coste`, `lib/ficha` | cuando existan (fase 1a del asistente, `ESTUDIO-terminal-mcp.md` §7.7-7.8): el coste de cada corrida y la ficha de confianza del modelo |

### 3.2 Lo que falta

| falta | para | propuesta |
|---|---|---|
| **`lib/outvarma`**: el lector del `.out` de la escalera (la puerta, los parámetros, la LR contra los univariantes, la diagnosis, la IRF, la FEVD) | páginas 2, 5, 6 y 7 | como `lib/outdiag` y `lib/outfcst` para drtran: **la librería devuelve el hecho, no la frase**, y la prueba corre contra un `.out` de verdad |
| el lector de `.forecast` y `.recursive` | página 7 | si su formato es el de drtran, `lib/outfcst` se generaliza; si no, una función hermana |
| la **M(l) de Tiao y Box** y las matrices S_k de Jenkins y Alavi | página 3 | a comprobar: hoy está en el porte Python (`drvarma.ladder`) y en sima. Si no hay C, se apunta, no se improvisa; la página 3 arranca con la CCF de residuos, que sí está en C |
| el **análisis canónico** de Box y Tiao | página 2 | no hay C. Primera versión sin él; aviso «hay n series diferenciadas: la pregunta de la cointegración es de VECM» |
| **simplificar** y **estructura** (N4s, N4t) | página 6 | son bucles de corridas del motor con LR. Se escriben en C en el GUI, sobre `lib/engine`, o en `atsw-cli` (§5) para que la madre, el GUI y el MCP usen el mismo; decisión 5 |
| un dibujo de la **rejilla m × m** de CCF | página 3 | `lib/ccfplot` dibuja un par; la rejilla es la composición de miniaturas en GTK |

---

## 4. La integración con la madre

### 4.1 Los casos con `motor: drvarma`

El manifiesto ya tiene el sitio (`DISENO-casos.md` §2.3): el mismo caso, la
misma lista de entradas `SERIE/modelo@sha256` en orden, y `motor: drvarma`.
El orden importa también aquí: es el de Cholesky de la IRF y la FEVD
(`casos_gui.c:225`). Las corridas:

    <raiz>/_casos/C2/work/C2_c01.out   .forecast   .recursive   _links.txt

Los `.pre` de entrada **no se copian**: son los de las series, y el hash dice
si siguen siendo los mismos (un caso desfasado se marca y se deriva, nunca se
reescribe por debajo).

drvarma_gui acepta los mismos argumentos que drtran_gui
(`gui/drtran/src/main.c:46-100`):

    drvarma_gui [--proyecto FICHERO [--caso C [--modelo c]]]

Sin `--proyecto` funciona suelto, con `.pre` abiertos a mano. La madre lo
lanza con `atsw_lanza_caso()` (`gestos.c:210`), que ya es genérica.

### 4.2 El menú VARMA

Va en la barra de la madre después de MT: `Proyecto · Datos · US · MT ·
VARMA · Ayuda`, y más tarde `… · VECM · Asistente · Ayuda`. Con las reglas de
la fase 1:

- el título con la **sigla**, y su globo con el nombre completo: «VARMA —
  modelos vectoriales ARMA, con retroalimentación entre las series (drvarma)»;
- **las mismas funciones constructoras de secciones** para la barra y para el
  menú del botón derecho del caso, como US y MT (la prueba `barra` comprueba
  que coinciden);
- los atajos con `<Primary>`, y en los globos con `atsw_tecla`.

Contenido, por secciones:

    VARMA
    ├─ Nuevo sistema…                 (elige las series y sus modelos, en orden)
    ├─ Pasar a VARMA                  (desde el caso MT marcado; §4.4)
    ├─ ─────────────
    ├─ Estimar c01           Ctrl+E   (abre drvarma_gui con el caso)
    ├─ Ver modelo…
    ├─ Diagnosticar ▸        (la puerta · los residuos · la CCF de residuos · Hosking)
    ├─ Prever ▸              (el gráfico · evaluar contra los univariantes · IRF · FEVD)
    ├─ ─────────────
    ├─ Derivar sistema…
    ├─ Elegir · Razón…
    ├─ Pasar a MT                     (si la estructura lo permite; §2.3)
    ├─ ─────────────
    └─ Borrar modelo… · Borrar sistema…

En la madre, a un caso VARMA se le llama **«sistema»**; a uno MT, «caso» (o
«red»). Decisión 2.

### 4.3 La columna «Clase»

La lista de casos gana una columna **Clase** (MT, VARMA, más tarde VECM),
sacada de `atsw_clase_de()` (`main.c:163`), que ya devuelve VARMA para
`drvarma`. La cabecera de la vista del caso ya dice «MT · drtran · …»
(fase 2) y dirá «VARMA · drvarma · …».

### 4.4 «Pasar a VARMA» desde un caso MT con ciclo

drtran sale con 7 cuando la red tiene un ciclo, e imprime la llamada a sima
(`drtran.c:2313-2324`); la madre también puede verlo en el `.dag` de la
corrida. Entonces:

- la fila del caso y el menú MT ofrecen **«Pasar a VARMA»**;
- crea un **caso hermano** con las mismas entradas, en el mismo orden, y
  `motor: drvarma`, con la razón precargada («la red tiene un ciclo: A ↔ B»);
- el linaje guarda de qué caso viene.

La vuelta, «Pasar a MT», es la misma operación con `motor: drtran`, cuando
N4t encuentra un orden triangular.

### 4.5 Ctrl+E y el linaje

- **Ctrl+E sigue a la pila** (fase 2): modelo US → fue; caso MT → drtran;
  sistema VARMA → drvarma.
- **Cada corrida es una entrada del linaje** con su padre, su razón y, cuando
  exista, `por: asistente` (`ESTUDIO-terminal-mcp.md`, decisión 8), su coste y
  su ficha.
- **«Elegir»** marca el sistema elegido, como con los modelos y los casos.

### 4.6 Lo que se comparte con drvec_gui (VECM)

El VECM va en su hito. Comparte con drvarma_gui:

- la página 1 (Series) y la puerta: drvec también parte de los `.pre`;
- la CCF de residuos y la diagnosis multivariante (`lib/diagmv`);
- la página de previsión, la IRF y la FEVD (en niveles para el VEC);
- la barra y el menú: el menú VECM tendrá la misma forma que el VARMA.

Por eso las páginas 1, 6 y 7 se escriben como **componentes de `lib/`** desde
el principio (`lib/sistema`, nombre a decidir), y no dentro de
`gui/drvarma/src`. La pregunta de la cointegración (N1c) se hace en drvarma_gui
como aviso y salida: «hay n series con d = 1: ¿VECM?». Hasta el hito drvec_gui
la salida abre la nota de que el VECM llegará con su menú.

---

## 5. drvarma_gui y su parte del MCP (`atsw-sis`)

`ESTUDIO-terminal-mcp.md` decide que el MCP crece con la escalera: la parte
VARMA de `atsw-sis` llega **con** drvarma_gui, no antes, para que el asistente
no cree sistemas que la madre no sabe enseñar. Para que lleguen juntos:

| `atsw-sis` necesita | lo da |
|---|---|
| crear un sistema y sus corridas en el proyecto | `atsw-cli proyecto caso-nuevo --motor drvarma`, el mismo `lib/proyecto` que la madre |
| la puerta, estimar, prever | el binario `drvarma` del paquete, en modo escalera (los motores en C primero, decisión 11) |
| leer los resultados | `atsw-cli varma` sobre **`lib/outvarma`** (§3.2): el mismo lector que el GUI |
| la CCF de residuos | `atsw-cli prewhiten` (ya en la fase 1a) |
| simplificar y estructura | la misma orquestación que el GUI (decisión 5) |
| canónico, matrices, echelon | el extra Python (`atsw-gui-mcp[python]`), sobre sima, hasta que haya C |
| abrir las ventanas | `atsw_gui --ver` con los tipos del sistema (la puerta, la CCF, la previsión) |
| el coste y la ficha | `lib/coste` y `lib/ficha`, igual que en US y MT |

La regla de fondo: **todo lo que drvarma_gui sepa hacer tiene que poder
hacerse sin la ventana** (`DISENO-madre.md` §10). Por eso los lectores y la
orquestación van en `lib/` y en `atsw-cli`, y las páginas solo los enseñan.

---

## 6. Plan por fases

Cada fase en su PR, con la CI en verde en Linux, macOS y Windows.

| fase | qué | se ve | pruebas |
|---|---|---|---|
| **V0** | Los repros de B1 (y B4 si se confirma) en `BUG-NNNN-repro/`, sin arreglar | no | el repro sale con 1 mientras el fallo esté abierto |
| **V1** | `lib/outvarma`: el lector del `.out` de la escalera; y el de `.forecast` / `.recursive` | no | contra `.out` **de verdad**, recién escritos por `drvarma` con los `.pre` de `jenkins_alavi` y de un caso de prueba: la puerta, la LR, los parámetros, Hosking |
| **V2** | `gui/drvarma/`: el esqueleto con `--proyecto/--caso/--modelo`, la barra (`lib/barra`), la barra de estado, las páginas 1 (Series) y 2 (Puerta) | sí | `gui/drvarma/tests/test_gui.c` en el estilo de drtran: cargar `.pre`, la ventana común, la puerta que pasa y **la que falla** (un `.pre` retocado: sale 5 y la página lo dice), el motor que no está |
| **V3** | Páginas 3 (Identificación: la rejilla de CCF de residuos y los enlaces) y 4 (Modelo: p, q, Σ, enlaces, opciones avanzadas) | sí | los enlaces elegidos llegan como `-links` exactos; la orden se ve antes de lanzar |
| **V4** | Páginas 5 (Estimación) y 6 (Diagnosis), con la pared MA y la LR contra los univariantes | sí | estimación fallida → la página lo dice (sale 3); un ajuste en la pared no se presenta como óptimo |
| **V4b** | «Ver modelo»: una página por serie con el dibujante de US y MT (§2.4), cuando la rama `ver-modelo-mt-como-us` esté en main | sí | la página de una serie sin enlaces cruzados es **idéntica** a la de su univariante en fue; con enlaces, los términos cruzados quedan antes de los deterministas |
| **V5** | Página 7 (Previsión): el gráfico, la evaluación contra los univariantes, IRF y FEVD | sí | el gráfico es el fichero que va al papel; la evaluación se guarda y la siguiente trae la diferencia |
| **V6** | La madre: menú VARMA, columna Clase, Ctrl+E, «Pasar a VARMA» y «Pasar a MT», el linaje | sí | `gui/atsw/tests/test_gui.c`: fase `varma`; la prueba `barra` cubre el menú VARMA; un caso MT con ciclo ofrece «Pasar a VARMA» y crea el hermano con las mismas entradas en orden |
| **V7** | Simplificar y estructura (N4s, N4t), con la orquestación de la decisión 5 | sí | sobre `jenkins_alavi`, los números de la simplificación coinciden con los de sima |
| **V8** | `atsw-sis` para VARMA (`ESTUDIO-terminal-mcp.md`) | — | la herramienta y la página dan el mismo resultado, byte a byte donde el texto es el mismo |
| **V9** | Retirar `engines/drvarma/gui` del árbol de compilación y de la CI (no `seasonal_detection.c`, que es del motor); el manual: el capítulo drvarma y sus capturas | — | la CI sin `drvarma_gui (ventana)` viejo; `docs/manual` regenerado en el release |

El GUI viejo **sigue compilando y probándose hasta V9**: nadie se queda sin
VARMA mientras el nuevo no lo cubre.

**Validación contra los ejemplos de sima:** en V4-V7, `jenkins_alavi`,
`tiao_box` y `lutkepohl_poskitt` se corren en el GUI y sus números se comparan
con los tutoriales de sima (`examples/*/TUTORIAL.md`): la puerta, la LR y los
parámetros. sima usa el porte Python (`drvarma.ladder`) y el GUI el C; las
diferencias van con la tolerancia que ya usa la CI fuera de Linux.

---

## 7. Maquetas

### 7.1 La ventana

```
┌ drvarma_gui — VARMA · C2 «muskrat y mink» · c01 ───────────────────────────┐
│ [Estimar] [Ver modelo ▾] │ [Diagnosis ▾] [Prever ▾]                         │
├────────────────────────────────────────────────────────────────────────────┤
│ 1·Series │ 2·Puerta │ 3·Identificación │ 4·Modelo │ 5·Estimación │ 6·Diag… │
├────────────────────────────────────────────────────────────────────────────┤
│  LA PUERTA DIAGONAL                                                         │
│                                                                             │
│   serie        logL univariante    del .pre                                 │
│   MUSKRAT          -12.4031         MUSKRAT_m03.pre  ✓                       │
│   MINK              -8.9177         MINK_m02.pre     ✓                       │
│   ─────────────────────────────                                             │
│   suma             -21.3208                                                 │
│   conjunta diag.   -21.3208         diferencia 2e-9   PUERTA SUPERADA ✓     │
│                                                                             │
│  Las dos series están diferenciadas (d = 1).                                │
│  ¿Se mueven juntas a largo plazo? Esa pregunta es del VECM.   [Ir a VECM…]  │
│                                                                             │
│                                          [◀ Series]   [Identificación ▶]    │
├────────────────────────────────────────────────────────────────────────────┤
│ Puerta superada: el sistema diagonal reproduce los univariantes.  Modelo: c01│
└────────────────────────────────────────────────────────────────────────────┘
```

Las cifras de la maqueta son ilustrativas.

### 7.2 La identificación

```
  3 · IDENTIFICACIÓN — CCF de los residuos univariantes
  ┌───────────────┬───────────────┐
  │ MUSKRAT       │ MINK → MUSKRAT│      pulsa un par para ampliarlo
  │  (su ACF)     │  ▁▃█▂▁ k=1    │
  ├───────────────┼───────────────┤      Enlaces propuestos:
  │ MUSKRAT → MINK│ MINK          │       [x] MUSKRAT ← MINK   (k = 1)
  │  ▁▁▂█▃ k=1,2  │  (su ACF)     │       [x] MINK ← MUSKRAT   (k = 1, 2)
  └───────────────┴───────────────┘       ⇒ retroalimentación: VARMA
  Σ: las innovaciones se correlacionan en k = 0 (r = 0,31) → Σ completa
```

### 7.3 El menú VARMA de la madre

```
 Proyecto  Datos  US  MT  VARMA  Ayuda
                          ┌───────────────────────────────┐
                          │ Nuevo sistema…                │
                          │ Pasar a VARMA                 │
                          ├───────────────────────────────┤
                          │ Estimar C2/c01         Ctrl+E │
                          │ Ver modelo…                   │
                          │ Diagnosticar                ▸ │
                          │ Prever                      ▸ │
                          ├───────────────────────────────┤
                          │ Derivar sistema…              │
                          │ Elegir                        │
                          │ Razón…                        │
                          │ Pasar a MT                    │
                          ├───────────────────────────────┤
                          │ Borrar modelo…                │
                          │ Borrar sistema…               │
                          └───────────────────────────────┘
   globo del título: «VARMA — modelos vectoriales ARMA, con
   retroalimentación entre las series (drvarma)»
```

---

## 8. Decisiones abiertas

1. **El GUI nuevo sobre la escalera (`.pre`), no sobre el `.inp`
   multivariante** (§0). Recomendado: sí. La entrada de datos crudos queda en
   la madre (y en sima sin proyecto).
2. **El nombre en la madre: «sistema»** para un caso VARMA (y VECM), «caso»
   para MT. Recomendado: sí; la lista de casos con la columna Clase los
   distingue igual.
3. **Siete páginas**, con la Puerta como la 2 y el resto en el orden de
   drtran. Recomendado: sí.
4. **La ruta V de sima (el vector primero, N0v)** fuera de la primera
   versión. Recomendado: fuera; la escalera es desde los univariantes.
5. **Dónde vive la orquestación de simplificar y estructura** (N4s, N4t):
   en `atsw-cli` (recomendado, la usan el GUI, la madre y el MCP) o dentro del
   GUI.
6. **`seasonal_detection.c` se queda en `engines/drvarma/gui/`** porque lo
   compila el motor; moverlo a `engines/drvarma/src/` es tocar el motor.
   Recomendado: se queda, y se mueve solo con OK, en un PR propio.
7. **Johansen y el VECM del GUI viejo no pasan al nuevo.** El VEC es de drvec,
   con su rango por bootstrap. Recomendado: no pasan.
8. **La volatilidad (`-volexp`, `-volmov`)**: un menú «Volatilidad» aparte en
   drvarma_gui, o fuera hasta que tenga diseño propio. Recomendado: fuera de
   la primera versión; la ruta del `.inp` sigue disponible en el motor.
9. **Componentes compartidos con drvec_gui en `lib/`** (páginas de Series,
   Diagnosis y Previsión) desde V2. Recomendado: sí.
10. **B1 (el motor sale con 0 en la ruta del `.inp`)**: abrir su
    `BUG-NNNN-repro` en V0 y arreglarlo en el motor con OK, aunque el GUI
    nuevo no use esa ruta. Recomendado: sí; otros llaman al motor por ahí.
11. **El GUI viejo hasta V9**: sigue compilando y en la CI hasta que el nuevo
    lo cubra. Recomendado: sí.
12. **«Ver modelo» con el dibujante de US y MT** (§2.4), en lo que no encaja
    en una ecuación univariante:
    - **Σ:** σ_ii en cada página y la matriz en una página propia del sistema
      (recomendado), o la matriz en cada página;
    - **el MA cruzado:** cómo se nombra la innovación de otra serie (a_B,t);
    - **la forma echelon:** fuera de la primera versión; si entra, una línea
      propia para el término contemporáneo.

    La regla de fondo, el mismo formato y la misma tipografía con un solo
    dibujante, ya está decidida por el analista.

---

## 9. Ficheros citados

`engines/drvarma/`: `gui/drvarma_gui.c` · `gui/johansen_test.c` ·
`gui/vecm.c` · `gui/seasonal_detection.c` · `src/drvarma.c` ·
`src/escalera.c` · `Makefile` · `BUGS.md` · `tests/gui/` ·
`docs/DESIGN-v5-ladder.md`.

`gui/`: `atsw/src/gestos.c` · `atsw/src/casos_gui.c` · `atsw/src/main.c` ·
`drtran/src/main.c` · `drtran/tests/`.

sima-python: `src/sima/mcp_server.py` · `docs/TOOLS.md` ·
`docs/MANUAL-jenkins-alavi.md` · `examples/jenkins_alavi`.

Ver también `DISENO-mtram.md`, `DISENO-barra-drtran.md`, `DISENO-casos.md`,
`ESTUDIO-madre-organizacion.md` §6, `ESTUDIO-terminal-mcp.md` y `RELEVO.md`
§6.
