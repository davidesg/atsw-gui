# Known bugs — drvarma (C)

## BUG (CRITICAL) — seasonal adjustment uses the wrong phase when the series does not start at subperiod 1

**Found:** 2026-07-28, while building the multiart MCP over the Python port and
running the WTI → CPI pass-through exercise.
**Files:** `src/deseason.c` (`deseasonalize_raw`, lines ~62-90),
`include/seasonal_detection.h` (`harmonic_regression_differenced_basis`).
**Caller affected:** `src/drvarma.c:356` — i.e. the main executable, not only the GUI.
**Same defect in the Python port:** `drvarma/src/drvarma/deseason.py`
(fixed there; see `drvarma/TODO.md` and `drvarma/tests/test_regression_bugs.py`).

### Symptom

With `-deseason` enabled, the seasonal pattern is subtracted **shifted by
`start_sub - 1` months**. The adjustment then *adds* seasonal variance instead of
removing it. It is completely silent — no warning, no diagnostic.

On real monthly CPI data (2002-2019), lag-12 autocorrelation of `dlog(CPI)`:

| series | no adjustment | adjusted, start_sub=2 | adjusted, start_sub=1 | monthly-dummy OLS |
|---|---|---|---|---|
| CPI_USA | +0.328 | +0.245 | **-0.140** | -0.137 |
| IPC_ES  | +0.798 | **+0.866** | **+0.098** | +0.056 |
| IPC_FR  | +0.728 | **+0.862** | **+0.250** | +0.201 |
| IPC_DE  | +0.608 | **+0.808** | **+0.177** | +0.163 |

### Root cause

The amplitudes are ESTIMATED by `harmonic_regression_differenced_basis()`, whose
signature takes no `start_sub`: its design matrix is built from `t = i + d + 1`, so
the harmonics are in a phase **relative to the first observation of the series**.
The resulting level dummies are therefore indexed by *offset from the start*.

But they are APPLIED in absolute-subperiod phase (`src/deseason.c:87-88`):

```c
double *level = transform_harmonics_to_dummies_general(coeffs, NULL, s);
for (int p = 0; p < s; p++) dummies[j][p] = level[p];
for (int i = 0; i < nobs; i++) {
    int period = (i + start_sub - 1) % s;   /* <-- absolute phase */
    raw[i + 1][j] -= level[period];         /* <-- relative-phase vector */
}
```

Estimation phase and application phase agree only when `start_sub == 1`. For any
other starting subperiod the two are off by `start_sub - 1` positions.

### Minimal reproduction

Take a series starting in January, and the same series with the first observation
dropped (so it starts in February), declaring `start_sub` correctly in each case.
The estimated dummy vector should be essentially identical. It comes out
**circularly shifted by one position** instead. Real IPC_ES levels, 2002-2019:

```
from JANUARY  : [-0.663, -0.750, -0.415,  0.365, 0.461, 0.466, -0.232, ...,  0.441,  0.349]
from FEBRUARY : [-0.751, -0.416,  0.365,  0.461, 0.466, -0.232, -0.195, ...,  0.350, -0.662]
```

Discrepancy as a share of the pattern amplitude: ES 83 %, DE 66 %, FR 57 %, US 36 %.
After the fix the same comparison changes the pattern by less than 1 % (only the
effect of one extra observation).

### Fix

Rotate the estimated dummies from start-relative into absolute-subperiod indexing
before storing and applying them. With `start_sub == 1` this is the identity, so
existing results and any C-vs-port parity goldens are unaffected.

```c
if (ok && do_des) {
    double *level_rel = transform_harmonics_to_dummies_general(coeffs, NULL, s);
    double *level = (double *) malloc((size_t) s * sizeof(double));
    /* level_rel is indexed by offset-from-start; dummies must be by subperiod. */
    for (int p = 0; p < s; p++)
        level[(p + start_sub - 1) % s] = level_rel[p];
    for (int p = 0; p < s; p++) dummies[j][p] = level[p];
    for (int i = 0; i < nobs; i++) {
        int period = (i + start_sub - 1) % s;
        raw[i + 1][j] -= level[period];
    }
    free(level);
    free(level_rel);
}
```

### Impact on past work

Any run that enabled deseasonalisation on a series **not starting at subperiod 1**
is affected. Runs starting in January (the common case, and the default
`start_sub = 1`) are correct — which is why this survived undetected. The published
WTI → CPI pass-through note is NOT affected: its data start in January 2002, and
its parameter and FEVD tables reproduce to within rounding once the correct phase
is used.

### Suggested regression test

Deseasonalise a synthetic series with a known seasonal pattern, starting at each of
the 12 subperiods in turn, and assert that the recovered dummies match the true
pattern every time. The existing tests only ever exercise `start_sub = 1`, which is
exactly the case that works. See
`drvarma/tests/test_regression_bugs.py::test_deseason_recovers_known_pattern_at_any_start_sub`.

---

## BUG (HIGH, FIXED in 5.0 as a report) — a fit pinned to the MA invertibility wall was reported as an optimum (bench case c2)

**Found:** 2026-09-28, by the second exact likelihood (`-lik shea`).

`tests/banco` case `c2_ipc_arma21` is `IPC.inp 2 1 -mean -diagcov`. With
`-m 2`:

- elf stops at ℓ = 66.2068;
- Shea's optimisation reaches ℓ = 69.2748.

Evaluated at Shea's optimum, **elf itself** gives 69.2747625346. So this is
not a disagreement between the likelihoods: the two agree to 1e-9 at every
point either optimiser visits. It is the optimiser's path. From the
Hannan-Rissanen start, drvarma's default route ends at a point 3.07 below
one it would itself score higher.

It is also the case where fdhess falls back to BFGS: the MA roots are at
modulus 1.00005, on the boundary. PSW ARMA(1,1) shows the same, smaller:
elf −871.0167 against Shea −870.9434.

### The study (2026-09-28): there is no interior maximum

**1. Every fit ends on the invertibility wall.** In all four runs (elf and
Shea, `-m 1` and `-m 2`), two MA inverse roots sit at modulus 1.000050.
That is exactly `chekma`'s threshold, beyond which a point is inadmissible
and the objective returns the sentinel. Only the stopping point differs:

| run | ℓ | iterations | stop |
|---|---|---|---|
| elf `-m 1` | 66.896 | 64 | "CONVERGED", steptol |
| elf `-m 2` | 66.207 | 58 | termcode 3, line search failed |
| Shea | 69.275 | 63 | termcode 3, line search failed |

**2. Why the wall.** The AR and MA polynomials nearly share two factors:

- a complex pair at ≈ 0.37 rad (period ≈ 16.6 months), with AR modulus
  0.977–0.987 and MA modulus 1.00005;
- a real root near −1 (Nyquist), with AR at −0.92/−0.96 and MA at
  −0.98/−0.99.

A nearly redundant ARMA(2,1) has a likelihood ridge along the cancellation,
and here the ridge rises towards the MA unit circle. This is the
over-parameterisation of §2 of the published-suite review, not a numerical
accident.

**3. The ridge keeps rising along the wall.** The fit was reproduced in the
Python port (same elf, ℓ = 66.206790, 58 iterations, termcode 3) and
restarted from the stopping point with Θ₁ scaled by 0.97, which pulls the MA
roots inside the circle. ℓ climbed in each round: 66.21 → 67.07 → 68.34 →
69.68 → **71.22**, then fell. Shea's 69.27 is not the top either. Each round
ends on the wall again.

### Conclusion

**This is not an optimiser defect that a better search would fix.** The
likelihood has no maximum inside the invertible region: its supremum lies on
the boundary, along a ridge. An unconstrained quasi-Newton method with a
sentinel wall stops at its first contact with the wall, so the reported
point, its ℓ and its estimates depend on the path. Every one of the numbers
above is arbitrary. The defects are in **what is reported**:

- `-m 1` says **"OPTIMIZER CONVERGED"** for a point pinned to the wall;
- the only trace is the SE line ("the optimum is on the boundary"). Nothing
  says that the estimates themselves are not a maximum, nor why: the
  near-common factors.

**Resolved as a reporting defect (2026-09-28), deliberately minimal.**
The engine now says what is true and adds nothing else:

- the stop line reads `OPTIMIZER STOPPED at the MA invertibility boundary`;
- `MA boundary: k of n inverse roots at modulus >= 1` is written as data.

It gives no verdict, refuses nothing and changes no model. The estimation
problem is ill-defined here, and studying it belongs to the assistant (sima)
with the LLM: a second path (`-lik shea`), restarts along the wall, and the
table of near-common AR/MA roots. The user decided against elaborate
safeguards in the engine.

---

## BUG (LOW) — Shea's likelihood underflowed for a Q of small scale

**Status: FIXED in 5.0** (2026-09-28), before `marma` was ever called.

`marma` returned the determinant factor as
`r2 = exp(log(exp(detp) · tsig^(n−j7)) / n)`. With a Q of small scale,
`detp` is about −1500 for n = 216, and `exp(−1500)` is 0 as a double. The
objective collapsed to 0, and every estimate was garbage: SEs of 0 and
infinite t. It showed on the first run of the `pt_*` bench cases with
`-lik shea` (WTI, variance 0.0017). It is now computed in logarithms,
`r2 = exp((detp + (n−j7) log tsig) / n)`.

---

## BUG (MEDIUM) — a Hessian that is not positive definite gave standard errors, and its status was lost

**Status: FIXED in 5.0** (2026-09-27). Found while making fdhess the default
(drvarma-python `docs/STUDY-standard-errors.md`). The same code in drtran is
its BUG-56 (drtran-python `docs/BUGS.md`), where it produced |t| > 100 at a
ridge.

`est` factored the fdhess Hessian with `choldcp`. That is the optimiser's
modified Cholesky: it patches small or slightly negative pivots, so a Hessian
that is not positive definite still yielded a covariance. When `choldcp` did
fail, its status went to `*ifault`, and block [4] (`cast`/`elf` at the final
point) then overwrote it. It affected only the ladder, the one path that
called fdhess.

**Fix.** `fdhess_cov` (drvmlest.c):

- checks the Hessian with a plain Cholesky;
- counts the neighbours `objcfunc` refuses. Any refusal means the optimum is
  on the boundary, where no unrestricted Hessian exists;
- keeps the BFGS factor in either case, and `est_se_how` records why. The
  reports print it (`Standard errors: …`).

`tests/escalera/pruebas.sh` §10b checks the method and its statement.

---

## BUG (HIGH) — the line search never returns when the objective is NaN

**Status: FIXED in 5.0** (2026-09-26). `lnsrch` treats a non-finite trial
value as an inadmissible point (λ ← 0.1λ without interpolating, and gives up
below `minlam`; `haveprev` replaces `lambda == 1.0` as the first-backtrack
test), and `objcfunc` returns 1.0 for a non-finite objective, as drtran does.
The same fix as drvarma-python's `csrc` (its BUG-0006). Regression:
`tests/escalera/pruebas.sh`, item 10 (the probe above, NaN and ±∞; before
the fix both hang, `timeout` → 124).

**Found:** 2026-09-23, in the review of drvec (whose engine files are
byte-identical to these; there it is BUG-41, item 1). Confirmed on this tree
2026-09-24.
**Files:** `src/qnewtopt.c` (`lnsrch`, the backtracking loop ~489-541),
`src/drvmlest.c` (`objcfunc`, which returns the objective unguarded).
**Same code in:** drvec (`src/qnewtopt.c`, `src/drvmlest.c`, identical).
**Already fixed in:** drtran (`src/drvmlest.c` ~180-197, `if (!isfinite(f)) return 1.0;`).

### Symptom

The program hangs: no error, no output, the process spins.  drtran met it on
real data — a long-memory rational transfer (δ ≈ 0.95) on 69 observations — and
ran for an hour and a half.

### Root cause

When the trial point gives `*fkp1 = NaN`, every comparison in the loop is false:
the step is neither accepted (`*fkp1 <= fk + …`) nor abandoned
(`lambda < minlam`), the interpolated `tlambda` is NaN, `lambda` becomes NaN,
and `*retcode` stays 2 for ever.  Whether drvarma's `elf` can itself return a
NaN is not established — `choldcp` and `chekma` let a NaN parameter through — but
nothing upstream prevents it.

### Minimal reproduction

The real `raxopt` with an objective that is NaN beyond a threshold
(drvec: `tests/repro/probes/probe_nan.c`):

```c
#include "main.h"
real macheps; FILE *outputv; int quiet_mode = 1; long nf = 0;
void raxopt(real (*)(real []), real *, int, real *, real **, int, int, real, real);
real f(real *x) { nf++; if (x[1] > 2.0) return NAN;
                  return 0.1*(x[1]-3)*(x[1]-3)/0.9; }
int main() { macheps = cmacheps(); outputv = stdout;
  real *x = vector(1,1); x[1] = 0.0; real **b = matrix(1,1,1,1), fk;
  raxopt(f, &fk, 1, x, b, 100, 1, 1e-6, 1e-8);
  printf("x=%g fk=%g nf=%ld\n", x[1], fk, nf); }
```

```sh
gcc -O0 -Iinclude -o pn probe_nan.c src/qnewtopt.c src/nlatools.c -lgsl -lgslcblas -lm
timeout 10 ./pn; echo $?        # 124: still looping after 10 s
```

### Fix

drtran's: in `objcfunc`, treat a non-finite objective as an inadmissible point,
exactly as an `elf` ifault is treated (return 1.0).  Optionally, and
independently, make `lnsrch` give up (`*retcode = 1`) when `*fkp1` or `tlambda`
is not finite.  Then propagate to drvec.

### Impact

A hang instead of an error, on data that push a trial step to a non-finite
likelihood.  Not observed in drvarma's own runs.

---

## BUG (HIGH) — heap overflow in the residual histogram (`File_HistSer`)

**Status: FIXED in 5.0** (2026-09-26). A count wider than its cell is printed
as `**` (or `****`), so every cell keeps exactly `nphor` characters. Checked
with the probe above under AddressSanitizer: `700 6`, `2000 7` and `20000 7`
are clean (before the fix, `2000 7` overflows). Still to propagate to drtran
and drvec.

**Found:** 2026-09-23, in the review of drvec (BUG-42, item 1; sibling of the
one-byte overflow fixed in bb81180, "BUG-16").  Confirmed on this tree
2026-09-24 with AddressSanitizer.
**File:** `src/diagnose.c`, `File_HistSer`, the count labels ~797-817.
**Same code in:** drvec (identical file); drtran (`src/diagnose.c` ~766, same
label code).

### Symptom

Memory corruption, silent in a normal build.  ASan:

```
ERROR: AddressSanitizer: heap-buffer-overflow ...
    #1 in File_HistSer src/diagnose.c:817
```

### Root cause

Each row `aux[j]` holds `NumCat` cells of `nphor` characters.  With a residual
beyond ~4σ the histogram widens its range and `nphor` drops to 2, but a count of
100 or more prints 3 characters (`sprintf(s1, "%d", freqs[i])`; the `nphor == 2`
branch only pads a 1-digit count).  Each such cell writes one byte more than the
row has; several in a row run past the block.  It needs n of the order of 700
or more with one outlier.

### Minimal reproduction

drvec: `tests/repro/probes/probe_hist.c` — n standard-normal values, one set to
6σ, then `File_StatSer` and `File_HistSer`:

```sh
gcc -g -fsanitize=address -Iinclude -o ph probe_hist.c src/diagnose.c src/nlatools.c -lgsl -lgslcblas -lm
./ph 700 6        # heap-buffer-overflow at diagnose.c:817
```

### Fix

Make every label exactly `nphor` characters wide: when the count does not fit,
print a marker (e.g. `**`) or cap it, and size the rows from the widest label
actually printed.  Then propagate to drvec and drtran.

### Impact

Any run whose residuals have one large outlier on a sample of ~700 or more:
undefined behaviour in the diagnosis, which may corrupt the report or crash.
