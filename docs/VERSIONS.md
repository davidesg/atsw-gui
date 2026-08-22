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
| `1.0` | nothing in the register argues against using it for what it claims to do, and the default specification has been stable long enough for the recorded figures to have been produced under it |
| `x.y.z` | `z` moves for fixes that change no recorded figure; `y` for work that adds capability or moves figures; `x` for a change in what the program claims |

**Two rules follow, and they are the whole point of writing this down.**

**A defect found is not a reason to hold a version back; a defect *hidden* is.**
`BUG-14` was found the day 0.9 was tagged, in the very route the version argument
rests on. It was fixed, the table it affected was re-measured, and the result
went into the register. That sequence is what a version number is allowed to
stand on. What it may not stand on is a measurement nobody re-ran.

**The number moves when the claim changes, not when the calendar does.** There
is no release schedule.

---

## 2. Where the number lives

One definition, in `src/drvec.c`:

```c
#ifndef DRVEC_VERSION
#define DRVEC_VERSION "0.9"
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

### Unreleased, after v0.9

Work that is in `master` and **not** in the tag; it will carry whatever number
the next release takes, and the entry for that release will say why.

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

Suite: 252 checks, 262 with `VALGRIND=1`.

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
