# Usage

```
drvec file p q r [options]
```

`file` without the `.inp` extension; results are written to `file.out`.
`p` and `q` are the AR and MA orders of the stationary VARMA **on `Ȳₜ`** — note
that with `r = 0` the effective AR order on `∇Y` is `p − 1`
([MODEL.md](MODEL.md) §5.2). `r` is the cointegration rank, `0 ≤ r < M`.

---

## 1. The input file

```
* comments, freely
1                               <- frequency: 1 annual, 4 quarterly, 12 monthly
2 62 1 1850                     <- M, nobs, start subperiod, start year
lmuskrat lmink                  <- M series names, in column order
1.0 0 0                         <- lambda, d, D  (normally 1.0 0 0)
<nobs rows of M columns>
```

**Column order is the modelling decision that this format hides.** Columns
`1…s` are the `Y₂` block and `s+1…M` the `Y₁` block, where `Y₁` is the
`r`-dimensional block the cointegrating matrix is normalised on:
`B = [I_r ; B₂]`. Putting a different series in `Y₁` is a different
normalisation. Prefer as `Y₁` a series that genuinely appears in every
cointegrating relation — and check the normalisation report the fit prints (§4),
which measures exactly that.

Supply **levels** for every column; that is the default. `-differenced` selects
the legacy layout where columns `1…s` already hold `∇Y₂`, and then the levels of
`Y₂` have to be reconstructed by cumulating from an arbitrary origin, which
shifts `Wₜ`. Harmless in cases 2 and 3, where the free `E[W]` absorbs it; wrong
in case 1, where nothing does. `drvec` warns.

---

## 2. Options

### Model specification

| | |
|---|---|
| `-case 1\|2\|3` | the deterministic specification (Mauricio's Remark 6). Case 1: `E[∇Y₂] = E[W] = 0`. Case 2: `E[W] ≠ 0`. Case 3: both free. Default is 1 |
| `-mean` | include a mean; implies case 2 unless a case was given |
| `-diagar` `-diagma` `-diagcov` | restrict `Fᵢ`, `Θⱼ` or `Σ` to be diagonal |
| `-m 1\|2` | exact (1, default) or approximate (2) maximum likelihood |
| `-differenced` | legacy layout: columns `1…s` already differenced |

### Restrictions

| | |
|---|---|
| `-fixb2 [v]` | hold `B₂` fixed instead of estimating it. Without a value it is held at the static-OLS estimate; with one, every entry is set to `v`. A value chosen a priori is what makes an LR test against the free model valid |
| `-alpha file` | impose `α = Aψ` with `A` read from `file`, and report the LR against the free model |
| `-weakex i` | shorthand for the `A` that declares equation `i` weakly exogenous |

The `A` file is deliberately plain — it is a hypothesis, not a ladder file:

```
* A declaring equation 2 not to adjust
2 1        <- M and sa (the number of columns of A)
1
0
```

Lines starting with `*` or `#` are comments. `A` must have full column rank; if
`A′A` is singular, `ψ` is not identified and `drvec` refuses before estimating
rather than returning numbers for a model that does not have them.

The degrees of freedom of the reported LR are `(M − sa)·r`, from Johansen and
Swensen (2024).

### Seeding the VEC block

| | |
|---|---|
| `-seedb2 v` | start `B₂` at `v` and estimate it **free** (`-fixb2` pins it there instead) |
| `-seedgate` | estimate the `r = 0` rung, hold `F`, `Θ`, `Σ` there, fit `Λ` and `B₂` on it, then release everything |

`-seedb2` is a measuring instrument: it is how you ask whether an answer depends
on where `B₂` starts, without recompiling for each value. Measured with it
([HOMOLOGATION.md](HOMOLOGATION.md) §4d): on the wheat pairs the answer barely
depends on the seed at all, on `mink_muskrat` it depends on it enormously, and
in no case does a bad `B₂` seed drive the moving-average root to the
invertibility boundary — the boundary is where the maximum is, and a seed bad
enough to matter kills the run instead, by making the starting point
non-stationary.

**Not the default, and measured to be worse on most of the bank** — it is here
because the measurement is worth keeping, not because it should be used. See
[HOMOLOGATION.md](HOMOLOGATION.md) §4c for the numbers and
[VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md) for what it was trying to do.

The idea is the ladder's own: estimate the rung below, then start the rung above
from it. It does not carry over unchanged, because at `Λ = 0` the transformed
system has an AR root of modulus exactly one — the null sits *on* the boundary
of the alternative — so the option instead enters along the direction the
conditional regression's `Λ` points, at the largest admissible step it can find,
and lets the likelihood profile `Λ` and `B₂` from there.

What the measurement says: the entry costs far more than the rung below is
worth. On Angers the released fit never leaves the entry point and ends 37.6
log-likelihood units below the cold start; on Milan 8.9 below. It wins on three
of fifteen cases. The `r = 0` dynamics, in short, do not transfer to `r = 1`.

### The ladder

| | |
|---|---|
| `-rungs` | estimate rungs 0-2 and report their LRs, then stop |

The construction goes from optima to optima: the diagonal gate is certified
first, and each wider model starts from the one below it. `-rungs` emits that
sequence from a single run, so it is something the program produces rather than
something the user assembles by hand out of three separate runs — which is where
the degrees of freedom get miscounted, and where nothing is left on record.

| rung | model | added |
|---|---|---|
| **0** | `r = 0`, `F`, `Θ` and `Σ` diagonal | the certified base; the likelihood factorises |
| **1** | `r = 0`, `Σ` free | contemporaneous correlation |
| **2** | `r = 0`, `F` and `Θ` free | cross dynamics |

All three sit at `r = 0`: what is being added is correlation structure, not
cointegration. Each is nested in the next as an **interior** point, so the
log-likelihood cannot fall and the statistic is `χ²` on the printed degrees of
freedom — `M(M−1)/2` for the covariance and `(p−1)M(M−1) + qM(M−1)` for the
dynamics. Rung 0 reports its own factorisation contract, since it is the base
everything above is built on, and a negative LR is flagged: it proves that the
wider fit did not converge.

The next rung — `r = 1`, the VEC matrix itself — is **not** an ordinary
comparison in either respect: the null sits on the boundary of the alternative,
where the transformed system has an AR root of modulus exactly 1, and `B₂` is
unidentified under it. That is why it lives in `-lrtest`, with a parametric
bootstrap for its distribution, and not here. See
[VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md).

```
bin/drvec datasets/mauricio/mink_muskrat 2 1 0 -case 1 -rungs
```

### Rank

| | |
|---|---|
| `-lrtest` | estimate `r = 0 … M−1` and report the sequential test |
| `-bootstrap N` | with `-lrtest`: bootstrap critical values under H₀ (see below) |

Reports, per rank, the parameter count, the log-likelihood, AIC and BIC, and for
each consecutive pair the statistic `2·[L(r+1) − L(r)]` against the non-standard
asymptotic critical values (λ-max form; cases 1 and 2 only — case 3 is not
tabulated here). A **negative** statistic is flagged: rank `r` is nested in
`r+1`, so a negative value proves one of the two fits did not converge.

Incompatible with `-differenced`, because the column split depends on `r`.

### Bootstrap critical values

| | |
|---|---|
| `-bootstrap N` | with `-lrtest`: N parametric-bootstrap replications under H₀ for each rank comparison |

The asymptotic tables are known to be optimistic at these sample sizes — measured
here, the sequential test over-rejects at about **six times** its nominal level
at n = 120 — and the bootstrap cuts that to four, without fixing it ([HOMOLOGATION.md](HOMOLOGATION.md) §2.3), and Mélard, Roy and Saidi
report the same for models with MA terms. `-bootstrap` replaces the tables with
percentiles simulated **from the fitted model at rank r**, so they carry the
sample size, the deterministic case and the MA component, which a table cannot.

It also fills the gap in **case 3**, which has no tabulated values here and
otherwise reports the statistic with `-` in every critical-value column.

Read with two caveats the output states for you: the Monte Carlo error of a
bootstrap p-value (`sqrt(p(1−p)/B)`), and its **floor of 1/(B+1)** — with
`B = 100` the p-value cannot go below 0.0099 however extreme the statistic is,
which is why the verdict is read from the critical values and why `B ≥ 999` is
needed for a p-value that resolves 1 %.

Cost is `N × 2` full estimations per comparison: about 4 s for a bivariate case
at `B = 100`, and 40 s for `M = 3` at `B = 200`. Replications where either fit
fails to converge are discarded and counted in the `reps` column.

**Combined with `-alpha` / `-weakex`**, the sequence is still computed — testing
the rank *within* the restricted model is a legitimate question — but the
**critical values are suppressed**, because under `α = Aψ` the statistic has a
different distribution and the tables are for `α` free. Printing them would be
wrong numbers wearing the right clothes.

**When there is no unit root at all, this program cannot say so.** The sequence
stops at `r = M−1` because `r = M` is a stationary process in levels, which the
parameterisation cannot express — that case belongs to `drvarma`. So a system
whose series are all stationary gets a rejection of `r = 0` (correctly: there are
fewer than M unit roots) and no way to reach the truth, which is that there are
none. Measured on real data: two wheat-price pairs where ADF rejects the unit
root in every series, `drvec` rejected `r = 0` at 1 % and returned a
cointegrating coefficient with the **wrong sign**, while Johansen picked `r = M`
— its way of saying the same thing. **Check the order of integration of the
series before reading a rank from here.**

*A caveat:* the eigenvalues of `Π̂` are sometimes quoted as a rank
criterion. Mélard, Roy and Saidi (2004) show that the assumption on `Φ(1)` does
**not** imply what that criterion assumes. Treat them as an indication; the LR
test is the instrument.

### The bridge to the suite

| | |
|---|---|
| `-writeres pfx` | write one `pfx.<i>.inp` per equation holding the conditional-regression residuals, then stop |
| `-writeinp pfx` | the same, one file per component of `Ȳ`, then stop |
| `-seed pfx` | read `pfx.<i>.pre` (falling back to `.inp`) and seed the MA block |
| `-seedybar pfx` | seed the whole univariate block from `-writeinp` output |
| `-eval` | evaluate the likelihood at the starting point and stop, without optimising |

### Multi-start

| | |
|---|---|
| `-multistart n` | estimate from `n` starting points and keep the best |

The standard errors reported come from **the best start**, not from a re-run at
its optimum: `cov` is built from the factor the optimiser accumulates *while*
iterating, so a run that begins already at the optimum leaves it at its
initialisation and returns the same standard error for every parameter. That was
the behaviour when multi-start was first written, and it is now a check in the
test suite.

Start 1 is the ordinary seed and the rest are deterministic perturbations of it,
so a result is reproducible and the procedure is **monotone in `n`**: the first
`n` starts of a long run are the starts of a short one. On this class of surface
it is the single most effective thing available — see
[CONVERGENCE.md](CONVERGENCE.md) §3 for the measurement — and the **spread** it
reports across starts is a diagnostic by itself.

The intended cycle:

```sh
bin/drvec mydata 2 1 0 -case 1 -diagar -diagma -diagcov -writeinp comp
python -m fue comp.1 eml
python -m fue comp.2 eml
bin/drvec mydata 2 1 0 -case 1 -diagar -diagma -diagcov -seedybar comp -eval
```

The last line reports the joint likelihood at the stored values *and* the sum of
the univariate ones, which is the suite's crossing identity — they must agree.
Dropping `-eval` fits, and the difference between fitting and evaluating is the
optimality certificate: it cannot be negative, and it is zero exactly when the
`.pre` files handed to it are univariate optima.

**Above `r = 0` this does not transport an optimum** and `drvec` warns. See
[SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4 for the measurement and the three
structural reasons.

`-eval` is also the tool for telling apart a bad starting point from a bad path:
if a change makes the *start* worse it is the seed, and if it makes the start
better and the end worse it is the surface.

---

## 3. Worked example: is there cointegration, and who adjusts?

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 0 -case 2 -lrtest
```

recovers `r = 1`, agreeing with the published analysis of these data. Then

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2
```

gives `Wₜ = log(mink) − 0.2405·log(muskrat)` with `E[W] = 7.66`, and adjustment
coefficients of 0.83 and 0.65. Finally

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2 -weakex 2
```

`LR = 3.59` on 1 df, `p = 0.058`: the mink equation can be treated as weakly
exogenous, so the muskrat population is what adjusts toward the equilibrium. For
a predator and its prey that is the expected direction, and it is the kind of
statement the program exists to support.

---

## 4. What the fit reports about the long run

Two blocks are printed after the parameters, and both exist to stop a specific
mistake.

**`Pi = Lambda B'`**, the long-run matrix, with its eigenvalues. `Π` is
**invariant to the normalisation** — reparameterising `Λ → ΛG`, `B → BG⁻ᵀ`
leaves it unchanged — while `Λ` and `B` are not. So `Π` is what to compare two
fits on.

The eigenvalues come with a caution that is stronger than the usual one: **here
`Π = ΛB′` has rank `r` by construction**, so its `M − r` zero eigenvalues are
guaranteed and prove nothing about whether `r` is right. Reading them as evidence
for the rank is circular. (Even in an unrestricted fit they are only an
indication — Mélard, Roy and Saidi (2004) show the assumption on `Φ(1)` does not
imply what that reading assumes.) The instrument for the rank is `-lrtest`.

**`Sigma = P D P'`**, the triangularisation. `P` is unit lower triangular and
`D` diagonal, so with `Aₜ = P A*ₜ` the innovations `A*ₜ` are uncorrelated and the
system premultiplied by `P⁻¹` can be read one equation at a time. The report also
gives `D_i / Σ_ii`, the share of each equation's innovation variance that is its
own once the earlier ones are projected out.

**The ordering is the column order of the `.inp`**, and a different order gives a
different `P`. That is the same class of silent modelling decision as the choice
of the `Y₁` block, so the output says it.

**The normalisation check.** `B = [I_r ; B₂]` assumes the `Y₁` block genuinely
appears in every cointegrating relation. When it does not, `B₂` inflates and the
fit describes a relation among the *other* series — silently, because nothing
about the output looks wrong. The check reports, per relation, the share of the
weight carried by `Y₁`, measured unit-free as `|coefficient| · sd(series)`:

```
Normalisation check (which series carry each cointegrating relation):
  relation 1: the Y1 block carries  75.5% of the weight
```

Below 5 % it is marked `<-- DUBIOUS` and a warning goes to stderr. The fix is to
reorder the columns of the `.inp` so that a series that does appear in the
relations sits in the `Y₁` block.

`datasets/synthetic/badnorm.inp` is a case built to fire it: two series that
cointegrate plus an independent random walk placed in `Y₁`. It reports 0.6 %.

---

## 4a. The entry gate, and what it certifies

At the diagonal rung — `r = 0` with `-diagar -diagma -diagcov` — the exact
likelihood **factorises** into the univariate models, and every run there
verifies two things before anything else is read.

**The crossing identity.** The joint fit must equal the sum of the univariate
fits exactly. That single number tests everything upstream of the likelihood at
once: the transformation, the differencing the rank implies, the parameter
walk, the deterministic terms subtracted and the scaling. If it holds, the
univariate rung crossed intact and a model built on top of it is worth reading;
if it does not, the base is wrong and nothing above it means anything — and the
fault is never in the likelihood routine itself.

**The optimality certificate**, whenever the run was seeded. The gap between the
fit and the values brought in is non-negative by construction and is zero if and
only if those values were the univariate optima, so it says whether what arrived
was a `.pre` — an optimum in re-runnable form — or a specification that still
needed estimating. Both are legitimate inputs; the point is to know which.

This is the same gate the suite's transfer-function program uses, and it is the
place to start: certify the base, then add structure to it.

## 4b. The roots of the estimated operators

Every fit reports the moduli of the roots of the autoregressive and
moving-average operators. The model is stationary and invertible when all of
them exceed one; a modulus at one is marked, and an infinite modulus simply
means the last coefficient matrix of that operator is singular.

```
Roots of the AR and MA operators (moduli; the model is stationary and
invertible when every modulus exceeds one):

  AR (Phi)          inf    1.17582    1.17582    1.74400
  MA (Theta)    1.06441    0.99995*
```

A marked root is worth acting on. The likelihood is defined only inside the
invertible region, so an estimate that reaches its boundary is a constrained
optimum: standard errors are not defined along the binding direction, and
`-fdhess` will decline to compute them and say why. A moving-average root on the
unit circle further suggests the data have been differenced more than they
require, which in this model is governed by the declared rank — `s = M − r`
series are differenced — so the rank is the first thing to re-examine. The
reasoning is set out in [CONVERGENCE.md](CONVERGENCE.md) §2b.

## 5. Residual diagnostics

Every successful fit ends with a diagnosis of its residuals, in two parts.

**The suite's standard block**, `multivariate_diagnostics` copied unchanged from
`drtran`: Hosking's multivariate portmanteau and a multivariate Jarque–Bera.
Copied rather than written here on purpose — the same residual has to read the
same way in `drvarma`, in `drtran` and here, and a private statistic, however
correct, forces comparisons of unlike things.

**And what `drvec` adds**, because a portmanteau cannot answer the question this
model class raises: the **cross-correlation matrices** `R(k)` for `k = 0…K`, with
anything past the ±2/√n band marked.

```
R(k)[i][j] = corr( a_i(t), a_j(t−k) )
```

* the **diagonal** is each equation's own residual ACF — own dynamics not
  captured;
* the **off-diagonal** is a cross effect the model has missed, and **its `k` is
  its order**;
* `R(k)[i][j]` and `R(k)[j][i]` are different statements — that is the
  *direction* of the effect.

**Lag 0 is reported apart and is not counted as a defect.** The contemporaneous
off-diagonal is `Σ`'s, and `Σ` is estimated; counting it would fire the alarm on
every model with correlated innovations, which is the normal case. Under
`-diagcov` it *is* a defect, because then the restriction is imposed, and the
output says so.

This matters when the univariate ARMA is already settled — identified from clean
ACF/PACF work upstream, as the suite's ladder intends. What is left to decide is
whether there are **cross effects and of what order**, and that is a question
about the off-diagonal of `R(k)`, not about the likelihood.

---

## 6. Exit behaviour

`drvec` writes `file.out` whatever happens. `ERROR` messages on stderr mean the
run did not produce an estimate; `WARNING` messages mean it did, but something
about the specification deserves attention. The suite treats
`ERROR output` and `ERROR init_guess` as failures, since both mean the parameter
vector was walked inconsistently.
