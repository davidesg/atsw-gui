# Fallos conocidos — fuf

## BUG-0001 — el informe de previsión escala mal las variaciones y ERR

**Encontrado:** 2026-10-17, al planear el gestor de previsiones
(`docs/DISENO-gestor-previsiones.md`), que va a leer la columna ERR.

### Síntoma

1. **Con λ = 1 (niveles)** la variación sale multiplicada por 100, como si la
   serie fuera en logaritmos. En `CPI_USA_model_AR` el índice pasa de 256.974
   a 257.971 (+0.997) y la columna dice **99.7**; la anual, 625.9.
2. **La columna ERR** de las observaciones no está en la escala de la σ de la
   entrada. En `IPC_ES_m01_ar1` (logaritmos), σ = √0.0627 = 0.25 y ERR llega
   a **100.4**; en `CPI_USA_model_AR`, a miles.

Con logaritmos las variaciones sí salen bien (−0.98 % en 1/2020).

Es la misma familia que el σ_w del gráfico de residuos (PR #46): una cifra que
supone logaritmos y no mira λ ni el refactor.

### Por qué importa

El gestor de previsiones lee los errores de los motores en vez de calcularlos
(decisión del analista). Con ERR mal escalado leería números falsos.

### Reproducción

`bugs/BUG-0001-repro/repro.py`: sale con 1 mientras la variación en niveles no
sea la diferencia de niveles o |ERR| supere diez veces la σ.

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
referencia, pero todas con λ = 1 o con el `.inp` de `IPC_ES_m01_ar1`, donde
las desviaciones pasan de 10 por el BUG-0001. El repro no depende de él.

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
