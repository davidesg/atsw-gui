# `lib/prewhiten` — partes

## BUG-0001 — Una serie con datos ≤ 0 no entra aunque λ = 1

- **Estado:** abierto
- **Gravedad:** media (la serie no entra; no da números falsos)
- **Encontrado:** 19-nov-2026, al comprobar la ruta V (Tiao y Box) en drvarma C
  con el horno de gas, el ejemplo `tiao_box` de sima.
- **Repro:** `bugs/BUG-0001-repro/repro.py` (sale con 1 mientras siga abierto).

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

### Arreglo propuesto

Rechazar un dato ≤ 0 solo cuando |λ − 1| > 1e-12, como `drvec.c` y
drvec-python. El repro comprueba además que con λ = 0 (logaritmos) el gas
se sigue rechazando.
