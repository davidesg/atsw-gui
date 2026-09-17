# `outfcst` — la previsión, leída del `.out`

Como `lib/outdiag`: el motor lo calcula todo y aquí sólo se lee. Y por la misma
razón —el motor no emite nada legible por máquina— la prueba se corre contra
`.out` **de verdad, recién escritos por el motor en el mismo banco**.

## Tres cosas distintas, y la pantalla no puede mezclarlas

**1. La previsión**, por serie: el nivel y su variación —del periodo y anual—
cada una con su desviación típica. Es el formato de fuf/forsil.

**2. La descomposición de la varianza** del error: de qué innovación viene cada
parte. El motor la da **sólo si Σ es diagonal**, y si no, **se niega y explica
por qué**:

> con innovaciones correlacionadas la descomposición NO ES ÚNICA — hay que darle
> la parte común a alguien, y eso exige una ORDENACIÓN (Cholesky). Es
> exactamente el problema del VAR. Se evita mientras Σ sea diagonal, y se
> declara cuando no lo es.

Esa negativa **hay que conservarla**: es un resultado, no un hueco. Por eso
`decomp` vale `0` cuando se negó y `-1` cuando ni siquiera venía.

**3. La evaluación fuera de muestra**: MAE, RMSE y MAPE por horizonte, con los
parámetros estimados **una vez** sobre la ventana y luego **clavados** mientras
el origen rueda.

## La distinción que hay que gritar

> Las desviaciones de (1) son **TEÓRICAS**. Dicen lo que el modelo implica, no
> lo que pasa fuera de muestra, donde la incertidumbre de los parámetros y el
> cambio estructural tienen su parte.

(3) es la única respuesta **empírica**, y la única forma de decidir si un modelo
predice mejor que otro. No el ajuste, no la verosimilitud, no las bandas.

Y es lo que **TASTE no podía hacer**: tenía una sola ranura de residuos
(`TASTECTV.PAS:475`), así que estimar un segundo modelo borraba el primero y no
había con qué comparar. La pantalla guarda la evaluación de una corrida y pone
la diferencia relativa del RMSE al lado de la siguiente.

## Un detalle de formato que costó

La tabla lleva líneas `+---` **por dentro de su propia cabecera**:

```
   +--------------------------------------------------------------+   <- ¿abre?
   |         |      LEVEL       |           VARIATION      |       |
   |  DATE   +-----------------+-------------------------+  ERR   |   <- y aquí
   |         |   VALUE  |  STD |  PERIOD |  STD | ANNUAL  |  STD   |
   +--------------------------------------------------------------+   <- ¿cierra?
    3/1991   1290.78    -      14.95 ...                              <- los datos
```

Un interruptor «estoy dentro de la tabla» queda **invertido justo para los
datos**, y además deja el resto del fichero fuera de juego. Una fila se reconoce
por lo que la hace fila: **empieza por una fecha**. Sin estado, sin ambigüedad.

La otra ambigüedad del formato es `-`, que significa **hueco** y no número
negativo. Se resuelve mirando si el campo es exactamente `"-"`. De ahí sale la
distinción que las pruebas exigen:

- una fila **observada** no trae desviación y sí trae error;
- una fila **prevista**, al revés.

Si el lector las mezclara, se dibujarían bandas sobre el pasado.

## Las pruebas que no son de formato

Además de los números exactos:

- la desviación del nivel **crece** con el horizonte;
- el RMSE **crece** con el horizonte;
- **RMSE ≥ MAE** en todos los horizontes — por Jensen, siempre; si saliera al
  revés, las dos columnas estarían cambiadas.

Y el banco comprueba que el CSV por origen tiene **31 × 6 filas más la
cabecera**.
