# Changelog

All notable changes to drvarma are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/), and the project aims to follow
[Semantic Versioning](https://semver.org/).

Versions up to 0.4 were maintained as directory snapshots (`drvarma_v.01` …
`drvarma_v.04`); 0.4.1 is the first release tracked in git.

## [5.0.0] — unreleased

### Added — `-echelon`: the echelon form with given Kronecker indices

- `-echelon n1,n2,…`, on route V: one Kronecker index per series. Row r has
  its own AR(n_r)/MA(n_r); the indices fix which cross lags exist and which
  contemporaneous terms (`AR0[r<-c]`, the lower-triangular A₀). Lütkepohl and
  Poskitt's reversed echelon form, the identified VARMA. Equal indices are
  the VARMA(n,n) of `-route v`.
- Homologated with the port (`Ladder(echelon=n, own="free")`) to six
  decimals wherever both certify the optimum: IPC and WTI at (1,0), (1,1),
  (2,1), (1,2); muskrat and mink at (1,0); IPI, IPC and WTI at (0,1,1), with
  a row factor.
- Not yet: the index search, the stage II start, and `-factortest` with
  unequal indices.

### Added — `-route v`: the vector first, through the gate

- Route V (Tiao and Box) used to mean specifications without univariate
  models. It now goes through the same gate as route U: the univariate
  models are the yardstick and give the mean, the deterministic terms and the
  stationary series. Past the gate, each series' regular ARMA is replaced by
  a free AR(p)/MA(q), so the whole VARMA(p,q) matrix is free, and its
  seasonal and fixed-frequency factors multiply its whole row (the row factor
  of the port's echelon form).
- The LR is against the same model without cross terms. A note says when the
  model does not contain a univariate's regular ARMA.
- When the univariates' regular part is an AR(p) and there are no seasonal
  factors, routes U and V give the same table. With a row factor and cross
  terms, the VARMA(1,1) of IPI and WTI gives the port's number,
  `Ladder(echelon=(1,1), own="free")`: −924.846133.
- **`-factortest`**: the LR of the row factor, the fitted model against the
  same rows with every position the factor reaches free. On the monetary
  VAR(1) (IPI, IPC, WTI, FF): LR 6.16, 4 df, p 0.19. Checked against an
  independent path (a hand-made file and `-zero`), to six decimals.
- **The seed of route V is the whole matrix**: least squares of the VAR(p)
  give the diagonal and the cross terms (the original drvarma's
  `init_varma`, whole), the MA starts at zero, and Q takes the variances of
  the exact diagonal fit and the least-squares residuals' correlation. With a
  row factor each row is fitted on the series filtered by its factor. It is
  the default with q = 0, and `-start ols` asks for it. On the gas furnace
  VAR(6) it is admissible where route U's cross-only seed is not; in every
  case measured it reaches the same optimum as from zero.

### Added — `-det` and `-detbase`: which deterministic terms are estimated

- `-redet` was all or nothing. `-det LIST` re-estimates only some of the
  deterministic terms in the joint fit: a class (`seasonal`, `calendar`,
  `interv`, `trend`), a series' class (`IPC:seasonal`) or single terms by the
  name the output gives them (`"w0_IPC[step 8 1990]"`); `-redet` is
  `-det all`. A shock common to several series is `-det` with its name in
  each. Only what the file marks as free is freed.
- `-detbase file|gate` says where the fixed ones stay: the file's values, or
  the gate's, estimated series by series on the common window and then held.
- **Changed by default:** a series the common window trims has its
  deterministic terms re-estimated at the gate and held there (`-detbase
  gate` for that series). Until now they stayed at the values of the longer
  sample while its ARMA and mean were re-estimated on the short one.
  `-detbase file` gives the old behaviour. Series that are not trimmed are
  unchanged. In the ladder's battery one reference moves (`corto`: France
  trimmed by a year; logL of the gate 43.308 → 43.743).
- The mean is not part of this: it is estimated whenever the file estimates
  it, as before.

### Changed — the default start of the ladder: `ols` with q = 0

- Without `-start`, the cross terms now start at the original drvarma's
  least squares (`-start ols`) when q = 0, and at zero when there is an MA.
  From zero, exact ML stalled after two iterations on the US monetary VARs
  (drvarma-python BUG-0027); from `ols` every VAR measured reaches its
  optimum, and none ends worse (`tests/rutas`: 16 monetary VARs, 29 golden
  models). With q > 0 no start seeds the MA yet and `ols` is not better than
  zero, so zero stays until the echelon form's start exists. `-start zero`
  and `-start ols` still decide when given.
- The `.out` says which start was used. The ladder's regression reference
  moves in one case (the WTI pass-through): the same estimates to six
  decimals, standard errors within 0.6 %, and the search now ends by the
  gradient criterion.
- `tests/rutas` gains the default-start run of the monetary cases, the
  property `defecto` and `limites.txt` (known limits that are not going to be
  fixed), apart from `abiertos.txt` (defects to fix).

### Fixed — `-split -mean` seeds the mean at the sample mean (BUG-0007)

- `-split -mean` wrote a free mean starting at zero. On series far from zero
  (levels around 100, so 10000 with the factor) the gate's univariate fits
  carried the level with the own AR, near a unit root, and the ladder on
  those files ended 49 log-likelihood points below the `.inp` path's same
  VAR(1). The seed is now the sample mean of the stationary series, as
  `init_varma` and sima take it. Only the writer changes.

### Fixed — `-start ols` falls back to zero for real (BUG-0006)

- When the least-squares start was not admissible, `start_ols()` zeroed the
  cross terms and then freed its check fit, whose last `shootx_esc` unpacks
  the checked point and put the least-squares values back. The fit started
  there and failed (AR nonstationary) while the `.out` said it started from
  zero. The check fit is now freed first. Gas furnace, route V, VAR(6):
  −2638.762693 from both starts, as the port (`Ladder._take_start`).

### Fixed — con λ = 1 la Box-Cox es la de fue (lib/prewhiten BUG-0002)

- La librería compartida (`lib/prewhiten`, `lib/fuepre/fuepre_forecast.c`)
  transformaba con refactor·(y − 1). fue, el dueño del formato, drvec y los
  portes transforman con refactor·y. Con d = 0 el −1 caía en la media: un
  `.pre` de fue en niveles con media no era un óptimo aquí (la puerta movía
  μ en el factor). Ahora es refactor·y, y su inversa y/refactor.
- Sin cambio en los bancos: ningún caso tiene λ = 1 con d = 0 y media.

### Added — `-start ols`: the original drvarma's start, in the ladder

- `init_varma` of the `.inp` path brought to the ladder: least squares of the
  VAR(p), with no constant, on the centred stationary series of the diagonal
  state. It is the ladder's start PLUS the parameters of the relation:
  - the free cross AR terms take Phi_k (Box and Jenkins: the cross term is
    +Phi_k), with `-links` and `-zero` respected;
  - Q's covariances take the residuals' correlation in the ladder's
    normalisation, q_ab = r_ab sqrt(Q_aa Q_bb), with the gate's Q_aa;
  - the cross MA terms stay at zero.
- The own models are never seeded, on either route: the ladder's principle
  (the univariate models first, from art and mtram). On route V the diagonal
  fit has already estimated them. If elf does not accept the point, zero is
  used and the `.out` says so.
- The default stays `-start zero`: `tests/banco` (18 cases) and
  `tests/escalera` are unchanged.
- On the US monetary VAR (FRED; IPI, IPC, WTI, GS1) the zero start stops the
  VAR(2) after 2 iterations at −1793.05, below the VAR(1) (drvarma-python
  BUG-0027). From least squares it reaches −1741.275477, the same as the
  port's `Ladder(start="ols")`: 46 parameters within 6e-6.
  `tests/escalera` case 10b.

### Added — `-irfboot N`: bands for the impulse responses by parametric bootstrap

- The ladder simulates the fitted model N times (the stationary VARMA of
  the w, Gaussian shocks with the fitted Σ, a burn-in thrown away), refits
  each replication as the data were (from the optimum, with elf and without
  the Hessian at the optimum) and reports Efron's percentiles (2.5, 5, 50,
  95, 97.5) of the orthogonalised IRF and of its accumulation: to the .out,
  and whole to `<base>.irfboot` in drvec's format. drvec's generator and
  seed, so the bands are reproducible. Not with `-redet`.
- `tests/escalera/pruebas.sh` §10d: the whole file, the same file in two
  runs, and the 90 % band within 30 % of the delta method's at h = 0..2
  (measured: 11 %). `lib/outvarma` reads it (`ov_irfboot_file`) and sim_gui
  draws it ("Bandas por bootstrap", in the background).

### Added — standard errors of the impulse responses, by the delta method

- The ladder writes, after the orthogonalised IRF, two new sections: the
  standard errors of the responses and of their accumulation. The gradient
  is taken by central differences THROUGH `shootx_esc` (the same map from
  the parameters to Phi and Theta that the estimation uses), with the
  parameters' covariance of the table; and through the Cholesky factor of
  Σ, with Cov(Σ_ab, Σ_cd) = (Σ_ac Σ_bd + Σ_ad Σ_bc)/n. The two blocks are
  independent under normality. Without standard errors for the parameters
  (boundary, Hessian not positive definite) the sections say so.
- `tests/escalera/pruebas.sh` §10c checks them against the closed form of
  an AR(1) (h = 0, 1, 2 and the accumulation). The four references only
  gain lines.
- `lib/outvarma` reads them (`irf_se`, `irfac_se`); sim_gui draws the IRF
  with bands of ±2 s.e.

### Fixed — the ladder's impulse responses were measured with Q, not Σ (BUG-0004)

- The ladder concentrates σ² and called `impulse_response` with the model
  `shootx_esc` leaves, where σ² = 1: the Cholesky factor was Q's, so a
  one-s.d. shock was √Q_jj instead of √Σ_jj and the orthogonalised IRF, its
  accumulation and the gain were all divided by σ. It now uses `F.sigma2`.
  The FEVD (a ratio) does not change. The `.inp` route and the Python port
  were right. `BUGS.md` BUG-0004, with its repro.

drvarma joins the ladder fue → drtran → drvarma: it reads fue's `.pre` files.
Design: `docs/DESIGN-v5-ladder.md`.

### Added
- **`-zero "AR1[A<-B],MA2[B<-A]"`** (2026-11-06, for sim_gui S7): single
  cross coefficients held at zero, Tiao and Box's simplification by
  coefficient (1981, §4), as the Python port's `zeros=` does. A zeroed
  coefficient leaves the vector of parameters and the table, the LR against
  the univariates loses its degree of freedom, and the header says
  «Zero coefficients: …». Each one has to be in the model asked for (its lag
  within p or q, its pair linked), or it is an error. Without `-zero` the
  output does not change, byte for byte.

### Changed
- **The deterministic terms of a `.pre` are filtered with fue's rule**
  (`lib/fuedet`, 2026-10-16; drvec BUG-40): ν(B) = ω(B)/δ(B) truncated at
  40 lags when there is a δ, the variables summed and subtracted once, the
  code of fue.c. Before, `apply_univariate_model` used the exact recursion
  and subtracted variable by variable, and `build_det_component` the exact
  recursion. No case of the battery has a δ; the change of order moves the
  last bits of w, and through the optimiser path the ladder regression
  (`tests/escalera/ref`, g1, pt, corto, fc, `.out` and `.log`): the gate's
  difference stays below 4e-13, the estimates agree to the printed digits,
  and the fdhess standard errors move by up to 0.6 % (pt: SE of
  AR1[WTI<-IPC_ES] 2.450373 → 2.465813). In g1 and fc six inverse roots of
  modulus 0.003 appear in |φ(B)| (a hexagon around 0): a defective zero
  eigenvalue of the companion matrix, perturbed by the last bit, splits by
  eps^(1/6) and crosses the printing threshold 5e-5.
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
