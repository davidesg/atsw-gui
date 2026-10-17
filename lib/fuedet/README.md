# `fuedet` — la componente determinista de un modelo de fue

El filtro ω(B)/δ(B) de las variables deterministas de un `.pre`, con **la
regla de fue**: el paso [5] de `cast_us` en `engines/fue/src/fue.c`, el
código de Mauricio.

    ruido_t = dato_t − Σ_j ν_j(B) x_{j,t}

- ν_j(B) = ω_j(B)/δ_j(B), con los signos de Box-Jenkins, por `calcnu`;
- ν_j se **trunca** en Nomega_j retardos si la variable no tiene δ, y en
  **40** si lo tiene;
- las variables se **suman** (`tmp2` por variable, `tmp1 += tmp2`) y la suma
  se resta **una vez**.

El código es el de fue.c, con el mismo orden de bucles y la misma
acumulación: un orden distinto da otros bits. fue.c lo llama desde aquí, y sus
109 ejecuciones de `tests/run_tests.sh` salen byte a byte iguales que antes.

## API

| función | qué |
|---|---|
| `fuedet_nulag(nomega, ndelta)` | el retardo máximo de ν: Nomega o 40 |
| `fuedet_nu(omega, s, delta, r, nu, lags)` | `calcnu` de fue.c, tal cual |
| `fuedet_sum(nvar, omega, nomega, delta, ndelta, x, n, det)` | det[t] = Σ_j ν_j(B) x_j[t] |
| `fuedet_noise(..., x, data, n, out)` | out[t] = data[t] − det[t], la resta de fue.c |

Índices de fue: variables 1..nvar, tiempo 1..n; `omega[j][0..nomega[j]]`,
`delta[j][1..ndelta[j]]`. C puro, sin el `real` ni los asignadores del
anfitrión: por eso se puede copiar tal cual.

## Quién lo usa

| quién | para qué |
|---|---|
| `engines/fue` (`cast_us` [5]) | la estimación |
| `engines/fuf` (`cast_us` [5]) | la previsión, con el horizonte (fuf 1.10; antes truncaba en 20) |
| `lib/fuepre` (`build_det_component`) | la vuelta al nivel de las previsiones de drtran y drvarma, y los GUIs |
| `lib/prewhiten` (`apply_univariate_model`) | el cast de drtran y de la escalera de drvarma, y la CCF preblanqueada del GUI de drtran |
| `engines/drvec` (su `fue_pre_reader.c`) | la ruta `.pre`, `-interv` y `-xpre` |
| paquete de Python de fue (`csrc/fuedet.[ch]`) | `cast_us_py` y `forecast._build_xi`, por cffi |

Fuera de esto quedan las funciones de transferencia de las relaciones —los
enlaces entrada→salida de drtran y las entradas `-xlink` de drvec—, que
siguen con la recursión exacta, sin truncar.

## La copia del paquete de Python

`atws/fue/fue/csrc/fuedet.c` y `fuedet.h` son copias **byte a byte** de estos
dos ficheros, y una prueba del paquete compara su sha256 con el apuntado. Si
se cambia aquí, se copia allí y se apunta el sha256 nuevo. Dentro de este
repositorio no hay copia: `conformidad/copias.sh` avisa si aparece un
`fuedet.c` fuera de `lib/`.

## Por qué 40 y no la recursión exacta

Hasta 2026-10-16 los hermanos de fue (drtran, drvarma, drvec) filtraban con
la recursión exacta y fuf truncaba en 20. Con un δ próximo a uno y más de 40
periodos tras el suceso, el `.pre` que fue escribió como óptimo dejaba de
serlo al leerlo otro programa (BUG-40 de drvec: un escalón con δ = 0,95
movía el logL 0,47 y cambiaba el veredicto del rango). La decisión fue la de
fue: su regla en todas partes, y fue sin tocar.

`test_fuedet.c` lo comprueba contra el código de fue.c, bit a bit (lo corre
`gui/drtran/tests/run_tests.sh`).
