# art — the ARMA/SARIMA identifier

ART_18.2.1's identification engine, in atsw-gui since 2026-10-04
(`docs/ESTUDIO-identificador.md`).

It **proposes** orders, with their evidence. It does not estimate: estimating
is fue's. The window that shows the proposal is `lib/analisis/an_identifica`
(phase 3).

## Provenance

- **Source.** `git subtree` from the private repo `davidesg/art-identifier-c`,
  tag `ART_18.2.1`.
- **Filtered history** (the 34 commits rewritten to the engine; 32 remain).
  Left out of this public repository:
  - the obsolete training datasets;
  - ART_19's regressors;
  - ART_18's GTK GUI and `old/`.

  The full history stays in `art-identifier-c`.
- **The reference is art-python.** The C follows it, and the 18.2 CHANGELOG
  measures how closely.

## Build

```sh
make          # bin/art, the engine
make dev      # bin/art_cli, ART_18's development CLI (simulation, benchmarks)
make check    # tests/run_tests.sh
make CROSS=x86_64-w64-mingw32.static-   # Windows, with MXE
```

Dependencies:
- **GSL**, through pkg-config;
- **zlib**, because `lib/datos` reads `.xlsx` through `lib/xlsx`.

## Command line

```
art DATA [-s S] [-l] [-d D] [-D D] [-p P] [-q Q] [-P P] [-Q Q]
         [--harmonics auto|on|off] [--no-tests] [--classic]
```

- **`DATA`** is read by `lib/datos`, the house's single data door. It can be
  text, CSV or `.xlsx`.
  - Column 1 is the series.
  - The frequency comes from a `# freq` header or a date column when the file
    declares it; otherwise from `-s`.
- **`-l`, `-d`, `-D`:** log, regular differences, seasonal differences.
- **`-p -q -P -Q`:** upper limits on the orders. The defaults are
  art-python's: p ≤ max(3, s/2), q ≤ 2, P ≤ 1, Q ≤ 1.
- **`--harmonics`:** whether the deterministic harmonics come out of w before
  the ACF/PACF.
  - `auto` removes them when D = 0 and s > 1, as art-python does.
  - `off` is for residuals whose harmonics are already modelled (point E3).
- **`--no-tests`:** skips the seasonal F test and ADF/KPSS.
- **`--classic`:** the classic grid search. The default is the option-B
  shortlist.

| point | how the window calls it |
|---|---|
| E2, the identification graphs | `art DATA -s S [-l] -d D -D D`, with the λ, d and D being viewed |
| E3, a base model's residuals | the window writes the residuals (from the `.out`, `lib/outfile`) to a file and runs `art RES -s S --harmonics off` |

### Output, written next to DATA (`lib/rutas`)

- `DATA_art.out` is the identification for a person to read: the tests, the
  ranked candidates and the messages.
- `DATA_art.cand` is the same for the window (format below).

### Exit status (`lib/engine/engine.h`)

| status | meaning |
|---|---|
| 0 | written |
| 1 | the command line or the data file could not be read |
| 2 | the data cannot be identified (bad options, log of a non-positive value, too few observations); nothing written |
| 3 | no candidate could be scored; the `.out` says why |
| 4 | run-time error |

## The `.cand` format

Text, one record per line. The first word is the key; the rest are values
separated by blanks. Numbers are written with `%.10g`. Lines starting with `#`
are comments. A reader must ignore keys it does not know.

```
# art 18.2.1 -- candidates for the identification window
series NAME
transform log L d D D DD s S
sample n_used N lags K band B
seasonal F f p p detected 0|1 s S      (only if the tests ran)
dummies v1 ... vS                       (100*log units, sample order)
adf stat t p p crit c lags k            (only if the tests ran and d <= 1, D = 0)
kpss stat t p p crit c lags k
acf r1 ... rK                           (empirical, of the series identified on)
pacf r1 ... rK
candidate I p P q Q P PP Q QQ sim S weight W aicc A scored 0|1 proposed 0|1
phi a1 ... ap                           (if p > 0; Box-Jenkins signs)
theta b1 ... bq                         (if q > 0)
Phi A1 ... AP                           (if P > 0)
Theta B1 ... BQ                         (if Q > 0)
tacf r1 ... rK                          (theoretical, if scored)
tpacf r1 ... rK
message TEXT                            (zero or more)
end
```

- **Candidates come in rank order.** Candidate 1 is the `proposed` one.
- **`sim`** is the pattern similarity that ranks them (option B).
- **`weight`** is the Akaike weight of a conditional (CSS) AICc. It is
  comparable between these candidates only.
- **`end`** closes the file. A file without it was cut short.

## Tests

- **`tests/run_tests.sh`** compares the `.out` (without its date line) and the
  `.cand` with `tests/golden/`, byte for byte, and checks the exit statuses.
  `--update` rewrites the goldens. The cases:
  - the E2 series WTI, PS and GY;
  - the frequency taken from the file's header;
  - an E3 residual series;
  - `--no-tests`;
  - three errors.
- **The batteries inherited from ART_18** need Python with numpy and scipy:
  - `tests/regression_identify.py`: the 114-run golden of the 18.2.1
    refactoring;
  - `tests/benchmark_c.py`, `tests/benchmark_seasonal.py`;
  - `tests/compare_seasonal_with_art.py`, which also needs art-python;
  - `tests/api_check.c`, the `art_identify` self-test.

## The library inside

`include/art.h`: `art_identify(x, n, &options, &result)`, array in and result
out, reentrant. The CLI is a thin layer over it.
