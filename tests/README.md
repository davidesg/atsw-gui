# Pruebas de regresión de FUE

```
make check                           # sh tests/run_tests.sh bin/fue
sh tests/run_tests.sh --update       # rehace tests/golden/ y los estados de runs.tsv
```

## Qué comprueban

`tests/runs.tsv` enumera las ejecuciones: un fichero de `tests/corpus/`, los
argumentos de fue (`-` sin argumentos), el formato del fichero y el código de
salida esperado. Cada ejecución debe terminar con ese código y, si termina bien
(0), escribir los mismos `.out`, `.pre` y ficheros LaTeX (`.tex`, `_res.tex`,
`_dist.tex`) que `tests/golden/<id>/`, byte a byte.

gnuplot y pdflatex se sustituyen por programas que no hacen nada: lo que se
compara no depende de ellos, y la batería tarda unos segundos.

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

## El corpus

`tests/corpus/` reúne sin duplicados los `.inp` de gtk_fue.09 (`data/`), de
fue-1.13 (`examples/`), del porte a Python (`fue-python/tests/`) y de ART
(`art-python`: casos y reproducciones de fallos). `tests/corpus/MANIFEST.tsv`
da el origen y el formato de cada uno. Antes de publicar la rama conviene
revisar que todos se pueden distribuir con fue.
