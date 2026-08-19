# The test suite: what it protects, measured

*`tests/run_tests.sh`, run by `make test`. 112 checks, and 122 with the opt-in
memory block. The claim that a suite
"protects" something is worth nothing unless it is measured, so it is measured
by mutation: real defects are put back and the failures counted.*

---

## 1. Nine kinds of check, in increasing order of value

| | what it establishes |
|---|---|
| **1. structural** | the parameter walk consumes exactly `npar` in 24 configurations. `calc_nparametrs`, `init_guess`, `vec_shootx` and the printer are four independent walks of the same vector; when they disagree the program reads past the end of `x[]` and reports numbers for a model nobody specified |
| **2. invariants** | properties that hold whatever the data says, so they need no external reference and cannot go stale: both printers must agree on `B₂`; logL must be monotone in `r` (rank `r` is nested in `r+1`); a restricted fit cannot beat the free one; \|Σ̂\| must agree between the two layouts of the same model |
| **3. the gate** | with `r = 0` and diagonal structure the exact likelihood factorises, so the joint logL must equal the sum of the univariate ones. This is the cast's oracle: when it breaks the fault is in `vec_shootx` or the seeding, **never** in `elf`. The tolerance is the `ξ` truncation and not a fixed number, and both halves are checked: with `q > 0` the identity is claimed only to `xitol`, and with `q = 0`, where there is nothing to truncate, it must hold to 1e−9 — the strict half is what keeps the loose half honest ([HOMOLOGATION.md](HOMOLOGATION.md) §1b) |
| **3b. the ladder** | `-rungs` emits rungs 0–2 at `r = 0` and their LRs, so the suite can check what a user assembling them by hand gets wrong: the degrees of freedom against the closed form, the monotonicity of logL, the sign of every LR, and — the one that matters — that the top rung reproduces the plain `r = 0` fit to the digit, which says the ladder re-estimates the same models |
| **4. golden** | current log-likelihoods, to catch unintended drift. **Regression baselines, not correct answers** — most stop on termcode 3 |
| **5. the bridge** | what `drvec` writes must be readable by `fue`, and what it reads must land in the right place: pure ASCII, every section present, the reader round-tripped against the file itself, and the two ladder contracts — which the program now claims itself, with the optimum/specification verdict checked in **both** directions. Plus what the header defect taught: a file that is *not* of the format must be **refused with a sentence**, because the reader used to count header lines and a file with a different header length was read one line off, silently ([DEVELOPMENT_RECORD.md](DEVELOPMENT_RECORD.md) §8) |
| **6. interpretation** | `α = Aψ`: `-weakex` and the equivalent `-alpha` file must agree exactly; the restricted fit cannot beat the free one; a rank-deficient `A` is refused before estimating; `Σ = P D P′` must reconstruct `Σ`; multi-start must be monotone; the convergence note must agree with the optimiser's banner; and the normalisation alarm is checked in **both** directions |
| **7. known truth** | the rank test on data generated to have a known rank — every other check of `-lrtest` compares against another program's answer, these compare against the truth |
| **8. roots and the boundary** | the moduli of the estimated AR and MA roots, and the invertibility boundary the likelihood enforces. Two things: that a model with `q ≥ 2` estimates at all, which is the regression for the allocation defect that made it abort ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5); and that the unit-root alarm and the `-fdhess` boundary diagnosis fire where the optimum binds and stay silent where it does not — an alarm with no negative case is not an alarm |
| **9. memory** | valgrind over the main paths; opt-in, see §3b |

## 2. What it actually catches

Measured by mutation against the current source — mutating an older source is
not a valid measurement, because then the baselines fail for the wrong reason:

| mutation | failures raised |
|---|---|
| sign of `Λ` in `Φ̄₁` (the core of the transformation) | **14** |
| the output ignores `-diagma` | 4 |
| the `.inp` writer drops the annual-difference section | 2 |
| the reader defect on annual files, reinstated | 2 |
| `B₂` read transposed in `vec_shootx` (the **estimator**) | 1 |
| the LDL′ cross term negated, on M = 3 | 1 (**0** on M = 2) |
| the normalisation alarm disabled | 1 |
| the residuals not computed for the diagnosis | 1 |
| the portmanteau p-value returned to the inherited expression | 1 (**0** on two other cases) |
| the Σ positive-definiteness check removed | **0** ← not caught |
| `B₂` fill transposed in the **printer** | **0** ← not caught |

Two rows carry the same lesson about **where** a test has to run. The
portmanteau p-value defect needs `df ≥ 30` *and* a statistic below its mean —
i.e. a model that fits — so on the bivariate case (28 df) and on UK (72 df but
Q = 148) the mutation raises nothing; it only bites on a synthetic three-series
case with `Q(126) = 110`. And the LDL′ row: the reconstruction check runs on M = 3 and
not on M = 2 because with M = 2 the inner loop never executes — there is no third
variable for the cross term to accumulate over — so the same mutation is
invisible. A test written on the smallest case would have been decoration.

### The two zeros, stated so a green suite is not over-read

* **The PD check is insurance with no test route.** No case in the bank drives Σ
  non-positive-definite, so nothing exercises it. It guards a region the
  optimiser does not currently reach.
* **A printer-only transposition is invisible**, because since the fix that gave
  both printers one shared copy, a transposition in the printer alone cannot
  change what the estimator did. What does guard the order that matters is the
  golden value for M = 5, r = 2: with `s = 3` and `r = 2` a transposed read
  changes the likelihood. That case had to be added — with `s = 1`, which is what
  every earlier fixture had, transposing is a no-op.

The reader defect on annual files deserves comment: through the **estimation** path
it scored zero, because seeding reads only the MA block, which sits earlier in
the file than the section the bug corrupts. It is caught by a dedicated harness,
`tests/pre_probe.c`, which is the only thing that reads the series and the
`refactor` at all.

## 3. Two checks that cannot go stale, and why that matters

Most of the suite compares against numbers. Two do not:

* **the zero-seed identity** — a seed with `Θ = 0` must reproduce the cold start
  *bit for bit*. This is what says the seeding plumbing is right — reader, factor
  expansion, coordinate route, parameter layout — independently of whether
  seeding helps. (It does not, above the diagonal rung; that is a separate
  finding and this check is what allowed the two to be told apart.)
* **the reader round-trip** — `tests/pre_probe.c` prints what `read_fue_pre`
  sees, and the suite compares it against the values pulled out of the same
  `.pre` with `awk`. There is no golden number to maintain.

The general principle, learned the hard way here: **a golden value is only valid
for the exact input it was measured on.** The UK fixture is written with `%.10f`;
a copy written with `%.8f` gives a different log-likelihood in the sixth decimal.
That is why the fixtures are generated inside the test rather than committed —
except the `.pre` files, which must come from `fue` and so are committed with
their provenance recorded in `tests/fixtures/README.md`.

## 3b. Memory, opt-in

```sh
VALGRIND=1 make test
```

Disabled by default so that the suite is deterministic on any machine. It is
not merely precautionary:
the first time it was run it found **two real defects**, and a third surfaced later.

* The multi-start block re-allocated the VARMA structure while the first
  allocation was still live — 1080 bytes orphaned per run, and nothing else
  would have noticed.
* `main` had four exits — the normal one, `-lrtest`, `-writeinp/-writeres` and
  `-eval` — and only the normal one freed the file-name buffers. Four exits and
  one cleanup is how a harmless leak becomes a real one.

A third was found later, and only because the allocators were aligned with the
suite's shared copy ([SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §5): the four
buffers `load_seed_pre` allocates had no deallocator. The leak had been there all
along and `valgrind` had not reported it, because the previous allocator returned
the base of the block and a pointer to the base looks like a live reference. The
offset form does not, so 32 bytes surfaced on the first run after the change.
That is worth stating as a property of the tooling rather than an anecdote: a
memory check is only as sharp as the allocator underneath it.

It also verifies something that cannot be verified any other way:
`free_fue_pre`, the deallocator written for the vendored `.pre` reader. That
reader has none anywhere in the suite, so the deallocator is
new code written against someone else's allocator — and valgrind catches
over-freeing as well as leaking, which is the only real check on it.

## 4. Running it

```sh
make test                      # build and run
tests/run_tests.sh -v          # verbose: one line per check
DRVEC=path/to/mutant tests/run_tests.sh    # check the suite bites
RUN_TIMEOUT=60 tests/run_tests.sh          # per-run timeout, default 30 s
```

The per-run timeout is functional. An ill-conditioned surface can send the optimiser
into a region where each likelihood evaluation is very slow, and that has
happened for real: a reparameterisation tried during development turned a
0.04-second fit into one that had not finished in 90 seconds. Without a timeout
the suite would hang instead of reporting it.
