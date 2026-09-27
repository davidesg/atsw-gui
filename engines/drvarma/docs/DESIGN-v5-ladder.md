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
before with an `.inp`**. The bench `tests/banco/` stores the 0.4.1 output, and
the `.inp` path must reproduce it byte for byte.

## 1. Usage

    drvarma A.pre B.pre [C.pre ...] p q [-diagcov] [-redet] [-fixarma]
                                        [-m method] [-o NAME]
                                        [-forecast H [-estwin N]]

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
- The deterministic terms stay **fixed** at their `.pre` values. `-redet`
  re-estimates them, and then the stationary series is rebuilt at every
  evaluation.
- `Σ = σ²·Q`, with `Q₁₁ = 1` and `log(Qᵢᵢ/Q₁₁)` free. `est()` concentrates σ²,
  and without this normalisation `Q → cQ` is a flat direction and the Hessian
  is singular (Mauricio 1995, eq. 2.1; the same as drtran). The covariances
  are free unless `-diagcov`.
- Standard errors come from a finite-difference Hessian at the optimum
  (`est_fdhess`), not from the one accumulated by BFGS. This is drtran's fix;
  the `.inp` path does not change.

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

The diagonal system is then optimised. It is the seed of the full model and
the base of the likelihood-ratio test of the cross dynamics:

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
- a `Program` line in the `.out` of both paths (the bench ignores it, and it
  is the only line it ignores);
- the GUI title.

## 7. Tests

- `tests/banco/banco.sh`: 18 `.inp` cases; the 0.4.1 output, byte for byte.
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
  (`build_det_component`);
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

Not yet: the block Wald tests of the `.inp` path; the ladder mode in the GUI;
forecasts conditional on a scenario for one of the series.

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
- The `.inp` path still reads the multivariate file, unchanged byte for byte
  (tests/banco). It prints a one-line deprecation note on stderr, the only
  other line the bench ignores. It stays until the GUI of drvarma, its only
  producer, is migrated to fue's files. It is removed in 6.0.
- A multivariate `.inp` given to the ladder (with its extension) is refused
  with fue's validator's reason and the `-split` command to convert it.

The Python port follows the same line: drvarma-python should read fue's files
through `fue.load`, the reference parser, instead of its own `inp.py`.

