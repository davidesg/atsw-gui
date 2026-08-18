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

### Rank

| | |
|---|---|
| `-lrtest` | estimate `r = 0 … M−1` and report the sequential test |

Reports, per rank, the parameter count, the log-likelihood, AIC and BIC, and for
each consecutive pair the statistic `2·[L(r+1) − L(r)]` against the non-standard
asymptotic critical values (λ-max form; cases 1 and 2 only — case 3 is not
tabulated here). A **negative** statistic is flagged: rank `r` is nested in
`r+1`, so a negative value proves one of the two fits did not converge.

Incompatible with `-differenced`, because the column split depends on `r`.

**Combined with `-alpha` / `-weakex`**, the sequence is still computed — testing
the rank *within* the restricted model is a legitimate question — but the
**critical values are suppressed**, because under `α = Aψ` the statistic has a
different distribution and the tables are for `α` free. Printing them would be
wrong numbers wearing the right clothes.

*A caveat worth knowing:* the eigenvalues of `Π̂` are sometimes quoted as a rank
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

## 5. Exit behaviour

`drvec` writes `file.out` whatever happens. `ERROR` messages on stderr mean the
run did not produce an estimate; `WARNING` messages mean it did, but something
about the specification deserves attention. The suite treats
`ERROR output` and `ERROR init_guess` as failures, since both mean the parameter
vector was walked inconsistently.
