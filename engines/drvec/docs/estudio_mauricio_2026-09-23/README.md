# Estudio de Mauricio (2006) frente a `drvec` — material de apoyo (2026-09-23)

El estudio consolidado es [../ESTUDIO_MAURICIO_2026-09-23.md](../ESTUDIO_MAURICIO_2026-09-23.md).
Aquí están los tres estudios de detalle, con sus demostraciones y los scripts que
las comprueban numéricamente:

| carpeta | qué cubre |
|---|---|
| `M1/` | el artículo, ec. (1)–(21), redemostrado paso a paso, y su correspondencia con `vec_shootx`, `build_y2_levels`, los casos deterministas y la muestra |
| `M2/` | el AddOn (A1, A2 y el modelo (A.14)), las Tablas 2–5 de *mink–muskrat*, el test de rango (Remark 5, Yap–Reinsel, las tablas de valores críticos) y la previsión en niveles |
| `M3/` | la verificación, resultado a resultado, de `docs/DEMOSTRACIONES.md` y de que el código hace lo que cada teorema dice |

## Cómo se reproducen

Ningún script modifica el repositorio. Algunos necesitan una **copia instrumentada**
de `drvec`, que hay que construir aparte:

- `M1/check_code.py` usa `M1/harness.c`: un `#include "drvec.c"` con `main`
  renombrado, que llama a `build_y2_levels`, `vec_shootx` y `elf` sobre un `x[]`
  dado. Se compila en una copia de `src/` (`drvec_copy/`, junto a los scripts):
  `cp -r ../../src drvec_copy/src && cp harness.c drvec_copy/src/` y el mismo
  `gcc` que el `Makefile`, sustituyendo `drvec.c` por `harness.c`.
- `M2/table4.py` y `M2/check_general.py` llaman a `../drvec_copy/bin/drvec`, un
  `drvec` con `M2/instrumentation.patch` aplicado (tres ganchos: `DRVEC_X`
  inyecta el vector de parámetros en `-eval`, `DRVEC_DUMP`/`DRVEC_FDUMP` vuelcan
  el sistema transformado). El parche no cambia ningún cálculo.
- Los demás scripts son Python puro (numpy/scipy) o llaman a `bin/drvec` tal cual.
