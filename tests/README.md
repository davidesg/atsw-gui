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

## La batería sintética

`syn_*.inp` (serie IPCM de `fug/examples`): ruido blanco sin nada libre y con
la media libre, una variable determinista sin ARMA, sus equivalentes con un
AR(1) fijado en 0, y un modelo mínimo de cada tipo de operador (AR, MA, MA
anual, AR(2) y MA(2) de frecuencia fija).

## El corpus

`tests/corpus/` reúne sin duplicados los `.inp` de gtk_fue.09 (`data/`), de
fue-1.13 (`examples/`), del porte a Python (`fue-python/tests/`) y de ART
(`art-python`: casos y reproducciones de fallos). `tests/corpus/MANIFEST.tsv`
da el origen y el formato de cada uno. Antes de publicar la rama conviene
revisar que todos se pueden distribuir con fue.
