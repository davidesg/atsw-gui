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

## Cambios de estado respecto a la línea base

Cada commit que cambia un código de salida de `runs.tsv` lo explica aquí.

- **Modelos sin operadores AR ni MA** (commit del arreglo del caso vacío):
  `forecast_RIPC.1` ya no termina en segmentation fault. Se arreglaron tres
  cosas de la misma familia —el C muere en vez de avisar—, las dos últimas
  portadas del BUG-0008 de fue:
  1. `nlatools.c`: matrices vacías válidas y liberables, y `cholfor`/`cholbak`
     no hacen nada con n < 1; `fuf.c` no llama al optimizador con 0 parámetros;
  2. `File_PlotSer`: una serie degenerada (varianza cero) daba AbsMax = NaN y
     el índice del gráfico de caracteres se iba fuera del buffer;
  3. `PlotCor`: con los residuos a cero, todas las correlaciones son NaN y la
     barra de la acf se escribía fuera del buffer.

  Ninguna de las 13 ejecuciones que ya funcionaban cambia.

  **`forecast_RIPC.1` no es un fichero de fuf**: es un `.inp` de fue (no tiene
  la sección del horizonte de previsión), así que fuf lo lee desplazado y
  escribe resultados sin sentido. Ahora termina con 0; el commit de la
  validación de la entrada lo rechazará con un mensaje y código 2.

## El corpus

`tests/corpus/` reúne los ficheros de entrada de fuf que hay: los que escribe
`fue -f`, los de gtk_fue.09 (`data/`), y los de ART y del porte a Python.
Antes de publicar la rama conviene revisar que todos se pueden distribuir.
