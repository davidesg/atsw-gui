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
starts at 16. **State on 2026-09-23:** `BUG-21` and `BUG-22` were taken
in `drtran-python/docs/BUGS.md`, so the entries of the 2026-09-23 review start at
`BUG-23`. That register also holds two uncommitted entries numbered 17 and 18,
which collide with the ones here; they need renumbering there. (`fue` also carries its own four-digit numbering — `BUG-0005`,
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

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `report_fit`: the Q table, the Q and Sigma matrices, the P/D block).
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

**How it was found.** Reading the report of the Bolivia replication, and
established by three independent checks, all on the same fit:

| | |
|---|---|
| innovation sd from `Sigma` | 1.875 · 5.731 · 3.468 |
| the report's own `residual sd` line | 5.707 · 3.397 · 7.128 |
| read `Sigma` as `[Y1;Y2]` | PGAS 5.731 vs 5.707 ✓ · RATIO 3.468 vs 3.397 ✓ |
| `Q`'s correlation between its rows 2 and 3 | −0.0546 |
| the residuals' `R(0)` between PGAS and RATIO | −0.057 — the same pair, the same number |

**What it cost.** It is not cosmetic. The reader compares the innovation sd
of a series against its univariate `sigma` — that is what the `.pre` route is
*for*. Here `ITCER`'s true innovation sd is **1.875**, better than its univariate
2.158 and than the 2.702 of its own differenced series: the system fits it well.
Reading the labels as printed makes it 3.468 or 7.128 and invites the conclusion
that the model is broken. That conclusion was actually reached during the
replication, with the documentation open, and held for several steps.

**Also, found 2026-09-23.** The same internal-order `Sigma` feeds the `P`, `D` and
"own share" block (drvec.c ~6563-6608), which factorises the internal `Qm` and
says *"the ordering is the .inp's column order"*: on a simulated fit it prints
the `D` vector as (4.014, 0.994) in `y`-first order while the structural shock sds
of the IRF are (0.998, 2.002) in `x`-first order. The IRF itself is right
(`ldl_sigma` factorises `Sigma*` in `Ybar` order, which is equivalent), so the
P11 note that the report and the IRF share one factorisation is not true: there
are two, and they disagree. `Gamma` and `Theta` have the same defect: BUG-23.


**Fixed.** The Q block of the parameter table keeps the vector's order (each
row is a parameter with its standard error) and is named through `lam2inp`; its
header says which variance is normalised to 1. The Q and Sigma matrices and the
P, D, own-share block are printed in the `.inp`'s order, so P and D now agree
with the IRF's factorisation. A test in `run_tests.sh` 8d had been PERMUTING the
printed Sigma to compare it with the forecast band — the defect, compensated
inside the battery; it now compares them directly. Re-measured on a known DGP
(`tests/repro/fixtures/sim2.inp`): var x 0.995, var y 4.014 (truth 1 and 4);
guarded by 8q.

---

## BUG-19 — the residual series of the `Y1` block is labelled with its file's name

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `diagnose_ybar`, `write_resid_inps`).

**How it was found.** Same session as BUG-18: the residual sd did not match any
series.

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

**What it cost.** A reader concludes the model fits a series badly when the
residual is of another variable. **Also, found 2026-09-23:** `write_resid_inps`
names its files in the `.inp`'s order while their columns are internal — on a
synthetic fit the file named for the `Y2` series correlates 0.78 with the `Y1`
series' difference, and the committed fixture `tests/fixtures/mmres.1.*`
("emuskrat") is really `nabla` mink. The seeding stays consistent; the name ART
sees is wrong.

**Suggested fix.** Name it for what it is — `W` or `ec1` — or state in the block
header that the third series is the equilibrium error. The `.pre` route makes
this worse, because there the name comes from a file the user chose.


**Fixed.** The per-series diagnosis (vendored, untouched) is called through
`diagnose_ybar`, which names the W block's innovations `ec1..ecr` — as the
Lambda table does — for the duration of the call. `write_resid_inps` names each
file after the series its column belongs to (`lam2inp`), keeping the numbering
the `-seed` route reads. Guarded by 8q.

---

## BUG-20 — the closing message prints the first input path, not the output's

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `main`, `run_lrtest`).

**How it was found.** Reading the terminal after a `.pre` run.

`drvec a.pre b.pre c.pre 5 0 1 -name /path/out` finishes with

```
Done. Output written to /…/PGAS_m30.pre
```

which is the **first input file**. The `.out` is written correctly to the path
`-name` asked for; only the message is wrong. On the `.pre` route it reads as an
announcement that the program has just overwritten one of the user's estimated
models, which is alarming enough to stop the work and check.

**What it cost.** A stop to check that nothing had been overwritten. **Also,
found 2026-09-23:** `run_lrtest` has the same defect (~7447): it prints
`base_name`, which on the `.pre` route is the first input file.


**Fixed.** Both print the `.out` path, before `cleanup_names` frees it. On the
`.pre` route: `Done. Output written to mmpre.muskrat_mmpre.mink.out`.

---

## BUG-23 — `Gamma` and `Theta` are printed in the internal order and labelled in the `.inp`'s, so the short-run Wald tests name the wrong series

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `report_fit`, `hypothesis_block`).

**What it is.** BUG-17 and BUG-18 again, in the two blocks neither of them
touched. `vec_shootx` builds `Phi* = Cbar F Cinv` and `Theta* = Cbar Theta Cinv`
(drvec.c ~3990-4060), and `Cbar` acts on `[Y1;Y2]`, so `F` and `Theta` are
internal in rows AND columns, exactly like `Lambda` and `Sigma`. The printer
(`report_fit` ~6130-6170, 6313-6337, 6392-6412) and `hypothesis_block`
(~4960-5050) label row `i` with `series_names[i]`; only `Lambda` goes through
`inp2lam`. The comment at `inp2lam` (~2283) and the BUG-17 entry state that
`Gamma` and `Theta` are in the `.inp`'s order: that is not so.

**How it was found.** Three reviewers found it independently, on data whose truth
is known: a simulated `nabla x = 0.4 nabla y(-1)` (`tests/repro/fixtures/sim2.inp`),
a second synthetic DGP, and Lütkepohl's `e1`, where the printed `Gamma` and
`Sigma` reproduce `logelf` only when read as `[Y1;Y2]` (13 of 13 fits).

**What it cost.** On `sim2` the report prints `D.yY1 <- D.xY2(-1) 0.3948
(t = 43)` and the Wald block concludes *"D.xY2 drives the others"* — the reverse
of the data-generating process. Every short-run coefficient, every `Theta` row
and every "who drives whom" verdict published since the `.out` was rebuilt in P10
is attached to the wrong series whenever `r < M`. The numbers are right; their
names are not, and nothing in the output looks wrong. With the default MA class
the report contradicts itself: it says the `nabla Y2` block carries no MA of its
own and prints that block's only free `Theta` row under a `D.x...` label.

**Repro.** `sh tests/repro/repro.sh 23`.
**Suggested fix.** One permutation object, applied to `F`, `Theta` and `Q` at the
report boundary and in the `ix_F`/`ix_Th` lookups of `hypothesis_block`; and a
check in `run_tests.sh` that pins a label against a known DGP (none exists today:
the 255 checks pass with this defect).


**Fixed.** The Gamma and Theta tables and matrices walk the `.inp`'s order and
read the internal entry (`inp2lam` on both indices), with its own standard
error; the "who drives whom" Wald tests index F and Theta by `inp2lam(i)`.
Re-measured on `sim2.inp`: `D.xY2 <- D.yY1(-1) 0.394751 (t = 43)` — the DGP's
0.4 — and "REJECT H0 -> D.xY2 is driven by the others", "D.yY1 drives the
others". Guarded by `run_tests.sh` 8q, the first checks in the battery that ask
which series a label names.

---

## BUG-24 — the case-1 critical values of the rank test belong to the model with an unrestricted constant

**Status: OPEN.** Found 2026-09-23.

**What it is.** `lr_cval_none` (drvec.c:148) was extracted from `urca::ca.jo(ecdet
= "none")`. But `ca.jo`'s `"none"` still fits an **unrestricted intercept**
(`Z1 <- cbind(1, Z1)` in its source), so the table is for Johansen's
unrestricted-constant model — closer to drvec's case 3 — while drvec's case 1 has
no deterministic term at all.

**How it was found.** Reading `ca.jo`'s source, then measuring. Under H0 (two
independent random walks, no constant, `n = 500`), `tests/repro/mc_case1.py`
runs drvec itself 200 times: the test rejects in **1.0 %** of samples at the
nominal 5 %, and drvec's own statistic has an empirical 95 % quantile of **9.54**
against the **14.90** it prints. An asymptotic simulation of the no-constant
lambda-max (another reviewer, 20 000 reps) gives 4.23 and 11.20 at 5 % for `M-r =
1, 2`, in line with MacKinnon-Haug-Michelis case 1 (4.13, 11.22).

**What it cost.** Every `-case 1 -lrtest` verdict is badly undersized: rank is
under-detected. No figure in `HOMOLOGATION.md` uses case 1 for a rank decision
that I found, so the cost to the register is nil; the cost to a user is a
cointegration relation missed.

**Repro.** `sh tests/repro/repro.sh 24` (Monte Carlo, ~4 min).
**Suggested fix.** Replace the table with the no-constant lambda-max values
(Osterwald-Lenum Table 0 / MHM case 1), and add the size simulation to `SLOW=1`.

**Also, from the Mauricio study.** Mauricio (2006, §4) computes his p-values with
MacKinnon–Haug–Michelis (1999), and his case 1 (Remark 6) has no mean at all. His
own p-values reproduce only with the right tables: 24.95 % → 25.3 % with the
no-deterministic one, 95.51 % → 95.8 % with the restricted constant. drvec's
case-2 table is the right one (Osterwald-Lenum Table 1*; Johansen 1991 Thm 2.2).

---

## BUG-25 — `-lrtest` fits each rank from one cold start and reports what it lands on: on UKconsumption the two LRs have swapped places since 2026-08-17, and the register marks it ✔

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `fit_search`, `run_lrtest`). Found 2026-09-23 (external validation, `benchmark/drvec_runs_2026-09-23/`).

**What it is.** `run_lrtest` (~7239) calls `est` once per rank from `init_guess`.
It ignores `-multistart` (the flag is accepted and the table is identical), and
its verdict column only checks the sign of the LR: rows built on a `termcode 3`
stop, or on a one-iteration stop, still read *"reject H0 at 1 %"*.

**How it was found.** Running the published bank. On `urca_UKconsumption`
(logs, `2 0 1 -case 2 -lrtest`) drvec prints today
`LR(0→1) = 25.52, LR(1→2) = 70.12`. On 2026-08-17 (`benchmark/README.md`) it
printed `70.1280, 25.5141` — the same two numbers **in the other rows**. The
`r = 1` fit now stops at a local optimum (termcode 3, logL 535.17; `-seedjoh`
reaches 557.47), which shrinks the first step and inflates the second.
`HOMOLOGATION.md` §2.1 records the new, wrong-order pair as ✔: the register
checked that the two numbers appeared, not where. More cases in the validation:
Denmark M=5 (`r = 2` fit 33 log-units low, which flips the rank from 2 to 0),
rao5 (LR printed 54.99, true 9.98), rao4, UKpppuip (negative LR).

**What it cost.** The rank decision on UKconsumption survives by luck (both LRs
still reject). The rank on the Danish M=5 system does not. Every `-lrtest` table
in the register must be read as "a lower bound on each logL", which is not what
it says.

**Repro.** `sh tests/repro/repro.sh 25`.
**Suggested fix.** Honour `-multistart` in `run_lrtest` and seed each rank from
the neighbouring ranks' optima as well as cold; flag rows whose fits did not stop
on the gradient; and check order, not presence, in the register.


**Fixed.** Every rank is now the best of several starts (P12): the cold seed,
Johansen's canonical one, the nested MA chain, and `-multistart`'s perturbations,
which `-lrtest` now honours. A rank whose winning fit did not stop on the gradient
is marked in the table (`[check: rank r stopped by criterion N]`). Re-measured:
UKconsumption gives 70.1280 / 25.5141 again, and the r = 1 fit rises from 535.17
to 557.47; with `-multistart 30` the r = 2 fit reaches 573.96. The Danish M = 5
golden value rose from 828.84 to 832.36. Guarded by `run_tests.sh` 8p.2 and
8p.6 (SLOW). What is not fixed: a multimodal rank (UKconsumption r = 2) still
needs `-multistart` to reach its best point; the default search does not jitter.

---

## BUG-26 — a negative rank LR is printed as proof that a fit did not converge, and the bootstrap throws those draws away

**Status: OPEN.** Found 2026-09-23.

**What it is.** `run_lrtest` (~7331) prints *"NOT INTERPRETABLE: LR < 0, so at
least one of the two fits did not converge (rank r is nested in r+1)"*, and
`bootstrap_rank` (~1882) keeps a replication only if `l1 >= l0`. But the exact
likelihood of `[nabla Y2; W]` is not nested in `r+1` at the boundary
`Lambda → 0`, where `W`'s stationary initial-state term diverges.

**How it was found.** Under H0 (independent random walks), 75 of 197
replications print the message (`mc_case1.py`). On one of them
(`tests/repro/fixtures/neg.inp`) both fits stop on the gradient and the `r = 1`
optimum is the same to 1e-10 from a cold start, `-seedjoh`, `-seedgate` and
`-multistart 20`: logL −1417.6801 against −1416.4647 at `r = 0`.

**What it cost.** The message tells the user the program failed when it did not.
And discarding ~40 % of the null mass pushes the bootstrap quantiles and
p-values upward.

**Repro.** `sh tests/repro/repro.sh 26`.
**Also, from the Mauricio study.** It is not a small-sample accident. Under H0
(1000 replications) the exact LR is shifted against Johansen's conditional LR by
−2.53, −3.70, −4.91 at T = 100, 400, 1600 — roughly −log T — the share of negative
LRs grows from 22 % to 49 %, and the size at the correct 5 % value falls from
3.9 % to 2.4 %. Independent refits confirm the negative LRs are true optima. So
censoring at 0 does not repair the distribution, and the asymptotic claim needs
BUG-27's caveat.

**Suggested fix.** Keep every draw in the bootstrap (it is the only valid
reference for the exact LR); reword the message to "not nested at the boundary";
do not present the asymptotic table as valid for the exact LR.

---

## BUG-27 — with `q >= 1`, the default `-lrtest` compares two MA classes that are not nested

**Status: OPEN.** Found 2026-09-23.

**What it is.** At `r = 0` `Theta` is free (`ma_struct_on` returns 0); at `r >= 1`
the default `marow` class zeroes `q·s·M` entries. The lambda-max tables assume the
same short-run space under H0 and H1.

**How it was found.** Code reading, then measured on `tests/repro/fixtures/lq.inp`
(`2 1`): `npar` goes 10 → 11 under the default and 10 → 13 with `-mafree`; the LR
is 11.22 against 13.52.

**What it cost.** The statistic the table is compared with is not the one the
table is for, by ~2.3 points on this case.

**Repro.** `sh tests/repro/repro.sh 27`.
**Also, from the Mauricio study.** Theorem 8 (`DEMOSTRACIONES.md`) cites Yap &
Reinsel (1995) Theorem 3, which is a **trace** test (r against full rank) with
unrestricted, strictly invertible `Theta`, for the **conditional** Gaussian
likelihood. drvec runs a sequential lambda-max test on the **exact** likelihood,
whose extra initial-state term makes the models non-nested as `Lambda → 0`
(BUG-26). Mauricio's Remark 5 ("MA terms do not affect the distribution") is
proven for the conditional LR only. And drvec cannot test `r = M−1` against `M`,
which Mauricio's Table 3 does.

**Suggested fix.** Force `-mafree` inside `-lrtest`, or refuse the critical values
when `q > 0`.


**Partly fixed on 2026-09-23.** With the free class as the default (BUG-48) the
default `-lrtest` fits the same MA class at every rank, so the non-nesting between
`r = 0` and `r ≥ 1` is gone unless a restricted class is asked for. The mismatch
with Theorem 8's source (a trace test for the conditional likelihood; drvec runs a
sequential lambda-max on the exact one) remains.
---

## BUG-28 — the `-alpha` file's rows are read in the internal order, and the test that should catch it compares a word with itself

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `build_weakex_A`, `init_guess`, `vec_shootx`, `report_fit`).

**What it is.** `load_alpha_A` (~2150) stores file row `i` at `alpha_A[i]`, which
is the internal order `[Y1;Y2]`; `build_weakex_A` goes through `inp2lam`. So the
USAGE.md example *"A declaring equation 2 not to adjust"* (`A = [1; 0]`)
restricts equation 1.

**How it was found.** Comparing LRs on mink-muskrat (`2 1 1 -case 2`):
`-alpha [1;0]` gives 6.2969985, identical to `-weakex 1`; `-weakex 2` gives
6.7481296. The battery's check (`tests/run_tests.sh:1170`) extracts field 5 of
`LR = 2(free - restr.) : 6.74…`, which is the literal `restr.)`, so its "exact
agreement" compares `restr.)` with `restr.)` and always passes.

**What it cost.** Any user-supplied `A` restricts the wrong equations, silently,
since the feature exists; and a check that has never been able to fail.

**Repro.** `sh tests/repro/repro.sh 28`.
**Suggested fix.** Store file row `i` at `alpha_A[inp2lam(i)]`; print `$NF` in
`lr_of`.


**Fixed.** A is kept in the `.inp`'s order (the order the user writes it and
`-weakex` names) and permuted where it meets Lambda, with the current r:
internal row i reads A's row `lam2inp(i)`. The battery's `lr_of` reads the last
field (`$NF`), so its "exact agreement" check compares numbers again.
Re-measured: `-alpha [1;0]` = `-weakex 2` (LR 7.3913497007).

---

## BUG-29 — `-lrtest` with `-weakex` or `-alpha` restricts a different series at each rank when `M >= 3`

**Status: FIXED on 2026-09-23** (`src/drvec.c`: A permuted with the rank in force, see BUG-28).

**What it is.** `A` is built once in `main` (~8342) with `inp2lam` at the
command-line `r`; the internal order depends on `r` (`s = M - r`), so at other
ranks the same internal row is another series.

**How it was found.** `datasets/synthetic/rank2.inp`: the `-lrtest -weakex 1`
table gives logL −479.4583 at `r = 2`; the standalone `r = 2` fits give −480.7040
(`-weakex 1`), −480.0489 (`-weakex 2`) and −479.4583 (`-weakex 3`).

**What it cost.** The weak-exogeneity rank sequence tests a different hypothesis
at each rank.

**Repro.** `sh tests/repro/repro.sh 29`.
**Suggested fix.** Keep `A` in the `.inp`'s order and permute it inside the cast
with the current `global_r`.


**Fixed** by BUG-28's fix: A is no longer permuted once at the command-line r.
Re-measured on `rank2.inp`: the r = 2 row of `-lrtest -weakex 1` is -480.7040,
the standalone `-weakex 1` fit.

---

## BUG-30 — `-warma` with `-alpha`/`-weakex` reads past the end of the parameter vector and does not impose the restriction

**Status: OPEN.** Found 2026-09-23.

**What it is.** `par_blocks` (~626) counts `alpha_sa·r` slots for `Lambda`; the
`-warma` branch of `vec_shootx` (~3747) ignores `global_alpha` and reads `M·r`.
The vector falls out of step and `B2` is read past its end.

**How it was found.** valgrind: *Invalid read … vec_shootx (drvec.c:3778) … 0
bytes after a block of size 64*, thousands of times; the run exits 0.

**What it cost.** A fit that claims a restriction it does not impose, built on
memory that is not its own.

**Repro.** `sh tests/repro/repro.sh 30` (needs valgrind).
**Suggested fix.** Refuse the combination in `validate_cli`, or implement
`Lambda = A psi` in the `-warma` branch.

---

## BUG-31 — three option combinations crash with SIGSEGV

**Status: OPEN.** Found 2026-09-23.

**What it is.**
1. `-differenced` with `-matest`/`-artest`: `simulate_h0` (~1795) writes row
   `nobs_raw + 1` of `sim`. Wrong in substance too: it simulates levels into a
   matrix that expects `nabla Y2`. `-lrtest` already refuses `-differenced`;
   `run_ma_ar_test` does not.
2. `-lrtest -fixb2 -bootstrap` with `M >= 3`: `init_guess` reallocates `B2_fixed`
   on every call, including inside the bootstrap's `fit_ll(rr+1)`, so the H0
   model's `B2` is the previous replication's and the last `vec_shootx` reads out
   of bounds (valgrind at ~3946).
3. `-warma -seedgate`: `init_guess` (~3199, `lusol`) via `gate_profile_seed`.

**How it was found.** Review; the three reproduce with exit 139.

**What it cost.** Three crashes on documented options; (2) also runs its
bootstrap under the wrong null before it dies.

**Repro.** `sh tests/repro/repro.sh 31`.
**Suggested fix.** (1) refuse the combination; (2) snapshot `B2_fixed` per rank
next to `xkeep[rr]`; (3) refuse or rebuild the gate for `-warma`.

---

## BUG-32 — `-seedgate` holds the `r = 0` optimum in the wrong coordinates, and with `-fixb2 v` it replaces `v` by 0

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `gate_profile_seed`, `fit_r0_ladder`). Found 2026-09-23.

**What it is.** `gate_profile_seed` (~4204) copies `hold_F`, `hold_Th`, `hold_S`
from the `r = 0` fit without permuting them: at `r = 0` the internal order is the
`.inp`'s, at `r0 > 0` it is not. It also calls `init_guess` with `global_r = 0`,
which frees and reallocates `B2_fixed` with `r = 0`, so a fixed `B2` comes back as
0 (`calloc`); and it overwrites `cond_resid` with the `r = 0` residuals, so
`-seedgate -writeres` writes the wrong ones (code reading).

**How it was found.** Review. Patching the permutation in raises the profiled
start on mink-muskrat from −60.37 to −1.37 (the `r = 0` rung is −6.33); on a
synthetic case from −1328.2 to −838.0 (rung −873.3). `-seedgate -fixb2 1.0`
reports `B2 = 0.000000 [FIXED at the value given]` and logL 0.639, against
`1.000000` and −21.091 without `-seedgate`.

**What it cost.** The final optimum survives on the cases tried because the
optimiser recovers; but every statement that "route B is measured to be worse"
(`run_tests.sh` ~787, `VEC_EMBEDDING_PLAN.md`) rests on the wrong-coordinate
start. And `-seedgate -fixb2 v` estimates a different model from the one asked.

**Repro.** `sh tests/repro/repro.sh 32`.
**Suggested fix.** Permute `hold_*` into the internal order (and renormalise the
`Sigma` block by `hold_S[1][1]`); save and restore `B2_fixed` and `cond_resid`
around the `r = 0` `init_guess`.


**Fixed.** Route (B) now takes the `r = 0` optimum from the ladder
(`fit_r0_ladder`: gate, `Σ` free, structure) and permutes `F`, `Θ` and `Σ` into
the internal order before holding them, renormalising `Σ` so its `[1][1]` is 1;
afterwards one `init_guess` at the rank restores `B2_fixed` and `cond_resid`.
The ladder is now the default start for `r ≥ 1` (P12). Re-measured: on
mink-muskrat the profiled crossing is -0.15 over an `r = 0` rung of -1.05; the
Danish M = 5, r = 2 fit rose from 832.36 to 858.34. The claim that "route B is
measured to be worse" (VEC_EMBEDDING_PLAN, run_tests ~787) rested on this defect
and no longer stands; route (B) wins or ties in most bank cases, not all.
---

## BUG-33 — `-fixb2 v` builds its starting point from the static-OLS `B2`, not from `v`

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `init_guess`, `fit_search`). Found 2026-09-23 (external validation).

**What it is.** `init_guess` seeds `E[W]`, `Lambda`, `F` and `Sigma` from the
static-OLS `W` and only then overwrites `B2` with `v` (~3243-3256, ~3622-3629),
so the start is inconsistent with the model being fitted.

**How it was found.** UK consumption-income `[incl; conl]`, `2 0 1 -case 2
-fixb2 0`: it ends at −275.36 (termcode 3). The same data with `incl` demeaned —
which leaves the case-2 likelihood unchanged — ends at 491.51 on the gradient.
The free fit is 523.73 on both.

**What it cost.** The LR for `B2 = v` that `-h` advertises comes out as 1598
instead of about 64.

**Repro.** `sh tests/repro/repro.sh 33`.
**Suggested fix.** Seed the other blocks from `W = Y1 + v'Y2` when `B2` is fixed.


**Fixed.** Under `-fixb2 v` (and `-seedb2 v`) the rest of the seed is built from
W = Y1 + v'Y2. The old seed is kept as one more start of the search, because it
is sometimes the better one (on mink-muskrat `-case 2 -fixb2 0` it still wins).
Re-measured: the raw and the demeaned UK consumption-income data now both give
491.5121303549; `-mafree -fixb2 0` on mink-muskrat rose from -8.48 to 3.01.
Guarded by `run_tests.sh` 8p.3.

---

## BUG-34 — `-fdhess` publishes standard errors built from the penalty whenever the Cholesky succeeds

**Status: OPEN.** Found 2026-09-23.

**What it is.** `exact_hessian_se` checks the count of rejected evaluations
(`fdh_rej`) only when the Cholesky fails (~1554-1585); on the success path
(~1587-1596) a step that hit the 1e10 penalty in `fdh_obj` enters the second
differences as enormous curvature.

**How it was found.** Instrumenting `fdh_rej`: VILL `2 1 1 -case 2` has 1
rejected step, VILL `2 2` has 3, SLL and VILL `-matri` 1 each.

**What it cost.** VILL prints `D.Vienna <- A.Vienna(-1) 1.000043 0.000000
123026806182.401 ***` under the heading "finite-difference Hessian at the
optimum", with no warning; the MA root is 0.99996.

**Repro.** `sh tests/repro/repro.sh 34`.
**Suggested fix.** If `fdh_rej > 0`, take the boundary branch (BFGS SEs and a
warning) or mark the affected coordinates as undefined.

---

## BUG-35 — `-artest` and the bootstraps' fits skip the admissible-start ladder that `-warma` needs

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `make_admissible`, `fit_search`). Found 2026-09-23 (two reviewers).

**What it is.** `main` (~8653) and `run_specs` shrink the `-warma` start through
{1, .8, .5, .3, .1, 0} until it is admissible; `run_ma_ar_test` (~7115) and
`fit_ll` (~1823) go straight from `init_guess` to `est`.

**How it was found.** mink-muskrat `2 1 1 -case 2 -artest 20` prints *"one of
the two fits failed (restricted failed, unrestricted ok); no test"*; plain
`-warma` on the same data converges ("start shrunk to x0.3", logL −13.7366).
The comment at `run_tests.sh` ~1141 ("-warma does not estimate there … not a
defect") predates the ladder.

**What it cost.** The AR comparison is unavailable on the canonical case, and
bootstrap replications under `-warma` may fail for the same reason.

**Repro.** `sh tests/repro/repro.sh 35`.
**Suggested fix.** One helper with the ladder, called from all four places.


**Fixed.** Every start of every fit goes through `make_admissible`, the ladder
`-warma` had, now for all callers (under `-warma` it shrinks from the first
block). Re-measured: `-artest 20` on mink-muskrat runs, restricted logL
-13.7366493880 (the plain `-warma` fit), LR = 27.4986 on 2 df. Guarded by
`run_tests.sh` 8p.4.

---

## BUG-36 — on the `.pre` route, forecasts and the rolling evaluation are in the engine's units, not the user's

**Status: OPEN.** Found 2026-09-23.

**What it is.** `read_pre_inputs` builds `rawmat = refactor·BoxCox(z) − det`
(~5476). `forecast_vec` and `rolling_eval` never divide by `refactor`, never
invert the Box-Cox and never add the deterministic path back, while the
`.forecast` header says *"Levels in the units of the .inp"*. `-interv` on the
`.inp` route has the same shape (code reading). With `-differenced`, the `Y2`
levels are cumulated from an arbitrary zero (`build_y2_levels` ~575), so level
forecasts and MAPE for `Y2` are meaningless.

**How it was found.** mink/muskrat `.pre` files with `rescaling 100`: forecasts
1379.5 and 1098.2 instead of ~13.8 and ~11.0; with a step of 5 the mink forecast
is 5.65 for a level of ~10.4. Differenced mink-muskrat: muskrat forecast 1.41
instead of ~13.6, MAPE(1) 107.9 % instead of 2.98 %.

**What it cost.** The `.pre` route exists to put the VEC forecast next to the
univariate ones from fue; they are not comparable, and the file says they are.

**Repro.** `sh tests/repro/repro.sh 36`.
**Suggested fix.** Carry each series' `refactor`, `lambda` and future
deterministic path and back-transform levels and bands (or label them in `w`
units); refuse level output under `-differenced`.

---

## BUG-37 — the per-series residual diagnosis, and the header, date the sample one period early

**Status: OPEN.** Found 2026-09-23.

**What it is.** `diagnose.c` dates residuals from `trans_d + trans_D·freq`
(~388); drvec never sets `trans_d` (drvec.c:170) although the default levels
layout drops the first observation (`nobs = nobs_raw - 1`, ~543).

**How it was found.** `data/synth.inp` (99 annual obs, 2000–2098), `2 0 1`:
the header says *"98 observations from 2000"* and the diagnosis
*"98 observations: from 2000 to 2097"*; the effective sample is 2001–2098.

**What it cost.** Every date the diagnosis prints — plots, min/max, the outlier
table — is one period early. That is the date an analyst uses to place an
intervention.

**Repro.** `sh tests/repro/repro.sh 37`.
**Suggested fix.** Pass the offset (1 under `global_levels`) to the diagnosis and
to the header.

---

## BUG-38 — the univariate evaluation of a seed `.pre` drops a fixed mean, the seasonal and fixed-frequency factors, and the `ifadf` differences

**Status: OPEN.** Found 2026-09-23.

**What it is.** In `pre_univariate`:
1. `uv.mu[1] = (Tm->Imu ? Tm->mu : 0.0)` (drvec.c:2973): a **fixed** mean is
   evaluated as 0. fue uses the value whatever the flag (`fue.c:2749`).
2. `p`/`q` count only the regular factors (~2961, 3070, 3088) and
   `expand_*_factors` truncate to that buffer (fue_bridge.c ~98-135): annual and
   fixed-frequency factors vanish when there is no regular one and are truncated
   when there is. drtran sizes the expansion with `total_ar_order` /
   `total_ma_order`; drvec did not copy those.
3. The guard (~2955) checks `nrdiff`/`nadiff` but not `ornsop`, so `ifadf`
   factors are ignored.

**How it was found.** Bridge review, confirmed by changing one field at a time:
the same `.pre` with `0.073615 1` and `0.073615 0` gives a univariate sum of
−9.7537 and −9.9454; an annual AR(1) = 0.5 alone gives the same logL as no AR
(the seed warning says "AR of order 0"); `ifadf = 0 0 0 0 0 0 1` gives the same
logL as all zeros.

**What it cost.** `seed_logl`/`seed_var` — the crossing identity, the `Sigma`
seed and the optimality certificate — are wrong for these files. (1) became
reachable on 2026-09-23: fue's writer now emits a fixed non-zero mean as
`<mu> 0` (fue BUG-0021) instead of dropping it.

**Repro.** `sh tests/repro/repro.sh 38`.
**Suggested fix.** `uv.mu[1] = Tm->mu`; port drtran's order totals; add
`|| Tm->ornsop != 0` to the guard.

---

## BUG-39 — the shared `.pre` reader accepts malformed files: stack overflow, uninitialised fields, a crash, and silent zeros

**Status: OPEN.** Found 2026-09-23. **Shared**: `src/fue_pre_reader.c` is
code-identical to drtran's (`drtran/src/fue_pre_reader.c`), so all of it applies
to the drtran binary.

**What it is.**
1. `char namef[80]` filled by an unbounded `%s` (~465-470): a long series name
   overflows the stack.
2. A 2- or 3-token date line (the DRVUS form `62 1850`) leaves the start year and
   the name uninitialised (no `sscanf` count check); on the `-seed`/`-interv`
   routes `begyear` is stack garbage.
3. An empty `ifadf` line: `off` is used uninitialised (~625) → SIGSEGV.
4. Truncated data (`break` at ~640 leaves calloc zeros), `NA` read as 0, a blank
   line shifting the series, a missing bands line shifting it by 2 and leaving
   `refactor` uninitialised.
5. On an early `return 2`, `free_fue_pre` walks uninitialised `detspec` entries
   (valgrind at fue_bridge.c:255). The error text says "drtran".

**How it was found.** A probe linking drvec's reader objects, then `bin/drvec` on
the fixtures in `tests/repro/fixtures/`.

**What it cost.** A 100-character name makes a λ = 1 file read as λ = 0 and fail
with a wrong message; an empty `ifadf` line kills the program; a truncated file
is estimated with its last observations set to 0, exit 0. With the short date
line a `step 1900` falls outside the garbage sample and is silently not
subtracted: the fit changes completely (`F` 0.0735 → 0.000157). fue.load rejects
or handles every one of these.

**Repro.** `sh tests/repro/repro.sh 39`.
**Suggested fix.** `%79s`; check every `sscanf` count; initialise `off = 0` and
`refactor = 1`; fail when fewer than `nobs` values are read; `calloc` for
`detspec`. Fix it once, in both copies.

---

## BUG-40 — the deterministic component of a `.pre` uses the exact rational filter; fue truncates `nu(B)` at 40 lags

**Status: OPEN.** Found 2026-09-23. **Shared** with drtran's copy of
`build_det_component`.

**What it is.** `build_det_component` (fue_pre_reader.c ~394-400) applies
`omega(B)/delta(B)` exactly; fue (`fue.c` ~2763-2767, `NuLag = 40`, and
`cast_us.py` ~330) truncates the impulse response at 40 lags.

**How it was found.** `step 1855`, ω = 5, δ = 0.95, 62 annual observations: the two
agree to `t = 40`; at `t = 62` drvec subtracts 94.63 and fue 87.79.

**What it cost.** On the `.pre` route and with `-interv`, drvec subtracts a
component other than the one the `.pre`'s ω and δ were estimated with, whenever
the series is longer than 40 observations and δ is near 1.

**Repro.** The probe in the review folder; to be added to `repro.sh` with a
`.pre` carrying a step with δ.
**Suggested fix.** Agree with fue — truncate at 40 — or change fue; the two must
compute the same regressor.

---

## BUG-41 — engine: the line search never returns on a NaN objective, termcode 3 reports the objective at the rejected point, and `est` leaks on a bad start

**Status: OPEN.** Found 2026-09-23. **Shared** with canonical drvarma
(`drvarma_v.04.1`), whose code drvec's engine is identical to.

**What it is.**
1. With a NaN objective the interpolated step is NaN, every comparison in
   `lnsrch` (qnewtopt.c ~489-541) is false, and the search never gives up.
   drtran met it on real data and fixed its own copy of `objcfunc`
   (`drtran/src/drvmlest.c` ~180-197, `if (!isfinite(f)) return 1.0;`); drvec and
   drvarma do not have the guard. Whether drvec's `elf` can produce a NaN is not
   established (`choldcp` and `chekma` let a NaN parameter through).
2. When `lnsrch` gives up it resets `xkp1 = xk` but leaves `*fkp1` at the trial
   value (~492/504), and `raxopt` returns it: it feeds the "Objective function"
   line and `pi1`. Measured error ~3e-8 relative on mink-muskrat.
3. `est` returns at an invalid initial point without the dealloc cast
   (drvmlest.c:93).

**How it was found.** `tests/repro/probes/probe_nan.c` calls the real `raxopt`
with a NaN objective (still looping after 10 s); an instrumented copy for (2).

**What it cost.** (1) a hang instead of an error; (2) ~3e-8 today, a factor 1/f
on the standard errors if the rejected trial were infeasible (not observed).
fue's Python port already fixes (2), so the Python ports disagree with the C and
with each other here.

**Repro.** `sh tests/repro/repro.sh 41`.
**Suggested fix.** In the canonical drvarma: drtran's `isfinite` guard,
`*fkp1 = fk` in `lnsrch`'s give-up branch, the missing dealloc; then propagate.

---

## BUG-42 — `diagnose.c`: a heap overflow in the histogram, `chisq` capped above 1000, and portmanteau/JB tests with the wrong reference distribution

**Status: OPEN.** Found 2026-09-23. **Shared** with drvarma (byte-identical file).

**What it is.**
1. `File_HistSer` (~797-814): with a residual beyond 4σ the label width is 2 but
   a count of 100 or more prints 3 characters; several in a row overrun the
   66-byte rows. A sibling of BUG-16.
2. `chisq` (nlatools.c:839) returns 1 for `x > 1000`, so p-values are 0 for
   `df ≳ 900`. Hosking's `df = m²·⌊√n⌋` passes 1000 at M=5 from n ≥ 1681, at M=6
   from n ≥ 900.
3. Hosking uses `df = m²·s` and the per-series Ljung-Box `df = lag`: neither
   subtracts the estimated parameters.
4. The "multivariate" Jarque-Bera is a sum of univariate ones with `df = 2m`,
   which over-rejects with correlated residuals (7.2–10.4 % at nominal 5 %,
   simulated).

**How it was found.** ASan on `tests/repro/probes/probe_hist.c` (n = 700, one
value at 6σ): *heap-buffer-overflow … File_HistSer diagnose.c:817*; `probe_chi.c`
against GSL (`chisq(1001, 1000) = 1`, GSL 0.515); a size simulation for (4).

**What it cost.** (1) memory corruption on samples of ~700 or more with one
outlier; (2) "REJECT" on every large system; (3) on `synth`, Q = 35.70 prints
p = 0.483 where df 28 gives 0.150 — biased toward "white noise"; (4) mild
over-rejection.

**Repro.** `sh tests/repro/repro.sh 42`.
**Suggested fix.** Fixed-width labels; remove the cap (Wilson-Hilferty holds
there) or use `gsl_cdf_chisq_Q`; subtract the dynamic parameters from the df;
Lütkepohl's multivariate JB.

---

## BUG-43 — no fallback from a non-stationary start, and data in raw units stop the optimiser at its first step

**Status: OPEN.** Found 2026-09-23 (external validation).

**What it is.**
1. When the start is non-stationary (`ifault = 3`) the fit fails; there is no
   fallback to a shrunk or OLS start. `-seedjoh` aborts the same way (exit 2)
   instead of falling back to the OLS seed, and its canonical seed uses the
   unrestricted constant and one observation fewer, although its comment says
   case 2.
2. On data whose scale is far from 1 the optimiser stops at iteration 1 at every
   rank and reports `ok (ifault = 0)`, exit 0.

**How it was found.** Rao's tables. `rao6` fails at every rank in case 2 and in
case 3, although an OLS VAR on `nabla Y` with a constant is stationary
(max |eig| 0.966). `rao7` in raw units gives LRs 7.26 / 61.28 / 0.21; divided by a
constant per column — the same model, the same LRs in theory — 80.09 / 39.09 /
11.84.

**What it cost.** One published case the program cannot fit, and one where it
prints a rank sequence built on fits that never moved.

**Repro.** `sh tests/repro/repro.sh 43`.
**Suggested fix.** A fallback ladder for non-stationary starts; rescale
internally (the `.pre` route already has `refactor`); flag fits that stop at
iteration 1.


**Partly fixed on 2026-09-23 (P12).** The fallback exists now: a start the engine
refuses is shrunk until it is accepted, and the search tries several. `rao6` in
case 3 now fits (LR(0) = 79.76), where every rank failed before. **Still open:**
the scale. `rao7` in raw units gives LRs 38.57 / 29.96 / 6.14 against 80.09 /
39.09 / 11.84 rescaled — better than 7.26 / 61.28 / 0.21, not right. That is the
optimiser's scaling, not the starting point, and the search cannot fix it.
---

## BUG-44 — the `.inp` writer loses precision on small series and can write a zero rescaling factor

**Status: OPEN.** Found 2026-09-23.

**What it is.** `write_inp_series` writes the data with `%.10f` (~2811) and
`refactor` with `%.2f` (~2805).

**How it was found.** A series around 1e-7 comes out with 3–4 significant digits
(`0.0000001039`); any `refactor <= 0.005` is written as `0.00`, which fue reads as
1, while the μ seed was already multiplied by the true factor (1e5× off scale).

**What it cost.** The files drvec writes for fue (`-writeinp`, `-writeres`) do not
reproduce the series for small scales. It is the writer-side twin of fue
BUG-0021/art BUG-0188, fixed there on 2026-09-23.

**Suggested fix.** `%.17g` for the data, `refactor` and seeds.

---

## BUG-45 — options accepted and ignored, and smaller report inconsistencies

**Status: OPEN.** Found 2026-09-23. Collected in one entry because each is small;
split any of them when it is fixed.

**What it is.** How it was found: each one run, in the 2026-09-23 review.
- `-name NAME` is ignored on the `.inp` route (no `NAME.out`).
- `-case 1 -mean` becomes case 2, although the comment says an explicit case wins.
- `-writeinp A -writeres B` share one prefix: both are written as `B.*`.
- Silently ignored: `-bootstrap` without `-lrtest`; `-f`/`-estwin` with any mode;
  several modes at once (`-lrtest -specs` runs only `-specs`).
- `-warma` ignores `-diagar`/`-diagma` while the header says "F diagonal".
- `-specs` with `q = 0` marks every non-`warma` rung "NO" (the `granger_sv = -1`
  sentinel read as a failure) and prints `MAmin 0.000`.
- ~~With `-multistart` the convergence note is the last start's, not the best's
  (`termcode_from_out` parses the text; code reading).~~ **Fixed 2026-09-23
  (P12)**: the search writes only the winning start's optimizer report to the
  `.out`, so the last criterion there is the winner's.
- The roots table says "Inverse roots" and prints root moduli.
- The `-warma` "same fit in VEC coordinates" block prints `Lambda` (Mauricio's
  sign) and `Pi` in the internal order, unlabelled, while the main report uses
  `alpha = -Lambda` in the `.inp`'s order.
- `-rungs`: USAGE.md says each rung starts from the one below; the code cold-starts
  every rung. With `-lrtest` the header prints the placeholder `r`.
- `-eval` prints `suma univariante … diferencia` in Spanish (P7 says English).
- `usage()` says `0 < r < M` (0 is accepted) and `-case 2 (-mean needed)` (it is
  not).
- In case 3 the printed VEC equation omits the drift `gamma`; the estimation
  itself is right (Mauricio study).

**What it cost.** Each one is a place where the program does something other than
what the user asked or read, without saying so; P1 was meant to close this class.

**Repro.** `sh tests/repro/repro.sh 45` (the first two).
**Suggested fix.** A post-parse applicability and conflict check that exits 2.

---

## BUG-46 — the rank diagnostic computes `sigma_min(Lambda_perp' Theta(1) B_perp)`, which is not Theorem 3's condition: it denies a correct rank

**Status: OPEN.** Found 2026-09-23 (study of Mauricio 2006 against the code,
`docs/ESTUDIO_MAURICIO_2026-09-23.md`).

**What it is.** Theorem 3 (`DEMOSTRACIONES.md`) proves that the rank is exactly
`r` if and only if `rank(Lambda_perp' Theta(1)) = s` — an `s x M` matrix; its
proof drops `B_perp` precisely because it has full column rank.
`granger_smin` (drvec.c:390-452) computes the smallest singular value of the
`s x s` product `Lambda_perp' Theta(1) B_perp`, a strictly stronger condition:
sufficient, not necessary. The same document then says that is "the condition
the program reports".

**How it was found.** Checking the theorem against the code. Counterexample in
the default class: `Lambda = (.5, -.2)'`, `B2 = -1`, `Theta_1 = [[.5, 3], [0, 0]]`.
The MA is invertible, `det Theta(1) = 0.5`, `Lambda_perp' Theta(1) = (.1, -.1)`
has rank 1 = s (the rank is right, also by simulation), and `G = 0`. On 2001
observations from this model `drvec g0 1 1 1` prints *"This fit DENIES THE
RANK"*.

**What it cost.** Every consumer of `G`: `-rankadm`, the `adm` column of `-specs`
and the p-value decisions built on it, the warning, and the 0.2 floor calibrated
on `G`; the `HOMOLOGATION.md` §4h/§4j measurements, including "-matri fails",
need re-measuring. Under `-mawarma`/`-warma`, `Theta(1) B_perp = B_perp`
identically, so the reported condition does not depend on `Theta` at all.

**Suggested fix.** Report `sigma_s(Lambda_perp' Theta(1))` — or, better, the
equivalent `sigma_M([Phi(1) Theta(1)])`, which is left-coprimeness at `z = 1`
(`STUDY_M3.md`: proved, and checked on 20 000 draws).

---

## BUG-47 — `-m 2` is labelled "Conditional (Approximate) ML" and is the untruncated exact likelihood; the default is exact ML truncated at 1e-3, and the entry gate uses that truncation as its tolerance

**Status: OPEN.** Found 2026-09-23. Noted before as H1/H2 in
`external_review.md`; confirmed here from the engine.

**What it is.** `met` only sets `xitol = ±1e-3` (drvec.c:3693 and eight other
places). In the engine `xitol` is the truncation threshold of the xi sequence
(`cxi`, elfvarma.c ~736-802): with the negative value the test `s2 < xitol`
never holds and nothing is truncated. So `-m 2` is the **untruncated exact**
likelihood, printed as *"Conditional (Approximate) Maximum Likelihood"*
(drvec.c:8420), and `-m 1`, printed as exact, is truncated. The source comment
`met: 1 = exact, 2 = approximate` reads backwards. The paper's conditional ML
(its CML columns) is not available in drvec at all.

**How it was found.** Reading `cxi`; measured: at fixed parameters `-m 1` moves
logL by up to 1.6e-3 to 4.6e-3 against the untruncated value, which matches an
independent exact likelihood to 5e-8. The entry gate (`gate_contract`) compares
the joint diagonal fit with the sum of the univariate ones using `xitol` as the
tolerance, while the truncation error grows like `xitol/(1 - |theta|)`: on a
simulated independent pair it prints NOT VERIFIED with a gap of 1.148e-3; a
rebuild with `xitol = 1e-10` closes it to 2e-8.

**What it cost.** A results file that names the wrong estimator; no way to
reproduce the paper's CML columns; an entry gate that can fail a correct model.

**Suggested fix.** Relabel `-m 2` as "exact, untruncated", or make it the default
and drop the truncation; derive the gate tolerance from the truncation actually
used; implement the conditional likelihood if the CML comparison is wanted.

---

## BUG-48 — the default MA class rests on Corollary 6.3, and Corollary 6.3 is false: the engine's invertibility gate is not the admissibility condition

**Status: FIXED on 2026-09-23** (`src/drvec.c`, `parse_cli`: the default is the free `Θ`). Found 2026-09-23.

**What it is.** With `q >= 1` and no class chosen, drvec fits `-marow`
(drvec.c:8169-8186), zeroing the `nabla Y2` rows of every `Theta_k`. The code
justifies it by Corollary 6.3: *"it is the parameterisation in which the
admissible region IS the whole space, and the invertibility gate the engine
already applies imposes it"*. That is false in both directions:
`Theta_1 = [[1, .7], [0, 0]]` (a `-marow` point) passes `chekma` (threshold
1.00005) and is inadmissible; `Theta_1 = [[3, .4], [0, 0]]` is rejected by
`chekma` and is admissible (`det Theta(1) = -2`, Corollary 3.1). The gate checks
invertibility; admissibility is a condition at `z = 1`. In every class an
inadmissible point has an MA eigenvalue of exactly 1; `-marow` only removes the
`T22 -> I` route to it.

**How it was found.** Verifying `DEMOSTRACIONES.md` result by result
(`docs/estudio_mauricio_2026-09-23/M3/`); the two counterexamples check by hand.

**What it cost.** The default is a proper subclass of Mauricio's model that the
paper's own example lies outside (the muskrat row of `Theta_1` is
(-0.8953, -0.0174) in Table 4 and (-0.6039, -0.1837) in Table 5), adopted on an
argument that does not hold. It may still be the right default — HOMOLOGATION
§4q/§4r measure it — but no test of `-marow` against the free model with a valid
distribution exists (`SPECIFICATION_PLAN.md` §9 says so). It also makes the
default `-lrtest` non-nested (BUG-27).

**Also, the source it was attributed to (`ESTUDIO_MAROW_2026-09-23.md`).** The
BVECM article does not motivate `-marow`: its restricted class (Definition 3 of
the appendix) is `-warma`, used only to frame the proofs, and the article says
three times that what it estimates is the full model. The legacy program that
accompanies it (`drv_project`) estimates a full AR plus **one** MA coefficient,
`θ22` — the `nabla Y2` equation's own —, which is exactly the entry `-marow` sets
to zero. On the wheat pairs that coefficient is significant whenever `nabla Y2`
has an AR lag (`-matri` beats `-marow` by LR ≥ 5.96 in 37 of 40 configurations),
and `HOMOLOGATION.md` §4t already found the free class forecasting better in 8 of
9 cases. The remaining argument for the default is §4q/§4r: estimability in short
samples.

**Suggested fix.** Correct Corollary 6.3 and the code comment; state the default
as a restriction with its measured justification; report the left-coprimeness
diagnostic of BUG-46; build the `-marow`-versus-free test.


**Fixed.** The user's decision, taken on the two studies above: the default is
the free `Θ`, Mauricio (2006)'s model, and the search of P12 carries the
estimability problem that motivated the restriction (the free fit starts from
the `-marow` and `-matri` optima, so it never ends below them). `-marow` remains
as an option. Corollary 6.3 is marked false in `DEMOSTRACIONES.md`; the decision
is `SPECIFICATION_PLAN.md` §11. Re-measured: the battery's flagless golden
values now equal their `-mafree` counterparts to the digit; the `-marow` ones are
kept with the flag. What is not done: the bootstrap test between classes.
---

## BUG-49 — on the bank the moving average ends on the invertibility boundary, and the report publishes that point as an interior optimum

**Status: OPEN** (partly fixed: reported and diagnosed, see below). Found 2026-09-23, comparing the ladder with the cold start.

**What it is.** With the free `Θ` the fitted MA has a root at the engine's
invertibility gate (modulus 0.99995–1.00000, marked `*`) in **5 of 5** bank cases
(`mink_muskrat`, `milan`, `vienna`, `penn`, `utrecht`, `2 1 1 -case 2 -mean`), and
by either start: ladder or cold. With `-marow` the single start stayed interior,
but the ladder finds **higher** optima on Vienna (23.84 against 23.01) and Penn
(38.74 against 33.62), and those are on the boundary too. The likelihood keeps
rising towards a non-invertible MA and the optimiser stops against the wall: the
point is not an interior maximum. drvec prints *"A root sits on the unit circle"*,
but then reports standard errors, t statistics and likelihood ratios as if it
were, and the search picks the highest point on the wall as "the optimum".

**How it was found.** Running each start alone (P12, 2026-09-23): the
difference between routes was not "better or worse optimum" but where on the
boundary each one stops.

**What it cost.** Every MA fit on the bank: the standard errors are not defined
along the binding direction (Theorem 10's hypothesis `Ω > 0` fails), LR tests
against or between such fits have no known distribution, and the comparison of
starts is a comparison of boundary points. And it is **information thrown
away**: an MA unit root is the signature of over-differencing — the paper's own
reading of mink (Mauricio 2006, §4.3) — so it says something about the
specification (the integration order, the rank) that the report does not say.

**Suggested fix.** Detect it on the fitted `Θ*`, say which block carries the root
(the `nabla Y2` rows or the `W` block) and what that suggests, mark the MA
standard errors and every LR that uses the fit as not having their distribution,
and flag it in `-lrtest` and in the search table.


**Partly fixed on 2026-09-23** (`src/drvec.c`, `operator_roots`, `run_lrtest`).
The report now says it is a constrained point and not an interior maximum, and
diagnoses it: the left null vector `u` of `Θ*(1)` is printed with its weights on
`Ȳ = [∇Y₂ ; W]`, by name, and read by block — in the `∇Y₂` block, that
combination of the common trends looks over-differenced (the rank may be higher,
or the series not I(1)); in the `W` block with an AR root near one as well, an
AR/MA near-cancellation (the rank may be lower); otherwise, mixed. `-lrtest`
marks every LR built on such a fit (`[MA on the boundary at rank r: no known
distribution]`). On the bank: Milan's direction is 70 % `∇London` — the series
the legacy study found nearly over-differenced — and Vienna's is 100 % `W`.
Guarded by `run_tests.sh` 8p.7. **Not done:** the standard errors of the MA are
still printed in the parameter table (the text says they are not defined), and
nothing is done about the boundary itself — which is information about the
specification, not a defect of the program.
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
