# The eight wheat-price pairs

The bank the seeding is measured on ([VEC_EMBEDDING_PLAN.md](../../docs/VEC_EMBEDDING_PLAN.md) §7),
and the same data as the Johansen comparison ([COMPARISON_JOHANSEN.md](../../docs/COMPARISON_JOHANSEN.md)).
Eight European markets against London, annual wheat prices in log levels, real
and short — the conditions this program is meant for.

| file | market | sample | n |
|---|---|---|---|
| `milan.inp` | Milan | 1701–1812 | 112 |
| `strasbourg.inp` | Strasbourg | 1700–1812 | 113 |
| `utrecht.inp` | Utrecht/Groningen | 1700–1812 | 113 |
| `vienna.inp` | Vienna | 1700–1812 | 113 |
| `aix.inp` | Aix | 1700–1789 | 90 |
| `arevalo.inp` | Arévalo | 1700–1812 | 113 |
| `angers.inp` | Angers | 1700–1789 | 90 |
| `penn.inp` | Pennsylvania | 1720–1812 | 93 |

**Column order is `[Y₂ ; Y₁] = [London ; market]`**, which is what `drvec`
expects, so the cointegrating relation is `W = log p_market + B₂·log p_London`
and the law of one price in its literal form is `B₂ = −1` — the restriction that
working with relative prices *imposes* and that `-fixb2 -1` *tests*.

They are **derived, not primary**: each is the overlap of two `fue` `.inp` files
from the price study, built with

```sh
python3 ~/Dropbox/Cycles/Analysis/drvec_par.py <market>.inp <london>.inp <out>.inp
```

and every source path is written into the header of the file it produced, so the
provenance of each series travels with it. They are committed here because a
baseline measured on files that live outside the repository cannot be repeated
against, which is the whole purpose of taking it ([HOMOLOGATION.md](../../docs/HOMOLOGATION.md) §4b).

The older `data/*LL.inp` pairs are a different vintage — different source series
and the opposite column order — and are kept for the regressions that already
cite them. New work uses these.
