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
