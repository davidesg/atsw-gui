# `lib/` — lo que comparten los programas

Un fichero que hace falta en dos sitios vive **aquí**, no copiado en los dos.

Ésa es la razón de ser de este repositorio, y está medida: cada divergencia que
encontró el estudio tiene la misma causa —el mismo fichero en dos árboles— y
una de ellas, la copia de `inpcheck` del GUI, divergió **dentro de una sola
sesión de trabajo**, dejando al GUI cargando un fichero que le destruía el
montón.

## `fugdraw/` — el motor vectorial

Escribe EPS y PDF comprimido con Flate desde un único *content stream*. **C
puro: no depende de GTK ni de glib.** Su API son veinte funciones — `fd_line`,
`fd_polyline`, `fd_disc`, `fd_text`, `fd_write_eps`, `fd_pdf_open/page/close`…

Estaba en **cuatro copias byte a byte idénticas** —fue, fuf, fug y el GUI de
fue— y las cuatro tenían el mismo md5. Que estuvieran idénticas no era virtud
del diseño: era suerte, la misma que tuvo `inpcheck` hasta que se le acabó.

Lo usan hoy: `engines/fue`, `engines/fuf`, `engines/fug`, `gui/fue`. Y lo
necesitará el GUI de drtran, si drtran adopta `fugdraw` para dibujar la CCF
preblanqueada en vez de EPS de gnuplot — que es lo que haría coherente la
biblioteca, porque entonces `preview.c` la muestra tal cual, con zoom y lupa.

## `fugplot/` — los gráficos del informe

Dibuja sobre `fugdraw` lo que el informe necesita: la serie, los residuos, la
ACF y la PACF con sus bandas, el histograma, el gráfico media-desviación.

Estaba en **dos copias idénticas**, en `engines/fue` y `engines/fug`. Y el
patrón ya estaba articulado por escrito, en la cabecera de `plothost.h`:

> *«fugdraw.c/h, fd_metrics.h and fugplot.c/h are the same files in fug and
> fue; each program has its own plothost.h.»*

Así que `fugplot.c` y `fugplot.h` vienen aquí y **`plothost.h` se queda en cada
programa** — 28 líneas en fue, 15 en fug. Es lo que cada uno tiene que aportar:
`struct Tseries`, `vector()`, `Acf()`, `Pacf()`, `ChiTest()` y los
estadísticos. La misma forma que `previewhost.h` tiene para `preview.c`.

## `preview/` — la ventana de gráficos

Muestra lo que `fugdraw` escribe, con **zoom** y con una **lupa** estilo `gv`
para mirar un incidente en los datos. Exporta a PDF, EPS, PNG y SVG, imprime, y
lleva una tabla global `ruta → ventana` para no abrir dos veces el mismo
fichero.

Lo que cada programa tiene que aportar vive en **su** `previewhost.h`, y son
tres cosas: un `typedef` del contexto, `preview_open_external()` y
`preview_show_status()`. La cabecera de `gui/fue/include/previewhost.h` ya
decía que esto iba a pasar:

> *«When the two are factored into a library this is the header that stays
> different.»*

**Queda una copia, en `gui/fug`, y es deliberado**: es GTK2, y ésta es esa
misma portada a GTK3 y ampliada. Se va cuando fug se porte — paso 2 del plan.
`conformidad/copias.sh` lo lleva anotado como excepción con su razón.

## `engine/` — correr un motor sin shell

`engine_run()` y `engine_run_async()`, con barra de progreso por iteración. Los
motores se lanzan **directamente, nunca por un shell**: un nombre de serie con
un espacio, una comilla o un punto y coma acababa en `/bin/sh`.

Traduce el estado de salida a un mensaje —0 escrito, 1 línea de órdenes, 2
`.inp` inválido, 3 no estimable pero escrito con los valores iniciales, 4 error
en ejecución— y distingue «hay ficheros» de «salió bien».

Cada corrida se mide con `lib/coste` (abajo): sin contexto puesto, no se
escribe nada.

## `coste/` — lo que cuesta cada corrida

Cada corrida de un motor deja una línea en `costes.csv`, en la raíz del
proyecto (docs/ESTUDIO-terminal-mcp.md §7.7): cuándo, quién la lanzó, para
qué, con qué motor y versión, el tiempo de CPU y el de reloj, la energía
**estimada** con su método escrito al lado, y el sha256 del `.pre` resultante.
Un registro de sólo añadir, con cerrojo (`flock` en Unix, `LockFileEx` en
Windows) y una sola escritura por línea, para que la madre y el MTM puedan
apuntar a la vez.

La CPU sale de `GetProcessTimes` en Windows y de la diferencia de
`getrusage(RUSAGE_CHILDREN)` en Unix, exacta mientras corra un motor a la vez.
El que lanza dice antes de qué proyecto y para qué (`coste_contexto`), y
`lib/engine` apunta la corrida; `coste_apunta` es para los de fuera (`atsw-cli
coste apunta`) y `coste_resumen` suma el registro. La prueba es
`test_coste.c`, en la batería de `gui/drtran`.

## `outfile/` — cómo acabó la estimación

Lee el `.out` y dice si convergió **de verdad**. Importa porque el motor
escribe `"CONVERGENCE OBTAINED"` para los cinco criterios de parada, **también
cuando lo que pasó fue que se acabaron las iteraciones**. Aquí se separan los
dos que son convergencia (gradtol, steptol) de los tres que son una parada sin
más.

Depende sólo de `qnewtopt.c`, que es **el mismo optimizador en fue, fuf y
drtran**. Entra en un GUI de drtran sin tocar una línea.

## `utils/` — formato y nombres

`inp_format()` escribe un número de modo que **vuelva a leerse exactamente
igual** —prueba de 6 decimales en adelante y se queda con el primero que
cumple—, que es lo que arregló que el GUI le quitara cifras a los datos del
usuario. Y `token_name()` frente a `single_token()`: el nombre de un fichero y
el nombre dentro del `.inp` no tienen las mismas reglas — ahí `PE/PU` es
legítimo y sólo el espacio está prohibido.

## `inpcheck/` — y por qué aquí sólo está la cabecera

**`inpcheck.c` no se copia.** El GUI compila **el fichero del motor**,
renombrando la función al vuelo:

    $(CC) -Dinp_check=inp_check_fue -c ../../engines/fue/src/inpcheck.c

La única razón del fork era la colisión de nombres —`inp_check` se llama igual
en fue y en fuf— y un `-D` la resuelve sin duplicar nada. Así que es
literalmente el mismo código que corre el motor, **por construcción y no por
vigilancia**.

Aquí sólo está `inpcheck.h`, que es la vista de los dos dialectos que cualquier
GUI necesita.

Esto importa más que las otras piezas porque es la que **ya divergió**: se
arregló una cota en el motor y no la copia, y el GUI se quedó cargando un
fichero que le destruía el montón. `conformidad/copias.sh`, que antes vigilaba
esa copia, comprueba ahora lo contrario: que nadie vuelva a hacer una.

## Cómo se usa

En el `Makefile` del programa:

    LIB_DIR  = ../../lib
    LIB_SRCS = $(LIB_DIR)/fugdraw/fugdraw.c
    CFLAGS  += -I$(LIB_DIR)/fugdraw

## La prueba de que extraerlo no cambió nada

El EPS que dibuja `fue` desde `lib/` es **byte a byte el mismo** que dibujaba
con su copia:

    md5  2e6ea3c55cf9   antes
    md5  2e6ea3c55cf9   despues

Y los tres bancos, idénticos: 109 corridas y 0 fallos en fue-1.14, 53
comprobaciones y 0 fallos en el GUI, y 102 pasan y 0 fallan en el corpus de
conformidad.

## `fuepre/`, `optim/` and `dates/` — added 2026-09-26

- **`fuepre/`**: the `.pre` reader, out of `engines/drtran`, now shared by
  drtran, its GUI and drvarma 5.0 (the ladder). Its README explains the
  packing contract and BUG-2.
- **`optim/lnsrch.c`**: the line search of the quasi-Newton optimiser. It was
  one function in four copies (drvarma, drtran, fue, fuf) that differed by
  `a == 0` against `a == 0.0`. It carries the fix for a non-finite objective,
  which hung the program; each engine supplies an `optimhost.h`. The rest of
  `qnewtopt.c` stays in each engine, because those copies diverge in what
  they print. The proof is the same as above: drvarma's `.inp` bench, the
  golden files of fue and fuf, and 13 drtran outputs, all byte-identical.
- **`dates/`**: `ObsToDate` and `DateToObs` are no longer copied. fue, fuf,
  fug, drvarma and the fue GUI link this file; their copies were identical.

## Lo que falta traer aquí

Por orden de facilidad:

| pieza | dónde está | estado |
|---|---|---|
| `nlatools.c` | los siete programas | **NO por ahora.** Siete copias vivas separadas entre 13 y 80 líneas. Son primos cercanos —drtran y drvarma difieren en 13 líneas de 1106— pero es el núcleo numérico y tocarlo mueve números. La mudanza tiene que preservar el comportamiento; reconciliar numéricas no lo preserva |

## `lik/` — Shea's exact likelihood beside elf, added 2026-09-28

- **What it is.** `multshea.c` is `marma`, Shea's AS 242 (1989),
  transcribed by Mauricio in 1996. It was compiled in drvarma and never
  called. `lik.c` puts it beside elf (AS 311) with `varma_lik()`, and
  `lik.h` declares both.
- **Users.** drvarma (`-lik elf|shea|both`) and drtran (`-l elf|shea|both`).
  Each supplies a `likhost.h` with `real`, `elf()`, `chekma()` and the
  allocators, like `optimhost.h` for `optim/`.
- **How it behaves.** Shea is always exact: its steady-state shortcut is a
  different approximation from elf's ξ truncation. The residuals always come
  from elf, because the forecasts need the exact residuals, not Shea's
  innovations (drtran BUG-55). Shea uses elf's MA frontier (`chekma`).
- **Validation.**
  - drtran's whole battery passes with `-l shea`: 326/326, including the
    homologation against fue, whose likelihood is Mélard's AS 197, the
    exact GLS and the synthetic truths.
  - With `-l both`, 147 runs and 524 795 points compare elf and Shea. Every
    optimum agrees below 4e-12, except §3's Y = X, which is degenerate by
    design.
  - In drvarma with `-m 2`, the difference is 1e-9 to 1e-13 at every point
    the optimiser visits.

## `artcand/` — the identifier's result

The one reader of `DATA_art.cand`, the line file that `engines/art` writes
(its format is in `engines/art/README.md`). It is pure C, so it is tested on
its own against the engine's goldens. The identification window
(`lib/analisis/an_identifica.c`) uses it.

## `inpdet/` — also the ARMA part and the transformation

There are two new functions next to the deterministics:
- **`id_pon_arma`** writes a candidate's ARMA orders and seeds: one factor
  each, estimated, with Box-Jenkins signs as fue stores them. A file without
  ARMA comes back identical when given none.
- **`id_pon_transformacion`** writes the λ, d, D line.

The rule is the module's: everything else is copied byte for byte, and the
judge is `inp_check_fue`. Both are tested over the fue corpus.


## `diagmv/` — the multivariate residual diagnosis, added 2026-10-11

- **What it is.** Hosking's portmanteau and the multivariate Jarque-Bera,
  with the block of the `.out` that prints them. They were three copies in
  the `diagnose.c` of drvarma, drtran and drvec, and all three had the same
  defects (drvec BUG-42, items 3 and 4).
- **What changed.** Hosking's Q is referred to χ²(m²s − k), with k the
  dynamic coefficients the engine estimated. Each engine passes its own
  count: drvarma Φ + Θ, drtran the transfer weights and the noise ARMA,
  drvec Λ + F + Θ. β is not counted (Brüggemann, Lütkepohl and Saikkonen
  2006). The lags are freq + 2, or 10 for non-seasonal data, the rule of
  drvarma's Python port. The Jarque-Bera is Lütkepohl's, on residuals
  standardised by the symmetric square root of their covariance, so it does
  not depend on the order of the series.
- **Users.** drvarma, drtran and drvec. Each supplies a `diagmvhost.h`, like
  `optimhost.h` for `optim/`.
- **Validation.** Against numpy and scipy on simulated residuals: Q and JB
  agree to every printed digit. The p-values differ by up to 1e-4, which is
  the Wilson-Hilferty approximation in each engine's `chisq`.


## `vuelta/` — what drtran gives back to the univariate model, added 2026-10-17

- **What it is.** drtran re-estimates each series' univariate block (its
  ARMA, deterministics and mean) beside the transfer, and the block moves.
  `vu_escribe` writes it back as a fue `.inp`: the series' `.pre` with the
  new seeds and every flag as it was. `vu_caso` does it for every entry of a
  case's model and registers each as a derived model of its series, child of
  the one that entered, with the reason (DISENO-casos §3).
- **`.inp`, not `.pre`.** The blocks are optimal *with* the transfer, not on
  their own, and fue moves them. The extension is the only mark the file
  carries, so it has to say so. The transfer does not go in.
- **By order, not by name.** The `.out` names two regular AR factors'
  first coefficients the same (`phi_1[B^1]`). What matches is the slot order
  of `lib/slots` against the `.pre`'s flags; if the counts differ, nothing is
  written. `inpdet` gained the deterministics' coefficients in that order
  (`id_det_coefs`, `id_escribe_det_coefs`), the mean (`id_media_lee`) and the
  count of fixed-frequency AR(2) operators, which this does not write yet.
- **Users.** drtran_gui (Ver modelo ▾) and the mother (the case model's menu),
  the same `vu_caso`; each only asks and saves.
- **Validation.** On the IPC ← WTI case of the manual, a diagonal run gives
  back the `.pre` to the six decimals of the `.out`, and fue estimates the
  written files to the same optimum (logelf −7.4026486474). `test_vuelta`
  runs drtran on the m6 and checks that what is written reads back as the
  `.out` says, with the `.pre`'s flags, and that a `.pre` from another series
  is rejected without leaving a file.

## `fuedet/` — the deterministic filter of a fue model, added 2026-10-16

- **What it is.** ω(B)/δ(B) applied to the deterministic variables of a
  `.pre`, with fue's rule: step [5] of `cast_us` in `engines/fue/src/fue.c`,
  moved here verbatim. ν(B) by `calcnu`, truncated at Nomega lags, or 40 when
  the variable has a δ; the variables summed and subtracted once.
- **What changed.** There were nine implementations. fue's stays as it was
  (its 109 runs are byte-identical); fuf truncated at 20, and drtran,
  drvarma, drvec and the GUIs used the exact recursion, one of them
  subtracting variable by variable. All of them now call this (drvec
  BUG-40).
- **Users.** fue, fuf, `fuepre` (`build_det_component`), `prewhiten`
  (`apply_univariate_model`), drvec, and the Python package of fue, which
  carries a byte-identical copy checked by its sha256. See
  `fuedet/README.md`.
