# drvec — VEC model EML estimation (Mauricio 2006)

Exact maximum likelihood estimation of **partially nonstationary vector ARMA models**
via the transformation described in:

> Mauricio, J.A. (2006). "Exact maximum likelihood estimation of partially
> nonstationary vector ARMA models." *Computational Statistics & Data Analysis*,
> 50, 3644–3662.

## Architecture

```
drvec/
├── src/
│   ├── drvec.c          ← VEC frontend (vec_shootx, init_guess, main)
│   ├── elfvarma.c       ← Mauricio's AS 311 exact log-likelihood (untouched)
│   ├── drvmlest.c       ← estimation driver (untouched)
│   ├── qnewtopt.c       ← factored BFGS quasi-Newton (untouched)
│   └── nlatools.c       ← linear algebra / GSL wrappers (untouched)
├── include/
│   └── main.h           ← Tvarma struct, function prototypes
├── literature/
│   ├── Mauricio.pdf     ← Mauricio (2006) paper
│   └── *-AddOn.pdf      ← Additional material (AddOn)
├── data/                ← .inp data files
├── docs/
│   └── LEGACY_NOTES.md  ← solutions rescued from legacy drv_project
├── Makefile
└── README.md
```

## The Mauricio transformation

The core insight of Mauricio (2006) is that a partially nonstationary VARMA in
VEC form can be transformed into a **standard stationary VARMA** on:

```
Ybar_t = (nabla Y_{2t}',  W_t')'
```

where:
- `s = M - r` variables are not normalised (Y_{2t} in levels)
- `r` cointegrating relations: W_t = Y_{1t} + B_2' Y_{2t}

The transformation matrices are:

```
Cbar = [ 0_{s x r}    I_s      ]    Cbar^{-1} = [ -B_2'    I_r ]
      [    I_r       B_2'     ]                 [ I_s       0   ]

Hbar = [ 0_{s x s}   0_{s x r} ]
      [ 0_{r x s}      I_r     ]
```

such that `Cbar * nabla Y_t = Ybar_t - Hbar * Ybar_{t-1}`.

The resulting VARMA on Ybar_t is **stationary** and estimable by standard EML.

## Usage

```
drvec file p q r [-mean] [-case 1|2|3] [-diagar] [-diagma] [-diagcov] [-m 1|2]
```

| Argument | Description |
|----------|-------------|
| `file`   | Data file name (without `.inp`) |
| `p`      | AR order of stationary VARMA on Ȳ_t |
| `q`      | MA order |
| `r`      | Cointegration rank (0 < r < M) |

| Option | Description |
|--------|-------------|
| `-mean` | Include mean in Ȳ_t |
| `-case 1\|2\|3` | Deterministic specification (Remark 6) |
| `-diagar` / `-diagma` / `-diagcov` | Diagonal restrictions |
| `-m 1\|2` | Exact (1) or approximate (2) ML |

## Input format (.inp)

```
* comments
12                              ← frequency (1=A, 4=Q, 12=M)
2 124 1 1965                    ← M nobs start_sub start_year
housing_starts houses_sold      ← M series names (Y_{1t} then Y_{2t})
1.0 0 0                         ← lambda d D (usually 1.0 0 0 for levels)
<data: nobs rows, M columns>
```

The first `s = M - r` columns contain **nabla Y_{2t}** (pre-differenced).
The remaining `r` columns contain **Y_{1t} in levels**.

## Deterministic cases (Mauricio 2006, Remark 6)

| Case | E[nabla Y_2] | E[W] | Description |
|------|-------------|------|-------------|
| 1    | = 0         | = 0  | No drift, zero-mean equilibrium |
| 2    | = 0         | ≠ 0  | No drift, non-zero equilibrium mean (-mean) |
| 3    | ≠ 0         | ≠ 0  | Restricted drift, non-zero equilibrium (-mean) |

## Building

```sh
make          # build bin/drvec
make clean    # remove objects
make rebuild  # clean + build
```

Requires: **GSL** (`libgsl-dev`), gcc.

## License

GPL v2 or later. Based on code by J.A. Mauricio (1995–2006),
A.B. Treadway, and D.E. Guerrero.
