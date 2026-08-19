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

Both differences are of the order of the format's own rounding: **a `.pre`
stores its coefficients with `%.6f`**, and that is what bounds how sharp the
certificate can be. Both contracts are checks in the test suite.

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

### The allocators that are still the pre-cleanup ones

`tensor()` was the only member of the family `drvec` could reach with a negative
lower bound, and it is the only one changed. The others remain as they were, and
the position is stated rather than left implicit:

| routine | state in `drvec` | exposure |
|---|---|---|
| `tensor`, `free_tensor` | **corrected**, identical to the shared copy | was live: `gamwa` with `q ≥ 2` |
| `vector`, `ivector` | pre-fix: no base offset | **latent** — no call site in `drvec` uses a negative lower bound. The suite corrected these for robustness after one of them bit the transfer-function program, whose identification step allocates `vector(−nlags, nlags)` |
| `matrix`, `imatrix` | pre-fix, and **deliberately** so across the suite | none; the offset variant uses an incompatible row layout, so the two are not interchangeable, and the univariate program left them alone for exactly that reason |

Adopting the offset form for `vector` and `ivector` would align `drvec` with the
transfer-function and VARMA copies, but it is not a free change: it makes every
allocation and release depend on agreeing bounds, where the present form ignores
them. It is recorded here as a decision to be taken deliberately, with the memory
checks covering it, rather than made in passing.

The consequence for results already reported is stated in
[HOMOLOGATION.md](HOMOLOGATION.md) §3b: specification searches conducted before
the correction could not reach `q ≥ 2`, and were bounded by a defect rather than
by a modelling decision.

## 6. What `drvec` gives back

Two things the suite asked for and did not have:

* **the parameter-vector layout is machine-checked.** The suite's documentation
  of the translation step leaves open whether the packing order should be
  verifiable rather than merely documented;
  `drvec`'s test suite asserts that the walk consumes exactly `npar` in 24
  configurations, and that check has already caught two real defects.
* **a third independent confirmation** of the factorisation gate's constants.
