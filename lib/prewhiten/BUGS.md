# `lib/prewhiten` — partes

## BUG-0001 — Una serie con datos ≤ 0 no entra aunque λ = 1 (ARREGLADO)

- **Estado:** arreglado el 19-nov-2026 (#196)
- **Gravedad:** media (la serie no entra; no da números falsos)
- **Encontrado:** 19-nov-2026, al comprobar la ruta V (Tiao y Box) en drvarma C
  con el horno de gas, el ejemplo `tiao_box` de sima.
- **Repro:** `bugs/BUG-0001-repro/repro.py` (salía con 1 mientras estuvo abierto; ahora sale con 0).

### Qué pasa

`apply_univariate_model` rechaza cualquier dato ≤ 0 antes de la Box-Cox, sea
cual sea λ:

    Error: dato no positivo para Box-Cox (t=1, y=-0.109)
    ERROR: could not build the stationary series

Con λ = 1 la transformación es (y − 1)/1, que no pide datos positivos. fue
estima ese mismo fichero sin problema. La librería es el reparto de la escalera
de drvarma, de drtran y de la entrada exógena de drvec (`-xpre`), así que
ninguna serie en niveles con negativos entra en un sistema ni en una
transferencia: un caudal centrado, un saldo o una tasa de variación. Tampoco
la serie J de Box y Jenkins, que es su propio ejemplo de función de
transferencia.

### El oráculo

- **fue**, el dueño del formato, acepta λ = 1 con datos ≤ 0.
- **drvec C**, en su propia ruta (`boxcox_w`, `drvec.c`), rechaza un dato ≤ 0
  solo cuando λ ≠ 1.
- **Los portes en Python**: `fue.cast_us`, drvarma-python (la escalera) y
  drtran-python lo aceptan. drvec-python (`data.py`, `_check_positive`) lo
  rechaza solo con λ ≠ 1, igual que drvec C. Ningún porte tiene este defecto.
  Comprobado el 19-nov-2026 con los ficheros del repro.

### La causa

La comprobación `y <= 0.0` no mira λ.

### El arreglo

Un dato ≤ 0 se rechaza solo cuando |λ − 1| > 1e-12, como en `drvec.c` y en
drvec-python. Con el arreglo, el gas entra en la puerta de drvarma y en
drtran: con los órdenes de Box y Jenkins (b = 3, r = 1, s = 2), ω₀ = −0.57 y
δ₁ = 0.60. El repro comprueba además que con λ = 0 (logaritmos) el gas se
sigue rechazando.

## BUG-0002 — Con λ = 1 la Box-Cox de la librería resta 1; la de fue, no

- **Estado:** abierto
- **Gravedad:** media. Con d = 0 y media, un `.pre` de fue no es un óptimo
  para la escalera. Con d ≥ 1 no se nota, porque la diferencia elimina la
  constante.
- **Encontrado:** 20-nov-2026, al homologar el arranque `-start ols` de la
  escalera de drvarma con el porte en la ruta V. El logL era idéntico, pero las
  medias diferían en exactamente 100.
- **Repro:** `bugs/BUG-0002-repro/repro.py`, con los ficheros de BUG-0001.
  Sale con 1 mientras siga abierto.

### Qué pasa

Con λ = 1, `apply_univariate_model` (`prewhiten.c`) y `fuepre_bc`
(`fuepre_forecast.c`) transforman con refactor·(y − 1). fue transforma con
refactor·y. Con d = 0 la diferencia (el factor, 100) cae entera en la media.

La puerta de la escalera de drvarma, con el horno de gas de BUG-0001 (dos
`.pre` que escribió fue, en niveles y con media):

    GAS   ...   max|move| 100   <- not an optimum
    CO2   ...   max|move| 100   <- not an optimum

La escalera tiene que mover μ exactamente el factor para reajustarlos. La
puerta pasa, pero porque compara con esos univariantes reajustados, no con
los del fichero. En la ruta V del gas furnace, el C y el porte dan μ distintas
en 100 con el mismo logL.

### El oráculo

- **fue (C)**, dueño del formato: `BoxCox` de `engines/fue/src/fue.c` con
  λ = 1 hace `refactor*DataInput[i]`.
- **drvec (C)**: `boxcox_w` (`drvec.c`) hace `z * refac` con λ = 1.
- **Los portes en Python** (`fue.forecast._boxcox`, y con él `fue.cast_us`,
  drvarma, drtran y sima) hacen refactor·y.

Solo `lib/prewhiten` y `lib/fuepre/fuepre_forecast.c` restan 1. El comentario
de `prewhiten.c` («con λ = 1 la transformación es (y − 1): un dato ≤ 0 vale,
como en fue y en drvec») es cierto en lo de los datos ≤ 0 (BUG-0001), no en la
fórmula.

### Alcance

La librería es el reparto de los tres motores de la escalera en C: drvarma
(`escalera.c`), drtran y la entrada exógena de drvec (`-xpre`). `fuepre_bc` y
su inversa los usan las previsiones de drvarma y de drtran. Con λ = 1, d = 0 y
una media **fija** en el `.pre` (no reestimada), el desplazamiento no se
compensa, y el modelo es otro.

### Arreglo propuesto (pendiente de confirmación: toca la librería de tres motores)

Con λ = 1, refactor·y, como fue. En `prewhiten.c`, y en `fuepre_bc` y
`fuepre_bc_inverse` (la inversa con λ = 1: y = y*/refactor). Después:
- los bancos de los tres motores;
- regenerar las referencias que tengan series con λ = 1 y d = 0, cuya μ
  cambiaría en el factor, y comprobar que nada más se mueve.
