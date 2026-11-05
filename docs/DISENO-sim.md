# DISEÑO — SIM: los sistemas simultáneos en la madre (VARMA y VEC)

Es el hito que viene después de drtran_gui (`RELEVO.md` §6) y de la
reorganización de la madre (`ESTUDIO-madre-organizacion.md` §6). Antes se
llamaba «hito drvarma_gui»; ahora es **SIM**, porque recoge los dos motores de
sistemas, como sima.

drtran y drvarma son hermanos: el primero modela series **sin
retroalimentación** (transferencia, un grafo sin ciclos) y el segundo **con
ella** (VARMA: todas las series se explican entre sí). drvec es el VARMA de
series **cointegradas**, en niveles. El alumno que sube por la escalera llega
a SIM desde US y desde MT. Lo que es común —abrir, estimar, ver el modelo,
diagnosticar y prever— tiene que estar en el mismo sitio, con el mismo icono y
el mismo atajo. Solo tiene que aprender lo nuevo: la puerta diagonal, las CCF
de los residuos univariantes, la dinámica cruzada, la estructura del sistema
y, con el VEC, el rango de cointegración.

**Decidido por el analista (2026-11-05):**
- **Un solo GUI y un solo menú, SIM**, para VARMA y VEC, como sima (§0.2).
- **La ruta V** (el vector primero, Tiao y Box) va en la **segunda versión**
  (§8).

**Lo que este documento NO cambia:**
- **Los motores.** drvarma y drvec no se tocan, salvo defectos reales, cada uno
  con su `BUG-NNNN-repro` y con el OK del analista (§1.4).

Es solo un documento. El código va en las fases de §7, cada una en su PR.

---

## 0. Las tres decisiones de fondo

### 0.1 El GUI nuevo sobre la ESCALERA, no sobre el `.inp`

drvarma tiene hoy **dos entradas**, y el GUI actual usa la que está en
retirada:

| | el `.inp` multivariante (el GUI actual) | la escalera (`escalera.c`, drvarma 5.0) |
|---|---|---|
| qué recibe | una tabla de datos crudos; el GUI la transforma (log, escala, ∇, desestacionaliza) y escribe un `.inp` | **un fichero de fue por serie** (`.pre` o `.inp`), con su modelo univariante entero |
| la diagonal | se estima de cero, con los mismos órdenes p, q en todas | **es el modelo de cada fichero**, reestimado con el resto (`escalera.c:1183`, «re-estimated jointly»; `-fixarma` la congela); p y q son solo la dinámica cruzada |
| la puerta | no existe | con p = q = 0 y Σ diagonal, la logL conjunta tiene que ser la suma de las univariantes; si no, **para** (`escalera.c:1251-1299`, sale con 5) |
| códigos de salida | **0 aunque la estimación falle** (§1.4, B1) | 1 uso, 2 fichero que no es de fue, 3 estimación fallida, 4 ventanas que no cuadran, 5 puerta fallida (`escalera.c:1051-1309`) |
| estado | «deprecated since 5.0» (el propio motor lo dice, `escalera.c:1000-1004`) | el camino de sima y de los casos |

drvec también parte de un fichero de fue por serie. **Un sistema es una lista
ordenada de ficheros de fue**, y es lo mismo que un caso MT
(`DISENO-casos.md` §2.3): el traspaso entre MT y SIM es «la misma lista».
**El GUI nuevo se construye sobre la escalera** y no transforma datos.

### 0.2 Un solo GUI de sistemas, como sima

sima es el asistente de los dos motores. En una sesión y un guion lleva a la
vez el VARMA y el VEC: **cada ajuste lleva su clase**, VARMA o VEC, y las
herramientas del final (prever, la respuesta al impulso, la descomposición de
la varianza, evaluar, reordenar) actúan sobre el ajuste vigente.

SIM hace lo mismo, por la misma razón: **VARMA o VEC no se sabe al entrar.**
Se decide dentro del análisis, en N1c (la pregunta de la cointegración) y con
el contraste de rango, sobre la misma lista de ficheros. Con dos programas, el
usuario tendría que elegir antes de tener la evidencia.

- **Un GUI hijo**, `gui/sim/` (binario `sim_gui`, nombre a confirmar,
  decisión 1), con los dos motores detrás.
- **Un menú en la madre, SIM**, que sustituye a los dos que se habían previsto
  (VARMA y VECM).
- **La clase va en cada ajuste**, no en el menú: la columna Clase de la lista
  dice VARMA o VEC.
- **La primera versión es solo VARMA.** La página del rango existe y dice que
  el VEC llegará con su fase (§7, S10).

### 0.3 Las entradas: todas acaban en la misma lista

| entrada | quién fabrica los ficheros | versión |
|---|---|---|
| **Escalera**, la de siempre | los modelos US que el analista construyó y **eligió** en la madre, uno por serie y en el orden que él decide, como un caso MT de drtran; o «Pasar a SIM» desde un caso MT con ciclo (§5.4) | **primera** |
| **Crudo, ruta U** (Jenkins y Alavi: los univariantes primero) | para las series que todavía no tienen modelo: un **modelo de partida** de cada una, propuesto por el identificador de la madre y estimado con fue (§5.5); queda en la lista US marcado «sin revisar» | **primera** |
| **Crudo, ruta V** (Tiao y Box: el vector primero) | un `.inp` por serie con solo su transformación (λ, d, D o armónicos, la media si d = D = 0) y un AR(p)/MA(q) libre en la diagonal (sima `write_specs`) | **segunda** (§8) |

**La ruta U no sustituye a la escalera, la completa.** Lo normal es que el
analista ya haya construido y elegido sus univariantes en US, y entonces el
sistema se hace con ellos, igual que un caso MT. La ruta U solo rellena las
series que aún no tienen modelo, y se pueden mezclar: en un mismo sistema,
unas series con su modelo elegido y otras con un modelo de partida.

**Los modelos de partida los hace la madre con los motores en C**: el
identificador de US → «Identificar la estructura ARIMA…» (el motor `art` en
C, `engines/art`) y fue para estimar. No es art-python ni el `build_univariate`
de sima, que hace lo mismo en Python; donde los dos calculen distinto, manda
la madre (la regla de la madre del MTM).

El motor en C ya sirve para las tres: la ruta V cabe porque la escalera
reestima la diagonal. La ruta V espera porque necesita dos cosas que hoy solo
existen en Python: la vara de medir de N5 (univariantes con los que
compararse) y la identificación sobre el vector (el análisis canónico y la
M(l) de Tiao y Box, §4.2).

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
| lanzar el motor y esperar con su código (`run_drvarma`, `:933`; `run_drvarma_win32`, `:825`) | **se reescribe sobre `lib/engine`**, como drtran (`estima.c`) | `lib/engine` ya busca el motor (`lib/sitio`), enseña la orden, lee el código de salida, lanza sin consola en Windows (madre BUG-0001) y para los hijos al salir |
| escribir el `.inp` multivariante (`generate_inp_file`, `:748`) | **sale** | la escalera recibe ficheros de fue (§0.1) |
| log, escala, d, D (`preprocess_series*`, `:506-747`) y la desestacionalización | **sale del GUI** | es del modelo univariante de cada serie: va en su fichero |
| `seasonal_detection.c` | **se queda donde está**, para el motor | lo compila el CLI; moverlo es tocar el motor (decisión 7) |
| Johansen y VECM dentro del GUI (`johansen_test.c`, `vecm.c`) | **sale** | el VEC de SIM es drvec, con su contraste de rango por bootstrap; un segundo VECM daría otros números (decisión 8) |
| Select VAR Order (`on_select_var_order`, `:1318`) | **se rehace** como parte de la identificación (§3.3) | hoy trabaja sobre los datos crudos preprocesados |
| volatilidad (`-volexp`, `-volmov`) | **fuera de la primera versión** | uso propio del motor que sima no cubre; la ruta del `.inp` sigue en el motor (decisión 9) |
| p, q, Σ diagonal, método | **se quedan**, como la especificación de la escalera | `-diagcov`, `-m` existen en la escalera; AR/MA diagonal se sustituyen por `-links` (§3.3) |
| «Include mean», «Two-step» | **salen** | la media va en cada fichero; `-twostep` no existe en la escalera |
| `-redet`, `-fixarma`, `-hessian`, `-lik` | **entran** como opciones avanzadas | son de la escalera y el GUI actual no los conoce |
| el visor de texto | **se sustituye** por `lib/preview` y las páginas de la diagnosis | regla 7 de mtram: lo que se ve es lo que va al papel |
| la prueba conducida | **se rehace** en el estilo de `gui/drtran/tests` | la ventana cambia entera |

### 1.4 Los fallos conocidos, comprobados en el código

| | fallo (`TODO.md`) | dónde | estado |
|---|---|---|---|
| B1 | el motor **sale con 0** cuando la estimación falla, y el GUI lo anuncia como éxito | `src/drvarma.c:457-473` guarda `est_fault` y escribe «ESTIMATION FAILED» en el `.out`, pero `main` acaba en `return 0` (`:725`) sin mirarlo. El GUI confía en el código (`run_drvarma`, ~`:1002-1032`) | **abierto, solo en la ruta del `.inp`**: es drvarma **BUG-0003**, con repro (S0). La escalera sale con 3 (`escalera.c:1335`, `:1397`). SIM no usa ese camino |
| B2 | «Use differenced data» en Select VAR Order no hace nada | `drvarma_gui.c:1359` lee la casilla; `:1365` llama a `preprocess_series` igual en los dos casos | **abierto**. Muere con la pantalla |
| B3 | la conclusión de Johansen se escribe con `strcat` en un búfer fijo | `johansen_test.c:419` reserva 2048 bytes; `:430` y `:443` entran con `strcat` sin límite | **abierto**. Muere con el código |
| B4 | las recargas pierden `base_name`; `transform_data` no se usa | `load_data_file` ya sustituye fichero, `base_name` y nombres solo si el fichero es bueno (`:207-211`, `:295-305`); `transform_data` (`:314`) sigue sin llamarse | **cerrado** desde 1915e6e: lo comprueba `prueba_carga` del GUI viejo (un fichero malo tras uno bueno, y luego Run); `transform_data` es código muerto (S0) |

Además, abiertos y con repro: drvarma BUG-0001 (la cabecera del `-split`) y
BUG-0002 (`NEW_STR(80)` en `drvarma.c:173-177`), del PR #92. Son de la ruta
del `.inp` y del `-split`, que SIM no usa.

Ninguno se arregla en este documento.

---

## 2. El MCP: sima, sin un cuarto asistente

sima (sima-python, `~/Dropbox/SRC/sima-python`) ya es el asistente de drvarma
**y** de drvec (decidido el 2026-10-13; el VEC entró en sima 0.2.0). No hay un
asistente nuevo para SIM: la parte de sistemas de `atsw-sis`
(`ESTUDIO-terminal-mcp.md`) llega **con** el GUI y no antes, para que el
asistente no cree sistemas que la madre no sabe enseñar.

**La regla de fondo: todo lo que SIM sepa hacer tiene que poder hacerse sin la
ventana** (`DISENO-madre.md` §10). Los lectores y la orquestación van en
`lib/` y en `atsw-cli`; las páginas solo los enseñan.

| `atsw-sis` necesita | lo da |
|---|---|
| crear un sistema y sus corridas en el proyecto | `atsw-cli proyecto caso-nuevo --motor drvarma`, el mismo `lib/proyecto` que la madre |
| la puerta, estimar, prever | el binario `drvarma` del paquete, en modo escalera (los motores en C primero) |
| leer los resultados | `atsw-cli sim` sobre **`lib/outvarma`** (§4.2): el mismo lector que el GUI |
| la CCF de residuos | `atsw-cli prewhiten` |
| simplificar y estructura (N4s, N4t) | la orquestación en `atsw-cli`, en C (decisión 5) |
| canónico, matrices, echelon | el extra Python (`atsw-gui-mcp[python]`), sobre sima, hasta que haya C; el GUI no los improvisa |
| abrir las ventanas | `atsw_gui --ver` con los tipos del sistema (la puerta, la CCF, la previsión) |
| el coste y la ficha | `lib/coste` y `lib/ficha`, igual que en US y MT |

Lo que sima hace en Python y el GUI todavía no (canónico, M(l), echelon) se
ofrece en el GUI como **«esto lo hace el asistente»**, con la orden de sima,
y no como un botón gris.

---

## 3. El protocolo de sima, nodo a nodo, en el GUI

El protocolo está en `src/sima/mcp_server.py` y en `docs/TOOLS.md` de
sima-python. Como hizo drtran_gui con mtram (`DISENO-mtram.md`), el GUI se
diseña desde él. El ejemplo de referencia es **Jenkins y Alavi** (ratas
almizcleras y visones; `examples/jenkins_alavi`,
`docs/MANUAL-jenkins-alavi.md`); después Tiao y Box (horno de gas) y
Lütkepohl y Poskitt (precios de la harina). Para el VEC, Johansen y Juselius
(`examples/johansen_juselius`).

### 3.1 La regla de mtram se hereda

**Nada se grisa dentro de las páginas; cada página pide por su nombre lo que
le falta y dice quién lo fabrica** (`DISENO-mtram.md`, reglas transversales).
En la barra, la regla de fue: apagado con globo que dice por qué
(`DISENO-barra-drtran.md`, D2).

### 3.2 Los nodos y su sitio

| nodo | sima | qué es | en SIM | motor |
|---|---|---|---|---|
| N0r | `load_data` | series crudas, sin modelos | la madre: Datos → Importar series | — |
| N0c | `characterize` | λ, d, estacionalidad, anómalos de cada serie | la madre, en «Nuevo sistema…», para las series sin modelo (§5.5) | identificador de la madre (`art` en C) |
| N0u | `build_univariate` | ruta U: los modelos de partida | la madre, mismo diálogo (§5.5) | identificador de la madre (`art` en C) y fue |
| N0v | `write_specs` | ruta V: el vector primero | **segunda versión** (§8) | — |
| N0 | `load_pre` | los ficheros, sus modelos, la ventana común | página **1 · Series** | `lib/fuepre` (lector y alineación) |
| N1 | `run_gate` | la puerta diagonal | página **2 · Puerta** | `drvarma A.pre B.pre … 0 0 -diagcov` |
| N1b | `canonical_analysis` | el análisis canónico de Box y Tiao de los niveles | página 2, «esto lo hace el asistente» | **no hay C** |
| N1c | `prepare_vec` | la pregunta de la cointegración | página 2, aviso con salida a la página **R · Rango** | drvec (S10) |
| N2 | `identify_cross` | CCF de los residuos del sistema diagonal | página **3 · Identificación** | `lib/prewhiten` + `lib/ccfplot` |
| N2 | `identify_matrices` | las dos identificaciones de Jenkins y Alavi como matrices; la M(l) de Tiao y Box | página 3 | **parcial** — §4.2 |
| N2 | `identify_echelon` | la forma echelon de Lütkepohl y Poskitt | «esto lo hace el asistente» | **no hay C** |
| N3 | `estimate` | un candidato: p, q, Σ, `links` | páginas **4 · Modelo** y **5 · Estimación** | `drvarma … p q [-links] [-diagcov]` |
| N4 | (en `estimate`) / `check_residuals` | LR contra los univariantes, residuos, portmanteau | página **6 · Diagnosis** | `.out` del motor + `lib/diagmv` |
| N4s | `simplify` | coeficientes con \|t\| < 1 a cero, reestimar, LR | página 6, «Simplificar» | orquestación (decisión 5) |
| N4t | `structure` | ¿es simultáneo? Cada par cruzado a cero, y los órdenes triangulares | página 6, «¿Es una red?» → **«Pasar a MT»** | orquestación |
| N4b | `study_estimation` | el ajuste se paró en la pared MA | página 5, aviso | lectura del `.out` (el motor lo declara) |
| N5 | `evaluate` | la vara de medir: previsiones desde cada origen contra los univariantes | página **7 · Previsión**, «Evaluar» | `-forecast H -estwin N` → `.recursive` |
| N5 | `forecast_uncertainty` | la tabla VIII de Jenkins y Alavi | página 7 | a comprobar |
| N6 | `forecast` | prever cada serie en su nivel | página 7 | `-forecast H` → `.forecast` |
| N6 | `impulse_response`, `variance_decomposition`, `reorder` | respuesta al impulso y descomposición | página 7 | el `.out` las trae (`drvarma.c:486-488`) |
| C1-C3 | `rank_test`, `estimate_vec`, `long_run`, `test_restriction` | el rango, el VEC, el largo plazo | página **R · Rango** y las 4-7 con clase VEC | drvec (S10) |
| — | `record_decision`, `export_guion` | la razón de cada paso | la razón de cada corrida y el linaje de la madre | `lib/proyecto` |

**Siete páginas, como drtran_gui** — Series · Puerta · Identificación ·
Modelo · Estimación · Diagnosis · Previsión — y en el mismo orden donde son
hermanas (1, 3-7). La nueva es la **2 · Puerta**: en drtran no existe porque
la transferencia no estima de nuevo la diagonal. Con el VEC (S10) entra una
página **R · Rango** entre la Puerta y la Identificación, que solo aparece
cuando hay dos o más series con d = 1.

### 3.3 Lo que cambia frente a drtran

- **La página 2, la puerta.** Si la puerta falla, el resto no se puede creer:
  la página lo dice y propone volver a fue con el fichero que se movió (sima
  N1, «a file MOVED»). El motor ya escribe la tabla de la puerta en el `.out`
  (`escalera.c:1261-1274`).
- **La identificación es una matriz, no una pareja.** La CCF de residuos
  univariantes se enseña **por par**, con la rejilla m × m de miniaturas y el
  par ampliado al pulsar, en el formato de `lib/ccfplot` (el de GraphMaker y
  drvus, el mismo que la figura de Jenkins y Alavi). Lo que se saca de la
  página son los **enlaces**: qué pares tienen dinámica cruzada.
- **El modelo son p, q, Σ y los enlaces.** `-links "A<-B,C<-A"` sustituye a
  «AR diagonal / MA diagonal»: es más fino y es lo que propone N2. Con un
  enlace de ida y vuelta (A←B y B←A) hay retroalimentación, que es lo que
  hace falta para estar aquí y no en MT.
- **La salida a MT.** Si N4t encuentra un orden triangular que se sostiene,
  el sistema es una red: la página lo dice y ofrece «Pasar a MT» (§5.4).

### 3.4 «Ver modelo»: cada ecuación, como una univariante

**La regla del analista:** «Ver modelo» de un modelo multivariante enseña
cada ecuación **exactamente** con el formato y la tipografía de fue, y con
**un solo dibujante**. Ya está hecho para MT (#72): `lib/equation/eqdraw.c`
es la página de fue y de drtran_gui, y la transferencia va en la primera
línea, delante de los deterministas (`EQ_TRANS`).

SIM usa **ese mismo dibujante**, sin uno propio:

- **Una página por serie**, como drtran: la ecuación de la serie i escrita
  como su univariante, con su transformación, su media, sus deterministas y
  sus factores AR y MA propios.
- **Los términos cruzados van donde van los de transferencia**, delante de los
  deterministas, con el nombre de la serie de la que vienen.
- «Ver modelo ▾» lista una entrada por serie, en el orden del sistema, que es
  el de Cholesky.
- La madre ofrece lo mismo; el GUI, la madre y `atsw-cli` dan la misma
  ecuación.

**Lo que no encaja en una ecuación univariante** (decisión 12):

- **La covarianza contemporánea Σ.** Propuesta: cada página lleva su σ_ii
  donde fue pone σ, y la matriz Σ (o las correlaciones) va en una página del
  sistema, «Σ y correlaciones», al final de «Ver modelo ▾».
- **El término MA cruzado.** θ_AB(B) multiplica una **innovación** de otra
  serie, a_B,t, que no se observa. Hay que nombrarla sin confundirla con la
  propia: por ejemplo a_MINK,t frente a a_t.
- **El VEC** (S10): el término de corrección del error, α β′ y_{t−1}, en la
  misma línea que la transferencia, y β en la página del sistema.
- **La forma echelon**, si llega: Φ_0 ≠ I necesita una línea propia.

---

## 4. Qué se reutiliza de `lib/` y qué falta

### 4.1 Lo que hay

| librería | para qué en SIM |
|---|---|
| `lib/barra` | la barra y los iconos de fue y drtran: Estimar · Ver modelo ▾ │ Diagnosis ▾ · Prever ▾ |
| `lib/engine`, `lib/sitio` | lanzar `drvarma` (y `drvec`): el del árbol propio primero, el PATH de respaldo; sin consola en Windows |
| `lib/fuepre` | leer cada fichero y comprobar la alineación de ventanas (el motor usa la misma, `escalera.c:1081`) |
| `lib/prewhiten` | los residuos univariantes de cada serie, para la CCF de N2 |
| `lib/ccfplot` | la CCF bidireccional, par a par |
| `lib/diagmv` | Hosking y Jarque-Bera multivariantes: ya es de drvarma, drtran y drvec |
| `lib/vuelta` | devolver a US el bloque diagonal estimado con la dinámica cruzada, como `.inp` (no `.pre`) |
| `lib/fugplot` (`fp_forecast`), `lib/fugdraw` | el gráfico de la previsión en nivel y variación anual, con bandas teóricas |
| `lib/preview` | el visor de páginas que van al papel |
| `lib/equation` | la ecuación de cada serie con el dibujante único (§3.4) |
| `lib/proyecto` | el sistema, sus corridas, la razón y el linaje |
| `lib/analisis`, `lib/cuaderno` | las ventanas de análisis con «Al cuaderno» |
| `lib/coste`, `lib/ficha` | el coste de cada corrida y la ficha del modelo |

### 4.2 Lo que falta

| falta | para | propuesta |
|---|---|---|
| **`lib/outvarma`**: el lector del `.out` de la escalera (la puerta, los parámetros, la LR, la diagnosis, la IRF, la FEVD) | páginas 2, 5, 6 y 7 | como `lib/outdiag` y `lib/outfcst`: **la librería devuelve el hecho, no la frase**, y la prueba corre contra un `.out` de verdad |
| el lector de `.forecast` y `.recursive` | página 7 | si su formato es el de drtran, se generaliza `lib/outfcst`; si no, una función hermana |
| la **M(l)** de Tiao y Box y las matrices S_k de Jenkins y Alavi | página 3 | hoy en Python (`drvarma.ladder`, sima). Sin C, la página arranca con la CCF de residuos, que sí está en C |
| el **análisis canónico** | página 2 | no hay C: «esto lo hace el asistente» |
| **simplificar** y **estructura** | página 6 | bucles de corridas del motor con LR, en `atsw-cli` (decisión 5) |
| la **rejilla m × m** de CCF | página 3 | `lib/ccfplot` dibuja un par; la rejilla es composición de miniaturas en GTK |
| los **componentes de sistema** (páginas Series, Diagnosis, Previsión) | VARMA y VEC | en `lib/sistema` (nombre a decidir) desde S2, para que la clase VEC no duplique páginas |

---

## 5. La integración con la madre, sin llenarla

### 5.1 La regla: la madre enseña sistemas; los nodos viven en el GUI hijo

Es la misma regla de US y MT: el detalle de un modelo vive en fue_gui y el de
un caso en drtran_gui. La madre gana **tres cosas y nada más**:

1. **un menú, SIM**, con la misma forma que MT (§5.2);
2. **una columna, Clase**, en la lista (§5.3);
3. **una acción contextual, «Pasar a SIM»**, que solo aparece cuando un caso
   MT tiene un ciclo (§5.4).

La puerta, los enlaces, el rango y la estructura **no aparecen en la madre**
más que como estado, en la fila del sistema:

    C2  SIM · VARMA   MUSKRAT, MINK       puerta ✓   c03 elegido

### 5.2 El menú SIM

Va en la barra después de MT: `Proyecto · Datos · US · MT · SIM · Ayuda`.
Con las reglas de la fase 1:

- el título con la **sigla**, y el globo en el formato de los demás:
  «SIM — Sistemas simultáneos, con drvarma (VARMA) y drvec (VEC)\n\nEl
  sistema marcado: varias series que se explican entre sí, cada una con su
  modelo univariante.»;
- **las mismas funciones constructoras de secciones** para la barra y para el
  menú del botón derecho, como US y MT (la prueba `barra` comprueba que
  coinciden);
- los atajos con `<Primary>`, y en los globos con `atsw_tecla`.

Contenido, **las mismas secciones que MT**: lo único nuevo son las dos
primeras entradas.

    SIM
    ├─ Nuevo sistema…                 (desde modelos US, o desde series sin modelo; §5.5)
    ├─ Pasar a SIM                    (desde el caso MT marcado, si tiene ciclo; §5.4)
    ├─ ─────────────
    ├─ Estimar c01           Ctrl+E   (abre sim_gui con el sistema)
    ├─ Diagnosticar ▸        (Ver modelo… · la puerta · los residuos · Hosking)
    ├─ Prever ▸              (el gráfico · evaluar contra los univariantes · IRF · FEVD)
    ├─ ─────────────
    ├─ Derivar sistema…
    ├─ Elegir · Razón…
    ├─ Pasar a MT                     (si la estructura lo permite; §3.3)
    ├─ ─────────────
    └─ Borrar modelo… · Borrar sistema…

La identificación (la rejilla de CCF, los enlaces) y el rango no tienen
entrada en la madre: son páginas de sim_gui.

### 5.3 La columna «Clase» y el nombre

- La lista de casos gana la columna **Clase**: MT, VARMA y, con S10, VEC.
  `atsw_clase_de()` (`main.c:163`) ya devuelve VARMA para `drvarma`.
- La cabecera de la vista dice «SIM · VARMA · drvarma · …», como hoy «MT ·
  drtran · …».
- En la madre, a un caso SIM se le llama **«sistema»**; a uno MT, «caso»
  (decisión 2).
- Un sistema que pasa de VARMA a VEC **no cambia de fila**: cambia la clase
  de sus ajustes.

### 5.4 «Pasar a SIM» y «Pasar a MT»

drtran sale con 7 cuando la red tiene un ciclo e imprime la llamada a sima
(`drtran.c:2313-2324`); la madre también puede verlo en el `.dag` de la
corrida. Entonces:

- la fila del caso y el menú MT ofrecen **«Pasar a SIM»**;
- crea un **sistema hermano** con las mismas entradas, en el mismo orden, y
  `motor: drvarma`, con la razón precargada («la red tiene un ciclo: A ↔ B»);
- el linaje guarda de qué caso viene.

La vuelta, «Pasar a MT», es la misma operación con `motor: drtran`, cuando
N4t encuentra un orden triangular.

### 5.5 «Nuevo sistema…»: de cada serie, su modelo

Un diálogo con una fila por serie, en el orden del sistema (que es el de
Cholesky), y de cada una **qué modelo entra**:

- **Su modelo elegido** (la escalera), que es lo que propone por defecto,
  como hoy un caso MT de drtran. Se puede cambiar por otro de sus modelos.
- **Un modelo de partida** (ruta U), solo si la serie aún no tiene modelo.
  La madre la identifica con su identificador (λ, d, estacionalidad y la
  estructura ARIMA, el motor `art` en C) y estima el modelo con fue. Queda en
  la lista US como un modelo más, con la marca **«sin revisar»** y la razón
  «modelo de partida para el sistema C2», y el sistema arranca sobre él.

Un sistema puede mezclar las dos cosas. Y si todas las series tienen modelo
elegido, el diálogo es el de siempre y la ruta U no aparece.

La marca «sin revisar» se quita cuando el analista revisa ese modelo en fue:
lo deriva o lo elige. sima dice lo mismo de su ruta U: los univariantes de
partida se pueden y se deben revisar.

La ruta V tendrá una tercera pestaña en la segunda versión (§8).

### 5.6 Ctrl+E y el linaje

- **Ctrl+E sigue a la pila**: modelo US → fue; caso MT → drtran; sistema SIM →
  sim_gui.
- **Cada corrida es una entrada del linaje** con su padre, su razón y, cuando
  la haga el asistente, `por: asistente`, con su coste y su ficha.
- **«Elegir»** marca el ajuste elegido, como con los modelos y los casos.

---

## 6. La documentación, fuera de la madre

La madre explica SIM en **una línea**, el globo del menú. Lo demás va donde
se usa:

| dónde | qué |
|---|---|
| el globo del menú SIM y el de «Pasar a SIM» | qué es un sistema y cuándo se llega: «la red tiene un ciclo» |
| cada página de sim_gui | qué pide, qué hace y quién fabrica lo que le falta (regla de mtram, §3.1); los términos nuevos (puerta, enlace, retroalimentación, rango) se explican en el globo del rótulo donde aparecen |
| el manual del alumno (`docs/manual`) | un capítulo **«Sistemas»**, después del de MT, con Jenkins y Alavi de ejemplo trabajado de principio a fin, y sus capturas sacadas por la CI como las de US y MT |
| los ejemplos | los de sima como **proyectos de ejemplo** de la madre: Ayuda → Ejemplos → «Ratas almizcleras y visones» abre un proyecto con los dos modelos US ya hechos y el sistema por estimar. El primero, `jenkins_alavi`; después `tiao_box` y `johansen_juselius` (este con S10) |
| la web | una ficha del capítulo, como la de los vídeos; más adelante, **un tercer vídeo**, autocontenido como los otros dos |

---

## 7. Plan por fases (primera versión)

Cada fase en su PR, con la CI en verde en Linux, macOS y Windows.

| fase | qué | se ve | pruebas |
|---|---|---|---|
| **S0** ✓ | B1 es drvarma BUG-0003, con repro, sin arreglar; B4 estaba cerrado | no | el repro sale con 1 mientras el fallo esté abierto |
| **S1** ✓ | `lib/outvarma`: el lector del `.out` de la escalera; y el de `.forecast` / `.recursive` (el de drtran, `lib/outfcst`, no sirve: otras tablas); la diagnosis, con `lib/outdiag` | no | contra `.out` **de verdad**, recién escritos por `drvarma` con los `.pre` de `jenkins_alavi` y de un caso de prueba: la puerta, la LR, los parámetros, Hosking |
| **S2** ✓ | `gui/sim/`: el esqueleto con `--proyecto/--caso/--modelo`, la barra, la barra de estado, las páginas 1 (Series) y 2 (Puerta), en `lib/sistema` donde se compartan | sí | `gui/sim/tests/test_gui.c` en el estilo de drtran: cargar ficheros, la ventana común, la puerta que pasa y **la que falla** (un `.pre` retocado: sale 5 y la página lo dice), el motor que no está |
| **S3** | Páginas 3 (Identificación: la rejilla de CCF y los enlaces) y 4 (Modelo: p, q, Σ, enlaces, opciones avanzadas) | sí | los enlaces elegidos llegan como `-links` exactos; la orden se ve antes de lanzar |
| **S4** | Páginas 5 (Estimación) y 6 (Diagnosis), con la pared MA y la LR contra los univariantes | sí | estimación fallida → la página lo dice (sale 3); un ajuste en la pared no se presenta como óptimo |
| **S4b** | «Ver modelo»: una página por serie con `lib/equation/eqdraw.c` (§3.4) | sí | la página de una serie sin enlaces es **idéntica** a la de su univariante en fue; con enlaces, los términos cruzados quedan delante de los deterministas |
| **S5** | Página 7 (Previsión): el gráfico, la evaluación contra los univariantes, IRF y FEVD | sí | el gráfico es el fichero que va al papel; la evaluación se guarda y la siguiente trae la diferencia |
| **S6** | La madre: menú SIM, columna Clase, Ctrl+E, «Pasar a SIM» y «Pasar a MT», el linaje | sí | `gui/atsw/tests/test_gui.c`: fase `sim`; la prueba `barra` cubre el menú SIM; un caso MT con ciclo ofrece «Pasar a SIM» y crea el hermano con las mismas entradas en orden |
| **S6b** | «Nuevo sistema…» desde series sin modelo (ruta U, §5.5), con la marca «sin revisar» | sí | sobre las series crudas de `jenkins_alavi`, los modelos de partida y la puerta; un sistema que mezcla un modelo elegido y uno de partida |
| **S7** | Simplificar y estructura (N4s, N4t) en `atsw-cli` | sí | sobre `jenkins_alavi`, los números coinciden con los de sima |
| **S8** | `atsw-sis` para VARMA (`ESTUDIO-terminal-mcp.md`) | — | la herramienta y la página dan el mismo resultado, byte a byte donde el texto es el mismo |
| **S9** | La documentación (§6): el capítulo «Sistemas», el proyecto de ejemplo `jenkins_alavi` en Ayuda → Ejemplos. Retirar `engines/drvarma/gui` del árbol de compilación y de la CI (no `seasonal_detection.c`, que es del motor) | sí | la CI sin el `drvarma_gui` viejo; el manual regenerado en el release |
| **S10** | **El VEC**: la página R · Rango, la clase VEC en las páginas 4-7, drvec detrás, la ecuación con α β′ | sí | sobre `johansen_juselius`, el rango y el VEC coinciden con sima |

El GUI viejo **sigue compilando y probándose hasta S9**: nadie se queda sin
VARMA mientras el nuevo no lo cubre.

**Validación contra los ejemplos de sima:** en S4-S7, `jenkins_alavi`,
`tiao_box` y `lutkepohl_poskitt` se corren en el GUI y sus números se comparan
con los tutoriales de sima (`examples/*/TUTORIAL.md`): la puerta, la LR y los
parámetros. sima usa el porte Python y el GUI el C; las diferencias van con la
tolerancia que ya usa la CI fuera de Linux.

---

## 8. Segunda versión: la ruta V

La ruta V (Tiao y Box: el vector primero, sin univariantes; sima
`write_specs`, `docs/STUDY-raw-entry.md` §3.4):

- **El motor ya sirve.** Cada serie lleva un `.inp` con solo su
  transformación y un AR(p)/MA(q) libre de semilla; la escalera reestima la
  diagonal con la dinámica cruzada.
- **No escribe `.pre` de las series.** Sus estimaciones son filas del
  sistema, no óptimos univariantes: los ficheros viven en el sistema
  (`_casos/C3/`), no en la lista US.
- **La puerta no es una vara de medir.** Con la diagonal en ruido blanco (o
  paseo aleatorio si d = 1) solo certifica el cast; la página 2 lo dice.
- **Le falta lo que hoy solo está en Python:** la identificación sobre el
  vector (R_k, S_k, la M(l) de Tiao y Box, el canónico) y la vara de medir de
  N5 (univariantes de la ruta U solo para comparar).
- **En la madre:** una tercera pestaña en «Nuevo sistema…», «Desde las series,
  el vector primero».

---

## 9. Maquetas

### 9.1 La ventana

```
┌ sim_gui — SIM · VARMA · C2 «muskrat y mink» · c01 ─────────────────────────┐
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
│  ¿Se mueven juntas a largo plazo? Esa pregunta es la del rango.  [Rango…]  │
│                                                                             │
│                                          [◀ Series]   [Identificación ▶]    │
├────────────────────────────────────────────────────────────────────────────┤
│ Puerta superada: el sistema diagonal reproduce los univariantes.  Modelo: c01│
└────────────────────────────────────────────────────────────────────────────┘
```

Las cifras de la maqueta son ilustrativas. Hasta S10, «Rango…» dice que el
VEC llegará y qué hace sima mientras tanto.

### 9.2 La identificación

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

### 9.3 El menú SIM de la madre

```
 Proyecto  Datos  US  MT  SIM  Ayuda
                          ┌───────────────────────────────┐
                          │ Nuevo sistema…                │
                          │ Pasar a SIM                   │
                          ├───────────────────────────────┤
                          │ Estimar C2/c01         Ctrl+E │
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
   globo del título: «SIM — Sistemas simultáneos, con drvarma
   (VARMA) y drvec (VEC)»
```

---

## 10. Decisiones

**Cerradas (2026-11-05):**
- Un solo GUI y un solo menú, **SIM**, para VARMA y VEC (§0.2).
- La **ruta V en la segunda versión** (§8).

**Aceptadas con la recomendación (2026-11-05):**

1. **El nombre del GUI hijo:** `gui/sim/`, binario `sim_gui`. Recomendado:
   sí (los otros se llaman por su motor, pero este tiene dos).
2. **«sistema»** para un caso SIM, «caso» para MT. Recomendado: sí.
3. **Siete páginas**, con la Puerta como la 2 y el resto en el orden de
   drtran; la R · Rango entra con S10. Recomendado: sí.
4. **La ruta U en la primera versión** (§5.5), como complemento de la
   escalera: modelos de partida, con el identificador de la madre, solo para
   las series sin modelo, marcados «sin revisar». Recomendado: sí.
5. **La orquestación de simplificar y estructura** (N4s, N4t) en
   `atsw-cli`, para el GUI, la madre y el MCP. Recomendado: sí.
6. **Lo que solo existe en Python** (canónico, M(l), echelon): «esto lo hace
   el asistente», con la orden de sima, hasta que haya C. Recomendado: sí.
7. **`seasonal_detection.c` se queda en `engines/drvarma/gui/`** porque lo
   compila el motor; se mueve solo con OK, en un PR propio.
8. **Johansen y el VECM del GUI viejo no pasan al nuevo.** El VEC de SIM es
   drvec. Recomendado: no pasan.
9. **La volatilidad (`-volexp`, `-volmov`)** fuera de la primera versión.
   Recomendado: fuera.
10. **B1** (el motor sale con 0 en la ruta del `.inp`): su repro en S0; el
    arreglo, con OK, aunque SIM no use esa ruta. Recomendado: sí, otros
    llaman al motor por ahí.
11. **Los proyectos de ejemplo** en Ayuda → Ejemplos, empezando por
    `jenkins_alavi` (§6). Recomendado: sí.
12. **«Ver modelo»** en lo que no encaja en una ecuación univariante (§3.4):
    Σ en una página propia del sistema (recomendado); el nombre de la
    innovación ajena (a_B,t); el VEC y la echelon cuando lleguen.

---

## 11. Ficheros citados

`engines/drvarma/`: `gui/drvarma_gui.c` · `gui/johansen_test.c` ·
`gui/vecm.c` · `gui/seasonal_detection.c` · `src/drvarma.c` ·
`src/escalera.c` · `Makefile` · `BUGS.md` · `tests/gui/` ·
`docs/DESIGN-v5-ladder.md`.

`gui/`: `atsw/src/gestos.c` · `atsw/src/casos_gui.c` · `atsw/src/main.c` ·
`drtran/src/main.c` · `drtran/tests/`. `lib/equation/eqdraw.c`.

sima-python: `src/sima/mcp_server.py` · `src/sima/mcp_vec.py` ·
`src/sima/raw.py` · `docs/TOOLS.md` · `docs/STUDY-raw-entry.md` ·
`docs/MANUAL-jenkins-alavi.md` · `examples/`.

Ver también `DISENO-mtram.md`, `DISENO-barra-drtran.md`, `DISENO-casos.md`,
`ESTUDIO-madre-organizacion.md` §6, `ESTUDIO-terminal-mcp.md` y `RELEVO.md`
§6.
