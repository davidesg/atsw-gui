# `lib/ccfplot` — partes

## BUG-0001 — Con datos mensuales y 12 retardos el eje marca cada 4; GraphMaker marca cada 12

- **Estado:** abierto
- **Gravedad:** baja (es la forma del gráfico, no sus números)
- **Encontrado:** 18-nov-2026, al llevar la CCF de los portes a Python
  (pyfug 2.1.0, `pyfug.graphics.ccf`) y comprobarla contra este C.
- **Repro:** `bugs/BUG-0001-repro/repro.py` (sale 1 mientras siga abierto).

### Qué pasa

El panel mensual por defecto —12 retardos, los de `ccf_lags_por_defecto`—
rotula el eje x en −12, −8, −4, 0, 4, 8, 12. GraphMaker lo rotula en −12, 0, 12.
Lo mismo con cualquier mensual de 9 a 20 retardos.

### El oráculo

GraphMaker, `SRC/GraphMaker/Projects/graphmakertri2/ccfgrafico.cpp`,
`TccfForm::FormCreate`. El incremento del eje lo decide la **frecuencia**:

| frecuencia | retardos | incremento |
|---|---|---|
| anual | 7 | 2 |
| trimestral | 15 | 4 (el de la ficha, por defecto) |
| mensual | 9 a 39 | 12 |
| mensual | 8 | 2 |

Decisión del analista (18-nov-2026): mensual cada 12, trimestral cada 4, y en
las demás frecuencias manda GraphMaker. GraphMaker sólo conoce esas tres; para
otra frecuencia no hay oráculo.

### La causa

`paso_eje` decide por el **número de retardos**: `<= 8` → 2, `<= 20` → 4, si no
12. Su propio comentario dice lo correcto («2 al año y 12 al mes; al
trimestre, 4»), pero el código no lo hace. Y `ccf_draw` no recibe la
frecuencia, así que con los mismos retardos no puede distinguir un mensual de
un trimestral.

### Arreglo propuesto

Pasar la frecuencia a `ccf_draw` y `ccf_write_eps`, y decidir el paso como
GraphMaker: anual 2; trimestral 4; mensual 12, salvo con 8 retardos, que es 2.
Los portes a Python (`pyfug.graphics.ccf.draw_ccf_panel`) ya siguen esa regla.
