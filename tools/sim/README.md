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

What they established, in order: the cast is exact; the estimator recovers
`Θ = 0` when that is the truth; it recovers the **inherited** structure by itself
on WARMA data (bottom row 1e−3 at `n = 8000`); and `B₂` is superconsistent —
recovered to three or four decimals in every replication — while `(Λ, Θ)` scatter
widely when `Θ` is left free, which is the identification problem `-mawarma`
removes.

```sh
python3 tools/sim/cast_check.py
python3 tools/sim/warma_struct.py
python3 tools/sim/sim_warma.py 1000 0.5 3 /tmp/w.inp && bin/drvec /tmp/w 1 1 1 -case 1 -mawarma
```
