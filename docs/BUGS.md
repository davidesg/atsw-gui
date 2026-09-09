# Defects: the register, and how it is kept

*What each defect was, how it was found, and what it cost. The third is the part
that cannot be omitted: a defect with no measured cost cannot be prioritised and
cannot be closed.*

---

## How this register works

**The numbering is the suite's, not this program's.** `fue`, `drvarma`, `drtran`
and `drvec` share an engine and a cast, so they share **one** sequence of
numbers: `BUG-1` to `BUG-13` live in `drtran-python/docs/BUGS.md`, and of those
`drvec` touches 10 to 13.  `BUG-16` is a defect in code `drvarma` and `drtran`
own, found here and fixed in both of them; it is registered here because here is
where it was found and measured. The ones that start here continue that sequence rather
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

**The lesson, and it is now acted on.** The structure had a field that no
filling function filled. While that was true, every new site that used it was
another chance at the same bug — patching `rolling_eval` fixed the symptom and
left the hole. **Closed at the root on 2026-08-23**, in P8: `vec_shootx` sets
`xitol` itself, so forgetting it is no longer possible. It cannot move a number
— every site that sets it uses exactly the same expression and sets it *after*
the call, writing the same value over the same value — and that is not an
argument, it is checked: the golden set (`tools/golden.sh`) compares
twenty-four whole reports byte for byte, and none moved.

It waited for P8 on purpose. A change with no measurable effect belongs with the
other changes that have none, where one net covers them all.

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

## BUG-16 — one-byte overflow in the residual histogram, in the suite's `diagnose.c`

**Status: FIXED on 2026-08-24** in the owners — `drvarma_v.04.1/src/diagnose.c`
and `drtran/src/diagnose.c` — and the fix brought back into `drvec/src/diagnose.c`.

**What it was.** `File_HistSer` allocates its histogram rows with
`malloc(NumCol + 1)` and then writes `NumCat` categories of `nphor` characters
each, followed by a closing `"|"`. But `NumCat * nphor` is *exactly* `NumCol`
— 16 × 4 when the range is ±4 sigma, 32 × 2 when it is ±8 — so the string is
`NumCol + 1` characters long and needs `NumCol + 2` bytes with its terminator.
`strcat` wrote the NUL one byte past the end of the block, on **every histogram
this routine has ever drawn**.

**How it was found.** drvec vendored the whole of `diagnose.c` on 2026-08-24 and
its `VALGRIND=1` block ran over the resulting `.out`: *Invalid write of size 2,
`strcat` in `File_HistSer` (diagnose.c:809), address 64 bytes inside a block of
size 65*. This is the first time this file has been linked into a program whose
suite runs valgrind over its whole output.

**What it cost.** Nothing observed, which is the honest answer and not a
reassuring one: a one-byte heap overflow corrupts whatever the allocator put
after the block, and `malloc` in practice rounds up, so it usually lands in
padding. It is undefined behaviour that happened to be survivable — the same
category as `BUG-14`, and the same reason for fixing it rather than noting it.

**The lesson.** The defect is in code shared by three programs and had been
there for as long as the routine has existed. What found it was not a reading:
it was linking the file into the one program of the family whose suite passes
valgrind over the full report. Sharing code shares its defects; it also shares
whatever any one of the three does to look for them.

---

## BUG-17 — the report named the wrong series for `alpha`, and `-weakex` restricted the wrong one

**Status: FIXED on 2026-08-24** (`src/drvec.c`, `inp2lam` and its call sites).

**What it was.** `drvec` carries two row orders and the report mixed them.

The data, the series names, `Gamma`, `Theta`, the residuals, the responses and
the forecast are all in the **`.inp`'s order**, `[Y2 block ; Y1 block]`. `Lambda`
and `B` are **not**: the cast writes the system in the **internal order**
`[Y1 ; Y2]` — `Cbar` maps `[Y1 ; Y2]` to `Ybar = [nabla Y2 ; W]`, and
`PhBar[1] = Cinv*Hbar - LamBar`, whose rows are `Cinv`'s, which are internal. So
`Lambda`'s rows `1..r` are the `Y1` block and `r+1..M` the `Y2` block.

The report labelled `Lambda`'s row `i` with `series_names[i]` straight. On any
fit with `r < M` that names the **wrong series** — in the parameter table, in
the `alpha` matrix and, since P6.8, in the **weak exogeneity tests**. And
`-weakex i` zeroed the internal row `i`, so it declared a different series
weakly exogenous from the one the user asked about.

**What was NOT wrong.** The fit itself: the estimation, the likelihood, `logelf`,
`sigma2`, the standard errors and every LR statistic are unaffected — the rows
were tested correctly, they were *named* wrongly. `granger_smin` is also correct:
it builds `Lambda_perp` and `B_perp` both in the internal order and multiplies
them by a `Theta` that is internal too, so it is self-consistent, and the rank
condition figures the register carries stand.

**How it was found.** Building the `beta' gain = 0` certificate for the long-run
gain (P11). That is the first thing in the program that had to multiply `beta`
by a quantity in the `.inp`'s order, so it is the first that could not paper over
the mismatch: it came out at `2.0e-01` where it must be zero, and putting `beta`
in the `.inp`'s order took it to `1.2e-10`. Nothing else in the program crossed
the two orders, which is why it had never shown.

**What it cost.** In the shipped output, a wrong series name on the adjustment
coefficients and on the weak exogeneity verdicts whenever `r < M`, which is every
fit that is not a plain VARMA. On mink–muskrat the loadings were swapped between
`muskrat` and `mink`. `Pi` was also assembled by pairing `Lambda`'s internal rows
with `B`'s internal rows and then *labelling* the result in the `.inp`'s order,
so `Pi` as a matrix was internally consistent but its rows and columns were named
wrongly; its non-zero eigenvalue, `-0.557476` on mink–muskrat, is unchanged.

**AND IT REACHED THE REGISTER — re-measured on 2026-08-24.**
[HOMOLOGATION.md](HOMOLOGATION.md) §4u lists `Lambda`, its `t` and the `-weakex`
LR **per series** for the three euro-area CPIs. The analysis was re-run through
the `.pre` route on the study's own files, and the table is superseded there.
The original sample could not be reproduced — the `.pre` files carry the
training window, 216 observations — so the figures are not the same figures;
what the re-run settles is the attribution, and it changes the reading: the old
table concluded that only Spain adjusted, and with the rows named correctly
**none of the three adjusts significantly** on that window. Which is what `r = 0`
looks like from the inside, and agrees with the bootstrap p of 0.313 the section
already carried.

**The lesson.** Two orders is one too many, and a program that carries both will
eventually print one while meaning the other. What caught it was not a reading of
the code — the mismatch had been read past several times — but an **identity that
had to cross the two**. Certificates earn their keep by being the only place
where an assumption cannot stay implicit.

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

## BUG-18 — `Q` and `Sigma` are printed in the internal row order and labelled in the `.inp`'s

**Status: OPEN.** Found 2026-09-09 on a third party's data (Bolivia TFM
replication, `M = 3`, `r = 1`, case 3, the `.pre` route).

**What it is.** Exactly the disease BUG-17 named — *«`drvec` carries two row
orders and the report mixed them»* — surviving in the covariance block. BUG-17
fixed `Lambda` and `B`; `Q` and `Sigma` were not looked at.

The fit prints, with `Sigma = sigma2 * Q`:

```
Q matrix (lower triangle; Q[1,1] = 1 by normalisation):
      1.000000
      0.382008    9.343358
     -0.496808   -0.308768    3.420599
Sigma = sigma2 * Q:
      3.515594
      1.342983   32.847451
     -1.746576   -1.085501   12.025435
```

and labels the entries in the parameter table `var RATIO`, `var ITCER`,
`cov ITCER, PGAS` — the `.inp`'s order `PGAS RATIO ITCER`. **The rows are in the
internal order `[Y1 ; Y2]`**, i.e. `ITCER PGAS RATIO`.

**How it was established** (three independent checks, all on the same fit):

| | |
|---|---|
| innovation sd from `Sigma` | 1.875 · 5.731 · 3.468 |
| the report's own `residual sd` line | 5.707 · 3.397 · 7.128 |
| read `Sigma` as `[Y1;Y2]` | PGAS 5.731 vs 5.707 ✓ · RATIO 3.468 vs 3.397 ✓ |
| `Q`'s correlation between its rows 2 and 3 | −0.0546 |
| the residuals' `R(0)` between PGAS and RATIO | −0.057 — the same pair, the same number |

**Why it matters, and it is not cosmetic.** The reader compares the innovation sd
of a series against its univariate `sigma` — that is what the `.pre` route is
*for*. Here `ITCER`'s true innovation sd is **1.875**, better than its univariate
2.158 and than the 2.702 of its own differenced series: the system fits it well.
Reading the labels as printed makes it 3.468 or 7.128 and invites the conclusion
that the model is broken. That conclusion was actually reached during the
replication, with the documentation open, and held for several steps.

---

## BUG-19 — the residual series of the `Y1` block is labelled with its file's name

**Status: OPEN.** Same fit as BUG-18.

**What it is.** The transformed system is `Ybar = (nabla Y2' , W')'` with
`W = Y1 + B2' Y2` (MODEL.md §2). Its third residual series is therefore the
innovation of `W`, not of `nabla ITCER`. The report calls it

```
--- Residual series a[3] (ITCER) ---
```

and its `residual sd` is 7.128 while `nabla ITCER` has sd 2.702 — a residual
larger than its supposed dependent variable, which is what makes a reader stop.
Its cross-correlations do not match any row of `Sigma` either: `R(0)[2][3] =
0.869`, which is the mechanical consequence of `W` carrying `2.0221 x RATIO`, not
a contemporaneous relation between the real exchange rate and public spending.

**Suggested fix.** Name it for what it is — `W` or `ec1` — or state in the block
header that the third series is the equilibrium error. The `.pre` route makes
this worse, because there the name comes from a file the user chose.

---

## BUG-20 — the closing message prints the first input path, not the output's

**Status: OPEN.** Same session.

`drvec a.pre b.pre c.pre 5 0 1 -name /path/out` finishes with

```
Done. Output written to /…/PGAS_m30.pre
```

which is the **first input file**. The `.out` is written correctly to the path
`-name` asked for; only the message is wrong. On the `.pre` route it reads as an
announcement that the program has just overwritten one of the user's estimated
models, which is alarming enough to stop the work and check.

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
