# FUE 1.13.1

**Free Univariate Estimation** — exact maximum likelihood estimation of univariate time series models.

Copyright (C) 1995-1996 J.A. Mauricio; 2009-2026 A.B. Treadway & D.E. Guerrero  
License: GNU General Public License v2 or later.

---

## Table of contents

1. [Introduction](#introduction)
2. [System requirements](#system-requirements)
3. [File structure](#file-structure)
4. [Building](#building)
5. [Command-line usage](#command-line-usage)
6. [Workflow with FUF and gtk\_fue](#workflow-with-fuf-and-gtk_fue)
7. [Example](#example)
8. [Integration with external editors](#integration-with-external-editors)
9. [Bug reports](#bug-reports)

---

## Introduction

FUE is a command-line program for estimating univariate time series models
using Exact (Unconditional) Maximum Likelihood (EML). It follows the
identification–estimation–validation cycle proposed by Box & Jenkins (1970),
with later extensions.

Two companion modules complete the univariate analysis toolkit:

- **FUF** — forecasting
- **gtk_fue** — GTK+3 graphical interface for FUE

## System requirements

- C compiler: GCC ≥ 9 (Linux/macOS) or MinGW-w64 (Windows)
- [GNU Scientific Library (GSL)](https://www.gnu.org/software/gsl/) ≥ 2.0
- [Gnuplot](http://www.gnuplot.info/) ≥ 5.0 (for high-resolution plots)
- GNU Make

On Debian/Ubuntu:

```
sudo apt install build-essential libgsl-dev gnuplot
```

On macOS (Homebrew):

```
brew install gsl gnuplot
```

## File structure

```
fue-1.13.1/
├── src/           source files (.c)
├── include/       header files (.h)
├── obj/           compiled objects (created by make)
├── bin/           executable (created by make)
├── Makefile
├── README.md
└── README.es.md
```

## Building

### Linux (default)

```
make
```

The executable is placed in `bin/fue`.

### macOS

```
make
```

If GSL is installed via Homebrew and `pkg-config` cannot find it:

```
PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig make
```

### Windows — cross-compilation from Linux using MXE

```
make CROSS=x86_64-w64-mingw32.static-    # 64-bit
make CROSS=i686-w64-mingw32.static-      # 32-bit
```

The executable is placed in `bin/fue.exe`.

### Other targets

```
make check       # regression tests (tests/README.md)
make clean       # remove object files and executable
make distclean   # remove obj/ and bin/
make install     # copy bin/fue to /usr/local/bin
make uninstall   # remove /usr/local/bin/fue
```

## Command-line usage

```
fue input [eml|aml] [chk|nochk] [-f [horizon]]
```

| Argument           | Description |
|--------------------|-------------|
| `input`            | model-data file name (without `.inp` extension) |
| `eml`\|`aml`       | exact \| approximate maximum likelihood (default: `eml`) |
| `chk`\|`nochk`     | check \| skip MA invertibility check (default: `chk`) |
| `-f [horizon]`     | generate `forecast_input.inp` for FUF; `horizon` is the number of forecast periods (default: 24) |

FUE produces a text output file `input.out` and, when Gnuplot is available,
a PostScript report `input.ps` with high-resolution plots.

Before writing anything, FUE checks the `.inp` file and, if something is
wrong, says what and in which line (for instance, when it is an input file of
FUF or FUG instead of FUE). Exit status:

| status | meaning |
|---|---|
| 0 | results written |
| 1 | command line error, or a file can not be opened |
| 2 | the `.inp` file is not valid: nothing is written |
| 3 | the model can not be estimated from its initial values: the results are written, with a warning in the `.out` that they are not valid |
| 4 | internal run-time error |

Error messages go to the standard error.

## Workflow with FUF and gtk\_fue

The normal workflow is:

1. **Estimate** with FUE:
   ```
   fue mymodel
   ```
   Reads `mymodel.inp` and writes the estimation report `mymodel.out`.

2. **Generate the forecast input file**:
   ```
   fue mymodel -f [horizon]
   ```
   Writes `forecast_mymodel.inp` containing the estimated parameters,
   the forecast horizon (default: 24 periods), and the innovation variance.

3. **Compute the forecasts** with FUF:
   ```
   fuf forecast_mymodel
   ```
   Reads `forecast_mymodel.inp` and produces the forecast report.

From **gtk\_fue**, steps 2 and 3 are triggered automatically by the
*Forecast* button in the toolbar.

## Example

Write a model specification file `mymodel.inp`, then:

```
fue mymodel
fue mymodel -f 12
fuf forecast_mymodel
```

The `src/` directory of the companion **fue-1.13** release contains a sample
`examples/PU.1.inp` file with US CPI data from January 2000 to December 2008.

## Integration with external editors

### KILE

1. `Settings` → `Configure Kile...` → `Build` → `New Tool...`
2. Name: `FUE`; type: `<Custom>`
3. **General** tab: Command = `fue`, Options = `'%S'`
4. **Advanced** tab: Type = `Run in Konsole`; Source Extension = `inp`; Target Extension = `out`
5. **Menu** tab: Add to Build menu = `Other`

## Bug reports

David E. Guerrero — davidesg@ucm.es
