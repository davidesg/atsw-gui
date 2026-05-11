# gtk_fue

GTK3 graphical user interface for the **FUE/FUF** time-series modeling programs.

**FUE** (Free Univariate Estimation) fits ARIMA-type models with Box-Cox
transformations, deterministic interventions, and stochastic AR/MA operators.
**FUF** (Free Univariate Forecast) generates forecasts from an estimated model.

## Features

- **Data Input** — series name, frequency, observation range, data file, workspace, rescaling factor
- **Box-Cox & Differences** — λ and m parameters, regular and seasonal differencing, individual frequency factors
- **Deterministic Component** — intervention variables (level shifts, pulses, seasonal dummies) with AR/MA dynamics
- **Stochastic Component** — AR/MA operators (regular and annual) and fixed-frequency operators
- **Console** — displays the `.inp` model specification; supports in-place editing and saving
- **Forecast** — loads and edits forecast input files, runs FUF, and opens the output PDF
- Cross-platform: Linux, macOS, and Windows (via MXE static cross-compilation)

## Requirements

- GTK+ 3.0
- GLib 2.0
- GCC

On Debian/Ubuntu:

```bash
sudo apt install libgtk-3-dev build-essential
```

## Build

```bash
make
```

The binary is placed in `bin/fue_gui`.

### Cross-compile to Windows (static) from Linux using [MXE](https://mxe.cc)

```bash
make CROSS=x86_64-w64-mingw32.static-   # 64-bit
make CROSS=i686-w64-mingw32.static-     # 32-bit
```

## Usage

```bash
./bin/fue_gui
```

1. Fill in the **Data Input** tab (series name, frequency, data file, workspace, input name)
2. Set transformations in **Box-Cox & Differences**
3. Add interventions in **Deterministic Component**
4. Add AR/MA operators in **Stochastic Component**
5. Click **Save** to write the `.inp` file, then **Run** to estimate the model with FUE
6. View results with **View Output** (opens the PDF)
7. Use the **Forecast** tab to generate and run forecasts with FUF

## Project structure

```
src/          C source files
include/      Header files
engine/       Pre-compiled FUE and FUF engine binaries (Windows)
data/         Sample input files and data
Makefile
build_windows_static_fue.sh   MXE cross-compilation helper
create_installer.sh           Windows installer builder
```

## License

© David E. Guerrero. All rights reserved.
