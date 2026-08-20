# Forecasting: the algorithms, and where each piece comes from

*What `-f H` computes, step by step, and which parts are the suite's and which
are `drvec`'s own. Written because the second question is the one that decides
whether a piece should exist here at all.*

---

## 1. The short answer on provenance

Forecasting a VARMA is standard, and the suite already implements it. The
division is:

| step | whose it is |
|---|---|
| the MA(∞) weights `Ψ_l` | **`drvarma`'s**, taken verbatim: `compute_psi_weights`, which lives in `drtran/src/forecast.c` under a header that reads *"part of drvarma"* — the same lineage as `elfvarma.c`. It is copied character for character |
| the point recursion | the same algorithm as that file's `forecast_mean`, written here because it runs on `Ȳ` and because that function is `static`. The mean convention is the engine's: `(w − μ) = Σφ(w − μ) + a − Σθa` |
| the error variance of the **stationary** system | the standard `Σ_{j<l} Ψ_j Σ Ψ_j′`, which is what that file's `forecast_variance` computes |
| **the step to levels** | **`drvec`'s own, and it has to be.** See §4 |

The first version of this section reimplemented `compute_psi_weights` instead of
taking it. That is a third copy of a shared function and exactly the drift
[PLAN_PRODUCCION.md](PLAN_PRODUCCION.md) P3 exists to stop; it was replaced by
the suite's on 2026-08-20 and the forecasts did not move by a digit. If it is
fixed there, it has to be fixed here.

---

## 2. Where the forecast happens: on `Ȳ`, not on `Y`

The fitted model **is** a stationary VARMA on `Ȳₜ = (∇Y₂ₜ′, Wₜ′)′`. That is the
whole point of Mauricio's transformation, and it means the forecast needs no
special theory: it is an ordinary VARMA forecast, and the non-stationarity is
put back afterwards by inverting the transformation.

This is the same route `simulate_h0` already takes for the bootstrap
(`drvec.c`), so there is no parallel code that could drift from it.

**The recursion**, for `h = 1 … H`:

```
Ŷ̄_{n+h} − μ  =  Σ_{k=1..p} Φ*_k (Ȳ_{n+h−k} − μ)  −  Σ_{j=1..q} Θ*_j Â*_{n+h−j}
```

with `Ȳ_t` the data for `t ≤ n` and the forecast for `t > n`; `Â*_t` the fitted
residuals for `t ≤ n` and **zero** for `t > n`. `Φ*`, `Θ*` and `μ` are the
transformed system's, which is what `vec_shootx` leaves in the `Tvarma`.

---

## 3. The certificate on the recursion

The recursion is run **backwards inside the sample** and checked against the
engine:

```
max_t | Ȳ_t − Â*_t − pred(t | t−1) |
```

over the second half of the sample. The residuals come from `elf` (AS 311), not
from this routine, so the check is not circular: it ties the recursion, the mean
convention and the indexing to the engine in one number, printed with every
forecast.

Measured on mink–muskrat, `p=2 q=1 r=1 -case 2`:

| | |
|---|---|
| as it stands | **3.5·10⁻⁴** |
| with `-m 2`, which switches the `ξ` truncation off | **1.8·10⁻¹⁵** |

So the residue **is the truncation**, of order `xitol = 10⁻³`, and not an error
— the same mechanism [HOMOLOGATION.md](HOMOLOGATION.md) §1b measures on the
factorisation identity. Both directions are in the suite.

---

## 4. The step to levels, and why it is not borrowed

`drtran` and `drvarma` integrate a forecast with a **scalar** differencing
operator, `Δ(B) = (1−B)^d (1−B^s)^D`, applied series by series. `drvec`'s
integration is not of that shape:

* only the `Y₂` block is differenced;
* the `W` block is **not** differenced at all;
* and `Y₁ = W − B₂′Y₂` **couples the two**.

So `forecast_level_variances` does not apply, and adopting it would be adopting
a formula for a different problem. What is done instead:

```
Ŷ₂_{n+h} = Y₂_n + Σ_{i≤h} Ŷ̄_{n+i}[1..s]        (cumulate the ∇Y₂ block)
Ŵ_{n+h}  = Ŷ̄_{n+h}[s+1..M]
Ŷ₁_{n+h} = Ŵ_{n+h} − B₂′ Ŷ₂_{n+h}
```

which is the inverse transformation, the same one `simulate_h0` uses.

### The variance, and the defect it is written to avoid

**BUG-10 of the transfer-function program** is exactly here: there the level was
integrated correctly in the **mean** and incorrectly in the **variance**,
because each reached the level by its own route — the mean asked the
authoritative source for the operator, the variance rebuilt one of its own from
`(d, D, s)`. The defect is invisible in the point forecast and comes out whole
in the bands: a factor **19 in variance** in the case measured, bands at under a
quarter of their width. And the bias always runs the dangerous way, because
omitting non-stationary factors can only *narrow* a band.

The trap here is the same in a different dress: `Y₁ = W − B₂′Y₂` **inherits the
cumulated `Y₂` error**, so computing its band from the `W` band alone leaves it
systematically narrow, and the point forecast would look perfect throughout.

So the map from innovations to level error is written **once**, in
`level_error_map()`, and the band comes from it. With `C_m = Σ_{k≤m} Ψ_k`:

```
G_m  =  [            rows 1..s of C_m                 ]
        [  rows s+1..M of Ψ_m  −  B₂′ (rows 1..s of C_m)  ]

Var(h)  =  Σ_{m=0}^{h−1}  G_m Σ* G_m′,        Σ* = σ² · Q*  =  cov(A*ₜ)
```

`C_m` appears because the `Y₂` error at horizon `h` is the **sum** of the
`∇Y₂` errors of steps `1…h`: the innovation at `n+t` reaches horizon `h` through
`Σ_{k=0}^{h−t} Ψ_k`.

### The certificate on the level step

At `h = 1`, `C_0 = Ψ_0 = I`, so `G_0 = [I_s  0 ; −B₂′  I_r]` and the band is the
innovation covariance **read in levels**. That quantity is available by a
completely different route — from the parameter vector, which is the `Σ` the
`.out` prints — so the two can be compared:

| configuration | `Σ` from the parameters, `(Y₁, Y₂)` | band² at `h = 1`, `(Y₂, Y₁)` |
|---|---|---|
| `-case 2` | (0.049847, 0.071903) | (0.071903, 0.049847) |
| `-case 3` | (0.049863, 0.071895) | (0.071895, 0.049863) |
| `-case 2 -mafree` | (0.048050, 0.063892) | (0.063892, 0.048050) |
| `2 0 1 -case 2` | (0.064673, 0.074088) | (0.074087, 0.064674) |
| `3 1 1 -case 2` | (0.043990, 0.066765) | (0.066765, 0.043990) |

Agreement to the printed precision (≈10⁻⁷) in all five. **The columns are
swapped on purpose**: the VEC internal order is `[Y₁ ; Y₂]` and the `.inp`'s is
`[Y₂ ; Y₁]`, and the forecast reports in the `.inp`'s. If the level map were
wrong this identity would not hold, and a first reading of it looked like a
defect until the two orders were pinned down.

The suite also checks that no band narrows as the horizon grows, which is the
shape BUG-10 takes when it happens.

---

## 4b. The oracle, and what the literature does not say

The two certificates above are **internal**: the recursion against the engine's
residuals, and the one-step band against the parameter vector. Neither reaches
past `h = 1`, which is where the accumulation `C_m` and the cumulated `Y₂` error
live. And the step that needs checking most is the one **nothing in the sources
documents**.

Searched, of the fifteen works in `literature/`: those that mention forecasting
mention it as a motivation. Ahn and Reinsel (1990) note that imposing unit roots
improves long-horizon forecasts; Mauricio (2006) lists forecasting as one of
three purposes; Yap and Reinsel (1995) speak of "efficient prediction". **None
writes the recursion for this class**, and the reference the last two point to
for it — *JTSA* 13, 353–375 — is not in the bank. So what licenses the
procedure is proved rather than cited: **Proposition 2** of
`DEMOSTRACIONES.md` §6b — the transformation loses no information, so
forecasting on `Ȳ` and inverting **is** the conditional expectation of `Y`, and
the level error is affine in the innovations with the coefficients `G_m`.

`tools/sim/forecast_oracle.py` supplies what the sibling programs each have and
this one lacked: an independent computation. It simulates the process, hands
`drvec` the sample, and continues the **same process** 20 000 times from its
true final state. At `n = 20 000`, with the model containing the truth, the
point forecast matches the true conditional expectation to about **2 % of a
forecast standard deviation at every horizon** and the bands to **0.5 %**. The
table, including what it revealed about the cost of the default specification,
is [HOMOLOGATION.md](HOMOLOGATION.md) §4s.

## 5. What this does **not** do yet

* **No rolling-origin evaluation.** `-f H` forecasts from the end of the sample
  with the parameters estimated on all of it. That is not an out-of-sample
  measurement and must not be read as one.
* **No comparison against a univariate model**, which is the only criterion that
  says whether a multivariate specification earns its parameters.
* `r = 0` is refused rather than approximated: with no `W` block there is
  nothing to invert back to levels.

Those are the rest of P5 in [PLAN_PRODUCCION.md](PLAN_PRODUCCION.md), and until
they exist the register still has no measurement of whether this model forecasts
better than an ARIMA on each series.

The bands are **theoretical**: they come from the model's own innovation
covariance and say nothing about whether the specification is right. That is the
distinction the transfer-function program's help puts plainly, and the reason
rolling-origin evaluation is a separate thing and not a refinement of this one.
