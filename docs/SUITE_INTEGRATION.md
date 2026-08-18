# Joining the ladder: contracts, files, and borrowed code

*What `drvec` shares with `fue`, `art`, `drtran` and `drvarma`, what it takes
from them, and what the suite's own conventions require of it. Established
2026-08-17/18; the working record is `PLAN_BETA.md` §F2.*

---

## 1. The architecture, and where `drvec` sits in it

The suite is a **bilevel optimisation with files as the interface**
(`drtran-python/docs/LADDER_AS_OPTIMISATION.md`):

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
construction»* (`atws/fue/fue/docs/CAST.md` §2).

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
   cannot be alive at once (`CAST.md` §9). Only the numbers are read.
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
| `src/fue_pre_reader.c`, `include/fue_pre_reader.h` | `drtran/src`, `drtran/include` | two: the `drtran.h` include is commented out (not needed), and **one bug fixed** — see below |
| `ObsToDate`, `DateToObs` in `src/fue_bridge.c` | `drtran/src/diagnose.c:128`, `drtran/src/drtran.c:838` | none |
| `expand_ar_factors`, `expand_ma_factors` in `src/fue_bridge.c` | `drtran/src/drtran.c:712`, `:779` | `DRTRAN_PI` → `M_PI` |
| `struct Tusmodel` in `include/main.h` | `drtran/include/main.h` | copied byte for byte |

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
**success**. `fue`'s own C reader has the `else` branch that `drtran`'s lost on
extraction (`fue.c:819-832`), so the defect entered with the extraction and is
not Mauricio's.

Fixed in `drvec`'s copy and verified against `fue`'s parser; filed as **BUG-11**
in `drtran-python/docs/BUGS.md`. It is not fixed in `drtran`, where it is live:
that program does read the series from the `.pre`.

## 6. What `drvec` gives back

Two things the suite asked for and did not have:

* **the parameter-vector layout is machine-checked.** `CAST.md`'s open question
  №5 asks whether the packing order should be verifiable rather than documented;
  `drvec`'s test suite asserts that the walk consumes exactly `npar` in 24
  configurations, and that check has already caught two real defects.
* **a third independent confirmation** of the factorisation gate's constants.
