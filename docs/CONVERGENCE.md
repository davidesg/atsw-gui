# What the optimiser reports, and how much to believe it

*The reported termination of the optimiser, and how far it should be trusted.*

---

## 1. The three ways a run ends

The optimiser is Mauricio's factored quasi-Newton (`JASA` 90, 282–291), used
unmodified. It stops for one of three reasons:

| termination | what it means |
|---|---|
| **relative gradient** ≤ tolerance | a genuine stationary point; believe it |
| **step** ≤ tolerance ("scaled distance between the last two steps") | the search stopped moving. Usually fine, but it is a statement about the *steps*, not about the gradient |
| **"last global step failed to locate a lower point"** (termcode 3) | the line search could not improve from where it is. **This is the common outcome in `drvec`**, and it is the one to be careful with |

## 1b. The note the fit prints, and the two things it corrects

Every fit now ends with a **convergence note** in the `.out`: not the code, but
what it means. It exists because two things were being read the wrong way, and
neither was `drvec`'s invention — the suite had already settled both.

* **`ifault` is model adequacy, not convergence.** `ESTIMATION SUCCESSFUL
  (ifault = 0)` reads like a convergence and is not one: a fit that stopped far
  from an optimum can report `ifault = 0` perfectly well. `drvarma` documents
  this explicitly, and had the same defect before it was fixed there.
* **Termination on `steptol` was being announced as a plain "CONVERGED".** It is
  one in the program's sense, but it is the typical symptom of an
  ill-conditioned likelihood, and then the standard errors are not to be
  trusted. This is not hypothetical here: the canonical mink–muskrat fit —
  the baseline used throughout this repository — stops on `steptol`.

The wording follows `drvarma`'s and `drtran`'s, so the same situation reads the
same way across the suite. Where `drvec` differs it says so: in `drtran`,
termcode 3 usually means the fit began *at* the optimum, having been seeded from
a `.pre`. Here it means the surface is hard.

*How the note gets the termination code:* `est()` does not return it and
`report()` lives in `qnewtopt.c`, which is engine and is not touched — not even
by one line to expose an observable. So the note reads the criterion back from
the text the optimiser already wrote into the `.out`. Fragile with respect to
that one string and nothing else; the alternative was editing published,
refereed code.

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
(the optimiser study carried out for the transfer-function program of the same suite) tried and rejected three variants of
the scaling on evidence.

## 3. What to do about it

In descending order of usefulness:

0. **Run it from several starting points** — `-multistart n`. This is the one
   that moves the answer, and it is not a matter of taste: the surface's
   path-dependence is measured (§2), and the global search that provides this
   program's `|Σ̂|` reference was itself a multi-start. On the canonical case,
   going from one start to sixty moves `|Σ̂|` from 7 % above the reference value to
   1.6 %, and reduces the spread across four equivalent configurations by a
   factor of three. The figures are in [HOMOLOGATION.md](HOMOLOGATION.md).

   The perturbations are deterministic, so a result can be reproduced; and the
   procedure is monotone in `n`, so asking for more starts can only help. The
   **spread** it reports is a diagnostic in its own right: on a well-behaved
   surface every start lands in the same place, and here they do not.

   **How many is enough is a question the data answers, not a default.** On one
   applied series 10 starts stopped at a local optimum of 15.28 where the answer
   is 21.14, and 40 found it. What exposed it was not the convergence note —
   which said «clean convergence», truthfully: it reports *how* the optimiser
   stopped, not whether the point is the global maximum — but two checks that
   cannot be fooled:

   * **a negative likelihood-ratio** between nested models, which is impossible
     at the true maxima and therefore proves one fit did not get there;
   * **re-estimating with the roles of the two series swapped**: for a single
     cointegrating relation the two normalised vectors must multiply to 1.

   Each costs one additional fit, and both are advisable before a reported value is
relied upon.

   The swap check earns its keep beyond convergence: on two real pairs it was the
   **only** signal that the model class did not apply at all — the series had no
   unit root, so the true rank was `r = M`, which this program cannot express.
   Convergence was clean, the portmanteau passed, and the rank test rejected
   `r = 0`; the swapped fits multiplying to 2.10 instead of 1 was what said
   something was wrong, before the reason was known.

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
* **A grid over `B₂` with full fits.** Tried and rejected on measurement:
  scanning `-fixb2` from −0.60 to 0.10, the log-likelihood jumps from −7.4 to
  +6.5 between −0.30 and −0.24, which is not the shape of a profile likelihood
  but optimiser failures contaminating the scan. And even at the best `B₂` the
  `|Σ̂|` stays at 0.002466 — `B₂` was not the cause.
* **Touching the optimiser.** It is published, refereed work, and the study that
  covers the whole suite is the place where that question belongs. `-multistart`
  does not touch it; it runs it more than once.

## 5. Summary

`drvec` will usually give you a fit that stopped on termcode 3. That fit is a
stationary-ish point of an exact likelihood, not a global optimum, and on this
class of problem the two are not close enough to ignore the difference. Read
`|Σ̂|` across equivalent configurations as a sanity check, use `-fixb2` as a
second opinion, and treat a single run as evidence rather than as an answer.
