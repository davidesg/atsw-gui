# Bug en el motor de previsión de fuf (C) y fue (Python): la deriva de la media

**Estado:** confirmado y parcheado (fuf C).
**Afecta:** `fuf-1.08.1` (`src/usfo.c`, función `forecast`) y `fue` Python
(`fue-0.1/src/fue/forecast.py`, que *"mirrors usfo.c/fuf.c exactly"*).
**No afecta:** drtran (C) ni drvarma (Python) — componen correctamente.
**Síntoma:** el **nivel** previsto sale sistemáticamente alto por una constante
`μ·φ(1)⁻¹·φ` ... más precisamente `μ·φ/(1−φ)` en el caso AR(1); la **tasa anual**
de los primeros `s` pasos también sale alta por el mismo término.

---

## 1. El modelo y la previsión correcta

Para una serie con una diferencia regular (`∇`) y ruido AR(1) con media, el modelo es

```
φ(B) [ ∇Y_t − μ ] = a_t        ⟹      ∇Y_t = μ + φ(B)⁻¹ a_t
```

La previsión puntual de la **diferencia** converge a la media:

```
E[∇Y_{n+l}] = μ + φ^l (∇Y_n − μ)   ⟶   μ
```

y el **nivel** se obtiene integrando (forma cerrada):

```
E[Y_{n+l}] = Y_n + l·μ + (∇Y_n − μ) · Σ_{j=1}^{l} φ^j          (†)
```

Esta es la referencia. La escriben así drtran (`forecast_mean`) y drvarma
(`forecast_w`, *"port of forecast_mean"*): previsión en diferencias con **forma de
media** (`f = μ + φ(w−μ)`) e integración.

## 2. Lo que hace fuf (y fue Python)

fuf pliega la diferenciación en el AR (`varphi`: `phi0(B) = φ(B)(1−B)`) y prevé el
**nivel** directamente. Su composición (`usfo.c` [1]–[2]) es:

```c
// [1] recursion homogenea del nivel, SIN restar la media:
f1[l] = Σ_i phi0_i · (f1[l-i] or w[n-i+l])  −  Σ_j theta_j · a[...]
// [2] deriva anadida como l·mu ACUMULADO:
f1[l] += xius[l+n];                 // determinista (armonicos): OK
if (mu) { s2 += mu; f1[l] += s2; }  // s2 = l·mu  ← EL BUG
```

`fue/forecast.py` reproduce esto literalmente (`s2 += mu; f1[l] += s2`).

### Por qué está mal

`phi0·w` es la solución **homogénea** del nivel, pero se alimenta con las
**condiciones iniciales reales** `w_n, w_{n−1}, …`, que **ya llevan la deriva**
`μ·t`. Sumar `l·μ` encima **cuenta la deriva dos veces** en el transitorio.

Formalmente, el modelo en nivel es `phi0(B) w_t = μ·φ(1) + a_t` (intercepto
constante `c = μ·φ(1)`, con `φ(1)=1−Σφ`). fuf resuelve `phi0(B) w = 0` (homogénea) y
añade `l·μ` — pero la solución particular de `phi0(B)w = μ·φ(1)` es `μ·t`, y usar las
condiciones iniciales sin des-tendenciar deja un residuo `+φ^l·μ` por paso.

### Magnitud del error (AR(1))

```
fuf − correcto  =  μ · Σ_{j=1}^{l} φ^j   ⟶   μ·φ/(1−φ)
```

## 3. Evidencia

| Implementación | Composición de la media | Nivel |
|---|---|---|
| drtran (C) `forecast_mean` | `f = μ + φ(w−μ)`, integra | ✅ correcto |
| drvarma (Python) `forecast_w` | idem (*"port of forecast_mean"*) | ✅ correcto |
| fuf (C) `usfo.c` | homogéneo `phi0·w` + `l·μ` | ✗ sobre-prevé |
| fue (Python) `forecast.py` | *"mirrors usfo.c"*: homogéneo + `l·μ` | ✗ sobre-prevé |

**Caso IPC_ES** (μ=0.154472, φ=0.402839): `μ·φ/(1−φ) = 0.104` en el espacio
transformado (×100 log). Medido, fuf_old − drtran = **0.104** exacto, en todo el
horizonte. Ejemplo a mano (φ=0.5, μ=1, ∇Y_n=3, Y_n=100), fórmula (†):

| l | correcto (=drtran=drvarma) | fuf/fue-py |
|---|---|---|
| 1 | 102.00 | 102.50 (+0.5) |
| 2 | 103.50 | 104.25 (+0.75) |
| ∞ | — | +`μφ/(1−φ)`=1.0 |

## 4. El parche (fuf C)

Añadir el intercepto correcto `c = μ·φ(1)` **dentro** de la recursión, y quitar el
`l·μ`. Aplicado en `fuf-1.08.1-fix`:

`usfo.c` [1]:
```c
f1[i1][l] = vtmp1[i1] - vtmp2[i1] + drift[i1];   // + mu*phi(1)
```
`usfo.c` [2] (solo el determinista, sin `l·μ`):
```c
if (has_deterministic)
    for (l=1;l<=L;l++) for (k=1;k<=m;k++) f1[k][l] += xius[k][l+n];
```
`fuf.c` (calcula el intercepto por serie y lo pasa):
```c
drift[k] = varma1.mu[k] * (1.0 - Σ_i varma1.phi[i][k][k]);   // mu·φ(1)
```
(El intercepto `μ·φ(1)` es válido para cualquier orden de diferenciación, porque
`phi0(B)w = φ(B)∇^d w = φ(B)s = φ(1)μ + a`.)

`fue/forecast.py` necesita el mismo cambio: en el bloque [7], sustituir
`s2 += mu; f1[l] += s2` por añadir `mu*(1 − sum(phi_coefs))` a `f1[l]` dentro de la
recursión [6].

## 5. Validación

Con el fuf parcheado, el nivel de IPC_ES coincide con drtran (y con la fórmula
cerrada †) en todo el horizonte; el exceso de 0.104 desaparece. Las tasas mensual y
anual también quedan correctas (fuf_old las tenía altas por 0.104 en los primeros
`s` pasos).

```
             fuf_old   fuf_fix   drtran
h=1 nivel    82.0316   81.9805   81.98   ✓
anual(h=12)  1.8747    1.7705    1.77    ✓
```

## 6. Nota

El bug solo se manifiesta con **media no nula** (`μ≠0`). Los modelos m6 del legacy
(`forsil`/`msfo`) tienen `μ=0`, por eso nunca lo expusieron. WTI (`μ≈0`) tampoco.
IPC_ES (`μ=0.154`, inflación) sí.
