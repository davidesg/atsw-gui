# fue — los errores estándar no son fiables

> **Status (2026-09-27): fixed.** `est` now takes the covariance from
> Mauricio's `fdhess` at the optimum, guarded: a boundary optimum, or a
> Hessian that is not positive definite, falls back to BFGS and says so.
> `-hessian bfgs` gives the old behaviour. On this document's case
> (ES_CPI_m10) fue now gives SE(μ) = 0.028502, the exact GLS, from any
> start. See fue BUG-0015 (fixed in 0.1.17), `tests/README.md` for the
> changed goldens, and the family-wide study, drvarma-python
> `docs/STUDY-standard-errors.md`. The text below is the original study.

**Fecha:** 2026-07-12
**Origen:** hallado al homologar `drtran` (puente fue → drvarma) contra fue.
**Afecta a:** la inferencia (SE, t, p-valores). **No** a las estimaciones puntuales
ni a la log-verosimilitud, que son correctas.

## El síntoma

fue produce **errores estándar distintos en ejecuciones distintas del mismo
modelo**, con las **mismas** estimaciones puntuales. Hay dos salidas del caso
`ES_CPI_m10` (AR(1) + 11 armónicos + media, ∇ ln y) que lo muestran:

| | SE(μ) | SE(φ) |
|---|---|---|
| run A (`drtran/examples/work/ES_CPI_m10.out`) | **0.073304** | 0.062333 |
| run B (`SF_MEG/empirical/cases/ES_CPI/work/ES_CPI_m10.out`) | **0.028316** | 0.061577 |

Puntos idénticos en ambas: μ = 0.154472, φ = 0.402839.

Y en los coeficientes de los deterministas la diferencia es mucho mayor. Comparado
con el **GLS exacto** sobre los propios datos (modelo completo: μ + 11
deterministas diferenciados + AR(1), con la matriz de covarianza AR(1) exacta):

| determinista | fue run A | fue run B | GLS exacto |
|---|---|---|---|
| 1 (cos f=1) | 0.0955 | 0.0567 | **0.0683** |
| 2 (sin f=1) | 0.0962 | 0.0562 | **0.0683** |
| 3 (cos f=2) | **0.0800** | 0.0271 | **0.0277** |
| 4 (sin f=2) | **0.0820** | 0.0265 | **0.0277** |
| 5 (cos f=3) | **0.0722** | 0.0163 | **0.0158** |
| 6 (sin f=3) | **0.0652** | 0.0159 | **0.0158** |
| 7…11 | ≈ ok | ≈ ok | |

La run A se equivoca por un factor de **4 a 5** en los deterministas 3 a 6. No es un
sesgo sistemático (la run A los infla, la run B infla unos y desinfla otros): es
**ruido**.

## La causa

`drvmlest.c` calcula la matriz de covarianzas invirtiendo el hessiano que
**acumula BFGS a lo largo de la trayectoria del optimizador** (`raxopt` lo deja en
`mtmp`):

```c
raxopt( objcfunc, &pi1, npar, par, mtmp, maxits, nrits, grtol, sptol );

/* This is an alternative way of computing the second derivative matrix:     */
/* fdhess( objcfunc, npar, par, pi1, macheps, mtmp );                        */
/* choldcp( mtmp, npar, &pi2, &pi3, ifault );                                */

/* [3]: Sample estimation of the variance-covariance matrix:                 */
for ( i = 1; i <= npar; i++ ) { ... cholsol( mtmp, npar, vtmp ); ... }
```

Ese hessiano **sirve para dirigir la búsqueda, pero no es la curvatura en el
óptimo**. BFGS **acumula** curvatura: cada iteración actualiza el hessiano en **una
sola dirección**, la del paso que acaba de dar. De ahí dos consecuencias:

- **depende del camino recorrido** → dos arranques distintos dan SE distintas para
  el **mismo** óptimo. Es lo que separa la run A de la run B;
- **solo tiene curvatura en las direcciones que ha recorrido**. Las demás conservan
  el valor **inicial** del hessiano (una identidad escalada), que no tiene relación
  con el problema.

### El disparador: arrancar en el óptimo

El `.pre` que escribe fue lleva las estimaciones **ya convergidas**. Si se
reestima desde ahí —que es el flujo natural—, el optimizador **apenas da pasos**, y
el hessiano se queda cerca de su valor inicial.

La run A lo dice sin ambigüedad: **`CONVERGENCE OBTAINED AFTER 9 ITERATIONS`, con
13 parámetros**. Con menos pasos que parámetros, el hessiano BFGS es
*estructuralmente incapaz* de tener curvatura en todas las direcciones: al menos
cuatro quedan con el valor inicial arbitrario. Las SE disparatadas de esa corrida
(det3 = 0.0800 cuando la verdad es 0.0277) no son mala suerte, son aritmética.

**El flujo de trabajo habitual dispara el problema de forma sistemática.**

### Aviso: perturbar el arranque NO es la solución

Es tentador concluir que basta con mover un poco las preestimaciones del `.inp`
para que el optimizador "trabaje" y el hessiano salga bien. **Medido: no funciona.**

Con drtran (mismo `drvmlest.c`), moviendo el AR de 0.4028 a 0.10 y con 18
iteraciones de trabajo real, la SE del armónico f=1 sigue en **0.0797** frente al
**0.0683** verdadero — un **17% de error**, prácticamente el mismo que arrancando en
el óptimo (0.0816):

| | BFGS: arranque en el óptimo | BFGS: perturbado | fdhess (exacto) | GLS |
|---|---|---|---|---|
| SE(μ) | 0.028723 | 0.028542 | **0.028502** | 0.028502 |
| SE(det f=1) | 0.081578 | 0.079703 | **0.068328** | 0.068328 |
| SE(det f=2) | 0.027704 | 0.028004 | **0.027692** | 0.027692 |

Aunque el optimizador trabaje, los pasos **se encogen** al acercarse al óptimo, y
las direcciones más planas —los armónicos de frecuencia baja— nunca llegan a
explorarse. Por eso el error se concentra ahí y no en los armónicos de frecuencia
alta, que salen bien en todas las columnas.

Perturbar el arranque sirve para **detectar** el problema (si las SE cambian, no son
de fiar), **no para resolverlo**.

## El arreglo

Está **escrito y comentado en el propio `drvmlest.c`**: recalcular el hessiano por
**diferencias finitas en el óptimo**. Basta descomentar:

```c
   fdhess( objcfunc, npar, par, pi1, macheps, mtmp );
   choldcp( mtmp, npar, &pi2, &pi3, ifault );
```

**Verificado en drtran**, que arrastraba el mismo `drvmlest.c`: con el hessiano
exacto, las SE pasan a coincidir con el GLS exacto **al sexto decimal** en todos
los parámetros (μ, φ y los 11 deterministas), y dejan de depender del arranque.

### Una salvedad que en fue NO aplica

En drtran, activar `fdhess` destapó además un hessiano **singular**: su cast metía
dos varianzas libres en `x[]` **y** concentraba `sigma2`, dejando la escala de `Q`
sin identificar (se veían SE de 4·10⁵). Hubo que quitar el parámetro redundante.

**fue no tiene ese problema**: es univariante y **no mete ninguna varianza en
`x[]`** (en `ES_CPI_m10`, `Parameters: 13` = 11 deterministas + φ + μ; la `sigma2`
se concentra entera). Su hessiano exacto no debería ser singular, así que
descomentar las dos líneas debería bastar.

## Por qué importa

Las SE de fue son las que deciden qué armónicos y qué medias entran en el modelo, y
las que sostienen la inferencia del estudio de inflación (SF_MEG). Con la run A, un
armónico con SE inflada 5× parece no significativo cuando sí lo es.

Conviene además **revisar las salidas ya publicadas**: la discrepancia entre las dos
copias de `ES_CPI_m10.out` indica que hay resultados en circulación calculados con
hessianos malos.

## Cómo reproducirlo

El GLS exacto de referencia (μ + deterministas **diferenciados** + AR(1)), sobre los
datos del propio `.pre`:

```python
import numpy as np
# w = 100*dlog(y) ; regresores = diferencia de los armonicos (fue resta los
# deterministas del NIVEL y luego diferencia, asi que el regresor efectivo es su
# primera diferencia)
X   = np.column_stack([np.ones(n), np.diff(D, axis=0)])
idx = np.abs(np.subtract.outer(np.arange(n), np.arange(n)))
V   = phi**idx / (1 - phi**2)                    # correlacion AR(1) exacta
cov = sigma2 * np.linalg.inv(X.T @ np.linalg.inv(V) @ X)
se  = np.sqrt(np.diag(cov))
```

Y la SE de la media de un AR(1) tiene forma cerrada, que confirma el resultado:

```
SE(mu) = sigma / ((1 - phi) * sqrt(n)) = 0.0286
```

Tres cálculos independientes coinciden — la fórmula (0.02859), una simulación de
20 000 réplicas (0.02845) y el GLS exacto (0.028502) —, y con ellos coincide drtran
tras el arreglo (0.028502). La run B de fue (0.028316) anda cerca; la run A
(0.073304) está 2.6× fuera.

## Nota sobre una lectura equivocada

Al detectar esto por primera vez se concluyó, mirando **solo la run A**, que "la SE
de μ de fue está mal". **Esa conclusión era incorrecta**: fue no calcula mal la SE
de forma sistemática, sino de forma **inestable**. Unas veces acierta y otras no,
según el camino que haya seguido el optimizador. El problema no es un error de
fórmula, es la fuente del hessiano — y por eso la solución es una sola línea.

---

## Estado: el arreglo se conoce, y NO se aplica todavía (2026-08-01)

Trabajando en drtran quedó establecido **cuál es el arreglo** y **por qué no es
una línea**.

**Cuál es.** `drvmlest.c:112` tiene la llamada comentada desde el original de
1995:

```c
/* fdhess( objcfunc, npar, par, pi1, macheps, mtmp );  */
/* choldcp( mtmp, npar, &pi2, &pi3, ifault );          */
```

Descomentarla sustituye la matriz que el BFGS acumula por el camino —que es el
origen de este defecto: depende del arranque, y ni siquiera se construye cuando
la búsqueda empieza en el óptimo— por el hessiano recalculado **en** el óptimo.
`fdhess` está definida y completa en `qnewtopt.c`; no hay nada que escribir.

**Que funciona, está comprobado.** drtran C es el único de la familia que la
tiene activa, y su puerto a Python la reproduce: los 17 errores estándar del
caso canónico coinciden con el binario con discrepancia relativa máxima de
2.8e-04. Y se verificó lo que aquí falla: perturbando el arranque, los s.e. no
se mueven.

**Por qué no se aplica aquí.** fue es de **uso general**. Le van a dar modelos
mal especificados que no convergen, y en un punto que no es el óptimo el
hessiano por diferencias finitas no tiene por qué ser definido positivo —
medido en el sistema m6 de drtran: en las semillas, 2 de 55 autovalores son
≤ 0; en el óptimo real, los 55 son positivos. `choldcp` es la Cholesky
**modificada**: parchearía esos pivotes y publicaría una columna de errores
estándar de aspecto impecable. La matriz del BFGS no puede fallar así, porque es
definida positiva por construcción; lo que pierde es que no es la curvatura en
el óptimo.

Ese intercambio —siempre responde aunque a veces mal, frente a responde bien o
no responde— es probablemente la razón por la que Mauricio la dejó comentada.
Se midieron y **se descartaron** las dos explicaciones alternativas: el coste
(18 % de lo que ya gasta la búsqueda, y bajando con el tamaño del problema) y el
truncamiento de `xitol`. El análisis completo está en
`drtran-python/docs/PORTE.md` §9.

**Lo que hace falta antes de tocarlo:** una sesión propia con barrido empírico
sobre la batería —cuántos de los casos reales caen en un punto donde el hessiano
no es definido positivo— y la decisión de qué hacer entonces: rechazar con aviso
(lo que hace el puerto de drtran, `ifault=2`), caer de vuelta a la matriz del
BFGS, o dejarlo a elección del usuario. Cambiar el defecto de todos los usuarios
de fue no es un descomentar prestado de un programa con precondiciones mucho más
fuertes.
