# What the optimiser reports, and how much to believe it

*Read this before trusting a fit. It is the honest part of the documentation.*

---

## 1. The three ways a run ends

The optimiser is Mauricio's factored quasi-Newton (`JASA` 90, 282–291), used
unmodified. It stops for one of three reasons:

| termination | what it means |
|---|---|
| **relative gradient** ≤ tolerance | a genuine stationary point; believe it |
| **step** ≤ tolerance ("scaled distance between the last two steps") | the search stopped moving. Usually fine, but it is a statement about the *steps*, not about the gradient |
| **"last global step failed to locate a lower point"** (termcode 3) | the line search could not improve from where it is. **This is the common outcome in `drvec`**, and it is the one to be careful with |

## 2. Termcode 3 is the normal case here, and what is known about it

Most configurations of `drvec` stop on termcode 3. Two things are established
about it, and they point in opposite directions:

**It is not a flat ridge any more.** The likelihood used to have a direction it
could not see at all: the covariance block carried a scale the concentrated
objective is exactly invariant to. That was an identification defect, it was
removed (`Σ[1][1] = 1`; see [MODEL.md](MODEL.md) §3), and `npar` dropped by one.
Log-likelihood rose in almost every configuration and the Hessian stopped being
singular.

**But the surface is still hard, and this is measured.** In case 1 on
mink–muskrat, moving `Θ` from exactly zero to `diag(0.0108, 0.0611)` — a
perturbation of hundredths in two parameters — moves the point the optimiser
stops at by **14.45 units of log-likelihood**. That is not a seeding defect: with
`θ = 0` the same code path reproduces the cold start bit for bit, which is a
check in the test suite.

So: **where the optimiser stops depends on where it starts**, and in the worst
configuration it depends on it strongly. The literature agrees that this is in
the nature of the problem — Mauricio's own paper notes that «multicollinearity
appears to be natural to CI» — and the suite's separate study of the optimiser
(`drtran/docs/OPTIMIZER_STOPPING_STUDY.md`) tried and rejected three variants of
the scaling on evidence.

## 3. What to do about it

In descending order of usefulness:

1. **Fix `B₂` and see if it converges** — `-fixb2`. Holding the cointegrating
   vector converges in cases where the free model stops on termcode 3, and the
   difference in log-likelihood tells you how much of the difficulty is in that
   one direction.
2. **Compare the deterministic cases.** Case 1 is the fragile one: it forces
   `E[W] = 0`, and if the cointegrating combination has a level, the optimisation
   starts from a point dominated by that misfit and travels a long way.
3. **Check the layout.** A file read with `-differenced` when it is in levels
   (or the reverse) gives a plausible-looking fit of the wrong model. The layout
   actually used is echoed in the console and the `.out` header.
4. **Compare the equivalent parameterisations.** The same model in the levels
   and pre-differenced layouts must give the same `|Σ̂|`; they now agree within
   0.00005, which is a check in the test suite. A wider gap means one of the two
   runs stopped somewhere it should not have.
5. **Use `-eval`** to separate a bad starting point from a bad path: it reports
   the likelihood *at* the starting point without optimising. If a change makes
   the start worse, it is the seed; if it makes the start better and the end
   worse, it is the surface.

## 4. What will not help

* **Seeding from the suite's `.pre` files above the diagonal rung.** Measured: it
  moves the starting log-likelihood *down* by 17 units at `r = 1`, for structural
  reasons ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4).
* **Reparameterising the covariance as `log` variances.** Tried and reverted with
  evidence: combined with informative seeding it drove the optimiser into regions
  where each likelihood evaluation is so slow that some configurations stopped
  finishing at all, and it lost log-likelihood on three of four configurations.
  Positivity is enforced by an explicit check instead, which costs one Cholesky
  and does not touch the geometry.
* **Touching the optimiser.** It is published, refereed work, and the study that
  covers the whole suite is the place where that question belongs.

## 5. The honest summary

`drvec` will usually give you a fit that stopped on termcode 3. That fit is a
stationary-ish point of an exact likelihood, not a global optimum, and on this
class of problem the two are not close enough to ignore the difference. Read
`|Σ̂|` across equivalent configurations as a sanity check, use `-fixb2` as a
second opinion, and treat a single run as evidence rather than as an answer.
