# Versions: the policy, the record, and where the number lives

*What `drvec`'s version number asserts, what has to be true before it moves, and
why each released number is the one it is. The reasoning used to live in a
comment in `src/drvec.c`; a source file is the wrong place for an argument a
reader needs **before** running the program.*

---

## 1. What a version number asserts here

A version number is a claim about what is inside, and this program is a research
estimator: what is inside is not "features" but **measurements and their
status**. So the scheme is not "how much was added" but **what a user can rely
on**:

| | what it asserts |
|---|---|
| `0.x` | the estimator works and is tested, and at least one thing the register measures is **against** it, or has been measured too recently to have been used by anybody |
| `1.0` | nothing in the register argues against using it for what it claims to do; the default specification has been stable long enough for the recorded figures to have been produced under it; and it has been run enough times, on data it was not developed on, for those two statements to mean something |
| `x.y.z` | `z` moves for fixes that change no recorded figure; `y` for work that adds capability or moves figures; `x` for a change in what the program claims |

**Three rules follow, and they are the whole point of writing this down.**

**A defect found is not a reason to hold a version back; a defect *hidden* is.**
`BUG-14` was found the day 0.9 was tagged, in the very route the version argument
rests on. It was fixed, the table it affected was re-measured, and the result
went into the register. That sequence is what a version number is allowed to
stand on. What it may not stand on is a measurement nobody re-ran.

**A result about the data is not a defect of the program.** Whether a VECM
forecasts better than an ARIMA per series is a property of the series, not of the
estimator. The theory runs the right way — if the structure is really there and
the gain in fit is real, the forecast should be the more efficient one — but the
gain is small, hard to estimate at these sample sizes, and routinely eaten by the
uncertainty in the parameters that carry it. That is the standing empirical
result of the forecasting literature, not a verdict on this code. So
`HOMOLOGATION.md` §4t is a measurement the program **owns and publishes**, and
not one that argues against it; it does not hold a version back by itself. What a
`1.0` needs on this front is not a win but **exposure**: the program run many
times, on data it was not developed on, with what it found written down each
time — and the defects that exposure turns up fixed as they appear. The two go
together, and neither is finished by a release. `BUG-15` is what the first half
looks like: it came out of applying the program to a study's own files, and it
had been silently wrong on every `.pre` whose `refactor` was not 1 — which is to
say, on data nobody had run it against yet. The other three on record came from
inside — a valgrind block, a certificate, a battery — and that is the second
half.

**The number moves when the claim changes, not when the calendar does.** There
is no release schedule.

---

## 2. Where the number lives

One definition, in `src/drvec.c`:

```c
#ifndef DRVEC_VERSION
#define DRVEC_VERSION "0.11"
#endif
```

Everything else must agree with it, and four places do:

| | how it gets there |
|---|---|
| `bin/drvec --version` | the `#define` |
| the first line of every `.out`, `.forecast` and `.recursive` | the `#define` |
| `CITATION.cff` (`version:`) | by hand |
| `CHANGELOG.md` (the top released heading) | by hand |
| this document (§3) | by hand |
| the git tag `v<version>` | by hand |

The three "by hand" rows are exactly where this kind of thing rots, so they are
**checked**: `tools/check_version.sh` compares all of them and the suite runs it
(block `[8l]`). A release whose `CITATION.cff` still says the previous number is
a release nobody can cite correctly.

---

## 3. The record

### 0.11 — 2026-10-10

**Why a release at all, and why now.** Because the Python port is about to
start, and a port needs a fixed oracle: a tag it is homologated against, so that
"the C says" names one program and not whichever copy was built last. Until this
release there were two copies — the standalone repository and
`atsw-gui/engines/drvec` — and each had something the other lacked (`-irfboot`
in one, the `lnsrch` guard against a NaN objective, `BUG-41` item 1, in the
other). 0.11 is both, in `atsw-gui`, tagged `drvec-v0.11`.

**Why 0.11 and not 0.10.1.** Figures moved: the default MA class went back to
the free `Θ` of Mauricio (2006), the rank test takes its case-1 table and the
`r = 0` fit starts from the ladder (`BUG-24`, `BUG-50`), forecasts on the `.pre`
route are in each series' units (`BUG-36`). And capability was added
(MEJORA-1 to 3, `-irfboot`). Both are `y`.

**Why 0.11 and not 1.0.** §1's conditions are no closer. The exposure is the
same nine-case bank, and the newest surface — `-fixb2row`, `-xpre`/`-xlink`,
`-xsys`, `-case pre`/`-trend`, `-irfboot` — is three days old, with gaps
declared in `TODO.md` (`-xsys` with `q > 0`, the rank test with exogenous
inputs or with `-trend`, `-xlink` with `-f`). That surface is **outside** the
first version of the port; the model it ports is cases 1–3, the `.pre` route,
the rank test, the impulse responses and variance decomposition, and `-f`.

**Defects fixed** since 0.10: `BUG-18` to `BUG-20`, `BUG-23` to `BUG-39`,
`BUG-43` to `BUG-50` (`BUG-40` closed as won't fix); `BUG-41` item 1 (via `lib/optim`'s `lnsrch`) and
`BUG-42` item 1 (the histogram overflow, drvarma 5.0's patch). **Still open**:
`BUG-41` items 2–3 and `BUG-42` items 2–4, shared with `drvarma`; whether the
port reproduces or corrects them is decided before it starts, not during.

**Re-measured for this release**: the twenty-four golden reports are
byte-identical to the baseline (`tools/golden.sh check`) with every change of
this release applied, and the suite passes 366 checks.

### 0.10 — 2026-08-24

**Why a release at all, and why now.** `BUG-17` shipped in `v0.9`: on every fit
with `r < M` the report **named the wrong series** for the adjustment
coefficients, and `-weakex i` restricted a different series from the one asked
for. The numbers were right and their names were not — which is the kind of
defect a user cannot detect from the output, because nothing about it looks
wrong. Under §1's rule, a defect found does not hold a version back; what holds
one back is a defect hidden. This one is fixed, the measurement it touched has
been re-run (`HOMOLOGATION.md` §4u) and **its conclusion changed**, which is
recorded there beside the table it replaces.

**Why 0.10 and not 1.0.** The conditions §1 sets for a 1.0 are still open, and
one of them got further away rather than closer:

1. the program has not been run enough. The out-of-sample bank is nine cases —
   one animal-population pair and eight wheat-price pairs — and §4t is what it
   measured there, re-measured and unchanged after `BUG-14`. That table is not
   the blocker under §1's third rule: not beating an ARIMA is a fact about those
   series, and it is published as one. What is open is the **exposure** the rule
   asks for, and nine cases from two sources is not it;
2. §1 asks for a default that has *survived a stretch of use* — and in this
   release the **entire report** was rebuilt, the parameter table is new, the
   notation moved to Johansen's, and three of the four defects on record were
   found in the last three days. A 1.0 on top of that would be asserting
   stability that nothing has had time to test.

**Why 0.10 and not 0.9.1.** `z` is for fixes that move no recorded figure. This
moves several: the `.out` is a different document, §4u's conclusion changed, and
the program gained the `.pre` input route, impulse responses, variance
decomposition, long-run gain and the per-series residual diagnosis. That is `y`.

**What it contains.**

| | |
|---|---|
| **P7** | `src/drvec.c` is English throughout, kept so by `tools/check_language.py` |
| **P8** | `main()` 2 320 → 468 lines, nine functions out of it, a map at the top of the file; `BUG-14` closed at its root |
| **P9** | the suite's `.pre` input route: one `fue` model per series, as in `drtran`, certified against the `.inp` route by identity |
| **P10** | the `.out` in the suite's shape, with a parameter table carrying `t` and `p`, and the whole report in **Johansen's notation**; the per-series residual diagnosis, vendored from `drvarma`'s `diagnose.c` |
| **P11** | impulse responses and variance decomposition **in levels**, with the long-run gain and mean lag, certified against the forecast bands |
| | the version and defect systems: this document, `tools/check_version.sh`, `tools/check_bugs.py`, `tools/golden.sh` |

**Defects fixed**: `BUG-14` (uninitialised `xitol` on the out-of-sample route),
`BUG-15` (the `.pre`'s rescaling under `-interv`), `BUG-16` (a one-byte overflow
in the suite's `diagnose.c`, fixed in `drvarma` and in `drtran`) and `BUG-17`.
Two of the four are in code this program does not own.

**Re-measured for this release, and the answer stated even where nothing moved**:
§4t's three columns reproduce the published table digit for digit after
`BUG-14`; the twenty-four golden reports were re-captured because the `.out`
changed on purpose, and every numeric literal of the old reports was checked to
survive into the new ones; §4u was re-run and does not survive, and says so.

Test suite: 256 checks, 266 with `VALGRIND=1`.

### 0.9 — 2026-08-22

The first numbered version. **Not 1.0, and the reason is not modesty**: two
things are true that a 1.0 would paper over, and both are measured rather than
suspected.

**It does not beat a univariate model out of sample.** Scored by `-estwin`
against `drvec`'s own diagonal rung — which by Theorem 9 *is* an ARIMA on each
series, so both sides share the sample, the estimator, the forecast recursion and
the scoring — the VEC loses in seven of nine cases at every horizon under the
default `q = 1`, by up to a factor of 2.9. At `q = 0` it is roughly a wash. See
[HOMOLOGATION.md](HOMOLOGATION.md) §4t.

That is not a defect to fix. It is what this program is: an exact maximum
likelihood estimator for a class of models, and for the cointegrating vector and
the hypotheses about it — where it is superconsistent and where a univariate
model says nothing at all. It is not a forecasting tool that earns its
parameters, and 1.0 on the tin would suggest it were.

**The default specification changed on 2026-08-20.** `Θ = [T₁₁ T₁₂ ; 0 0]`
(`-marow`) replaced the free `Θ`, and with it every figure in the register
measured under the old default. The change is argued from Corollary 6.3 and
measured in §4q and §4r, and it is the right change — but a default that has just
moved has not yet had time to be wrong in anybody's hands.

**What would make it 1.0.** Neither of the two is a task; they are conditions:

1. the forecasting result stands or is overturned **by measurement**, and either
   way the claim on the tin matches it (a 1.0 that says "estimator, not
   forecaster" is perfectly honest — the claim has to match, not the outcome);
2. the default has survived a stretch of use — the applied cases in the register
   re-run under it, and no rung of the specification ladder found to contradict
   it.

**What is in it.** The command line validates before estimating (P1); the default
specification is the structured class (P4); forecasting in levels with bands and
rolling-origin out-of-sample evaluation (P5); the suite's file system, `.out` +
`.forecast` + `.recursive` (P6.7); a default block of Wald tests on the relations
(P6.8); and packaging, licence, citation and this register (P6). Test suite at
the tag: 235 checks, 245 with `VALGRIND=1`.

**What it fixed.** `BUG-13` (the engine's χ² tail inverted at `df ≥ 30`, in the
file shared with `drvarma` and `drtran`), `BUG-14` (`xitol` uninitialised in the
out-of-sample route) and `BUG-15` (the `.pre`'s rescaling factor not undone in
`-interv`). All three are in [BUGS.md](BUGS.md) with what each cost.

---

### What was in 0.10, in detail

The list the release above summarises, item by item.

- **P7, the language of the source.** `src/drvec.c` is English throughout — 363
  comments and about forty messages — and `tools/check_language.py` keeps it
  that way. The translation was done against an invariant: strip the comments
  from both versions and the files must be byte-identical, so a translation that
  changed one character of code could not pass.
- **The version and defect systems.** This document, `tools/check_version.sh`
  and `tools/check_bugs.py`, all three in the suite.
- **P9, the suite's `.pre` input route.** `drvec s1.pre s2.pre ... p q r`: one
  univariate model per series, as in `drtran`. Certified against the `.inp`
  route by identity — the reports are byte-identical below `ESTIMATION
  SUCCESSFUL`, and the deterministic handling agrees exactly with `-interv`.

- **P8, the refactor.** `main()` went from 2 320 lines to 468, nine functions
  out of it, and a map of the file at the top. It moved nothing, and that is
  checked rather than asserted: `tools/golden.sh` compares twenty-four whole
  reports byte for byte. `BUG-14` is closed at its root in the process.
- **P10, the `.out`.** Rebuilt to the suite's shape — `drvarma`'s: a
  `key : value` header, blocks under one separator, a parameter table with `t`
  and `p`, and the report in **Johansen's notation** (`alpha`, `beta`,
  `Gamma`, `Pi`) rather than the algorithm's. The per-series residual
  diagnosis, which drvec simply did not have, comes from vendoring the suite's
  `diagnose.c` whole.
- **P11, impulse responses and variance decomposition, in levels**, with the
  long-run gain and mean lag, built on the same `level_error_map` the forecast
  bands use and certified against them.
- **Three defects found on the way**, all in [BUGS.md](BUGS.md): `BUG-15`
  (the `.pre` rescaling under `-interv`), `BUG-16` (a one-byte overflow in the
  suite's `diagnose.c`, fixed in `drvarma` and `drtran`) and **`BUG-17`**,
  which is the one that matters for a release: the report **named the wrong
  series** for the adjustment coefficients, and `-weakex` restricted the wrong
  one, on every fit with `r < M`.

Suite: 256 checks, 266 with `VALGRIND=1`.



---

## 4. Releasing

The checklist, in order. It is short because the checks are mechanical and the
suite runs them:

1. `make test` and `VALGRIND=1 make test` — both green.
2. Move `DRVEC_VERSION` in `src/drvec.c`.
3. Update `CITATION.cff`, `CHANGELOG.md` and §3 of this document. The §3 entry
   says **why this number and not the next one**; that is the part that cannot
   be generated.
4. `tools/check_version.sh` — it must pass, which is what says step 3 was done.
5. Re-measure whatever the change could have moved, and say in the register that
   it was re-measured **even when nothing moved**. A figure that was not re-run
   is a figure the new version does not carry.
6. Commit, then `git tag -a v<version>`, then push both.

---

## 5. What is deliberately not versioned

The **register** (`HOMOLOGATION.md`) and the **development record** are
cumulative and dated, not versioned: a measurement belongs to the day and build
it was made on, not to a release. Where a figure was produced by a build that has
since changed, the register says so and says whether it was re-run. That is why
§4v exists.
