# `outvarma` — el VARMA de la escalera, leído de lo que escribe drvarma

Es el lector de la fase S1 de `docs/DISENO-sim.md`. Las páginas de sim_gui,
`atsw-cli` y el asistente leen con él lo que escribe drvarma en la escalera:

    drvarma A.pre B.pre … p q [-diagcov] [-links …] [-forecast H [-estwin N]] -o NOMBRE
      → NOMBRE.out   NOMBRE.forecast   NOMBRE.recursive

## Aquí no se calcula nada

Como `lib/outdiag` y `lib/outfcst` para drtran: el motor ya lo calcula todo y
esto solo lee. **La librería devuelve el hecho, no la frase.** Cómo se dice lo
deciden la página, la orden o el asistente.

Del `.out` se leen:

| | qué |
|---|---|
| la cabecera | p, q, los enlaces, Σ completa o diagonal, si los deterministas y los ARMA univariantes van fijos o se reestiman, la ventana común, y la tabla de modelos univariantes con su `.pre` |
| **la puerta** | por serie, la logL, σ² y cuánto se movió del `.pre`, con la razón: la muestra recortada por la ventana común, o «no es un óptimo» (un `.inp`); la suma, la conjunta, la diferencia y si pasa |
| el optimizador | convergió, se paró, o **se paró en la pared MA** (no es un óptimo aunque salgan números); o la estimación falló |
| los parámetros | nombre, estimación, error típico, t y p, tal cual |
| la LR | el sistema contra el diagonal (los univariantes) |
| Σ | la matriz y las correlaciones |
| las raíces | cuántas y el módulo mayor, de \|φ(B)\| y de \|θ(B)\| |
| la IRF y la FEVD | la ortogonalizada (Cholesky, en el orden del sistema) y los porcentajes |
| la diagnosis | la de `lib/outdiag`, que ya lee este `.out`: los residuos de cada serie, Hosking y el Jarque-Bera multivariante |
| la previsión | por serie, en su nivel, con la banda del 95 % |
| la evaluación | recursiva, por serie y horizonte (con `-estwin`) |

Y aparte, el `.forecast` (las mismas tablas de previsión) y el `.recursive`
(una fila por origen, serie y horizonte).

## La prueba, contra ficheros de verdad

El motor no emite nada legible por máquina, así que esto lee texto
formateado, que es frágil por naturaleza. La defensa es la de `outdiag`:
`run_test.sh` pide a drvarma cinco salidas con los `.pre` de Jenkins y Alavi
(`datos/`, los del ejemplo `jenkins_alavi` de sima) y `test_outvarma.c` las lee:

- la puerta sola (p = q = 0, Σ diagonal);
- el AR(2) cruzado, con su `.forecast`;
- el mismo con `-estwin`, con su `.recursive`;
- un VARMA(1,1) con un solo enlace, que se para en la pared MA;
- la puerta con un `.pre` retocado, que ya no es un óptimo.

Lo corre la batería de drvarma (`engines/drvarma/tests/run_tests.sh`): si el
formato cambia, se sabe el mismo día. Lo que se exige son sobre todo hechos
de forma y unos pocos números con tolerancia amplia, porque fuera de Linux
otra libm puede acabar en otro sitio.
