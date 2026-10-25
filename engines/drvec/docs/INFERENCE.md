# Inference on the cointegrating coefficients

*Why the standard errors and χ² tests this program reports for `Λ` and `B₂` are the
ones the theory licenses — and what, precisely, it is that `drvec` makes possible
that a VAR-based route does not. A study of the bibliography, 2026-08-19.*

Everything asserted here is sourced to a paper in the bibliography and, where it is a
claim about the code, checked against a measurement. The chain is short and it
holds together, but it has one condition that does all the work, so it is worth
following in order rather than summarising.

---

## 1. Phillips (1991): the result, and the condition that carries it

Phillips (1991a).

> *«It is shown that full system maximum likelihood brings the problem of
> inference within the family that is covered by the locally asymptotically mixed
> normal asymptotic theory **provided that all unit roots in the system have been
> eliminated by specification and data transformation**. … It means that
> cointegrating coefficient estimates are symmetrically distributed and median
> unbiased asymptotically, that an optimal asymptotic theory of inference applies,
> and that hypothesis tests may be conducted using standard asymptotic
> chi-squared tests.»*

The condition is not incidental. From the conclusions:

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

Phillips thereby identifies the requirement — **joint ML of the ARMA
parameters and `B`, which needs the exact likelihood of a general ARMA system** —
judges it inconvenient, and develops a spectral alternative instead (Phillips 1988c,
leading to the fully-modified estimators).

**That requirement is what this program computes.**

## 3. Mauricio (2006): the same triangular system, with the ARMA likelihood built

The transformation produces, as equation (19),

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

Johansen (1991), Johansen (1991), from its abstract:

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
the result is a non-existence theorem. Cappuccio and Lubian (1996),
Cappuccio and Lubian (1996):

> *«**there exist no reparameterization** of the ARMA model … that allows us to
> write a VECM model … **with independent errors**. Therefore, if we assume that
> the true DGP … is of the form (6) with `d(L)=1` and proceed to estimate the
> cointegrating vectors according to Johansen's ML procedure, **some model
> misspecification is induced**. Even though the MA dynamics … could be
> approximated by including more autoregressive terms, it is likely that
> **estimation and inference on the cointegrating vectors** and on the short-run
> parameters **will be somehow affected** by this model misspecification.»*

Cappuccio and Lubian also note that both representations yield LAMN limits and permit
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
rank test before the approximation converges. Mélard, Roy and Saidi (2004)
report the same phenomenon from the other end — *«although the addition of the MA
terms does not alter the asymptotic distribution of the likelihood ratio test
statistic, **finite sample performance of the test is affected by the MA
terms**»*.

## 6. Confirmed in this implementation

A theoretical result that cannot be checked against the implementation is of
little use here. Two checks were carried out:

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
fixed it (established previously for the transfer-function program of the same suite). `drvec` now offers **`-fdhess`**, which
recomputes the Hessian at the optimum by finite differences, using a routine the
estimation driver itself provides for the purpose, and without modifying it.

**And a limit on when it can be formed at all.** The exact likelihood is defined
only where the moving-average operator is invertible, and the estimate may come
to rest against that constraint; on the mink–muskrat data every specification
with `q ≥ 1` does. A finite-difference step then leaves the admissible region,
and no unconstrained curvature exists in the binding direction to be measured.
`-fdhess` detects this — it counts the evaluations that fall outside — and
reports the boundary as the cause, retaining the BFGS standard errors rather
than substituting a number computed from inadmissible points. The distinction it
draws is between two different situations that must not be conflated: an
optimum on the boundary, where the unconstrained Hessian is undefined, and an
indefinite Hessian assembled entirely from admissible evaluations, which instead
says the optimiser did not stop at a maximum. Only the second is a statement
about the optimiser, and the program makes only the one the evidence supports.
The moduli of the roots reported with every fit identify which direction binds
([CONVERGENCE.md](CONVERGENCE.md) §2b).

What this costs is stated plainly: where the estimate lies on the boundary, the
standard errors reported for it are the BFGS ones, with the reservations above,
and inference along the binding direction is not supported by them.

## 7. The bootstrap: where the procedure comes from

*Added 2026-10-24, from the three papers on the bootstrap of the cointegrated
VAR.*

`-bootstrap` was built from the prescription of the BVECM article (Guerrero and
García-Hiernaux 2026, §6.3–6.5): simulate under H₀ with the parameters
estimated at rank r, refit both ranks, read the percentiles. The theory for that
scheme exists, for the VAR, and it was not cited until now.

The article proves no result about that distribution. Its theorems are
representation results — the WARMA–VEC equivalence (Theorem 1 and its
corollaries) — and what they give the bootstrap is the structure of the
simulated sample: a stationary system, inverted, is a VEC with `Π` of rank r.
They do not give its distribution, nor the I(1, r) conditions, and Corollary 2,
the moving-average case, covers only the class with `Θ` on the `w` block
(`-mawarma`); for the free `Θ` of the default the representation is Mauricio's
(2006). Its asymptotic claim, that moving-average terms leave the limit
unchanged, rests on Yap and Reinsel (1995, Theorem 3), a trace test on the
conditional likelihood (BUG-27).

**Swensen (2006)** gave the first asymptotic theory for the rank bootstrap and
for the bootstrap sequential procedure. His scheme mixes two fits: α and β at
rank r, the short-run coefficients and the residuals from the unrestricted one.
**Cavaliere, Rahbek and Taylor (2012)** showed what that costs: when the null
rank is below the true one, the mixed scheme can generate explosive or I(2)
samples, asymptotically and not only in small ones, unless auxiliary conditions
hold on the process. They estimate **every** parameter under the null, prove
that the estimates converge to pseudo-true values that satisfy the I(1, r)
conditions whatever the true rank, and hence that the bootstrap test is
correctly sized at the true rank and consistent below it. In their Monte Carlo
(VAR(2), p = 4, r₀ = 1, restricted constant) the asymptotic test rejects about
45 % at T = 50 and 13 % at T = 200 against a nominal 5 %, and their bootstrap
stays close to 5 % from T = 50.

**`drvec`'s scheme is theirs, not Swensen's**: `bootstrap_rank` recovers the
whole model at rank r (Λ, F, Θ, B₂, Σ) and simulates from it. The sequential
reading, the first comparison not rejected, is their Algorithm 2.

Where the program departs from the papers:

| | Cavaliere, Rahbek and Taylor (2012) | `drvec` |
|---|---|---|
| model | VAR, i.i.d. errors | VARMA on `Ȳ`; Θ is part of H₀ |
| likelihood | Gaussian, reduced-rank regression | exact, unconditional |
| statistic | trace, H(r) against H(p) | λ-max, r against r+1 |
| bootstrap errors | i.i.d. resampling of the restricted residuals, recentred | Gaussian draws with Σ̂ of the fit at rank r |
| initial values | the observed ones | `Ȳ` from its mean with a burn-in of 50 + 10p; `Y₂` cumulated from its first observed level |
| the I(1, r) check | explicit: p − r unit roots, the others outside the circle | explicit too (`i1r_check`): `Ȳ` stationary and `∇Y₂`'s long-run covariance of rank s; with q = 0 it holds by construction ([ESTUDIO_CRT_2026-10-24.md](ESTUDIO_CRT_2026-10-24.md) §3) |
| failed replications | discarded when the root check fails | discarded when a refit does not converge (`fitfail`), counted in `reps` |

Three consequences follow.

* **No theorem covers the program's case.** Moving-average terms, the exact
  likelihood (whose LR is not nested at `Λ = 0` and goes negative in about 40 %
  of the null draws, BUG-26) and the λ-max form are all outside the three
  papers. Gaussian draws should not change the first-order result, since the
  limit distribution does not depend on normality, but that is an argument, not
  a proof. The justification for `-bootstrap` here is the papers' design plus
  measurement.
* **The moving average can nearly undo what the check checks.** A stationary
  `Ȳ` does not by itself give `Y₂` its s stochastic trends: that needs
  `rank(Λ⊥′Θ(1)) = s`, condition (d) of
  [ESTUDIO_MAURICIO_2026-09-23.md](ESTUDIO_MAURICIO_2026-09-23.md), the
  VARMA counterpart of the full rank of `α⊥′Γβ⊥`. `-bootstrap` now checks it
  exactly before simulating, but with q ≥ 1 and a null rank below the true one
  the fit absorbs the surplus stationary directions with MA roots next to the
  unit circle, and the sample it simulates carries *nearly* fewer trends than
  H₀ says — the problem Cavaliere, Rahbek and Taylor solve in the VAR, back
  through the moving average. Measured in
  [ESTUDIO_CRT_2026-10-24.md](ESTUDIO_CRT_2026-10-24.md) §3; its effect on
  power is open.
* **Discarding on non-convergence is not discarding on the root check.** If the
  refits that fail are not a random subset of the draws, the simulated
  distribution is censored. The count is printed so that it can be judged.

**The restrictions.** Cavaliere, Nielsen and Rahbek (2015) treat the LR of a
known β, and find what Section 1's asymptotics do not promise: the χ²
approximation is poor in finite samples, the bootstrap from unrestricted
estimates is unreliable exactly where the χ² fails worst, and a bootstrap that imposes the null is valid and
consistent. The χ² tests of `-fixb2`, `-fixb2row`, `-alpha`/`-weakex` and
`-gzero` are in that situation; the last three are not β = τ, so the paper is
the closest result rather than a direct one. When the χ² over-rejects, its
p-values are too small, so a restriction it does not reject would not be
rejected by a better-sized test either; a χ² rejection near 5 % is the reading
that needs care. `-matest` and `-artest` already simulate under the restricted
model, which is the design the paper validates.

## 8. What this does *not* establish

* **It is asymptotic theory.** Every result above is a limit result, and this
  program is used on 60–120 observations. On Cavaliere, Rahbek and Taylor's
  design (§7; q = 0, case 2) the test of the true rank rejects 11 % at T = 50
  with the tables and 4 % with the bootstrap; at T = 100 the tables reject 4 %
  ([ESTUDIO_CRT_2026-10-24.md](ESTUDIO_CRT_2026-10-24.md)). The earlier
  figure, six times the nominal level at `n = 120` and four with the bootstrap
  ([HOMOLOGATION.md](HOMOLOGATION.md) §2.3), predates BUG-24, 26, 27 and 50
  and no longer describes the program. Mélard et al. report finite-sample
  distortion for the MA case, which that design does not cover. Optimality in
  the limit is not accuracy in the sample.
* **Optimality is conditional on the specification being right.** `drvec` is the
  licensed case *when the data have an ARMA error structure of the order fitted*.
  A misspecified VARMA has no stronger claim than a misspecified VECM, which is
  why a residual diagnosis is reported with every fit
  ([USAGE.md](USAGE.md) §5).
* **`r = M` is outside the model class.** If the series have no unit roots, the
  transformation has nothing to remove and the parameterisation cannot express the
  answer; that is `drvarma`'s case, and reading a rank from `drvec` there is a
  mistake the program cannot catch for you.
* **Sections 1–6 are not about the *rank* decision**, which follows non-standard
  distributions in both frameworks and is the weaker part of both; §7 says what
  the bootstrap that replaces the tables rests on.

## 9. The genealogy, one line each

The papers in the bibliography, and what each contributes to the chain:

| | contribution |
|---|---|
| **Phillips (1991)**, *Optimal Inference in Cointegrated Systems* | the theorem, the crucial proviso, the triangular ECM, and Remark (j) naming what ARMA errors require |
| **Phillips (1991)**, *Error Correction and Long-Run Equilibrium in Continuous Time* | the same triangular ECM format in continuous time; temporal aggregation and aliasing |
| **Mauricio (2006)** | the transformation; states that (19) is Phillips' triangular form and that optimal inference therefore applies to EML |
| **Mauricio AddOn (JAM106)** | the closed form of the transformation for M=2, r=1 — used here to verify the cast to 0.000e+00 |
| **Johansen (1991)** | ML in the VECM: mixed-Gaussian limits, χ² tests on the cointegrating relations, non-standard rank test |
| **Johansen & Swensen (2024)** | `α = Aψ` restrictions; weak exogeneity as the factorisation that licenses conditional inference |
| **Cappuccio and Lubian (1996)** | the non-existence result: no VECM with independent errors for an ARMA DGP, so Johansen's route is misspecified there |
| **Ahn & Reinsel (1990)** | reduced-rank estimation imposing the unit-root structure — the AR-only precedent, no MA |
| **Mélard, Roy & Saidi (2004)** (`TR0444`) | the state-space route to the same exact likelihood; Hessian-at-the-optimum standard errors; finite-sample cost of MA terms |
| **Hillmer & Tiao (1979)** | the exact likelihood of a stationary VARMA — the machinery the transformation delivers its system to |
| **Chan & Wallis (1978)** | the mink–muskrat analysis and its `|Σ̂|`, used here to calibrate magnitude |
| **Trenkler (2004)** | response surfaces for finite-sample critical values — the route out of §8's first bullet |
| **Swensen (2006)** | the first theory for the rank bootstrap and the bootstrap sequential procedure, with a mixed restricted/unrestricted scheme |
| **Cavaliere, Rahbek & Taylor (2012)** | every parameter under the null rank: bootstrap data I(1, r) whatever the true rank — the scheme `-bootstrap` implements |
| **Cavaliere, Nielsen & Rahbek (2015)** | the bootstrap of the LR on β with the null imposed: valid and consistent where the χ² is not accurate |

The line of descent is: **Phillips supplies the theory and names the requirement;
Hillmer–Tiao and Mauricio (1997) supply the exact VARMA likelihood; Mauricio
(2006) joins them; `drvec` is that join, executable.**
