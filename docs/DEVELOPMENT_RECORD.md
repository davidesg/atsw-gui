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

## 8c. Route (B), built and measured, and it loses

`-seedgate` is the plan's preferred route, built behind a flag exactly as the
plan said: estimate the `r = 0` rung, hold `F`, `Θ` and `Σ` there, fit `Λ` and
`B₂` on that base, release everything. The measurement is
[HOMOLOGATION.md](HOMOLOGATION.md) §4c, and it falls against it — three wins in
eleven, seven losses, one of them by 37.6 log-likelihood units.

Recording that is the point of having planned it this way, so it is recorded.
What is worth more than the verdict is the two things the attempt measured.

**The step off the boundary is one-sided, and nobody had said so out loud.** The
plan's §3 measured that at `Λ = 0` the transformed system has an AR root of
modulus exactly one, and that the two directions leaving that point behave
differently. What the implementation ran into is the operational form of the
same fact: with the rung below held and `Λ` at its conditional-regression value,
`elf` answers `ifault = 3`, the optimiser refuses to start, and the run dies
before it begins. The conditional regression's `Λ` has no reason to point the
admissible way. So the option scans multiples of it — ±1 down to ±0.01 — and
enters at the best admissible one. That is not the arbitrary constant the plan
forbade: the *direction* is estimated and the *step* is chosen by the
likelihood among admissible candidates.

**And the entry is expensive.** Milan's `r = 0` optimum is 77.78; the best
admissible entry is −24.09; profiling `Λ` and `B₂` on the held base recovers to
−1.34. Sixty to a hundred units to cross, and only part of it back. On Angers
the released fit then moves by 0.005 and stops, at −8.42 against the cold
start's 29.09: the entry point is a trap rather than a start, and these surfaces
— which stop on `steptol` or on a failed line search, never on the gradient —
do not let go of a bad one.

So the premise was wrong, and it is worth naming precisely because it is the
premise everything else in this program rests on. **Carrying an optimum up a
rung works everywhere in this construction except across this rung.** `F`, `Θ`
and `Σ` are the same objects at `r = 0` and `r = 1`, but their optima are not
near each other, and the error-correction term is not a perturbation of the
model without it.

The route that answers the objection is not a better bridge from below but a
better estimate of what is being added. Johansen's canonical reduced-rank
solution gives `β` in closed form with `α` following from it, and the register
already says — from §2.1b, measured for another purpose entirely — that his `β`
lands within 0.0003 to 0.052 of `drvec`'s own optimum on the eight pairs. That
is one to two orders of magnitude closer than anything (B) or (C) starts from.
It is the next thing to measure, on the same bank and with the same five
quantities.

## 8d. The question that found the specification error

The objection came in one sentence, and it was the right one: *in a VEC model
the error-correction term is a stationary regressor, and a stationary regressor
cannot move a moving average that much.* On the eight wheat pairs the same
component's moving-average coefficient goes from 0.833 at `r = 0` — where it
reproduces `fue`'s univariate 0.7482 — to 0.999 at `r = 1`. Either those data
are extraordinary or the embedding is wrong, and the simplest answer is usually
the embedding.

**It was not the embedding.** The transformation was checked against Mauricio's
Eqs. (10)–(18) term by term, and then numerically, on a simulated VEC process
with a general non-scalar `Θ`: the identity holds to 4.2e−15, and the three
plausible alternative forms of `Θ*` fail by 0.56 to 2.5, so the test
discriminates rather than merely agreeing. Nor was it the estimator: on data
simulated from the WARMA process of the BVECM paper's Corollary 2, `drvec` with
a **free** `Θ` recovers the true structure by itself — bottom row 1e−3 against a
truth of exactly zero, `B̂₂ = −0.49993` against −0.5.

**It was the specification.** Corollary 2 says what the moving average of a VEC
representation looks like when the process admits a WARMA form: the error is
`ε_t = [β′η_t + Θ(B)a_t ; η_t]`, and regrouping on the innovations gives
`Θ̃₁ = [[Θ₁, Θ₁B₂′],[0, 0]]` — verified numerically at 4.4e−16, against 2.0 for
the sign-flipped alternative. **The last `s` rows are zero and the top-right
block is not a parameter at all**: it is `Θ₁₁B₂′`. With `M = 2` and `r = 1`,
`drvec` was estimating four moving-average parameters where the structure allows
one. That is not a coding error, which is why reading the transformation over and
over would never have found it. It is a model that is freer than the theory that
justifies it.

What imposing it does, on the eight pairs, is the measurement worth keeping
([HOMOLOGATION.md](HOMOLOGATION.md) §4g): the smallest moving-average root moves
off 1.0000 to between 1.38 and 12.55; all eight converge **on the gradient**,
where the free version converges on none; and `B̂₂` lands within **0.001 to
0.052** of Johansen's canonical `β`, where the free version was 0.1 to 0.4 away.
Three independent symptoms clearing together is not what a coincidence looks
like.

The likelihood still prefers the free version — LR 13 to 26 on 3 degrees of
freedom — and that comparison is precisely the one that cannot be made, because
the unrestricted optimum sits on the invertibility boundary in all eight cases.
So `-mawarma` is offered, the register carries the comparison, and no default
moves.

Two smaller things came out of the same work. The `-mawarma` printer had to be
rewritten because it was reading the free `q·M²` stride and publishing a `Θ` and
a `B₂` **nobody had estimated** — the fifth walk of the parameter vector, and the
same class of defect as §4.1. The suite's structural check catches it now,
including on `M = 5, r = 2`, where `T₁₁` is 2×2 and the block it determines is
2×3: with `r = 1` and `s = 1` a wrong dimension there is invisible. And
`-seedb2` exists at all because a question about a starting value should not
require a recompilation to answer.

## 8e. What the paper leaves unguarded

`-mawarma` removes the pathology, but it does so by narrowing the model class,
and that raised the better question: is there a condition **the class itself
needs** that nobody is enforcing? There is, and it is one line of the Granger
representation.

For a VEC model with moving-average errors to be an I(1) process of rank exactly
`r`, `G = σ_min(Λ⊥′Θ(1)B⊥)` must stay away from zero — it is what makes `C(1)`
carry the `M−r` stochastic trends. Mauricio (2006) assumes partial
nonstationarity **of the true process** and refers to Yap and Reinsel for
identifiability, but the estimation imposes neither: the engine checks that
roots are not *inside* the unit circle, and this degeneracy lives exactly *on*
it, in the permitted edge.

Measured, and the measurement corrected my own first reading of it. `Θ̂(1)` is
singular to working precision in seven of the eight pairs, and the direction of
the singularity is aligned with `Λ⊥` at 0.946–0.9996 — so the degeneracy is in
the direction that must stay integrated. But `G` itself is not zero: 0.016 to
0.133, against ≈ 1 both under the inherited structure and at a known truth in
simulation. The free optimum does not sit *on* the inadmissible set; it sits one
to two orders of magnitude inside its neighbourhood. Saying it the first way
would have been a stronger claim than the evidence carries.

`G` is now reported at every fit, beside the roots, and `-rankadm [tol]` refuses
points below a floor the way a non-positive-definite `Σ` is refused. What that
buys is in [HOMOLOGATION.md](HOMOLOGATION.md) §4h, and the shape of it is what
matters: **`B̂₂` has two regimes.** On Milan it is −0.447 inside the
near-degenerate region and −0.55 to −0.60 outside it — stable across a twenty-fold
range of the floor, and equal to what Johansen's canonical estimator and the
inherited moving average both give. The quantity this program exists to produce
depends on whether the fit is allowed into that neighbourhood, and outside it
every route agrees.

That is the honest summary of the episode: the transformation is right, the
estimator is right, and the estimation **problem** is less well posed than the
paper's statement of it suggests. Two instruments came out of it that will
outlast the argument — the rank condition as a reported diagnostic, and
`tools/sim`, which is how any of this was decidable at all.

## 8f. The comparison that could not be made, made

Two sections were left with the same open end: the inherited moving average
fits worse than the free one by an LR of 13 to 26 on 3 degrees of freedom, and
that comparison could not be read because the unrestricted optimum sits where
the rank condition degenerates. The way to close it was already in the program —
the rank test's parametric bootstrap simulates under `H₀` — so `-matest N` does
the same thing for this hypothesis: fit the restricted model, simulate from it,
and fit both models on each replication.

**The asymptotic reading was overstated but not empty.** The bootstrap 5 %
critical value runs from 11.9 to 18.1 where `χ²(3)` says 7.81 — a factor of 1.5
to 2.3, the same direction and roughly the same size as the over-rejection §2.3
measures for the rank test. And with the correct distribution the restriction is
**still rejected at 5 % in eight of the eleven cases**. So these data do want a
moving average richer than the WARMA class allows; what was wrong was the
strength claimed for that conclusion, not the conclusion.

The part worth keeping is what the two results say together, because they pull
in opposite directions and both are measured. The inherited structure is **too
narrow** for this bank. The free `Θ` is **inadmissible** — its optimum denies
the rank it is estimated at. Rejecting `H₀` licenses neither: it says the truth
is not the restricted model, not that it is the free one. What both point at is
the middle, a free `Θ` under a rank floor, and that is exactly where `B̂₂` stops
depending on which route produced it.

Which is the whole episode in one line: **three conjectures arrived, none of
them survived as stated, and all three were worth measuring.** The `B₂` seed
does not drive the moving average to the boundary; the embedding is not wrong;
the moving average does not simply inherit. What came out instead is a
condition the model class needs and nobody was enforcing, an option that
enforces it, and a test with the distribution it actually has.

## 8g. Which zero mattered, and the specification that came out of it

The obvious compromise was the wrong one, and finding that out took one
measurement. Corollary 2 imposes two zeros — the lower-left block of `Θ`, and
the differenced block's own moving average — and the natural intermediate
looked like keeping the first and releasing the second, since the lower-left
block is where the free estimates blow up (entries of −1.32, 1.51, **5.09**).
`-matri` does exactly that. **It changes nothing**: `G` stays at 0.02–0.12 and
the moving-average root stays at 1.000 in eight of eleven cases.

What carries the inadmissibility is `T₂₂`, the moving average of the **already
differenced** block. Left free it goes to one, which is `(1 − B)` sitting on
`∇Y₂` — the model undoing its own differencing — and `Θ(1)` loses the identity
in its lower block, taking the rank condition with it. That is the conjecture
this whole episode started from, finally located in a single parameter.

`-marow` zeroes it and frees the cross block instead: gradient convergence in
nine of ten, moving-average roots at 1.4–10.9, `B̂₂` in the canonical region, and
`G` between 0.22 and 1.32. The decomposition of the likelihood says why it is
the right cut ([HOMOLOGATION.md](HOMOLOGATION.md) §4j): freeing the cross block
is worth 0.06 to 8.0 on 1 degree of freedom and is not rejected at 5 % in seven
of ten, while freeing `T₂₁` and `T₂₂` is worth 8 to 23 — all of it bought in the
direction that makes the model inadmissible.

And a detail worth keeping because it is arithmetic rather than judgement: on
Aix and Penn the **free** fits come out below the nested `-matri` ones. A
restricted model cannot beat the model containing it, so those two free fits
never reached their own optimum.

## 8h. Estimating the theorems' class where the theorems live

The review of §8g's papers ended with a structural observation rather than a
number: the BVECM theorems are proved for Phillips' triangular model in the
WARMA parameterisation, the equivalence with the general VARMA-VEC is asserted
rather than proved, and the two do not carry the same restrictions — a free
`Γ_i` has rank `M` where the WARMA image has rank `r`. Which raised the question
of where a restriction belongs. In VEC coordinates the moving-average structure
couples `Θ` to `B₂` and has to be rebuilt at every likelihood evaluation; in the
transformed coordinates the same class is a pattern of zeros and `B₂` enters
only by forming `W` — by subtraction, like the input of a transfer function.

`-warma` estimates it there. No `C̄`, no `Φ̄` recursion: the parameters already
are the transformed system's. It recovers its own truth on simulated data
(`B̂₂ = −0.49994` against −0.5), and it produced the check this program did not
have. **With `p = 1` it and `-mawarma` describe the same family by two casts
that share no code**, and they reach the same maximum to nine decimals —
−22598.3542436254 against −22598.3542436269 on the simulation,
64.1597098018 against 64.1597098014 on Utrecht. Two independent implementations
of the same likelihood agreeing to that is worth more than either of them
agreeing with a table.

On the bank at `p = 2` it converges **on the gradient in eight of eight**, which
nothing else in the register does, with moving-average roots from 1.0 to 25.1
and `B̂₂` in the canonical region. The autoregressive half of the class is not
rejected at 5 % in five of the eight. And on Angers the **less** restricted
`-mawarma` fit comes out 3.8 units worse than the more restricted one, both
stopping on the gradient: a nested model cannot beat the model containing it, so
that is the VEC-coordinate parameterisation stuck in a local optimum where the
`Ȳ`-coordinate one is not. The conditioning claim of
[ESTUDIO_BVECM_vs_DRVEC.md](ESTUDIO_BVECM_vs_DRVEC.md) §3.1 now has a number.

One thing to note about how it was built, since it is the recurring failure of
this whole stretch. The first version printed its results through a new exit
path of its own, and valgrind found 2112 bytes in 9 blocks immediately. A new
way out of a function is a new set of leaks; the fix was to route it through the
same cleanup as everything else and gate the printing with a flag. That is the
third time in this record that adding a branch to a walk or an exit has cost
something — the `-diagma` printer, the `-mawarma` printer, and now this.

## 8i. The measurement that changed an answer about data

The plan put this third on purpose: the rank test is where an applied conclusion
could move, and it moved. All p-values below are bootstrap ones, because the
asymptotic table over-rejects six-fold.

**At `q = 0` the eight wheat pairs cointegrate unanimously**, every bootstrap
p-value sitting at `1/(B+1)` — the resolution floor of a hundred replications.
With a free moving average at `q = 1`, two of eight stay below 5 %. With
admissibility enforced, one.

The reason is Theorem 3. The rank is `r` only while `Λ⊥′Θ(1)` keeps full row
rank, and the free `r = 1` fits land with `Θ̂(1)` singular in almost exactly the
`Λ⊥` direction — the one carrying the common trend. **The moving average absorbs
the very thing the rank test measures.** So the `q = 1` free column was never a
weaker test of the same hypothesis: its alternative is not a rank-one model.

Enforcing admissibility does not give the evidence back, and should not be
expected to: the constraint binds on `L(1)` and leaves `L(0)` alone, because
`r = 0` has no rank condition, so the statistic can only fall — Vienna from 26.7
to 9.2, Angers from 22.0 to 1.8. That column is a bound, not a verdict: most of
what the `q = 1` fit appeared to establish was bought in the inadmissible region.

What comes out is a protocol rather than a number: **select the rank at `q = 0`,
then fit the moving average at the selected rank.** And an obstacle worth
recording rather than papering over: the restricted classes cannot supply an
alternative for a rank test, because they are defined relative to the `r/s`
split and collapse at `r = 0` — with `r = 0`, `-marow` annihilates `Θ` entirely
and reproduces the `q = 0` fit to the digit, and `-warma` has no dynamics left
and does not estimate. The null and the alternative cannot be written in the
same restricted family.

It is worth saying what this does **not** show. It does not show that these
series fail to cointegrate — at `q = 0`, where the instrument is clean, they
cointegrate about as clearly as data of this length can. It shows that the
instrument stops working when a free moving average is added, and now there is a
theorem saying why.

## 8j. The default, and why it stays where it is

Step 4 of the plan was the only one that could touch a recorded result, and the
decision is that **the computation stays and the presentation changes**.

Two candidates were refused by their own measurements, which is the point of
having taken them. Making a restricted class the default fails because
`-mawarma` is rejected at 5 % in eight of eleven cases by a bootstrap that is
valid, and `-warma` fits `mink_muskrat` twenty units worse than the free model:
a default has to behave where it does not apply, and neither does. Making
`-rankadm` the default is more tempting — by Theorem 4 the admissible set *is*
the model class, so searching outside it is a defect rather than a preference —
but §4n measures the constrained fit binding on `L(1)` while leaving `L(0)`
alone, which makes it a downward-biased bound and not an answer. Substituting it
would move every number in the register to a value nobody is prepared to defend.

So the free model stays, because it is the widest in the class and because every
recorded result rests on it. What was wrong was never that the program fits it;
it was that the program handed the result over without saying that on this kind
of data the fit denies the rank it was estimated at. Now it says so on the
terminal, with the one command that shows the alternatives, and
[GETTING_STARTED.md](GETTING_STARTED.md) carries the order to do things in:
rank at `q = 0`, ladder at the selected rank, then estimate.

The notice is checked in both directions, because a warning that fires on
everything is not a warning. And no golden value moved — verified rather than
assumed.

*Stated so the decision is not read as a closure: there is no default that is
both admissible and unbiased, because building one needs a rank test whose
alternative is admissible, and §4n records why that cannot be written in the
restricted classes. Until then the honest default is the widest model with its
verdict attached.*

## 8k. The last rung, and a reading of my own that it reversed

Step 5 tested the autoregressive half of the triangular class — `Γ_i = M_iα′`,
every lag entering through the cointegrating combination — as a restriction
instead of as the eyeballed indication §4k had left it at. `-artest` reuses the
machinery of `-matest` with the other pair: `H₀` is `-warma`, `H₁` is
`-mawarma`, the moving-average structure held in both.

**It is not rejected in seven of the eight pairs.** Only Milan rejects, at the
bootstrap resolution floor; Utrecht is marginal. So the class splits cleanly:
its moving-average half is rejected in eight of eleven cases (§4i) and its
autoregressive half survives. What these data reject is the assumption that the
differenced block carries no moving average of its own — not the assumption that
the short-run dynamics enter through `W`.

**And it reverses what §4k said.** I had measured that `F̂₁`'s dominant row
direction matches `α′` in only one of the eight pairs, and read that as an
indication the data are not in the class in the autoregressive block. Tested
properly, the reading is wrong: the row space of a freely estimated `F̂₁` is a
weakly determined direction, and a distance between two weakly determined
directions is not evidence of anything. That is the same mistake as the one this
record opens with in §7 — believing an instrument before measuring it — and here
the difference it makes is between "the class does not fit these data" and "the
class fits seven of the eight".

Two smaller things. The reason for bootstrapping is **not** the one `-matest`
has, and the output says so: here the unrestricted model is admissible, and what
is non-standard is that a reduced-rank restriction's likelihood ratio is not
`χ²` when the true rank may already be below the one the restriction allows.
And the first version of the shared block printed a conditional format string
with the numbers concatenated onto one branch of the ternary — it compiled, and
`-artest` silently lost its logL, LR and p-value lines. Splitting the call in
two fixed it; the suite now checks that `-artest` reports the pair it claims to
be testing, by reproducing both fits on its own.

## 9. P1 — the command line, and what a review of the suite found

*The full plan is [PLAN_PRODUCCION.md](PLAN_PRODUCCION.md); this is the account
of what was done and what it cost.*

### The defect, and why it was the first thing

The option loop was a chain of `strcmp` with **no `else`**. An option `drvec` did
not know — a typo like `-diagcv`, an option belonging to another program of the
suite — was ignored in silence, and the program estimated a **different model**
and reported it as the one that had been asked for. On this data that is not a
cosmetic difference: a free `Θ` and a diagonal one are the difference between a
fit that denies its own rank and one that does not (§8j). A program that shouts
when `σ_min(Λ⊥′Θ(1)B⊥)` falls below 0.2 and says nothing when handed an option
that does not exist has its alarms badly distributed.

And `p`, `q`, `r` were read with `atoi`, which returns 0 on text without saying
so. Four command lines a user could type by accident ended in `SIGSEGV` inside
`init_guess` — `p = 0`, `p = -1`, `p = 200`, and a non-numeric order — one hung,
and `p = 60` on 61 observations was attempted, failed, and returned **0** to the
shell.

### The design, and the one rule that made it safe

A **table** declares which options exist and what argument each takes; a
**validation pass** walks it before anything is assigned; the `strcmp` chain
stays exactly as it was, behind. The rule: **the validation does not assign.**
That is what guarantees it cannot change by accident what the program estimates,
and it did not — no golden value moved.

The same table generates the complete option list that `-h` prints, so the set
accepted and the set documented cannot separate again. Before this they had:
`usage()` covered 22 of 33 options, and nothing detected it.

The convention is not invented here. `drtran` in C uses `getopt` with
`default: usage(argv[0]); return 1;`, and the Python port states it as a
principle — *«Refusing rather than ignoring the option»* — with three exit codes.
`drvec` now has the same three: `0` completed, `1` refused, `2` attempted and not
completed.

**The distinction that had to survive.** A termcode-3 stop is **not** a failure.
It is the explained stop most of what this program publishes rests on
([CONVERGENCE.md](CONVERGENCE.md)), and it still exits 0. Turning it into an
error would have marked half the register as broken. There is a test that pins
it.

### Measured

The suite went from 152 checks to 190 (200 with the memory block), and it bites —
measured by mutation against the current source, which is rule 3 of the method:

| mutant | failures raised |
|---|---|
| `validate_cli()` made a no-op | 16 |
| `p`, `q`, `r` back to `atoi`, and the degrees-of-freedom bound removed | 7 |
| `usage()` enumerating only the first 22 options | 1 |
| the estimation-failure exit code back to 0 | 1 |

Two checks do **not** bite and are written into the script as such, because a
green suite that is not read carefully is worse than a red one.

### What the review of the suite found on the way

The rest of the review is in [PLAN_PRODUCCION.md](PLAN_PRODUCCION.md) §4, and
three of its results belong here because they are about this program:

* **The engine is intact, and now that is measured against the source rather
  than against our own history.** `elfvarma.c`, `drvmlest.c` and `qnewtopt.c`
  are **byte for byte** the VARMA program's. The one that has diverged from the
  canonical copy is the transfer-function program, for reasons of its own.
* **`nlatools.c` is the only copy in the family that diverges in code**, and the
  divergence is `matrix`/`imatrix` and their releases, which are the univariate
  program's variant rather than the shared one. The recorded reason for leaving
  them — that the two layouts are not interchangeable — was tested: with the
  shared variant in place **all 152 functional checks pass**, and the only
  consequence is that valgrind then reports **two real leaks that the current
  layout conceals** (`cond_resid` and `rawmat`, both process-lifetime). So the
  reason is true of the univariate program and **false of `drvec`**. That is the
  same mechanism §5 of [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) already
  describes for `vector`, met a second time.
* **Two defects still open in the suite's register are fixed here**, in C: the
  reader's annual-section bug and the missing deallocator. The register does not
  know it.

---

## 9b. P4 — the default moved, and why that was the whole point

*Plan: [PLAN_PRODUCCION.md](PLAN_PRODUCCION.md) §5. Decision and what it
reverses: [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md) §10. Measurements:
[HOMOLOGATION.md](HOMOLOGATION.md) §4q and §4r.*

**The argument that forced it.** «Rank and `B₂` are reliable, ship that» is not
a product: Johansen's procedure already delivers both, in closed form. A
VARMA-VEC exists to say something a univariate model cannot, and that something
is the short-run structure. If `Λ`, `F` and `Θ` cannot be pinned down, the extra
parameters are not an advantage, they are a cost.

**And they could not be**, in the regime this program is used in. §4q: with a
moving average of the `(1 − θB)` kind — which is what differenced price and
population series give — the free `Θ` is not recoverable at `n = 120` or
`n = 250`, at any magnitude tried, and `-multistart` makes it worse rather than
better, because the likelihood rewards the invertibility boundary and searching
harder finds it more reliably.

**The theory had the answer and nobody had written it down.** Corollary 6.3: with
the lower `s` rows of every `Θ̃ₖ` zero, `det Θ̃(1) = det(I_r − ΣT_k)` and the
non-zero companion eigenvalues are exactly those of the `r × r` block. So the
moving average is invertible **iff** its `r × r` block is, the rank condition
holds automatically, and the degenerate point of `𝒫 \ 𝒞` — the one Theorem 4
says the likelihood reaches and rewards, and Theorem 5 says no root check can
see — **is not reachable**. Theorem 5 is a statement about the free class.
`chekma` on `Θ̃` is `chekma` on the `r × r` block; with `M = 2, r = 1` the whole
admissibility condition is `|θ_w| < 1`, a scalar the engine already checks.

Half of it was already there: Corollary 6.2 gives the determinant for `q = 1`
under the full inherited structure. What was missing is that it holds for every
`q` and without the inherited cross block — the lower row alone does it, which
§4j had located by measurement — and the eigenvalue half, which is what connects
the condition to the check the engine already performs.

**What §8 of the specification plan got wrong.** Its two legs — `-rankadm` is a
downward-biased bound, and the inherited class is rejected in eight of eleven —
are statements about *restricting* `𝒫`. The class of Theorem 6 is not a
restriction on `𝒫`; it is a parameterisation in which `𝒞` is the whole space.
There is no bound to bind. And the rejection was measured against a free
alternative sitting on the boundary, where the statistic has no distribution —
§4i says so itself.

**Measured, on the criterion that matters.** P4.3: the default reaches the
boundary in **0 of 5** bank cases, `-mafree` still does in **4 of 5**, and the
lower rows of `Θ̂` are exactly zero in all five. §4r: on a WARMA truth with an
identified `w` block, `θ = +0.9` — positive coefficient, zero at 1.11, the hard
case — is recovered with bias 0.050 and IQR 0.272, interior, `G = 0.96`, where
the free fit parks on the gate with `G = 0.45` and twice the dispersion.

### What it cost, and the lesson underneath it

Eight golden values moved; seven are preserved under `-mafree` so the previous
parameterisation stays protected. Six tests that built an inadmissible case now
ask for it explicitly. And three places that **transport parameters between
fits** had to learn which class they are going to: `init_guess`, whose MA block
was written from the flags with no guard on `r`; `gate_profile_seed`, which
wrote `M·M` held values into a vector that now expects `r·M` and needed the
projection onto the target class; and the `r = 0` case everywhere, where the
structured classes are undefined and the moving average must be free — which is
also what `-lrtest` needs, since it compares ranks and can only do that inside
one class.

That last one is the general lesson: **a default is not a value, it is a
contract between every part of the program that hands parameters to another
part.** Moving it found three places that had been reading the flags directly
instead of asking what the current fit is.

### What it does not settle

Whether the resulting model forecasts better than a univariate one. `drvec`
still cannot forecast, and until it can this remains a better-behaved estimator
of a model nobody has shown to be worth its parameters. That is P5, and it is
the phase that closes the program.

---

## 10. Open, and honestly so

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
