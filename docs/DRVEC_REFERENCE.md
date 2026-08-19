# drvec — VEC EML Estimation (Mauricio 2006)

> **Status**: superseded in part. Written 2026-07-21, while the `Q` normalisation
> was still being debugged; that question was settled on 2026-08-17 (`Q[1][1] = 1`,
> see [MODEL.md](MODEL.md) §3). This document is kept for its derivation of the
> transformation and its notes on the paper and the AddOn, which remain accurate.
> For the current state start at [README.md](README.md).
> **Date**: 2026-07-21

---

## 1. Theoretical Foundation

### 1.1 Mauricio (2006) — The Key Paper

**Reference**: Mauricio, J.A. (2006). "Exact maximum likelihood estimation of partially nonstationary vector ARMA models." *Computational Statistics & Data Analysis*, 50, 3644–3662.

**Core insight**: A partially nonstationary VARMA in VEC form can be transformed into a **standard stationary VARMA** via a simple linear transformation. This allows EML estimation using existing algorithms (Mauricio 1995, 1997; Shea 1989).

#### The Mauricio Transformation

Starting from the VEC model:

$$\nabla Y_t = -\Pi Y_{t-1} + \sum_{i=1}^{p-1} F_i \nabla Y_{t-i} + \Theta(L) A_t$$

with $\Pi = \Lambda B'$ of rank $r$, and normalization $B = [I_r, B_2']'$:

Define the transformation matrices:

$$\bar{C} = \begin{pmatrix} 0 & I_{M-r} \\ I_r & B_2' \end{pmatrix}, \quad
\bar{C}^{-1} = \begin{pmatrix} -B_2' & I_r \\ I_{M-r} & 0 \end{pmatrix}, \quad
\bar{H} = \begin{pmatrix} 0 & 0 \\ 0 & I_r \end{pmatrix}$$

Then $\bar{Y}_t = (\nabla Y_{2t}', W_t')'$ where $W_t = Y_{1t} + B_2' Y_{2t}$ is **stationary** and follows a VARMA:

$$\bar{\Phi}_0 \bar{Y}_t = \sum_{i=1}^p \bar{\Phi}_i \bar{Y}_{t-i} + \Theta(L) A_t$$

where the $\bar{\Phi}_i$ are explicit functions of $\Lambda, F_i, B_2$ (equations 15-16 in the paper):

$$\begin{aligned}
\bar{\Phi}_0 &= \bar{C}^{-1} \\
\bar{\Phi}_1 &= \bar{C}^{-1}\bar{H} - \bar{\Lambda} + F_1\bar{C}^{-1} \\
\bar{\Phi}_i &= F_i\bar{C}^{-1} - F_{i-1}\bar{C}^{-1}\bar{H} \quad (i=2,\dots,p-1) \\
\bar{\Phi}_p &= -F_{p-1}\bar{C}^{-1}\bar{H}
\end{aligned}$$

where $\bar{\Lambda} = [0_{M \times (M-r)}, \Lambda]$.

After normalization (premultiplying by $\bar{C} = \bar{\Phi}_0^{-1}$):

$$\bar{Y}_t = \sum_{i=1}^p \Phi_i^* \bar{Y}_{t-i} + \bar{C}\Theta(L)\bar{C}^{-1} A_t^*$$

with $\Phi_i^* = \bar{C}\bar{\Phi}_i$ and $A_t^* = \bar{C}A_t$.

#### Key Properties

1. **Stationarity**: The VARMA on $\bar{Y}_t$ is stationary by construction
2. **Uniqueness**: The transformation is uniquely identified given the normalization $B = [I_r, B_2']'$
3. **Joint estimation**: All parameters ($\Lambda, F_i, \Theta_j, B_2, \Sigma$) estimated simultaneously
4. **Triangular structure**: $\Phi_1^*$ has its first $M-r$ columns zero — this is the structural restriction the VEC imposes

#### LR Test for Cointegration Rank (Remarks 5-6, Section 4)

Mauricio shows that:
- Standard LR tests for cointegration rank apply in this EML framework
- **MA terms do NOT affect the asymptotic distribution** (Yap & Reinsel 1995, Theorem 3)
- Critical values from MacKinnon et al. (1999) can be used directly
- Three deterministic cases (Remark 6):
  - Case 1: $E[\nabla Y_2]=0, E[W]=0$ (no drift, zero-mean equilibrium)
  - Case 2: $E[\nabla Y_2]=0, E[W]\neq 0$ (no drift, non-zero equilibrium)
  - Case 3: $E[\nabla Y_2]\neq 0, E[W]\neq 0$ (restricted drift)

### 1.2 Mauricio AddOn (JAM106)

**Reference**: Mauricio, J.A. (2005). "Additional material on exact maximum likelihood estimation of partially nonstationary vector ARMA models."

Contains:
- **A1**: Explicit stationary VAR(1) derivation for bivariate case ($M=2, r=1$)
- **A2**: Census Housing data example (Hillmer & Tiao 1979) with full EML vs CML comparison
- Key result: EML reveals non-invertible seasonal MA structure that CML misses

**Census Housing Data** (Hillmer & Tiao 1979, JASA 74, 652-660):
- Monthly, Jan 1965 – May 1975 (125 obs raw, 112 effective after seasonal differencing)
- Housing starts ($x_{1t}$) and houses sold ($x_{2t}$), in thousands
- Seasonally differenced: $y_t = (1-B)(1-B^{12})x_t$
- Model (A.14): $\nabla y_t = \Lambda B' y_{t-1} + (I - \Theta_1 L) A_t$

**EML estimates (Table A3)**:
- $\Lambda = [0.5191, -0.1085]'$
- $B = [1, -1.8625]'$ → cointegration: $y_{1t} \approx 1.8625 \cdot y_{2t}$
- $\Theta_1 = \text{diag}(0.9600, 1.1318)$ — note $\theta_{22} > 1$ (non-invertible!)
- $\Sigma = \begin{pmatrix} 29.4526 & 5.7112 \\ 5.7112 & 9.9830 \end{pmatrix}$
- $\log L = -664.24$, AIC = 12.00

**LR tests (Table A2)**:
- $H_0: P=0$ vs $H_1: P=1$: LR = 59.22, $p < 0.01\%$ → cointegration confirmed
- $H_0: P=1$ vs $H_1: P=2$: LR = 1.55, $p = 24.95\%$ → cannot reject single cointegrating vector

### 1.3 Hillmer & Tiao (1979) — Original Data Source

**Reference**: Hillmer, S.C. and Tiao, G.C. (1979). "Likelihood function of stationary multiple autoregressive moving average models." *JASA*, 74(367), 652-660.

- Original source of the Census Housing data
- Introduces the exact likelihood for multivariate MA models
- Bivariate model (Table 2): $(I - \Phi B) y_t = (I - \Theta B)(I - \Psi B^{12}) a_t$
- EML approximation $\tilde{\ell}^*$ reveals near-unit-root seasonal MA that CML misses
- Implication: $z_{2t}$ (houses sold) is nonstationary with deterministic seasonality; $z_{1t}$ depends on $z_{2,t-1}$

---

## 2. Project Architecture

### 2.1 Directory Structure

```
SRC/drvec/
├── src/
│   ├── drvec.c          ← VEC frontend (vec_shootx, init_guess, main)
│   ├── elfvarma.c       ← Mauricio's AS 311 exact log-likelihood (untouched)
│   ├── drvmlest.c       ← estimation driver (untouched)
│   ├── qnewtopt.c       ← factored BFGS quasi-Newton (untouched)
│   └── nlatools.c       ← linear algebra / allocators (one allocation fix)
├── include/
│   └── main.h           ← Tvarma struct, function prototypes
├── literature/
│   ├── Mauricio.pdf     ← Mauricio (2006) CSDA paper
│   ├── 518-2013-11-11-JAM106-AddOn.pdf  ← Mauricio AddOn
│   └── Hillmer-LikelihoodFunctionStationary-1979.pdf
├── data/
│   ├── synth.inp        ← Synthetic cointegrated data (β=0.8, for validation)
│   ├── housing_v0.inp   ← Simulated VEC (no MA) from Mauricio Table A3 params
│   ├── housing_v1.inp   ← Simulated VEC (stationary, no MA)
│   ├── housing_mauricio.inp ← Simulated VEC with MA(1) (non-invertible)
│   ├── AL_pre1810.inp   ← Real Utrecht-London wheat prices, 1701-1810
│   ├── ARLL.inp         ← Real Arnhem-London, 1701-1813
│   ├── VILL.inp         ← Real Vienna-London, 1701-1813
│   ├── SLL.inp          ← Real Strasbourg-London, 1701-1813
│   └── PLL.inp          ← Real Pennsylvania-London, 1721-1813
├── docs/
│   ├── LEGACY_NOTES.md  ← Solutions rescued from drv_project
│   └── DRVEC_REFERENCE.md ← This file
├── Makefile
├── README.md
└── .gitignore
```

### 2.2 Data Format (.inp)

```
* comments (optional)
1                               ← frequency (1=A, 4=Q, 12=M)
2 110 1 1701                    ← M nobs start_sub start_year
Arnhem London                   ← M series names
1.0 0 0                         ← Box-Cox lambda, d, D
<data: nobs rows, M columns>   ← col 1..s = ∇Y_{2t}, col s+1..M = Y_{1t} levels
```

### 2.3 Parameter Vector Order (Mauricio Parametrization)

For VEC with cointegration rank $r$:

```
x = [μ₁ ... μ_M,              ← mean of Ȳ_t (if -mean)
     λ_{1,1} ... λ_{M,r},      ← Λ (M×r adjustment matrix)
     vec(F_1), ... vec(F_{p-1}), ← F_i (M×M short-run, i=1..p-1)
     vec(Θ_1), ... vec(Θ_q),   ← Θ_j (M×M MA, j=1..q)
     vech(Σ),                   ← Σ lower triangle
     b_{1,1} ... b_{s,r}]      ← B_2 (s×r cointegration, s=M-r)
```

### 2.4 CLI

```
drvec file p q r [-mean] [-case 1|2|3] [-diagar] [-diagma] [-diagcov] [-m 1|2]
```

---

## 3. Validation Results

### 3.1 Synthetic Data (β = 0.8)

**Data**: 99 obs, bivariate, β_true = 0.8, no MA, lambda = [0.52, -0.11]

| Version | B₂ estimated | Status |
|---------|-------------|--------|
| Free VARMA parametrization | −0.800 ± 0.022 | ✓ CONVERGED |
| Mauricio parametrization | — | ✗ ifault=1 (Q bug) |

### 3.2 Real Wheat Data — VAR(1), Pre-1810

All pairs with London as numeraire, p=1, q=0, -mean:

| Pair | $n$ | $\hat{\beta}$ | $\hat{\alpha}_1$ (city) | $\hat{\alpha}_2$ (London) | $\log L$ | AIC |
|---|---|---|---|---|---|---|
| **Arnhem** | 112 | 1.07 | −1.21 | −0.12 | −42.0 | 0.93 |
| **Vienna** | 112 | 0.85 | −1.15 | **−0.03** ✗ | −9.4 | 0.35 |
| **Strasbourg** | 112 | 0.71 | −1.09 | **+0.04** ✗ | −4.2 | 0.25 |
| **Pennsylvania** | 92 | 1.06 | −0.89 | +0.10 | −1.6 | 0.25 |

**Key finding**: London is weakly exogenous ($\alpha_2 \approx 0$) for Vienna and Strasbourg — consistent with the BVECM article.

### 3.3 Comparison with Cycles FUE Univariate Ratios

| Pair | VEC $\hat{\beta}$ | FUE ratio mean $\mu$ | $e^\mu$ |
|---|---|---|---|
| Arnhem | 1.07 | −0.42 | 0.66 |
| Vienna | 0.85 | −0.75 | 0.47 |
| Strasbourg | 0.71 | −0.43 | 0.65 |
| Pennsylvania | 1.06 | −0.47 | 0.63 |

The FUE ratio **imposes** $\beta=1$ by construction. The VEC finds $\beta \neq 1$ for Vienna (0.85) and Strasbourg (0.71), demonstrating the advantage of the multivariate approach.

### 3.4 VARMA(2,1) with Moment-Based MA Init

Using the legacy method (autocorrelation-based MA(1) θ estimate):

| Pair | $\hat{\beta}$ | $\log L$ | AIC | vs VAR(1) |
|---|---|---|---|---|
| Arnhem | 0.29 | −21.8 | 0.71 | worse |
| Vienna | **0.82** | 11.1 | 0.12 | better |
| Strasbourg | 0.33 | 14.6 | 0.06 | better AIC, wrong β |
| Pennsylvania | **1.01** | 27.7 | −0.21 | better |

Vienna and Pennsylvania converge to correct values with VARMA(2,1). Arnhem and Strasbourg get stuck in local optima — need better initialization or convergence operator.

### 3.5 Utrecht-London Pre-1810 (Real Levels)

Using log-level data from Cycles study, 1701-1810:

- $n = 110$, VAR(1)
- $\hat{B}_2 = -0.647$ → $\hat{\beta} = 0.647$
- $\log L = 63.6$, AIC = −0.975
- Interpretation: Utrecht prices ~65% of London in equilibrium (consistent with transport costs)

---

## 4. Legacy Code Solutions Documented

### 4.1 The Triangular `shootx()` Pattern

From `drv_project/src/main.c`:
- `shootx` rebuilds data matrix `armax->w` on every optimizer call
- Extracts β first, constructs $w_t = z_{1t} - \beta z_{2t} - c_t$
- Key pattern: **data depends on parameters** — not fixed before estimation

### 4.2 BEC Formulas

For bivariate ($M=2, r=1$):

| Parameter | Formula |
|-----------|---------|
| $\alpha_1(k)$ | $\phi_{11}(k) - \delta_{k1} + \beta \cdot \phi_{21}(k)$ |
| $\alpha_2(k)$ | $\phi_{21}(k)$ |
| $\gamma_{12}(k)$ | $\beta \cdot \phi_{22}(k) + \phi_{12}(k)$ |
| $\gamma_{22}(k)$ | $\phi_{22}(k)$ |

Where $\Phi$ are estimated VAR coefficients on $\bar{Y}_t = (\nabla Y_{2t}, W_t)$.

### 4.3 Moment-Based MA(1) Initialization

From `drv_project/src/main.c:328-365`:
- Compute $\rho_1 = \text{Corr}(u_t, u_{t-1})$ from VAR residuals
- Solve $\theta$ from $\rho_1 = -\theta/(1+\theta^2)$
- Pick invertible root ($|\theta| < 1$)
- Much simpler and more robust than Hannan-Rissanen for small samples

### 4.4 Convergence Operator Pre-Estimation

- β and μ by OLS of $z_{1t}$ on $z_{2t}$
- Grid search: $\phi = 0.00, 0.01, \dots, 0.99$, compute $\sum \phi^k X_{t-k}$
- Pick $(\alpha, \phi)$ minimizing RSS
- VAR on $(w_t, \Delta z_{2t})$ by OLS

### 4.5 MA Diagonal Structure

The legacy enforces $\theta_{11}=\theta_{12}=\theta_{21}=0$, only $\theta_{22}$ free. This is specific to the triangular form where MA only corrects the $\Delta z_2$ equation.

---

## 5. Implementation Status

### Completed ✓

| Item | Status |
|---|---|
| Motor EML (elfvarma, drvmlest, qnewtopt) | Copied from drvarma v.04.1, untouched |
| nlatools | Copied from drvarma v.04.1; one allocation defect corrected in `tensor()`, which changes no computed value (SUITE_INTEGRATION.md §5) |
| `vec_shootx()` — free VARMA version | Working, validated on synth |
| `vec_shootx()` — Mauricio parametrization | Implemented, has Q bug |
| `calc_nparametrs()` — Mauricio version | Done |
| `init_guess()` — OLS B₂ + moment MA | Working for q=0, partial for q=1 |
| CLI argument parsing | Done |
| Data I/O (.inp format) | Done |
| Moment-based MA(1) initialization | Implemented (from legacy) |
| Lambda extraction from OLS VAR | Implemented |

### Pending

| Item | Priority |
|---|---|
| Fix Q normalization bug in Mauricio vec_shootx | High |
| BEC transform ($\alpha, \Gamma$ from $\Phi$) | High |
| Wald tests (weak exogeneity, joint) | High |
| Convergence operator extension | Medium |
| LR test with MacKinnon critical values | Medium |
| VEC parameter recovery ($\Lambda, \Gamma_i$ from $\bar{\Phi}$) | Medium |
| Convergence statistics ($g, \tau, l$, half-life) | Low |
| Economic interpretation output | Low |
| Hannan-Rissanen proper init for q>1 | Low |

---

## 6. Key Lessons Learned

1. **The free VARMA parametrization works but is not 100% Mauricio.** Mauricio's transformation imposes zero restrictions on $\Phi_1^*$ (first $M-r$ columns are zero). Estimating a free VARMA on $\bar{Y}_t$ ignores these restrictions, leading to overparameterization.

2. **The Q normalization bug is numerically subtle.** The transformation $\bar{C}\Sigma\bar{C}'$ is mathematically sound but can produce near-singular Q when $\Sigma$ is initialized from VAR residuals with strong cross-correlation.

3. **Non-invertible MAs are real.** The Census Housing data has $\theta_{22}=1.1318 > 1$. The EML engine can handle this (unlike conditional ML), but the initialization must respect invertibility constraints.

4. **The convergence operator is structurally necessary.** Without it, even pre-1810 wheat data shows weak cointegration. The legacy code includes it by default.

5. **The Cycles study provides validated univariate benchmarks.** FUE ARIMA models for London and the ratios give starting values that should inform the VEC initialization.

---

## 7. References

- Hillmer, S.C. & Tiao, G.C. (1979). *JASA*, 74(367), 652-660.
- Mauricio, J.A. (1995). *JASA*, 90(429), 282-291.
- Mauricio, J.A. (1997). *Applied Statistics*, 46(1), 157-171.
- Mauricio, J.A. (2006). *CSDA*, 50, 3644-3662.
- Yap, S.F. & Reinsel, G.C. (1995). *JASA*, 90(429), 253-267.
- MacKinnon, J.G., Haug, A.A. & Michelis, L. (1999). *J. Appl. Econom.*, 14(5), 563-577.
- García-Hiernaux, A. & Guerrero, D.E. (2021). *Economic Modelling*, 104, 105641.
