# Eliminación de Numerical Recipes en drvarma (para publicar como GPL)

**Objetivo:** quitar todo el código de Numerical Recipes (NR), cuya licencia impide
la redistribución libre, para poder publicar drvarma bajo **GPL**.

**Plantilla:** `fue` ya hizo esta migración para el caso univariante.
- `atws/fue/fue-1.13`   = última versión **con** NR.
- `atws/fue/fue-1.13.1` = primera versión **sin** NR / **GPL** (tiene `COPYING`).
  Enfoque de fue-1.13.1: GSL para álgebra pesada (autovalores) + reimplementación
  limpia de utilidades (allocators, LU, Cholesky, `pythag`) + eliminación de SVD/NR.

## Footprint NR en drvarma

Todo el código NR está **concentrado en `src/nlatools.c`** (18 bloques con el comentario
`(C) Copr. 1986-92 Numerical Recipes Software`). Ningún otro fichero contiene código NR;
los demás (`elfvarma.c`, `drvmlest.c`, `qnewtopt.c`, `multshea.c`, `diagnose.c`, …) solo
**llaman** a esas funciones. drvarma **ya enlaza GSL** y ya tiene `choldcp` limpio.

## Funciones NR y plan por categoría

| Función(es) NR | Uso en drvarma | Acción |
|----------------|----------------|--------|
| `eigenql, tred2, balanc, elmhes` | **ninguno** (código muerto) | **Borrar** |
| `eigenqr` | `elfvarma.c` (1 llamada) | Reemplazar por **`gsl_eigenqr`** (portar de fue-1.13.1) y actualizar la llamada |
| `svdcp, svsol` | `diagnose.c` (resolver con Σ) | Reemplazar por envoltorio **GSL SVD** (`gsl_linalg_SV_decomp`/`SV_solve`, disponibles) conservando la firma, o reescribir el bloque |
| `ludcp, lusol` | `elfvarma.c, multshea.c, drvarma.c` | Portar versiones **limpias** de fue-1.13.1 |
| `nrerror`, allocators (`vector, ivector, matrix, imatrix, tensor, free_*`) | todo el código | Portar versiones **limpias** de fue-1.13.1 (mismo API, sin copyright NR) |
| `choldcp, cholfor, cholbak, cholsol` | varios | **Ya limpios** en drvarma (no llevan copyright NR) — verificar idénticos a fue-1.13.1 |
| `pythag` | interno | Portar limpio (trivial) |
| `chisq, tdist, normal_cdf, calcnu, matrix_transpose` | propios de drvarma | **Conservar** (no son NR) |

## Estrategia recomendada

1. **Portar `nlatools.c` limpio desde fue-1.13.1** como base (allocators, `nrerror`,
   `ludcp/lusol`, `choldcp/cholfor/cholbak/cholsol`, `pythag`, `gsl_eigenqr`, utilidades de
   string, `Easter`, `cmacheps`, `rmax/rmin`). Tipos compatibles (`real`=double).
2. **Añadir encima las funciones propias de drvarma** que fue no tiene (`chisq, tdist,
   normal_cdf, calcnu, matrix_transpose`) — confirmar que no son NR.
3. **Borrar** las rutinas de autovalores NR muertas y el bloque SVD NR.
4. **Sustituir** la llamada `eigenqr(...)` en `elfvarma.c` por `gsl_eigenqr(...)`.
5. **Sustituir** `svdcp/svsol` en `diagnose.c` por un envoltorio GSL SVD (o reescritura).
6. Ajustar `include/nlatools.h`/`main.h` (prototipos: quitar eigen/SVD NR, añadir
   `gsl_eigenqr`, renombrar `round_local`↔`iround` si conviene).
7. Añadir **`COPYING` (GPL)** y cabeceras de licencia (como fue-1.13.1).
8. **Recompilar y validar numéricamente**: estimaciones/previsiones idénticas en los casos
   de referencia (IPC3 trivariante, pass-through, univariantes) — test de regresión.

## Esfuerzo y riesgo

- **Esfuerzo:** moderado y mayormente **mecánico** (portar el mismo fichero ya limpio de
  fue-1.13.1). Lo no-trivial: el envoltorio **GSL SVD** para `diagnose.c` (fue-1.13.1 no
  tiene SVD) y el cambio de la llamada de autovalores.
- **Riesgo:** bajo. fue-1.13.1 es una reimplementación **probada** de las mismas rutinas;
  LU/Cholesky/allocators son drop-in. Validar que el SVD GSL reproduce el resultado del
  diagnóstico, y que las estimaciones no cambian (mismo óptimo).

## Verificación de licencia tras la migración
- `grep -rl "Numerical Recipes\|Copr." src include` → debe quedar **vacío**.
- Compilar con `-Wall`, correr los casos de `MODELS_RESULTS.md` y comparar salidas.
