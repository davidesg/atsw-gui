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

# Compact tables — generated by final.py

### denmark — JJ1990 Danish money demand, published spec: m2,y,ib,id; K=2; restricted constant; centered seasonals
M=4, n=55, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=LRY IBO IDE LRM; **drvec data pre-adjusted** (seasonals removed from levels, not jointly estimated). Published/assumed rank r=1.

Published: JJ1990 Danish: r=1 chosen; lambda=(.4332,.1776,.1128,.0434); trace (49.14, 19.06, 8.69, 2.35); beta=(m2 1, y -1.03, ib 5.21, id -4.22, const -6.06); alpha=(-.213, .115, .023, .029) [values from memory of JJ1990 Tables 3-4 / urca docs; paper not in literature/ -- reproduced EXACTLY by ca.jo row 1]

| source | rank 5% (trace / lmax) | lmax r=0..3 | trace r=0..3 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const, season=4) | 0 / 1 | (30.09, 10.36, 6.34, 2.35) | (49.14, 19.06, 8.69, 2.35) | (-1.033, 5.207, -4.216, 1.000) | (0.115, 0.023, 0.029, -0.213) | (6.060) | 669.115 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 0 / 1 | (30.02, 10.36, 6.33, 2.31) | (49.02, 19.00, 8.64, 2.31) | (-1.034, 5.208, -4.218, 1.000) | (0.116, 0.023, 0.029, -0.212) | (6.052) | 668.802 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-1.034, 5.208, -4.218, 1.000) | (0.116, 0.023, 0.029, -0.212) | (6.052) | 668.802 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 0 | (22.33, 11.16, 14.19) | partial (47.68, 25.35, 14.19) | | | | (664.50, 675.66, 681.24, 688.33) | r0 ok, r1 ok, r2 ok, r3 ok |
| drvec q=0 best-found per rank | - / 0 | (22.33, 11.16, 14.19) | partial (47.68, 25.35, 14.19) | | | | (664.50, 675.66, 681.24, 688.33) | routes: r0 lrtest, r1 cold, r2 cold, r3 cold |
| drvec q=0 at r=1, best route = cold | (imposed) | | | (-1.119, 5.103, -4.190, 1.000) | (0.141, 0.012, 0.025, -0.180) | (5.532) | 675.660 exact | yes (tc1 gradient) |
| drvec q=1 (default MA class) at r=1 | (imposed) | | | (-1.120, 2.859, -1.094, 1.000) | (0.126, -0.002, -0.009, -0.111) | (5.434) | 683.987 exact | NO (tc3); min MA root 1.0065 |

### denmark5 — benchmark-file variant: 5 vars incl. LPY; K=2; restricted constant; NO seasonals (the spec of benchmark/urca_denmark.results.txt)
M=5, n=55, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=LPY IBO IDE LRM LRY. Published/assumed rank r=2.

| source | rank 5% (trace / lmax) | lmax r=0..4 | trace r=0..4 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const) | 2 / 2 | (63.50, 32.43, 17.54, 8.25, 1.97) | (123.69, 60.19, 27.76, 10.22, 1.97) | (-0.072, 4.385, -1.341, 1.000, -0.000); (0.060, -1.115, 0.738, 0.000, 1.000) | (0.022, 0.001, 0.010, -0.333, -0.023); (-0.063, 0.013, 0.007, 0.066, -0.048) | (12.527, 6.213) | 855.159 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (63.50, 32.43, 17.54, 8.25, 1.97) | (123.69, 60.19, 27.76, 10.22, 1.97) | (-0.072, 4.385, -1.341, 1.000, -0.000); (0.060, -1.115, 0.738, 0.000, 1.000) | (0.022, 0.001, 0.010, -0.333, -0.023); (-0.063, 0.013, 0.007, 0.066, -0.048) | (12.527, 6.213) | 855.159 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-0.072, 4.385, -1.341, 1.000, 0.000); (0.060, -1.115, 0.738, 0.000, 1.000) | (0.022, 0.001, 0.010, -0.333, -0.023); (-0.063, 0.013, 0.007, 0.066, -0.048) | (12.527, 6.213) | 855.159 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 0 | (12.01, 9.48, 82.45, 18.72) | partial (122.66, 110.66, 101.17, 18.72) | | | | (818.10, 824.10, 828.84, 870.07, 879.43) | r0 ok, r1 tc3, r2 tc3, r3 ok, r4 tc3 |
| drvec q=0 best-found per rank | - / 2 | (59.11, 28.92, 16.16, 18.48) | partial (122.66, 63.56, 34.64, 18.48) | | | | (818.10, 847.65, 862.11, 870.19, 879.43) | routes: r0 lrtest, r1 seedjoh, r2 johfix, r3 seedjoh, r4 seedjoh |
| drvec q=0 at r=2, best route = johfix | (imposed) | | | (-0.072, 4.385, -1.341, 1.000, 0.000); (0.060, -1.115, 0.738, 0.000, 1.000) | (0.021, -0.001, 0.011, -0.344, -0.014); (-0.065, 0.013, 0.006, 0.120, -0.060) | (12.530, 6.205) | 862.111 exact | yes (tc1 gradient); cold/-lrtest fit: 828.845 tc3 |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-0.206, 2.333, 2.625, 1.000, 0.000); (-0.153, -0.295, 2.638, 0.000, 1.000) | (-0.007, -0.010, -0.027, -0.223, 0.113); (0.063, 0.037, 0.026, -0.026, -0.383) | (12.355, 6.147) | 828.775 exact | NO (tc3); min MA root 1331.2413 |

### finland — JJ1990 Finnish money demand: lrm1,lny,lnmr,difp; K=2; unrestricted constant; centered seasonals (urca example spec)
M=4, n=106, K=2 -> drvec p=2, case 3; columns [Y2;Y1]=lny lnmr difp lrm1; **drvec data pre-adjusted** (seasonals removed from levels, not jointly estimated). Published/assumed rank r=2.

Published: JJ1990 Finnish: paper not in repo; benchmark README records r=2 (from a generic ca.jo K=2 const, not the paper's spec)

| source | rank 5% (trace / lmax) | lmax r=0..3 | trace r=0..3 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=none, season=4) | 2 / 2 | (38.49, 26.64, 7.89, 3.11) | (76.13, 37.65, 11.00, 3.11) | (-0.012, 0.171, 1.000, 0.000); (-1.058, -5.894, 0.000, 1.000) | (-0.288, 0.690, -0.434, -0.695); (0.017, 0.100, -0.012, 0.013) | - | 924.492 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=none) | 2 / 2 | (38.48, 26.60, 7.89, 3.15) | (76.13, 37.65, 11.05, 3.15) | (-0.012, 0.171, 1.000, 0.000); (-1.057, -5.893, 0.000, 1.000) | (-0.287, 0.688, -0.434, -0.695); (0.017, 0.100, -0.012, 0.013) | - | 924.345 cond. | closed form |
| statsmodels VECM('co'), drvec's data | - | - | - | (-0.012, 0.171, 1.000, 0.000); (-1.057, -5.893, 0.000, 1.000) | (-0.287, 0.688, -0.434, -0.695); (0.017, 0.100, -0.012, 0.013) | - | 924.345 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 2 | (34.63, 27.74, 11.93) | partial (74.31, 39.68, 11.93) | | | | (899.07, 916.39, 930.26, 936.23) | r0 ok, r1 ok, r2 ok, r3 ok |
| drvec q=0 best-found per rank | - / 2 | (34.63, 27.74, 11.93) | partial (74.31, 39.68, 11.93) | | | | (899.07, 916.39, 930.26, 936.23) | routes: r0 lrtest, r1 cold, r2 cold, r3 cold |
| drvec q=0 at r=2, best route = cold | (imposed) | | | (-0.013, 0.133, 1.000, 0.000); (-1.026, -4.796, 0.000, 1.000) | (-0.293, 0.661, -0.441, -0.717); (0.022, 0.117, -0.011, 0.016) | (-0.022, -1.878) | 930.260 exact | yes (tc1 gradient) |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-0.012, -0.197, 1.000, 0.000); (-0.952, -3.783, 0.000, 1.000) | (-0.321, 0.718, -0.729, 1.271); (0.049, 0.090, 0.026, 0.013) | (-0.062, -1.416) | 939.048 exact | NO (tc3); min MA root 1.0000 |

### finland_const — benchmark-file variant: K=2, restricted constant, no seasonals
M=4, n=106, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=lny lnmr difp lrm1. Published/assumed rank r=2.

Published: JJ1990 Finnish: paper not in repo; benchmark README records r=2 (from a generic ca.jo K=2 const, not the paper's spec)

| source | rank 5% (trace / lmax) | lmax r=0..3 | trace r=0..3 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const) | 2 / 2 | (44.11, 32.77, 11.15, 7.76) | (95.78, 51.67, 18.90, 7.76) | (-0.012, 0.177, 1.000, 0.000); (-1.075, -5.742, -0.000, 1.000) | (-0.035, 0.915, -0.429, -0.778); (0.063, 0.110, -0.017, 0.031) | (-0.001, -2.352) | 865.522 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (44.11, 32.77, 11.15, 7.76) | (95.78, 51.67, 18.90, 7.76) | (-0.012, 0.177, 1.000, 0.000); (-1.075, -5.742, -0.000, 1.000) | (-0.035, 0.915, -0.429, -0.778); (0.063, 0.110, -0.017, 0.031) | (-0.001, -2.352) | 865.522 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-0.012, 0.177, 1.000, -0.000); (-1.075, -5.742, 0.000, 1.000) | (-0.035, 0.915, -0.429, -0.778); (0.063, 0.110, -0.017, 0.031) | (-0.001, -2.352) | 865.522 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 2 | (40.27, 32.24, 15.10) | partial (87.61, 47.34, 15.10) | | | | (833.23, 853.37, 869.49, 877.04) | r0 ok, r1 ok, r2 tc3, r3 ok |
| drvec q=0 best-found per rank | - / 2 | (40.27, 33.59, 13.75) | partial (87.61, 47.34, 13.75) | | | | (833.23, 853.37, 870.16, 877.04) | routes: r0 lrtest, r1 seedjoh, r2 seedjoh, r3 cold |
| drvec q=0 at r=2, best route = seedjoh | (imposed) | | | (-0.014, 0.133, 1.000, 0.000); (-1.034, -4.671, 0.000, 1.000) | (-0.069, 0.873, -0.441, -0.834); (0.076, 0.125, -0.016, 0.039) | (-0.019, -1.998) | 870.161 exact | yes (tc1 gradient); cold/-lrtest fit: 869.487 tc3 |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-0.018, -0.106, 1.000, 0.000); (-1.049, -3.390, 0.000, 1.000) | (0.062, 1.042, -0.719, 1.464); (0.108, 0.097, 0.003, 0.087) | (-0.072, -1.887) | 878.133 exact | yes (tc1 gradient); min MA root 1.3791 |

### UKconinc — UK log consumption/income (HEGY data), benchmark spec: K=2, restricted constant, no seasonals
M=2, n=120, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=incl conl. Published/assumed rank r=1.

Published: no published Johansen spec for these data (HEGY is a seasonal-cointegration paper); benchmark README asserts r=1

| source | rank 5% (trace / lmax) | lmax r=0..1 | trace r=0..1 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const) | 2 / 2 | (69.73, 16.06) | (85.79, 16.06) | (-0.868, 1.000) | (-0.185, -1.422) | (1.258) | 521.653 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (69.73, 16.06) | (85.79, 16.06) | (-0.868, 1.000) | (-0.185, -1.422) | (1.258) | 521.653 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-0.868, 1.000) | (-0.185, -1.422) | (1.258) | 521.653 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 1+ | (69.94) | partial (69.94) | | | | (488.76, 523.73) | r0 ok, r1 ok |
| drvec q=0 best-found per rank | - / 1+ | (69.94) | partial (69.94) | | | | (488.76, 523.73) | routes: r0 lrtest, r1 cold |
| drvec q=0 at r=1, best route = cold | (imposed) | | | (-0.869, 1.000) | (-0.187, -1.420) | (1.244) | 523.734 exact | yes (tc1 gradient) |
| drvec q=1 (default MA class) at r=1 | (imposed) | | | (-0.870, 1.000) | (-0.010, -2.351) | (1.240) | 550.186 exact | NO (tc3); min MA root 1.0494 |

### UKconinc_seas — UK log consumption/income with centered seasonals (ca.jo season=4), K=2, restricted constant
M=2, n=120, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=incl conl; **drvec data pre-adjusted** (seasonals removed from levels, not jointly estimated). Published/assumed rank r=1.

Published: no published Johansen spec for these data (HEGY is a seasonal-cointegration paper); benchmark README asserts r=1

| source | rank 5% (trace / lmax) | lmax r=0..1 | trace r=0..1 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const, season=4) | 2 / 2 | (45.37, 17.69) | (63.06, 17.69) | (-0.877, 1.000) | (0.715, 0.172) | (1.152) | 627.832 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (45.40, 17.69) | (63.10, 17.69) | (-0.877, 1.000) | (0.715, 0.173) | (1.152) | 627.787 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-0.877, 1.000) | (0.715, 0.173) | (1.152) | 627.787 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 1+ | (44.88) | partial (44.88) | | | | (608.91, 631.35) | r0 ok, r1 ok |
| drvec q=0 best-found per rank | - / 1+ | (44.88) | partial (44.88) | | | | (608.91, 631.35) | routes: r0 lrtest, r1 cold |
| drvec q=0 at r=1, best route = cold | (imposed) | | | (-0.876, 1.000) | (0.735, 0.174) | (1.167) | 631.347 exact | yes (tc1 gradient) |
| drvec q=1 (default MA class) at r=1 | (imposed) | | | (-0.883, 1.000) | (0.513, 0.282) | (1.084) | 642.182 exact | yes (tc1 gradient); min MA root 1.2395 |

### UKconsumption — Pokorny UK cons/inc/price, LOGS, K=2, restricted constant: the homologated drvec fixture (tests/run_tests.sh) re-run
M=3, n=76, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=lcons linc lprice. Published/assumed rank r=2.

Published: Pokorny (1987) p.408: data source only; the r=2 in the README comes from ca.jo (levels)

| source | rank 5% (trace / lmax) | lmax r=0..2 | trace r=0..2 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const) | 2 / 2 | (76.90, 44.73, 5.34) | (126.97, 50.07, 5.34) | (-1.365, 1.000, 0.000); (-0.624, 0.000, 1.000) | (1.626, 0.567, -0.028); (0.194, 0.123, 0.036) | (-3.040, -1.375) | 577.528 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (76.90, 44.73, 5.34) | (126.97, 50.07, 5.34) | (-1.365, 1.000, 0.000); (-0.624, 0.000, 1.000) | (1.626, 0.567, -0.028); (0.194, 0.123, 0.036) | (-3.040, -1.375) | 577.528 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-1.365, 1.000, 0.000); (-0.624, 0.000, 1.000) | (1.626, 0.567, -0.028); (0.194, 0.123, 0.036) | (-3.040, -1.375) | 577.528 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 2+ | (25.52, 70.12) | partial (95.64, 70.12) | | | | (522.41, 535.17, 570.23) | r0 ok, r1 tc3, r2 tc3 |
| drvec q=0 best-found per rank | - / 2+ | (70.13, 32.98) | partial (103.10, 32.98) | | | | (522.41, 557.47, 573.96) | routes: r0 lrtest, r1 ms30, r2 ms30 |
| drvec q=0 at r=2, best route = ms30 | (imposed) | | | (-1.275, 1.000, 0.000); (-1.182, 0.000, 1.000) | (1.596, 0.451, -0.023); (0.221, 0.177, 0.027) | (-2.262, -6.209) | 573.960 exact | multistart best start 23: yes (gradient); cold/-lrtest fit: 570.230 tc3 |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-1.253, 1.000, 0.000); (-1.620, 0.000, 1.000) | (1.331, 0.137, -0.281); (0.262, 0.107, -0.015) | (-2.057, -10.028) | 581.931 exact | yes (tc2 steptol); min MA root 1.0000 |

### UKconsumption_lev — same, in LEVELS (the spec of benchmark/urca_UKconsumption.results.txt)
M=3, n=76, K=2 -> drvec p=2, case 2; columns [Y2;Y1]=cons inc price. Published/assumed rank r=2.

Published: Pokorny (1987) p.408: data source only; the r=2 in the README comes from ca.jo (levels)

| source | rank 5% (trace / lmax) | lmax r=0..2 | trace r=0..2 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=const) | 2 / 2 | (67.57, 33.12, 5.61) | (106.30, 38.73, 5.61) | (-1.606, 1.000, 0.000); (-0.006, 0.000, 1.000) | (1.355, 0.571, -0.001); (16.394, 12.640, 0.034) | (-2780.013, 24.636) | -1101.457 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=const) | 2 / 2 | (67.57, 33.12, 5.61) | (106.30, 38.73, 5.61) | (-1.606, 1.000, 0.000); (-0.006, 0.000, 1.000) | (1.355, 0.571, -0.001); (16.394, 12.640, 0.034) | (-2780.013, 24.636) | -1101.457 cond. | closed form |
| statsmodels VECM('ci'), drvec's data | - | - | - | (-1.606, 1.000, 0.000); (-0.006, 0.000, 1.000) | (1.355, 0.571, -0.001); (16.394, 12.640, 0.034) | (-2780.013, 24.636) | -1101.457 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 1 | (28.88, 11.09) | partial (39.97, 11.09) | | | | (-1168.02, -1153.58, -1148.03) | r0 tc3, r1 tc3, r2 tc3 |
| drvec q=0 best-found per rank | - / 1 | (61.65, 3.19) | partial (64.84, 3.19) | | | | (-1168.02, -1137.19, -1135.60) | routes: r0 lrtest, r1 johfix_ms, r2 ms30 |
| drvec q=0 at r=2, best route = ms30 | (imposed) | | | (-1.403, 1.000, 0.000); (-0.039, 0.000, 1.000) | (1.424, 0.654, -0.000); (6.420, 1.296, -0.019) | (-1726.067, -146.127) | -1135.598 exact | multistart best start 22: NO (tc3); cold/-lrtest fit: -1148.033 tc3 |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-1.455, 1.000, 0.000); (-0.030, 0.000, 1.000) | (0.976, 0.226, 0.000); (4.425, 2.448, -0.042) | (-1816.132, -108.626) | -1148.033 exact | NO (tc3); min MA root - |

### Canada — Pfaff/Luetkepohl Canada: prod,e,U,rw; K=3; PUBLISHED ecdet='trend' (not expressible) -> drvec closest = case 3 (ecdet='none')
M=4, n=84, K=3 -> drvec p=3, case 3; columns [Y2;Y1]=e U rw prod. Published/assumed rank r=1.

Published: Pfaff (2008)/vars: ecdet='trend', K=3: trace (84.92, 36.42, 18.72, 3.85) -> r=1 [README]; reproduced EXACTLY by ca.jo row 1

| source | rank 5% (trace / lmax) | lmax r=0..3 | trace r=0..3 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=trend) | 1 / 1 | (48.50, 17.70, 14.87, 3.85) | (84.92, 36.42, 18.72, 3.85) | (-0.024, 3.169, 1.835, 1.000) | (-0.009, -0.005, -0.046, -0.007) | (1.302) | -161.838 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=none) | 1 / 1 | (43.82, 16.36, 10.72, 0.06) | (70.96, 27.14, 10.78, 0.06) | (1.560, 0.859, -1.092, 1.000) | (0.016, 0.005, 0.061, 0.001) | - | -164.179 cond. | closed form |
| statsmodels VECM('co'), drvec's data | - | - | - | (1.560, 0.859, -1.092, 1.000) | (0.016, 0.005, 0.061, 0.001) | - | -164.179 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 0 | (24.27, 7.02, 24.23) | partial (55.52, 31.25, 24.23) | | | | (-191.93, -179.80, -176.29, -164.17) | r0 ok, r1 ok, r2 tc3, r3 ok |
| drvec q=0 best-found per rank | - / 0 | (26.62, 15.39, 13.51) | partial (55.52, 28.90, 13.51) | | | | (-191.93, -178.62, -170.92, -164.17) | routes: r0 lrtest, r1 seedjoh, r2 ms30, r3 cold |
| drvec q=0 at r=1, best route = seedjoh | (imposed) | | | (-0.028, -1.961, -0.431, 1.000) | (0.025, 0.000, 0.062, 0.007) | (179.088) | -178.618 exact | yes (tc1 gradient); cold/-lrtest fit: -179.796 tc1 |
| drvec q=1 (default MA class) at r=1 | (imposed) | | | (2.051, 10.785, -0.248, 1.000) | (-0.008, -0.003, -0.046, -0.012) | (2330.473) | -177.718 exact | NO (tc3); min MA root 2.5094 |

### UKpppuip — JJ1992 PPP/UIP: p1,p2,e12,i1,i2; K=2; unrestricted constant; centered seasonals; dumvar=(doilp0,doilp1) unrestricted I(0)
M=5, n=62, K=2 -> drvec p=2, case 3; columns [Y2;Y1]=p1 p2 i2 e12 i1; **drvec data pre-adjusted** (seasonals+doilp0+doilp1 removed from levels, not jointly estimated). Published/assumed rank r=2.

Published: JJ1992: r=2; lambda=(.407,.285,.254,.102,.083) [from memory of JJ1992 Table 1; reproduced by ca.jo row 1 with season=4 + dumvar]

| source | rank 5% (trace / lmax) | lmax r=0..4 | trace r=0..4 | beta norm. on Y1 (cols) | alpha | E[W] | logL | converged |
|---|---|---|---|---|---|---|---|---|
| ca.jo, published spec (ecdet=none, season=4, dumvar) | 2 / 0 | (31.33, 20.16, 17.59, 6.48, 5.19) | (80.75, 49.42, 29.26, 11.67, 5.19) | (-0.874, 0.812, 4.021, 1.000, 0.000); (-0.055, 0.045, -0.550, 0.000, 1.000) | (0.060, 0.016, -0.082, -0.093, -0.018); (0.272, 0.064, 0.097, -0.345, -0.263) | - | 926.083 cond. | closed form |
| ca.jo, drvec's data & case (ecdet=none) | 2 / 0 | (23.34, 20.80, 13.61, 10.20, 4.82) | (72.78, 49.44, 28.64, 15.02, 4.82) | (-0.832, 1.229, 3.510, 1.000, 0.000); (0.043, -0.153, -0.766, 0.000, 1.000) | (0.029, -0.009, -0.030, -0.150, -0.035); (0.185, 0.083, 0.249, -0.644, -0.239) | - | 917.257 cond. | closed form |
| statsmodels VECM('co'), drvec's data | - | - | - | (-0.832, 1.229, 3.510, 1.000, 0.000); (0.043, -0.153, -0.766, 0.000, 1.000) | (0.029, -0.009, -0.030, -0.150, -0.035); (0.185, 0.083, 0.249, -0.644, -0.239) | - | 917.257 cond. | closed form |
| drvec q=0 `-lrtest` (as reported) | - / 0 | (22.47, 12.19, -9.38, 21.61) | partial (46.89, 24.42, 12.23, 21.61) | | | | (911.22, 922.46, 928.55, 923.86, 934.67) | r0 ok, r1 ok, r2 ok, r3 tc3, r4 ok |
| drvec q=0 best-found per rank | - / 0 | (22.47, 12.19, 9.42, 2.80) | partial (46.89, 24.42, 12.23, 2.80) | | | | (911.22, 922.46, 928.55, 933.27, 934.67) | routes: r0 lrtest, r1 cold, r2 cold, r3 ms30, r4 cold |
| drvec q=0 at r=2, best route = cold | (imposed) | | | (-0.744, 0.598, 4.002, 1.000, 0.000); (0.032, -0.108, -0.778, 0.000, 1.000) | (0.036, 0.033, -0.022, -0.029, -0.000); (0.144, 0.089, 0.336, -0.433, -0.210) | (-4.740, -0.318) | 928.553 exact | yes (tc1 gradient) |
| drvec q=1 (default MA class) at r=2 | (imposed) | | | (-0.544, 0.239, 2.298, 1.000, 0.000); (0.216, -0.442, -0.874, 0.000, 1.000) | (0.049, 0.041, -0.015, 0.088, 0.135); (0.029, 0.040, 0.321, -0.964, -0.511) | (-5.672, -1.027) | 962.039 exact | NO (tc3); min MA root 1.0000 |
# drvec rank-by-rank optimum audit (q = 0) — generated by analyze.py

Columns: exact logL and optimizer termcode (tc1 gradient, tc2 steptol, tc3 'no lower point', tc4 iteration limit). `johfix` = exact logL profiled at Johansen's beta (-fixb2 0 on data with Y1 replaced by Johansen's W): a LOWER BOUND for the free optimum. `J maxroot` = largest root modulus of Johansen's fitted levels VAR after removing the M-r unit roots (>1 = explosive, outside the exact likelihood's domain).

## denmark (M=4, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 664.495 tc1 | | | | | | 664.495 | 653.793 | - |
| 1 | 675.660 tc1 | 675.660 tc1 | 675.660 tc1 | 675.660 (best start 2; tcs 11111133111131113333111111311) | 675.497 tc3 | 675.497 (best start 1; tcs 3111113113) | 675.660 (cold) | 668.802 | 0.6639 |
| 2 | 681.239 tc1 | 681.239 tc1 | 681.239 tc1 | 681.239 (best start 2; tcs 1111111331111313111111311111) | 681.036 tc1 | 681.036 (best start 1; tcs 111111133111) | 681.239 (cold) | 673.983 | 0.7219 |
| 3 | 688.334 tc1 | 688.334 tc1 | 688.334 tc1 | 688.334 (best start 1; tcs 1111311111131131311111311) | 688.276 tc1 | 688.276 (best start 1; tcs 111111) | 688.334 (cold) | 677.148 | 0.7724 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 4 | 22.33 | 22.33 | 30.02 | 28.14 |
| 1 | 3 | 11.16 | 11.16 | 10.36 | 22.00 |
| 2 | 2 | 14.19 | 14.19 | 6.33 | 15.67 |
| 3 | 1 | not expressible (r=M) | - | 2.31 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=0; drvec best-found r=0; Johansen same data r=1

## denmark5 (M=5, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 818.100 tc1 | | | | | | 818.100 | 807.195 | - |
| 1 | 824.104 tc3 | 824.104 tc3 | 847.653 tc1 | 847.653 (best start 29; tcs 3333131311331331313113) | 811.218 tc3 | 811.218 (best start 1; tcs 311113333) | 847.653 (seedjoh) | 838.943 | 0.9813 |
| 2 | 828.845 tc3 | 828.845 tc3 | 832.355 tc3 | 861.614 (best start 2; tcs 343333333333233333) | 862.111 tc1 **>free** | 862.111 (best start 1; tcs 1333333333333) | 862.111 (johfix) | 855.159 | 0.9823 |
| 3 | 870.070 tc2 | 870.070 tc2 | 870.191 tc1 | 870.191 (best start 2; tcs 211131311113311111) | 856.320 tc3 | 856.320 (best start 1; tcs 33113111111) | 870.191 (seedjoh) | 863.928 | 0.9810 |
| 4 | 879.432 tc3 | 879.432 tc3 | 879.432 tc3 | 879.432 (best start 1; tcs 3411411114111111) | 879.291 tc3 | 879.291 (best start 1; tcs 341113111113) | 879.432 (seedjoh) | 868.052 | 0.9820 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 5 | 12.01 | 59.11 | 63.50 | 34.40 |
| 1 | 4 | 9.48 | 28.92 | 32.43 | 28.14 |
| 2 | 3 | 82.45 | 16.16 | 17.54 | 22.00 |
| 3 | 2 | 18.72 | 18.48 | 8.25 | 15.67 |
| 4 | 1 | not expressible (r=M) | - | 1.97 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=0; drvec best-found r=2; Johansen same data r=2

## finland (M=4, p=2, case 3)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 899.074 tc1 | | | | | | 899.074 | 891.806 | - |
| 1 | 916.390 tc1 | 916.390 tc1 | 916.390 tc1 | 916.390 (best start 4; tcs 111131111434111411113111111111) | 916.212 tc3 | 916.212 (best start 3; tcs 3133) | 916.390 (cold) | 911.045 | 0.6109 |
| 2 | 930.260 tc1 | 930.260 tc1 | 930.260 tc1 | 930.260 (best start 2; tcs 11111113133111111111311111) | 930.155 tc1 | 930.155 (best start 2; tcs 111133) | 930.260 (cold) | 924.345 | 0.6335 |
| 3 | 936.227 tc1 | 936.227 tc1 | 936.227 tc1 | 936.227 (best start 22; tcs 111111344113331111111111114143) | 936.126 tc1 | 936.126 (best start 4; tcs 1111111111111111311) | 936.227 (cold) | 928.292 | 0.8298 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 4 | 34.63 | 34.63 | 38.48 | 27.14 |
| 1 | 3 | 27.74 | 27.74 | 26.60 | 21.07 |
| 2 | 2 | 11.93 | 11.93 | 7.89 | 14.90 |
| 3 | 1 | not expressible (r=M) | - | 3.15 | 8.18 |

rank (sequential lambda-max, 5%): drvec -lrtest r=2; drvec best-found r=2; Johansen same data r=2

## finland_const (M=4, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 833.231 tc1 | | | | | | 833.231 | 827.081 | - |
| 1 | 853.367 tc1 | 853.367 tc1 | 853.367 tc1 | 853.367 (best start 2; tcs 1111111111111111111111311111) | 853.232 tc1 | 853.232 (best start 2; tcs 133131) | 853.367 (seedjoh) | 849.138 | 0.5950 |
| 2 | 869.487 tc3 | 869.487 tc3 | 870.161 tc1 | 870.161 (best start 3; tcs 331333333141333333333333343) | 870.040 tc1 | 870.040 (best start 1; tcs 111134) | 870.161 (seedjoh) | 865.522 | 0.7549 |
| 3 | 877.036 tc1 | 877.036 tc1 | 877.036 tc1 | 877.036 (best start 24; tcs 11111411134113111131111131111) | 874.404 tc1 | 874.404 (best start 1; tcs 11131111114311111331) | 877.036 (cold) | 871.095 | 0.9856 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 4 | 40.27 | 40.27 | 44.11 | 28.14 |
| 1 | 3 | 32.24 | 33.59 | 32.77 | 22.00 |
| 2 | 2 | 15.10 | 13.75 | 11.15 | 15.67 |
| 3 | 1 | not expressible (r=M) | - | 7.76 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=2; drvec best-found r=2; Johansen same data r=2

## UKconinc (M=2, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 488.761 tc1 | | | | | | 488.761 | 486.786 | - |
| 1 | 523.734 tc1 | 523.734 tc1 | 523.734 tc3 | 523.734 (best start 1; tcs 1111111111111111111111) | 523.722 tc1 | 523.722 (best start 1; tcs 11111111111111111111) | 523.734 (cold) | 521.653 | 0.4558 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 2 | 69.94 | 69.94 | 69.73 | 15.67 |
| 1 | 1 | not expressible (r=M) | - | 16.06 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=1+ (r=M untestable); drvec best-found r=1+ (r=M untestable); Johansen same data r=2

## UKconinc_seas (M=2, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 608.907 tc1 | | | | | | 608.907 | 605.087 | - |
| 1 | 631.347 tc1 | 631.347 tc1 | 631.347 tc1 | 631.347 (best start 1; tcs 111111111111111311111111111) | 631.337 tc1 | 631.337 (best start 1; tcs 11111111111111111111) | 631.347 (cold) | 627.787 | 0.6995 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 2 | 44.88 | 44.88 | 45.40 | 15.67 |
| 1 | 1 | not expressible (r=M) | - | 17.69 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=1+ (r=M untestable); drvec best-found r=1+ (r=M untestable); Johansen same data r=2

## UKconsumption (M=3, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 522.409 tc1 | | | | | | 522.409 | 516.713 | - |
| 1 | 535.170 tc3 | 535.170 tc3 | 557.473 tc3 | 557.473 (best start 12; tcs 31313313313331323123333) | 514.185 tc3 | 514.185 (best start 1; tcs 33) | 557.473 (ms30) | 555.164 | 0.8726 |
| 2 | 570.230 tc3 | 570.230 tc3 | FAIL(AR operator non-stationary) | 573.960 (best start 23; tcs 31331311133111111) | 560.102 tc3 | 569.333 (best start 3; tcs 3311) | 573.960 (ms30) | 577.528 | 1.0372 (explosive) |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 3 | 25.52 | 70.13 | 76.90 | 22.00 |
| 1 | 2 | 70.12 | 32.98 | 44.73 | 15.67 |
| 2 | 1 | not expressible (r=M) | - | 5.34 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=2+ (r=M untestable); drvec best-found r=2+ (r=M untestable); Johansen same data r=2

## UKconsumption_lev (M=3, p=2, case 2)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | -1168.020 tc3 | | | | | | -1168.020 | -1151.803 | - |
| 1 | -1153.579 tc3 | -1153.579 tc3 | -1139.857 tc2 | -1142.734 (best start 5; tcs 3332333333333333) | -1137.194 tc3 **>free** | -1137.194 (best start 2; tcs 32) | -1137.194 (johfix_ms) | -1118.018 | 0.9151 |
| 2 | -1148.033 tc3 | -1148.033 tc3 | FAIL(AR operator non-stationary) | -1135.598 (best start 22; tcs 333333333333) | -1152.091 tc3 | -1151.942 (best start 6; tcs 3333) | -1135.598 (ms30) | -1101.457 | 1.0616 (explosive) |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 3 | 28.88 | 61.65 | 67.57 | 22.00 |
| 1 | 2 | 11.09 | 3.19 | 33.12 | 15.67 |
| 2 | 1 | not expressible (r=M) | - | 5.61 | 9.24 |

rank (sequential lambda-max, 5%): drvec -lrtest r=1; drvec best-found r=1; Johansen same data r=2

## Canada (M=4, p=3, case 3)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | -191.930 tc1 | | | | | | -191.930 | -186.088 | - |
| 1 | -179.796 tc1 | -179.796 tc1 | -178.618 tc1 | -179.796 (best start 2; tcs 11111113111111111113) | FAIL(AR operator non-stationary) | -438.354 (best start 4; tcs 111) | -178.618 (seedjoh) | -164.179 | 0.8910 |
| 2 | -176.285 tc3 | -176.285 tc3 | -193.289 tc3 | -170.924 (best start 2; tcs 3131333331131333) | FAIL(AR operator non-stationary) | FAIL(AR operator non-stationary) | -170.924 (ms30) | -156.000 | 0.8484 |
| 3 | -164.169 tc1 | -164.169 tc1 | -164.169 tc1 | -164.169 (best start 1; tcs 111111131113131131) | -164.389 tc1 | -164.389 (best start 1; tcs 1113333131) | -164.169 (cold) | -150.639 | 0.9347 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 4 | 24.27 | 26.62 | 43.82 | 27.14 |
| 1 | 3 | 7.02 | 15.39 | 16.36 | 21.07 |
| 2 | 2 | 24.23 | 13.51 | 10.72 | 14.90 |
| 3 | 1 | not expressible (r=M) | - | 0.06 | 8.18 |

rank (sequential lambda-max, 5%): drvec -lrtest r=0; drvec best-found r=0; Johansen same data r=1

## UKpppuip (M=5, p=2, case 3)

| r | -lrtest logL | cold | -seedjoh | -multistart 30 | johfix | johfix+ms20 | best | J cond. logL | J maxroot |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 911.223 tc1 | | | | | | 911.223 | 895.185 | - |
| 1 | 922.457 tc1 | 922.457 tc1 | 903.703 tc3 | 922.457 (best start 3; tcs 111133111311331111) | 921.847 tc1 | 921.847 (best start 4; tcs 1111111131) | 922.457 (cold) | 906.857 | 0.7601 |
| 2 | 928.553 tc1 | 928.553 tc1 | 928.553 tc1 | 928.553 (best start 22; tcs 111311111113313313) | 922.329 tc1 | 922.329 (best start 2; tcs 114331) | 928.553 (cold) | 917.257 | 0.7839 |
| 3 | 923.864 tc3 | 923.864 tc3 | 930.515 tc1 | 933.265 (best start 23; tcs 333341111111133113) | 918.849 tc3 | 918.849 (best start 1; tcs 333311313131) | 933.265 (ms30) | 924.065 | 0.9690 |
| 4 | 934.668 tc1 | 934.668 tc1 | 934.668 tc1 | 934.668 (best start 1; tcs 111111111111144411111) | 928.583 tc1 | 928.583 (best start 1; tcs 11311111111) | 934.668 (cold) | 929.166 | 0.9782 |

| r0 | M-r | LR drvec -lrtest | LR drvec best-found | Johansen lambda-max (same data) | 5% cv |
|---|---|---|---|---|---|
| 0 | 5 | 22.47 | 22.47 | 23.34 | 33.32 |
| 1 | 4 | 12.19 | 12.19 | 20.80 | 27.14 |
| 2 | 3 | -9.38 | 9.42 | 13.61 | 21.07 |
| 3 | 2 | 21.61 | 2.80 | 10.20 | 14.90 |
| 4 | 1 | not expressible (r=M) | - | 4.82 | 8.18 |

rank (sequential lambda-max, 5%): drvec -lrtest r=0; drvec best-found r=0; Johansen same data r=0
