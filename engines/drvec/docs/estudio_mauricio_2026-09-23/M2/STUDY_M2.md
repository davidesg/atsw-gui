# STUDY M2 — Mauricio (2006) supporting material, and what `drvec` does with it

*Scope: the AddOn (A1, A2), the estimation-practice / rank-testing / example
parts of Mauricio (2006), Yap & Reinsel (1995) Theorem 3, Johansen (1991)
Theorems 2.1–2.2, and drvec 0.10 (`src/drvec.c`, docs). READ-ONLY on every
repository: all runs were made on a copy (`drvec_copy/`, instrumented only to
inject a parameter vector and dump the transformed system) or on copies of the
inputs (`run/`, `run3/`, `mc/`). Every number below is reproduced by a script
in this directory (list in §7).*

Labels: **VERIFIED** · **CONFIRMED DISCREPANCY** · **OPEN**.

---

## 0. Verdicts at a glance

| # | item | verdict |
|---|---|---|
| 1a | AddOn A1 derivation (A.1)–(A.11) | **VERIFIED** (correct; three small gaps filled, §1.2) |
| 1b | drvec's general map reduces to A1 exactly (algebra + numerics) | **VERIFIED**: Φ*, Σ*, Ȳ identical (diff 0.0); likelihood equal to an independent Toeplitz *and* Kalman exact likelihood to 2.5·10⁻⁸ (the residue is drvec's truncated `LOG2PI` constant) |
| 1c | general case (M=3, r=1,2, p=3, q=2, case 3) | **VERIFIED**: map exact to 10⁻¹⁵; logL equal to 5·10⁻⁸ with `-m 2`; default `-m 1` carries the AS 311 ξ-truncation, up to 1.6·10⁻³ in logL |
| 1d | general stationarity of the Ȳ-VARMA (the proof the paper calls unnecessary) | **supplied** here (§1.4): operator identity Φ(x) = Φ̄(x)D(x)C̄, checked numerically |
| 2a | AddOn A2 tables internally consistent | **VERIFIED** (IC with N=112, LRs, eigenvalues, Q̂⁻¹, P̂′Λ̂=0) |
| 2b | AddOn A2: EML Θ̂₁ non-invertible (θ̂₂₂ = 1.0844 / 1.1318) | **CONFIRMED DISCREPANCY** with the paper's own assumption (1) — a gap in the paper, not an arithmetic error |
| 2c | Can drvec express (A.14)? | **No** — missing: a per-lag MA mask (or multiplicative seasonal MA), and evaluation beyond the invertibility gate. Seasonal differencing must be done outside (§2.3) |
| 3a | mink–muskrat: drvec fits a *different* model? | **Ruled out** (§3.2–3.4): same data, ordering, sample, layout, conditioning and case; drvec's logL = independent exact likelihood of the paper's model at the paper's own parameter values |
| 3b | the published EML column | **CONFIRMED DISCREPANCY — now localised exactly** (§3.5): for Tables 2, 4 and 5 the published EML logL equals, within rounding (0.009–0.037), the exact log-likelihood evaluated with the published Σ̂ **but with the quadratic form set to nM**; the ML scale of Σ at the published (Φ̂, Θ̂, ĉ) is 18–21 % larger than the published Σ̂ (Σ̂ 15–18 % too small). The true exact logL at the published points is 4.75 / 5.18 / 0.58, not 15.61 / 15.13 / 12.40 |
| 3c | the paper's EML LR tests (0.9718; 5.4512) | consequence of 3b: both are mostly artefact. With drvec's correct likelihood, B = [1,0]′ gives LR = 7.90 (p = 0.005), so the paper's "cannot be rejected at 1 %" **does not survive** (with caveats, §3.6) |
| 3d | the published CML column | **OPEN / inconclusive**: its (L, Σ̂) pairs satisfy the CML identity, a CML value ≥ 12.05 is attainable on these data (12.41), but the printed CML point does not reproduce its own L under any of three start-up conventions (the CML depends on the unknown start-up rule; Θ̂ eigenvalues ±0.945 make it slow to forget) |
| 4a | drvec's case-2 critical values | **VERIFIED** (Osterwald-Lenum Table 1*, Johansen 1991 Thm 2.2) |
| 4b | drvec's case-1 critical values | **CONFIRMED DISCREPANCY** (= BUG-24): they are the unrestricted-constant/zero-drift table (Johansen 1991 Thm 2.1, α⊥′μ = 0), not the no-deterministic one (YR 1995 Thm 3). Mauricio's own p-values reproduce only with the correct tables |
| 4c | "MA terms do not affect the distribution" for drvec's statistic | **CONFIRMED DISCREPANCY (conceptual, beyond BUG-26)**: YR Thm 3 is for the *conditional* Gaussian likelihood. drvec's (and Mauricio's) *exact* LR is not nested at Λ → 0; its null distribution drifts down ≈ −log T (measured mean shift −2.5, −3.7, −4.9 at T = 100, 400, 1600) and is not Johansen's at any T. Negative LRs are genuine optima (independently re-fitted) |
| 5 | forecasting in levels (P5, FORECAST.md, Proposition 2) | **VERIFIED**: inverse map and G_m derived; point forecast equal to a direct VEC-in-levels recursion to 4·10⁻⁷; bands equal to a 200 000-path Monte Carlo of the VEC in levels within ±0.3 % |

---

## 1. AddOn §A1 — the bivariate VAR(1)

### 1.1 The derivation redone

Model (A.1): Y_t = Φ₁Y_{t−1} + A_t, M = 2. |I − Φ₁x| = (1 − μ₁x)(1 − μ₂x), so
the roots are x_i = 1/μ_i. Partial nonstationarity with D = 1 ⇔ μ₁ = 1,
|μ₂| < 1 (with the convention x₂ = ∞ when μ₂ = 0). ✔

Π = I − Φ₁ has eigenvalues 1 − μ₁ = 0 and 1 − μ₂ ≠ 0; it is therefore
diagonalisable with rank exactly 1 (a 2×2 matrix with distinct eigenvalues 0 and
1−μ₂). ✔ (A.3)

(A.4) Π = ΛB′ = [λ₁; λ₂][1, β₂]. *Gap 1*: a rank-1 Π is some ab′ with a, b ≠ 0;
normalising b = (1, β₂)′ requires b₁ ≠ 0, i.e. the **first column of Π is not
zero** (1 − φ₁₁, −φ₂₁) ≠ 0. If it is zero, B = (0, 1)′ and the normalisation (6)
fails — exactly the ordering issue the paper warns about on p. 3648. A1 asserts
(A.4) without the condition.

(A.5): the general map with p = 1 has no F terms, so (15)–(16) read
Φ̄₀ = C̄⁻¹, Φ̄₁ = C̄⁻¹H̄ − Λ̄ (*Gap 2*: (16) as printed lists Φ̄_p = −F_{p−1}C̄⁻¹H̄,
which is meaningless for p = 1; the p = 1 reading must be Φ̄₁ = C̄⁻¹H̄ − Λ̄ only;
drvec implements exactly that, `if (p >= 2)` guards). Premultiplying
C̄⁻¹Ȳ_t = (C̄⁻¹H̄ − Λ̄)Ȳ_{t−1} + A_t by C̄ gives Ȳ_t = (H̄ − C̄Λ̄)Ȳ_{t−1} + C̄A_t. ✔

(A.6): C̄Λ̄ = [0 1; 1 β₂][0 λ₁; 0 λ₂] = [0 λ₂; 0 λ₁+β₂λ₂], hence
H̄ − C̄Λ̄ = [0 −λ₂; 0 1−λ₁−λ₂β₂]. ✔

(A.7)–(A.9): trace and determinant of Φ₁ with μ₁ = 1 give
μ₂ = φ₁₁ + φ₂₂ − 1 = φ₁₁φ₂₂ − φ₁₂φ₂₁. ✔ (A.10) follows from |μ₂| < 1. ✔

(A.11): tr Π = 2 − φ₁₁ − φ₂₂ = tr(ΛB′) = B′Λ = λ₁ + λ₂β₂, so
φ₁₁ + φ₂₂ − 1 = 1 − λ₁ − λ₂β₂, and |1 − λ₁ − λ₂β₂| < 1. ✔

Eigenvalues of (A.6): upper triangular ⇒ {0, 1 − λ₁ − λ₂β₂}. ✔ Conclusion
correct. What the AddOn does not say, and is the cleaner statement:
**1 − λ₁ − λ₂β₂ = 1 − B′Λ = μ₂ exactly** — the nonzero eigenvalue of the Ȳ-VAR
*is* the stable eigenvalue of Φ₁; (A.7)–(A.11) are a detour to that identity.

*Gap 3 (initial conditions)*: "(A.5) is a stationary VAR(1)" proves that a
stationary solution exists; that the Ȳ built from data **is** that solution
requires W at the first date to be drawn from its stationary law. With Y started
from a fixed value it is only asymptotically stationary. This is exactly the
assumption the exact likelihood encodes, and it is the root of the rank-test
problem of §4.3.

**Verdict: VERIFIED** (no error; gaps 1–3 are omissions).

### 1.2 General statement for VAR(1), any M, r (proof)

With Λ = [Λ₁ (r×r); Λ₂ (s×r)], C̄Λ̄ = [0 Λ₂; 0 Λ₁ + B₂′Λ₂] = [0 Λ₂; 0 B′Λ], so

    Φ*₁ = H̄ − C̄Λ̄ = [ 0_s   −Λ₂ ; 0   I_r − B′Λ ],

block upper-triangular: eigenvalues are 0 (s times) and eig(I_r − B′Λ); and
eig(ΛB′)\{0} = eig(B′Λ), so eig(I_r − B′Λ) are exactly the non-unit eigenvalues
of Φ₁ = I − ΛB′. Stationarity ⇔ those lie inside the unit circle, and
no unit eigenvalue in I_r − B′Λ ⇔ B′Λ nonsingular ⇔ Johansen's I(1)
condition for a VAR(1) (|α⊥′β⊥| ≠ 0). ∎

### 1.3 drvec reduces to A1 — numerically

`check_A1.py` (mink–muskrat data, λ₁ = 0.3, λ₂ = 0.1, β₂ = −0.5, case 2, E[W] = 10,
Σ = [1 .3; .3 1.5]); drvec run through `-eval` with the injected vector:

    A.5 == A.6 closed form ................... 0.0
    general map (eqs 16, 18) vs A.6 ........... 0.0
    drvec Φ* vs A.6 ........................... 0.0
    drvec Σ* vs C̄ΣC̄′ ........................... 0.0
    drvec Ȳ vs eq. (17) ........................ 1.3e-15
    independent Toeplitz logL (conc.)  -215.2907070684
    independent Kalman   logL          -215.2907070684
    drvec -eval                        -215.2907070434   (Δ = 2.5e-8)

The 2.5·10⁻⁸ is `LOG2PI = 1.837877066` in `drvmlest.c`/`run_eval` against
log 2π = 1.8378770664093: 4.1·10⁻¹⁰ × nM/2 = 2.5·10⁻⁸. **VERIFIED.**

General case (`check_general.py`, M = 3, p = 3, q = 2, case 3, random
parameters): Φ*, Θ*, Σ*, μ, Ȳ agree to ≤ 10⁻¹⁴ for r = 1 and r = 2. The
likelihood agrees to 5·10⁻⁸ with `-m 2`; with the default `-m 1` it differs by
9·10⁻⁵ (r=1) and 1.6·10⁻³ (r=2): the AS 311 ξ-truncation at `xitol = 1e-3`
(documented in USAGE.md for the forecast certificate; note that the source
comment `met: 1 = exact, 2 = approximate` reads the wrong way round for the
likelihood value — `-m 2` is the untruncated one). Harmless for LR tests at the
precision they are read, but it is the default.

### 1.4 The general stationarity proof the paper omits

The paper (p. 3650–3651) replaces a proof by the argument "a stationary process
following a uniquely identified VARMA ⇒ the model is stationary". A direct
proof: with D(x) = diag((1−x)I_s, I_r), Ȳ_t = D(L)C̄Y_t (because
C̄Y_t = [Y₂ₜ; Wₜ]). The derivation (8)→(15) is a polynomial rearrangement valid
for every sequence, hence the operator identity

    Φ(x) ≡ F(x)(1−x) + ΛB′x  =  Φ̄(x) D(x) C̄,        Φ̄(x) = Φ̄₀ − Σ Φ̄ᵢ xⁱ.

(p = 1 by hand: Φ̄(x)D(x)C̄ = C̄⁻¹(D(x) − H̄x)C̄ + Λ̄C̄x = (1−x)I + ΛB′x, since
D(x) − H̄x = (1−x)I and Λ̄C̄ = [Λ, ΛB₂′] = ΛB′.) Checked numerically for
(M,r,p) ∈ {(2,1,1),(2,1,2),(3,1,3),(3,2,4),(4,2,2)} at random complex x:
max error 8·10⁻¹⁵ (`check_polyid.py`). Taking determinants,
|Φ(x)| = ±(1−x)^s |Φ̄(x)|, and |Φ*(x)| = |C̄||Φ̄(x)|. So **|Φ*(x)| has all its
roots outside the unit circle iff x = 1 is a root of |Φ(x)| of multiplicity
exactly s = M − r and every other root is outside** — i.e. iff the
partial-nonstationarity (I(1)) assumptions hold. No identifiability argument is
needed. ∎

---

## 2. AddOn §A2 — the census housing example

### 2.1 Internal consistency (`check_A2.py`)

| check | result |
|---|---|
| AIC = (−2L + 2K)/N, BIC = (−2L + K ln N)/N, N = 112, K = 9 (A1), 8 (A3) | all four columns reproduce to 4 decimals ✔ (the main paper prints AIC = −(2L*+2K)/N: **sign typo**, the numbers use −2L+2K) |
| LR 2[L(2) − L(1)] | EML 1.5534 ✔, CML 1.9866 ✔ (implied L_E(0) = −693.85, L_C(0) = −700.93) |
| eig(I − Φ̂₁), Table A1 | EML 0.0459, 0.7140 ✔; CML 0.0524, 0.7334 ✔ |
| eig(Λ̂B̂′) = B̂′Λ̂, Table A3 | EML 0.7212 ✔; CML 0.7369 ✔ |
| CML identity L = −N(1+log 2π) − (N/2)log\|Σ̂\| | A1 CML −669.9048 vs −669.9050 ✔; A3 CML −670.8978 vs −670.8983 ✔ |
| EML vs the same identity | −32.0 and −34.7 below it: the log-determinant (initial-state) term of an exact likelihood with a seasonal MA at the unit circle — plausible in size |
| Q̂⁻¹ (p. 10) | [[0.7198, 1.3406], [−0.1504, 0.7198]] ✔; P̂′Λ̂ = −1·10⁻⁵ ✔ |
| ±2N^{−1/2} | 0.1890 ✔ |

### 2.2 Errors and gaps in A2

* **Non-invertible EML estimates** (CONFIRMED DISCREPANCY with the paper's own
  model assumption (1), "|Θ(x)| = 0 has no roots inside the unit circle"):
  Θ̂₁ = diag(0.9641, 1.0844) (A1) and diag(0.9600, 1.1318) (A3); root moduli of
  I − Θ₁L¹² are θ^{−1/12} = 0.99327 and 0.98974 < 1. The paper even builds its
  "Θ₁ = I" reading on it. With a diagonal Θ and a non-diagonal Σ, flipping one
  root generally leaves the diagonal class, so the invertible equivalent is not
  available inside the estimated class; the printed standard errors (0.1870,
  0.1735) are not valid at a boundary/flip point (flat likelihood; DCD-type
  non-standard limits), and "Θ₁ = I" is never tested formally.
* A2 tests P = 1 vs P = 2 with an exact-likelihood LR and nonstandard
  p-values; §4.3 shows that statistic does not have the tabulated null
  distribution. The conclusions of Table A2 (59.2 ≫ any cv; 1.55 small) are
  robust to that, but the p-values 24.95 % / 18.70 % are not what they claim.
* Its p-values themselves are correctly computed for case 1 (no deterministic
  term): simulated no-constant λ-max(g=1) gives P(>1.5534) = 25.3 %, P(>1.9866)
  = 19.2 % (vs 24.95 %, 18.70 % printed). The restricted-constant or
  unrestricted-constant tables would give 86 % / 66 %. So Mauricio used the
  right table for his case.
* drvec's own `docs/DRVEC_REFERENCE.md` §1.2 misstates (A.14): it writes
  "y_t = (1−B)(1−B¹²)x_t" (the ∇ belongs to the VEC, not the data) and
  "∇y_t = ΛB′y_{t−1} + (I − Θ₁L)A_t" (sign of the EC term, and the MA is at lag
  12, not 1). Documentation defect only.

### 2.3 Can drvec express (A.14)?  ∇y_t = −ΛB′y_{t−1} + (I − Θ₁L¹²)A_t, Θ₁ diagonal

| requirement | drvec 0.10 |
|---|---|
| y_t = (1−L¹²)x_t | the `.inp` route **does not apply** λ/d/D (warns, `drvec.c:8473`). Supply y already seasonally differenced; the default levels layout then forms ∇ internally ✔ |
| VEC(1), p = 1 on Ȳ, M = 2, r = 1, case 1 (zero mean) | ✔ |
| MA **only at lag 12**, Θ₁₂ diagonal | ✘ q = 12 makes Θ₁…Θ₁₁ free (44 params, or 22 with `-diagma`); there is no per-lag zero mask and no multiplicative seasonal operator. The engine (AS 311) and the map Θ*ₖ = C̄ΘₖC̄⁻¹ both preserve zero lags, so only the **cast** (`vec_shootx`/`calc_nparametrs`/`init_guess`/printer) is missing a lag mask |
| the EML optimum with θ̂₂₂ = 1.13 | ✘ `chekma` rejects any MA eigenvalue modulus ≥ 1.00005 (ifault 4): even with the mask drvec could not reach or even evaluate the published point. An exact Gaussian likelihood needs no invertibility (my Toeplitz code evaluates it) |
| the data | not in the repository (`data/housing_*.inp` are simulations) |

**Verdict: not expressible.** Missing: (i) a lag mask for Θ (and F) in the
parameter cast; (ii) a way to evaluate / optimise beyond the invertibility gate
(or a seasonal-MA parameterisation with the root on/over the circle handled as
in DCD); (iii) the census data.

---

## 3. Mink–muskrat — is drvec fitting a different model?

### 3.1 The paper's tables are internally consistent (`check_published.py`)

AIC/BIC for all six columns reproduce with **N = 61** and K = 15, 13, 12 (EML),
14, 12, 11 (CML) — the K's match the "(—)" zero patterns. The LR statistics are
exact differences of the printed L's (0.9718, 5.4512, 3.7228, 14.5320 ✔).
Eigenvalues: Π̂ (0.0413, 1.0602), (0*, 0.7191), (0*, 0.9382) ✔.
Θ̂₁ eigenvalues: **Table 2 EML 0.8461, −1.0063; Table 4 EML 0.9904, −1.0078**
(non-invertible as printed — the paper quotes only the +0.9904); Table 5 EML
0.8134, −0.9971 ✔; CML columns ±0.945, ±0.924, ±0.834. A 4-decimal rounding moves
these by ~10⁻⁴, so the non-invertibility is not a rounding artefact. Agree with
ANALISIS_PRELIMINAR §5.3.

### 3.2 Pinning down the model the paper fits

* **Ordering.** E[W] = 8.1345 with B = [1, −0.2042]′ equals mink − 0.2042·muskrat
  at the sample means (10.79 − 0.2042·13.0 ≈ 8.13); Table 5's 10.8161 is the mink
  mean. So Y₁ = mink, Y₂ = muskrat, the order drvec's `.inp` encodes as
  columns [Y₂ ; Y₁] = [muskrat, mink] and `vec_shootx` reads as paper order
  [Y₁; Y₂] (C̄, Λ, F, Θ, Σ all in paper order; Ȳ columns in [∇Y₂; W]). ✔
* **Sample/conditioning.** N = 61 from the IC (both columns, all tables) = 62
  annual observations with one consumed: exactly drvec's levels layout
  (`nobs = nobs_raw − 1`, Ȳ over 1851–1911, conditioning on Y₂,₁₈₅₀ through the
  unit-Jacobian map). ✔
* **Case.** (25) has E[W] only: case 2. ✔ (Mauricio's LR p-values also match the
  restricted-constant table: 0.9718 → 95.8 % simulated vs 95.51 % printed;
  3.7228 → 46.0 % vs 45.50 %.)
* **Data.** ANALISIS §5.5b matched the series with the canonical Hipel–McLeod
  source. Misalignment variants (muskrat ±1 year, dropping 1850 or 1911) make
  every fit worse (`shift_test.py`).

### 3.3 drvec computes the paper's likelihood — at the paper's own parameters

`table4.py`: Ȳ-VARMA built from the paper's equations (10)–(18) with the
Table 4 EML values (Θ̂ shrunk ×0.9 and ×0.99 so drvec's gate admits it),
independent Toeplitz exact likelihood vs drvec `-eval`:

    Θ×0.90   independent 5.2544618653   drvec 5.2544618903
    Θ×0.99   independent 5.1820226364   drvec 5.1820226614

(same 2.5·10⁻⁸). The published point itself (no shrinking; the Gaussian density
does not need invertibility; Toeplitz and Kalman agree):

    logL at published (Λ̂, B̂, F̂, Θ̂, Ê[W], Σ̂)  = 4.3246      (paper: 15.1257)
    same, scale of Σ concentrated          = 5.1756

So the number the paper prints is not the exact likelihood of its own model at
its own estimates on these data, and drvec is not the reason.

### 3.4 Alternatives ruled out

| alternative "different model" | evidence |
|---|---|
| ordering [Y₂;Y₁] reversed | fixed by E[W] (§3.2); ANALISIS §1: the permuted order makes Table 4 non-stationary |
| levels vs differenced layout | identical Ȳ (drvec `Ȳ` = eq. (17) to 10⁻¹⁵); `-differenced` shifts W by a constant absorbed by E[W] in case 2 |
| conditioning on the first observation / N | N = 61 fixed by the IC; Table 2 on 62 obs does not help (below) |
| mean / case | case 2 fixed by (25) and by the p-values |
| sample window / alignment | `shift_test.py`: all variants worse |
| matrix orientation (transposes, Θ sign) | ANALISIS §1 (8 variants), `cml_orient.py`: the printed orientation is the best |
| log10 vs ln | ĉ = 10.80 = ln-mean of mink |

### 3.5 Where the published EML numbers come from (new)

Hypothesis tested in `sigma_hyp.py`: the published EML logL is the exact
concentrated formula evaluated with the published Σ̂,
L_pub = −(nM/2)(log 2π + 1) − ½ log|V(Σ̂_pub)|, i.e. the exact log-likelihood at
Σ̂_pub with the quadratic form y′V⁻¹y **replaced by nM**. If c is the ML scale
of Σ at the published point, that equals L_conc + n·log c (M = 2):

| table | n | L at published Σ̂ | L, scale concentrated | c | L_conc + n log c | published | diff |
|---|---|---|---|---|---|---|---|
| 2 (model 22) | 61 | 3.7300 | 4.7544 | 1.1946 | **15.6025** | 15.6116 | −0.009 |
| 2 (model 22) | 62 | 4.7776 | 5.6725 | 1.1797 | 15.9164 | 15.6116 | +0.305 |
| 4 (model 25) | 61 | 4.3246 | 5.1756 | 1.1765 | **15.0889** | 15.1257 | −0.037 |
| 5 (B=[1,0]′) | 61 | −0.6397 | 0.5795 | 1.2135 | **12.3826** | 12.4001 | −0.018 |

Three independent tables, three matches at the rounding level (and the n = 61
reading, not 62, as the IC say). **The whole 8–11 unit gap is accounted for:**
the published Σ̂ is 15–18 % too small in scale for the published (Φ̂, Θ̂, ĉ, Λ̂,
B̂, F̂), and the log-likelihood was computed as if that Σ̂ were the maximiser
(S/(nM) = 1), when on these data S(Σ̂_pub)/(nM) = 1.18–1.21. The same pattern
explains why |Σ̂_EML| is "systematically" small (ANALISIS §5.9 fact 1) and the
factor ≈ 2 in housing.

The *mechanism* in the author's program is **OPEN**. One candidate was tested
and rejected (`sigma_mech.py`): Σ̂ = (1/n)Σ â_t â_t′ with â_t = E[a_t | data]
(i.e. omitting the presample part of the quadratic form) gives |·| = 0.00239 /
0.00237 / 0.00286, not the published 0.00179 / 0.00179 / 0.00206 — the presample
share of the quadratic form is only 2–3 %. The divisor is not a
degrees-of-freedom factor either (c ≠ n/(n−K)).

Consequences:
* The published EML (Φ̂, Θ̂, ĉ) points are plausible for these data; their
  **Σ̂ and log-likelihoods are not** (CONFIRMED DISCREPANCY in the paper).
* drvec's optimum under the correct exact likelihood (6.48–6.96 for model (25)
  unrestricted, depending on start/case; 6.964 with `-multistart 60`, case 2,
  `-mafree`) is **above** the true value of the paper's own point (5.18) — as it
  must be, since drvec's class contains the paper's restricted one.
* ANALISIS §5.11's "cannot be closed without the original program" is now
  mostly closed: what is left open is only *why* the program's Σ̂ is small.
* Minor doc slip: HOMOLOGATION §3 labels 15.61 / 15.13 as "Table 5"; they are
  Tables 2 and 4 (Table 5 is 12.4001).

### 3.6 Consequences for the paper's inferences

* LR 2[L(2) − L(1)] = 0.9718: with the true likelihood at the *published* points
  it is 2(4.7544 − 5.1756) = −0.84; at proper maxima drvec cannot compute
  P = M = 2 (§4.4). The printed 0.9718 ≈ −0.84 (true) + 2·61·log(1.1946/1.1765)
  = −0.84 + 1.86 (difference of the two Σ̂-scale artefacts n·log c) = 1.02.
* B = [1, 0]′: printed 5.4512 (p = 1.96 %). drvec, same data, unrestricted
  Θ/F, 60 starts each: L_free = 6.9640, L(B₂=0) = 3.0124, **LR = 7.90,
  p = 0.0049** — rejects at 1 %, reversing the paper's "cannot be rejected at the
  1 % level". Caveats: both drvec optima sit on the MA unit circle (the χ²
  reference is then not guaranteed), and drvec cannot impose the paper's zero
  pattern (F₁[2,2] = Θ₁[1,1] = 0).

### 3.7 The CML column (OPEN)

The CML (L, Σ̂) pairs satisfy the CML identity to ≤ 0.06 (`check_published.py`
§4), so they are internally consistent. But evaluating the conditional
likelihood at the printed CML points gives, for Table 2, 4.05 / 1.70 / −0.33
under three start-up conventions (presample Y := ĉ and a := 0 from 1850, from
1851, or true values from 1852), against 12.047; for Table 4, 1.99 / 0.74
against 10.19. A local re-optimisation from the printed point never returns to
it (`cml_local.py`), and the global CML maximum on these data is 12.41 with
N = 61 (`cml_opt.py`) — so a value of 12.05 is attainable, only not at the
printed parameters under my conventions. Because Θ̂ has eigenvalues ±0.945 the
start-up rule decays with half-life ≈ 12 years and dominates a 61-observation
CML, and Mauricio's rule is not stated. ANALISIS §5.6's statement that "the CML
column is coherent with these data" rests only on |Σ̂| and is not supported by
this stronger check; it is not evidence against the data either. Inconclusive.

**Bottom line for item 3: I agree that the published EML column is not
reproducible, and the evidence now rules out that drvec represents a different
model: drvec computes, to 2.5·10⁻⁸, the exact likelihood of the paper's model
(same data, order, sample, layout, conditioning and case) at the paper's own
parameter values, and the paper's EML log-likelihoods are reproduced to
rounding by a specific miscomputation (Σ̂ too small, quadratic form taken as nM).**

---

## 4. Rank testing

### 4.1 What the sources say

* **Mauricio, Remark 5** (p. 3652): (19) is Phillips' triangular form, hence
  "testing for partial nonstationarity can be done using exactly the same
  procedures as those already well-established" (YR 1995, Reinsel 1997 §6.3,
  Johansen 2001). Remark 6 / §4 (p. 3653): LR statistics follow different
  distributions per case; p-values from MacKinnon–Haug–Michelis (1999); "as
  shown by Yap and Reinsel (1995, Theorem 3), moving average terms do not affect
  the asymptotic distributions". His statistics are 2[L*(P+1) − L*(P)]
  (λ-max type, sequential; Tables 3, A2), computed with **exact** L*.
* **Yap & Reinsel (1995), Theorem 3** (p. 260): for the **Gaussian
  (conditional) estimator** of Section 3–4 — S = Σ ε̂ₜε̂ₜ′ from recursions with
  fixed initial values ("initial Z₁'s equal zero", p. 255) — the statistic
  −T log|S⁻¹S₀| for H₀: rank(C) = r **vs H_A: rank = m (full, trace type)** has
  limit tr{(∫B_d dB_d′)′(∫B_dB_d′)⁻¹(∫B_d dB_d′)}, B_d a d-dim standard BM —
  the **no-deterministic-term** functional, unaffected by the MA terms. §6: with
  an unrestricted constant μ satisfying Q₁′μ = 0 (no drift) B_d is replaced by
  its demeaned version; YR's Table 1 (from Reinsel–Ahn 1992 Table 1) gives for
  d = 1: 6.59 / 8.16 / 11.65 (10/5/1 %). The paper uses a finite-sample
  correction T → T − m(p+q−1) − d − 1.
* **Johansen (1991), Thm 2.1**: unrestricted μ: trace/λ-max of (2.15) with
  F = (demeaned B, t − ½) if α⊥′μ ≠ 0, and F = B − ∫B if α⊥′μ = 0.
  **Thm 2.2**: μ = αβ₀ (restricted constant): F = (B′, 1)′. Tabulated in
  Johansen–Juselius (1990) and Osterwald-Lenum (1992).

### 4.2 Which table for which drvec case (`johansen_sim.py`, T = 1000, 20 000 reps)

| spec | λ-max g=1 (10/5/1 %) | λ-max g=2 | trace g=2 | source |
|---|---|---|---|---|
| no deterministic term | 3.05 / 4.20 / 7.06 | 9.50 / 11.20 / 15.26 | 10.55 / 12.35 / 16.53 | YR Thm 3; Johansen 1995 T15.1; O-L Table 0 |
| restricted constant | 7.60 / 9.23 / 12.81 | 13.97 / 15.91 / 20.46 | 17.99 / 20.31 / 25.33 | Johansen 1991 Thm 2.2; O-L Table 1* |
| unrestricted constant, zero drift | 6.60 / 8.14 / 11.56 | 13.08 / 15.16 / 19.45 | 15.96 / 18.25 / 22.98 | Johansen 1991 Thm 2.1 (α⊥′μ = 0); YR §6 Table 1; urca `ecdet="none"` |
| drvec `lr_cval_const` | 7.52 / 9.24 / 12.97 | 13.75 / 15.67 / 20.20 | — | = restricted constant ✔ |
| drvec `lr_cval_none` | 6.50 / 8.18 / 11.65 | 12.91 / 14.90 / 19.19 | — | = **unrestricted constant, zero drift** ✘ for case 1 |

* drvec case 1 (E[∇Y₂] = 0, E[W] = 0: nothing deterministic) needs the
  **no-deterministic** table (O-L Table 0 / MHM case 1 / YR Thm 3). drvec prints
  the zero-drift unrestricted-constant one — conservative by a factor ≈ 2 at
  g = 1 (8.18 vs 4.20) and by 3.7 points at g = 2. **CONFIRMED DISCREPANCY**
  (= BUG-24, now also tied to the literature: the drvec table is YR's §6 / Table 1
  table, which is for the constant-included model).
* drvec case 2 = Mauricio case 2 = Johansen Thm 2.2 = O-L Table 1*. **VERIFIED.**
* drvec case 3 (E[∇Y₂] ≠ 0: drift in the common trends) = Johansen Thm 2.1 with
  α⊥′μ ≠ 0 (O-L Table 1; e.g. g=1 λ-max 2.69/3.76/6.65); drvec prints none — fine.
* drvec's statistic 2[L(r+1) − L(r)] is λ-max type and is compared with λ-max
  tables ✔. The sequence ends at the pair (M−2, M−1): it never tests
  H₀: r = M−1 against full rank (P = M), which Mauricio does (Tables 3, A2 second rows) and which YR
  Thm 3 is stated for. drvec cannot fit P = M ("not expressible", run_lrtest
  note), although Mauricio's map covers it (s = 0, C̄ = I, Ȳ = Y, Λ̄ = Π).

### 4.3 Does "MA terms do not affect the distribution" hold for drvec's statistic?

**No — and the reason is not the MA terms but the exact likelihood itself.**

*Theory.* At rank r+1 the extra cointegrating combination W is treated as
stationary and its first value enters through its stationary density, with
variance ∝ 1/(1 − ρ²), ρ the extra eigenvalue of I − B′Λ. At rank r the same
direction is differenced and conditioned on. As Λ → 0 along the extra
direction, ρ → 1 and the rank-(r+1) exact log-likelihood → −∞ (−½log Var(W₁)),
**not** → L(r): rank r is a boundary limit, not a nested submodel (DEMOSTRACIONES
Thm 7(iii) states the undefinedness but draws no consequence for the test).
Under H₀ the fitted ρ̂ = 1 − O_p(1/T), so the initial term contributes
≈ −½ log T per spurious stationary direction and 2[L(r+1) − L(r)] carries a
≈ −log T drift on top of a Johansen-like O_p(1) part. YR's Theorem 3 assumes
the conditional Gaussian likelihood with fixed initial values, where the
nesting holds; it does not cover this statistic. Mauricio's Remark 5 transfers
the tables to exact-likelihood LRs without proof — a gap in the paper.

*Measurement* (`mc/mc_exact_lr.py`, truth: bivariate random walk, r = 0, drvec
`1 0 1 -case 1 -lrtest`, 1000 reps each, compared with the conditional Johansen
λ-max on the same data):

| T | exact LR mean | p10 / p50 / p90 / p95 | P(LR<0) | size at 11.20 (correct 5 % cv) | conditional LR mean, size | mean(exact − cond.) |
|---|---|---|---|---|---|---|
| 100 | 3.04 | −1.40 / 2.49 / 7.96 / 10.12 | 0.216 | 0.039 | 5.57, 0.054 | −2.53 |
| 400 | 1.78 | −2.54 / 1.11 / 6.43 / 8.77 | 0.357 | 0.026 | 5.49, 0.046 | −3.70 |
| 1600 | 0.63 | −4.07 / 0.13 / 5.93 / 8.32 | 0.490 | 0.024 | 5.53, 0.052 | −4.91 |

The conditional statistic is stable at Johansen's law (5 % size at 11.20); the
exact one drifts down ≈ 1.2 per quadrupling of T (≈ 0.87·log T) and its size
falls towards 0. *Genuineness* (`mc/verify_negLR.py`): 8 replications re-fitted
independently (closed-form exact VAR(1) likelihood, 8 starts): drvec's LR equals
the independent one to 10⁻⁴ in all 7 where drvec reported, **including the
negative −0.7778**; one replication drvec failed to fit (nan) where the
independent LR is −0.28. The r = 1 optima are interior (W root 0.96–0.99).

**Verdict: CONFIRMED DISCREPANCY.** Beyond BUG-26 (the misleading "did not
converge" message): the tabulated critical values do not apply to drvec's exact
LR even asymptotically; the test is increasingly undersized (and less powerful)
as T grows, independently of MA terms and of the case-1 table error. "Censor at
0" does not repair the distribution. Repairs: compute the rank LR on the
conditional likelihood (the statistic YR Thm 3 covers) while keeping EML for
estimation; or condition the exact likelihood on the first Ȳ value of the extra
W direction; or keep `-bootstrap` but **retain** the negative draws (currently
discarded in `bootstrap_rank`, which biases its quantiles upward). The HOMOLOGATION
§2.3 "over-rejection ×6 at n = 120" measurement uses a different DGP/case and is
finite-sample; it is not in contradiction, but it cannot be read as the
asymptotic behaviour.

### 4.4 Degrees of freedom / other claims

* drvec LR for a fixed B₂ (`-fixb2 v`): χ²(s·r) ✔ (Mauricio's B = [1,0]′ test:
  1 df ✔). `-fixb2` without value: data-chosen, correctly flagged as not a test.
* AIC/BIC in `run_lrtest`: (−2L + 2K)/n, (−2L + K ln n)/n ✔ (Mauricio's
  convention with the sign typo corrected).

---

## 5. Forecasting in levels (P5, FORECAST.md, Proposition 2)

### 5.1 The inverse map and the level error (derivation)

Ȳₜ = [∇Y₂ₜ; Wₜ], Wₜ = Y₁ₜ + B₂′Y₂ₜ. Given the anchor Y₂ₙ:

    Y₂,ₙ₊ₕ = Y₂ₙ + Σ_{i=1..h} ∇Y₂,ₙ₊ᵢ ,        Y₁,ₙ₊ₕ = Wₙ₊ₕ − B₂′Y₂,ₙ₊ₕ.

With the infinite-past predictor, e_Ȳ(n+i) = Σ_{k<i} Ψₖ A*ₙ₊ᵢ₋ₖ. Collecting the
coefficient of A*ₙ₊ⱼ (j = 1..h, m = h − j):

    e_{Y₂}(n+h) = Σ_j [Σ_{i=j..h} Ψ_{i−j}]_{1..s} A*ₙ₊ⱼ = Σ_j [C_{h−j}]_{1..s} A*ₙ₊ⱼ,   C_m = Σ_{k≤m} Ψₖ
    e_{Y₁}(n+h) = e_W − B₂′e_{Y₂} = Σ_j ([Ψ_{h−j}]_{s+1..M} − B₂′[C_{h−j}]_{1..s}) A*ₙ₊ⱼ

so G_m = [C_m rows 1..s ; Ψ_m rows s+1..M − B₂′ C_m rows 1..s] and
Var(h) = Σ_{m<h} G_m Σ* G_m′ — exactly `level_error_map` and `forecast_vec`.
At h = 1: G₀C̄ = [0 I_s; I_r 0] (G₀C̄A = [A₂; A₁]), so Var(1) is Σ read in the
.inp order — the certificate FORECAST.md prints. ✔

### 5.2 Proposition 2 (DEMOSTRACIONES §6b)

Part 1 (σ(Y₁..Yₙ) = σ(Y₁, Ȳ₂..Ȳₙ)) is correct. Part 2 additionally needs
**Ȳₙ₊ₕ ⊥ Y₁,₁ | Ȳ₂..Ȳₙ** (Y₁ at the first date carries no information beyond
Ȳ): true under the model the exact likelihood encodes (Ȳ stationary from t = 2,
the likelihood conditions only on Y₂,₁), not for a process started at a fixed
Y₀ — state it as an assumption. The code also uses (a) the fitted AS 311
residuals in the MA part of the recursion and (b) the infinite-past MSE
Σ G Σ* G′; both are the standard approximations to the exact finite-sample
predictor (certificate residue 3.5·10⁻⁴ = ξ-truncation, as documented).

### 5.3 Numerical verification (`check_forecast.py`)

drvec (instrumented copy) fitted mink–muskrat `2 1 1 -case 2 -mean -mafree -f 8`
and dumped its transformed system. From it the **VEC parameters were recovered
by inverting Mauricio's map** (F₁C̄⁻¹ from Φ̄₁'s first s columns and −Φ̄₂'s last r
columns; Λ from Φ̄₁; the check Φ̄₂ + F₁C̄⁻¹H̄ = 0 holds to 0.0), and the forecast
was recomputed **directly from VEC (5) in levels** — no Ȳ, no Ψ, no G:

* point forecast, 8 steps: max |diff| 4.2·10⁻⁷ (muskrat), 4.4·10⁻⁷ (mink) — the
  `.out`'s 6-decimal printing;
* bands: Monte Carlo of VEC (5) in levels, 200 000 paths: every s.e. within
  ±0.3 % of drvec's (MC s.e. of an s.d. ≈ 0.16 %), both series, h = 1..8; MC mean
  vs drvec point 0.0013.

**Verdict: VERIFIED** (map, bands, and the Y₁-inherits-cumulated-Y₂-error
structure).

---

## 6. Errors / gaps found — consolidated

**In the paper / AddOn**
1. Published EML log-likelihoods and Σ̂ (Tables 2, 4, 5) are inconsistent with
   the published parameters on the data: L_pub = exact formula with S := nM
   and a Σ̂ 15–18 % too small (§3.5). Invalidates the EML LR 0.9718 and the
   B = [1,0]′ LR 5.4512 (drvec: 7.90, p = 0.005). Severity: high for the
   example's EML-vs-CML narrative; the model itself is not affected.
2. EML Θ̂ non-invertible as printed (Table 2: −1.0063; Table 4: −1.0078; A1:
   θ̂₂₂ = 1.0844; A3: 1.1318), against assumption (1); SEs reported there.
3. Remark 5 / §4: the transfer of Johansen/YR tables to **exact**-likelihood LR
   rank tests is unproven and false as stated (§4.3). YR Thm 3 is for the
   conditional likelihood and a trace-type statistic vs full rank.
4. AIC printed as −(2L*+2K)/N (sign typo); (16) meaningless for p = 1 as
   printed; A1 omits the normalisation condition and the initial-condition
   assumption; the general stationarity proof is replaced by an argument (supplied
   in §1.4).
5. CML column: the printed CML points do not reproduce their own L under three
   start-up conventions — inconclusive because the rule is unstated (OPEN).

**In drvec (none changes the model it estimates)**
1. Case-1 rank critical values are the constant-included table (BUG-24,
   confirmed against Johansen 1991 Thm 2.1/2.2, YR Thm 3 and §6 Table 1, and
   by simulation). Severity: medium (case 1 only; rank under-detected).
2. The exact-likelihood rank LR does not have the tabulated null law at any T
   (drift ≈ −log T); negative LRs are genuine (BUG-26 is the symptom). Severity:
   medium-high for `-lrtest` decisions; the estimator is unaffected.
3. `-lrtest` cannot test r = M−1 vs M (Mauricio's second rows).
4. (A.14) not expressible: no lag mask for Θ/F; invertibility gate at 1.00005
   excludes the paper's EML region.
5. Default `-m 1` likelihood carries ξ-truncation error up to ~10⁻³; the source
   comment calls `-m 2` "approximate" although it is the untruncated value.
   `LOG2PI` truncated to 9 decimals (2.5·10⁻⁸ in logL). Cosmetic.
6. Docs: DRVEC_REFERENCE §1.2 misstates (A.14); HOMOLOGATION §3 labels
   Tables 2/4 as "Table 5"; ANALISIS §5.6 "CML column coherent" not supported
   by evaluation at the CML point; ANALISIS §5.11 "not closable" superseded by
   §3.5 here.

---

## 7. Scripts and artefacts (this directory)

| file | what it does |
|---|---|
| `exactlik.py` | independent exact Gaussian likelihood of a stationary VARMA (Toeplitz + Kalman); the paper's map (10)–(18); Ȳ from levels (17) |
| `drvec_copy/` | drvec 0.10 source copy; patched only with `DRVEC_X` (inject x in `-eval`), `DRVEC_DUMP`, `DRVEC_FDUMP` |
| `check_A1.py` | §1.3 |
| `check_general.py [-m 2]` | §1.3 general case |
| `check_polyid.py` | §1.4 operator identity |
| `check_A2.py` | §2.1 |
| `published.py`, `check_published.py` | the tables; IC, LR, eigenvalues, identities, conditional likelihood at published points |
| `table4.py` | §3.3 |
| `shift_test.py`, `cml_orient.py`, `cml_opt.py`, `cml_local.py` | §3.4, §3.7 |
| `sigma_hyp.py` | §3.5 the accounting identity |
| `sigma_mech.py` | §3.5 the rejected mechanism |
| `run/mm_free.out`, `run/mm_fix.out` | §3.6 LR for B = [1,0]′ |
| `johansen_sim.py`, `johansen_sim.log` | §4.2 |
| `mc/mc_exact_lr.py`, `mc/mc_T*.log`, `mc/verify_negLR.py` | §4.3 |
| `check_forecast.py` | §5.3 |
