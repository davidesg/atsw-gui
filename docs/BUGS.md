# Defects: the register, and how it is kept

*What each defect was, how it was found, and what it cost. The third is the part
that cannot be omitted: a defect with no measured cost cannot be prioritised and
cannot be closed.*

---

## How this register works

**The numbering is the suite's, not this program's.** `fue`, `drvarma`, `drtran`
and `drvec` share an engine and a cast, so they share **one** sequence of
numbers: `BUG-1` to `BUG-13` live in `drtran-python/docs/BUGS.md`, and of those
`drvec` touches 10 to 13. The ones that start here continue that sequence rather
than opening a private one, so that a number never means two things. A pointer in
the other register says where 14 onward are and that the next one numbered there
starts at 16. (`fue` also carries its own four-digit numbering — `BUG-0005`,
`BUG-0010`, `BUG-0012`, `BUG-0018` — which does not collide with this.)

**States.** A defect is `OPEN`, `FIXED` or `WON'T FIX`, and the last one carries
its reason. There is no `CLOSED`: a defect that was never reproduced is `OPEN`
and says so.

**What an entry must contain**, and `tools/check_bugs.py` enforces it:

```markdown
## BUG-N — one line saying what it is

**Status: FIXED on YYYY-MM-DD** (where, in the source).

**What it was.**   The mechanism, not the symptom.
**How it was found.**   Which check, which run, which measurement.
**What it cost.**   Measured.  "Nothing measurable" is a valid answer and is
                    worth more than silence, but it has to have been measured.
**The lesson.**   Optional, and it is where the checks come from.
```

**A defect is not fixed until what it could have moved has been re-measured**,
and the register says it was re-measured even when nothing moved. That is why
`HOMOLOGATION.md` §4v exists and why `BUG-14` did not delay the 0.9 tag: found,
fixed, re-measured, recorded. See [VERSIONS.md](VERSIONS.md) §1.

---

## BUG-14 — `xitol` uninitialised in the rolling-origin evaluation

**Status: FIXED on 2026-08-22** (`src/drvec.c`, `rolling_eval`).

**What it was.** `rolling_eval` builds its own `struct Tvarma` on the stack and
fills it with `vec_shootx`, which sets every field **except `xitol`**: that
tolerance is set by each site that uses the structure, and a dozen sites do it.
This one did not. `elf` was therefore called with whatever happened to be in that
word of the stack as the truncation tolerance of the `ξ` vector.

**How it was found.** Running `valgrind` over the paths P6 added: sixty
conditional jumps on an uninitialised value, all in `cxi` (`elfvarma.c:773`),
with the origin in `rolling_eval`'s stack allocation. Nothing else would have
found it — the program did not crash, did not warn and did not produce a `nan`.

**What it cost.** This is the out-of-sample route, i.e. the one the measurement
that decides this program's version number goes through
([HOMOLOGATION.md](HOMOLOGATION.md) §4t). All three columns of §4t, nine cases
each, were re-run with the fix in and **reproduce the published table digit for
digit**. So the measured cost is **zero**: the value that happened to sit in that
stack word fell in the range where the truncation does not bite. That is not the
same as harmless — it was undefined behaviour, and a different compiler, call
depth or day could have moved it without a word. Recorded in §4v.

**The lesson.** The structure has a field no filling function fills. While that
is true, every new site that uses it is another chance at the same bug. What
closes it is `vec_shootx` setting `xitol` itself — deliberately not done now,
because touching it would move every figure in the register through the back
door. It is written down for the refactor (P8), which is where a change with no
measurable effect belongs.

---

## BUG-15 — the `.pre`'s `refactor` was not undone under `-interv`

**Status: FIXED on 2026-08-21** (`src/drvec.c`, `subtract_interventions`).

**What it was.** The `.pre` files that connect `fue` to the rest of the suite
carry their series rescaled by 100 as a matter of course — it improves the
optimiser's conditioning — and the factor travels in the file itself.
`subtract_interventions` read the deterministic terms out of the `.pre` and
subtracted them from the levels **without dividing by the factor**, so it
subtracted a hundred times what it should.

**How it was found.** Estimating a VARMA-VEC on IPC_ES, IPC_DE and IPC_FR from
the `.pre` files of their models with deterministic seasonality.

**What it cost.** The rank analysis of the three CPIs returned `r = 1` where it
is `r = 0`: a cointegrating relation invented by a scale factor. With the fix,
`r = 0` with a bootstrap p of 0.313, and Johansen agrees under the matching
specification.

**The lesson, which was already written down.** `drtran-python/docs/PORTE.md`
and `ARCHITECTURE_MCP.md` say "never hard-code the rescaling factor — the suite
has three logged defects because of it". This was the fourth instance of an
already-named failure mode. The check that would have caught it is in the suite
now, block `[8f]`.

---

## Found and fixed before 0.9

Unnumbered because they were fixed inside the development and their full report
is in the record, not here. They are listed because they are the failure modes
this program *has*, and whoever knows them will look for them first:

| what it was | where it is told |
|---|---|
| the parameter-vector walk fell out of step in the printer and published a `Θ` nobody had estimated | `DEVELOPMENT_RECORD.md` §4.1 — the suite's **structural** block comes from this, and so does the rule that indices are noted **in the walk that already exists** |
| with `-multistart` there was no final estimation, so the residuals were left uncomputed: the diagnosis came out with `Q = nan` and "the residuals appear to be white noise" | next to the fix in `src/drvec.c` — the worst possible way to be wrong, because the message was reassuring |
| the multi-start block reallocated the VARMA structure with the first allocation still live: 1080 bytes lost | this is why the suite has its `VALGRIND=1` block |
| double free (`SIGABRT`) on centralising the cleanup: `main` already freed `datamat` and `Y2_levels`, and with `nobs` instead of `alloc_nobs` | latent under `-estwin`, which is precisely where `nobs` is trimmed |
| a hand-made `.pre` fixture was read one row off: the deterministic block carries **two** flag lines, not one | same signature as `BUG-11`, and on `drvec`'s side |

---

## Watched, and not defects

The live list is `DEVELOPMENT_RECORD.md` §10. What matters most when reading an
output:

- **`termcode 3`.** The optimiser stops on "last global step failed" often.
  Explained and measured, not eliminated: [CONVERGENCE.md](CONVERGENCE.md).
- **The invertibility boundary in the free class** (`-mafree`): there the
  standard errors are not defined along the binding direction. In the default
  class it **cannot be reached** (Corollary 6.3).
- **The rank test's critical values are asymptotic**: on a process with `r = 1`
  at `n = 120` they pick the right rank 68 % of the time and over-reject 30 %.
  `-bootstrap` raises that to 78 %/20 %; it does not fix it.
- **The choice of the `Y₁` block** is the user's and is not checked. The
  normalisation alarm fires when the relation barely involves it.
