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

## BUG-0002 — Con λ = 1 la serie se transforma como refactor·(y − 1); fue, como refactor·y

- **Estado:** abierto
- **Gravedad:** media. Con la media estimada solo la desplaza y no cambia la
  verosimilitud. Con la media fija, o sin media, en una serie en niveles
  (λ = 1, d = D = 0), el sistema lee otro modelo: números falsos.
- **Encontrado:** 19-nov-2026, al preparar la ruta V. La puerta marcaba
  «<- not an optimum» con un movimiento de 100 en .pre que son óptimos de fue,
  y en el lado de Python el movimiento era 0.
- **Repro:** `bugs/BUG-0002-repro/repro.py` (sale con 1 mientras siga abierto).

### Qué pasa

fue (`engines/fue/src/fue.c`, `BoxCox`) transforma con λ = 1 como
`refactor*y`: en niveles no resta nada. `apply_univariate_model` y
`fuepre_bc` (`lib/fuepre/fuepre_forecast.c`) usan la Box-Cox de libro también
con λ = 1: `refactor*(y − 1)`. La diferencia es una constante, el factor de
reescalado (100).

- **Con la media estimada**, la media de la escalera sale 100 por debajo de
  la del .pre, y la puerta dice que el .pre no es un óptimo: en el repro, el
  gas y el CO2 se mueven 100. La verosimilitud es la misma.
- **Con la media fija** (el CO2 con su media a 5350.912162 y marcada 0), la
  puerta lee otro modelo: logL −2140.941448 frente a −2127.124082 de fue, y σ²
  112189 frente a 102189 (100² más).
- **La previsión no se ve afectada**: `fuepre_bc_inverse` deshace la misma
  transformación.

### El oráculo

fue, el dueño del formato. fuf hace lo mismo (`fuf.c`, [4.3]), y también
drvec (`boxcox_w`) y los portes de Python (`fue.cast_us`), que por eso no
tienen el defecto.

### La causa

`apply_univariate_model` y `fuepre_bc` no tienen la rama de λ = 1 que tienen
fue, fuf y drvec.

### Arreglo propuesto

Añadir la rama λ = 1 (`refactor*y`) en `apply_univariate_model` y
`fuepre_bc`, y su inversa en `fuepre_bc_inverse` (`ystar/refactor`, con
jacobiano `1/refactor`). Las salidas de referencia de drtran, drvarma y drvec
con series en niveles y media cambiarán en la media (100 más), y habrá que
regenerarlas.
