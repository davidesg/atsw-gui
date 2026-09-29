# Changelog

All notable changes to drvarma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

Versions up to 0.4 were maintained as directory snapshots (`drvarma_v.01` …
`drvarma_v.04`); 0.4.1 is the first release tracked in git.

## [5.0.0] — unreleased

drvarma joins the ladder fue → drtran → drvarma: it reads fue's `.pre` files.
Design: `docs/DESIGN-v5-ladder.md`.

### Changed
- **The MA wall has one tolerance for both sides** (`MA_WALL_TOL`, 5e-5, in
  lib/lik/lik.h; drtran too). chekma refuses a root at modulus >= 1 + 5e-5; a
  stop with a root at modulus >= 1 - 5e-5 is now reported as on the wall
  ("MA boundary: k of n inverse roots within 5e-5 of the unit circle").
  Before, only >= 1 counted, so a fit that reached the wall from inside said
  CONVERGED. Bench: only the wording of c2, c3 and c7 changes.

### Added
- **Restricted cross terms, `-links "A<-B,C<-A"`** (ladder mode). Only the
  named pairs carry cross coefficients (B enters the equation of A: AR and MA,
  every lag up to p and q); the others are zero. Names as in the .pre files;
  the .out header lists the links and the LR's df counts only them. Without
  `-links` nothing changes (bench: 18 cases, 0 differences). Every pair named
  explicitly reproduces the unrestricted run byte for byte; on IPC_ES_m10 +
  IPC_FR_msar, 1 1 -links "IPC_ES<-IPC_FR": 8 parameters, logL 81.114461, the
  same as drvarma-python's Ladder(links=) and reached again from the full
  model's optimum.
- **Shea's exact likelihood, `-lik elf|shea|both`** (both paths).
  - **What it is.** drvarma's source had carried Shea's algorithm since
    1996: `marma`, AS 242 (1989), the other efficient exact method and the
    benchmark in Mauricio's papers. It was compiled and never called.
  - **`-lik shea`** makes it the objective. Shea is always run exact,
    whatever `-m` says.
  - **`-lik both`** optimises with elf and evaluates Shea at every point the
    optimiser visits. The `.out` reports the largest |ΔlogL| along the path
    and at the optimum.
  - **Measured with `-m 2`:** 1e-9 to 1e-13 at every point, over thousands
    of points per case (VAR, VARMA(1,1), VARMA(2,1), and the m6 ladder with
    6 592 points). Two independent algorithms give the same likelihood.
  - **Residuals** always come from elf. marma returns the one-step
    innovations, not the exact residuals the forecasts need (drtran
    BUG-55).
  - **MA admissibility.** Shea uses elf's MA admissibility check (`chekma`).
  - Code: `lib/lik/` (`lik.c`, `multshea.c`), shared with drtran (`-l`).
  - **Validated outside AS 311:** drtran's whole battery passes with Shea as
    the objective (326/326). That includes the homologation against fue,
    whose likelihood is Mélard's AS 197, the exact GLS and the synthetic
    truths.
- **The `.inp` path prints the exact log-likelihood** (`Exact log-likelihood:`
  in the `.out`, with a `Likelihood:` line saying which algorithm). It never
  printed it before; the ladder did.
- **`.pre` input (ladder mode).** `drvarma A.pre B.pre [...] p q [-diagcov]
  [-redet] [-fixarma] [-m method] [-o NAME]`. Each series carries its
  univariate model from fue on the **diagonal** of the VARMA: Box-Cox,
  deterministic terms, non-stationary operator, mean, and ARMA factors
  (seasonal and fixed-frequency ones included). `p` and `q` are the orders
  of the **cross** dynamics. The deterministic terms stay at their `.pre`
  values unless `-redet`.
- **The diagonal gate, and it closes.** Before anything cross is estimated,
  each series is fitted alone and the diagonal system is evaluated at those
  optima. If it does not match the sum of the univariate log-likelihoods,
  the program stops (exit code 5). The gate also reports how far each
  `.pre` moves: a genuine `.pre` is a fixed point. An LR test compares the
  cross dynamics against the diagonal system.
- **Declared version** in `include/version.h`, with the git commit the
  binary was built from (`5.0.0 (git <hash>[-dirty])`). It is used by
  `-version`, the `Program` line of the `.out` (both paths) and the GUI
  title.
- **Forecasting in ladder mode**: `-forecast H` and `-estwin N`. Each
  series goes back to its level with its own `.pre` model (deterministic
  terms, operator, Box-Cox), with 95% bands from the integrated psi weights.
  With `-estwin`, fixed-parameter forecasts from every origin
  (`NAME.recursive`) and an out-of-sample evaluation by series and horizon.
  The diagonal system reproduces fue's fixed-parameter forecasts: 3456
  values, relative difference ≤ 2.3e-6.
- `tests/banco/`: byte regression of the `.inp` path against 0.4.1.
  `tests/escalera/`: the assertions of the ladder and its regression.

- **`drvarma -split`** converts a multivariate `.inp` into one univariate
  `.inp` of fue per series (`-mean`, `-harmonics`, `-ar P`, `-ma Q`,
  `-scale`, `-dir`). The ladder on them reproduces the `.inp` path's full
  VARMA: VAR(1) with mean, μ and φ(1) identical to six decimals.
- The ladder takes fue's `.inp` specifications as well as `.pre` optima,
  recognised by their content (fue's own validator, `inp_check_fue`), not
  by their extension.

### Deprecated
- **The multivariate `.inp`.** It is still read, byte for byte as in
  0.4.1, with a one-line note on stderr; it goes in 6.0, once the GUI is
  migrated. One format for the whole ecosystem: fue's.

### Fixed
- **The diagonal gate is evaluated with the untruncated likelihood,** whatever
  `-m` says. It is an identity, and elf's ξ truncation (`-m 1`) is not the
  same for one series as for m.
  - Pairs of m6 failed the gate by up to 0.0018, and the report blamed the
    cast. They now close to 1e-13.
  - fue's `.pre` files are estimated untruncated too, so each series'
    "move" is now measured on the same likelihood.
  - The estimation keeps the `-m` asked for.
- **A stop on the MA invertibility wall was reported as "OPTIMIZER CONVERGED".**
  The optimiser's report is now written after `est()` checks the MA roots:
  `OPTIMIZER STOPPED at the MA invertibility boundary`, plus
  `MA boundary: k of n inverse roots at modulus >= 1`.
  - The line states facts and gives no verdict: studying the situation is
    the assistant's job (sima), with `-lik shea` as a second path.
  - In the bench, only the three cases with roots at 1.00005 change: c2, c3
    and c7.
- **`-m` labels were swapped.** `-m 1`, the default, is the exact likelihood
  with the ξ sequence truncated at 1e-3 (`xitol > 0`). `-m 2` is the exact
  likelihood without truncation, though the help called it "approximate".
  Only the text changes, not the numbers. drvec had found the same (its
  BUG-47).
- **The m6 ladder gate's −0.000428 is explained:** it is that truncation.
  With `-m 2` the gate closes to 0, and to 4.6e-13 with Shea.
- **BUG-2**: series are crossed by date, not by position. They must share
  the frequency and the last date, or they are rejected (`lib/fuepre`,
  shared with drtran).
- The line search (`lnsrch`) never returned on a NaN or infinite objective;
  `objcfunc` now returns 1.0 for a non-finite objective. `lnsrch` now lives
  in `lib/optim/lnsrch.c`, one source for drvarma, drtran, fue and fuf.
- Heap overflow in the residual histogram (`File_HistSer`) with three-digit
  counts.
- **A Hessian that is not positive definite gave standard errors, and the
  status that said so was lost** (BUGS.md; drtran BUG-56). `choldcp` is a
  modified Cholesky that patches pivots, and its status went to `*ifault`,
  which `est` then overwrote. `fdhess` is now checked with a plain
  Cholesky and for neighbours on the boundary. If either check fails, the
  BFGS factor is used and the report says why.
  The fallback applies only if raxopt iterated (`opt_iters`). It starts
  at the identity, so a search that did not move has no BFGS Hessian: then
  there are no standard errors and the table says `none (…)`.

### Changed
- The `.pre` reader, `struct Tusmodel`, the univariate transformations and
  the date helpers come from `lib/` (`fuepre`, `prewhiten`, `dates`): the
  same code that drtran and its GUI use. The copy of `ObsToDate` in
  `diagnose.c`, which was identical, is gone.
- **Standard errors come from fdhess by default, in both paths**
  (`-hessian fd|bfgs`). This is Mauricio's finite-difference Hessian at the
  optimum, from the published code. drvarma-python's study
  (`docs/STUDY-standard-errors.md` there) chose it: it matches the exact GLS
  within 0.35 % and OLS within 1 %, including series of very different
  scales. The BFGS Hessian of the search was off by up to 1483 % and
  depended on the start. Every parameter table now ends with
  `Standard errors: <method>`.
  - In the `.inp` path, `qq[1,1]` (the flat direction Q → cQ) is held
    while the Hessian is taken, and it prints as `(normalised)`.
  - Estimates do not move: the 18 cases of `tests/banco` differ only in
    SE, t, p and the Wald statistics.
  - One Wald conclusion changes: in `c1_ipc_ar1`, y1 not influenced by
    the others, p goes from 0.0425 to 0.0676.
  - Three cases whose MA has roots on the unit circle (modulus 1.00005)
    keep BFGS, and the report says the optimum is on the boundary.
- Apart from that, the `.inp` path is unchanged: its `.out` also differs in
  the `Program` line.

## [0.4.1] — 2026-06-24

First GPL-ready, Numerical-Recipes-free release; prepared for public
distribution on GitHub.

### Removed
- **All Numerical Recipes code** (concentrated in `src/nlatools.c`). The codebase
  now contains no NR routines or copyright markers.

### Changed
- **Linear algebra now uses the GNU Scientific Library (GSL):** eigenvalues
  (`eigenqr` via `gsl_eigen_nonsymm`) and SVD (`svdcp`/`svsol` via
  `gsl_linalg_SV_decomp`/`SV_solve`). Public signatures are unchanged, so callers
  are unaffected. Verified numerically transparent (identical objective,
  parameters, eigenvalue moduli and Wald statistics on the reference cases).
- **Dynamic-memory routines** (`vector`/`matrix`/`tensor`/`free_*`/`nrerror`)
  reimplemented cleanly (Treadway & Guerrero 2009).
- **Default rescale factor is now 100** (`-scale`). Rescaling improves the
  conditioning of the convergence criteria (per J.A. Mauricio); the optimum and
  forecasts are unchanged and the optimiser converges on the gradient criterion.
- **GUI** manages generic series names (`y1, y2, …`) consistently across the
  interface, the generated `.inp` and the log output.

### Added
- **`-estwin N`**: fixed-parameter recursive (multi-origin) forecasting. Estimate
  once on the first N observations, then forecast from every origin to the end of
  the data, written to `file.recursive` — for out-of-sample evaluation without
  re-estimating at each origin.
- **GPL licensing**: `COPYING` and `LICENSE` (GNU GPL v2 or later) and a GPL
  notice in every source file.
- **Documentation**: `README.md`, `docs/USER_GUIDE.md`, `docs/DEVELOPER_GUIDE.md`,
  `CONTRIBUTING.md`, plus the reference-model notes (`MODELS_PLAN.md`,
  `MODELS_RESULTS.md`, `NR_REMOVAL_PLAN.md`).
- **Continuous integration**: `.github/workflows/ci.yml` builds the engine and
  GUI, runs a smoke-test estimation/forecast, and scans for any reappearance of
  Numerical Recipes copyright markers.

### Removed (dead code)
- Unused NR eigenvalue helpers (`tred2`, `balanc`, `elmhes`, `eigenql`) and the
  now-redundant `eigenql` prototype.
