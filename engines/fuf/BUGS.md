# Fallos conocidos — fuf

## BUG-0001 — con λ = 1 la tabla de previsión sale ×100, salvo el nivel

**Encontrado:** 2026-10-17, al planear el gestor de previsiones
(`docs/DISENO-gestor-previsiones.md`), que va a leer la columna ERR.
**Revisado:** 2026-11-13. La primera versión de este fallo decía dos cosas, y
una era falsa (abajo).

### Síntoma

`forecast_table_acii` (`src/usfo.c`) multiplica por 100 las variaciones, sus
desviaciones típicas, la del nivel y ERR. En logaritmos eso las pasa a
porcentaje, que es lo que se quiere. **En niveles (λ = 1) las deja cien veces
más grandes** que en las unidades de la serie. Solo el nivel sale bien.

En `CPI_USA_model_AR`, con el `.fuf.inp` que escribe fue (`fue x -f 24`):

- el índice pasa de 257.208 a 256.974 (−0.234) y la variación dice **−23.4**;
- la d.t. de la primera previsión del nivel es σ = √3148/100 = 0.56 y la
  tabla dice **56.1**;
- ERR va en la misma escala ×100, coherente con esa d.t.

### Lo que no era

La primera versión decía también que, en logaritmos, ERR no estaba en la
escala de σ (`IPC_ES_m01_ar1`: σ = 0.25 y ERR llegaba a 100.4). No es de fuf.
Los `.fuf.inp` con los que se vio (los de `tests/corpus` y
`conformidad/corpus`) llevan **factor de reescalado 1** con parámetros y
varianza estimados con **100**, el de su `.inp` de fue. Con ese fichero la
previsión del IPC cae de 104 a 38 en un mes. El fue de hoy escribe el factor
bien (100), y con él ERR no pasa de 0.74 con σ = 0.25 %. Esos ficheros del
corpus no se tocan: son la línea base byte a byte de la batería, no un
modelo que haya que creerse.

### Por qué importa

El gestor de previsiones lee los errores de los motores en vez de calcularlos
(decisión del analista). Con una serie en niveles leería las d.t. y ERR cien
veces más grandes.

### Reproducción

`bugs/BUG-0001-repro/repro.py`, con los dos `.fuf.inp` escritos por fue desde
`conformidad/corpus`. Sale con 1 mientras, en niveles, la variación no sea la
diferencia de niveles o la d.t. de la primera previsión no sea σ. En
logaritmos (`IPC_ES_m01_ar1`) comprueba que la tabla sale bien, como control.

## BUG-0002 — la tabla de previsión escribe los campos pegados

**Encontrado:** 2026-11-12, en la fase P2 de la previsión (`docs/TODO.md`),
al llevar a drtran el formato de esta tabla. Apuntado el 2026-11-13.

### Síntoma

`forecast_table_acii` (`src/usfo.c`) escribe los campos con anchos fijos y sin
separador (`" %2d/%4d"`, `"%9.4f%7.4f%8.4f%8.4f%9.4f%7.4f"`). Un número que no
cabe en su ancho se come el espacio y se junta con el de al lado:

- un nivel de 1000 o más, con la fecha: `  1/20201990.3293 0.2323 …`;
- una desviación típica de 10 o más, con el campo anterior:
  `  1/2022  66.34572092.7726-4150.4301…` (`IPC_FR_model.fuf` del corpus).

### Por qué importa

La tabla se lee mal a ojo y no se puede leer a máquina partiendo por espacios,
que es como la leen `lib/outfcst` y el gestor de previsiones. Con el nivel en
miles (un PIB, un índice en base 1000) pasa en todas las filas.

En el corpus de `tests/` ya se ve en 24 de las 49 filas de siete salidas de
referencia, pero en todas las desviaciones pasan de 10 por el ×100 de λ = 1
(BUG-0001) o por un `.fuf.inp` con el factor de reescalado mal puesto. El
repro no depende de ninguno de los dos.

### Lo que no se hace

fuf funciona y no se toca sin decidirlo («no tocar lo que funciona»).
drtran y drvarma, que escriben ya esta tabla, dejan un espacio entre campos y
no lo heredan. Arreglarlo cambia las salidas de referencia de fuf.

### Reproducción

`bugs/BUG-0002-repro/repro.py`: corre fuf sobre `forecast_D.1` del corpus
(logaritmos, d = 1), cuya tabla sale bien, y sobre la misma serie por 20, con
niveles de unos 2000. En logaritmos el modelo de las diferencias es el mismo;
solo cambia el ancho del nivel. Sale con 1 mientras alguna fila no se parta en
fecha y sus campos.
