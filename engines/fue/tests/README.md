# Pruebas de regresión de FUE

```
make check                           # sh tests/run_tests.sh bin/fue
sh tests/run_tests.sh --update       # rehace tests/golden/ y los estados de runs.tsv
```

## Qué comprueban

`tests/runs.tsv` enumera las ejecuciones (y `tests/errors.tsv` los errores): un fichero de `tests/corpus/`, los
argumentos de fue (`-` sin argumentos), el formato del fichero y el código de
salida esperado. Cada ejecución debe terminar con ese código y, si termina bien
(0), escribir los mismos `.out`, `.pre`, ficheros LaTeX (`.tex`, `_res.tex`,
`_dist.tex`) y gráfico de los residuos (`A<input>.eps`) que
`tests/golden/<id>/`, byte a byte.

pdflatex se sustituye por un programa que no hace nada: lo que se compara no
depende de él, y la batería tarda unos segundos. (fue 1.14 ya no usa gnuplot.)

## La línea base (fue 1.13.1)

`tests/golden/` y los códigos de `runs.tsv` son los de **fue 1.13.1** (commit
c4a6d2c), compilado en Linux x86-64 con gcc. Son la referencia de fue 1.14: lo
que ya funcionaba debe dar exactamente lo mismo, y cada cambio de un código de
salida tiene que ser una decisión, hecha en el commit que lo cambia.

Estado de partida (98 ejecuciones):

| formato | ejecuciones | 1.13.1 |
|---|---|---|
| fue | 65 | terminan bien (0): su salida es la referencia |
| fue | 16 | **segmentation fault (139)**: modelos sin operadores AR ni MA |
| fuf | 12 | 1, 134 (abort por memoria corrupta) o 139: fue no reconoce el formato |
| fug | 5 | 139: ídem |

Los ficheros de fuf (con la sección del horizonte de previsión) y de fug (con
la línea Box-Cox tras la fecha) no son entradas de fue: están para comprobar
que fue 1.14 los rechaza con un mensaje y un código de error, en lugar de
corromper la memoria o, peor, escribir resultados sin sentido.

## Cambios de estado respecto a la línea base

Cada commit que cambia un código de salida de `runs.tsv` lo explica aquí.

- **Modelos sin operadores AR ni MA** (commit del arreglo del caso vacío):
  los 16 ficheros de fue que terminaban en segmentation fault terminan bien.
  Su salida no tenía referencia en 1.13.1, así que se valida de otra forma:
  para todo modelo sin AR ni MA, `run_tests.sh` ejecuta también el mismo
  modelo con un factor AR(1) fijado en 0 (la forma en que los `.inp` evitaban
  el fallo) y exige el mismo `.out` —salvo las líneas de ese factor— y los
  mismos ficheros LaTeX. Ninguna de las 65 referencias de 1.13.1 cambia.
  Con este arreglo, 7 ficheros de fuf y fug que antes terminaban mal
  "terminan bien" con resultados sin sentido: fue todavía no valida el
  formato de la entrada (siguiente commit).

- **Validación de la entrada** (commit de la validación): fue comprueba el
  `.inp` antes de escribir nada (`src/inpcheck.c`, que lo lee con las mismas
  llamadas y en el mismo orden que fue). Los 12 ficheros de fuf y los 5 de fug
  terminan con código 2 y un mensaje que dice qué son. Ningún fichero de fue
  cambia de estado ni de salida. Los códigos de salida y los errores
  provocados a propósito están en `errors.tsv` (ficheros
  `tests/corpus/bad_*.inp`): cada uno debe terminar con su código, decir qué
  pasa y, con código 2, no escribir nada.

- **Gráficos nativos** (commit de los gráficos): el gráfico de los residuos
  `A<input>.eps` lo dibuja fugdraw (`src/fugdraw.c`, `src/fugplot.c`, los
  mismos ficheros que en fug) y se añade a las referencias. Ningún `.out`,
  `.pre` ni `.tex` cambia.

- **La ecuación, separada de LaTeX** (commit de la ecuación): `src/equation.c`
  recorre el modelo una vez y construye la ecuación (`include/equation.h`);
  `src/eqlatex.c` la escribe como el fichero LaTeX de siempre. Los 81 `.tex`
  de referencia salen idénticos byte a byte, que es lo que dice que la
  reorganización no cambió nada. Ningún `.out`, `.pre` ni EPS cambia.

- **Standard errors from fdhess** (commit of fue BUG-0015, 2026-09-27; in
  English from here on). By default `est()` takes the Hessian at the
  optimum by finite differences. This is the `fdhess` call Mauricio left
  commented out. Before, the covariance came from the Hessian that BFGS
  accumulated along the search path. `-hessian bfgs` gives the old
  behaviour.
  - **Where the goldens differ from 1.13.1:** in the `.out` and `.tex`,
    the standard errors (the numbers in parentheses), the covariance and
    correlation matrices with their list of correlations ≥ 0.7, and a new
    line `Standard errors: <method>` before the covariance matrix.
  - **What is unchanged:** every estimate, every `.pre`, every EPS, and
    every exit status in `runs.tsv`. That was checked line by line before
    regenerating (145 files).
  - **Methods in the 149 outputs:**
    - fdhess: 79;
    - no free parameters: 69;
    - estimation failed (`bad_nonstationary`): 1, and it says so.
  - **Fallbacks to BFGS,** each stated in the `.out`:
    - `R.4_2`: the optimum is on the boundary;
    - `syn_ARF`: the Hessian is not positive definite, because of an
      f-fixed AR coefficient of −0.000006.
  - **The check:** ES_CPI_m10 from its `.pre` now gives SE(μ) = 0.028502,
    which is the exact GLS. With `-hessian bfgs` it gives 0.073304, the
    "run A" of BUG-0015.

- **Quarterly year labels with two digits** (branch fugplot-two-digit-years,
  2026-10-03). The residual graph `A<input>.eps` of a quarterly series labels
  the years with their last two digits (`96`, `98`, `00`), as GraphMaker does
  (`singletrim.cpp`: `FormatFloat("00", …)`). With the full year the labels
  overlapped past about 25 years of quarterly data. Monthly series keep the
  full year, as Treadway approved it, and switch to two digits only if the
  full year would not fit. Annual series always keep the full year.
  - **Where the goldens differ:** the 34 quarterly EPS. Only the label text
    and its centring changed, checked line by line before regenerating.
  - **What is unchanged:** every other EPS (monthly and annual), every
    `.out`, `.pre` and `.tex`, and every exit status.

- **The title where GraphMaker puts it** (same branch, 2026-10-03). In the
  residual graph the title is left-aligned from 80 % of the width of the
  series panel (GraphMaker's `singletrim.cpp`, `singlemonth.cpp`). If the
  title would not fit, it moves left, never into the acf/pacf column. It used
  to end flush at the panel's right edge, which looked too far right.
  - **Where the goldens differ:** all 92 residual EPS. Only the title's x
    moved (its y never) — checked on every file before regenerating —
    together with the quarterly labels above.
  - **What is unchanged:** every `.out`, `.pre` and `.tex`, and every exit
    status.

- **`Q(39)`, not `Q( 39 )`** (same branch, 2026-10-03). The Ljung-Box label of
  the residual graph and of `fug -b` loses the spaces inside the parentheses,
  which were a slip carried over from GraphMaker's «Q ( 15 )». `Q(k)` is the
  usual notation and the one art's text uses.
  - **Where the goldens differ:** all 92 residual EPS, only in the Q line (its
    text and its centring), checked on every file before regenerating.
  - **What is unchanged:** every `.out`, `.pre` and `.tex`, and every exit
    status.

## La batería sintética

`syn_*.inp` (serie IPCM de `fug/examples`): ruido blanco sin nada libre y con
la media libre, una variable determinista sin ARMA, sus equivalentes con un
AR(1) fijado en 0, y un modelo mínimo de cada tipo de operador (AR, MA, MA
anual, AR(2) y MA(2) de frecuencia fija).

## Ver el informe

El informe en PDF no se compara en la batería (lo que se compara es lo que lo
determina: el `.out`, el `.tex` y el EPS). Para verlo:

```sh
mkdir -p samples && cd samples
cp ../tests/corpus/D.inp .
../bin/fue D            # el PDF lo dibuja fue
../bin/fue D -latex     # el PDF lo compila pdflatex desde el .tex
```

`samples/` está en `.gitignore`: se regenera cuando haga falta.

Modelos del corpus que conviene mirar, porque cada uno enseña una cosa:

| modelo | qué tiene |
|---|---|
| `D` | mensual: once armónicos deterministas, AR(3) y media |
| `ES_CPI_S` | armónicos y operadores AR y MA de frecuencia fija (`f = 3`) |
| `R.2` | trimestral: una intervención con su fecha (impulso de 2/2008) |
| `R.6` | trimestral con pocos datos: el informe sale en vertical |
| `syn_ARF` | AR(2) de frecuencia fija y diferencia anual completa |
| `en4_ar18` | anual, AR(18): la ecuación más larga, parte de línea |

## El corpus

`tests/corpus/` reúne sin duplicados los `.inp` de gtk_fue.09 (`data/`), de
fue-1.13 (`examples/`), del porte a Python (`fue-python/tests/`) y de ART
(`art-python`: casos y reproducciones de fallos). `tests/corpus/MANIFEST.tsv`
da el origen y el formato de cada uno. Antes de publicar la rama conviene
revisar que todos se pueden distribuir con fue.
