# Joining the ladder: contracts, files, and borrowed code

*What `drvec` shares with `fue`, `art`, `drtran` and `drvarma`, what it takes
from them, and what the suite's own conventions require of it. Established
2026-08-17/18; the working record is `PLAN_BETA.md` §F2.*

---

## 1. The architecture, and where `drvec` sits in it

The suite is a **bilevel optimisation with files as the interface**
(the suite's own account of the estimation ladder):

```
   art  identifies  ->  .inp   (a SPECIFICATION: these values are a starting point)
   fue  estimates   ->  .pre   (an OPTIMUM, in re-runnable form)
                          |
   the consumer reads the .pre and only ASSEMBLES
```

`drvec` is a consumer, and — this took a while to see correctly — it was already
inside the architecture rather than outside it. The keystone is that **the cast
is a function pointer passed to the estimation driver**:

```c
void est( void (*cast)( real *, struct Tvarma *, int *, int, int ), … );
```

`est` does not know what a model is; it knows something will turn a parameter
vector into a `Tvarma`. `drvec`'s `vec_shootx` has exactly that signature. The
suite's own study of the cast puts it plainly: *«the cast is replaceable by
construction»* (the suite's documentation of the parameter translation step §2).

## 2. What `drvec` may and may not do with the files

These are not style preferences. The format has no schema validation, so a file
that parses is not thereby a correct file, and every one of these rules exists
because breaking it fails silently.

1. **`drvec` writes `.inp`. It never writes a `.pre`.** A `.pre` asserts that its
   values are an optimum, and only the program that performed the estimation may
   assert that — the file carries no mark of its author, so a fabricated one is
   indistinguishable downstream from a certified one.
2. **Everything written is pure ASCII.** `fue`'s Python parser cannot read
   Latin-1 (its BUG-0010, open), and this engine's sources *are* Latin-1. The
   test suite checks the bytes.
3. **Every section is written, in order, including the ones that are empty.**
   The parser is positional: the presence of a separator matters even when its
   contents do not. The annual-difference section is the trap — for annual data
   both of `fue`'s writers emit a literal `" 0"`, and omitting it shifts
   everything after it with no error.
4. **`drvec` never calls `fue`'s cast at run time.** That cast keeps its model,
   series and data in module-level globals, so it is not reentrant and two
   cannot be alive at once (the suite's documentation of the translation step, §9). Only the numbers are read.
5. **`μ` follows the deterministic case of the joint model.** Letting `fue`
   estimate a mean the joint model cannot represent returns a `θ` conditioned on
   something that does not exist. Case 1 gets no mean at all.
6. **Scale matters and is a directive, not a transformation.** `refactor` is
   written into the file and applied by `fue`; the data itself goes in raw.
   The value follows the suite's measured rule — the finite-difference gradient
   step is ~6·10⁻⁶ *absolute*, so a series whose typical `|w|` is tiny produces a
   gradient that is noise. The comfortable band is `0.01 … 100`, aiming at ~1.

### 2b. And since 0.9, `drvec` READS `.pre` as its input

Rule 1 says `drvec` writes `.inp` and never a `.pre`. Reading is the other
direction and has no such constraint: a `.pre` is a model somebody else
certified, and consuming one is the whole point of sitting on this side of the
suite.

```sh
drvec s1.pre s2.pre ... sM.pre p q r [options]
```

This is `drtran`'s interface — `drtran output.pre input1.pre ...` — and `drvec`
belongs with `drtran` rather than with `drvarma` for a reason that is
architectural and not cosmetic: **`drvarma` does not share the ladder, it is the
engine the ladder is built on**, so a single `.inp` with every series is right
for it. `drvec` does share the ladder: its design (`PLAN_BETA.md` F2) is that the
univariate identification happens in `fue` and arrives here already done. Until
0.9 its interface did not say so, and the consequence was three manual steps —
export to an `.inp`, `-interv`, `-seed` — of which the second is where `BUG-15`
appeared.

What is taken from each file is what the file is *for*: the series, its
`w = refactor·BoxCox(z)` (the format's contract, and what `drtran.c:885` does),
its deterministic terms, and its calendar. Alignment is **by date**, not by
position. What is *not* taken is the moving average: seeding it is measured to
make the fit worse with `r ≥ 1`, so it stays behind `-seed`.

The route is certified against the old one by identity, not by resemblance — the
two reports are byte-identical below `ESTIMATION SUCCESSFUL`, and the
deterministic handling agrees exactly with `-interv`. Both are in the suite.

## 3. The two contracts, and `drvec` satisfies them

From `LADDER_AS_OPTIMISATION.md` §2.1 and §3:

```
   Σᵢ logL(series i)  =  logL(joint DIAGONAL fit)  ≤  logL(joint model)
        \________________________________/              \____________/
             the factorisation: proves the CROSSING       the extra term

   logL(diagonal fit)  ≥  logL(AT the stored values),
        with equality iff the stored values are the univariate optima
        -> the CERTIFICATE: one evaluation, no optimisation
```

The first says the transformation, the differencing, the scaling and the seeds
all arrived intact. The second is a free optimality certificate: its **sign**
tells an analyst whether the files they were handed are optima or something that
still needed estimating.

Measured on mink–muskrat at the diagonal rung
(`2 1 0 -case 1 -diagar -diagma -diagcov`), seeding from `.pre` files that `fue`
wrote from `drvec`'s own `-writeinp` output:

| | |
|---|---|
| joint logL **evaluated** at the `.pre` values, no optimisation | −34.6278402874 |
| sum of the univariate logL, computed by `drvec` from the `.pre` | −34.6278225170 |
| **crossing identity** (difference) | **1.8·10⁻⁵** |
| logL of the diagonal **fit** from that seed | −34.6278400462 |
| **certificate** (fit − evaluate) | **+2.4·10⁻⁷ ≥ 0** ✔ |

**The two differences do not have the same cause, and until 2026-08-21 this
paragraph said they did.** It read that both were the format's own rounding.
Measured, on the diagonal rung with `-diagar -diagma -diagcov`:

| | truncation on (`-m 1`) | off (`-m 2`) |
|---|---|---|
| `q = 0` | −1.123e−12 | −1.123e−12 |
| `q = 1` | **−1.773e−05** | **−1.634e−10** |

* **The crossing identity is the `ξ` truncation**, not the format. Switching the
  truncation off moves it five orders of magnitude, and at `q = 0` — where there
  is no series to truncate — it is machine zero whatever `-m` says. The reason
  is in `gate_contract`: the joint system and the univariate ones do not
  truncate at the same term, because the joint sums `m` entries and each
  univariate one. The tolerance the program applies is `xitol` itself for
  `q > 0` and `1e−6` for `q = 0`, which is the only thing that can honestly be
  claimed: the two routes agree **as far as the approximation reaches**.
* **The certificate is the format's rounding.** That one evaluates the
  likelihood *at the stored values*, and a `.pre` keeps its coefficients with
  `%.6f`, so `%.6f` is what bounds how sharp it can be.

**And it corrects a comparison made in the production plan.** That plan recorded
the sibling program's gate at −1.50e−07 against this one's 1.8e−05 and called
`drvec` "two orders of magnitude worse". The two numbers were never comparable —
different models, different `q`, and one of them carrying a truncation the other
does not — and with the truncation off this side is 1.6e−10. The lesson is the
plan's own rule turned on itself: a figure taken from another program's document
is not a measurement of this one.

**And the program now claims both contracts itself**, not only the test suite.
Any run at the diagonal rung prints the crossing identity — computing the
univariate side from the fitted diagonal blocks, so no external file is needed —
and, whenever the run was seeded, the optimality certificate beside it. The
protocol is the transfer-function program's, adopted rather than reinvented: the
likelihood at the values brought in is taken **before** the fit overwrites them,
since once the optimiser has run the question can no longer be asked. Reported,
never refused: both kinds of input are legitimate, and what was missing was
being told which one arrived.

```
  --- optimality certificate ---
  logL AT the values brought in   =     -34.6278402874
  logL of the diagonal fit        =     -34.6278400462
  optimality gap (fit - brought)  = +2.413e-07
  largest coefficient movement    = 5.162e-05

  The gap is >= 0 always, and zero exactly when what came in were
  the univariate optima.  Here they were: this input is AN OPTIMUM.
```

The verdict's threshold is the suite's, and it is **measured rather than
chosen**: a genuine `.pre` does not return exactly to its own values, because
the format stores six decimals and the optimiser stops inside its own tolerance;
fed the specification beside it, the same gate reports a gap of 6.15 and a
movement of 0.97. Four orders of magnitude separate the two, and 1e-3 sits
comfortably between them. Both contracts and both verdicts are checks in the
test suite, the verdict in both directions.

The same computation reproduces the two univariate constants of the
factorisation gate — **−20.057976** and **−14.569847** — by a third independent
route, the first two being `drvarma`'s Python port and `fue` itself.

### σ² is not in the `.pre`, and is derivable anyway

The `.pre` carries no innovation variance; that field exists only in `fuf`
files. But it carries **the model and the data**, so σ² is recovered by
evaluating the univariate likelihood with the same `elf` the whole suite uses.
Stopping at "it is not in the file" is half the argument.

## 4. How far the univariate information travels: exactly one rung

At `r = 0` the transformation collapses (`C̄ = I`, `H̄ = 0`) and the univariate
optima *are* the joint model's diagonal blocks. Above that they are not, and the
measurement is unambiguous — seeding the full univariate block at `r = 1` moves
the **starting** log-likelihood down by 17 units:

| configuration | cold start | seeded start |
|---|---|---|
| `-case 1` (r=1) | −283.33 | −256.56 (+26.8) |
| `-case 2` (r=1) | −8.74 | **−25.95 (−17.2)** |
| `-case 3` (r=1) | −8.66 | **−25.79 (−17.1)** |

Three reasons, all structural rather than numerical:

* the marginal of one component of `Ȳ` is **not** the corresponding diagonal
  block of the joint model — marginalising a VARMA mixes AR and MA and inflates
  the orders;
* `C̄` and `Λ` couple the equations densely, so a per-component fit cannot see
  the coupling;
* `Φ̄_p = −F_{p−1}C̄⁻¹H̄` is determined, so imposing all `p` autoregressive
  matrices from univariate fits is over-determined.

**So the ladder hands `drvec` a certified starting point at the diagonal rung and
nothing above it** — which is precisely where `drtran` puts its own entry gate.
Seeding a cointegrated model needs information about the coupling, which is what
the conditional regression in `init_guess` already supplies.

## 5. Borrowed code, and its provenance

`drvec` is C, so the reference implementations are the C ones. `drtran` in C is
the direct precedent: it uses `fue`'s cast and `drvarma`'s engine, which is
exactly what `drvec` does.

| what | from | changes |
|---|---|---|
| the `.pre` reader | the transfer-function program of the suite | two: one unnecessary header include removed, and **one defect fixed** — see below |
| the date-conversion helpers | the transfer-function program of the suite | none |
| the operator-expansion routines | the transfer-function program of the suite | one constant renamed |
| the univariate model structure | the transfer-function program of the suite | copied unchanged |

`struct Tseries` gained `numbering` and `refactor`, the two fields `drtran` added
for FUE. That change is inert: `Tseries` is not referenced in any `.c` of
`drvec`.

**Why copy rather than link:** `fue`'s own reader lives inside `src/fue.c` and is
not linkable; `drtran`'s is the only factored-out reader in C. Linking against
`drtran` would couple two programs that are independent by design. The cost is
that the copy can drift, which is why the provenance is recorded in the file
headers as well as here.

### The bug found on the way in

`drtran`'s reader skips the annual-difference section when `freq == 1`, but both
of `fue`'s writers emit it always. On an annual `.pre` everything after it shifts:
`refactor` comes back from uninitialised memory and the series is read two
positions late, losing the last two observations — with `read_fue_pre` returning
**success**. The univariate program's own reader retains the branch that the extracted copy
lost, so the defect entered with the extraction and does not originate in the
published code.

Fixed in `drvec`'s copy and verified against `fue`'s parser; filed in the defect register of the program it came from. It is not fixed in `drtran`, where it is live:
that program does read the series from the `.pre`.

### The engine, and the one correction it required

The four engine files are carried unchanged in their numerical content: the
exact likelihood, the estimation driver and the quasi-Newton optimiser are
byte-for-byte as published, and `elf()` in particular is never touched. One
correction was necessary in the supporting allocator library, and it is recorded
here because the claim "the engine is unmodified" would otherwise be imprecise.

`tensor()` allocated `nrh + 1` row pointers and then wrote at `t[nrl…nrh]`. For
a non-negative lower bound this merely over-allocates, which is why it had never
shown. The likelihood routine, however, allocates the cross-covariance array
with a lower bound of `−q + 1`, which is **negative** as soon as the model has
two or more moving-average lags; the write then landed before the start of the
block. The result was heap corruption and an abort, so no model with `q ≥ 2`
could be estimated at all.

**The correction was not devised here.** The defect belongs to the
Numerical-Recipes cleanup that reimplemented these allocators: the original
allocated `nrh − nrl + 1` pointers and offset the base, and the cleanup replaced
that with `nrh + 1` pointers indexed from zero, which silently withdraws support
for a negative lower bound. It was diagnosed and fixed elsewhere in the suite on
2026-06-15, and the univariate program's defect register records the symptom
("double free or corruption"), the cause (`tensor(-q+1, 0, …)` with `q ≥ 2`) and
the remedy. The same correction is already present in the transfer-function and
VARMA programs and in the C sources that both Python ports compile. What `drvec`
was carrying was the pre-fix copy, and it reached the fault by the ordinary
route of a user asking for `q ≥ 2`. The code adopted is the suite's, verbatim,
so `tensor()` and `free_tensor()` are now byte-identical to the shared copy.

Two properties of the correction matter for homologation. It changes no computed
value: where the lower bound is non-negative — every call site in `drvec`
itself — the slots addressed are exactly the ones addressed before. And it is
verified rather than asserted — the whole regression suite, the golden
log-likelihood baselines included, is unchanged, and the previously fatal
configuration now runs clean under `valgrind`. Both are permanent checks
([TESTING.md](TESTING.md)).

### The allocators, and which of them are now the suite's

`tensor()` was the only member of the family `drvec` could reach with a negative
lower bound, so it was the only one whose defect was live. `vector` and
`ivector` carried the same defect latently, and have since been aligned as well;
`matrix` and `imatrix` have not, and deliberately so.

| routine | state in `drvec` | why |
|---|---|---|
| `tensor`, `free_tensor` | **the suite's, identical** | was live: `gamwa` with `q ≥ 2` |
| `vector`, `ivector` | **the suite's, identical** | was latent — no call site here uses a negative lower bound — but the suite corrected these after the transfer-function program was bitten by one, its identification step allocating `vector(−nlags, nlags)` |
| `matrix`, `imatrix` | **the suite's, identical** — adopted 2026-08-21 | the reason for leaving them had been measured and did not hold *here*; see below |

**Measured first, then done.** On 2026-08-20 the four routines were replaced by
the shared variant in a copy of the tree and the whole suite was run: every
functional check passed, no golden value moved, the diagonal gate still closed
and the `.pre` still read. The only consequence was under `valgrind`, and it was
the mechanism described two paragraphs below — the shared variant **reveals
leaks that the previous one conceals**, because a pointer returned at the base
of its block looks reachable and an offset one does not. So the incompatibility
is the univariate program's and **not** `drvec`'s.

Adopted on 2026-08-21. **Stripped of comments, `nlatools.c` now differs from the
canonical copy in zero lines**, and so do `drtran`'s and the Python package's:
four copies, one file.

**It revealed five leaks, not two.** The prediction was `cond_resid` and
`rawmat`; the other three appeared once the paths were run — `alpha_A` in the
`α = Aψ` route, and `datamat` and `Y2_levels` in `-lrtest` and `-rungs`, which
call `build_y2_levels` once per rank. All five are of process lifetime and none
harms a batch program. What they harm is the next one: a `valgrind` report with
five known leaks in it is a report in which the sixth is invisible.

They are closed in `free_case_data()`, called from `cleanup_names()`, which is
where all seven exits of `main` already passed. And doing that found one more
thing: `main` had been freeing `datamat` and `Y2_levels` with `nobs` rather than
with the dimension they were **allocated** with, which stopped being the same
number when `-estwin` began truncating the estimation sample. That free is gone;
there is one now, and it uses the allocation's own dimensions.

Aligning `vector` and `ivector` is not a cosmetic change, because it moves the
release from ignoring the bounds it is given to depending on them: every
`free_vector` must now be handed the same lower bound its allocation used. That
was checked mechanically rather than by reading. The allocator was instrumented
to record the lower bound against each pointer returned and to verify it at
release, and the whole program was then exercised — nineteen configurations
spanning every case, the diagonal restrictions, multi-start, the rank test, the
bootstrap, `α = Aψ`, seeding from a `.pre`, both writers, the legacy layout and
the `.pre` reader's own harness, plus the entire regression suite. No allocation
was released under a lower bound other than its own, and no release reached a
pointer the allocators had not produced. The instrumentation was then removed.

**And the alignment paid for itself immediately.** The offset form does not only
remove a latent fault; it makes leaks visible that the previous form concealed.
A pointer returned at the base of its block looks reachable to `valgrind` even
when nothing will ever free it, whereas an offset pointer does not — so the
first run after the change reported 32 bytes definitely lost that had been
sitting silently in the seeding path. The four buffers `load_seed_pre` allocates
had no deallocator at all. One was written, and the memory block of the test
suite now covers it.

The consequence for results already reported is stated in
[HOMOLOGATION.md](HOMOLOGATION.md) §3b: specification searches conducted before
the correction could not reach `q ≥ 2`, and were bounded by a defect rather than
by a modelling decision.

### The engine's chi-square, and a defect given back

`chisq()` in `nlatools.c` is documented as the cumulative χ² and, for `df ≥ 30`,
applied **two** tail corrections instead of one. The Abramowitz–Stegun
polynomial at `|z|` gives the upper tail of `|z|`, which for `z < 0` already
*is* the lower tail of `z`; the second correction flipped a value that was
right. And `z < 0` is a statistic **below** its mean — the case where the model
is fine — so every p-value written `1.0 - chisq(...)` came out complemented and
the residual diagnosis declared clean residuals non-white. Measured:
`Q = 23.4777` on 40 d.f. has `p = 0.9825` and was printed as 0.0175 with
*"REJECT H0"*.

Found from here on 2026-08-19, recorded as BUG-13 in the suite's defect
register, and **fixed on 2026-08-21 in the four copies at once** — the canonical
`drvarma`, the Python package's, `drtran`'s and this one — character for
character, because a divergent fix to a shared file is worse than the defect.
Checked first that no caller had adapted to it: all of them write
`1.0 - chisq(...)`. Verified against `gsl_cdf_chisq_P`: agreement to 1.9e−04 for
`df ≥ 30`, which is Wilson–Hilferty's own error, and to machine precision below.

`drvec` still does not use the function — `diagnose_mv.c` routes through
`gsl_cdf_chisq_Q`, which is the upper tail directly and needs no `1 - …` — and
that is now a deliberate belt-and-braces rather than a workaround. The battery
watches `chisq` anyway, in `[8h]`, on the grounds that it belongs to a file two
other programs execute and neither of them has an automatic suite.

## 6. What `drvec` gives back

Two things the suite asked for and did not have:

* **the parameter-vector layout is machine-checked.** The suite's documentation
  of the translation step leaves open whether the packing order should be
  verifiable rather than merely documented;
  `drvec`'s test suite asserts that the walk consumes exactly `npar` in 24
  configurations, and that check has already caught two real defects.
* **a third independent confirmation** of the factorisation gate's constants.

## 7. Why the univariate rung comes first: a measured defence against a spurious rank

The suite's order — `art`/`fue` first, then `drvec` — reads as ergonomics: the
univariate work is done, so do not do it twice. It is not. It is a defence, and
this section measures how much it buys, on a case where the answer was already
published.

### 7.1 The case

Bolivia's real exchange rate (TFM, M. Tapia Torrico 2026). Three quarterly
series, `n = 84`, a VECM with `r = 1` on `(ln ITCER, ln P_gas, RATIO_SA)`, and on
that single cointegrating relation the whole applied contribution: the
equilibrium exchange rate, the misalignment, the required depreciation.

The specification carries three break dummies as exogenous regressors —
`d2011` (step 2011Q4), `d2015` (step 2015Q1), `d_covid` (pulse 2020Q2–Q4) — and
Johansen's trace for `r = 0` is **32.676** against a 5 % critical value of 29.80.
It rejects by 2.9 points. The figure is reproduced here exactly.

### 7.2 What the result rests on

Changing one thing at a time:

| specification | trace `r=0` | verdict |
|---|---|---|
| as published | **32.676** | r = 1 |
| `RATIO` in logs instead of levels | 31.724 | r = 1 |
| `RATIO` **not** seasonally adjusted | **32.676** | r = 1 |
| **without the three break dummies** | **21.379** | **r = 0** |

The transformation does not matter. The seasonal adjustment does not matter *at
all* — the statistic is identical to the digit, because the centred seasonal
dummies are already in the auxiliary regressions and adjusting beforehand is
redundant. **The three break dummies carry the entire result.**

### 7.3 And the dummies are a modelling choice, not a measurement

None of the three is dated in **2008-09**, which is the largest episode in the
sample and the one every series in the system identifies. Adding it, in the eight
forms a careful analyst might defend:

| form added to the published specification | trace `r=0` | verdict |
|---|---|---|
| step from 2008Q2 | **45.367** | r = 1 |
| step from 2008Q3 | 32.006 | r = 1 |
| **step from 2008Q4** | **29.773** | **r = 0** |
| step from 2009Q1 | 43.386 | r = 1 |
| impulse at 2008Q4 only | 39.490 | r = 1 |
| pulse 2008Q4–2009Q1 | 42.942 | r = 1 |
| pulse 2008Q2–2009Q2 | 41.431 | r = 1 |

**Between 29.77 and 45.37**, for the same episode, the same data and the same
sample. Moving the start date by one quarter — 2008Q3 to 2008Q4 — moves the
statistic 2.2 points and crosses the threshold. The verdict is decided in the
third decimal place of a statistic whose critical value, with step and pulse
dummies in the auxiliary regressions, is not the tabulated one anyway and errs
in the unfavourable direction (Johansen, Mosconi and Nielsen 2000).

### 7.4 What the ladder does about it

The forms above are not equally supported by the data, and the univariate rung
is where that is decided — **with a contrast, not by hand**:

| the ladder's instrument | what it settles about the episode |
|---|---|
| `residual_episodes` / `incident_configurations` | how many periods of the **level** the event alters, and which configurations the data admits at all |
| the **gain contrast** `H0: ω(1) = 0` | permanent or transitory — the difference between a step and a pulse, which above is worth 12 points of trace |
| the **Treadway rule** | whether the chosen form left an anomalous neighbour, i.e. whether the date is off by one — worth 2.2 points and the verdict |
| the **Ockham ladder** | climbs a rung only when something justifies it, and **the AIC does not arbitrate the climb** |

On this case the ladder answers: for `RATIO`, a **permanent step at 2008Q4**
(ω = 18.12, t = 5.22; the gain contrast rejects `ω(1) = 0` at p = 0.0002, and the
`×5` general form was abandoned for collinearity after it had served as a probe).
For `PGAS`, **three impulses, transitory**. The two series do not ask for the same
form, and that is itself information the system-level dummy cannot represent.

And the form the univariate contrast supports — the step at 2008Q4 — is exactly
the one that leaves the published result at **29.773 against 29.80**.

### 7.5 The architectural claim, stated so it can be argued with

> **A cointegration verdict that moves 15 points of trace according to how one
> episode is dated and shaped is not a property of the data. It is a property of
> the specification — and the specification is being chosen at the level where
> there is no contrast to choose it with.**

At the system level, an intervention is an exogenous column: the analyst types a
date and a shape and the estimator accepts them. There is no `ω(1) = 0` test, no
Treadway neighbour, no ladder — nothing that can say *this form and not that
one*. At the univariate level all four exist, they run on one series at a time
where they have power, and they leave their reasons written in the `guion`.

So the order of the suite is not a convenience. **`fue` and `art` are where the
deterministic part of a system model becomes falsifiable**, and passing through
them is what stops a rank from resting on an undocumented dummy. This case is the
measurement of what that is worth: on a published result, the difference between
`r = 1` and `r = 0`.

Two consequences for `drvec` itself, which are work items and not rhetoric:

1. **The `.pre` route already does the right thing and does not say so.** It
   subtracts each series' deterministic terms *with its own dates and forms*,
   which is how the univariate verdict reaches the system. The report does not
   record which terms were subtracted, so a reader cannot tell a fit that
   inherited a contrasted form from one that inherited a guess. It should print
   them.
2. **A rank test is worth little without its specification alongside.**
   `-lrtest` reports the sequence; it could report, next to it, what deterministic
   terms each series brought — the closest thing to a robustness statement that
   costs nothing to produce.

**Provenance.** Measured on the Bolivia replication, 9 September 2026; the full
figures, including `drvec`'s own `-lrtest` sequence (`LR = 3.07` for `r=0`
against `r=1`; AIC and BIC both selecting `r = 0`) and the adjustment coefficient
of the normalising series under an imposed `r = 1` (`-0.0033`, t = −0.07), are in
`TFM_UCM/Tesis_Michael/replica/run5_guiado/COINTEGRACION.md`. The Johansen
implementation used was verified against an independent replication that
reproduces the published tables exactly.
