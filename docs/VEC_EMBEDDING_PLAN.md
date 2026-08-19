# Embedding the VEC matrix: the specification, the obstacle, and the plan

*How `Π = ΛB′` is added on top of the certified entry gate, why the ladder's
usual bridge does not reach this rung, and what has to be built and measured
before it does.*

*Companion documents: [MODEL.md](MODEL.md) for the model and the parameter
vector; [SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §3–§4 for the ladder's two
contracts and for how far univariate information travels.*

---

## 1. What this is for

The suite's ladder is a construction of **optima from optima**. Each rung is
estimated, certified, and handed upward: the certificate of one rung is the
starting point of the next, and a rung whose base has not been certified cannot
be read, because whatever is wrong below is already inside it.

`drvec` now holds the base. At the diagonal rung — `r = 0` with diagonal `F`,
`Θ` and `Σ` — the exact likelihood factorises, and every run there verifies both
contracts and reports them: the crossing identity, and the optimality
certificate that says whether what arrived was an optimum or a specification.

What comes next is the structure this program exists to estimate. Where the
suite's transfer-function program adds a filter, `drvec` adds the
**error-correction matrix**

```
  Π = Λ B′,     rank r,     B = [ I_r ; B₂ ],     B₂ of size s × r.
```

This document specifies how that matrix is embedded, establishes why the
embedding the ladder uses everywhere else does not work here, and sets out the
order of work. It is written before the work rather than after it, so that the
route is decided once.

## 2. The model, and where the matrix enters

The estimated model is the vector error-correction form

```
  F(L) ∇Yₜ = −Λ (B′Y_{t−1} − E[W])  +  Θ(L) Aₜ,     Aₜ ~ iid N(0, Σ)
```

and estimation is carried out not on `Yₜ` but on Mauricio's (2006)
transformation

```
  Ȳₜ = ( ∇Y_{2t}′ , Wₜ′ )′,      Wₜ = Y_{1t} + B₂′ Y_{2t},
```

which is a **stationary** VARMA whose exact unconditional likelihood the engine
computes. The transformed autoregressive matrices are, with `C̄`, `C̄⁻¹`, `H̄`
and `Λ̄ = [ 0_{M×s} , Λ ]` as in [MODEL.md](MODEL.md) §2,

```
  Φ̄₀ = C̄⁻¹
  Φ̄₁ = C̄⁻¹H̄ − Λ̄ + F₁C̄⁻¹
  Φ̄ᵢ = FᵢC̄⁻¹ − F_{i−1}C̄⁻¹H̄        i = 2 … p−1
  Φ̄_p = −F_{p−1}C̄⁻¹H̄
```

Two properties of this decide everything below.

**The VEC matrix enters only through `Φ̄₁`, and only as `−Λ̄`.** Every other
appearance of `Λ` in the model is through that one term. `B₂`, by contrast,
enters *everywhere*: it defines `C̄`, so it reaches every `Φ̄ᵢ`, the moving-average
matrices `Θ*ᵢ = C̄ΘᵢC̄⁻¹`, the covariance `Σ* = C̄ΣC̄′`, and the data itself through
`Wₜ`. `Λ` is a coefficient; `B₂` is a change of coordinates.

**At `r = 0` the transformation is the identity.** With `s = M` we have `C̄ = I`
and `H̄ = 0`, so `Φ̄ᵢ = Fᵢ`, `Θ*ᵢ = Θᵢ` and `Σ* = Σ`. The gate therefore estimates
`F`, `Θ` and `Σ` **in the same coordinates** the error-correction form uses. This
is what makes the gate a usable base at all: its estimates are not in a
representation that has to be translated before they can be carried upward.

## 3. The obstacle, and it is structural

The ladder's bridge everywhere else is: take the certified optimum, set the new
block to zero, and start there. The new block at zero reproduces the previous
rung exactly, so the starting log-likelihood equals the certified one and the
optimiser can only improve on it. **That bridge does not reach this rung.**

Setting `Λ = 0` does reproduce `Π = 0`, which is the `r = 0` model. But it does
so at a point where the transformed system has a **unit autoregressive root**,
and the likelihood is not defined there: the engine rejects it. Computed
directly from the expressions above, for `M = 2`, `r = 1`, `p = 2`, `B₂ = −0.9`
and `F₁ = diag(0.3, 0.2)`, the smallest root modulus of `Φ̄(z)` is

| ‖Λ‖ | adjusting direction | opposite direction |
|---|---|---|
| 0 | **1.000000** | **1.000000** |
| 1e−6 | 1.000000 | 1.000000 |
| 1e−3 | 1.00177 | 0.99824 |
| 0.05 | 1.10130 | 0.92111 |
| 0.40 | 1.88424 | 0.61937 |

Three things follow, and they are the substance of the problem.

**The base sits exactly on the boundary of the space above it.** Not near it:
at `Λ = 0` the modulus is one to every digit computed. The `r = 0` model is not
an interior point of the `r = 1` parameter space, so the certified optimum
cannot be evaluated there, let alone started from.

**Only one direction leaves the boundary admissibly.** The sign of `Λ` decides
whether the root moves outside the unit circle or inside it, and the second is
non-stationary and equally unusable. A seed must therefore enter the interior
along the adjusting direction, which is a statement about *where* to start, not
merely how far.

**And `B₂` is unidentified at the boundary.** `Π = ΛB′ = 0` for every `B₂` when
`Λ = 0`, so the point the base occupies is also a point of non-identification.
This is the same feature that makes the rank test's distribution non-standard
(Johansen, 1988), and it is why the parametric bootstrap exists in this program
at all; here it reappears as an obstacle to seeding.

### This explains a measurement already on record

[SUITE_INTEGRATION.md](SUITE_INTEGRATION.md) §4 records that seeding the full
univariate block at `r = 1` moves the **starting** log-likelihood *down* by 17
units, and attributes it to three structural reasons: marginalising a VARMA
mixes autoregressive and moving-average structure, `C̄` and `Λ` couple the
equations densely, and `Φ̄_p` is determined rather than free. All three stand.
What the table above adds is the reason underneath them: the natural embedding
is not merely inaccurate, it is **inadmissible**, so any seed has to be pushed
off the boundary by an amount nobody has specified — and an unspecified amount
is what a 17-unit drop looks like from outside.

## 4. What the gate will not tell you

Stated so that the certificate is not over-read. The gate certifies the base:
that the transformation, the differencing, the parameter walk, the deterministic
terms and the scaling all crossed intact, and that the values brought in were
optima. It says nothing whatever about

* whether the rank is one — the gate is fitted **at** `r = 0` by construction;
* whether a cointegrating relation exists;
* whether `B₂` is identified, or well conditioned, at the fit above;
* whether the moving-average operator will rest on its own boundary once the
  rank is raised — measured on eight real pairs, it usually does
  ([HOMOLOGATION.md](HOMOLOGATION.md) §3b).

A certified gate is a necessary condition for reading what is built on it, and
nothing more.

## 5. The rungs, specified

The path from the certified gate to the full model is a sequence of nested
models, each of which is a valid null for the next. Writing them out is the
point of this document, because the sequence — not any single fit — is what
makes the construction reproducible.

| rung | model | added | df of the LR against the rung below |
|---|---|---|---|
| **0** | `r = 0`, `F`, `Θ`, `Σ` all diagonal | — | *the certified base* |
| **1** | `r = 0`, `Σ` free | contemporaneous correlation | `M(M−1)/2` |
| **2** | `r = 0`, `F` and `Θ` free | cross dynamics | `(p−1)M(M−1) + qM(M−1)` |
| **3** | `r = 1` | `Λ`, `B₂` — **the VEC matrix** | non-standard |

Rungs 0 → 1 → 2 are ordinary nested comparisons: the restricted model is an
interior point of the unrestricted one, the log-likelihood cannot fall, and the
statistic is `χ²` with the stated degrees of freedom. Each is a legitimate
optimum-from-optimum bridge, and each is a check that can be run.

Rung 2 → 3 is the one this document is about, and it is not ordinary: by §3 the
null lies on the boundary of the alternative and `B₂` is unidentified under it.
The log-likelihood still cannot fall — `r = 0` is nested in `r = 1` as a model —
but neither the seeding bridge nor the `χ²` reference distribution survives the
crossing. The rank test already handles the second half of that with a
parametric bootstrap; the first half is what has to be built.

## 6. Three ways across, and they are not equally good

**(A) Enter the interior along the adjusting direction.** Carry the gate's
`F̂`, `Θ̂`, `Σ̂` unchanged — they are already in the right coordinates, by §2 —
take `B₂` from the static regression, and set `Λ` to a small step in the
direction that moves the root outward, calibrated so that the transformed system
is admissible with margin. Cheap, uses the certificate, and its only free choice
is the step length, which can be **measured** rather than assumed.

**(B) Profile `Λ` and `B₂` on the certified base.** Hold `F̂`, `Θ̂`, `Σ̂` at the
gate's values and estimate only `Λ` and `B₂`, then release everything. The
conditional problem is small and much better behaved than the joint one, and its
solution is by construction admissible, so it lands in the interior without any
step-length choice. It costs one extra optimisation.

**(C) What is done now.** `init_guess` ignores the gate and runs a conditional
regression for every block from scratch. It works, and it is what every recorded
result rests on, but it makes no use of the certificate and cannot state where
its starting point came from.

**The plan is (B), with (A) as the fallback** if the conditional problem proves
ill-conditioned on the bank. (B) is preferred because it needs no arbitrary
constant: the step off the boundary is chosen by the likelihood rather than by
the implementer. (C) is retained unchanged as the default until (B) is measured
to be at least as good, so that no recorded result moves without evidence.

### What must not be the fix

**Not relaxing the engine's stationarity check.** The unit root at `Λ = 0` is a
property of the model, not of the check. A tolerance that admits it would let
the optimiser evaluate points where the likelihood does not exist, and every
number downstream would be undefined rather than merely wrong.

**Not starting `Λ` at an arbitrary constant.** The table in §3 shows the root's
distance from the circle is a function of `Λ`, `B₂` and `F`; a fixed constant is
a different distance in every dataset, which is precisely the kind of hidden
choice that makes results irreproducible.

**Not abandoning the gate because the bridge is harder here.** The certificate
is what makes rungs 1 and 2 meaningful and what localises any failure below rung
3. That it does not extend to rung 3 unchanged is a fact to be documented, which
is what this document does, not a reason to stop certifying.

## 7. The bank this needs

Measurement, not opinion, decides between (B) and (C), so the bank is specified
before either is built.

* **The eight wheat-price pairs** already used for the Johansen comparison,
  where the answer at rung 3 is known from the recorded runs, so a change in the
  seeding must reproduce them or explain itself.
* **`mink_muskrat`**, the published case, in all three deterministic cases.
* **The synthetic series of known rank**, where rung 3's answer is known by
  construction and a seeding change must not alter it.
* **A case where the gate fails its own contract**, constructed on purpose, to
  confirm that the failure is reported at rung 0 and not first noticed at rung 3.

For each: the starting log-likelihood, the final log-likelihood, the optimiser's
termination, the number of restarts needed to reach the best point, and the
smallest moving-average root at the optimum.

## 8. The order of work

1. **Report the rung ladder.** Emit rungs 0–2 and their LRs from a single run, so
   the sequence is a thing the program produces rather than a thing a user
   assembles. Rung 3 already exists as `-lrtest`.
2. **Measure (C) as it stands** on the whole bank, to have the baseline the
   change is judged against. Nothing is altered at this step.
3. **Build (B)** behind an option, so the default and the recorded results do not
   move.
4. **Measure (B) against (C)** on the same bank, by the same five quantities.
5. **Decide and document**, whichever way the measurement falls, including if it
   falls against (B). A negative result recorded is the route not walked twice.
6. Only then, if (B) wins, make it the default and re-measure the register.

Steps 2 and 4 are the reason for writing this first: without a baseline taken
before the change, the change cannot be evaluated, and the work is repeated.
