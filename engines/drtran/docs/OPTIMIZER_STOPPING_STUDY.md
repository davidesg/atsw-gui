# The optimiser's stopping tests: a study, and why nothing was changed

**Dates:** 2026-08-04 / 2026-08-05.
**Outcome: DIAGNOSED, NOT FIXED. The optimiser is unchanged and stays that way**
until there is a proven better alternative.
**Note 2026-09-26:** one change was made since, and it is not a change to the
stopping tests. The line search (`lnsrch`) never returned when the objective
was NaN or infinite. It now treats such a point as inadmissible, and it lives
once in `lib/optim/lnsrch.c` for drvarma, drtran, fue and fuf. On every finite
trajectory the arithmetic is the original's: the benches are byte-identical. The two
distributed copies carry the same fix: drvarma-python's C and `_qnewt.py`
(its BUG-0006, 2026-09-26) and the fue wheel's C and `qnewtopt.py` (fue
BUG-0025, 2026-09-27, where 103 corpus fits are bit-identical before and
after).

**Scope:** `qnewtopt.c` (`raxopt`, `umstop`, `umstop0`, `cdgrad`, `fdhess`) and
its faithful Python port `drvarma/_qnewt.py` — shared by `drtran`, `drvarma`
(`sima`) and, through them, `mtram`.

Read this before touching the stopping criteria. It exists so the study is not
repeated: every measurement below cost real time, and three plausible fixes were
implemented and rejected on evidence.

---

## 0. Why the bar for changing this is high

`raxopt` and `elf` are J. A. Mauricio's published, refereed work (the exact
VARMA likelihood is AS 311). The optimiser is Dennis & Schnabel A9.4.1 as
transcribed by him, and the Python side is a deliberate line-by-line port whose
whole value is that it reproduces the C's quasi-Newton trajectory — which is
what makes the pure-Python standard errors match the C engine's.

So a change here is not a local edit. It moves every estimate in the suite, it
invalidates the byte-exact output comparisons, and it has to be re-homologated
against the TASTE oracle. **The convergence announcements are questionable in a
specific, understood way — but "questionable" is not a licence to change
published numerics on the strength of one session's experiments.**

---

## 1. The mechanism

`qnewtopt` fixes Dennis & Schnabel's **typical parameter size** (`typx`) to 1.
That is the book's simplified A9.4.1; the full algorithm takes `typx` as an
input. Four places, one decision — C `src/qnewtopt.c`, port `_qnewt.py`:

| site | expression | what it is |
|---|---|---|
| `qnewtopt.c:185, 208` (`max1`) | `\|g\|·(\|x\|+1)/(\|f\|+1)` | gradient test |
| `qnewtopt.c:215` (`max2`) | `\|Δx\|/(\|x\|+1)` | step test |
| `qnewtopt.c:401` (`cdgrad`) | `η^⅓·max(\|x\|,1)` | finite-difference step |
| `qnewtopt.c:134` (`fdhess`) | `η^⅓·max(\|x\|,1)` | Hessian step |

The `+1` is a smoothed `max(|x|, 1)`. It is exactly right while the parameters
are of order 1. When they are not — at `refactor=1` the deterministic omegas are
~1e-4 — the gradient test degenerates into an **absolute** tolerance that cannot
be met, and the step test into an absolute one that is met immediately.

**The optimiser still finds the optimum. What it loses is the ability to certify
it.** Canonical case (`ES_CPI ← WTI`, `-V`, b=0 r=0 s=1), same model at two
scales — only mu and the 11 deterministic omegas differ, by construction:

```
refactor=100   logL  -718.287406   termcode 1 (gradient)   25 iters
refactor=  1   logL  1261.935774   termcode 2 (step)       25 iters
omegas identical to six decimals in both: 0.016400, -0.010747
```

`1261.935774 − 2·215·ln(100) = −718.287406` exactly: it is the same model, and
the log-likelihood difference is precisely the Jacobian of the rescaling.

The stopping statistic at the optimum, measured:

```
refactor=100   max|g| 2.152e-08   relgrad 1.088e-08   (<= 1e-7, FIRES)
refactor=  1   max|g| 1.007e-05   relgrad 5.034e-06   (does NOT fire)
```

The worst slot at `refactor=1` is `omega_d1[10,0]`, x = +9.8e-05 — a
**deterministic** omega, i.e. exactly the one class of parameter that shrinks
when the data is rescaled. The AR/MA and the transfer omegas are scale-free.
With `typx = |x|` the same statistic is `2.471e-08` and would fire.

Both the C and the port reproduce this identically, to six decimals and to the
same termination criterion.

### 1.1 A hypothesis that was recorded and is WRONG

The old TODO note blamed "finite-difference gradients (step ~6e-6) with a
terrible signal-to-step ratio at raw scale". The `~6e-6` is real — it is
`η^⅓ = 6.06e-6` from `cdgrad:401` — but the conclusion is false. **Measured:
scaling the finite-difference step changes nothing**: same optimum, same
termcode, same 25 iterations. The defect is in the stopping tests, not in the
gradient.

Moreover, scaling the step would be *harmful*. The objective is a ratio of order
1, for which the absolute step `η^⅓·max(|x|,1) ≈ 6.06e-6` is near-optimal. A
step relative to a ~1e-4 parameter gives `h ≈ 6e-9`, whose cancellation error
`ε·|f|/h ≈ 3e-7` **exceeds `gradtol` itself**. The same argument protects
`fdhess`. Neither should ever be made relative.

---

## 2. What was tried, and what each attempt cost

Three formulations of `typx` were implemented and measured end-to-end.

### 2.1 Adaptive floor — `typx_i = max(|x_i|, 1e-3)`, recomputed each iteration

Applied to `max1` and `max2` only. Results:

* **Fixes the certificate.** `refactor=1` returns to termcode 1 with an
  identical optimum, in both the C and the port.
* **Fixes a runaway.** drtran's near-collinear case (`-b 0 -r 0 -s 1 -S` with
  `q[2,1] = free`) burns all 500 iterations under the historical test and lands
  on `q[2,1] = −7.15`, |t| = 2424; with `typx` it converges by gradient in 23
  and reports `q[2,1] ≈ 0`, s.e. 9.44, t = 0.
* **But it breaks scale-invariance of the point estimates.** On the WTI/IPC
  pass-through — this engine's documented ill-conditioned case, cond(cov)
  1e5–1e7 — `max|φ(scale=100) − φ(scale=25)|`:

  | | ES | FR | DE |
  |---|---|---|---|
  | historical | 7.9e-06 | 2.2e-05 | 1.2e-05 |
  | adaptive `typx` | 3.5e-04 | **3.7e-03** | 8.8e-05 |

  Up to 165× worse, against a documented tolerance of 1e-4. This failed
  `test_passthrough_point_estimates_scale_invariant` for ES and FR, plus four
  byte-exact output comparisons downstream of the same shift.
* **And it costs likelihood.** In the `var_disparity` regime it saves
  iterations (166 → 110) but the log-likelihood ends **2.55e-04 worse**. The
  saving is not free: it stops earlier.

### 2.2 Norm-relative floor — `typx_i = max(|x_i|, τ·max_j|x_j|)`

Motivated by the hypothesis that the fixed 1e-3 floor is itself scale-dependent,
so a floor proportional to the parameter vector's norm would restore invariance.

**Gives numerically identical results to 2.1.** The hypothesis is false: the
floor's scale-dependence is not the cause.

### 2.3 Fixed `typx` — frozen once from the seeds (Dennis & Schnabel's actual design)

Motivated by the observation that 2.1 and 2.2 are *adaptive*: the criterion is
recomputed from the current iterate, so it moves during the optimisation, and in
a flat basin where you stop then depends on the path. D&S intend `typx` as a
fixed vector supplied by the caller.

| rule | ES | FR | DE | drtran `refactor=1` |
|---|---|---|---|---|
| historical | 7.9e-06 | 2.2e-05 | 1.2e-05 | termcode 2 |
| adaptive | 3.5e-04 | 3.7e-03 | 8.7e-05 | termcode 1 |
| **fixed** | 3.4e-04 | 2.2e-04 | 1.2e-05 | termcode 1 |

Clearly better than adaptive — FR improves 17×, DE recovers completely — so path
dependence *is* part of the problem. **But it still fails**: ES 3.4e-04 and FR
2.2e-04 are both above the 1e-4 tolerance. Freezing the criterion explains part
of the degradation, not the residue.

### 2.4 The residue, stated honestly

What is left after removing path dependence is simply that **a relative test
stops earlier than an absolute one**, and on an ill-conditioned likelihood the
extra depth the historical test forces is what makes two rescalings of the same
problem agree. `typx` trades convergence depth for iterations. On a
well-conditioned problem the trade is free; on the ill-conditioned data that is
drvarma's documented use case, it is not.

---

## 3. A claim made during the study that was over-stated

The `q[2,1]` runaway (§2.1) was initially presented as "the real bug the fix
cures". That is too strong. `test_battery.sh`'s case 3d **asserted that runaway
as the correct demonstration of near-collinearity**, and the person who wrote it
had a case: a parameter that is not identified, on an unbounded flat ridge, that
exhausts `maxits` and reports termcode 4, is telling the truth — *this did not
converge*. Reporting `t = 0` with a large standard error is a different and
arguably nicer answer, but it is not obviously the more honest one.

The two battery tests changed during the study (3d, and the SYNI `phi` identity
tolerance 1e-6 → 5e-6) have been **restored to their originals**.

---

## 4. What the ORACLE said

Independently of the above, on the cases TASTE covers the change was inert:

| case | historical vs `typx` |
|---|---|
| `cpi_wti_canonical` (6 parameters) | difference **0.0e+00** |
| `syn_estimate` | identical: 0.783515, −0.407915, 0.276244 |

7/7 oracle cases pass either way. This is consistent with everything else: the
effect is a property of the **conditioning of the problem**, not of which
program is calling. Well-scaled data does not notice.

The same point, from drtran's own side — the canonical case's scale invariance:

```
historical   max|x(100) − x(25)| = 1.20e-09    termcodes 1/2
typx=1e-3    max|x(100) − x(25)| = 2.08e-08    termcodes 1/1
```

The same ~17× degradation as drvarma, harmless here only because the problem is
well conditioned. **drtran and drvarma are the same program once inside
`raxopt`** — the embedded cast IS a VARMA — so enabling this in one and not the
other was never defensible.

---

## 5. Where sima stands

`sima`/drvarma does **not** exhibit the runaway. Nothing reaches `maxits` in any
bench regime (`well`, `near_unit_root`, `var_disparity`, `high_corr`,
`near_cancellation`): its parameters are seeded from the data at O(0.1–1), and
its flat directions are genuine common factors where the line search fails
cleanly (termcode 3) and ends the loop. drtran's `q[2,1]` had an escape route
that drvarma's parameters do not — a covariance seeded at **zero** on an
unbounded ridge.

A separate negative result worth keeping: over-parameterised VARMA fits on
VAR(1) data stop on termcode 2/3 **with or without** `typx` (VARMA(2,1): 2→3;
VARMA(3,2): 3 both ways, same likelihood). So drvarma's recorded observation
that "every VARMA(3,2) stops on termcode 3" is **not** explained by this defect.
It is weak identification, and the open question in `estimate_py.py:329-331` —
whether termcode 3 means "at the optimum" or "ill-conditioned" — is untouched by
this study.

---

## 6. Current state of the code

**The optimiser is Mauricio's original, everywhere.** No `typx`, no globals, no
environment variables, in either the C or the Python port, in drtran or drvarma.

One thing was **added**, and it is a report-layer change, not a numerics one:
drtran's port now emits the same convergence interpretation drvarma already had
(`Fit.convergence_note`, printed under the status line), with drvarma's wording
unchanged, so the same situation reads the same way across the suite. termcode 3
is deliberately not flagged as a failure in drtran, because drtran seeds from
the `.pre` and on the diagonal rung that already IS the optimum.

Pinned so the study is not re-trodden:

* `drtran-python/tests/test_refactor_scale.py` — asserts the limitation itself:
  same model, same optimum, same iterations, **different termcode**, and that
  `fit` has no `typx` knob.
* `drtran-python/scripts/repro_refactor1_relgrad.py` — the stopping statistic
  slot by slot at both scales.
* The implementations of all three rejected variants are in this repository's
  git history (drtran `86aebb2`, drtran-python `e232e00`, drvarma `23be6b3`),
  recoverable with one `git show`.

---

## 7. Known caveat, deliberately left in place

At `refactor=1` the run reports termcode 2, and the convergence note then says
"typical of an ill-conditioned likelihood … treat the standard errors with
caution and re-estimate … with a smaller order". **In this specific situation
that advice is wrong**: the optimum is exact to six decimals, the standard
errors are fine, and the order is not the problem — the scale is.

It is left as-is because the announcements should be the ones Mauricio's code
made, and because the situation requires ignoring the standing guidance to
rescale to 100 (M0.2), which fue now emits by default. Qualifying that message
is a candidate for the future study, not a change to make in passing.

---

## 8. What a real fix would require

Not another afternoon of patches. Concretely:

1. **The literature.** Dennis & Schnabel's full A9.4.1 with `typx`/`typf`, and
   what the modern successors (More–Thuente line search, trust-region
   alternatives) do about scaling. Also whether Mauricio wrote anywhere about
   why he took the simplified form.
2. **A per-parameter-class `typx`**, which is the one variant NOT tried: AR, MA,
   transfer omegas and covariances have typical size 1; mu and the deterministic
   omegas have the size the data gives them. Only the second class changes under
   rescaling, and it is exactly where the test failed. This is the most
   promising untested option.
3. **Deciding what "converged" should mean on a flat likelihood** before
   changing how it is detected. §3 shows this is not obvious, and §5 shows the
   termcode-3 question is open independently.
4. **A homologation plan up front**: the TASTE oracle, the byte-exact output
   comparisons, the scale-invariance tests, and the C's 296-test battery — with
   the acceptance criteria written down *before* the change, not adjusted after.

Until then: **rescale to 100 and read the termcode.**
