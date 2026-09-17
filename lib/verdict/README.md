# `verdict` — cómo acabó el optimizador

TASTE distinguía **tres** desenlaces y les ponía nombre (`MRQEST.PAS:372-376`):
`CONVERGENCIA`, `ALCANZADO EL Nº MAXIMO DE ITERACIONES`, `LONGITUD DE PASO MUY
PEQUEÑA`. Era de lo mejor de su diseño: **distingue converger de rendirse, y
distingue dos formas de rendirse.**

drtran tiene **cinco**, más el `ifault` del evaluador de la verosimilitud
(`drtran.c:3272-3282`):

| | qué pasó |
|---|---|
| 1 | convergencia por el **gradiente** |
| 2 | convergencia por el **parámetro** |
| 3 | parado sin mejora — *lo normal si se arranca **en** el óptimo* |
| 4 | NO converge: límite de iteraciones |
| 5 | NO converge: cinco pasos seguidos de longitud máxima |

Aquí **no se reimplementa ningún criterio de parada**: se lee lo que el motor
escribe. El motor es quien sabe cómo acabó; el GUI sólo tiene que no perderlo.

## Por qué un módulo y no tres greps

Por el **caso 3**. El motor lo titula:

    **** STOPPED AT A POINT WITH NO IMPROVEMENT AFTER 1 ITERATIONS (of 500)
    **** last global step failed to locate a lower point (usual when starting AT the optimum)

Suena a fracaso. Es lo que sale cuando el `.pre` **ya era** el óptimo — que es
exactamente la invariante del contrato: *corre el motor sobre un `.pre` y los
números no se mueven*. Tomarlo por fallo sería **confundir el éxito con el
fracaso**, y es un error que se comete una vez y cuesta una tarde.

`verdict_ok()` lo cuenta como óptimo, y la pantalla lo explica.

## Las pruebas

Los textos no están inventados: son los que drtran escribe. Los dos primeros se
comprobaron corriendo el motor; los otros se arman con las frases exactas de
`drtran.c:3272-3282`, que es donde el motor las elige.

Y hay dos que merecen nombrarse:

- **Los cinco caen en cajones distintos.** Si dos colapsaran en el mismo, la
  pantalla diría lo mismo de cosas distintas y nadie se enteraría.
- **Se lanza el motor de verdad.** `gui/drtran/tests/run_tests.sh` corre drtran
  sobre el m6 y exige que las iteraciones que lee `verdict_parse` sean las que
  el motor escribió, sacadas de su salida con `sed`. Si un día el motor cambiara
  cómo lo dice, esto se entera.

```
frase: **** CONVERGENCE OBTAINED AFTER 349 ITERATIONS
ver: 2   iters: 349   ifault: 0   ok: 1   logl: -1701.348826
las iteraciones leidas son las que el motor escribio     ok
```

## Un detalle de formato que hay que aguantar

El motor escribe el desenlace **dos veces y no igual**: en el `.out` con
`(of 500)` y por pantalla sin él (`drtran.c:3289` frente a `:3562`). El parser
acepta las dos y pone `maxits = -1` cuando no se dice — **−1, no 0**, porque
«no lo sé» y «cero» no son lo mismo.
