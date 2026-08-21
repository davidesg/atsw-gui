# What `drvec` shares with the suite, and where it does not

*Written 2026-08-21, at the close of P3 of [PLAN_PRODUCCION.md](PLAN_PRODUCCION.md).
It exists because until now the only way to find out whether a shared file had
drifted was to diff it, and nobody diffs on a Tuesday. The numbers here are
`gcc -fpreprocessed`-stripped comparisons — **code lines, comments excluded** —
and the command that produces them is at the end.*

---

## 1. Where `drvec` sits

`drvec` is, at the level of the suite, a **subproduct of `drvarma` and `fue`**:
it runs the first one's engine on the second one's cast, which is exactly what
`drtran` does. It is not an independent program that happens to resemble them.

```
   fue            .pre files, univariate ARMAX by exact ML
    |             (the cast: fue_pre_reader.c reads them)
    v
   drvarma        elf (AS 311), the optimiser, the linear algebra
    |             (the engine: elfvarma.c, drvmlest.c, qnewtopt.c, nlatools.c)
    +---> drtran   transfer functions:  fue's cast + drvarma's engine
    +---> drvec    partially nonstationary VEC: the same two
```

## 2. The engine: four files, and `drvec` carries them unchanged

Code lines differing from the canonical copy,
`drvarma_source/drvarma_v.04.1/src`:

| file | `drvec` | `drtran` | `drvarma`'s Python package |
|---|---|---|---|
| `elfvarma.c` | **0** | 0 | — |
| `drvmlest.c` | **0** | 10 | — |
| `qnewtopt.c` | **0** | 53 | — |
| `nlatools.c` | **0** | **0** | **0** |

**`drvec` diverges from the canonical engine in zero lines of code**, on all
four files, since 2026-08-21. What it carries beyond that is comments recording
where each piece came from.

`drtran`'s two divergences are its own and deliberate: it exposes `opt_iters`
and `opt_termcode` as globals so its report can say how the optimiser stopped,
and it keeps the older `report()`. `drvec` needs the same thing — the
convergence note — and gets it inside `report()`, in `drvarma`'s version. Two
solutions to one problem, and this is the sentence that says there are two.

### How `nlatools.c` got to zero

It was the last one, and the only copy in the family that diverged in code.
`vector`, `ivector` and `tensor` had been aligned in August 2026 after the
`q ≥ 2` heap corruption; `matrix`, `imatrix` and their releases were left on
`fue`'s variant because the shared one was recorded as "not interchangeable".

Measured, that reason held for `fue` and not for `drvec`: with the shared
variant every functional check passed and no golden value moved. What it did do
was **reveal five leaks the previous layout concealed** — a pointer returned at
the base of its block looks reachable to `valgrind`, an offset one does not. All
five are closed. See [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5.

## 3. The cast: `drvec` was ahead, and gave it back

`fue_pre_reader.c` is a copy of `drtran`'s — `fue`'s own reader lives inside
`src/fue.c` and is not factored out, so `drtran`'s is the only one in C that can
be reused. Reusing it on an **annual** bank, which `drtran` never had, produced
three defects and a missing deallocator. All four are now in the original
(`drtran d5bf5be`), and BUG-11 and BUG-12 are closed in the suite's register.

What still differs between the two copies is structure, not behaviour:
`drtran`'s includes `drtran.h` and carries `free_fue_pre` in the reader itself,
while `drvec`'s keeps it in `fue_bridge.c`.

## 4. Where `drvec` deliberately does something else

| | what | why |
|---|---|---|
| p-values | `gsl_cdf_chisq_Q` instead of the house `chisq()` | it is the upper tail directly, with no `1 - …`. This began as a workaround for BUG-13; since that is fixed in all four copies it is belt-and-braces, and the battery watches `chisq()` anyway ([8h]) |
| the MA default | the class of Theorem 6, not a free `Θ` | [SPECIFICATION_PLAN.md](SPECIFICATION_PLAN.md) §10 |
| forecasting to levels | its own map, not `forecast_level_variances` | the siblings integrate with a scalar `Δ(B)` series by series; here only the `Y₂` block is differenced and `Y₁ = W − B₂′Y₂` couples the two ([FORECAST.md](FORECAST.md) §4) |

## 5. The conventions, which have to be borrowed too

The provenance sections of this documentation list what **code** was taken from
where. That turned out not to be enough. `-interv` subtracted a `.pre`'s
deterministic terms **in the units of the `.pre`** rather than of the data,
which is only right when `refactor = 1` — and the suite's norm is
`refactor = 100`, because the optimiser converges there and hangs at raw scale.
The suite's own written discipline already said *«never hardcode the rescaling
factor; read `model.refactor` — the suite has three logged bugs from getting
this wrong»*. This was the fourth ([HOMOLOGATION.md](HOMOLOGATION.md) §4u).

So: **borrowing the engine means borrowing its conventions**, and this page is
the reminder.

## 6. What `drvec` gave back

| | |
|---|---|
| **BUG-11** | the annual `.pre` read one row off, silently, returning success |
| **BUG-12** | `read_fue_pre` had no deallocator: 20 leaked blocks per run, now 2 (and none from the reader) |
| **BUG-13** | `chisq()` inverted its tail for `df ≥ 30`, so every p-value came out complemented and clean residuals were declared non-white. Fixed in all four copies |

None of the three was found by looking. They came out because a fourth program
reused this code on a bank the original never had, under `valgrind`, with a
suite that had to be green.

---

## Reproducing the table

```sh
strip() { gcc -fpreprocessed -dD -E -P "$1" | sed 's/[[:space:]]\+/ /g;s/^ //;s/ $//' | grep -v '^$'; }
A=../drvarma_source/drvarma_v.04.1/src
for f in elfvarma.c drvmlest.c qnewtopt.c nlatools.c; do
    strip $A/$f > /tmp/a; strip src/$f > /tmp/b
    echo "$f $(diff /tmp/a /tmp/b | grep -c '^[<>]')"
done
```

If a row stops being zero, either someone fixed something here and not there, or
there and not here. Both are worth a look, and neither announces itself.
