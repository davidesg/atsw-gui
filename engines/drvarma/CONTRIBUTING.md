# Contributing to drvarma

Thanks for your interest in improving drvarma. This is a small scientific C
codebase; the notes below keep contributions consistent and license-clean.

## Getting started

```sh
sudo apt install build-essential pkg-config libgsl-dev libglib2.0-dev
make            # builds bin/drvarma
```

Read [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) for the architecture and
[`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) for behaviour/output.

## License of contributions

drvarma is **GPL v2 or later** (see `COPYING`). By contributing you agree your
changes are released under the same license.

**Do not add Numerical Recipes code** (or any code under an incompatible/
non-redistributable license). The project was deliberately made NR-free: use the
**GNU Scientific Library (GSL)** for linear algebra, or clean, independently
written routines. New source files should carry the short GPL header used across
the tree.

## Coding conventions

- C99, compile cleanly with `-Wall`.
- Numeric type is `real` (= `double`); use it for new numeric code.
- **1-based indexing** with the `nlatools.c` allocators (`vector(1,n)`,
  `matrix(1,m,1,n)`, `tensor`, and the matching `free_*`). Access matrices as
  `m[i][j]`; do not assume a tight contiguous layout.
- Keep public signatures stable; if you change one, update `include/main.h` and
  all callers.
- Keep GSL behind the 1-based wrappers of `nlatools.c` (`gui/seasonal_detection.c`
  is the one other file that includes it).

## Before opening a pull request

1. `make` succeeds with no new warnings.
2. **The batteries pass.** The rules for a change in an engine (the study
   first, the C before the port, which batteries to run) are in the
   repository's `docs/MOTORES.md`. In short:

   ```sh
   sh tests/run_tests.sh        # banco (.inp path), escalera (the ladder), the output reader
   sh tests/rutas/rutas.sh      # the routes: minutes; before and after any change in the engine
   make -C ../.. check          # every battery of the repository: others drive this engine
   ```

   The `.out` files kept in `data/` come from older binaries and are **not** a
   reference. A reference (`tests/*/ref`) is regenerated only when the change
   of output is deliberate, and the pull request says which one moved and why.
3. **License scan** is clean:

   ```sh
   grep -rn "Numerical Recipes\|Copr\." src include gui   # must be empty
   ```

4. Describe the change and its numerical impact (if any) in the PR.

## Commit messages

Use a concise summary line and a body explaining the *why* and any numerical
effect. Group logically distinct changes into separate commits.
