# STUDY B — what `drv_project` (the BVECM code) actually estimates, and how it maps onto `drvec`'s MA classes

*2026-09-23. Read-only study of `~/Dropbox/SRC/drv_project` (legacy) against
`~/Dropbox/SRC/drvec` (binary `bin/drvec` 0.10, built 2026-08-24). No repository was
modified. Everything built or run lives under this directory:*

```
B/legacy/      verbatim copy of drv_project/{src,include,Makefile}, rebuilt   (bin/drv)
B/legacy_x/    same + instrumentation patch (scripts/legacy_x.patch)          (bin/drv)
B/data/        inputs: legacy 4-col .inp (AL PL SL VL, P_<pair>), drvec .inp (D<xx>, V_<pair>)
B/scripts/     runlib.py equiv.py evidence.py report.py summary.py omega.py bec_check.py
B/work/        every run (one directory per fit) + equiv_p{1,2}.txt, report_*.txt, summary.txt, evidence_*.json
```

Labels: **VERIFIED** (checked in code and/or numerically), **DISCREPANCY** (code, doc
or earlier study says something the evidence contradicts), **OPEN**.

Line numbers are those of `drv_project/src/main.c` (3393 lines, CRLF, mtime 2026-02-25 18:28).

---

## 0. Answers in one page

1. **Model.** A bivariate VARMA(p,1) on `Ȳ_leg = (w_t, ∇z2_t)` with **every AR matrix
   free** (4p parameters) and **a single MA coefficient, θ₂₂(1), on the ∇z₂ (= ∇Y₂,
   London) equation's own innovation**. β enters only through the data
   (`w = A − βL − ν(B)X`), μ is the mean of `w` only (Mauricio's case 2), Σ is the full
   2×2 (3 parameters, one of them unidentified). The MA restriction is **by construction**
   (hard-coded `global_q = 1`, no CLI switch, one MA slot in the parameter vector).
   In drvec's Ȳ order `(∇Y2, W)` the MA is `Θ*₁ = [[θ,0],[0,0]]`: **the ∇Y₂ row is the
   only nonzero row — the exact opposite of `-marow` (the default), which zeroes it.**
   It is neither (a) free, (b) ∇Y₂-rows-zero, nor (c) triangular.
2. **Correspondence.** All of drvec's MA classes are zero patterns on `Θ* = C̄ΘC̄⁻¹`
   (proved §2.2, verified numerically: 24/24 fits at p = 1 agree to <1e-6 in logL).
   The legacy MA class is `{a}` = only `Θ*_{∇Y2,∇Y2}`, which in VEC coordinates is
   `Θ = [[0, −B₂′θ],[0, θ]]`: a **one-parameter subclass of `-matri`**, disjoint from
   `-marow`/`-mawarma`/`-warma`. The legacy AR is also wider than drvec's at the same p
   (Ms = 2 extra coefficients). **No drvec class equals the legacy model.**
3. **analisis_BEC()** is algebraically right for the AR part (levels VAR reproduced to
   1e-16, Π exact), prints per-lag α(k) on `w_{t−k}` rather than Johansen's α, drops the
   MA and the convergence term from the printed BEC, and its optional structural form
   uses the wrong Σ and flips the sign of the contemporaneous coefficient.
4. **Evidence.** Whenever the ∇Y₂ equation has an AR lag, the ∇Y₂-row MA coefficient
   `a` is large (0.7–1.0) and highly significant (|t| 5–20); `-matri` beats `-marow` by
   LR 6–25 on 1 df in 37 of 40 such configurations, and the legacy one-parameter class
   beats the two-parameter `-marow` in 36 of 40. It disappears only in drvec's p = 1,
   where ∇Y₂ has no AR lag at all. **But** on the short 1700–1812 pairs `a` goes to the
   invertibility boundary (1.000) in most fits; on the legacy's 175-obs samples it is
   interior (0.72–0.91). The driver is a data feature: ∇London has a near-zero
   long-run variance (sample ACF −0.03, −0.23, −0.19, −0.08).
5. **Dropped/changed:** full AR on Ȳ (drvec drops the ∇Y₂ columns of Φ*_p); MA on the
   ∇Y₂ block instead of the W block; θ₂₂ seeded by MA(1) moment inversion; Σ seeded /100
   with the scale direction left free; gradtol 1e-7 (drvec 1e-5); the convergence operator;
   `beta_fijo` (= drvec `-fixb2 -1`). Same engine, same xitol, same invertibility gate.

---

## 1. Which model does `drv_project` estimate?

### 1.1 The code path (VERIFIED)

| what | where (main.c) |
|---|---|
| `global_p = 1` (CLI), `global_q = 1` **hard-coded, never read from argv** | 25–26, 918–1001 |
| npar = 3 (Σ) + **4p** (AR) + **1** (θ₂₂) + 2 (conv) + 1 (β) + 1 (μ) | 35–70 (MA: **49**) |
| m = 2 literal; p = global_p; q = global_q | 1564–1567 |
| Σ read as full triangle `qq1[1][1], qq1[2][1], qq1[2][2]` (no Cholesky, no scale fix) | 1655–1664, symmetrised 1835–1836 |
| **AR: all four φᵢⱼ(k) read from x for every k** | 1667–1673 |
| **MA: `theta1[1][2][2] = x[idx++]`; θ₁₁ = θ₁₂ = θ₂₁ = 0** | **1676–1681** |
| α_conv, φ_conv; β (or 1 if `beta_fijo`); μ | 1684–1702 |
| `mu1[1] = μ`, `mu1[2] = 0` (∇z₂ has zero mean) | 1700–1701 |
| convergence operator ν(B) = α/(1−φB) (`calcnu`, nlatools.c:1348), 25 lags, on column 4 | 1717–1735 |
| **data**: `w[t][1] = A_t − ν(B)X_t − β·L_t`, `w[t][2] = ∇L_t` | 1740–1748 |
| normalisation by Φ₀ = Θ₀ = I (identity: no-op) | 1753–1860 |
| header "VARMA(%d, 1) - MA diagonal fixed", "triangular Phillips (MA diagonal fixed)" | 1094, 1098 |
| xitol ±1e-3 (method 1/2); maxits 500, gradtol 1e-7, steptol 1e-7 | 1079–1083, 1109 |
| `est(&shootx, …)` then `analisis_BEC()` | 1117, 1222 |

Data columns (from `gui/drv_gui.c:180–243`, which writes every `gui/*.inp`):
`dL = 100·(log L_t − log L_{t−1})`, `A = 100·log A_t`, `L = 100·log L_t`,
`X = 1{t ≥ convergence start}` (a step). nobs = raw − 1 (the first level is lost to the difference).

### 1.2 The model as equations (VERIFIED)

Legacy order `Ȳ^leg_t = (w_t, ∇z_{2t})′`, `z₁ = A` (the partner market, drvec's `Y₁`),
`z₂ = L` (London, drvec's `Y₂`):

```
w_t      = z_{1t} − β z_{2t} − ν(B) X_t ,        ν(B) = α_c / (1 − φ_c B)   (omitted with no_conv)
[w_t − μ ; ∇z_{2t}] = Σ_{k=1..p} Φ_k [w_{t−k} − μ ; ∇z_{2,t−k}] + [e_{1t} ; e_{2t} − θ e_{2,t−1}]
Φ_k : 2×2, ALL FOUR ENTRIES FREE          e_t ~ N(0, Σ), Σ full 2×2
Θ₁  = [[0, 0], [0, θ]]                    (elf convention Θ(B) = I − Θ₁B)
```

* **AR**: free on Ȳ, including lags of ∇z₂ in both equations and lags of w in the ∇z₂ equation.
* **MA**: one coefficient, on the **∇Y₂ equation, own innovation**. The labels "MA diagonal"
  and "triangular Phillips" are misleading (**DISCREPANCY**, cosmetic): it is not diagonal
  (θ₁₁ ≡ 0) and "triangular" refers to the Ȳ transform, not to Θ.
* **β**: enters only through the data; φ, θ, Σ do not depend on it. (ESTUDIO §3.1: VERIFIED.)
* **μ**: mean of w only; E∇z₂ = 0 → drvec `-case 2`. (ESTUDIO §2.3: VERIFIED.)
* **Convergence**: a transfer-function term on a step inside the cointegrating relation.
* **Restriction imposed by construction**, not by option: there is no way to ask the legacy
  binary for another MA structure or for q ≠ 1. The older versions in `src/olds/` show the
  history: Jan-05/06 (`main.c.AL1/AL2`) θ₂₂ only; Jan-21 (`main.c.AL3.c`) diagonal θ₁₁, θ₂₂
  (whose run `bin/AL.3.out` gave θ₁₁ = 0.942, θ₂₂ = 0.997); from Jan-22 on, θ₂₂ only again.

**In drvec's notation** (Ȳ = (∇Y₂, W), `B₂ = −β`, VEC over Y = (Y₁; Y₂)):

```
Φ*_k = S Φ_k S  (S = swap)  all free            Θ*₁ = [[θ, 0], [0, 0]]
levels:  a VAR(p+1) / VEC with F_1..F_{p−1} free and F_p = [0 | f_p]   (only the Y₂ column)
MA:      Θ₁ = C̄⁻¹ Θ*₁ C̄ = [[0, −B₂′θ], [0, θ]] = [[0, βθ], [0, θ]]
```

Proof of the levels form: with `Ȳ_t = K₀z_t + K₁z_{t−1}`, `K₀ = [[1,−β],[0,1]]`,
`K₁ = [[0,0],[0,−1]]`, the levels VAR is `A₁ = K₀⁻¹(φ₁K₀ − K₁)`,
`A_j = K₀⁻¹(φ_jK₀ + φ_{j−1}K₁)`, `A_{p+1} = K₀⁻¹φ_pK₁`; so
`F_p = −A_{p+1} = K₀⁻¹φ_p·diag(0,1)` has a zero Y₁ column. Parameter count
4p = Λ(2) + F₁…F_{p−1}(4(p−1)) + F_p(2). (Numerically checked in `bec_check.py`, §3.)

### 1.3 Reproduction (VERIFIED)

The shipped binary `bin/drv` (= `gui/drv`, byte-identical) and a rebuild of the current
source both reproduce the four `gui/*.out` exactly:

| file | command | logelf (shipped .out) | rerun |
|---|---|---|---|
| AL | `drv AL 2 beta_fijo 1` | −1488.4741510977 | −1488.4741510977 |
| PL | `drv PL 2 beta_fijo 1` | −1459.6081266567 | −1459.6081266567 |
| SL | `drv SL 2 1` | −1480.6918325707 | −1480.6918325707 |
| VL | `drv VL 4 1` | −1536.2347471311 | −1536.2347471311 |

The `bin/AL.*` runs use a 17-column `.inp` and come from older `main.c` versions (with
dummies); they cannot be rerun with the current source. Their MA is also θ₂₂ (0.71–1.00005;
`AL.2.beta1.out` sits at θ₂₂ = 1.00005, i.e. on/over the boundary). **OPEN**: exact
provenance of each `bin/` output.

### 1.4 The engine (VERIFIED)

`elfvarma.c` and `drvmlest.c` are identical to drvec's apart from the GPL header;
`qnewtopt.c` differs only in reporting (`quiet_mode`, the termination message);
`nlatools.c` differs in eigen/SVD back-ends (NR vs GSL) and in drvec adding headers.
Same call: `est(cast, …, xitol, …)`, objective `(f1/f1₀)^m (f2/f2₀)` with sigma2 = 1,
a cast fault or an `elf` fault returns 1.0 (rejection). The legacy `shootx` never raises a
fault itself: no bounds on β, φ_conv or θ beyond `elf`'s stationarity/invertibility checks.

---

## 2. Mapping onto drvec's classes

### 2.1 AR part (VERIFIED algebraically and numerically)

drvec's `p` is the AR order on Ȳ (MODEL.md §5.2). From MODEL.md (eq. 15–16),
`Φ*_1 = H̄ − C̄Λ̄ + C̄F₁C̄⁻¹`, …, `Φ*_p = −C̄F_{p−1}C̄⁻¹H̄`; since `C̄⁻¹H̄ = [0 I_r; 0 0]`
kills the ∇Y₂ columns, **drvec(p) = {Φ*_1..Φ*_p free except the ∇Y₂ columns of Φ*_p = 0}**
(count M²p − Ms = Mr + (p−1)M²; the map (Λ, F) ↔ Φ* is bijective). Hence

```
drvec(p)  ⊂  legacy(p)  ⊂  drvec(p+1)          (each inclusion costs Ms = 2 parameters at M = 2)
legacy(p) = drvec(p+1) with the Y₁ column of F_p zero  =  drvec(p) plus ∇Y₂_{t−p} in both equations
```

`-warma` is `Φ*_k = [0 Ψ_k; 0 Φ_k]` for **every** k (no ∇Y₂ lag anywhere) with MA only
in the W block. **DISCREPANCY**: drvec.c's comment at `global_warma` ("That is what the
legacy shootx does", drvec.c:333) is true only of *how B₂ enters* (through the data);
the legacy AR and MA patterns are the opposite of `-warma`'s (legacy: ∇Y₂ lags everywhere,
MA only on ∇Y₂; `-warma`: no ∇Y₂ lags, MA only on W).

### 2.2 MA part — all classes are zero patterns on Θ* (VERIFIED)

`Θ*_k = C̄Θ_kC̄⁻¹` with `C̄ = [[0, I_s],[I_r, B₂′]]`, `C̄⁻¹ = [[−B₂′, I_r],[I_s, 0]]`.
Writing `Θ = [[T₁₁, T₁₂],[T₂₁, T₂₂]]` over (Y₁, Y₂):

```
Θ* = [[ T₂₂ − T₂₁B₂′ ,  T₂₁           ],          rows/cols: (∇Y₂, W)
      [ T₁₂ + B₂′T₂₂ − (T₁₁ + B₂′T₂₁)B₂′ ,  T₁₁ + B₂′T₂₁ ]]
   =: [[a, b], [c, d]]
```

| class | VEC restriction | Θ* pattern (M = 2, r = 1) | free MA |
|---|---|---|---|
| `-mafree` | none | a b c d | 4 |
| `-matri` | T₂₁ = 0 | **b = 0** | 3 (a c d) |
| `-marow` (default) | T₂₁ = T₂₂ = 0 | **a = b = 0** | 2 (c d) |
| `-mawarma` | T₂₁ = T₂₂ = 0, T₁₂ = T₁₁B₂′ | only d | 1 |
| `-warma` | (+ AR: Φ* ∇Y₂ columns 0 at every lag) | only d | 1 |
| **legacy drv** | T₁₁ = T₂₁ = 0, T₁₂ = −B₂′T₂₂ | **only a** | 1 |

So the legacy class ⊂ `-matri` ⊂ `-mafree`, and legacy ∩ `-marow` = {Θ = 0}.
The patterns do not involve B₂, which is why, in Ȳ coordinates, every class is a plain
zero pattern — the structural fact behind ESTUDIO §3.1's "better conditioning" hypothesis.

### 2.3 Numerical proof of the correspondence (VERIFIED)

`legacy_x` = the legacy code with env-selectable zero patterns on Φ_k and Θ₁
(`DRV_AR ∈ {full, vec, warma}`, `DRV_MA ∈ {leg, none, ww, diag, wrow, tri, free}`;
patch: `scripts/legacy_x.patch`; default reproduces AL.out exactly). drvec run with
`-case 2 -differenced` on the same 175 rows (`data/D*.inp`: col 1 = ∇L, col 2 = A).
With the legacy's convergence operator off (`no_conv`) and β free.

**p = 1 (`work/equiv_p1.txt`): 24 of 24 agree to < 1e-6** (the residual is the xitol
truncation and LOG2PI rounding), both `grad`-converged:

```
AL free −1504.512542/−1504.512542  matri −1506.753844/−1506.753844  marow −1506.763230/−1506.763230
   mawarma −1507.544356/−1507.544356  q0 −1511.307996/−1511.307997  warma −1507.544356/−1507.544356
PL, SL, VL: the same, |diff| ≤ 9e-7 in all 18
```

**p = 2 (`work/equiv_p2.txt`)**: exact agreement (≤1e-6) whenever both land on the same
optimum — free (AL, VL), matri (SL), marow (PL), mawarma (PL, SL), q0 (all four), warma
(AL); the other 10 differ because one program stopped on another local optimum
(termcodes 2/3); the winner is sometimes legacy_x, sometimes drvec. npar(legacy) =
npar(drvec) + 1 always: the legacy's unidentified Σ scale.
Cross-check on drvec's own bank (levels layout, Milan p = 2, q = 1): this study's best
logLs marow 82.6558, matri 91.7611, free 93.2880, mawarma 81.3133 reproduce the `-specs`
table in drvec's USAGE.md (82.6558 / 91.7596 / 93.2880 / 81.3133).

### 2.4 Correspondence table

| legacy | drvec | relation |
|---|---|---|
| legacy AR (full Φ on Ȳ, order p) | drvec p (Φ*_p ∇Y₂ cols = 0) | legacy(p) ⊃ drvec(p) by Ms; ⊂ drvec(p+1) |
| legacy MA θ₂₂ (Θ* = only a) | none | ⊂ `-matri`; ∩ `-marow` = {0}; not expressible in drvec |
| legacy_x `vec+free` | `-mafree` | identical (logL to 1e-7) |
| legacy_x `vec+tri` | `-matri` | identical |
| legacy_x `vec+wrow` | `-marow` | identical |
| legacy_x `vec+ww` | `-mawarma` | identical |
| legacy_x `warma+ww` | `-warma` | identical (= `-mawarma` at p = 1, as USAGE says) |
| legacy_x `vec+none` | q = 0 | identical |
| μ on w only | `-case 2` | identical |
| `beta_fijo` (β = 1) | `-fixb2 -1` | identical restriction (B₂ = −β) |
| ν(B)X convergence | — | absent in drvec |
| Σ full (3) | Σ with Q₁₁ = 1 (2) | same likelihood; legacy carries a flat direction |

---

## 3. `analisis_BEC()` (main.c 455–858)

What it computes (from the Ȳ-VAR, ignoring MA, μ and ν):
`Δz_t = Σ_k α(k) w_{t−k} + Σ_k Γ(k) Δz_{2,t−k} + ε_t` with
`α₁(k) = φ₁₁(k) − δ_{k1} + βφ₂₁(k)`, `α₂(k) = φ₂₁(k)`, `γ₁₂(k) = φ₁₂(k) + βφ₂₂(k)`,
`γ₂₂(k) = φ₂₂(k)` (559–566), `Π = Σ_k α(k)(1, −β)` (583–589), delta-method SEs
(2773–2882), Wald "no EC" (all α(k) = 0, 2p df) and "weak exogeneity" (α_i(k) = 0 for all
k, p df) with SVD pseudo-inverse (3076–3328), convergence g = α/(1−φ), τ = μ + g,
l = 1/(1−φ) (3331–3393), and with `struct` a recursive structural form (735–835).

Checked on the SL fit (`scripts/bec_check.py`, `work/bec/SL.out`):

* **VERIFIED** — the levels VAR(p+1) implied directly by the Ȳ-VAR and the one implied by
  the printed BEC coincide to **1.1e-16**; Π coincides (−0.385564, 0.282773; 0.025036,
  −0.018361). The inversion is a correct, one-shot map Ȳ → levels (ESTUDIO's central
  thesis VERIFIED: estimate on Ȳ, invert once at the end).
* **DISCREPANCY (labelling)** — the header says "Johansen VEC", but the printed form is in
  lags of w, not Johansen's `αw_{t−1} + ΣΓΔz`: Johansen's α is Σ_k α(k)
  (SL: −0.3856, 0.0250), which is never printed; the "weak exogeneity" test (p df, all lags)
  is stricter than Johansen's (1 df on Σ_k α_i(k)).
* **DISCREPANCY (omission)** — the MA is not carried into the BEC: the correct error is
  `ε_t − Θ₁ε_{t−1}` with `Θ₁ = [[0, βθ],[0, θ]]`; the printed form says "+ epsilon_t".
  The convergence term Δν(B)X also leaves the Δz₁ equation unprinted.
* **DISCREPANCY (bug, `struct` only)** — the structural form Cholesky-factors
  `Σ_e = σ²Q` (innovations of (w, ∇z₂)), but the equations shown are for (Δz₁, Δz₂), whose
  innovations are `ε = K₀⁻¹e` with `Σ_ε = K₀⁻¹Σ_eK₀⁻ᵀ`; and it prints
  `Δz₂ = Pinv[2][1]·Δz₁` with `Pinv[2][1] = −P₂₁` (799, 819), whereas moving the term to
  the right-hand side gives `+P₂₁`. On SL: printed +0.4665; from its own Σ it would be
  −0.4665; the correct value (from Σ_ε) is **+0.3339**, and d₁, d₂ are 265.9/285.5, not
  305.2/248.8.
* **DISCREPANCY (minor)** — "mean lag" `l = 1/(1−φ)` (3389); the mean lag of
  α/(1−φB) is φ/(1−φ) = l − 1. Pre-estimation truncates ν at K = 20 lags (136), shootx at 25 (1717).

---

## 4. Empirical evidence on the MA structure

Scripts: `evidence.py` (for each data set × AR mode × MA class: 7 starts of legacy_x with
different MA seeds, plus drvec default and `-seedjoh` when the class exists in drvec; the
best logL is kept), `report.py` (full tables: `work/report_{legconf,legdata,pairs}.txt`),
`summary.py` (`work/summary.txt`). MA entries in drvec's Θ* naming:
**a = ∇Y₂←A.∇Y₂, b = ∇Y₂←A.W, c = W←A.∇Y₂, d = W←A.W.** Nested LRs:

* `LR tri/row` = 2[L(matri) − L(marow)]: **a**, given W row free, 1 df
* `LR free/row` = 2[L(free) − L(marow)]: the whole ∇Y₂ row (a, b), 2 df
* `L(leg) − L(row)`: legacy 1-parameter class vs `-marow`'s 2 parameters (non-nested)

Three data blocks: **legconf** = the legacy's own published configurations (convergence on;
AL, PL with β = 1; VL p = 4); **legdata** = the legacy's 175-obs data, no convergence,
β free, p = 1, 2, both AR modes; **pairs** = drvec's eight wheat pairs (data/pairs,
1700–1812, levels), p = 1, 2, both AR modes. `*` = a at the invertibility boundary.

```
set        data        p AR   |  a(leg) t LR leg/none | LR tri/row     p | LR free/row     p | L(leg)-L(row) | LR free/tri | marow term
legconf    AL          2 full | +0.89  16.2       18.63 |      20.66 0.000 |       23.05 0.000 |          3.90 |        2.40 | step d=1
legconf    PL          2 full | +0.72   8.5       18.05 |      16.59 0.000 |       17.81 0.000 |          7.96 |        1.22 | grad
legconf    SL          2 full | +0.79   9.3       17.83 |      17.48 0.000 |       18.21 0.000 |          6.44 |        0.73 | grad
legconf    VL          4 full | +0.89  11.6        7.74 |       7.91 0.005 |        5.38 0.068 |         -3.60 |       -2.53 | lower d=1
legdata    AL          1 full | +0.91  20.0       15.87 |       5.96 0.015 |        5.98 0.050 |          1.93 |        0.02 | grad
legdata    AL          1 vec  | -0.16  -2.4        5.32 |       0.02 0.891 |        4.50 0.105 |         -1.89 |        4.48 | grad
legdata    AL          2 full | +0.83  10.8       13.86 |      16.22 0.000 |       17.75 0.000 |          3.30 |        1.53 | step
legdata    AL          2 vec  | +0.87  17.0       18.49 |      15.33 0.000 |       16.82 0.000 |          2.25 |        1.49 | step
legdata    PL          1 full | +0.86  16.2       12.76 |      24.08 0.000 |       25.67 0.000 |          3.01 |        1.59 | grad
legdata    PL          1 vec  | -0.16  -2.0        3.94 |       2.56 0.110 |        2.95 0.228 |         -1.77 |        0.39 | grad
legdata    PL          2 full | +0.74   9.6       20.62 |       2.31 0.128 |        2.84 0.242 |         -1.53 |        0.52 | grad
legdata    PL          2 vec  | +0.82  14.3       21.83 |       1.49 0.222 |        2.36 0.307 |         -4.45 |        0.87 | lower
legdata    SL          1 full | +0.89  18.2       22.78 |      25.27 0.000 |       25.28 0.000 |          1.69 |        0.02 | grad
legdata    SL          1 vec  | -0.21  -3.0        6.29 |       0.14 0.703 |        1.25 0.535 |         -7.48 |        1.11 | grad
legdata    SL          2 full | +0.82  11.3       20.15 |      18.95 0.000 |       20.62 0.000 |          6.44 |        1.67 | grad
legdata    SL          2 vec  | +0.87  18.9       29.45 |      11.55 0.001 |       16.98 0.000 |          0.74 |        5.43 | lower
legdata    VL          1 full | +0.88  14.9       14.26 |       6.60 0.010 |        7.58 0.023 |          1.08 |        0.98 | grad
legdata    VL          1 vec  | -0.00  -0.0        0.00 |       0.14 0.704 |        0.48 0.787 |         -0.37 |        0.33 | grad
legdata    VL          2 full | +0.75   5.7       12.80 |       9.01 0.003 |       23.09 0.000 |          3.30 |       14.09 | grad
legdata    VL          2 vec  | +0.88  15.1       16.37 |       3.38 0.066 |       10.39 0.006 |          0.09 |        7.01 | lower
pairs      aix         1 full | +0.84   9.5       21.78 |      13.42 0.000 |       12.57 0.002 |          4.12 |       -0.84 | lower
pairs      aix         1 vec  | +0.07   0.5        0.30 |       1.36 0.243 |        7.44 0.024 |         -2.10 |        6.07 | grad
pairs      aix         2 full | +0.79   7.2       19.68 |      18.66 0.000 |       19.39 0.000 |          3.86 |        0.73 | lower
pairs      aix         2 vec  | +0.75   5.1       10.30 |      16.09 0.000 |       16.14 0.000 |          2.22 |        0.05 | grad
pairs      angers      1 full | +1.00*    -       10.34 |      12.23 0.000 |       13.62 0.001 |          2.27 |        1.39 | grad
pairs      angers      1 vec  | +0.04   0.2        0.06 |       0.02 0.878 |        0.03 0.984 |         -2.96 |        0.01 | grad
pairs      angers      2 full | +0.85  11.9       13.60 |       7.91 0.005 |        8.03 0.018 |          3.55 |        0.12 | step d=1
pairs      angers      2 vec  | +0.92  14.6       12.57 |      12.80 0.000 |       17.60 0.000 |          2.16 |        4.80 | lower d=1
pairs      arevalo     1 full | +1.00*    -       11.67 |      18.29 0.000 |       18.32 0.000 |          2.83 |        0.03 | lower
pairs      arevalo     1 vec  | +0.11   1.0        1.09 |       0.97 0.324 |        6.94 0.031 |         -2.97 |        5.97 | step
pairs      arevalo     2 full | +1.00*    -        9.25 |      16.86 0.000 |       20.43 0.000 |          3.96 |        3.56 | lower
pairs      arevalo     2 vec  | +1.00*    -        9.84 |      14.57 0.000 |       19.79 0.000 |          4.79 |        5.22 | step
pairs      milan       1 full | +1.00*    -       17.12 |      21.10 0.000 |       21.11 0.000 |         -4.17 |        0.01 | grad
pairs      milan       1 vec  | -0.22  -2.6        5.62 |       0.76 0.383 |        0.76 0.683 |        -12.60 |        0.00 | grad
pairs      milan       2 full | +0.71   6.5       13.89 |      19.12 0.000 |       20.03 0.000 |          3.22 |        0.91 | grad
pairs      milan       2 vec  | +1.00*    -       19.50 |      18.21 0.000 |       21.26 0.000 |          3.74 |        3.05 | grad
pairs      penn        1 full | +1.00*    -       21.08 |      20.57 0.000 |       20.32 0.000 |          3.53 |       -0.25 | lower
pairs      penn        1 vec  | -0.21  -1.6        2.39 |       2.85 0.092 |        6.12 0.047 |         -3.47 |        3.28 | grad
pairs      penn        2 full | +1.00*    -       26.89 |      19.75 0.000 |       20.21 0.000 |          9.35 |        0.47 | lower
pairs      penn        2 vec  | +1.00*    -       21.68 |       9.17 0.002 |        8.64 0.013 |          3.64 |       -0.53 | lower d=1
pairs      strasbourg  1 full | +1.00*    -       18.76 |      22.59 0.000 |       22.71 0.000 |          7.78 |        0.11 | lower
pairs      strasbourg  1 vec  | +0.03   0.2        0.06 |       1.15 0.283 |        3.65 0.162 |         -1.23 |        2.49 | lower
pairs      strasbourg  2 full | +0.73   5.7       13.08 |      18.66 0.000 |       18.87 0.000 |          6.32 |        0.21 | grad
pairs      strasbourg  2 vec  | +0.83   6.8       14.59 |      19.14 0.000 |       20.66 0.000 |          5.70 |        1.52 | grad
pairs      utrecht     1 full | +1.00*    -       13.46 |      21.50 0.000 |       23.07 0.000 |          4.24 |        1.57 | grad
pairs      utrecht     1 vec  | -0.17  -1.9        3.83 |       2.02 0.155 |        2.55 0.279 |         -2.50 |        0.53 | grad
pairs      utrecht     2 full | +0.67   4.8       11.59 |      23.91 0.000 |       27.16 0.000 |          3.35 |        3.24 | lower
pairs      utrecht     2 vec  | +0.77   6.4        8.79 |      22.63 0.000 |       22.92 0.000 |          4.12 |        0.30 | grad
pairs      vienna      1 full | +1.00*    -       19.61 |      21.28 0.000 |       22.01 0.000 |          8.37 |        0.73 | lower
pairs      vienna      1 vec  | +0.06   0.5        0.24 |       0.64 0.425 |        1.67 0.433 |         -0.39 |        1.04 | grad
pairs      vienna      2 full | +1.00*    -       15.68 |      17.24 0.000 |       21.95 0.000 |          6.51 |        4.71 | grad
pairs      vienna      2 vec  | +1.00*    -       21.06 |      19.69 0.000 |       22.58 0.000 |          8.50 |        2.89 | grad
```

(`t` at a boundary point is meaningless — the SE collapses — and is shown as "-".)

### 4.1 Reading it

1. **The split is by whether ∇Y₂ has an AR lag.** In drvec's p = 1 ("vec", p = 1) the
   ∇Y₂ row of Φ*₁ is (0, −Λ₂): ∇London has no own lag. There `a` is small (|a| ≤ 0.22)
   and LR tri/row ≤ 2.9 in 12/12. In every configuration where ∇Y₂ has lags (legacy full
   AR at p = 1, 2, 4; drvec p = 2), `a` is **0.67–1.00 with |t| 5–20** where interior,
   LR(leg vs none) 7.7–29 on 1 df, and **LR tri/row ≥ 5.96 in 37 of 40** (≥ 7.9 in 35);
   the exceptions are PL p = 2 full/vec (2.31, 1.49) and VL p = 2 vec (3.38).
   (40 = legconf 4 + legdata full p1/p2 and vec p2, 12 + pairs full p1/p2 and vec p2, 24.)
   Against drvec's own bootstrap inflation of the χ² critical values (1.5–2.3×,
   USAGE §testing: 5.8–8.8 for 1 df) most of these still reject `-marow`.
2. **The legacy class alone (1 MA parameter) beats `-marow` (2 parameters)** in 36 of
   the same 40 configurations (`L(leg) − L(row)` > 0), by up to 9.4 log-points.
   The b coefficient (∇Y₂ ← W innovation) is rarely needed: LR free/tri is < 3.84 in
   34/40 (4.7–14.1 in VL p = 2 full/vec, SL p = 2 vec, angers p = 2 vec, arevalo p = 2 vec,
   vienna p = 2 full).
3. **Where it lives.** On the legacy 175-obs samples (1701–1875), `a` is interior
   (0.72–0.91, SE 0.05–0.13) in every legacy configuration. On the 90–113-obs pairs it is
   at 1.000 in 10 of the 16 "full" and 4 of the 8 "vec p = 2" legacy-class fits, and
   `-matri`'s `a` is at 1.000 in 7 of 8 pairs at p = 2 (penn 0.64 the exception) — the
   T₂₂ → 1 route drvec's HOMOLOGATION §4j documented.
4. **Why.** ∇London's sample autocorrelations are −0.03, −0.23, −0.19, −0.08 (legacy data;
   −0.10, −0.23, −0.25, −0.05 on the pairs): the partial sums nearly cancel, i.e. its
   spectrum at frequency 0 is close to zero — "London is nearly over-differenced". A Ȳ-model
   can only express that with an MA root near 1 in the ∇Y₂ row, which is exactly the
   coefficient `-marow` removes. `omega.py` measures the implied long-run variance of ∇Y₂
   relative to its innovation variance, ω = [Φ*(1)⁻¹Θ*(1)ΣΘ*(1)′Φ*(1)⁻ᵀ]_{∇Y₂}/Σ_{∇Y₂}:
   0.04–0.34 in the legacy-class and `-matri` fits, **0.03–0.11 also in `-marow` on the
   legacy data** (AL/PL/SL/VL p = 2), 0.36–0.90 in `-marow` on six of the pairs. Even at
   a = 1 ω stays > 0 (0.06–0.34), because the trend is fed through the AR cross-terms;
   it vanishes only if also Λ₂ = 0 (London not adjusting) — the Ȳ form of
   `rank(Λ⊥′Θ(1)) = s ⇔ rank[Φ(1) Θ(1)] = M` (ESTUDIO_MAURICIO §4).
5. **Caveat on the LR's distribution (OPEN).** Whenever the wider class sits at a = 1 the
   χ² reference is not valid (boundary / near non-identification). On the legacy's own
   samples, where a is interior, the LRs are ordinary χ² tests of a zero restriction on an
   interior parameter and reject `-marow` decisively (16.6–20.7 in the three published
   configurations with p = 2). A parametric bootstrap of LR(matri vs marow) — drvec's
   `-matest` machinery — has not been run here.

---

## 5. What drvec dropped or changed, and whether it matters for the decision

| item | legacy | drvec | matters? |
|---|---|---|---|
| AR on Ȳ | all Φ_k free (∇Y₂_{t−p} in both equations) | Φ*_p ∇Y₂ columns = 0 | **yes**: the ∇Y₂ AR lag is what makes `a` significant; at drvec p = 1 ∇Y₂ has no own lag and the question never arises |
| MA | only ∇Y₂ own (a); hard-coded | classes; default `-marow` zeroes a, b | **yes**: the default removes the only coefficient the legacy estimated |
| MA seed | θ₂₂ by inverting the lag-1 autocorrelation of the VAR residual of ∇L (main.c 327–363), σ₂₂/(1+θ²) (383) | MA seeded 0 (`-seed` optional; USAGE §0 says seeding the MA was measured to hurt with r ≥ 1) | moderately: in this study the legacy_x 7-start search beat drvec's 2 starts in 13/120 fits, drvec won 8/120, 99 tied |
| Σ | full triangle, seed /100 (395–402), flat scale direction | Q₁₁ = 1, σ² concentrated | no for the optimum (logL identical), yes for conditioning (ESTUDIO §4.1 VERIFIED) |
| β | by data only; OLS static seed; `beta_fijo` | B₂ by data only in `-warma`, otherwise through C̄ in the cast | parametrisation only; same optimum (§2.3) |
| convergence ν(B)X | yes (step, α/(1−φB), 25 lags) | no | not for the MA decision; matters for the post-1812 legacy samples |
| deterministic | case 2 only | cases 1–3 | no |
| tolerances | gradtol 1e-7, steptol 1e-7, maxits 500, xitol 1e-3 | gradtol 1e-5, steptol 1e-7, maxits 500, xitol 1e-3 | stopping only; the p = 1 agreement is to 1e-6 |
| admissibility | only elf's gate (roots not strictly inside) | same gate + optional `-rankadm` | same boundary behaviour: the legacy reaches θ₂₂ = 1.00005 (bin/AL.2.beta1.out) |
| dimension | m = 2, r = 1 literal | general M, r | — |

**DISCREPANCY with ESTUDIO_BVECM_vs_DRVEC.md**: its §2.2/§2.3/§3.1/§3.6/§4.1–4.2 are
VERIFIED; §5 "MA restringida a θ₂₂(1)" is VERIFIED but omits the point that matters here —
θ₂₂ is the **∇Y₂** equation (legacy order `(w, ∇z₂)`), so the legacy estimated precisely
the MA that `-marow`, `-mawarma` and `-warma` exclude; §3.1's "φ, θ written as-is from x[]"
is VERIFIED, but the Ȳ class the legacy searches (full AR, a-only MA) is not Mauricio's
class nor any drvec class.

---

## 6. OPEN

* Bootstrap distribution of LR(`-matri` vs `-marow`) and LR(legacy vs none) on the legacy
  samples (interior a) and the pairs (boundary a).
* Whether the "legacy class + drvec AR" (a-only MA, p = 2) is worth offering as a drvec
  class (`-madiff`?): it is the most parsimonious class that captures the dominant MA
  signal (one parameter, 36/40 better than `-marow`), but it inherits the a → 1 boundary on
  short samples.
* Provenance of `bin/AL.*.out` (older main.c, 17-column input, dummies).
* Whether the article (studied by the other agent) specifies the MA on the W block
  (its Corollary 2 / WARMA, as drvec's `-mawarma` comment says) while its own code put it
  on ∇Y₂ — if so, the code and the article disagree on the MA block.

---

## 7. Reproduce

```sh
B=<this directory>
cd $B/legacy   && make            # verbatim rebuild
cd $B/legacy_x && make            # instrumented (patch: scripts/legacy_x.patch)
cd $B/scripts
python3 equiv.py 1; python3 equiv.py 2          # §2.3
python3 evidence.py legconf; python3 evidence.py legdata; python3 evidence.py pairs   # §4 (~75 s)
python3 summary.py; python3 report.py pairs     # tables
python3 bec_check.py                            # §3 (needs work/bec/SL.out: drv SL 2 struct 1)
python3 omega.py 'ev_milan_p2_vec_matri_n_s*'  # §4.1 item 4
```

legacy_x environment: `DRV_AR=full|vec|warma`, `DRV_MA=leg|none|ww|diag|wrow|tri|free`,
`DRV_MASEED=v` (seed of the non-θ₂₂ MA slots), `DRV_A22SEED=v` (seed of θ₂₂). The legacy
positional syntax is `drv <file> <p> [beta_fijo] [no_conv] [struct] <method>`.
