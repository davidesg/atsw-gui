# Contributing to drvarma

Thanks for your interest in improving drvarma. This is a small scientific C
codebase; the notes below keep contributions consistent and license-clean.

## Getting started

```sh
sudo apt install build-essential pkg-config libgsl-dev libglib2.0-dev libgtk-3-dev
make            # builds bin/drvarma and bin/drvarma_gui
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
- Confine GSL includes to `nlatools.c` and keep 1-based wrappers there.

## Before opening a pull request

1. `make` succeeds with no new warnings.
2. **Regression check**: re-run the reference cases and confirm results are
   unchanged for unrelated changes —

   ```sh
   ./bin/drvarma data/models_group1/IPC3 3 0 -mean -deseason auto
   ```

   The objective, parameters and diagnostics in `IPC3.out` should match the
   committed output (to full precision) unless your change is meant to alter them.
3. **License scan** is clean:

   ```sh
   grep -rn "Numerical Recipes\|Copr\." src include gui   # must be empty
   ```

4. Describe the change and its numerical impact (if any) in the PR.

## Commit messages

Use a concise summary line and a body explaining the *why* and any numerical
effect. Group logically distinct changes into separate commits.
