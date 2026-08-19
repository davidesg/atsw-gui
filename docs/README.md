# drvec

**Exact maximum likelihood estimation of partially nonstationary VARMA models
in vector error-correction form** — Mauricio (2006), *CSDA* 50, 3644–3662.

The model is a VARMA whose autoregressive polynomial has unit roots of a known
multiplicity, written so that the cointegrating structure is explicit:

```
  (I − F₁L − … − F_{p−1}L^{p−1}) ∇Yₜ  =  −Λ(B′Y_{t−1} − E[W])  +  (I − Θ₁L − … − Θ_qL^q) Aₜ
```

with `Π = ΛB′` of rank `r` and `B = [I_r ; B₂]` fixing the normalisation.

Three things follow from estimating **in these coordinates** rather than
estimating a VARMA and transforming afterwards:

1. **The likelihood is exact and unconditional**, not a conditional sum of
   squares: the engine is Mauricio's AS 311, used unmodified.
2. **`Λ` — the adjustment matrix, `α` in Johansen's notation — is a parameter,
   not a derived quantity.** So a linear restriction on it, `α = Aψ`, is a
   substitution inside the translation step, and its standard errors come
   straight from the Hessian. In VECM coordinates `α` is derived, and the same
   restriction needs constrained optimisation through the inverse map.
3. **`r = 0` is expressible**, which makes the sequential likelihood-ratio test
   for the cointegration rank a sequence of fits of one program.

What `drvec` is **not**: it is not a Johansen reduced-rank regression. Johansen's
procedure is a closed-form eigenvalue problem on a VAR; this is a numerical
optimisation of the exact likelihood of a VAR**MA**. On a pure VAR they answer
the same question; the MA terms are what this program adds, and what makes the
optimisation hard.

---

## Status

**Pre-beta.** The estimator works and is tested; the interpretable layer is
partially built — restrictions on the adjustment coefficients, the long-run
matrix `Π`, the triangularisation `Σ = PDP′` and the normalisation diagnostic are
in; the rank test is asymptotic only. What is verified, what is not, and what is known to be wrong is
in [DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) — including the results that
were *rejected* after measurement, which are the ones that say most about where
the program stands.

The one number a user should carry: **the optimiser stops on «last global step
failed to locate a lower point» in most configurations**, and moving a parameter
by hundredths can move the answer by units. See
[CONVERGENCE.md](CONVERGENCE.md) before trusting a fit.

---

## Ten lines that run

```sh
make
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2
```

`p = 2`, `q = 1`, `r = 1` on Hudson's Bay mink and muskrat fur sales,
1850–1911. The rank can be tested rather than assumed:

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 0 -case 2 -lrtest
```

and a hypothesis about who adjusts to the equilibrium can be tested too:

```sh
bin/drvec datasets/mauricio/mink_muskrat 2 1 1 -case 2 -weakex 2
```

which reports `LR = 3.589519, 1 df, p = 0.058145` — mink is weakly exogenous at
the 5 % level, the muskrat equation carries the adjustment. For the predator and
its prey, that is the expected direction.

---

## Why the numbers can be trusted, and where they are not

Checks of four different kinds, each independent of the others:

| | what it establishes |
|---|---|
| **the engine against a hand-computed normal** | `elf` (AS 311) agrees to **2·10⁻⁸** on a multivariate normal computed by hand |
| **the transformation against the paper's own closed form** | Mauricio's AddOn gives Φ̄ᵢ in closed form for M = 2, r = 1; term by term the difference is **0.000e+00** |
| **the factorisation identity** | with `r = 0` and diagonal structure the exact likelihood factorises, and the joint fit equals the sum of the univariate fits to **1e−9**. Three independent programs now agree on the two univariate constants |
| **published rank results** | the sequential LR test recovers `r = 1` on mink–muskrat and `r = 2` on UK consumption, the latter matching `urca::ca.jo` |

And the limits, stated because a document that lists only what works is an
advertisement:

* the published EML log-likelihoods of Mauricio (2006) Table 5 **are not
  reproduced**, and the discrepancy is localised but not closed
  ([DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) §2);
* the acceptance criterion on |Σ̂| **is met in agreement but not in level**;
* seeding from the suite's `.pre` files **transports an optimum exactly one rung
  of the ladder** and no further, which is measured, not assumed
  ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4).

---

## The documentation

| | |
|---|---|
| [GETTING_STARTED.md](GETTING_STARTED.md) | build, first model, reading the `.out` |
| [MODEL.md](MODEL.md) | the model class, the parameter vector, and the conventions that bite |
| [USAGE.md](USAGE.md) | every option, the input format, and worked examples |
| [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) | the ladder, its two contracts, and the provenance of the borrowed code |
| [CONVERGENCE.md](CONVERGENCE.md) | what the optimiser reports, and how much to believe it |
| [HOMOLOGATION.md](HOMOLOGATION.md) | which cases it reproduces, to what tolerance, and which it does not |
| [COMPARISON_JOHANSEN.md](COMPARISON_JOHANSEN.md) | against the reference implementation: same specification, then each at its optimum |
| [TESTING.md](TESTING.md) | the test suite: what it protects, measured by mutation |
| [DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) | the process: what was tried, what was measured, what was rejected |

The Spanish documents in this directory — `ANALISIS_PRELIMINAR.md`,
`PLAN_BETA.md`, `ESTUDIO_BVECM_vs_DRVEC.md`, `LEGACY_NOTES.md`,
`ENCUADRE_ESTUDIO.md` — are the **working record**: the diagnosis as it was made,
the plan with its exit criteria, and the study of the legacy program. They are
the primary source for everything asserted here.

---

## Where this sits

`drvec` is one program of a family that shares an engine and a set of file
conventions:

* [`fue`](../../atws/fue) — univariate ARMAX by exact ML, with transfer
  functions; identification is `art`;
* `drtran` — the bridge from `fue` to `drvarma`: transfer functions estimated
  jointly;
* `drvarma` — stationary multivariate VARMA;
* **`drvec`** — the partially nonstationary case, in VEC coordinates.

They share `elf`, the parameter-vector-to-model translation contract, and the
`.inp`/`.pre` files. What that costs and what it buys is
[SUITE_INTEGRATION.md](SUITE_INTEGRATION.md).

## Licence and lineage

GPL v2 or later. The engine is José Alberto Mauricio's C (AS 311, and his
quasi-Newton optimiser from *JASA* 90, 282–291); the file format and the user
manual are Arthur B. Treadway's. `drvec` is the VEC driver on top of them.
