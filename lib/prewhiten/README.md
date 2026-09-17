# `prewhiten` — el preblanqueo y la CCF que identifica

Esto **no es código nuevo**. Es el núcleo numérico de la identificación que ya
estaba dentro de `engines/drtran/src/drtran.c`, sacado a un fichero propio para
que el GUI pueda llamarlo sin arrastrar el `main()` del motor. Misma razón por
la que `DateToObs` salió a `lib/dates`, y el mismo criterio de siempre: **un
séptimo lector, no; una séptima cuenta, tampoco.**

## Qué se movió

| de `drtran.c` | qué es |
|---|---|
| `total_ar_order` / `total_ma_order` | el orden **expandido**: un factor anual de orden p ocupa `p·sper` |
| `expand_ar_factors` / `expand_ma_factors` | los factores del `.pre` multiplicados en un polinomio |
| `apply_univariate_model` | Box-Cox, restar deterministas, diferenciar |

Las cuatro eran **puras** — no tocaban un solo global del motor. `prewhiten_ccf`
es lo nuevo aquí, y son los pasos 1–4 de `prewhiten_and_identify` con sus
argumentos en vez de los globales (`lnk`, `w`, `phi`, `theta`, `n_stat`, `Ts`).

## El procedimiento

    1. la ENTRADA se preblanquea con SU PROPIO ARMA:   a_t = φ(B)/θ(B) w_X
    2. la SALIDA se filtra con EL MISMO filtro:      β_t = φ(B)/θ(B) w_Y
    3. r(k) = corr( β_t , a_{t−k} )

    k > 0   la salida responde a la entrada: LA TRANSFERENCIA
    k < 0   la salida antecede: RETROALIMENTACIÓN, y ahí no debería haber nada

## Por qué el paso 1 no es opcional

La CCF de las series crudas no identifica: la autocorrelación de cada una se
propaga a la cruzada. Medido sobre ES_CPI contra WTI:

| | r(0) | retardos k≥0 fuera de banda | Hosking |
|---|---|---|---|
| **cruda** | 0.4625 | **25 de 25** | P(96) = 5231.3 |
| **preblanqueada** | 0.3542 | 4 | P(96) = 138.4 |

Preblanquear exige el modelo univariante de cada serie, y eso es exactamente lo
que trae un `.pre`. **Aquí es donde la escalera se paga sola.**

## La prueba tiene oráculo

El motor imprime sus propios números:

    ./bin/drtran -p tests/cases/ES_CPI_airline.pre tests/cases/WTI_ar1.pre

y `gui/drtran/tests/test_prewhiten.c` exige esos, al dígito escrito:

    observaciones estacionarias     203        ok
    banda 2/sqrt(N)                 0.14037    ok
    r(0)  r(1)  r(12)  r(13)        0.3542  0.2167  -0.3282  -0.1797    ok
    ν(0)  ν(12)                     0.0151  -0.0140                     ok
    retardos negativos fuera         0         ok

Y una que no es de igualdad: exige que la CCF **cruda** dé otra cosa. Si diera
lo mismo, preblanquear no estaría haciendo nada y la prueba de arriba no
probaría nada.

La batería del motor sigue en **304 PASS, 0 FAIL** después de la mudanza.
