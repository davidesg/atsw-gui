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
LaTeX (`.tex` y `<input>_prev.<sub>.<año>.tex`) y el gráfico de previsión
`prev<input>.<sub><año>.eps`, que desde fuf 1.09 dibuja el propio programa. El PDF no se compara, porque lo que lo determina
sí se compara.

pdflatex se sustituye por un programa que no hace nada: lo que se compara no
depende de él, y la batería tarda unos segundos. (fuf 1.09 ya no usa gnuplot;
el falso está para que fuf 1.08.2 pueda pasar la misma batería.)

## La línea base (fuf 1.08.2)

`tests/golden/` y los códigos de `runs.tsv` son los de **fuf 1.08.2** (commit
ce00ecd), compilado en Linux x86-64 con gcc. Son la referencia de fuf 1.09: lo
que ya funcionaba debe dar exactamente lo mismo, y cada cambio de un código de
salida tiene que ser una decisión, hecha en el commit que lo cambia.

Códigos de salida de fuf 1.09: 0 resultados escritos; 1 error en la línea de
órdenes o al abrir un fichero; 2 el `.inp` no es válido (no se escribe nada);
3 el modelo no se puede estimar desde sus valores iniciales; 4 error interno.

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

- **Validación de la entrada** (commit de la validación): fuf comprueba el
  `.inp` antes de escribir nada (`src/inpcheck.c`, adaptado del de fue: el
  `.inp` de fuf es el de fue con el horizonte de previsión y la varianza de
  la innovación tras la fecha, y eso es lo que los distingue).
  `forecast_RIPC.1` pasa a terminar con código 2 y un mensaje que dice que es
  un fichero de fue. Ninguna de las 13 ejecuciones que funcionaban cambia.
  Los errores provocados a propósito están en `errors.tsv` (ficheros
  `tests/corpus/bad_*.inp`).

- **Gráfico nativo** (commit del gráfico): el gráfico de previsión
  `prev<input>.<sub><año>.eps` lo dibuja fuf (`src/fufplot.c`, con el motor
  gráfico de fug) y ya no llama a gnuplot. Los 13 EPS entran en las
  referencias, y se comprueba que con el PATH vacío el gráfico se escribe
  igual. Ningún `.out` ni fichero LaTeX cambia.

- **Informe nativo** (commit del informe): fuf dibuja su informe
  (`src/report.c`: encabezamiento, tabla de previsiones con su sombreado y
  gráfico) y ya no llama a pdflatex salvo con `-latex`. Se comprueba que con
  el PATH vacío escribe el PDF y que es de fugdraw. Ningún `.out` ni fichero
  LaTeX cambia: el `.tex` se sigue escribiendo siempre.

- **La parte determinista con la regla de fue** (fuf 1.10, `lib/fuedet`):
  los pesos ν(B) = ω(B)/δ(B) de una determinista con δ se truncan en 40
  retardos, como en fue, y no en 20; el código es el de fue, compartido.
  Ninguna de las 14 ejecuciones anteriores cambia: ninguna lleva δ. Entra
  `forecast_RIPC.2`, que sí lo lleva (escalón en 2/2002 con δ = 0,8434, 78
  observaciones): es el `forecast_RIPC.2.inp` que escribe `fue RIPC.2 -f 12`
  con el corpus de fue. Con el 20, fuf partía de un punto que no era el
  óptimo de fue --norma del gradiente 3,4490 en la iteración 0, media de los
  residuos 0,0415--; con el 40, 0,0230 y −0,0005. La tabla de previsiones
  se mueve hasta 0,52 (la variación anual) y 0,0023 en el nivel.

## Ver el informe

El informe en PDF no se compara en la batería (lo que se compara es lo que lo
determina: el `.out`, los ficheros LaTeX y el EPS). Para verlo:

```sh
mkdir -p samples && cd samples
cp ../tests/corpus/forecast_D1.inp .
../bin/fuf forecast_D1            # el PDF lo dibuja fuf
../bin/fuf forecast_D1 -latex     # el PDF lo compila pdflatex desde el .tex
```

`samples/` está en `.gitignore`: se regenera cuando haga falta.

## El corpus

`tests/corpus/` reúne los ficheros de entrada de fuf que hay: los que escribe
`fue -f`, los de gtk_fue.09 (`data/`), y los de ART y del porte a Python.
Antes de publicar la rama conviene revisar que todos se pueden distribuir.
