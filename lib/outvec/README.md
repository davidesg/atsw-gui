# `outvec` — el rango y el VEC, leídos de lo que escribe drvec

Es el lector de la fase S10a de `docs/DISENO-sim.md`, el par de
`lib/outvarma` para drvec. La página R · Rango de sim_gui, `atsw-cli` y el
asistente leen con él las dos salidas de drvec por la ruta de los `.pre`:

    drvec A.pre B.pre … p q 0 -case c -lrtest [-bootstrap B] -name RANGO
      → RANGO.out   RANGO.boot (una réplica por línea, mientras corre)
    drvec A.pre B.pre … p q r -case c [-fdhess] [-weakex i | -fixb2 v | …] -name VEC
      → VEC.out

## Aquí no se calcula nada

El motor ya lo calcula todo y esto solo lee. **La librería devuelve el hecho,
no la frase**: cómo se dice lo deciden la página, la orden o el asistente.

| | qué |
|---|---|
| la cabecera | el programa, VARMA-VECM(p,q), el rango (o «prueba del rango»), el caso determinista, las series **en el orden de la orden** y cuántas van en el bloque ∇Y₂, las observaciones |
| **la puerta** | la escalera bajo el rango la comprueba siempre: la suma de los univariantes, la conjunta, la diferencia y si se verifica. A diferencia de drvarma, una puerta que no pasa **no para** a drvec: el que lee tiene que mirarla |
| el optimizador | cuántos ajustes convergieron o se pararon, el último (el del ajuste) con su criterio, y la búsqueda: cuántos arranques y entre qué logL |
| la prueba del rango | logL, npar, AIC y BIC por rango; la LR de cada comparación r → r+1 con los valores asintóticos y, con `-bootstrap`, los del bootstrap con su p-valor y sus réplicas |
| el VEC | σ², logL, npar, AIC, BIC; α y β (M × r), E[W] y Σ |
| los parámetros | nombre, estimación, error típico, t y p, con la sección a la que pertenecen (α, β, Γ, Q…) |
| las restricciones | la LR de cada una (débil exogeneidad, `-fixb2 v`, …): libre, restringida, LR, gl y p |

Y dos ayudas:

- `ovec_rango_propuesto`: la regla de sima (`propose_rank`). Es el primer r
  cuya comparación **no** se rechaza al 5 %, con los valores del bootstrap si
  los hay. Si todas se rechazan, **al menos** M − 1, porque drvec no
  contrasta el rango completo.
- `ovec_boot_hechas`: cuántas réplicas lleva el `.boot` de un bootstrap en
  marcha. El bootstrap tarda minutos, y la página enseña el progreso con esto.

## El orden importa, y la normalización también

Las primeras M − r series forman el bloque ∇Y₂ y las últimas r forman Y₁, sobre
el que se normaliza β. α va con el signo de Johansen: **negativo corrige el
error**. Con estos datos, **la logL exacta del VEC cambia con la serie que va
en Y₁**: −322.82 con IDE, −324.35 con LRM y −323.80 con LRY, cada una
convergida igual desde tres arranques. Solo son comparables por LR los ajustes
hechos en el mismo orden.

## La prueba, contra ficheros de verdad

`run_test.sh` pide a drvec del árbol tres salidas con los `.pre` de Johansen y
Juselius (`datos/`, los del ejemplo `johansen_juselius` de sima, **sin la línea
«Built by sima's raw entry…»** que sima pone en la cabecera: fue y drvarma
rechazan un `.pre` con esa línea de más; drvec la tolera): la prueba
del rango **sin** bootstrap (2 s), el VEC con r = 1 normalizado sobre LRM (con
`-fdhess`, como sima) y el mismo con IBO débilmente exógena.
`test_outvec.c` las lee y comprueba las cifras del tutorial de sima:

- las LR de la prueba: 25.91, 21.22 y 4.07;
- β: LRM = 1.12 LRY − 5.16 IBO + 4.26 IDE;
- α en LRM: −0.17, con t −2.27;
- la LR de la débil exogeneidad de IBO: 0.0003, con p 0.99.

El bootstrap tarda: 6 minutos con 99 réplicas. Por eso su tabla se lee de una
salida guardada, `datos/rango_b99.out`, con su `.boot`. La prueba corre dentro
de la batería de drvec (`engines/drvec/tests/run_tests.sh`): si el formato del
`.out` cambia, se sabe el mismo día.

Sin dependencias: libc y libm.
