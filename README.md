# gtk_fue 0.9

**GTK+3 graphical interface for FUE and FUF** — univariate time-series modeling and forecasting.

Copyright (C) 2026 A.B. Treadway & D.E. Guerrero  
License: GNU General Public License v2 or later.

---

## Table of contents

1. [Introduction](#introduction)
2. [System requirements](#system-requirements)
3. [Building](#building)
4. [Running](#running)
5. [Interface overview](#interface-overview)
6. [Workflow](#workflow)
7. [File structure](#file-structure)
8. [Bug reports](#bug-reports)

---

## Introduction

gtk_fue is a GTK+3 graphical front-end for the univariate time-series toolkit:

- **FUE** — Free Univariate Estimation: identification and exact maximum likelihood
  estimation of SARIMA models with Box-Cox transformations, deterministic
  interventions, and stochastic AR/MA operators.
- **FUF** — Free Univariate Forecasting: probabilistic forecasting from an
  estimated FUE model.

gtk_fue lets you build, edit, and save model specification files (`.inp`),
run FUE and FUF, and view the output reports, all from a single window.
It is cross-platform: Linux, macOS, and Windows.

## System requirements

**Build-time:**

- C compiler: GCC ≥ 9
- GTK+ 3.0 and GLib 2.0 development headers
- GNU Make

**Runtime:**

- `fue` — FUE estimation engine (must be on `PATH`)
- `fuf` — FUF forecasting engine (must be on `PATH`)
- A PDF viewer (used by View Output and Forecast)

On Debian/Ubuntu:

```
sudo apt install build-essential libgtk-3-dev
```

## Building

```
make
```

The executable is placed in `bin/fue_gui`.

### Cross-compile to Windows (static) from Linux using [MXE](https://mxe.cc)

```
make CROSS=x86_64-w64-mingw32.static-   # 64-bit
make CROSS=i686-w64-mingw32.static-     # 32-bit
```

Pre-compiled `fue.exe` and `fuf.exe` binaries for Windows are kept in `engine/`.
The `create_installer.sh` script builds a self-contained Windows installer using
these binaries.

## Running

```
./bin/fue_gui
```

On Windows, launch `fue_gui.exe` from the installation directory or the Start
Menu shortcut created by the installer.

## Interface overview

The main window has a toolbar and a tabbed notebook.

### Toolbar

| Button | Action |
|--------|--------|
| New | Clear all fields and start a new model |
| Open | Load an existing `.inp` or `.pre` file |
| Save | Write the current model specification to a `.inp` file |
| Run | Save the `.inp` file and run FUE |
| View Output | Open the estimation PDF report |
| Forecast | Run `fue -f` then `fuf` and open the forecast PDF |
| Quit | Exit the application |

### Tabs

| Tab | Contents |
|-----|----------|
| **Data Input** | Series name, frequency, observation range, data file, workspace directory, rescaling factor |
| **Box-Cox & Differences** | λ and m parameters, regular differencing, seasonal differencing, individual frequency factors |
| **Deterministic Component** | Intervention variables (level shifts, pulses, seasonal dummies) with optional AR/MA transfer dynamics |
| **Stochastic Component** | Regular and annual AR/MA operators; fixed-frequency AR/MA operators |
| **Console** | Displays the current `.inp` model specification; supports in-place editing and saving |
| **Forecast** | Load and edit a forecast input file, set the horizon, run FUF, open the output PDF |

## Workflow

1. **New model**: Click *New*, fill in *Data Input* (series name, frequency,
   data file, workspace, observation range).

2. **Specify the model**: Set transformations in *Box-Cox & Differences*, add
   intervention variables in *Deterministic Component*, and add AR/MA operators
   in *Stochastic Component*.

3. **Estimate**: Click *Save* to write the `.inp` file, then click *Run* to
   invoke FUE. The *Console* tab shows the model specification as FUE reads it.

4. **View results**: Click *View Output* to open the estimation PDF report.

5. **Forecast**: Click *Forecast* to run `fue -f` (which generates
   `forecast_{model}.inp`) and then `fuf` automatically. The forecast PDF opens
   when FUF finishes. Alternatively, use the *Forecast* tab to load any forecast
   input file manually and set a custom horizon.

## File structure

```
gtk_fue.09/
├── src/                          C source files
├── include/                      Header files
├── engine/                       Pre-compiled FUE and FUF binaries (Windows)
├── data/                         Sample input files and data series
├── tests/                        Tests (make check)
├── obj/                          Compiled object files (created by make)
├── bin/                          Compiled executable (created by make)
├── Makefile
├── build_windows_static_fue.sh   MXE cross-compilation helper
├── create_installer.sh           Windows installer builder
├── README.md
└── README.es.md
```

## Running the engines

**Run FUE** does not block the interface any more: the engine runs in the
background and the bar at the bottom shows the iteration the optimizer is
on and the value of the objective function, as they happen. When it is done
the bar says how it ended, taking it from the `****` lines the optimizer
writes in the `.out`:

```
fue finished.  CONVERGENCE OBTAINED AFTER 20 ITERATIONS [GRADIENT NORM = 0.0000];
GRADIENT STOPPING CRITERIUM SATISFIED TO WITHIN TOLERANCE LIMITS
```

and if it did not, what the engine said and why. The engines are run
directly, never through a shell.

This needs **FUE 1.14 or later and FUF 1.09 or later**: the optimizer writes
the iterations all in one line, with no newline between them, so before
those versions the buffer of the pipe kept them until the end.

## Graph window

The PDF that FUE and FUF write is drawn by the engines themselves
(`src/fugdraw.c`), not by an external program, so the GUI can read the page
back and draw it with Cairo: **View PDF** opens a window with the page, the
page buttons, *Save As* (PDF, EPS, PNG or SVG) and *Print*. A PDF that was
not drawn by the engines -- one compiled by pdflatex, for instance -- is
handed to the viewer of the system, as before.

| | |
|---|---|
| `+` `-` or **Ctrl+wheel** | larger, smaller (around the pointer) |
| `0` / `1` | fit the page in the window / one point, one pixel |
| **drag with button 1** | move the page when it does not fit |
| **hold button 3** | the magnifier: the bit under the pointer, drawn again and larger, in a window that follows it |
| `Page Up` `Page Down` `Home` `End` | pages |
| `Ctrl+S` `Ctrl+P` `Esc` | save as, print, close |

The magnifier draws from the content stream, not from what is on the
screen, so it has the resolution of the page however small the window is:
that is what lets one look closely at an incident in the data.

`src/preview.c` is the same file as `gui/src/preview.c` of FUG, ported to
GTK+3; what each program has to supply is in `include/previewhost.h`. The
two copies are meant to become one library.

## Bug reports

David E. Guerrero — davidesg@ucm.es
