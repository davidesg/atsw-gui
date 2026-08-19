# Development record

*What was tried, what was measured, and what was rejected. The Spanish documents
in this directory are the primary source — `ANALISIS_PRELIMINAR.md` for the
diagnosis, `PLAN_BETA.md` for the plan and its exit criteria,
`ESTUDIO_BVECM_vs_DRVEC.md` for the legacy program — and this is the account in
one place, in order.*

**Why a record of rejections and not just of features.** Three of the most useful
results here are things that did not work: a reparameterisation that looked free
and was not, a seeding route that was well motivated and measurably wrong, and a
conclusion that was right about the numbers and wrong about what they meant.
Each of them cost a day and would cost it again if only the successes were
written down.

---

## 0. Method

Four rules, adopted after the first of them was broken:

1. **A claim about the code is measured, not argued.** Every number in these
   documents was produced by a command that can be re-run.
2. **A change that moves results says so.** Two of the initial corrections move
   estimates on purpose; the rest do not, and that distinction is recorded.
3. **A test suite is only as good as its measured bite.** Every claim that the
   suite protects something is checked by putting the defect back and counting.
4. **A golden value is only valid for the exact input it was measured on.** This
   one was learned by measuring the same model on the same data written with 8
   and with 10 decimals and getting different answers.

---

## 1. Starting point: seven corrections, and one of them was identification

Applied to `src/drvec.c` only; the engine — `elfvarma.c`, `drvmlest.c`,
`qnewtopt.c`, `nlatools.c` — is byte for byte untouched, and `elf` in particular
is never touched: it is the published AS 311, validated here to **2·10⁻⁸**
against a multivariate normal computed by hand.

| | what | kind |
|---|---|---|
| §3.5 | seed the covariance and report `Σ = sigma2 · Q` | seeding + output |
| **§3.6** | **remove the redundant covariance parameter (`Σ₁₁ = 1`, `npar` − 1)** | **identification** |
| §4.1 | the output respects `-diagar/-diagma/-diagcov` and stops reading past `x[]` | output |
| §4.2 | `B₂` was printed transposed | output |
| §4.3 | read `Y₂` **in levels** (now the default); levels built once | data |
| §4.4 | `-lrtest`: the sequential rank test, previously absent | feature |
| §4.5 | file-name buffers sized by the path, not fixed at 80 bytes | robustness |

§3.6 is the one that matters. The concentrated objective is exactly invariant to
rescaling the covariance block, so the parameter vector carried a direction the
likelihood cannot see — an exactly flat ridge, which is why the line search
failed and the Hessian was singular. Removing it raised the log-likelihood in 5
of 6 configurations and made the standard errors computable.

**What is still not reproduced:** the EML column of Mauricio (2006) Table 5. The
discrepancy is localised — the engine, the transformation and the data all check
out independently — but it is not closed, and it cannot be closed with the
material available. The program is therefore homologated against a criterion on
`|Σ̂|`, which is invariant across the representations, rather than against the
published log-likelihoods.

---

## 2. F0 — the safety net

Everything above had been verified by hand, which was the largest process risk
in the project. `tests/run_tests.sh` turned it into a script.

**And it was checked that the suite bites**, by reintroducing four already-fixed
defects. Three were caught; the fourth — `B₂` transposed in the printer only —
was not, and that gap is written into the script rather than left implicit. It
also forced a new fixture: with `s = 1`, which is what every case in the bank had
until then, transposing a matrix is a no-op, so a Danish money-demand case with
`M = 5, r = 2` had to be added to make the estimator's read order detectable at
all.

The method note that came out of this phase, and that has since bitten twice
more: **a golden value is only valid for the exact input it was measured on.**

---

## 3. F1 — hardening Σ, and a premise that was false

Three changes were planned, copied from `drtran`'s established practice.
**Two were kept and one was reverted with evidence.**

| | outcome |
|---|---|
| seed the covariance at the **variance ratios** of the data instead of the correlation matrix | kept |
| an explicit positive-definiteness check at translation time | kept |
| `var_i = exp(x_i)`, making positivity structural | **reverted** |

The phase had been written on the assumption that `exp()` was free — *"if it does
not improve convergence it is kept anyway, since guaranteed positivity is
strictly better"*. **That premise was false.** Combined with informative seeding
it drove the optimiser into regions where each likelihood evaluation is very
slow: a configuration that took 0.04 s and 45 iterations stopped finishing at all
within 90 s. Bisected, neither change hangs alone; it is the combination. It also
lost log-likelihood in three of four configurations.

The seeding change is worth its own line, because the size of the effect was not
expected: seeding the variance **ratios** rather than putting them all at 1 is
worth **+79.26** in log-likelihood on Danish money demand (M = 5, r = 2), which
mixes logarithms with interest rates. On the small bivariate case it is worth
almost nothing. The failure mode only shows up when the series differ in scale —
which, for anything but a set of comparable series, is the normal case.

**Exit criterion: met in agreement, not in level.** The four equivalent
configurations of the same model went from a spread of 0.000225 in `|Σ̂|` to
0.000048 — but they agree at ~0.00248 against a target of ~0.00230. And the table
shows something that has to be said: before the change, one configuration sat at
0.002344, *closer* to the target than anything now. **The spread closed upwards.**

Two corrections to the documentation came out of this phase:

* the acceptance band «0.0022–0.0024» had cited Chan & Wallis in support although
  their value, 0.00246, falls outside it. The target is the global search over
  the same model and data (~0.00230); Chan & Wallis calibrates magnitude and is
  not the target;
* the legacy-layout rows of the comparison tables had been measured on a copy of
  the input written with `%.8f` while the test suite generates `%.10f`. Both are
  right on their own input. The 8-decimal file is now in the repository, because
  without it the published numbers are not reproducible.

---

## 4. F2 — the bridge to the suite, and a conclusion drawn too early

The plan said `drvec` should stop starting cold and seed itself from `fue`'s
`.pre` files. Building the bridge was straightforward; **the conclusions took
three attempts to get right, and the corrections are the interesting part.**

### 4.1 What the bridge is

`drvec` writes one `.inp` per equation (`-writeres`, `-writeinp`), `fue` estimates
each and leaves a `.pre`, `drvec` reads the numbers back (`-seed`, `-seedybar`).
It never writes a `.pre` and never calls `fue`'s cast at run time. The rules and
their reasons are in [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md).

A defect in the borrowed reader was found and fixed on the way in, filed as
**BUG-11** against `drtran`: every annual `.pre` was read wrong, silently.

### 4.2 First conclusion: "seeding does not help" — measured, and mis-framed

Seeding `Θ` was measured across six configurations and was worse than the cold
start in four. That measurement stands. **The framing did not.**

Two objections, both correct, were raised against it:

**"Check the sign convention. The MA operator's sign has caused problems in this
suite before."** It had not caused one here — the chain was verified at source
level across `elf`, `fue`'s cast, `drtran`'s expansion and `drvec`'s own, all
four agreeing on `Θ(B) = I − Σθ_jB^j`, and negating the seed makes the starting
likelihood worse. But the objection forced the diagnostic that had been missing:
**the likelihood at the starting point**, which is what separates a bad seed from
a bad path. `-eval` exists because of it. With it: the seed starts *better* in 5
of 6 configurations, and only case 1 starts worse — where the cause turned out to
be that the conditional regression demeans `W` even in the case that admits no
mean.

**"A `.pre` seeds at the optimum. `drtran` estimates the diagonal model as a
pre-contract of optimality. Study the ladder's contracts — `drvec` belongs to the
same suite and should share them, and duplicating that work reinvents a wheel and
makes it square."** This was the one that mattered. Seeding only `Θ` and leaving
`Λ`, `F` and `Σ` from the conditional regression produces a point that **is
nobody's optimum**, so there is no certificate to claim and the question "does the
final log-likelihood improve" is not the question the ladder defines.

### 4.3 Second conclusion: the contracts, and they hold

With the **whole univariate block** seeded, at the diagonal rung, the suite's two
contracts hold: the crossing identity to 1.8·10⁻⁵ and the optimality certificate
at +2.4·10⁻⁷ ≥ 0, both bounded by the `.pre`'s own `%.6f` rounding. Numbers in
[SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §3.

Two things believed impossible turned out not to be:

* **σ² is derivable from a `.pre`** even though it is not stored in one: the file
  carries the model *and* the data, so evaluating the univariate likelihood with
  the same `elf` recovers it. Stopping at "it is not in the file" was half an
  argument.
* **`r = 0` is a first-class configuration.** It used to be reachable only from
  inside `-lrtest`, which is exactly where it cannot be inspected — and it is the
  rung where the contracts live.

### 4.4 What survives of the negative result

Above the diagonal rung the univariate information does not transport, and now
there is a mechanism rather than a mood: the marginal of a component of `Ȳ` is
not the joint's diagonal block, `C̄` and `Λ` couple, and `Φ̄_p` is determined by
`F_{p−1}`. Measured, the full-block seed starts 17 units below the cold start at
`r = 1`.

**And the most useful thing the phase produced was not about seeding at all.** In
case 1, moving `Θ` from exactly zero to `diag(0.0108, 0.0611)` moves the answer
by **14.45 units**. That it is not a plumbing defect is proved by an identity now
in the test suite: with `θ = 0` the seeded path reproduces the cold start bit for
bit. What is being measured is the surface. `drvec`'s difficulty is not where it
starts; it is where it can walk.

---

## 5. F3 — the interpretable layer (in progress)

The first piece is done: **`α = Aψ`**, the class of linear restrictions on the
adjustment coefficients from Johansen and Swensen (2024). Weak exogeneity is the
special case where `A` selects rows, so there is no ad-hoc test — `-weakex i` is
a shorthand that builds the `A`.

This is where `drvec`'s parameterisation pays. `Λ` is *in* the parameter vector,
so the restriction is a substitution inside the translation step — the same kind
of change as `-fixb2` — and the standard errors come from the Hessian. The legacy
program needed the delta method with an SVD pseudo-inverse for the same test,
because in its coordinates `α` is derived.

On mink–muskrat with `p=2, q=1, r=1, -case 2`:

| hypothesis | LR | df | p |
|---|---|---|---|
| muskrat equation does not adjust | 26.573 | 1 | ≈ 0 |
| mink equation does not adjust | 3.590 | 1 | 0.058 |

The prey adjusts and the predator is weakly exogenous at the 5 % level, which is
the direction the biology predicts.

Adding the restriction immediately exposed a real defect through the suite's
structural block: the output printer still walked the parameter vector as though
`Λ` had `M·r` entries, and consumed 24 of 22. That is the fourth walk of the same
vector, and the reason that block exists.

### `Π`, and a caveat that turned out to be stronger than the literature's

`Π = ΛB′` is now reported with its eigenvalues. It is the quantity to compare
fits on, because unlike `Λ` and `B` it does not move under a reparameterisation
of the cointegrating space.

The eigenvalues carry a warning, and writing it forced a sharper statement than
the one that was planned. The plan quoted Mélard, Roy and Saidi (2004): the
assumption on `Φ(1)` does not imply that reading the eigenvalues of `Π̂` as a rank
criterion is valid. True — but **in this parameterisation the objection is much
worse than that**: `Π = ΛB′` is *built* with rank `r`, so its `M − r` zero
eigenvalues are guaranteed by construction and reading them as evidence for `r`
is circular. The output says so.

### The normalisation alarm, and the case built to fire it

`B = [I_r ; B₂]` normalises on the `Y₁` block. If that block does not appear in
the cointegrating relation, `B₂` inflates and the fit describes a relation among
the *other* series — with nothing in the output looking wrong. This was the trap
Mélard et al. exposed and beta criterion 6c.

The measure is unit-free: in `W = Y₁ + B₂′Y₂` each series weighs
`|coefficient| · sd(series)`, and what is reported is the share carried by `Y₁`.

| | share of `Y₁` | |
|---|---|---|
| `mink_muskrat` | 75.5 % | quiet |
| `datasets/synthetic/badnorm.inp` | **0.6 %** | **warns** |

The second file is committed, and it is the point: **an alarm with no case to
fire on is not an alarm.** It is built so that two series cointegrate and the
third — the one the column order puts in `Y₁` — is an independent random walk.
The suite checks both directions, that it fires there and stays quiet on real
data, and disabling the alarm raises a failure.

### `Σ = P D P′`, and a coverage lesson

The LDL′ triangularisation: `P` unit lower triangular, `D` diagonal, so that
`Aₜ = P A*ₜ` with `A*ₜ` uncorrelated and the system readable one equation at a
time. The legacy program had this for the bivariate case only; here it is
general.

The check that `P D P′` reconstructs `Σ` runs on **M = 3, not on M = 2**, and the
reason is only visible by measuring: with `M = 2` the LDL′ inner loop never
executes — there is no third variable for the cross term to accumulate over — so
a mutation of that term is invisible. Negating it raises **0 failures on M = 2
and 1 on M = 3**. A test written on the smallest case would have been decoration.

**F3 is closed** apart from what the plan assigned to F4.

---

## 6. The `|Σ̂|` level, closed with multi-start

The last criterion open from F1, and the answer came from putting together two
measurements that were already in hand rather than from new theory:

* the outcome is strongly **path-dependent** (F2: hundredths in `Θ` move the
  answer by units);
* the global search that provides the `|Σ̂|` reference **was itself a
  multi-start**, and it stops pressed against `chekma`'s invertibility barrier
  at `max|λ(Θ₁)| = 1.00005`.

The check that settled it: `drvec`'s own fit sits at **exactly the same
barrier**, `max|λ(Θ₁)| = 1.000050`, but at a different point of it. Same edge,
worse place — so the problem was never the admissible region.

**Rejected before building anything.** A grid over `B₂` with full fits is
useless here: scanning `-fixb2` from −0.60 to 0.10, the log-likelihood jumps from
−7.4 to +6.5 between −0.30 and −0.24, which is not a profile likelihood but
optimiser failures contaminating the scan — and even at the best `B₂` the `|Σ̂|`
stays at 0.002466. `B₂` was not the cause. That is the plan's own contingency,
tried and discarded on evidence.

**`-multistart n`** — not touching the optimiser, running it more than once, with
deterministic perturbations so a result can be reproduced.

| | one start | `-multistart 60` |
|---|---|---|
| four equivalent configurations | 0.00246 – 0.00251 | **0.002344 – 0.002358** |
| spread | 0.000048 | **0.000014** |
| above the reference (0.002311) | +7 % | **+1.6 %** |

**And a design defect the measurement caught.** The first version scaled the
jitter amplitude by `n`, so asking for more starts *changed* the set instead of
extending it: 24 starts gave 0.002349 and 40 gave 0.002453. The ladder now
depends only on the start index, which makes the procedure monotone — the first
`n` starts of a long run are the starts of a short one — and that is an invariant
in the test suite. Without measuring across several `n` this would have shipped
looking fine.

## 7. After the beta: a false diagnosis of my own, and a defect beneath it

The question that opened this was narrow — whether `-fdhess`, which computes the
Hessian at the optimum instead of accumulating it along the optimiser's path,
should be the default rather than an option. Since the program's purpose is
inference on the cointegrating coefficients, the standard errors are the
product, and having the better route be opt-in is the wrong way round.

The measurement said it could not be: on six of seven configurations the
finite-difference Hessian was not positive definite. The first reading of that
was that the optimiser had not stopped at a maximum, and the program was made to
say so. **That reading was wrong, and the fault was in the measuring
instrument.** `fdh_obj` answers `10¹⁰` at a parameter point the likelihood
rejects. Counting those answers showed 42 of them inside a single Hessian
evaluation: the "negative eigenvalues" were the penalty, not curvature, and the
gradient norm of 10¹⁵ that appeared to confirm a non-stationary point was the
same artefact. A diagnosis was being published that the evidence did not carry.

What the evidence does carry is more useful. The rejections come from the
invertibility check, and the estimated moving-average operator has a root at
0.99995 — the boundary the engine enforces at 1.00005. The optimum is on the
constraint. That is not particular to one set-up: every specification with
`q ≥ 1` behaves identically, across all three cases, with and without
`-diagma`, and with sixty restarts. It also explains what had been recorded as
three separate open items — termination on termcode 3, the dependence on the
starting point, and the standard errors — as one thing: these are constrained
optima, and the unconstrained apparatus does not apply at them.

Three changes followed. Every fit reports the moduli of the roots of both
operators and marks any on the unit circle, which makes the condition visible
instead of inferable. `-fdhess` distinguishes the two cases that had been
conflated — an optimum on the boundary, where no unconstrained curvature exists,
from an indefinite Hessian built entirely from admissible evaluations, which
would genuinely indict the optimiser — and states only the one it can support.
And the reading is recorded: a unit moving-average root here is the signature of
overdifferencing (Plosser and Schwert, 1977), which in this model is governed by
the declared rank, so the rank is what to re-examine first.

**And underneath, a defect that had been silently bounding the work.** Extending
the sweep to `q = 2` aborted the program. It aborted at `HEAD` as well, so it
was not a regression, and it was not in the front end: `tensor()` in the
allocator library reserved `nrh + 1` row pointers and then wrote at
`t[nrl…nrh]`, which for a negative lower bound writes before the block. The
likelihood routine asks for exactly that whenever `q ≥ 2`. So **no model with
two or more moving-average lags had ever been estimable**, and every
specification search in this record — the comparison against Johansen at each
procedure's own optimum included — had been bounded at `q ≤ 1` by a defect
rather than by a decision. That is now stated where those results are recorded
([HOMOLOGATION.md](HOMOLOGATION.md) §3b) rather than left to be inferred.

**And the defect turned out to be already known.** Asked whether it had
precedents, the suite answered plainly: the fault belongs to the
Numerical-Recipes cleanup that reimplemented these allocators, it was diagnosed
and fixed on 2026-06-15, the univariate program's defect register describes the
symptom and the cause in the same terms reached here, and the correction is
already present in the transfer-function and VARMA programs and in the C sources
both Python ports compile. The register even explains why the standalone C
programs had not met it: in the univariate program the operator sections that
would raise `q` past the regular moving-average order are commented out, so its
`q` never reaches two. `drvec` reaches it by the ordinary route of a user asking
for `q = 2`. What `drvec` had was the stale copy, and the fix first written here
was accordingly discarded in favour of the suite's, adopted verbatim, so the
function is now byte-identical to the shared one
([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5).

The correction changes no computed value, and this is checked rather than
argued: the entire suite, golden baselines included, is unchanged, and the
configuration that used to corrupt the heap now runs clean under `valgrind`.
Both are permanent checks.

Two methodological points are worth keeping, and they are different. The first
finding was an artefact of the instrument, and what turned it into a result was
measuring the instrument before believing the measurement. The second was
already solved next door: a vendored copy is a fork with no notification
channel, so the question to ask before repairing one is not how to fix it but
whether the suite has fixed it already — and if it has, the suite's version is
the one to take, because two correct implementations of the same routine are
still a divergence.

`vector` and `ivector` were aligned with the suite for the same reason
immediately afterwards. Their defect was latent here rather than live, so the
case for changing them rested on removing the divergence, not on fixing a fault;
what made it safe was checking the new contract mechanically — the release now
depends on the bounds it is given, where before it ignored them — over every
path the program has ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5). It
repaid the change at once: an offset pointer, unlike one at the base of its
block, cannot be mistaken for a live reference, so the first run reported 32
bytes that had been leaking unseen in the seeding path, whose four buffers had
never had a deallocator.

## 8. The ladder, and the header that was being counted

The plan for embedding the VEC matrix ([VEC_EMBEDDING_PLAN.md](VEC_EMBEDDING_PLAN.md))
puts one thing before any change to the seeding: **report the ladder**. The
construction this program makes is optima-from-optima — the diagonal gate is
certified, and each wider model starts from the one below — but only the two
ends of it were ever emitted. The rungs in between had to be assembled by hand
out of three runs and a subtraction, which is where the degrees of freedom get
miscounted and where nothing at all is left on record. `-rungs` now emits rungs
0 (`F`, `Θ`, `Σ` diagonal), 1 (`Σ` free) and 2 (`F`, `Θ` free), all at `r = 0`,
with their `χ²` statistics, and rung 0 certifies itself in place. On
`mink_muskrat`, `p = 2, q = 1`, case 1:

| rung | npar | logL | LR | df | p |
|---|---|---|---|---|---|
| 0 `F`, `Θ`, `Σ` diagonal | 5 | −34.6278 | | | |
| 1 `Σ` free | 6 | −30.5844 | 8.087 | 1 | 0.0045 |
| 2 `F`, `Θ` free | 10 | −6.3311 | 48.507 | 4 | 0.0000 |

The check that gives this its value is not any of those numbers: it is that rung
2 reproduces, to the digit, the fit the program gives without `-rungs` at all
(−6.3311226118). That says the ladder is re-estimating **the same** models and
not a family configured slightly differently, which is the failure such a
convenience feature would otherwise hide.

Underneath it sat a real defect, and the way it surfaced is the point. The
suite's check that a hand-written `.inp` reads as *a specification* and not as
*an optimum* had been passing; run again it killed the process for want of
memory. Both outcomes came from the same code. The vendored `.pre` reader skips
**five header lines by count**, but the format's authoritative parser
(`fue/src/fue/inp.py` [3.0]) skips whatever is there *until a separator whose
text says* `frequency` — the one place in the whole grammar where it reads what
a comment says. The `.pre` is written by **fue** (`report.py:write_pre`; DRVUS
writes no `.pre` at all — the banner naming it is legacy text fue copies), and
fue writes a blank line after that banner: five lines. The `.inp` that `drvec`
writes has four, and DRVUS's own `.inp` carries one specification line fewer
again. The family this reader faces does not have a header of fixed length, so
the same reader landed correctly on one file and one line short on the next. A one-line slip raises no error: it reads
`nobs` off the wrong line, and `nobs` was never initialised, so the number was
whatever the stack held — small and harmless on one run, 1 787 128 427 and a
request for that many reals on the next.

Counting lines reads a *file*; scanning for the separator reads the *format*.
The reader now scans, refuses a file with no frequency separator, and refuses a
non-positive `nobs` before allocating anything, so a malformed input produces a
sentence instead of a death. Two lessons, and only the second is about parsers.
A test that passes intermittently is not a passing test — it is an
uninitialised read waiting for its turn — and the drift of a vendored copy is
not only in what it computes but in what it *assumes about its input*, which is
the part no golden value will ever catch. `drtran`'s copy has the same count;
that one is live.

## 8b. The baseline, and an alarm that was ringing at the wrong thing

With the ladder emitting, the next step of the plan was to **measure what is
done now** — route (C), where `init_guess` starts every block from a conditional
regression and ignores the gate — over a bank fixed in advance: the eight
wheat-price pairs, `mink_muskrat` in all three deterministic cases, the
synthetic series of known rank, and the pathological one. Five quantities each.
The whole bank takes eleven seconds, and the numbers are in
[HOMOLOGATION.md](HOMOLOGATION.md) §4b. Two of them matter for what comes next:
the cold start gives away **11.5 to 16.8** units of log-likelihood on the pairs
and **287** on `mink_muskrat` case 1, and the best of twenty restarts beats one
cold start in **ten of the fifteen** cases. That is the number route (B) has to
improve on, and it is on record before the change rather than after it.

The bank also handed over, without being built for it, the case §7 of the plan
said would have to be constructed: **a gate that fails its own contract**. Milan
failed it. The failure was reported at rung 0, which is exactly what that bank
item was there to confirm — but the diagnosis was wrong. The identity separates
the two sides only as far as `elf`'s `ξ` truncation reaches, and the joint system
and the univariate ones do not truncate at the same term. Lowering `xitol` from
`1e−3` to `1e−8` and rebuilding moved the Milan gap from `1.385e−04` to
`3.100e−09`: five orders of tolerance, five orders of gap. The disagreement *is*
the truncation.

So the fixed `1e−4` threshold was ranking cases by the size of their
log-likelihood rather than by their agreement — it failed Milan, whose relative
disagreement is 2.1e−6, while passing Angers at 6.3e−6. The tolerance is now the
truncation itself, `xitol` where there is a truncation and `1e−6` where there is
none, and both halves are in the suite: the `q = 0` half must hold to 1e−9, and
that strict half is what keeps the loose half honest. No estimate moved; what
changed was a verdict.

It is the same lesson as §7 from a different direction, and worth stating in the
form it keeps taking here: **before believing what an instrument says about the
program, measure the instrument.** The difference this time is that the
instrument was one we had written on purpose to be trustworthy, and it still
needed measuring.

## 9. Open, and honestly so

| | |
|---|---|
| the published EML log-likelihoods of Table 5 | not reproduced; localised, not closed |
| `\|Σ̂\|` in level | 0.002346 with `-multistart 60` against 0.002311, i.e. 1.6 % — substantially closed, not exactly met |
| termcode 3 | explained and measured, not eliminated — see [CONVERGENCE.md](CONVERGENCE.md) |
| the choice of the `Y₁` block | not diagnosed; the user's responsibility, and currently unchecked |
| finite-sample critical values for the rank test | asymptotic only, and now **measured**: over 60 replications of a true r = 1 process at n = 120 the asymptotic test picks the right rank 68 % of the time and over-rejects 30 % — six times its nominal 5 %. `-bootstrap` improves that to 78 %/20 % (paired, all 6 discordant pairs its way, McNemar p = 0.031) but does not fix it |
| `drtran`'s BUG-11 | fixed in `drvec`'s copy, live in `drtran` |
| the invertibility boundary | explained and now reported, not resolved: on these data every specification with `q ≥ 1` rests on it, so the reported optima are constrained ones and the unconstrained standard errors do not apply along the binding direction — see §7 |
| the rank test's dependence on the optimiser | measured, not resolved: on Aix the sequential test returns `r = 0` at 20 and 60 restarts and `r = 1` at 5 % with 150, because the `r = 1` fit does not reach its optimum sooner ([HOMOLOGATION.md](HOMOLOGATION.md) §3b) |
| the moving-average boundary on the eight pairs | at `p = 2, q = 1` seven of eight rest on it, so the standard errors reported there are not defined along the binding direction. It is over-specification rather than data, and largely absent at the criterion-selected specifications, but the published columns carry the caveat |
