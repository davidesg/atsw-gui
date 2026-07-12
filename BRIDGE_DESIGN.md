# drtran — Bridge FUE → drvarma: Design Document

**Revisado: 2026-07-12.** Sustituye a la versión del 2026-07-11, cuyo diagnóstico
central (la "Fase 7: el problema de elf") era **erróneo**. Ver §7.

## Objetivo

drtran es el **puente** entre dos programas que ya funcionan:

- **fue** — identifica y estima modelos **univariantes** (ARIMA + Box–Cox +
  deterministas). Produce un `.pre` por serie.
- **drvarma** — evalúa la **verosimilitud exacta VARMA** de Mauricio (`elf`) y la
  maximiza con BFGS factorizado (`est`/`qnewtopt`).

drtran lee dos `.pre` de fue, construye el *cast* paramétrico
(parámetros → estructura VARMA) y deja que drvarma estime **todos los parámetros
a la vez**: ARMA de ambas series, deterministas, medias, varianzas y —cuando la
haya— la función de transferencia.

### Principio de diseño (no negociable)

> **El `elf` de drvarma se usa TAL CUAL. No se modifica, no se parchea, no se
> caso-especializa.** Es la implementación de referencia de la verosimilitud
> exacta. Cualquier discrepancia con fue es un bug de drtran (del cast o de la
> serie estacionaria), **nunca** de `elf`.

## Criterio de validación (puerta de entrada a todo lo demás)

**Estimación conjunta diagonal ≡ fue por separado.**

Si se especifican dos modelos de fue y se estiman conjuntamente con estructura
**diagonal** (AR/MA diagonales, covarianza diagonal, sin transferencia), la
verosimilitud exacta se factoriza y el resultado **debe coincidir con ejecutar
fue sobre cada serie por separado**. Si no coincide, el cast está mal.

### Caso de prueba canónico

| | Y = `ES_CPI_m10` | X = `WTI_ar1` |
|---|---|---|
| transformación | log, ×100, 1ª diferencia | log, ×100, 1ª diferencia |
| estructura | AR(1) + 11 armónicos + media | AR(1), media fijada en 0 |
| φ (fue) | 0.402839 (SE 0.0623) | 0.299193 (SE 0.0650) |
| μ (fue) | 0.154472 (SE 0.0733) | 0 (fijada) |
| σ² (fue) | 0.062666 | 68.8381 |
| logL (fue) | −7.3917 | −760.0326 |

**Objetivo: logL conjunta ≈ −767.4243** (la suma) y los mismos coeficientes.

### Estado verificado (2026-07-12, con los arreglos de §8 aplicados) ✅

| | drtran conjunto | fue separado |
|---|---|---|
| φ_N | **0.402839** | **0.402839** |
| φ_X | **0.299193** | **0.299193** |
| μ_Y | **0.154472** | **0.154472** |
| μ_X | fijada en 0 | fijada en 0 |
| **logL** | **−767.424341** | **−767.4243** |

La verosimilitud conjunta reproduce la **suma** de las dos univariantes. Las SE de
φ también coinciden. Con los 11 armónicos libres el óptimo y el logL son los
mismos. **El puente funciona: el criterio de validación está cerrado.**

Comprobaciones colaterales: el pass-through `Y = X` recupera **ω₀ = 1.000011**, y
con transferencia contemporánea aparece ω₀ = 0.0149 (t = 8.0), logL = −739.01.

## Arquitectura

```
        Y.pre (fue)            X.pre (fue)
             │                      │
             └──── read_fue_pre ────┘
                        │
                        ▼
        TmY, TsY, DataMatY / TmX, TsX, DataMatX
        (ARMA factorizado, deterministas, datos crudos)
                        │
                        ▼
              shootx(x, &varma)            ← se llama en CADA iteración
              ─────────────────
               1. desempaquetar factores ARMA de x[] → expandir a φ, θ
               2. Box–Cox(datos crudos) · refactor
               3. restar deterministas (Ω/Δ·D[t])
               4. aplicar operador no estacionario (rnsop) → w_Y, w_X
               5. ν(B) = ω(B)/δ(B)·Bᵇ  → transfer[t] = Σ νⱼ·w_X[t−j]
               6. VARMA bivariante:
                    w[·,1] = w_Y − transfer   (ruido N_t)   AR/MA = ARMA de Y
                    w[·,2] = w_X              (entrada)     AR/MA = ARMA de X
                    qq     = diag(var_Y, var_X)
                        │
                        ▼
                elf() → logL exacta  [drvarma, SIN TOCAR]
                        │
                        ▼
                est()/qnewtopt() → BFGS factorizado
```

El acoplamiento entre las dos ecuaciones es **exclusivamente** `transfer[t]`.
Con ω = 0 (o s = −1) el modelo se parte en dos univariantes independientes: ese
es el caso que debe reproducir a fue.

## Vector de parámetros x[]

```
x[]  = ω₀…ω_s              (s+1)    numerador de la transferencia   [s=−1 ⇒ ninguno]
     = δ₁…δ_r              (r)      denominador
     = factores ARMA de Y  (p_Y+q_Y) si !fix_noise
     = factores ARMA de X  (p_X+q_X) si !fix_X
     = Ω_Y deterministas   (d_Y)     si !fix_det_Y
     = Ω_X deterministas   (d_X)     si !fix_det_X
     = μ_Y, μ_X            (2)       si !fix_mu
     = var_Y, var_X        (2)       siempre
```

Los factores ARMA se guardan **sin expandir** (tal como los da fue) y se expanden
a φ/θ dentro de `shootx` en cada iteración, para que la parametrización estimada
sea la misma que la de fue.

## Escala numérica: por qué importa

`qnewtopt/cdgrad` usa un paso de diferencias finitas
`eta^(1/3)·max(|x|, 1.0)` ≈ **6e-6 absoluto**. Si las varianzas son de orden 1e-5,
el paso es mayor que el propio parámetro, lo empuja a negativo, `shootx` devuelve
`ifault=1` y el objetivo se evalúa en la región inválida: el gradiente resultante
es basura.

El **factor de reescalado de fue** (`refactor`, típicamente 100) existe
precisamente para esto: `w = refactor · BoxCox(z)` deja las varianzas en O(10)
—σ²(WTI) = 68.8— exactamente el rango en el que trabajan los modelos legacy de
Mauricio (`m6-1`: `sigma11 = 12.71`). **Hay que multiplicar, no dividir.**

## §7 — Sobre el "problema de elf" (diagnóstico ANTERIOR, INCORRECTO)

La versión previa de este documento afirmaba que la fórmula multivariante de
Mauricio (1995) daba una superficie de verosimilitud distinta para m=2 que para
dos ajustes m=1, incluso en modelos diagonales, "por diseño", y recomendaba la
**Opción B**: partir la verosimilitud en dos llamadas m=1.

**Eso era falso.** La verosimilitud exacta **sí** se factoriza para modelos
diagonales, y el `elf` de drvarma la calcula correctamente. La discrepancia
observada no venía de `elf`, sino de dos bugs de drtran:

1. la serie "estacionaria" **no estaba diferenciada** (§8, F1), de modo que se
   estimaba sobre niveles casi con raíz unitaria;
2. el objetivo estaba **corrompido** por el propio parche que intentaba
   implementar la Opción B (§8, F3).

Con `elf` intacto y las series bien construidas, la estimación conjunta diagonal
reproduce a fue a 6 decimales. **La Opción B queda descartada y el parche
`diag_cov` debe eliminarse.**

## §8 — Los cuatro arreglos (verificados en prototipo)

**F1 — `fue_pre_reader.c`, `CalcNonsOp`.** El operador no estacionario final
descartaba `pol1` (diferencias regulares y estacionales) y copiaba solo `pol3`
(factores de frecuencia fija), además de leer `pol3[1]` fuera de rango. Falta el
producto de convolución:

```c
for (i = 0; i <= ord; i++) op[i] = 0.0;
for (i = 0; i <= pp1; i++)
    for (j = 0; j <= pp; j++)
        if (i + j <= ord) op[i+j] -= pol1[i] * pol3[j];   /* op = -(pol1 * pol3) */
```

Sin esto, **ningún modelo con d>0 o D>0 se diferencia jamás**.

**F2 — `drtran.c`, `apply_univariate_model`.** `refactor` debe **multiplicar** la
serie transformada (`w = refactor·BoxCox(z)`), no dividir el dato crudo.

**F3 — `drvmlest.c`.** Eliminar el parche `(diag_cov ? 1 : varmax.m)` en
`objcfunc` y en la fórmula de `logelf`. Sustituir `m` por `1` en la verosimilitud
concentrada la vuelve impropia: el objetivo decrece monótonamente cuando qq → 0 y
el optimizador empuja las varianzas a cero. **Usar el `elf` de drvarma sin tocar.**

**F4 — `drtran.c`.** Inicializar `var_Y`, `var_X` con la varianza muestral de cada
`w` (antes: 0.01 fijo).

## §8b — Sobre `qq`: NO reparametrizar (decisión ya tomada en drvarma)

`qq` **no es la matriz de covarianzas**: es la covarianza **normalizada**. La real
es `Σ = sigma2 · Q`, donde `sigma2` es el factor de escala que `est` **concentra**.
drvarma lo dice en su `struct` (*"a normalized VARMA model"*, `qq` = *normalized
covariance*) y **imprime las dos matrices** (`drvarma.c:1548-1558`: "Q matrix" y
"Sigma = sigma2 * Q").

drvarma estima las varianzas libres en `x[]` (`drvarma.c:872`: `qq1[i][i] = x[idx++]`)
**y además** concentra `sigma2`. Eso deja la escala global de `qq` como una
**dirección plana** de la verosimilitud: el optimizador fija el *cociente* de
varianzas y `sigma2` absorbe el nivel. drtran hace exactamente lo mismo, así que
**no se desvía de drvarma**.

### Revisión (2026-07-12): la redundancia SÍ había que quitarla — pero en drtran

Este apartado decía "no reparametrizar `qq`". **Era una conclusión precipitada.**
Al calcular el hessiano EXACTO en el óptimo (ver §8c) aparecieron errores estándar
de `Q[2,2]` de **4·10⁵**: el hessiano es **exactamente singular**. La escala global
de Q no está identificada, así que meter `var_Y` **y** `var_X` en `x[]` deja un
parámetro libre de más. El BFGS lo ocultaba (su aproximación nunca llega a ser
singular del todo); el hessiano exacto lo destapa.

La solución es de **drtran, no de drvarma**: se normaliza `Q[1,1] = 1` y se estima
`log(var_X/var_Y)` — un único parámetro, bien escalado y positivo por
construcción. La escala la recupera la `sigma2` concentrada. **No se toca `elf`**;
solo el *cast* de `shootx`, que es competencia de drtran. La verosimilitud no
cambia (logL idéntica a 12 decimales); solo desaparece un parámetro que no estaba
identificado.

Lo que **sigue en pie** del apartado original es la convención (`Q` normalizada,
`Σ = sigma2·Q`, imprimir ambas) y el caveat de drvarma sobre el mal
condicionamiento del pass-through, que se reproduce abajo. Nótese que drvarma
calcula sus SE con el hessiano BFGS, así que **es plausible que su caveat sobre los
SE/Wald cruzados tenga esta misma raíz** — merece comprobarse allí:

> El desbalance de varianzas WTI (σ≈8%) vs IPC (σ≈0.25%) mal-condiciona la
> estimación de la matriz var/cov → los **SE/Wald de los términos cruzados no son
> fiables**. Es un problema **conocido del pass-through**, no del default de
> escala. Los **puntos estimados, elasticidades y previsiones son robustos e
> invariantes**; la direccionalidad se apoya en el punto estimado + economía +
> evidencia de previsión, no en el Wald cruzado.

## §8c — Los errores estándar: hessiano exacto, no BFGS

`est` calculaba la matriz de covarianzas invirtiendo el hessiano que **acumula
BFGS** a lo largo de la trayectoria del optimizador (`raxopt` lo deja en `mtmp`).
Eso sirve para dirigir la búsqueda, pero **no es la curvatura en el óptimo**:
depende del camino recorrido —dos arranques distintos daban SE distintas para el
mismo óptimo— y se degrada justo en las direcciones más planas, que son las de
mayor error estándar.

Se recalcula ahora el hessiano por **diferencias finitas en el óptimo**
(`fdhess` + `choldcp`), que es la alternativa que el propio `drvmlest.c` dejaba
**apuntada y comentada**. Junto con la eliminación de la redundancia de `Q` (§8b),
las SE pasan a cuadrar con el GLS exacto sobre los datos:

| | drtran | fue | GLS exacto / teoría |
|---|---|---|---|
| SE(φ_N) | 0.062204 | 0.062333 | 0.062421 |
| SE(φ_X) | 0.064980 | 0.064979 | 0.065075 |
| SE(μ) | **0.028502** | 0.073304 | **0.028502** |
| SE(det f=1, cos) | 0.068328 | 0.056673 | 0.068328 |
| SE(alternador) | 0.006094 | 0.006092 | 0.006094 |

**Sobre la SE(μ) de fue.** Era el número que no cuadraba (M0.8) y la sospecha
recaía sobre drtran. No: **drtran acierta**. La SE de la media de un AR(1) es
`σ/((1−φ)√n) = 0.0285`, y lo confirman tres cálculos independientes — la fórmula,
una simulación de 20 000 réplicas (0.0284) y el GLS exacto sobre los datos reales
con el modelo completo (0.028502). El **0.073304 de fue es el valor anómalo**, 2.6×
demasiado grande. Curiosamente fue y drtran **sí** coinciden en la SE de μ en el
resto de casos (`ES_CORE_S3`: 0.0509 vs 0.0507; `ES_CORE_S135b`: 0.0470 vs 0.0469):
la discrepancia aparece solo en el modelo con deterministas, donde fue también da
SE distintas para los armónicos de f=1. **Es un problema de fue, no del puente**, y
queda anotado para revisarlo allí.

## §9 — Abierto tras los arreglos

**F5 — medias por serie (hecho).** Cada serie hereda del `.pre` su media y su
condición de libre/fija (`Tm->Imu`): fue marca con un flag las que estima
(`0.154472  1`) y escribe un simple `0` para las que no forman parte del modelo.
Antes un `fix_mu` global las trataba igual. Se eliminó además una rama
`hardcoded` en `fue_pre_reader.c` que, para **cualquier** serie mensual con 11
deterministas, clavaba el φ de ES_CPI (0.402839) en vez de parsear el fichero.

Pendiente:

- **Varianzas reportadas**: se imprime `qq` crudo, pero la varianza real es
  `sigma2 · qq` (el `sigma2` concentrado que devuelve `est`). Salen un 9.5% altas,
  con factor **constante** 0.9132 en ambas series. Es un bug de *reporte*: la
  estimación y el logL son correctos.
- **SE de μ**: 0.0285 frente a 0.0733 de fue. Las SE de φ coinciden bien; la de μ
  no. Revisar el escalado de `cov` en `est` (`cov = 2·pi1·H⁻¹/n`) para m=2.
- **Modo "sin transferencia"**: hoy solo se alcanza con el truco `-s -1`. Debe ser
  un flag propio, porque es el modo de validación.
- **Identificación (preblanqueo + CCF)**: se calculaba sobre las series **sin
  diferenciar**, así que era ciega. Ya con las series correctas sugiere
  (b, r, s) = (0, 0, 0) y aparece el efecto contemporáneo WTI → IPC_ES
  (ω₀ ≈ 0.0149, t ≈ 8), invisible para la CCF antigua. Conviene revalidarla contra
  un caso de relación conocida.
- **Deterministas**: `shootx` solo desempaqueta `Omega[j][0]`. La estructura
  racional `Nomega`/`Ndelta` (intervenciones) no está implementada.
- **Convergencia**: `drtran.c` sigue imprimiendo `"OPTIMIZER CONVERGED after 500
  iterations"` pasando `maxits` e ignorando `ifault`.
- `transfer_weights[5000]` es un buffer de pila sin comprobación contra `n_stat`.

## Horizonte: más allá de la transferencia de una entrada

Los modelos escritos a mano de `../drv-source` (`m6-1/2/3`, decodificados en
`M6-*_DECODED.md`) muestran a dónde debe llegar el cast: operadores factorizados
y racionales, parámetros compartidos y fijos, covarianza estructurada (no
diagonal), diferenciación por serie y regresores deterministas. drtran automatiza
hoy el caso de una entrada; el objetivo a largo plazo es un pequeño DSL para el
mapa parámetros → estructura VARMA.
