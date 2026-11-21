# drvarma — Developer Guide

This document describes the internal architecture of drvarma for contributors.

## 1. Overview

drvarma fits a general **VARMA(p,q)** model

    Φ(B) (w_t − μ) = Θ(B) a_t,   a_t ~ N(0, Σ),

to a vector series `w_t` obtained from the raw data by Box-Cox transform,
regular/seasonal differencing and optional harmonic seasonal adjustment.
Estimation is **exact Gaussian maximum likelihood** via a factorized BFGS
quasi-Newton optimiser.

The codebase is C (C99), 1-based indexing throughout, with `real` = `double`
(see `include/main.h`). Heavy linear algebra uses the **GNU Scientific Library
(GSL)**; there is **no Numerical Recipes code** (see `NR_REMOVAL_PLAN.md`).

## 2. Source layout

| File | Responsibility |
|------|----------------|
| `src/escalera.c` | **The ladder (5.0)**: one fue file per series; the gate, the cross orders, `-links`/`-zero`, the starts (`-start zero\|ols`: `start_ols`, the original `init_varma` in the ladder), `-lik`, `-irfboot`, forecasts and `-estwin`. It reads and casts the files with `lib/fuepre`, `lib/prewhiten` and `lib/fuedet`. Its architecture, and the homologation with the Python port: drvarma-python `docs/ARCHITECTURE.md`. |
| `src/split.c`, `src/inpread.c` | `-split` (the multivariate `.inp` → one fue file per series) and the `.inp` reader. |
| `src/drvarma.c` | `main`: CLI parsing, I/O, and the whole **deprecated `.inp`** pipeline (it hands the ladder's command lines to `escalera_main`). Defines `shootx()` (maps the parameter vector ↔ `struct Tvarma`, builds residuals) and the starting-value routines (`init_varma`, `hannan_rissanen_diag`). |
| `src/drvmlest.c` | `est()` — estimation driver: prepares the problem and calls the optimiser. |
| `src/qnewtopt.c` | Factorized BFGS quasi-Newton optimiser (J.A. Mauricio). Owns the convergence criteria (gradtol/steptol) and the final convergence report. |
| `src/elfvarma.c` | `elf()` — the exact log-likelihood of a VARMA(p,q); `chekma()` — MA invertibility check via eigenvalues. |
| `lib/lik/lik.c`, `lib/lik/multshea.c` | `varma_lik` (elf, or Shea's AS 242 with `-lik`), shared with drtran (drvec calls `elf` directly). |
| `lib/optim/lnsrch.c` | The line search, one copy for drvarma, drtran, drvec, fue and fuf (and `gui/sim`). |
| `lib/diagmv` | The multivariate residual diagnosis (Hosking's Q with its degrees of freedom, Lütkepohl's Jarque–Bera), shared with drtran and drvec. |
| `lib/fuepre`, `lib/prewhiten`, `lib/fuedet`, `lib/dates`, `lib/inpcheck` | fue's file: the reader, the univariate cast (Box-Cox, differences, the factor expansions), the deterministic filter, dates, the validator (with `engines/fue/src/inpcheck.c`). Shared with drtran and the GUIs; a change there moves them too. |
| `src/nlatools.c` | Linear algebra + memory + strings: LU (`ludcp`/`lusol`), Cholesky (`choldcp`/`cholfor`/`cholbak`/`cholsol`), eigenvalues (`eigenqr`, **GSL**), SVD (`svdcp`/`svsol`, **GSL**), allocators (`vector`/`matrix`/`tensor`/`free_*`), `nrerror`, `pythag`, string utils. |
| `src/diagnose.c` | Residual diagnostics (summary, ACF/PACF; the multivariate tests are `lib/diagmv`'s), Wald hypothesis tests, impulse responses and FEVD. **`gui/sim` compiles this file and `nlatools.c`**: a change here changes sim_gui. |
| `src/forecast.c` | `forecast_model()` (point forecasts + error variances, supports an origin offset `b`), `forecast_level_variances()`. |
| `src/transform.c` | `transform_series()` (Box-Cox + differencing → stationary matrix) and `integrate_forecast()` (invert differencing + Box-Cox), for the `.inp` path: (y^λ − 1)/λ also at λ = 1. The ladder uses fue's transform (`lib/prewhiten`), refactor·y at λ = 1. |
| `src/deseason.c` | `deseasonalize_raw()` — harmonic seasonal adjustment on the differenced basis; returns level dummies for re-seasonalising forecasts. |
| `src/volatility.c` | Conditional volatility (exponential / moving window). |
| `include/main.h` | Core types (`real`, `struct Tvarma`, `struct Tseries`) and most prototypes. |
| `gui/seasonal_detection.c` | The harmonic seasonal detection of `-deseason auto`, compiled into the engine. The GTK front-end was retired (2026-11-06); the window is `gui/sim`. |

## 3. Core data structures (`include/main.h`)

```c
struct Tvarma {           /* a normalized VARMA model */
    int m, n, p, q;       /* series, obs, AR order, MA order */
    real *mu;             /* mean vector            [1..m]            */
    real ***phi, ***theta;/* AR/MA matrices         [0..p][1..m][1..m]*/
    real **qq;            /* normalized covariance   [1..m][1..m]     */
    real **w, **a;        /* data and residuals      [1..n][1..m]     */
    real sigma2, logelf, xitol;
};
```

`struct Tseries` holds a single (univariate) series with its summary statistics,
used by the diagnostics/plot routines.

## 4. Estimation pipeline (`main` in `drvarma.c`)

1. **Parse CLI** → global flags (`global_p/q`, `global_diag_*`, `global_include_mean`,
   `trans_lambda/scale/d/D`, `do_deseason`, `forecast_horizon`, `g_estwin`, …).
2. **Read `.inp`** (raw levels).
3. **`deseasonalize_raw()`** (optional) → seasonal dummies subtracted from levels.
4. **`transform_series()`** → `datamat` (the stationary `w`), plus `bc_series`
   (Box-Cox levels, kept to integrate forecasts back).
5. **`calc_nparametrs()`**, **`init_varma()`** (OLS-based starting values;
   `hannan_rissanen_diag()` if `-twostep`).
6. **`est()`** drives **`qnewtopt`**; at each trial parameter vector it calls
   **`shootx()`** to rebuild the `Tvarma` model and **`elf()`** to evaluate the
   exact log-likelihood (Cholesky factorisations in `multshea`/`elfvarma`).
7. **Reporting**: `print_parameters`, `all_hypothesis_tests` (Wald),
   `impulse_response` + `variance_decomposition`, `multivariate_diagnostics`
   (Q, JB), `print_roots` (eigenvalues via `eigenqr`/GSL), `diagnose`.
8. **Forecasting** (if `-forecast`): `forecast_model` → `integrate_forecast`
   → re-seasonalise → `file.forecast`. With `-estwin`, a second loop produces
   `file.recursive` (see §6).

### `shootx()` (parameter ↔ model mapping)

`est()` is passed `shootx` as the objective hook. `shootx(x, &varma, &ifault,
firstx, lastx)` unpacks the flat vector `x` into `varma.mu/phi/theta/qq` honoring
the diagonal flags, copies `datamat` into `varma.w`, and (with the likelihood
code) computes residuals. `firstx`/`lastx` allocate/free the model buffers.

## 5. Conventions and gotchas

- **1-based indexing**: `vector(1,n)` yields `v[1..n]`; `matrix(1,m,1,n)` yields
  `m[1..m][1..n]`. Allocators (in `nlatools.c`) size `[0..nh]` with `calloc` and
  index from `nl`. Matrices are **not assumed tightly contiguous**; always access
  as `m[i][j]` (the GSL wrappers copy element-by-element, so layout-agnostic).
- **`real`** is `double`; keep new code type-generic via `real`.
- **Globals**: dimensions (`nser`, `nobs`, `nobs_raw`), orders and transform
  parameters are file-scope globals in `drvarma.c` and `extern` elsewhere. The
  estimation window for `-estwin` uses `g_estwin` / `g_in_est` (the latter is set
  only while the optimiser runs, so `shootx` restricts the likelihood to the
  training window without shrinking buffers).
- **Scaling**: `trans_scale` (default 100) multiplies the series after Box-Cox.
  Coefficients (Φ/Θ) are scale-invariant; `μ` scales by the factor and `Σ` by its
  square. *Standard errors of cross terms can be ill-conditioned when series have
  very different variances* (e.g. WTI vs CPI) — see the caveat in `MODELS_RESULTS.md`.

## 6. Fixed-parameter recursive forecasting (`-estwin`)

For `q = 0`, `forecast_model()` with an origin offset `b` forecasts from
observation `n − b` using only `w` up to that origin (the MA branch, which needs
residuals, is skipped). `main` therefore: estimates once on `g_estwin`
observations, then loops `b` from `nobs − g_estwin` down to 0, integrating each
forecast anchored at the corresponding raw index and re-seasonalising with the
training-window dummies. The deseasonalisation is also restricted to the training
window for a clean fixed-parameter design.

## 7. Build system

`Makefile` builds `bin/drvarma`. GSL is located via `pkg-config`. Object files go to `build/`. Cross-compilation
to static Windows binaries is supported via MXE (`make CROSS=…`).

## 8. Linear algebra / GSL boundary

`nlatools.c` holds the GSL-backed routines (`gui/seasonal_detection.c` also
includes GSL). Public signatures (`eigenqr`, `svdcp`, `svsol`)
are unchanged from the historical (NR-based) versions, so the rest of the code is
agnostic to the backend. When adding numerical routines, prefer GSL and keep the
1-based wrapper convention used in `nlatools.c`.

## 9. Testing / regression

`sh tests/run_tests.sh` runs three batteries (`banco`, `escalera` and the
reader of the ladder's output, `lib/outvarma`); `make check` at the monorepo
root runs it with every other engine and GUI, and it is what a change in this
engine is checked with: `gui/sim`, `gui/atsw-cli`, `lib/partida` and the MCP
tests drive drvarma and pin its numbers. `tests/rutas/rutas.sh` is the fourth
battery, long, outside `make check`. The rules are in the repository's
`docs/MOTORES.md`.

- `tests/banco/banco.sh`: 18 `.inp` cases over `data/` (the Group 1
  trivariate, the WTI+CPI pass-through, IPC, PSW), covering the diagonal and
  full models, `-m 2`, `-twostep`, `-volexp`, `-deseason`, `-forecast` and
  `-estwin`. The reference `tests/banco/ref/` pins the `.inp` path's output byte for byte, except the `Program` line and the deprecation note. It started as the 0.4.1 output and has been regenerated on purpose with every deliberate change of that output (the fdhess standard errors, Shea's likelihood, the MA wall report, the line search's objective, the shared multivariate diagnosis): `git log -- tests/banco/ref`.
  `--generar` rewrites it, and should only be run when a change of output is
  deliberate. The `.out` files kept in `data/` come from older binaries (scale
  1) and are not a reference.
- `tests/escalera/pruebas.sh`: the ladder. It makes explicit assertions (the
  gate, σ² and coefficients against fue's `.out`, genuine `.pre` files as
  fixed points, BUG-2, alignment at the end, rejected options, the
  pass-through, the line search on NaN/∞) and a byte regression of the 5.0
  output with the path and version lines filtered.
- `tests/rutas/rutas.sh`: the routes U and V. The US monetary VARs by both
  routes, the echelon form, and the same VARMA by the `.inp` path, route V
  and route U (`golden.txt`), each from several starts: about 300
  estimations, twenty minutes. It checks facts against `ref/hechos.tsv` and
  properties that need no reference (a VAR(p) is not below the VAR(p−1); the
  starts agree; the three paths agree). What does not hold is listed in
  `abiertos.txt` (defects to fix) or `limites.txt` (known limits), and a
  property that starts to hold fails the battery until it is taken off the
  list. Run it before and after any change in the engine.

The `cases/*.py` scripts compute recursive forecast-error metrics.
