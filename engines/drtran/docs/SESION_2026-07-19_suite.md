# Sesión 2026-07-18/19 — del bug de previsión al release de la suite en PyPI

Registro de una sesión larga que cruzó **fue, fuf, ART, atsw, pyfug y drtran**.
Cada pieza tiene su documentación de detalle (trackers de bugs, CHANGELOGs,
memoria); esto es el mapa que las une.

## 0. De dónde arrancó

Construyendo el ejemplo de **pass-through IPC↔WTI** (petróleo→inflación) en drtran
con sus informes de previsión, las previsiones de IPC_ES parecían raras. La
investigación (drtran vs fuf-C vs fue-Python vs drvarma) demostró que **drtran era
correcto** (forma-media) y que el bug estaba en fuf/fue.

## 1. El bug de previsión (BUG-0001 de fuf/fue)

`φ(B)[∇Y−μ]=a` ⟹ el nivel se preveía con un drift **doble-contado** (`l·μ`
acumulado sobre las condiciones iniciales), sobrepasando por `μ·φ/(1−φ)`;
**catastrófico con d=0** (explota). Corrección = intercepto `c=μ·(1−Σφ)` dentro de
la recursión del nivel.
- **fuf C**: corregido → **fuf 1.08.2** (`usfo.c`/`fuf.c`), empujado a
  `github-fuf:davidesg/fuf`.
- **fue Python**: corregido en `forecast.py` (0.1.5). Ver `drtran/docs/FUF_FORECAST_BUG.md`.

## 2. Sistemas de bug-tracking

Se creó un tracker in-repo (frontmatter Markdown, sin dependencias, CLI):
- **fue**: `fue.bugs` + `fue-bug` (`bugs/BUG-*.md`).
- **ART**: `art.bugs` + `art-bug` (espejo del de fue).

## 3. Bugs corregidos

**fue** (ver `fue/CHANGELOG.md` y `fue/bugs/`):
- BUG-0001 forecast mean-drift (0.1.5).
- BUG-0002 topes fijos del binding cffi (8 factores/orden 16 → 32/64) (0.1.6).
- BUG-0003 eje X del gráfico de residuos anual (0.1.7).

**ART** (ver `art-python/CHANGELOG.md` y `art-python/bugs/`), destapados en
`Cycles/bugs_art_fue.md`:
- BUG-0001 colapso de μ (reescalado ×100) → `_mu_seed`.
- BUG-0002 sobre-diferenciación d (ADF gobierna sobre KPSS).
- BUG-0003 display-tools persisten `.pre`/`.out`.
- BUG-0004 SE (delta) de damping/periodo en `ar_factorization`.

## 4. drtran — informe de previsión homologado con fuf

`forecast_latex_doc` (`-L`) reescrito para verse **idéntico a fuf** (informes de
ambos van juntos en publicación): título (plantilla `usfo.c`: nombre / *Series
brief description* / Base·Data Source·Forecast Origin), **tabla en cajones**
portada de `usfo.c` (líneas, `$\mathsf{}$`, `(%)`, filas grises, convención L/2
historia + L/2 meses + fines de año), **gráfico** del módulo `forecast_graphic` de
fuf, **dos informes por cara**. Ejemplo en `examples/passthrough/`. Detalle en la
memoria `drtran-forecast-report-fuf-style`.

## 5. Diagnóstico y arreglo de la instalación (el objetivo de fondo)

En Windows `pip install atsw` fallaba: fue está en PyPI **solo como sdist**, así
que compilaba la extensión C → necesita compilador **y GSL**. Causa: fue **no
tenía CI de wheels ejecutándose** (repo sin remoto). Se validó y arregló el
`wheels.yml` (cibuildwheel):
- Windows: `$VCPKG_INSTALLATION_ROOT` + `/` (no `{env:}`).
- macOS: `_discover_gsl_dirs` (gsl-config/Homebrew) + `MACOSX_DEPLOYMENT_TARGET`
  fijado; Intel-macOS fuera (runners escasos).
- Linux: `before-all` portable (dnf/apk) para manylinux **y** musllinux.
- Test por wheel = `test_smoke.py` (la batería golden es platform-sensible: R.4).

## 6. Release de la suite en PyPI

Los 4 paquetes al mismo nivel (repo GitHub + trusted publishing OIDC). Publicado:
- **fue 0.1.7** (26 ficheros: 24 wheels binarios cp310–313 × Win/macOS-arm64/Linux
  glibc+musl x86_64+aarch64 + pure + sdist, GSL empaquetado).
- **art-tseries 0.1.2**, **atsw 1.0.3**; **pyfug 2.0.0** ya estaba.

Detalle del cómo-publicar en la memoria `atsw-suite-pypi-release`. Resultado:
`pip install atsw` en Windows/Mac-ARM/Linux baja binario, **sin compilador ni GSL**.

## Punteros
- fue: `atws/fue/fue` — `CHANGELOG.md`, `bugs/`, repo `davidesg/fue-python`.
- ART/atsw: `ART/art-python` — `CHANGELOG.md`, `bugs/`, repo `davidesg/art-python`.
- pyfug: `atws/fug/pyfug` — repo `davidesg/pyfug`.
- fuf C: `atws/fuf/fuf-1.08.1` (1.08.2) — repo `davidesg/fuf`.
- drtran: este repo — `forecast_latex_doc`, `examples/passthrough/`, `docs/FUF_FORECAST_BUG.md`.
- Cycles: `Dropbox/Cycles/bugs_art_fue.md` (origen de los bugs de ART/fue).
