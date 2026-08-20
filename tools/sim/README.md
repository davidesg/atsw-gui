# The identification bank: simulate a truth, then ask the program for it

*These are not tests — the suite is `tests/run_tests.sh`. They are the
instruments that answered "is the embedding wrong?", and they are kept because
that question comes back and because the answer is only worth what the
experiment behind it is ([HOMOLOGATION.md](../../docs/HOMOLOGATION.md) §4g,
[DEVELOPMENT_RECORD.md](../../docs/DEVELOPMENT_RECORD.md) §8d).*

| | what it does |
|---|---|
| `cast_check.py` | simulates a VEC with a **general, non-scalar** `Θ` and checks Mauricio's transformation as coded: `Ȳ_t − ΣΦ*_kȲ_{t−k} = A*_t − ΣΘ*_kA*_{t−k}`. Reports 4.2e−15 for the form in `vec_shootx` and 0.56–2.5 for the three plausible alternatives, so it **discriminates** rather than merely agreeing |
| `warma_struct.py` | simulates the WARMA process of BVECM Corollary 2 and checks the structure its VEC error must have: `Θ̃₁ = [[Θ₁, Θ₁B₂′],[0,0]]` at 4.4e−16, against 2.0 for the sign-flipped form. Also pins `Λ` for that DGP |
| `sim_vec.py` | writes a `.inp` from a VEC truth (`B₂ = −0.5`, `Λ = (0.30,0.10)`, `F₁ = 0.2I`, `Θ₁ = θI`) so the program can be asked to recover it |
| `sim_warma.py` | the same for the WARMA truth, whose VEC representation satisfies Corollary 2 |
| `ma_identification.py` | **the instrument that says WHEN the moving average is estimable.** Moves the true MA root against the AR roots of the same system — which the DGP holds fixed — and measures the recovery of `Θ` as a function of the distance between them, i.e. of how close `(Φ, Θ)` is to sharing a common factor. Three modes: `separation` (the main table), `multistart` (the same fits with 20 restarts, which separates "the optimiser misses the optimum" from "the optimum is not the truth"), and `gap` (per-replication distance against the error in `Θ` and against the admissibility statistic `G`, which is what shows `G` does not see this failure mode). See [HOMOLOGATION.md](../../docs/HOMOLOGATION.md) §4q |
| `zroots_check.c` | the operator roots against the house's own root finder: `zroots` (Laguerre) from `Root-1.01`, byte-identical to the copy in `ART_18.1`. With `M = 2` the moving-average polynomial is scalar, so the two methods can be compared directly — and they agree to six figures on the number the whole admissibility diagnosis rests on |

What they established, in order: the cast is exact; the estimator recovers
`Θ = 0` when that is the truth; it recovers the **inherited** structure by itself
on WARMA data (bottom row 1e−3 at `n = 8000`); and `B₂` is superconsistent —
recovered to three or four decimals in every replication — while `(Λ, Θ)` scatter
widely when `Θ` is left free.

**That last clause used to end "which is the identification problem `-mawarma`
removes", and §4q of the register measures that it is not.** The scatter is
governed by the distance between the AR and MA roots, not by the number of free
moving-average parameters: a free `Θ` is recovered to three decimals at `n = 120`
when the two operators are separated, and `-mawarma` on a near-cancelling DGP
does not recover its own `θ` either. What `-mawarma` removes is the boundary, not
the identification problem.

**A note on `sim_warma.py`'s own parameters**, because it bears on how its
results should be read: `phi` is fixed at 0.6 and the canonical run uses
`th = 0.5`, so the `w` block is an ARMA(1,1) whose roots are 1.67 and 2.00 —
a near-common factor by construction. That is why establishing the inherited
structure on it took `n = 8000`. A separated setting (`th = -0.5`, say) is the
one to use when the question is whether the estimator works, rather than how it
behaves under weak identification.

```sh
python3 tools/sim/cast_check.py
python3 tools/sim/warma_struct.py
python3 tools/sim/sim_warma.py 1000 0.5 3 /tmp/w.inp && bin/drvec /tmp/w 1 1 1 -case 1 -mawarma
python3 tools/sim/ma_identification.py separation 15
```
