# Pruebas de regresión de FUF

```
make check                           # sh tests/run_tests.sh bin/fuf
sh tests/run_tests.sh --update       # rehace tests/golden/ y los estados de runs.tsv
```

## Qué comprueban

`tests/runs.tsv` enumera las ejecuciones: un fichero de `tests/corpus/`, los
argumentos de fuf (`-` sin argumentos) y el código de salida esperado. Cada
ejecución debe terminar con ese código y, si termina bien (0), escribir los
mismos ficheros que `tests/golden/<id>/`, byte a byte: el `.out`, los ficheros
LaTeX (`.tex` y `<input>_prev.<sub>.<año>.tex`) y el gráfico
`prev<input>.<sub><año>.eps`. El PDF no se compara, porque lo que lo determina
sí se compara.

gnuplot y pdflatex se sustituyen por programas que no hacen nada: lo que se
compara no depende de ellos, y la batería tarda unos segundos.

## La línea base (fuf 1.08.2)

`tests/golden/` y los códigos de `runs.tsv` son los de **fuf 1.08.2** (commit
ce00ecd), compilado en Linux x86-64 con gcc. Son la referencia de fuf 1.09: lo
que ya funcionaba debe dar exactamente lo mismo, y cada cambio de un código de
salida tiene que ser una decisión, hecha en el commit que lo cambia.

Estado de partida (14 ejecuciones): 13 terminan bien y **una termina en
segmentation fault** (`forecast_RIPC.1`), un modelo sin operadores AR ni MA.
Es el mismo defecto que fue 1.13.1 tenía, en el mismo sitio: `cholfor`
(`nlatools.c`) con n = 0, desde `elf` (`elfvarma.c`), porque max(p, q) = 0.

## El corpus

`tests/corpus/` reúne los ficheros de entrada de fuf que hay: los que escribe
`fue -f`, los de gtk_fue.09 (`data/`), y los de ART y del porte a Python.
Antes de publicar la rama conviene revisar que todos se pueden distribuir.
