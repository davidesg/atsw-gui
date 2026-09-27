# drvec 0.10 external validation: Lütkepohl e1/e3 and Rao (1994) tables 1–7

Working dir: this folder. Repository untouched (read-only).
Detail files: `results_ranks.md` (every rank, every route), `results_auto.md` (β/α/Π per fit),
`identity_check.md`, `initial_term.md`, `*.out` (drvec), `johansen_*.json`, `exact_*_r*.json`.

## Specification and mapping

| reference (ca.jo) | drvec |
|---|---|
| K = 2 (VAR order in levels; 1 lagged difference; statsmodels `k_ar_diff=1`) | `p = 2` (AR order on Ȳ; `p = k_ar_diff + 1`, as in docs/COMPARISON_JOHANSEN.md) |
| `ecdet="const"` (constant restricted to the cointegrating space) | `-case 2` (E[∇Y₂]=0, E[W] free); drvec E[W] = −(Johansen constant) |
| trace + λmax, 5 % | `-lrtest` gives only the λmax form 2[L(r+1)−L(r)], r = 0…M−2 (r = M cannot be expressed) |
| β normalised on the first variable | β = [B₂; I_r] normalised on the LAST r columns (Y₁). Johansen β is renormalised on the same block before comparing |
| no seasonal dummies (e1 and e3 are seasonally adjusted, per the file headers) | none needed |
| MA | q = 0 (Johansen's model). q = 1 (default class, `marow`) as a secondary column |

Column order (Y₁ last): e1 `linvest lincome lcons` (Y₁ = cons); e3 `lgnp rd rb lM1` (Y₁ = M1);
Rao: CSV columns rotated so the first CSV column is in Y₁. Rao NA rows dropped listwise, as ca.jo does
(Raotbl1 row 1, Raotbl3 row 1, Raotbl4 rows 1–30, so rao4 = 1900–1986, n = 87).

Three Johansen implementations agree to 1e-9: ca.jo (benchmark files), statsmodels `VECM(deterministic="ci")`,
and an independent numpy reduced-rank regression (`johansen_ref.py`). **statsmodels
`coint_johansen(det_order=0)` is the UNRESTRICTED constant** (= ca.jo `ecdet="none"`; e1 trace 32.68, not 75.00).
An independent exact-ML implementation (`exact_ml.py`, a Kalman filter on z_t = (ΔY_t, W_t−μ), which shares
no code with drvec) was used both to evaluate drvec's reported points and to maximise the likelihood from Johansen's point.

## Summary per case (q = 0 unless noted; logL: drvec exact on N = n−1, Johansen conditional on T = n−2, not comparable in level)

### e1 — Lütkepohl, West German invest/income/cons, logs, 1960Q1–1982Q4 (n = 92)
| source | rank | stats | β (cons = 1) | α | logL | converged |
|---|---|---|---|---|---|---|
| published (README, book Ch.7) | 1 (not verifiable here: book not in `literature/`) | — | — | — | — | — |
| ca.jo / Johansen | 1 | trace 75.00, 15.45, 6.08; λmax 59.55, 9.37, 6.08 | (−0.035, −0.928, 1), E[W] 0.221 | (0.047, −0.226, −0.294) | 743.737 (cond.) | closed form |
| drvec -lrtest | 1 | LR 58.65, 6.02 | | | 721.199 / 750.525 / 753.535 | grad / grad / tc3 (= exact max, verified) |
| drvec r=1 | | | (−0.028, −0.933, 1), E[W] 0.222 | (0.042, −0.233, −0.306) | 750.525 | grad; identical with multistart and -seedjoh |
| independent exact ML | 1 | 58.65, 6.02 | (−0.028, −0.933, 1) | (0.042, −0.233, −0.306) | 750.525 | — |
| drvec q=1 (marow) | | | (0.040, −0.995, 1) | (−0.008, −0.214, −0.046) | 754.873 | grad |

**Verdict: reproduces.** Rank identical; β within 0.007, α within 0.012; drvec's optimum matched to 1e-4 by the independent exact ML at every rank.

### e3 — Lütkepohl, US log M1, log GNP, rd, rb, 1954Q1–1987Q4 (n = 136)
| source | rank | stats | β (M1 = 1) | α | logL | converged |
|---|---|---|---|---|---|---|
| published (README) | 1 (not verifiable here) | — | — | — | — | — |
| ca.jo / Johansen | 1 (trace 34.30 vs 34.91 at r≤1: borderline) | trace 91.01, 34.30, 14.75, 2.52; λmax 56.71, 19.55, 12.23, 2.52 | (gnp −0.500, rd 11.62, rb −6.09, 1), E[W] 2.765 | (−0.0375, 0.0012, 0.0014, −0.0105) | 1979.938 | — |
| drvec -lrtest | 1 | LR 47.26, 18.62, 7.98 | | | | grad ×4 (r=3 tc3 with -seedjoh) |
| drvec r=1 | | | (−0.530, 8.858, −3.525, 1), E[W] 2.495 | (−0.0489, 0.0008, 0.0014, −0.0137) | 1990.760 | grad; same with multistart, -seedjoh |
| independent exact ML | 1 | 47.26, 18.62, 8.51 | (−0.530, 8.858, −3.525, 1) | same | 1990.760 | — |
| drvec q=1 | | | (−0.541, 7.73, −2.34, 1) | | 1997.841 | **tc3, not converged** |

**Verdict: reproduces with an explained difference.** Rank identical. The interest-rate coefficients differ
(rd 8.86 vs 11.62, rb −3.53 vs −6.09); the independent exact ML lands on drvec's β exactly, and the exact
likelihood at Johansen's point is only 0.31 below the optimum: a flat direction on which the
exact and conditional likelihoods pick different points. Not a defect. (Rescaled-data run: r=2 fit stopped
tc3 3.2 units low and flipped the 10 % verdict at r=2: see defects.)

### Rao tables — what the reference is
The README's Rao ranks come from `tools/extract_benchmarks.R`, which runs ca.jo on **every column of the urca
data frame, untransformed**. That is not the chapters' specification in several cases (verified from the urca
documentation; the printed chapters are not available):
Raotbl1 puts `k` and its seasonally adjusted copy `ksa` in one system; Raotbl3 puts three **impulse dummies in as
I(1) endogenous variables**; Raotbl4/5 are Perron's univariate trend-break data (no cointegration claim in that
chapter to my knowledge); Raotbl6/7 are in raw levels (GNP ~10³, deflators, M1 ~10⁴, GDP ~10⁵ next to p ~0.3).
The `n_obs` in the results files is the pre-NA count (Raotbl4 says 117; ca.jo used 87).
**None of the Rao ranks is a published result.** They are a same-specification Johansen reference, which is what drvec is checked against below.

| case | ca.jo trace rank (README) | Johansen λmax rank (5 %) | drvec λmax rank: best drvec fits / best known max | Johansen λmax | drvec LR (best known max) | β, α, Π at reference rank | convergence at reference rank | verdict |
|---|---|---|---|---|---|---|---|---|
| rao1 (M=5, r=2) | 2 | 2 | 1 / 1 | 50.32, 32.42, 10.77, 8.02 | 45.86, 23.17, 10.17, 6.36 | max\|ΔΠ\| 0.25; β differs (e.g. −2.86 vs −3.71) | tc3 (all 20 starts), but the independent exact ML confirms 1747.256 as the maximum | reproduces the λmax sequence with explained differences; rank at 5 % 1 vs 2 (LR 23.2 vs 32.4, cv 28.14) |
| rao2 (M=4, r=3) | 3 | ≥3 | 2 / 2 | 49.43, 32.77, 23.29 | 31.33, 30.63, 14.03 | max\|ΔΠ\| 0.035–0.056; β entries not comparable (Y₁ block badly conditioned) | r=3: tc3 (2340.12) / steptol (2341.19, -seedjoh); **not certified** | disagrees on rank (2 vs 3), difference explained by the exact likelihood's initial-value term (below) |
| rao3 (M=6, r=3), dummies as I(1) | 3 | 3 | 3 / 3 (unverified) | 94.06, 85.31, 44.21, 15.40, 6.12 | 86.62, 81.89, 48.89, 20.78, −2.01 | Δβ up to 45 (dummy coefficients), ΔΠ 0.54 | r=3 grad; r=1,2 tc3; LR(4) < 0 (flagged by drvec) | spec not meaningful; rank agrees but the sequence contains non-converged fits |
| rao3_3v (lc, li, lw; closest to Holden–Perman, dummies dropped) | — (trace 47.12, 12.73, 2.32 → 1) | 1 | 1 / 1 | 34.39, 10.42 | 33.59, 4.85 | β (li −0.945, lw −0.054, lc 1) vs (−0.953, −0.050, 1); α within 0.04 | grad, verified by exact ML | **reproduces** |
| rao4 (M=6, r=2) | 2 | 1 | 1 / 1 | 72.64, 31.64, 19.71, 19.13, 8.19 | 71.76, 28.83, 17.14, 22.07, 7.48 | max\|ΔΠ\| 0.08, β within 0.3 | r=2 tc3, 0.03 below the exact max; **r=1 and r=3: drvec 2.8 and 1.6 below the exact max even with -seedjoh; default -lrtest r=1 was 16 below (8 iterations)** | reproduces (on λmax) only after the fits are repaired; default `-lrtest` output wrong |
| rao5 (M=5, r=2) | 2 | 2 | 1 / 1 | 72.71, 28.61, 11.75, 10.55 | 68.41, 26.03, 9.98, 8.10 | max\|ΔΠ\| 0.025, α within 0.014 | **default r=2 fit tc3 at 1093.92, 22.5 below the optimum 1116.43** (reached by multistart, -seedjoh, and the exact ML) | reproduces with explained differences once converged (λmax rank 1 vs 2: 26.0 vs 28.6, cv 28.14); default `-lrtest` printed LR = −18.98 and 54.99 |
| rao6 (M=8, r=5) | 5 | — | **no fit** | 85.96, 71.72, 50.48, 43.61, 26.65, … | — | — | ifault=3 "AR operator non-stationary" at the starting point, every rank, case 2 and case 3, raw and rescaled; multistart r=5 tc3 at −1147.97 (junk) | **cannot express / cannot fit** |
| rao7 raw levels (M=4, r=2) | 2 | 2 | 0 (garbage) | 83.72, 48.90, 11.53 | 7.26, 61.28, 0.21 | meaningless | every rank stops at iteration 1 (tc3), exit 0, "ok (ifault=0)" | **defect (scaling)** |
| rao7 rescaled (each column / sd of its difference) | 2 | 2 | 2 / 2 | 83.72, 48.90, 11.53 | 80.09, 42.76, 8.17 | max\|ΔΠ\| 0.017 (multistart) | r=0,1 grad; r=2 tc3 (best −526.12 by multistart; -seedjoh −535.44) | reproduces with explained differences |

The secondary q = 1 fits (default `marow` class) converged on the gradient only for e1; for e3, rao1–5 and rao7_sc
they stop on tc3 or on the iteration limit, so their numbers are not reportable as optima.

## Why exact and conditional LRs differ (expected, quantified)

drvec's logL includes the stationary density of the first observation of Ȳ = (∇Y₂, W). Johansen's
conditional likelihood does not. When W is close to a unit root, that density is diffuse and costs likelihood,
and the cost changes with r (`initial_term.md`). For rao2 the first-observation term is +11.03 at r=1 and +3.99 at r=3.
That accounts for about 14 of the 11.4-point gap between Johansen's λmax(1)+λmax(2) = 56.1 and drvec's 44.7.
The largest eigenvalue of the state VAR is 0.9955. The same mechanism lowers the drvec LRs for rao1, rao5 and e3.
Every case where the ranks differ (rao1, rao2, rao5) is borderline at 5 % in one of the two statistics. None of these is a defect.

## Identities (all hold)

- **drvec's likelihood is correct.** At 13 reported drvec optima (e1, e3, rao1 ×2, rao2 ×3, rao3, rao3_3v, rao4, rao5 ×2, rao7_sc), an independent Kalman exact likelihood reproduces `logelf` to 1e-4 (`identity_check.md`).
- At r = 0 the independent maximum equals drvec to 1e-3 in every case run (e1, e3, rao1, rao2, rao4, rao5, rao7_sc, rao3_3v).
- LR invariance to rescaling holds when both fits converge (rao2: LR(0), LR(1) identical raw/rescaled; e3: LR(0) identical).
- Pi = αβ′ printed is consistent with α and β, in the .inp order.

## Suspected drvec defects (separate from the methodology)

1. **Γ(k) is printed in the internal [Y₁;Y₂] order but labelled in the .inp order** (new; docs/BUGS.md claims Γ is in .inp order). This is the same disease as BUG-17/18.
   BUG-18 (Q/Σ, open) is confirmed on the same points. The parameter-table rows "D.x <- D.y(-1)", "var x", "cov x,y" and the per-variable short-run Wald blocks inherit the wrong names.
   Evidence: on 13/13 fits the printed Γ/Σ reproduce logelf only when read as [Y₁;Y₂]; with the printed labels the point is non-stationary or 300–29000 log-units off.
   Minimal repro: `drvec e1 2 0 1 -case 2` on e1.inp (`linvest lincome lcons`). Printed Γ row 1 (−0.132, 0.006, 0.085) is statsmodels' **lcons** equation (−0.156, 0.005, 0.099) in the order (cons, invest, income), not linvest's (−0.225, 0.532, 0.674).
   Printed Σ diag (0.000094, 0.001837, 0.000130) = (cons, invest, income); var(Δ log invest) = 0.00198.
2. **`-lrtest` silently ignores `-multistart`**. `run_lrtest` calls `est` once per rank; the rank tables are identical with and without the flag (rao4, rao1, rao2, e3, e1). `-seedjoh` is honoured.
   This goes against the "refused, not ignored" convention, and it matters because:
3. **The `-lrtest` verdict column does not look at convergence.** Rows built on tc3 or 1-iteration stops get "reject H0 at 1 %" like any other row. Only a negative LR is flagged.
   rao5: the r=2 fit stopped 22.5 below its optimum → "LR = 54.99, reject at 1 %" (true 9.98).
   rao4: r=1 stopped after 8 iterations → LR(0) 39.5, LR(1) 61.0 (true 71.8 / 28.8).
   rao7 raw: every fit stops at iteration 1 → "r=1: reject at 1 %".
   The optimizer banners are printed above the table, but they are not tied to r.
4. **Scale fragility**: rao7 in raw units (column sd of differences from 0.0074 to 2700) does not optimise at all, and still exits 0 with "ok (ifault=0)".
   Rescaling each column by a constant (which leaves the model and the LRs unchanged) gives clean gradient convergence at r=0 and r=1.
   Repro: `drvec rao7 2 0 1 -case 2 -lrtest` vs the same on `rao7_sc.inp`.
5. **No stationary fallback for the starting point**: rao6 fails at the start (ifault=3) at every rank, case 3 included.
   For case 3 at r=0 a stationary start exists (OLS VAR(1) with constant on ∇Y has max|eig| 0.966).
   For case 2 the no-drift start is genuinely non-stationary (max|eig| 1.004): the data have strong drift, which case 2 / `ecdet="const"` excludes. So the case 2 part is the specification, not drvec.
   Repro: `drvec rao6 2 0 1 -case 3 -lrtest`.
6. Minor: Σ is printed with `%.6f`. For log data (Σ ~1e-5) the printed Σ is not positive definite (rao2); `sigma2 × Q` has to be used instead.

Not drvec, but in the repository's tooling: `tools/compare_johansen.py` says `det_order=0` equals the restricted constant (case 2).
For `coint_johansen` that is wrong (it is the unrestricted constant), so its "rank chosen by Johansen" column uses a different deterministic case from drvec's `-case 2`. Its β comparison uses `VECM(deterministic="ci")`, which is correct.

## Scripts (re-runnable, in this order)
`build_inputs.py` → `run_drvec.sh` → `run_extra.sh` → `johansen_ref.py` (+ `johansen_ref.py rao7_sc rao3_3v rao6_sc`) → `run_exact.sh` / `run_exact2.sh` (slow, hours) → `identity_check.py`, `initial_term.py` → `compare.py`, `synth.py`. `cajo_lutk.R` reproduces the ca.jo numbers for e1/e3.
Not completed: the independent exact ML for rao3 (6-var) and for rao1 r=3,4, rao4 r=4,5, rao5 r=4 (stopped for time). Those drvec values are gradient-converged except rao3 r=1,2.
