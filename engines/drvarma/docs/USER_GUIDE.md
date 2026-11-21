# drvarma — User Guide

drvarma estimates **vector ARMA (VARMA)** models by exact maximum likelihood and
produces diagnostics, impulse-response analysis and forecasts. This guide covers
the command-line program. The window for these models is SIM (`gui/sim`).

## 1. Quick start

```sh
make drvarma
./bin/drvarma data/models_group1/IPC3 3 0 -mean -deseason auto -forecast 24
```

This estimates a trivariate VAR(3) on `IPC3.inp`, with a mean term and automatic
harmonic seasonal adjustment, and forecasts 24 periods. Outputs:

- `IPC3.out` — full results (parameters, tests, IRF, FEVD, residual diagnostics).
- `IPC3.forecast` — point forecasts and 95% bands, in original units.

## 2. Input file (`.inp`)

The data file is plain text. `file` on the command line is given **without** the
`.inp` extension.

```
* Free-form comment line(s) starting with a single '*'
** Frequency (1=A, 4=Q, 12=M):
 12
** Series, observations, start (subperiod year):
 3 216 1 2002
** Series names:
 IPC_ES IPC_FR IPC_DE
** Box-Cox lambda, regular differences, annual differences:
 0.0 1 0
** Data:
 69.5300 81.9400 77.7000
 69.5900 82.0400 78.0000
 ...
```

- Header lines beginning with `**` are labels; the value(s) follow on the next line.
- **Data are raw levels** (one row per observation, one column per series). The
  engine applies the transformation itself.
- `Box-Cox lambda`: `0.0` = natural log; any other value applies (y^λ − 1)/λ,
  so `1.0` gives y − 1, not the identity. (The ladder reads fue's files with
  fue's transform, refactor·y at λ = 1: lib/prewhiten BUG-0002.)
- `regular differences` (d) and `annual differences` (D): differencing orders;
  the seasonal lag is the frequency.

The transformation actually fed to the estimator is
`scale · BoxCox_lambda(level)` then `(1-B)^d (1-B^s)^D`, optionally with the
seasonal component removed (`-deseason`). Forecasts are inverted back to the
original level scale automatically.

## 3. Command-line options

```
drvarma file p q [options]
```

`p`, `q` are the regular AR and MA orders.

| Option | Effect |
|--------|--------|
| `-mean` | estimate a mean/drift term for the (differenced) series |
| `-diagar` | restrict the AR coefficient matrices to diagonal |
| `-diagma` | restrict the MA coefficient matrices to diagonal |
| `-diagcov` | restrict the innovation covariance to diagonal |
| `-m 1` / `-m 2` | exact maximum likelihood: ξ truncated at 1e-3 (default) or untruncated |
| `-twostep` | Hannan–Rissanen two-step start (only meaningful with q>0) |
| `-deseason auto` | remove harmonic seasonality from series that test seasonal (default mode) |
| `-deseason force` | remove harmonic seasonality from all series |
| `-scale f` | rescale after Box-Cox; **default 100**. Rescaling improves the conditioning of the convergence criteria; the optimum is unchanged (the optimiser's path is not: drvarma-python BUG-0027) and forecasts are inverted |
| `-forecast N` | write N-step forecasts to `file.forecast` |
| `-estwin N` | fixed-parameter recursive forecasting (see §6) |
| `-volexp [α w]` | exponential-window volatility |
| `-volmov [w]` | moving-window volatility |

A **diagonal** VAR (`-diagar -diagcov`, q=0) factorizes into independent
univariate models — useful as a validation baseline against univariate software.
The **full** model (no diagonal flags) captures cross-series dependencies.

## 4. Reading the output (`.out`)

- **Estimated Parameters** — `mu`, `phi[k]_ij` (AR), `theta[k]_ij` (MA),
  `cov[i,j]` (innovation covariance Q), each with standard error, t-stat, p-value.
- **Joint hypothesis tests (Wald)** — significance of the last lag and of the
  cross-effects (directional Granger-type tests).
- **Impulse-response & variance-decomposition** — orthogonalized (Cholesky)
  responses; variable order follows the model.
- **Multivariate residual diagnostics** — Hosking portmanteau Q and multivariate
  Jarque–Bera (normality).
- **Inverse roots** — of |Φ(B)|; all moduli < 1 ⇒ stationary.
- **Per-series residual diagnostics** — summary stats, ACF/PACF, Q.

> Note on convergence: with the default `-scale 100` the optimiser normally stops
> on the gradient criterion (gradtol). On flat / highly collinear problems it may
> stop on the step criterion (steptol); both are valid optima.

## 5. Forecasts (`file.forecast`)

For each series: `Level`, 95% band (`Low95`, `High95`) in original units, plus
the monthly and annual variation rates with their standard errors.

## 6. Recursive (multi-origin) forecasting — `-estwin N`

`-estwin N` estimates the parameters **once** on the first `N` raw observations,
then, holding them fixed, forecasts from **every** origin from observation `N` to
the end of the data, writing `file.recursive`:

```
origin series horizon level
```

This supports out-of-sample evaluation across many origins **without
re-estimating** at each origin (parity with fixed-parameter univariate tooling).
Provide the full data file (training + later observations) and use together with
`-forecast H`.

```sh
# estimate on first 216 obs, recursive forecasts (24-step) from each origin
drvarma data/passthrough/WTI_IPC_ES_ext 1 0 -mean -deseason auto -forecast 24 -estwin 216
```

## 6b. The ladder — fue's files as input (5.0)

> **The multivariate `.inp` of §2 is deprecated since 5.0.** Convert it with
> `drvarma -split FILE.inp [-mean] [-harmonics] [-ar P] [-ma Q] [-dir DIR]`:
> one univariate `.inp` of fue per series. Refine them in fue or art, or
> give them to the ladder as they are. With `-mean -ar P -ma Q` and cross
> orders p = P, q = Q, the ladder is the old full VARMA(P,Q).

```
drvarma IPC_ES_m10.pre IPC_FR_msar.pre IPC_DE_mar3sar.pre 1 0 -o ipc3
```

Give the univariate models that fue (or art) wrote as `.pre`, then the cross
orders `p q`. Each series keeps its own transformation and ARMA on the
diagonal of the system; `p`/`q` add the dynamics between series. Options:

| Option | Meaning |
|---|---|
| `-diagcov` | diagonal innovation covariance (default full) |
| `-redet` | re-estimate all the deterministic terms (default: fixed); it is `-det all` |
| `-fixarma` | keep the univariate ARMA factors fixed |
| `-m 1\|2` | exact likelihood, ξ truncated at 1e-3 / untruncated |
| `-lik elf\|shea\|both` | the likelihood: AS 311 (default), Shea's AS 242, or elf checked against Shea |
| `-hessian fd\|bfgs` | standard errors from fdhess (default) or the BFGS Hessian |
| `-links "A<-B,..."` | the cross dynamics only on these pairs |
| `-zero "AR1[A<-B],..."` | single cross coefficients held at zero |
| `-route u\|v` | `u` (default), the ladder: each series keeps its univariate ARMA on the diagonal and the cross terms are added. `v`, the vector first: the same gate with the univariate models (they are the yardstick and give the mean, the deterministic terms and the stationary series); past it, each series' regular ARMA is replaced by a free AR(p)/MA(q) — the whole VARMA(p,q) matrix is free — and its seasonal and fixed-frequency factors multiply its whole row, cross terms included. The LR is against the same model without cross terms. A note says when p or q is below a univariate's regular order. Not with `-fixarma` |
| `-echelon n1,n2,…` | the echelon form (Lütkepohl and Poskitt), on route V: one Kronecker index per series, in the order of the files; give `0 0` as cross orders. Row r has its own AR(n_r)/MA(n_r); the indices say which cross lags exist (cross AR r ← c only at lags n_r − n_rc + 1 … n_r, with n_rc = min(n_r + 1, n_c) for c < r and min(n_r, n_c) otherwise; cross MA at 1 … n_r) and which contemporaneous terms (`AR0[r<-c]`, only for c < r and n_c > n_r). The order of the series is part of the model. Equal indices are the VARMA(n,n) of `-route v` |
| `-echelon search` | the Kronecker indices from the data: Lütkepohl and Poskitt's stages I and II. A long VAR by least squares (its order by AIC), then least squares for every candidate (with one round of Hannan and Kavalieris' correction) and a criterion, `-criterion SC` (default) or `HQ`, with indices up to `-nmax N` (default 3). Every candidate while they are at most 256, Hannan and Kavalieris' two steps beyond. With row factors, each row is fitted on the series filtered by its own. It gives the table and stops: no maximum likelihood; the model is then fitted with `-echelon` and the indices chosen. Give `0 0` as cross orders |
| `-factortest` | on route V, the likelihood-ratio test of the row factor: the fitted model against the same rows with every position the factor reaches, own and cross, free. Multiplying a row by the univariate's seasonal factor is a restriction (Tiao and Box's caveat), and this is its check. It starts at the fitted optimum, so the LR is never negative; if the search without the factor does not certify an optimum, the output says that the LR is a lower bound |
| `-det LIST` | which deterministic terms are re-estimated jointly: `all` (= `-redet`), `none`, a class (`seasonal`: cos, sin, alter; `calendar`: easter; `interv`: impulse, compimp, step, ramp; `trend`), `SERIES:class`, or a coefficient's name as the output writes it (`"w0_IPC[step 8 1990]"`, which frees that term), separated by commas. Only what the file marks as free is freed. A shock common to several series is `-det` with its name in each |
| `-detbase file\|gate` | where the fixed deterministic terms stay: the file's values, or the gate's (estimated series by series on the common window, then held). Default: `gate` for a series the common window trims, `file` for the rest |
| `-start echelon` | with `-echelon`: Lütkepohl and Poskitt's stage II least squares as the start of the whole model (own AR and MA, cross terms, A₀ and Q). Roots left outside the unit circle are pulled inside, and the output says by how much; a point the likelihood rejects goes back to the model without cross terms. It is not the default: with indices larger than the data ask for it can start worse than zero |
| `-start zero\|ols` | where the cross terms start: zero, or the original drvarma's least squares (`init_varma`): the cross AR terms and Q's covariances; the own models stay the univariate ones. Default: `ols` with q = 0, `zero` with q > 0 (no start seeds the MA yet) |
| `-irfboot N` | bands for the impulse responses by parametric bootstrap, N refits |
| `-o NAME` | results to `NAME.out` (default: the `.pre` names joined by `_`) |
| `-forecast H` | forecast H periods, every series in its level → `NAME.forecast` |
| `-estwin N` | estimate on the first N observations of the first series, then fixed-parameter forecasts from every origin to the end → `NAME.recursive`, with MAE/RMSE/MAPE by series and horizon in the `.out` (needs `-forecast`) |

The `.out` starts with the **diagonal gate**: each series fitted alone, the
sum of their log-likelihoods, and the diagonal system evaluated at those
optima. They must agree; if they do not, the program stops with code 5. The
gate also shows how far each `.pre` moved when re-estimated: a genuine `.pre`
moves ~1e-5 (the rounding of its six decimals); more than 1e-3 means a
specification, not an optimum, unless the common window trimmed that series.
Then the requested model, with an LR test of the cross dynamics against the
diagonal system.

Series must have the same frequency and end on the same date; different
starting dates are aligned at the end. `-mean`, `-deseason` and `-scale` are
rejected in this mode: the transformation comes from each `.pre`.

To judge a VARMA, compare its recursive evaluation with the diagonal
system's on the same data and window:

```
drvarma ES.pre FR.pre DE.pre 0 0 -diagcov -forecast 24 -estwin 216 -o diag
drvarma ES.pre FR.pre DE.pre 1 0          -forecast 24 -estwin 216 -o var1
```

The diagonal system is the univariate models, and it reproduces fue's
forecasts. A VARMA that does not improve on it out of sample has no reason to
exist, however significant its cross terms are in sample.

## 7. Graphical interface

The engine's own GTK front-end was retired in November 2026. The window for
these models is SIM (`gui/sim`, `sim_gui`), built from the repository root:
it runs this engine on the ladder's files. Cointegration is drvec's.

## 8. Worked examples

The `data/models_group1/` and `data/passthrough/` directories contain ready-to-run
cases; `MODELS_RESULTS.md` documents the reference-model battery (univariate vs
diagonal vs full VARMA, and the WTI→CPI pass-through study) with their conclusions.
