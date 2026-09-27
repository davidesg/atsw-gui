# drvec external validation — JJ block (Denmark, Finland, UKconinc, UKconsumption, Canada, UKpppuip)

drvec 0.10 (`/home/david/Dropbox/SRC/drvec/bin/drvec`), R 4.3.3 / urca 1.3.4 `ca.jo`, statsmodels 0.14.6.
Re-run: `./run_all.sh` (tables) and `./repro_defects.sh` (defect repros). Repository untouched.

## Conventions (worked out from docs/USAGE.md, MODEL.md, COMPARISON_JOHANSEN.md, src/drvec.c)

* **Lag order.** urca `K` = order of the VAR in levels; statsmodels `k_ar_diff = K-1`; drvec carries
  `F_1..F_{p-1}` on nabla Y, so **drvec p = K** (= statsmodels k+1, as COMPARISON_JOHANSEN states).
* **Deterministics.** drvec case 2 (E[nabla Y2]=0, E[W] free) = restricted constant = ca.jo `ecdet="const"` =
  statsmodels `deterministic="ci"`. drvec case 3 (E[nabla Y2], E[W] free) = unrestricted constant =
  `ecdet="none"` = statsmodels `"co"`. drvec has **no** restricted trend (`ecdet="trend"`), **no seasonal
  dummies**, **no exogenous I(0) regressors** (`-interv` only subtracts deterministic terms estimated
  univariately in `.pre` files — a pre-subtraction, not a joint estimation).
* **Normalisation.** drvec `beta = [B2 ; I_r]` with the identity on the **last r columns** of the `.inp`
  (the printed `beta` already has Johansen's sign; alpha negative = error-correcting). Johansen's beta/alpha
  were renormalised the same way: beta_J <- V_r inv(V_r[Y1 rows]), alpha_J <- W_r V_r[Y1 rows]'.
  `E[W]` = minus Johansen's restricted-constant coefficient after that normalisation.
* **Not comparable by construction:** logL levels. drvec's is the EXACT likelihood of Ybar on n-1
  observations (it conditions on Y_1 and includes the stationary density of the initial values); Johansen's
  is CONDITIONAL on K initial values (T = n-K). Rank LRs are asymptotically equivalent (same null
  distributions; Mauricio 2006 Rem. 5) but not numerically equal in finite samples.
* **Trace.** drvec cannot fit r = M (a stationary VAR in levels), so it has no trace statistic and no
  last lambda-max. "partial trace" = sum of drvec's LR(i->i+1) for i = r..M-2, comparable with Johansen's
  trace(r) - lambda-max(M-1). Rank decisions below are sequential lambda-max at 5%.
* **Seasonals / dummies that drvec cannot hold** were removed beforehand from the LEVELS
  (`bench.py:preadjust`: regress nabla Y on [1, centred seasonals, X], cumulate the fitted exogenous part,
  subtract). That makes the spec EXPRESSIBLE but APPROXIMATE; its cost is measured by running ca.jo both
  with the real dummies and on the pre-adjusted data (rows 1 vs 2 of each table).
* `-m 2` ("Conditional (Approximate) ML") gives logL identical to `-m 1` at q = 0 (AS 311's approximation
  only touches the MA part), so drvec cannot reproduce Johansen's conditional likelihood itself.

## Optimum audit — why a "best-found" row exists

`-lrtest` runs ONE cold start per rank. For every rank r = 1..M-1 I also ran `-seedjoh`, `-multistart 30`,
and an exact likelihood **profiled at Johansen's beta** (`johfix`: the Y1 columns replaced by Johansen's
W = Y1 + B2_J'Y2, Y2 demeaned, `-fixb2 0`; unit-Jacobian map, so it is a lower bound for the free optimum).
The best of these is the "best-found" value. Where the cold fit is below it, the cold fit (and the
`-lrtest` sequence built on it) is a local optimum. Full per-route numbers: `RESULTS_rank.md`.

| case | ranks where cold/`-lrtest` < best found (logL gap) | effect on the rank decision |
|---|---|---|
| denmark (M=4) | none (all routes agree; profile at Johansen's beta 0.06-0.20 below) | none |
| finland (M=4, case 3) | none | none |
| UKconinc, UKconinc_seas (M=2) | none | none |
| finland_const (M=4) | r=2: -0.67 (tc3) | none |
| UKconsumption (M=3, logs) | r=1: **-22.30** (tc3); r=2: -3.73 (tc3) | LR sequence reads (25.52, 70.12) instead of (70.13, 32.98); r=2 either way |
| UKconsumption_lev (M=3, raw levels) | r=1: -16.4; r=2: -12.4 (all tc3; profile at Johansen's beta beats every free route at r=1) | unreliable |
| denmark5 (M=5, benchmark-file spec) | r=1: **-23.55**; r=2: **-33.27** (tc3; profile at Johansen's beta 862.11 beats -multistart 30's 861.61) | `-lrtest` gives r=0; best-found gives **r=2 = Johansen** |
| Canada (M=4, case 3) | r=1: -1.18 (cold is **tc1**, a gradient-converged local optimum); r=2: -5.36 (tc3) | r=0 either way (borderline) |
| UKpppuip (M=5, case 3) | r=3: -9.40 (tc3) | `-lrtest` prints a NEGATIVE LR (-9.38) r=2->3 (drvec flags it); best-found r=0 = Johansen lambda-max |

## Verdicts

| case | verdict | one-line reason |
|---|---|---|
| urca_denmark (JJ1990, M=4, published spec) | **reproduces beta/alpha with explained differences; rank borderline, differs** | beta (m2=1: y -1.119, ib 5.103, id -4.190) vs published (-1.03, 5.21, -4.22); drvec lambda-max(0) 22.33 < 28.14 -> r=0, Johansen 30.02 -> r=1 (trace 49.14 < 53.12 also says r=0 with Osterwald-Lenum values; JJ's r=1 used their own tables). Optimum verified; the gap is exact vs conditional LR at n=55 |
| urca_denmark M=5 (benchmark file) | reproduces (only after the optimum audit) | best-found r=2 = ca.jo r=2; drvec's own `-lrtest` says r=0 because of local optima (defect D2) |
| urca_finland (JJ1990 spec: case 3 + seasonals) | **reproduces with explained differences** | r=2 in both; lambda-max (34.63, 27.74, 11.93) vs (38.48, 26.60, 7.89); beta rel.2 lnmr -4.80 vs -5.89 |
| urca_UKconinc | **reproduces** | beta -0.869 vs -0.868, alpha (-0.187,-1.420) vs (-0.185,-1.422), LR 69.94 vs 69.73. NB: ca.jo also rejects r<=1 (16.06 > 9.24) -> full rank; the README's r=1 is not what ca.jo says, and drvec cannot test r=M |
| urca_UKconsumption (re-run of the homologated fixture) | **rank reproduces; LR values and beta differ — partly explained, partly defect** | r=2 in all variants. `-lrtest` no longer gives the documented (70.13, 25.51) of ANALISIS_PRELIMINAR §4.4: its cold r=1 fit is a tc3 local optimum 22.3 below (D2). Best-found (70.13, 32.98) vs Johansen (76.90, 44.73): Johansen's r=2 fit has an EXPLOSIVE root (1.037), outside the exact likelihood's domain, so drvec's r=2 beta legitimately differs |
| vars_Canada | **cannot express** (restricted trend); closest spec (case 3) **disagrees** | Johansen none: r=1 (43.82); drvec best-found LR 26.62 < 27.14 -> r=0; beta is ill-identified under this spec (Johansen's own beta changes from (1.56, 0.86, -1.09) to (0.65, -1.81, -0.83) when ONE observation is dropped), multimodal surface |
| urca_UKpppuip | **cannot express** (seasonals + I(0) oil regressors); closest (pre-adjusted) **reproduces with explained differences** | ca.jo WITH the proper spec (season=4, dumvar) works and reproduces JJ1992 (lambda .407 .285 .254 .102 .083, trace r=2) — the benchmark's ca.jo failure came from passing the dummies as endogenous. On pre-adjusted data drvec and Johansen agree on lambda-max (r=0); drvec's r=2 beta close to Johansen's on the same data |

## Suspected drvec DEFECTS (separate from methodological differences)

**D1 — `-fixb2 v` builds its starting point from the static-OLS B2, not from v.** `init_guess` computes the
E[W], Lambda, F and Sigma seeds from W built with the static-OLS B2 and only then overwrites B2 with v
(src/drvec.c ~3243-3256 and ~3622-3629). When v is far from the OLS value the start is catastrophic and the
optimiser stalls. Repro (`repro_defects.sh`; UKconinc logs, [incl ; conl]):
```
drvec ukc  2 0 1 -case 2 -fixb2 0    -> start -831.72, end -275.36 (tc3)
drvec ukcd 2 0 1 -case 2 -fixb2 0    -> start   76.03, end  491.51 (tc1)   # ukcd = same data, incl demeaned
drvec ukc  2 0 1 -case 2             ->  523.7338 (free; identical on ukcd)
```
In case 2 demeaning a Y2 column is likelihood-invariant, so the two `-fixb2 0` fits must be identical.
Consequence: the LR "2[L(free) - L(fixed)] IS a valid LR test" advertised in `-h` (and used for Mauricio's
Table 5 test B = [1,0]') can be wrong by hundreds of units (here 1598 instead of ~64). On mink_muskrat it
happens to recover (start -143, converges to 0.78). Workaround used here: demean the Y2 columns.

**D2 — `-lrtest` relies on one cold start per rank, which stops at local optima.** Not a likelihood error
but an estimation-strategy defect that changes published conclusions: see the audit table above
(UKconsumption r=1 535.17 vs 557.47; denmark5 r=2 828.84 vs 862.11; UKpppuip negative LR; Canada r=1 a tc1
local optimum). Repro:
```
drvec uk 2 0 1 -case 2            -> 535.1702 (tc3)     drvec uk 2 0 1 -case 2 -seedjoh      -> 557.4726
drvec uk 2 0 2 -case 2            -> 570.2297 (tc3)     drvec uk 2 0 2 -case 2 -multistart 30 -> 573.9601 (tc1)
```
The documents are out of step with the binary: ANALISIS_PRELIMINAR §4.4 / ESTUDIO_BVECM (r=1 logL 557.4726,
LR 70.13 / 25.51) vs HOMOLOGATION §2.1 (LR 25.52 / 70.12, marked ✔) — the swap is the r=1 fit falling into
the local optimum after a seeding change; HOMOLOGATION's "urca_denmark M=5 r=2 logL 828.8447, converges" is a
tc3 point 33.3 below the profile at Johansen's beta.

**D3 (minor) — `-seedjoh`.** (a) `canonical_b2` demeans both regressions (Johansen's UNRESTRICTED constant,
ecdet="none") and uses T = n-1-p, while its comment says it corresponds to case 2; the seed therefore is not
ca.jo(ecdet="const") on the same data (UKconsumption r=2: seed (-1.408, -0.319) = statsmodels "co" on n-1
obs; ca.jo const gives (-1.365, -0.624); Canada r=1 seed (0.653, -1.807, -0.830) vs ca.jo none
(1.560, 0.859, -1.092)). Only a seed, so optima are unaffected. (b) When the canonical seed yields a
non-stationary start, the run aborts (`ifault = 3`, exit 2) instead of falling back to the OLS seed
(UKconsumption r=2, UKconsumption_lev r=2).

Cosmetic: with `-lrtest`, the `.out` header prints "cointegration rank r = 1" (the placeholder r argument).

Not a drvec defect, but in the repo's tooling: `tools/compare_johansen.py` labels
`coint_johansen(det_order=0)` as the restricted constant (case 2). Empirically det_order=0 reproduces
ca.jo `ecdet="none"` exactly (finland: 76.13/37.65/11.05/3.15 in both), i.e. the UNRESTRICTED constant, so the
"rank Johansen selects" column of COMPARISON_JOHANSEN §1 was computed under a different deterministic case
than drvec's (its beta comparison uses VECM "ci", which is right).

## Expected methodological differences (not defects)

1. **Exact vs conditional LR in small samples.** Where every route agrees on the optimum (denmark, finland,
   finland_const), drvec's LR(0->1) is below Johansen's lambda-max and its last testable LR above it:
   denmark (22.33, 11.16, 14.19) vs (30.02, 10.36, 6.33); finland (34.63, 27.74, 11.93) vs (38.48, 26.60,
   7.89). The partial traces are closer (denmark 47.68 vs 46.71). The exact - conditional logL gap varies
   with r (denmark 10.7, 6.9, 7.3, 11.2 for r=0..3): it contains one more observation plus the stationary
   density of the initial values of the (r-dependent) stationary block. At n=55, M=4 that moves the Danish
   r=0 test across the 5% line.
2. **Johansen points outside the exact likelihood's domain.** UKconsumption r=2 (root 1.037 in logs, 1.062
   in levels): the conditional estimator is unconstrained, exact ML is not, so beta/alpha differ.
3. **Pre-adjustment for seasonals** costs little (denmark trace 49.14 -> 49.02, beta equal to 3 dp; finland
   76.13 -> 76.13; UKconinc 63.06 -> 63.10). **Pre-adjusting the oil regressors** (UKpppuip) costs a lot
   (lambda-max(0) 31.33 -> 23.34; PPP beta on p2 0.812 -> 1.229): a real limitation, not approximable.
4. **q = 1** (default MA class, secondary column): higher logL everywhere, but the smallest MA root sits on
   the unit circle (1.0000) in finland, UKconsumption and UKpppuip and at 1.0065 in denmark, and most fits
   end tc3 — constrained/boundary optima, as the docs warn. Not used for the rank.

## Published values used (no JJ1990 / JJ1992 / Pfaff PDF in `literature/`)

* JJ1990 Danish (quoted from memory of the paper's Tables 3-4 / urca documentation, and reproduced
  **exactly** by ca.jo(K=2, ecdet="const", season=4) on LRM LRY IBO IDE): eigenvalues .4332 .1776 .1128
  .0434; trace 49.14 19.06 8.69 2.35; beta = (1, -1.03, 5.21, -4.22 | const -6.06); alpha = (-.213, .115,
  .023, .029). Note: the published model has **4** variables (LPY excluded), not the 5 of the benchmark file.
* JJ1992 (from memory, reproduced by ca.jo(K=2, season=4, dumvar=oil), ecdet="none"): eigenvalues .407
  .285 .254 .102 .083; r = 2.
* Pfaff (2008) Canada: trace 84.92 36.42 18.72 3.85 (ecdet="trend", K=3) — as in benchmark/README.md,
  reproduced exactly.
* JJ1990 Finnish: not verifiable locally; ca.jo under the urca-example spec (K=2, ecdet="none", season=4)
  gives r=2 (trace 76.13, 37.65, 11.00, 3.11).
* UKconinc / UKconsumption: no published Johansen analysis; references are ca.jo only.
* The `benchmark/<case>.results.txt` files all use K=2, ecdet="const", no seasonals, all columns — i.e. NOT
  the published specs (Denmark with LPY, Canada with K=2/const although the README quotes K=3/trend,
  UKconsumption in levels, UKpppuip with the oil dummies as endogenous).

---

