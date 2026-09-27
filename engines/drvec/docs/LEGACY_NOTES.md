# Legacy notes — solutions rescued from drv_project

This document catalogues the key design decisions and formulas from the legacy
`drv_project` codebase that informed `drvec`.  The goal is to document *what* was
learned so we don't reinvent it, while noting *why* we didn't simply copy it.

---

## 1. The triangular `shootx()` pattern

**Legacy solution** (`drv_project/src/main.c:shootx`, lines 1544–1882):
- On every optimizer call, `shootx` rebuilds the data matrix `armax->w` from
  the current parameter vector `x[]`.
- The cointegration parameter $\beta$ (and convergence params) are extracted
  first, then $w_t = z_{1t} - \beta z_{2t} - c_t$ is constructed, then
  `armax->w[t][1] = w_t`, `armax->w[t][2] = \Delta z_{2t}`.
- This is the key pattern: **data is not fixed — it depends on parameters**.

**What drvec does**: `vec_shootx()` follows the same pattern. The difference is
that `drvec` handles general $M$ and $r$, using the Mauricio (2006)
transformation $\bar{Y}_t = (\nabla Y_{2t}', W_t')'$ instead of hardcoding
$M=2$.

---

## 2. BEC formulas (mapping VARMA $\Phi$ → VEC $\alpha, \Gamma$)

**Legacy solution** (`drv_project/src/main.c:analisis_BEC`, lines 455–620):

For the bivariate case ($M=2, r=1$):

| Parameter | Formula |
|-----------|---------|
| $\alpha_1(k)$ | $\phi_{11}(k) - \delta_{k1} + \beta \cdot \phi_{21}(k)$ |
| $\alpha_2(k)$ | $\phi_{21}(k)$ |
| $\gamma_{12}(k)$ | $\beta \cdot \phi_{22}(k) + \phi_{12}(k)$ |
| $\gamma_{22}(k)$ | $\phi_{22}(k)$ |

Where $\delta_{k1} = 1$ if $k=1$, else $0$. The $\Phi$ matrices are the
estimated VAR coefficients on $\bar{Y}_t$.

**What drvec does**: These formulas are **not yet implemented** in drvec.
They will go into `bec_transform()` (future).  For general $M, r$, the
generalisation follows directly from the proof in the article's appendix:

$$\mathbf{A} = \sum_{l=1}^{p^*} \mathbf{M}_l, \quad
  \boldsymbol{\Gamma}_i = -\sum_{l=i+1}^{p^*} \mathbf{M}_l \boldsymbol{\alpha}'$$

with $\mathbf{M}_l$ defined in BVECM_models_proofs.tex.

---

## 3. Convergence operator pre-estimation

**Legacy solution** (`drv_project/src/main.c:preestimar_parametros`, lines 78–449):
- $\beta$ and $\mu$ estimated by OLS of $z_{1t}$ on $z_{2t}$
- Convergence operator $\nu(B) = \alpha/(1-\phi B)$ pre-estimated by **grid
  search**: try $\phi = 0.00, 0.01, \dots, 0.99$, for each compute the
  convolution $\sum_{k=0}^K \phi^k X_{t-k}$, and pick the $(\alpha, \phi)$ that
  minimises RSS.
- VAR coefficients on $(w_t, \Delta z_{2t})$ initialised by OLS.

**What drvec does**: `init_guess()` (lines 132–230 of drvec.c) uses a simpler
initialisation: OLS on $\bar{Y}_t$ for the VAR coefficients, zero for MA and
$B_2$.  The convergence operator is **not yet in drvec** (this is part of the
future `drvec_bvec` extension).

**Note on the grid search**: The legacy grid search with 100 $\phi$ values and
$K=20$ lags is inefficient. A better approach would use `scipy.optimize.minimize_scalar`
(Python) or a golden-section search (C).  We can improve this when we implement
the convergence operator.

---

## 4. MA diagonal structure

**Legacy solution** (`drv_project/src/main.c:1676–1681`):
The MA matrix is forced to be diagonal with only $\theta_{22}(1)$ free:
```c
theta1[1][2][2] = x[idx++];  /* θ₂₂(1) - only this one is free */
theta1[1][1][1] = 0.0;       /* θ₁₁(1) = 0 */
theta1[1][1][2] = 0.0;       /* θ₁₂(1) = 0 */
theta1[1][2][1] = 0.0;       /* θ₂₁(1) = 0 */
```

This is specific to the triangular form where the MA only corrects the $\Delta z_2$
equation. The article should document this restriction explicitly.

**What drvec does**: The MA structure is configurable via `-diagma` flag.
By default, the full MA matrix is estimated. For the triangular form with MA
correction, use `-diagma` and set only the relevant element free (future).

---

## 5. Wald tests on adjustment coefficients

**Legacy solution** (`drv_project/src/main.c:calcular_chi2_weakex`,
`calcular_chi2_joint`, lines ~3060–3393):
- Delta method from $\Phi$ covariance to $\alpha$ covariance
- SVD-based pseudoinverse for the Wald statistic
- Tests: joint (all $\alpha=0$) and per-variable (weak exogeneity)

**What drvec does**: Not yet implemented. The mathematics is sound and can be
ported directly when we add the BEC layer.

---

## 6. Convergence statistics ($g, \tau, l$)

**Legacy solution** (`drv_project/src/main.c:calcular_g_convergencia` ff):
- $g = \alpha/(1-\phi)$ (long-run gain)
- $\tau = \mu + g$ (final equilibrium gap)
- $l = 1/(1-\phi)$ (mean lag)
- Standard errors via delta method
- $p$-values via normal approximation

**What drvec does**: Not yet implemented. Will be added in the convergence
operator extension.

---

## 7. Data format

**Legacy solution**: Hardcoded 4 columns: `dL, A, L, X` (differenced London,
Arnhem level, London level, dummy). Variable names `A` and `L` hardcoded.

**What drvec does**: Uses the standard drvarma `.inp` format with configurable
series names. Columns 1..s are $\nabla Y_{2t}$ (pre-differenced), columns
s+1..M are $Y_{1t}$ (levels).

---

## 8. What we deliberately did NOT carry over

1. **Hardcoding to $M=2$**: drvec handles arbitrary $M$ and $r$.
2. **Global variables for model config**: drvec uses the same pattern (globals)
   for compatibility with the engine, but the VEC-specific globals (`global_r`,
   `global_case`) are cleanly separated.
3. **`exit(1)` on errors**: The legacy code calls `exit(1)` liberally. drvec
   reports errors via `fprintf(stderr, ...)` and `ifault` codes where possible.
4. **gnuplot graphics**: drvec does not link gnuplot. Diagnostics will be added
   later via the `diagnose.c` module (from drvarma).
5. **Spanish comments / variable names**: All new code is in English.
6. **Numerical Recipes dependency**: The engine was already cleaned of NR
   dependencies in drvarma v.04.1; drvec inherits this.
