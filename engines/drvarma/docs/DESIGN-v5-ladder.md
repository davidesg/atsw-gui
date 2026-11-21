# drvarma 5.0 — the ladder: fue's `.pre` files as input

drvarma is the third rung of the ladder fue → drtran → drvarma. drtran
estimates a **triangular** network, a restricted VARMA with no cycles. When
the network has a cycle, when two series determine each other, a general
VARMA is needed, and that is drvarma. Up to 0.4.1 drvarma did not read a
`.pre`. It loaded the series in levels and started over, with a single λ, d
and D for all of them, no deterministic terms and no seasonal factors. The
hand-over from fue was prose (`docs/DISENO-escalera.md` §6, at the root of
the monorepo).

5.0 accepts the `.pre` files, as drtran does, **and still behaves exactly as
before with an `.inp`**. The bench `tests/banco/` pins the `.inp` path's output
byte for byte. It started as the 0.4.1 output and has been regenerated on
purpose with each deliberate change of that output (`git log --
tests/banco/ref`).

## 1. Usage

    drvarma A.pre B.pre [C.pre ...] p q [-diagcov] [-redet] [-fixarma]
                                        [-m method] [-o NAME]
                                        [-forecast H [-estwin N]]
                                        [-lik elf|shea|both] [-hessian fd|bfgs]
                                        [-links "A<-B,..."] [-zero "AR1[A<-B],..."]
                                        [-start zero|ols] [-irfboot N]

The options after the first three lines came after this design: `-lik`, the
likelihood; `-hessian`, the standard errors; `-links` and `-zero`, the cross
dynamics on some pairs or single coefficients at zero; `-start`, where the
cross terms start (§3); `-irfboot`, the bootstrap bands of the responses.
The usage text of the program (`escalera_usage`) is the reference. The
architecture of the ladder, in the C and in the Python port, and what is
homologated between them, is drvarma-python's `docs/ARCHITECTURE.md`.

The ladder takes **univariate model files of fue**: a `.pre` (an optimum) or
an `.inp` (a specification). They are the same format, and the program tells
them from anything else by their **content**, not by their name. A file is a
model of fue if fue's own validator accepts it (`engines/fue/src/inpcheck.c`,
compiled as `inp_check_fue`, as the fue GUI does). art used to refuse a file
because it was *called* `.pre`, and a renamed one went through
(`DISENO-escalera.md` §1.2). In a specification the values are seeds. When
the gate sees one move, it says so, and if it carries deterministic terms
kept at their seeds, it asks for `-redet`.

Otherwise it is the usual 0.4.1 program (`drvarma file p q ...`), now
deprecated (§10).

Options of the `.inp` path that the `.pre` already settles (`-mean`,
`-deseason`, `-scale`) are rejected with an explicit error rather than
ignored. Forecasting is §8.

## 2. The model

Each series `i` brings its full univariate model from its `.pre`:

- Box-Cox `λᵢ` and fue's rescaling factor;
- the deterministic terms (`cos`, `sin`, `alter`, `step`, `impulse`,
  `easter`, ...);
- the non-stationary operator `∇^dᵢ ∇ₛ^Dᵢ`, including the irreducible
  factors of the annual difference;
- the mean `μᵢ`;
- the ARMA factors `φᵢ(B) Φᵢ(Bˢ)` and `θᵢ(B) Θᵢ(Bˢ)`, including the
  fixed-frequency ones.

This takes each series to its stationary series `wᵢ`, with the same code that
drtran and the drtran GUI use (`lib/prewhiten`). All of them are trimmed to the
common window, aligned at the end (§4). The model on `w` is

    Φ(B) (w_t − μ) = Θ(B) a_t,      a_t ~ N(0, Σ)

where the **diagonal** of each row comes from its `.pre` and the
**off-diagonal** is free:

    Φᵢᵢ(B) = φᵢ(B) Φᵢ(Bˢ)                       Θᵢᵢ(B) = θᵢ(B) Θᵢ(Bˢ)
    Φᵢⱼ(B) = − Σ_{k=1..p} c_{ij,k} Bᵏ   (i≠j)   Θᵢⱼ(B) = − Σ_{k=1..q} e_{ij,k} Bᵏ

`p` and `q` are the orders of the **cross** dynamics; each `.pre` already
carries its own. With `p = q = 0` and `-diagcov` the system splits into the m
univariate models. That is the gate (§3).

**Parameters.**

- The ARMA factor coefficients that the `.pre` flags as estimable are
  re-estimated jointly, starting from their `.pre` values. `-fixarma` keeps
  them fixed.
- The mean is estimated if the `.pre` estimates it.
- The deterministic terms stay **fixed** unless asked. The decision is taken
  term by term and twice, at the gate (each series alone) and in the joint
  fit:
  - `-det LIST` says which are re-estimated jointly: `all` (which is
    `-redet`), a class (`seasonal`, `calendar`, `interv`, `trend`),
    `SERIES:class`, or a coefficient's name as the output writes it. Only
    what the file marks as free is freed. A shock common to several series
    is `-det` with its name in each of them: the term comes in through the
    univariate models, and the ladder only decides which is freed.
  - `-detbase` says where the fixed ones stay: `file`, the file's values, or
    `gate`, estimated at the gate on the common window and then held. By
    default it is `gate` for a series the common window trims, whose file
    is no longer the optimum of the sample in use, and `file` for the rest
    (a genuine `.pre` on its own sample is a fixed point: both are the same).
  - Whatever is free jointly is free at the gate too. The mean is not one of
    them: it is estimated whenever the file estimates it.
  - With fixed deterministic terms the standard errors are conditional on
    them, and the parameter count does not include them.
- `Σ = σ²·Q`, with `Q₁₁ = 1` and `log(Qᵢᵢ/Q₁₁)` free. `est()` concentrates σ²,
  and without this normalisation `Q → cQ` is a flat direction and the Hessian
  is singular (Mauricio 1995, eq. 2.1; the same as drtran). The covariances
  are free unless `-diagcov`.
- Standard errors come from a finite-difference Hessian at the optimum
  (`est_fdhess`), not from the one accumulated by BFGS. This is drtran's fix,
  and since the standard-error study (drvarma-python
  `docs/STUDY-standard-errors.md`) it is the default of the `.inp` path too,
  where `qq[1,1]` is held (`est_fixed`). `-hessian bfgs` asks for the old
  behaviour. If the Hessian is not positive definite by a plain Cholesky, or
  a neighbour of the optimum is inadmissible (the boundary), BFGS is kept and
  the table says why (`Standard errors: …`).

## 3. The diagonal gate, and it closes

Before anything cross is estimated, the program:

1. fits each series **alone** (m = 1), with the same cast and on the same
   window;
2. **evaluates**, without optimising, the diagonal system (`p = q = 0`,
   diagonal Q) at those optima, with `Qᵢᵢ/Q₁₁ = σ̂²ᵢ/σ̂²₁`.

At that point the identity is exact: `logL_diag = Σ logLᵢ`. What is checked
is an **evaluation**, so the gate certifies the likelihood and the cast, not
the convergence of an optimiser. If the identity fails (relative tolerance
1e-6), the program **stops with code 5**. The lesson of `DISENO-escalera.md`
§1 is that a gate that only prints 🛑 is not a gate. On the bench cases the
difference is of the order of 1e-13.

The gate also reports, per series, how far the parameters move when
re-estimated from the `.pre`:

- A genuine `.pre` is a fixed point: it moves ~1e-5, the rounding of its six
  decimals (3e-5 on the bench).
- A series that moves more than 1e-3 is flagged "not an optimum": that `.pre`
  is a specification.
- If the common window trimmed that series, it is flagged "trimmed sample"
  instead: it was re-estimated on a different sample, and moving is
  legitimate.

**Route V (`-route v`).** The school's rule holds on both routes: the
univariate models first. They are the yardstick and give the initial
conditions; the gate is that. Past the gate, the two routes differ only in
what the own part is and how it is seeded:
- **route U** (default): each series keeps its univariate ARMA; the cross
  terms are added;
- **route V** (the vector first, Tiao and Box): each series' *regular* ARMA is
  replaced by a free AR(p)/MA(q), p and q the vector's orders, so the whole
  VARMA(p,q) matrix is free. What is left of the univariate model, its
  seasonal and fixed-frequency factors, multiplies its whole row, cross terms
  included: F_a(B)[Φ(B)w]_a = G_a(B)[Θ(B)a]_a, the row factor of the port's
  echelon form. The deterministic terms, the differences and the mean are the
  gate's.
- **The seed is the whole matrix.** On route V the own part is the vector's,
  so the original drvarma's start (`init_varma`) holds whole: least squares
  of the VAR(p) give the diagonal and the cross terms, and the MA, own and
  cross, starts at zero. With a row factor, row a is fitted on every series
  filtered by that row's (F_a/G_a)(B). The variances of Q are those of the
  exact diagonal fit (the model without cross terms); the covariances, the
  least-squares residuals' correlation on that scale. It is the default with
  q = 0 and `-start ols` asks for it; `-start zero` keeps the own part at
  the univariate's regular polynomials when they fit (at zero otherwise) and
  the cross terms at zero. A seed the likelihood rejects goes back to the
  model without cross terms, and the output says so.
- **The row factor is a restriction, and `-factortest` checks it.** It is
  Tiao and Box's caveat on carrying univariate operators into the joint
  model. The alternative is the same rows with every position the product
  F_a(B)[Φ(B)]_a (and G_a(B)[Θ(B)]_a) reaches free, own and cross; it starts
  at the expansion of the fitted optimum, where its likelihood is the same,
  so the LR is ≥ 0 by construction (the port's `factor_test`). Checked by an
  independent path: for the VAR(1) of IPI and WTI, a hand-made file with a
  regular AR of order 13 free at 1, 12 and 13 and `-zero` on the cross lags
  the product does not reach gives the same model, logL −925.888325, 11
  parameters. With an MA the model without the factor has the usual
  identification problem: the search may not certify an optimum, and the
  output then says the LR is a lower bound.
- With the echelon form the alternative is another echelon form with the
  same A₀: the rows are taken before A₀⁻¹ (Φ̃ = (I − C₀)Φ), and a
  contemporaneous term times the factor reaches the factor's own lags, in
  the AR and, since M₀ = A₀, in the MA. Homologated with the port's
  `factor_test` on eight cases with unequal indices, A₀ and an MA row
  factor: the same degrees of freedom and parameter counts in all, and the
  same LR wherever both certify the fit.
- Measured: the same optimum as from zero in every case; on the gas furnace
  VAR(6) the whole-matrix seed is admissible where route U's (cross terms
  only, on the diagonal fit's own models) is not.
- A model with p below a univariate's regular AR order (or q below its MA
  order) does not contain that univariate model: it is estimated, with a
  note.
- When every univariate's regular part is exactly an AR(p) and none has
  seasonal factors, the two routes are the same model and give the same
  table (the golden case of `tests/escalera`).
- Homologated with the port on the VARMA(1,1) of IPI and WTI:
  `Ladder(echelon=(1,1), own="free")`, logL −924.846133.

**The echelon form (`-echelon n1,n2,…`).** A VARMA with an MA is not
identified without restrictions, and from any start it tends to end on the
MA wall (`tests/rutas`, the open cases). Lütkepohl and Poskitt's echelon form
is the identified one: each series has a Kronecker index n_r, and the indices
fix which coefficients exist. It runs on route V:
- row r's own part is a free AR(n_r)/MA(n_r) (the substitution, with the
  row's order);
- the cross AR r ← c exists at lags n_r − n_rc + 1 … n_r, with
  n_rc = min(n_r + 1, n_c) for c < r and min(n_r, n_c) otherwise; the cross
  MA at 1 … n_r. They are set on the mask of `-zero`;
- A₀ is lower triangular with unit diagonal: a contemporaneous term r ← c
  only for c < r and n_c > n_r. It is kept as C₀ = I − A₀, with Box and
  Jenkins' sign (`AR0[r<-c]`), and the cast goes to the standard form,
  Φ_k = A₀⁻¹Φ̃_k and Θ_k = A₀⁻¹Θ̃_k, after the row factors (whose lag 0 is
  A₀'s row);
- the order of the series is part of the model;
- with equal indices nothing is restricted and A₀ = I: it is the VARMA(n,n)
  of `-route v`.

Homologated with the port, `Ladder(echelon=n, own="free", start="zero")`, in
every case both certify: IPC and WTI at (1,0), (1,1), (2,1) and (1,2);
muskrat and mink at (1,0); IPI, IPC and WTI, with IPI's row factor, at
(0,1,1). The parameter counts and the model without cross terms agree in
every case tried. Where neither certifies (indices larger than the data
ask for) the two stop at different points.

**The index search (`-echelon search`)** is what makes a VARMA operative:
with indices larger than the data ask for the model is over-parameterised
again and no start saves it; with the right ones nearly any start will do.
It is Lütkepohl and Poskitt's stages I and II, the port's `echelon.search`
operation by operation: a long VAR(h) by least squares (h by AIC up to
(log T)^1.5), whose residuals stand for the innovations; then, for each
candidate n, least squares row by row and the criterion
log|Σ(n)| + c_T dim(n)/T (SC by default), after one round of Hannan and
Kavalieris' correction (the residuals recomputed from the model just
fitted, its roots pulled inside). Every candidate up to 256, their two steps
beyond. It gives the table and stops.
- **The least squares are solved by singular values**, not by the normal
  equations. In the candidates that are too large the regressors are nearly
  collinear: the residuals are well determined but the coefficients are not,
  and the roots and the correction come from the coefficients. With the
  normal equations six of the sixteen candidates of IPC and WTI differed
  from the port in the second decimal; by singular values, with numpy's
  threshold, all agree.
- Homologated with the port on six cases (two, three and four series; with
  and without row factors; SC and HQ; every candidate and the two steps):
  the same candidates, dimensions and choice, and every log|Σ| and criterion
  to six decimals.
**The stage II start (`-start echelon`)** is the port's `_echelon_start`:
the long VAR, plain stage II on the positions the model holds free (links
and zeros stay out), and everything seeded from it: own AR and MA, cross
terms, A₀, and Q from the stage II residuals (Q₁₁ = 1). Least squares can
leave a root outside the unit circle: A_k and M_k are scaled by λ^k so that
the largest inverse root is 0.95, and the output says so. A point the
likelihood rejects goes back to the model without cross terms.
- Homologated with the port on ten cases: the same start (the same λ when
  the roots are pulled in), and the same iterations and optimum wherever the
  search certifies.
- **It is not the default, and the measurement says why.** With the indices
  the search chooses, zero and stage II reach the same optimum. With larger
  ones neither dominates: on IPC and WTI the stage II start ends above zero
  at (2,1) and below it at (1,2), and on IPI, IPC and WTI at (2,1,1) it
  starts at logL −1421 against −858 from zero and stalls there. The start
  does not make an over-parameterised VARMA estimable; the indices do.

The diagonal system is then optimised. On route V it is the model without
cross terms, the base of its LR. It is the seed of the full model: the
own models start there, and they are re-estimated jointly unless
`-fixarma`. With `-start ols` the cross terms start at the original
drvarma's least squares (`init_varma` brought to the ladder: the cross AR
terms and Q's covariances; never the own models); with `-start zero`, at
zero. **The default is `ols` with q = 0 and `zero` with q > 0.** From zero,
exact ML can stall when the series' scales differ (drvarma-python BUG-0027):
the monetary VARs of `tests/rutas`. With an MA no start seeds it yet, and
`ols` is not better than zero there (same battery); that waits for the
echelon form's start. The diagonal is also the base of the likelihood-ratio test of the
cross dynamics:

    LR = 2 (logL_full − logL_diag)  ~  χ²(number of cross and covariance parameters)

## 4. Alignment (BUG-2)

The C of drtran only compared the number of observations, so it crossed series
fourteen years apart without a word. Python rejects such input
(`cast.check_alignment`), and the C now does the same, with a single function
in `lib/fuepre` shared by drtran and drvarma:

- the same frequency;
- the same **last date**.

Series are aligned at the end and trimmed to the shortest stationary series.
Series that do not end on the same date are **rejected**: trimming to the
common calendar window changes the sample, and that decision belongs to
whoever builds the `.pre` files in art.

## 5. A single source

| what | where it was | where it lives |
|---|---|---|
| `.pre` reader (`read_fue_pre`, `free_fue_pre`, `build_det_component`, `operators_differ_tm`) | `engines/drtran/src/fue_pre_reader.c` | `lib/fuepre/fue_pre_reader.c` |
| `struct Tusmodel` | `engines/drtran/include/main.h` | `lib/fuepre/tusmodel.h` |
| counting and (un)packing of free coefficients, `invalid_fixfreq` | `static` in `drtran.c` | `lib/fuepre/fue_pre_reader.c` |
| `unstable_delta` (uses `chekma`) | `tran_shootx.c` | `lib/fuepre/fuepre_motor.c` |
| alignment by date | — (only nobs) | `lib/fuepre/fue_pre_reader.c` |
| Box-Cox, deterministic terms, differencing, factor expansion | already in `lib/prewhiten` | unchanged |
| `ObsToDate` | an identical copy in drvarma's `diagnose.c` | `lib/dates` |

The reader has its own line size (`FUEPRE_LINE`). It used the host's `MAXSTR`,
which is 200 in drtran and 80 in drvarma, and with 80 the long lines of a
`.pre` were split.

## 6. Version

It is declared in a single place, `include/version.h` (`5.0.0`), together with
the git commit the binary was built from. A version constant alone does not
say which code wrote an output (`docs/DISENO-interfaz.md`, "la versión del
motor no sirve"). The Makefile rewrites `build/git_hash.h` when the hash
changes, so `-version` prints, for instance, `drvarma 5.0.0 (git e36e510421)`,
with `-dirty` when the tree had uncommitted changes. The full string is used
by:

- the CLI usage and `-version`;
- a `Program` line in the `.out` of both paths (the bench ignores it, and the
  deprecation note);
- the header `-split` writes.

## 7. Tests

- `tests/banco/banco.sh`: 18 `.inp` cases, byte for byte against a reference
  that started as the 0.4.1 output and is regenerated with each deliberate
  change.
- `tests/rutas/rutas.sh`: the routes U and V, the echelon form and the three
  paths of the same VARMA; long, outside `make check`.
- `tests/escalera/pruebas.sh`: explicit assertions and a byte regression of
  the 5.0 ladder output. The assertions cover:
  - the gate;
  - drvarma's univariate fit equals fue's, both σ² and coefficients against
    fue's `.out`;
  - genuine `.pre` files are fixed points;
  - BUG-2 rejection;
  - alignment at the end when the series have different lengths;
  - unsupported options are rejected;
  - the WTI → IPC_ES pass-through agrees with the `.inp` bench;
  - the line search returns on a NaN or infinite objective.
- `engines/drtran/test_battery.sh` §16: BUG-2 in drtran.

## 8. Forecasting (phase 2)

    -forecast H            H periods from the end of the estimation sample
    -forecast H -estwin N  estimate on the first N observations of the first
                           series, then forecast from every origin to the end

The system forecasts the stationary series w (`forecast_model`, with the
exact residuals: `elf` with `atf = TRUE`). Each series goes back to its
**level** with its own `.pre` model, through `lib/fuepre/fuepre_forecast.c`:

- the deterministic terms, which are known functions of time
  (`build_det_component`, filtered with fue's rule from `lib/fuedet`:
  ν(B) truncated at 40 lags when there is a δ);
- the integration with its operator `rnsop`;
- the inverse Box-Cox, with its rescaling factor.

That code was inside drtran and now serves both engines: drtran's forecasts
are byte-identical after the move.

The 95% band is computed on the transformed scale and mapped back through the
inverse Box-Cox. It uses the psi weights of the VARMA integrated with
1/`rnsop`ᵢ(B) and Σ = σ²Q. The same weights, differenced at lag 1 and at lag
s, give the standard deviations of the period and annual variations.

With `-estwin`, the parameters are estimated once and held **fixed**. At every
origin the stationary series and the exact residuals are rebuilt on the data
up to that origin, which is what fue does when it forecasts from a `.fuf`. The
output is:

- `NAME.forecast`: the forecast from the end of the estimation window, in the
  format of the `.inp` path;
- `NAME.recursive`: one line per origin, series and horizon;
- the MAE, RMSE and MAPE by series and horizon in the `.out`.

**The check.** The diagonal system (`p = q = 0`, `-diagcov`) on the three CPI
series, with the data extended to 2023-11 (`tests/escalera/data`), reproduces
fue's fixed-parameter forecasts. That covers 3 series × 48 origins × 24
horizons = 3456 values, with a maximum relative difference of 2.3e-6, which
comes from the re-estimated parameters moving ~3e-5 from the `.pre`. The
reference is today's fue. The CSVs in `cases/*/work/*_recursive_eval*.csv`
came from an older fue and do not agree with it: at origin 12/2019, h=1,
IPC_ES gives 97.1389 there and 97.0785 in today's fue and in the ladder.

Not yet: the block Wald tests of the `.inp` path; forecasts conditional on a
scenario for one of the series.

## 9. Where drvarma enters: a cycle is where drtran ends

drtran casts a transfer network as a triangular VARMA, which needs a DAG. When
the identification finds a cycle, two series that feed each other, the system
is simultaneous and drtran has nothing more to do: a cycle is a **dead end**,
not an error to patch inside drtran. drtran's error message now says where to
go.

The way on is back to the ladder: drvarma takes **the same `.pre` files**. That
is not the best way to parametrise a VARMA, because free cross polynomials of
orders p and q for every pair are not a structural model. But it is a good
place to **seed** one:

- each series arrives with its autocorrelation already modelled, if its
  univariate model is adequate, so the diagonal starts at an optimum and only
  the cross dynamics are new;
- the univariate model is **the yardstick for forecasting**. If a VARMA cannot
  improve on the univariate models, it has no reason to exist. The diagonal
  gate certifies that the starting point *is* the univariate models, and the
  LR test measures the cross dynamics against them in sample. The forecasting
  comparison is phase 2: the `.inp` bench already showed, for the CPI trio
  and the WTI pass-through, that significant cross effects in sample did not
  improve out-of-sample forecasts (`MODELS_RESULTS.md` §3-§4).

## 10. One format: the multivariate `.inp` is deprecated

drvarma had its own `.inp`: one file, m columns, a single λ, d and D, the
series names on a line of their own. It was a second dialect called `.inp`
next to fue's univariate one, which is what the rest of the ecosystem reads
and writes: the fue GUI, the mother GUI, `inpcheck`, the conformance battery,
and fue's Python parser, which is the reference. Keeping both means two
readers in C, two in the Python port and a guessing game in every GUI.

Everything the multivariate file expresses can be written in fue's format,
series by series:

| multivariate `.inp` | one fue `.inp` per series |
|---|---|
| one λ, d, D for all | λᵢ, dᵢ, Dᵢ (more general) |
| run option `-mean` | the mean, flagged estimable |
| run option `-scale` (100) | the rescaling factor |
| run option `-deseason` (harmonics computed outside the model) | `cos`/`sin`/`alter` deterministic terms, estimated |
| a full VARMA(p,q) | a free regular ARMA(p,q) in every file (the diagonal) and cross orders p, q |

So from 5.0:

- `drvarma -split FILE[.inp] [-mean] [-harmonics] [-ar P] [-ma Q] [-scale F]
  [-dir DIR]` writes one univariate `.inp` of fue per series. The reader of
  the multivariate file moved out of `main()` into `src/inpread.c`, verbatim,
  and is shared by the `.inp` path and `-split`. `-split` never overwrites a
  file. What it writes is checked in `tests/escalera`:
  - fue's validator accepts it;
  - fue's Python parser reads the same data and transformation;
  - the fue engine estimates it and writes its `.pre`;
  - and the ladder on the files of `-split -mean -ar 1`, with p = 1, reaches
    **the same VAR(1)** as the `.inp` path with `IPC3 1 0 -mean`, with μ and
    the whole of φ(1) identical to six decimals.
- The `.inp` path still reads the multivariate file (tests/banco pins its
  output). It prints a one-line deprecation note on stderr, which the bench
  ignores with the `Program` line. Its only producer, the engine's GTK
  front-end, was retired in November 2026. It is removed in 6.0.
- A multivariate `.inp` given to the ladder (with its extension) is refused
  with fue's validator's reason and the `-split` command to convert it.

The Python port follows the same line: drvarma-python should read fue's files
through `fue.load`, the reference parser, instead of its own `inp.py`.

