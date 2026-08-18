# The model, the parameter vector, and the conventions that bite

*Reference: Mauricio, J.A. (2006), «Exact maximum likelihood estimation of
partially nonstationary vector ARMA models», Computational Statistics & Data
Analysis 50, 3644–3662, and its unpublished AddOn (JAM106).*

---

## 1. What is estimated

An `M`-dimensional VARMA whose autoregressive polynomial has `M − r` unit roots,
written in vector error-correction form:

```
  F(L) ∇Yₜ = −Λ (B′Y_{t−1} − E[W])  +  Θ(L) Aₜ
```

with

```
  F(L) = I − F₁L − … − F_{p−1}L^{p−1}
  Θ(L) = I − Θ₁L − … − Θ_qL^q
  Π    = ΛB′,   rank r,   B = [I_r ; B₂],   B₂ of size s × r,  s = M − r
  Aₜ   ~ iid N(0, Σ)
```

`Λ` is Johansen's `α` (the adjustment matrix) and `B` is his `β` **with the
opposite sign convention** — see §5.

## 2. The transformation, and why the likelihood is exact

The estimation is not carried out on `Yₜ`. Mauricio's result is that the
transformation

```
  Ȳₜ = ( ∇Y_{2t}′ , Wₜ′ )′,     Wₜ = Y_{1t} + B₂′Y_{2t}
```

turns the partially nonstationary model into a **stationary VARMA on `Ȳₜ`**,
whose exact unconditional likelihood the engine already computes. With

```
  C̄  = [ 0_{s×r}  I_s ; I_r  B₂′ ]        C̄⁻¹ = [ −B₂′  I_r ; I_s  0 ]
  H̄  = [ 0  0 ; 0  I_r ]                  Λ̄   = [ 0_{M×s} , Λ ]
```

the VARMA parameters on `Ȳ` are (equations 15–16 of the paper):

```
  Φ̄₀ = C̄⁻¹
  Φ̄₁ = C̄⁻¹H̄ − Λ̄ + F₁C̄⁻¹
  Φ̄ᵢ = FᵢC̄⁻¹ − F_{i−1}C̄⁻¹H̄        i = 2 … p−1
  Φ̄_p = −F_{p−1}C̄⁻¹H̄
```

and after normalising by `C̄ = Φ̄₀⁻¹`,

```
  Φ*ᵢ = C̄Φ̄ᵢ         Θ*ᵢ = C̄ΘᵢC̄⁻¹        Σ* = C̄ΣC̄′
```

which is what the engine receives. **This was verified term by term against the
closed form the AddOn gives for M = 2, r = 1: difference 0.000e+00.**

Two consequences that are easy to get wrong and have both cost time here:

* **`Φ̄_p` is not free.** It is determined by `F_{p−1}`. So a procedure that
  tries to impose all `p` autoregressive matrices on `Ȳ` is over-determined —
  which is exactly why univariate seeds cannot be transported above the diagonal
  rung ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4).
* **`Θ*` is not `Θ`.** What a univariate model of one component of `Ȳ` sees is
  `Θ̄ = C̄ΘC̄⁻¹`. Seeding a `θ` straight into `Θ` is correct only when `C̄ = I`,
  i.e. `r = 0`, and is a silent error otherwise.

## 3. The parameter vector

`x[]` is walked in this order by `calc_nparametrs`, `init_guess`, `vec_shootx`
and the output printer — **four places that must agree**, which is why the test
suite's first block checks that the walk consumes exactly `npar` in 24
configurations:

| # | block | count |
|---|---|---|
| 1 | `E[Ȳ]` | case 1: none · case 2: `r` · case 3: `M` |
| 2 | `Λ` (M×r), or `ψ` (sa×r) under `α = Aψ` | `M·r` or `sa·r` |
| 3 | `Fᵢ`, i = 1…p−1 | `(p−1)M²`, or `(p−1)M` with `-diagar` |
| 4 | `Θⱼ`, j = 1…q | `qM²`, or `qM` with `-diagma` |
| 5 | `Σ`, lower triangle **minus one** | `M(M+1)/2 − 1`, or `M − 1` with `-diagcov` |
| 6 | `B₂` (s×r), unless `-fixb2` | `s·r` |

### Why block 5 is one short

The engine calls `elf` with `sigma2 = 1` and minimises
`(f1/f1₀)^m · (f2/f2₀)`. That objective is **exactly invariant to rescaling the
covariance block** (`f1 → f1/c`, `f2 → c^m f2`), so carrying the full triangle
leaves one direction the likelihood cannot see: a flat ridge along which the
line search fails and the Hessian is singular. `Σ[1][1]` is therefore fixed at 1
and the scale is reported through `sigma2`, so that `Σ̂ = sigma2 · Q`.

This is not a local trick: `drtran` reached the same normalisation independently
(`BRIDGE_DESIGN.md` §10), and the same concentration is why the covariance block
should be **seeded at the variance ratios of the data**, not at the correlation
matrix. Measured on Danish money demand (M = 5, r = 2), which mixes logarithms
with interest rates, that seeding is worth **+79.26** in log-likelihood.

## 4. The deterministic cases

Remark 6 of the paper, and they are not interchangeable:

| | `E[∇Y₂]` | `E[W]` | meaning |
|---|---|---|---|
| case 1 | 0 | 0 | no drift, equilibrium centred at zero |
| case 2 | 0 | ≠ 0 | no drift, equilibrium with a level |
| case 3 | ≠ 0 | ≠ 0 | restricted drift |

Case 1 is the one to be careful with: it forces `E[W] = 0`, and if the
cointegrating combination of the data has a level, the starting point of the
optimisation is dominated by that misfit. On mink–muskrat the case-1 fit starts
at a log-likelihood of **−283** and climbs to **3.69**, travelling a distance no
other configuration travels. Its results are correspondingly path-dependent.

## 5. The conventions that bite

Every one of these has caused a real error in this codebase or in a sibling
program. They are collected here because a convention that lives only in
someone's head is a defect waiting for a maintainer.

### 5.1 The sign of the MA operator

**`Θ(B) = I − Θ₁B − … − Θ_qB^q`**, the same shape as the AR. Verified at source
level across the whole suite, because this is where these programs have burned
themselves before:

| link | convention | where |
|---|---|---|
| `elf` | `aₜ = (w−μ) − Σφⱼ(w−μ)_{t−j} + Σθⱼa_{t−j}` ⟹ `Θ(B) = I − Σθⱼ B^j` | `elfvarma.c:300` |
| `fue`'s cast | `_unscramble` returns the coefficients of `1 − c₁B − c₂B²…`, passed to `elf` unchanged | `forecast.py:143-147`, `cast_us.py:305` |
| `drtran`'s `expand_ma_factors` | the same, obtained by negating its work array | `drtran.c:770-772` |
| `drvec`'s cast | `armax->theta[k] = C̄·Θ_k·C̄⁻¹` with `Θ_k` from `x[]` | `drvec.c`, block [6] |

All four agree. The empirical check, which is the one that would have caught a
mismatch: seeding with **negated** `θ` makes the starting log-likelihood *worse*
(−9.0987, against −8.5267 with the correct seed and −8.7426 from a zero seed).

### 5.2 `p` is the AR order on `Ȳ`, not on `∇Y`

With `r = 0` we have `Φ̄_p = 0`, so the effective AR order on `∇Y` is **`p − 1`**.
Comparing a `drvec` fit with `p = 2` against a univariate ARMA(2,1) instead of an
ARMA(1,1) produces a discrepancy of 5.5 units that looks exactly like a bug and
is not one.

### 5.3 `B₂ = −β`

Johansen's `β` and this program's `B₂` differ in sign. The cointegrating relation
here is `Wₜ = Y_{1t} + B₂′Y_{2t}`, so a `B₂` of `−0.5` is a `β` of `+0.5`.

### 5.4 The column order of the `.inp` chooses the normalisation

Columns `1…s` are the `Y₂` block and `s+1…M` the `Y₁` block, and `B = [I_r ; B₂]`
normalises on `Y₁`. **Which series go in `Y₁` is therefore a modelling decision
made by the column order of the input file**, and `drvec` does not currently
diagnose whether that choice is appropriate. Mauricio warns about this (p. 3648);
Mélard, Roy and Saidi (2004) avoid it by construction, using the null space of
`Φ(1)` instead of a normalisation. `drvec` reports a **normalisation check** for exactly this: per relation, the
share of the weight carried by the `Y₁` block, measured unit-free as
`|coefficient| · sd(series)`. Below 5 % it warns. See [USAGE.md](USAGE.md) §4.

### 5.5 A log-likelihood is not scale-invariant

When a series is rescaled — which the suite does routinely, through `fue`'s
`refactor` — the log-likelihood moves by the Jacobian:
`logL_z = logL_w + n·log(refactor)`. Forgetting it makes the factorisation
identity fail by 280.92 on mink–muskrat (which is exactly `2·61·log 10`) and look
like a broken transformation.
