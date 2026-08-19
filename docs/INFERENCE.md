# Inference on the cointegrating coefficients

*Why the standard errors and χ² tests this program reports for `Λ` and `B₂` are the
ones the theory licenses — and what, precisely, it is that `drvec` makes possible
that a VAR-based route does not. A study of `literature/`, 2026-08-19.*

Everything asserted here is sourced to a paper in `literature/` and, where it is a
claim about the code, checked against a measurement. The chain is short and it
holds together, but it has one condition that does all the work, so it is worth
following in order rather than summarising.

---

## 1. Phillips (1991): the result, and the condition that carries it

`Phillips-OptimalInferenceCointegrated-1991.pdf`, *Econometrica* 59(2), 283–306.

> *«It is shown that full system maximum likelihood brings the problem of
> inference within the family that is covered by the locally asymptotically mixed
> normal asymptotic theory **provided that all unit roots in the system have been
> eliminated by specification and data transformation**. … It means that
> cointegrating coefficient estimates are symmetrically distributed and median
> unbiased asymptotically, that an optimal asymptotic theory of inference applies,
> and that hypothesis tests may be conducted using standard asymptotic
> chi-squared tests.»*

And the condition is not decoration. From the conclusions:

> *«**This condition is crucial.** If maximum likelihood does involve the
> estimation of unit roots, then the likelihood no longer belongs to the LAMN
> family. Instead it involves unit root asymptotics … These asymptotics import a
> **bias and asymmetry** into the cointegrating coefficient estimates and they
> carry **nuisance parameter dependencies** into the limit theory which inhibit
> inference.»*

So the question for any program is not "does it report a standard error" — it is
**whether its likelihood still contains unit roots**. Phillips' vehicle for
removing them is the **triangular system ECM**, whose advantage he states plainly:

> *«Our system is **linear in the parameters that define the cointegration
> space**, whereas in the Engle-Granger representation the same parameters appear
> **nonlinearly**. … eigenvalue routines are not required, and the limit
> distribution theory is easy to derive. **More general parametric and
> nonparametric models for the errors are also easily accommodated** in our
> approach.»*

That last clause is the one that matters here.

## 2. Phillips' Remark (j): what optimal inference costs when the errors are ARMA

The prototypical model has iid errors. Phillips extends it, and in doing so names
exactly the thing `drvec` is:

> *«If `vₜ` is driven by a parametric scheme such as a **vector ARMA model**, then
> full system estimation by MLE involves the **simultaneous estimation of the
> parameters of the stationary ARMA system and the coefficient matrix `B` of the
> long-run equilibrium relationship**. Obviously this involves the construction of
> the likelihood function for general ARMA systems. An alternative approach … is
> to deal with the time series properties of `vₜ` non[parametrically by spectral]
> regression procedures … The latter approach turns out to be **most
> convenient**.»*

Read that twice. Phillips identifies the requirement — **joint ML of the ARMA
parameters and `B`, which needs the exact likelihood of a general ARMA system** —
judges it inconvenient, and goes the spectral route instead (Phillips 1988c,
leading to the fully-modified estimators).

**The requirement he set aside is what `drvec` computes.**

## 3. Mauricio (2006): the same triangular system, with the ARMA likelihood built

`Mauricio.pdf`. The transformation produces, as equation (19),

```
  ∇Y_2t          =  U_1t
  Y_1t + B₂Y_2t   =  U_2t
```

and the paper closes the loop explicitly (Remark 5):

> *«Noting that **(19) is a triangular representation of the type introduced by
> Phillips (1991)**, it follows that **asymptotic optimal inference applies to
> full-system EML estimation** of the stationary VARMA representation given in
> (15)–(17). … (ii) that general hypothesis tests on parameters of the VEC model
> … can be conducted using **standard (e.g., Wald or likelihood ratio) asymptotic
> χ² tests**.»*

The paper also states the counterfactual, which is the reason the transformation
exists at all:

> *«[approaches] based directly on the representation given in (1) without
> considering the restrictions … imply that inference on parameters in the VEC
> model, especially on the elements of `Λ` and `B` … is **quite complicated, if not
> impossible at all, in practice**, and that such inference, when possible, **might
> not be optimal in the sense given by Phillips (1991)**.»*

So the three conditions Phillips requires are met by construction, not by
assumption:

| Phillips requires | Mauricio's transformation supplies |
|---|---|
| all unit roots eliminated by specification **and data transformation** | the rank restriction `Π = ΛB′`, and `Ȳₜ = (∇Y_{2t}′, W_t′)′` |
| **full-system** ML, not single-equation or conditional | one likelihood over `Λ, F, Θ, Σ, B₂` jointly |
| with ARMA errors, **joint** estimation of the ARMA parameters and `B` | exactly the parameter vector `drvec` optimises |
| the exact likelihood of a general ARMA system | AS 311 (Mauricio 1997), the engine, unmodified |

## 4. Where `drvec` makes it possible, and why — concretely

The abstract chain lands on three properties of this code:

**1. The estimated object is the triangular system.** `vec_shootx` builds
`Ȳₜ = (∇Y_{2t}′, W_t′)′` with `W_t = Y_{1t} + B₂′Y_{2t}`, which is equation (19)
term for term. It is not an approximation of it and not a related
parameterisation. (The legacy program's `shootx` had the same shape,
`(w_t, ∇z_{2t})` — see `LEGACY_NOTES.md` §1.)

**2. `B₂` is a coordinate of the parameter vector, estimated jointly with `Θ`.**
This is what makes the Hessian-based standard error the licensed one rather than a
number that happens to be computable. In a VECM parameterisation the cointegrating
coefficients enter the reduced-rank problem nonlinearly and the MA cannot be there
at all; here `B₂` and `Θⱼ` are neighbouring blocks of the same vector
([MODEL.md](MODEL.md) §3), maximised together.

**3. The likelihood is exact and unconditional.** Phillips' condition is about
*which* likelihood; conditional likelihoods are asymptotically equivalent but, as
Mauricio notes, *«it is not difficult to find practical situations (always
involving finite samples, which are often far from large) where the more efficient
use of available information associated with EML delivers more reliable
inferences»*.

Consequences a user can act on:

* the standard errors printed for `B₂` and `Λ` support **t-ratios and Wald tests**
  read the ordinary way;
* `-fixb2 <v>` against a value fixed **a priori** gives a valid LR test, and it
  must agree with the Wald from the reported standard error;
* `-alpha` / `-weakex` restrictions are tested by LR with `(M − sa)·r` degrees of
  freedom, on the same footing.

## 5. Where Johansen stands — the precise version

It is tempting to say inference on the cointegrating vector is not available in
Johansen's framework. **That is not what the sources say, and the correct version
is sharper.**

Johansen (1991), `Johansen-EstimationHypothesisTesting-1991.pdf`, own abstract:

> *«We show that the asymptotic distribution of the maximum likelihood estimator
> is **mixed Gaussian**. Once a certain eigenvalue problem is solved … one can
> conduct inference on the cointegrating rank using some nonstandard
> distributions, and **test hypotheses about cointegrating relations using the χ²
> distribution**.»*

And Phillips himself confirms it, placing Johansen's procedure on the *right* side
of his own dividing line — it *does* impose unit roots by construction:

> *«A closely related result has been given by Johansen (1988) … Johansen proves
> that the likelihood ratio test of a linear hypothesis about the cointegrating
> vector is **asymptotically distributed as chi-squared**. For the reasons given
> here his theory applies also to more general hypotheses about the cointegrating
> coefficients.»*

**So the difference is not the inference theory. It is the model class**, and there
the result is a non-existence theorem. Cappuccio (1996),
`Oxf Bull Econ Stat … Triangular Representation and Error Correction Mechanism.pdf`:

> *«**there exist no reparameterization** of the ARMA model … that allows us to
> write a VECM model … **with independent errors**. Therefore, if we assume that
> the true DGP … is of the form (6) with `d(L)=1` and proceed to estimate the
> cointegrating vectors according to Johansen's ML procedure, **some model
> misspecification is induced**. Even though the MA dynamics … could be
> approximated by including more autoregressive terms, it is likely that
> **estimation and inference on the cointegrating vectors** and on the short-run
> parameters **will be somehow affected** by this model misspecification.»*

Cappuccio also notes that both representations yield LAMN limits and permit
optimal inference *when correctly specified*, and that weak exogeneity is what
licenses the conditional route — which is the same condition Johansen and Swensen
(2024) build on: weak exogeneity *«implies that the distribution factorizes in a
conditional and marginal part such that inference on the parameters of interest
can be performed in the conditional part without any loss of information»*.

So, stated exactly:

| | Johansen (VECM) | `drvec` (triangular VARMA) |
|---|---|---|
| unit roots imposed by construction | yes | yes |
| LAMN limits, χ² tests on `β` | **yes**, for the model it fits | yes |
| can represent MA errors | **no** — no VECM with independent errors exists | yes |
| with MA present in the DGP | misspecified; inference affected | the licensed case |
| escape via more AR lags | possible in principle | — |

The escape is the interesting part, because it is measurable rather than
arguable, and it is measured in [COMPARISON_JOHANSEN.md](COMPARISON_JOHANSEN.md):
on samples of 90–113 observations the lags needed to whiten the residuals cost the
rank test before the approximation converges. Mélard, Roy and Saidi (`TR0444.pdf`)
report the same phenomenon from the other end — *«although the addition of the MA
terms does not alter the asymptotic distribution of the likelihood ratio test
statistic, **finite sample performance of the test is affected by the MA
terms**»*.

## 6. Confirmed in this implementation

Theory that cannot be checked against the code is decoration. Two checks:

**Wald and LR must agree.** They are asymptotically equivalent under §1, so
disagreement would indict the implementation. On simulated data with a known
`B₂ = −0.6`, testing that value:

| n | B̂₂ | sd | Wald | LR | \|difference\| |
|---|---|---|---|---|---|
| 100 | −0.5526 | 0.0380 | 1.556 | 1.477 | 0.079 |
| 200 | −0.5963 | 0.0211 | 0.030 | 0.030 | **0.0001** |
| 400 | −0.6058 | 0.0115 | 0.253 | 0.253 | 0.0006 |
| 800 | −0.6022 | 0.0032 | 0.492 | 0.491 | 0.0008 |
| 1600 | −0.5977 | 0.0014 | 2.722 | 2.704 | 0.018 |

**The standard error must fall faster than `1/√T`** — superconsistency. It falls by
a factor of 27 over a 16-fold increase in `n` (roughly `T^-1.2`), and it converges
to the value an independent implementation computes: at `n = 800` and `n = 1600`
`drvec` and Johansen agree to four decimals (0.003180 vs 0.003178; 0.001417 vs
0.001414).

**And one correction the study forced on the code.** `est()` computed the
covariance from the factor BFGS accumulates *along the optimiser's path*, which is
not the curvature at the optimum: it depends on the route taken and degrades in
the flattest directions — the ones with the largest standard errors. Mélard et
al. state the right practice for the same class of model — *«their estimated
standard errors are computed by inverting the observed Hessian matrix **at the
final estimate** of the parameters»* — and `drtran` had already diagnosed and
fixed it (`BRIDGE_DESIGN.md` §8c). `drvec` now offers **`-fdhess`**, which
recomputes the Hessian at the optimum using the routine the engine itself left
commented out (`drvmlest.c:104-107`), without modifying the engine.

## 7. What this does *not* establish

* **It is asymptotic theory.** Every result above is a limit result, and this
  program is used on 60–120 observations. The rank test's own finite-sample size
  is measured at about three times its nominal level at `n = 120`
  ([HOMOLOGATION.md](HOMOLOGATION.md) §2.3), and Mélard et al. report the same
  for the MA case. Optimality in the limit is not accuracy in the sample.
* **Optimality is conditional on the specification being right.** `drvec` is the
  licensed case *when the data have an ARMA error structure of the order fitted*.
  A misspecified VARMA has no better claim than a misspecified VECM — which is
  why the residual diagnosis is printed with every fit
  ([USAGE.md](USAGE.md) §5).
* **`r = M` is outside the model class.** If the series have no unit roots, the
  transformation has nothing to remove and the parameterisation cannot express the
  answer; that is `drvarma`'s case, and reading a rank from `drvec` there is a
  mistake the program cannot catch for you.
* **Nothing here is about the *rank* decision**, which follows non-standard
  distributions in both frameworks and is the weaker part of both.

## 8. The genealogy, one line each

The papers in `literature/`, and what each contributes to the chain:

| | contribution |
|---|---|
| **Phillips (1991)**, *Optimal Inference in Cointegrated Systems* | the theorem, the crucial proviso, the triangular ECM, and Remark (j) naming what ARMA errors require |
| **Phillips (1991)**, *Error Correction and Long-Run Equilibrium in Continuous Time* | the same triangular ECM format in continuous time; temporal aggregation and aliasing |
| **Mauricio (2006)** | the transformation; states that (19) is Phillips' triangular form and that optimal inference therefore applies to EML |
| **Mauricio AddOn (JAM106)** | the closed form of the transformation for M=2, r=1 — used here to verify the cast to 0.000e+00 |
| **Johansen (1991)** | ML in the VECM: mixed-Gaussian limits, χ² tests on the cointegrating relations, non-standard rank test |
| **Johansen & Swensen (2024)** | `α = Aψ` restrictions; weak exogeneity as the factorisation that licenses conditional inference |
| **Cappuccio (1996)** | the non-existence result: no VECM with independent errors for an ARMA DGP, so Johansen's route is misspecified there |
| **Ahn & Reinsel (1990)** | reduced-rank estimation imposing the unit-root structure — the AR-only precedent, no MA |
| **Mélard, Roy & Saidi (2004)** (`TR0444`) | the state-space route to the same exact likelihood; Hessian-at-the-optimum standard errors; finite-sample cost of MA terms |
| **Hillmer & Tiao (1979)** | the exact likelihood of a stationary VARMA — the machinery the transformation delivers its system to |
| **Chan & Wallis (1978)** | the mink–muskrat analysis and its `|Σ̂|`, used here to calibrate magnitude |
| **Trenkler (2004)** | response surfaces for finite-sample critical values — the route out of §7's first bullet |

The line of descent is: **Phillips supplies the theory and names the requirement;
Hillmer–Tiao and Mauricio (1997) supply the exact VARMA likelihood; Mauricio
(2006) joins them; `drvec` is that join, executable.**
