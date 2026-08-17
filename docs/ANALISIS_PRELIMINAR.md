# Análisis preliminar de `drvec`

*Continúa `ENCUADRE_ESTUDIO.md`, que era el reconocimiento. Esto es el estudio:
pasos 2 y 3 de su orden de lectura (el layout contra los Remarks, y `drvec.c`
completo contra el artículo). Todo número de aquí está medido, no estimado; el
método está indicado en cada caso para que se pueda repetir.*

**Fechas:** diagnóstico 2026-08-16; correcciones 2026-08-16 y 2026-08-17.

**Estado del código.** El diagnóstico se hizo sobre copias instrumentadas en un
directorio temporal, sin tocar el repo. Después se aplicaron siete correcciones,
**todas en `src/drvec.c`**:

| | qué | efecto |
|---|---|---|
| §3.5 | sembrar la covarianza en correlación; reportar `Sigma = sigma2 * Q` | siembra + salida |
| **§3.6** | **quitar el parámetro redundante de Σ (Σ₁₁ = 1, `npar` −1)** | **identificación** |
| §4.1 | la salida respeta `-diagar/-diagma/-diagcov` y deja de leer fuera de `x[]` | salida |
| §4.2 | B₂ se imprimía traspuesta | salida |
| §4.3 | leer Y₂ **en niveles** (ahora el defecto); niveles calculados una sola vez | datos |
| §4.4 | `-lrtest`: test secuencial de rango, antes inexistente | funcionalidad |
| §4.5 | nombres de fichero dimensionados por la ruta, no a 80 bytes | robustez |

**El motor no se toca**: `elfvarma.c`, `drvmlest.c`, `qnewtopt.c`, `nlatools.c` y
`main.h` están byte a byte como estaban. En particular **`elf()` no se toca
nunca** — es la AS 311 publicada y refereada, y está validada aquí a 2·10⁻⁸
(§5.4).

Dos de ellas mueven resultados a propósito: **§3.6**, que quita un parámetro que
no estaba identificado, y **§4.3**, cuyo layout en niveles pasó a ser el defecto
el 2026-08-17 (el antiguo queda tras `-differenced`). Las otras cinco no tocan
las estimaciones. Valgrind no reporta accesos inválidos ni en la estimación
normal ni en `-lrtest`.

---

## 0. Resumen

La transformación de Mauricio (2006) **está bien implementada**: `vec_shootx`
reproduce las ecuaciones (10)–(18) término a término, y lo he corroborado
numéricamente. La pregunta de diseño que dejó abierta el encuadre queda
**contestada y confirmada**.

Lo que fallaba no era la transformación sino la capa que la rodea. El defecto
principal era de **identificación**: el vector de parámetros llevaba un grado de
libertad que la verosimilitud no puede ver, una cresta exactamente plana por la
que el optimizador se deslizaba hasta que el line search fallaba. **Corregido**
(§3.5 y §3.6): `npar` baja en uno, la log-verosimilitud sube en casi todas las
configuraciones, |Σ̂| se acerca mucho a la banda de aceptación, y el `-lrtest`
pasa a dar log-verosimilitudes monótonas en r en vez de estadísticos negativos
imposibles. Lo que **queda**: el optimizador sigue parando en termcode 3, y el
punto exacto donde para varía según por dónde se entre — por eso |Σ̂| oscila
entre 0.00237 y 0.00257 en configuraciones equivalentes, y el criterio de §5.11
todavía no se cumple de forma fiable.

Queda **un asunto abierto y relevante**: `drvec` no reproduce la verosimilitud
publicada por Mauricio (6.5 frente a 15.13). He descartado que la causa esté en
`drvec` —motor validado a 2·10⁻⁸, transformación verificada, datos verificados
contra la fuente canónica— y he localizado el hueco: **está entero en el Σ̂ que
publica el artículo**, cuyo determinante es 1.35 veces menor que el que producen
sus propios parámetros sobre estos datos. Curiosamente la columna **CML** del
artículo sí encaja; la que no encaja es la **EML**. Todo en §5.

Chan & Wallis (1978) y el AddOn cierran la parte que importa. Con un modelo más
rico (AR de orden 4, ML exacta) la literatura obtiene |Σ̂| = 0.00246 y nuestro
VARMA(2,1) llega a 0.002311 — en línea; la columna EML del artículo está un 27 %
por debajo de todo lo publicado (§5.8). Y el AddOn enseña que **esa reducción de
Σ̂ bajo EML es sistemática en el artículo**: factor 2 en su otro ejemplo, el
Census Housing (§5.9). Así que no es un desliz de una tabla.

**Ya hay criterio de aceptación** —sobre |Σ̂|, no sobre la log-verosimilitud— y
el trabajo deja de estar bloqueado, aunque el *porqué* de esa columna sigue sin
poder cerrarse con el material disponible. Ambas cosas, en §5.11.

---

## 1. La transformación es fiel al artículo

Verificado término a término contra el PDF (`literature/Mauricio.pdf`, pp.
3649–3651):

| Artículo | Código | ¿Coincide? |
|---|---|---|
| C̄, ec. (11) | `drvec.c:411-413` | ✔ |
| C̄⁻¹, ec. (14) | `drvec.c:415-417` | ✔ |
| H̄, ec. (13) | `drvec.c:419` | ✔ |
| Ā = [0, Λ], ec. (10) | `drvec.c:421` | ✔ |
| Φ̄₀ = C̄⁻¹, ec. (16) | `drvec.c:428` | ✔ |
| Φ̄₁ = C̄⁻¹H̄ − Ā + F₁C̄⁻¹ | `drvec.c:430-443` | ✔ |
| Φ̄ᵢ = FᵢC̄⁻¹ − Fᵢ₋₁C̄⁻¹H̄ | `drvec.c:445-457` | ✔ |
| Φ̄_p = −F_{p−1}C̄⁻¹H̄ | `drvec.c:459-468` | ✔ |
| Φ*ᵢ = Φ̄₀⁻¹Φ̄ᵢ = C̄Φ̄ᵢ, ec. (18) | `drvec.c:472-478` | ✔ |
| Θ*ᵢ = Φ̄₀⁻¹ΘᵢΦ̄₀ = C̄ΘᵢC̄⁻¹ | `drvec.c:480-489` | ✔ |
| Var[A*] = C̄ΣC̄' | `drvec.c:491-500` | ✔ |
| Ȳ_t = [∇Y₂; W], ec. (17) | `drvec.c:527-538` | ✔ |

**Remark 6 (los tres casos) también está bien planteado.** El caso 3 exige
deriva restringida `E[∇Y₁] = −B₂'E[∇Y₂]`; el código no impone nada, y no hace
falta: en la parametrización sobre Ȳ_t la restricción **es automática**, porque
`E[∇W] = 0` en el proceso estacionario W y `∇W = ∇Y₁ + B₂'∇Y₂`. Es exactamente
lo que el artículo llama «a trivial and easily interpretable implementation».

**Confirmación en forma cerrada (AddOn §A1).** El material adicional
(`518-2013-11-11-JAM106-AddOn.pdf`) desarrolla explícitamente el caso bivariante
M=2, P=1, p=1 —justo la configuración de mink–muskrat— y da las matrices y el
resultado cerrados, ecs. (A.5)–(A.6):

```
Ȳ_t = [∇Y_t2 ; Y_t1 + β₂Y_t2]     C̄ = [0 1; 1 β₂]     H̄ = [0 0; 0 1]
Ā = [0 λ₁; 0 λ₂]                  Φ*₁ = H̄ − C̄Ā = [0  −λ₂ ; 0  1−λ₁−λ₂β₂]
```

Replicando paso a paso el álgebra de `drvec.c` (líneas 411–421, 430–434 y
472–478) con λ₁=0.8392, λ₂=0.5881, β₂=−0.2042:

```
AddOn (A.6)     [[0, -0.5881], [0,  0.28089002]]
drvec           [[0, -0.5881], [0,  0.28089002]]      diferencia máx. = 0.000e+00
```

C̄, H̄ y Ā coinciden además una a una con (A.5). Es una comprobación
**independiente y en forma cerrada** de la implementación, en el mismo caso que
usa el banco de pruebas.

**Corroboración numérica del layout** (no sólo lectura). Evaluando la
verosimilitud en los valores publicados de la Tabla 4 y probando las ocho
variantes de orientación:

```
F sin trasponer, Θ sin trasponer   ->  logL  5.1820   <- la del código
F traspuesta                       ->  logL  5.3587
Θ traspuesta                       ->  logL -21.8557
permutando el orden de series      ->  ifault=3 (no estacionario) en las 4
```

La permutación del orden de series produce un modelo **no estacionario**, lo que
confirma que el orden interno `∇Y = [∇Y₁; ∇Y₂]` (Y₁ primero, el de las
relaciones de cointegración) es el que exige el artículo, y es el que usan
`Λ`, `F`, `Θ` y `Σ`.

---

## 2. La pregunta de diseño del encuadre: contestada

> **¿Dónde vive la restricción de rango sobre la matriz de largo plazo?**

**En la forma de los parámetros, no en una restricción impuesta sobre ellos.**
La hipótesis del encuadre queda **confirmada**.

El artículo lo fija en las ecs. (4) y (6): `Π = ΛB'` con `B = [I_P; B₂]`, donde
Λ es M×P y B₂ es (M−P)×P. Cualquier par (Λ, B₂) ∈ ℝ^{Mr} × ℝ^{sr} es admisible
y produce rango ≤ r **automáticamente**; la normalización `B = [I_P; B₂]` es lo
que además lo identifica de forma única (p. 3648). No hay ninguna restricción
que imponer ni verificar.

**Consecuencia para el cast:** puede ser una traducción pura. El patrón de
`drtran` (`cast_diagonal` / `cast_embedded`) sirve **en cuanto a los
parámetros**.

### 2.1 Pero el contrato del cast de `drvec` es más ancho que el de `drtran`

Esto es lo accionable que el encuadre pedía anotar «en su momento», y es un
hecho estructural, no un detalle de implementación:

> **El cast de `drvec` tiene que devolver parámetros *y datos*.**

`W_t = Y₁ₜ + B₂'Y₂ₜ` depende de B₂, que es un parámetro estimado. Por tanto
`Ȳ_t` **se reconstruye en cada evaluación de la verosimilitud**. Los casts de
`drtran` devuelven sólo estructura VARMA; éste no puede.

(No es una novedad de `drvec`: `LEGACY_NOTES.md` §1 ya identificó este patrón en
`drv_project`. Lo que faltaba es la contabilidad de abajo.)

**Qué le pide `drvec` al cast, por evaluación:** una sola llamada, y dentro de
ella el acoplamiento es muy asimétrico:

| Bloque de `x[]` | Tamaño | Qué toca |
|---|---|---|
| `E[Ȳ]` | 0, r ó M | sólo `mu` |
| `Λ` | M·r | sólo Φ̄₁ |
| `Fᵢ` | (p−1)·M² | Φ̄ᵢ |
| `Θᵢ` | q·M² | sólo Θ* |
| `Σ` | M(M+1)/2 | sólo `qq` |
| **`B₂`** | **s·r** | **C̄, C̄⁻¹, todos los Φ̄, Θ*, `qq` y los `nobs`×M datos `Ȳ`** |

**B₂ es el parámetro caliente**, y es el único que acopla parámetros con datos.
Todo lo demás es O(M³) sobre matrices pequeñas. Si algún día se diseña el cast
propio, ésta es la única distinción que importa.

**Constante entre evaluaciones y hoy recomputado en todas:** los niveles de Y₂,
que `vec_shootx:518-525` reconstruye acumulando `∇Y₂` en cada llamada. Es
O(nobs·s) desperdiciado por evaluación; se iza fuera sin cambiar nada.

---

## 3. El defecto principal: Σ y σ² son el mismo parámetro, contado dos veces

### 3.1 El mecanismo

El motor **concentra la escala**. `drvmlest.c:89` llama a `elf` con `sigma2 =
1.0` y minimiza (`objcfunc`, `drvmlest.c:168`)

```
(π₁/π₁₀)^m · (π₂/π₂₀)
```

Este objetivo es **exactamente invariante** a reescalar `qq` por cualquier
constante c > 0: `f1 → f1/c` y `f2 → c^m·f2`. La escala se recupera aparte, como
`σ̂² = π₁/(n·m)` (`drvmlest.c:139`), y la covarianza estimada es `σ̂²·qq`.

`drvec` mete además **el triángulo inferior completo de Σ** en `x[]`
(`calc_nparametrs`, `drvec.c:104`). Es decir: `x[]` tiene **exactamente un grado
de libertad que la verosimilitud no puede ver**. El Hessiano es singular por
construcción y BFGS se desliza por la cresta hasta que el line search falla.

### 3.2 Medido

Escalando el Σ inicial por 1, 10, 100 y 0.01 (copia instrumentada,
`mink_muskrat`, p=2 q=1 r=1, caso 2):

| Σ₀ × | `sigma2` | `Σ_x[2][2]` | **producto** | `logelf` | parada |
|---|---|---|---|---|---|
| 1 | 0.580732 | 0.109085 | **0.0634** | 6.5053 | line search |
| 10 | 0.090618 | 0.713738 | **0.0647** | 6.4836 | line search |
| 100 | 0.008022 | 7.358633 | **0.0590** | 6.2633 | line search |
| 0.01 | 74.596545 | 0.000869 | **0.0648** | 6.6021 | line search |

`sigma2` y `Σ_x` se mueven en proporción inversa, **su producto es constante** y
`logelf` apenas cambia. Es una cresta plana, exactamente como predice el álgebra.

### 3.3 No es una sospecha: la suite ya lo tiene documentado

`drvarma/src/drvarma/estimate_py.py:15-18` lo dice con estas palabras:

> `qq` started at the **correlation matrix** of the OLS residuals. Because the
> concentrated objective is **scale-invariant in `qq`** (`f1 -> f1/c`,
> `f2 -> c^m f2`), this starting scale is what pins the reported `sigma2`/`Q`
> split, exactly as in the C.

La convención de la suite es, por tanto:

1. dejar el triángulo completo en `x[]` (`drvarma.c:1019` cuenta
   `m(m+1)/2`, igual que `drvec`), pero
2. **sembrar `qq` en la matriz de correlación** de los residuos (diagonal
   unidad), que fija un punto canónico sobre la cresta, y
3. **reportar `Sigma = sigma2 * Q`**.

**`drvec` incumplía (2) y (3) — corregido el 2026-08-16, ver §3.5.**
`init_guess` sembraba la covarianza cruda (valores ~0.08–0.11, dos órdenes por
debajo de O(1)), y la impresora sacaba el bloque sin multiplicar por `σ̂²`. El
legado sí lo hacía bien
(`drv_project/src/main.c:758`: `Sigma[i][j] = varma->sigma2 * varma->qq[i][j]`),
y `drvarma` también (imprime literalmente `Sigma = sigma2 * Q`).

Esto explica de paso por qué el Σ reportado parecía el doble del publicado:

```
drvec imprime      0.0838  0.0429  0.1091      <- esto es qq, no Sigma
sigma2 x eso       0.0487  0.0249  0.0633      <- esto es Sigma
Tabla 4 publica    0.0385  0.0181  0.0549
```

### 3.4 Efecto de corregirlo

Normalizando (fijando Σ₁₁ = 1 y quitando ese parámetro de `x[]`, npar 15 → 14),
el caso (2,1,1) **converge**:

```
antes:   OPTIMIZER STOPPED   58 iter   line search failed to locate a lower point
después: OPTIMIZER CONVERGED 57 iter   scaled distance between last two steps <= steptol
```

Sembrar en correlación (la convención de la suite, sin tocar `npar`) mejora pero
no basta por sí solo en este caso concreto.

**Aviso:** mientras exista la dirección plana, los errores estándar salen de
invertir un Hessiano singular (`drvmlest.c:113-121`, `cholsol`). Los `sd` que
`drvec` imprime **no son fiables**.

### 3.5 Lo aplicado (2026-08-16)

Se aplicaron **(2) y (3)**, que es alinearse con `drvarma`, no innovar:

- `init_guess` siembra ahora el bloque de covarianza en la **matriz de
  correlación** de los residuos (diagonal unidad; `qq = I` bajo `-diagcov`,
  como hace `init_varma`).
- La salida imprime **`Q`** (lo que de verdad se estima, con sus errores
  estándar) y por separado **`Sigma = sigma2 * Q`**, la covarianza de
  innovaciones — misma presentación que `drvarma`. De paso, el bloque de Σ
  respeta ya `-diagcov`, con lo que deja de leer fuera de `x[]` en ese caso.

Efecto medido sobre `mink_muskrat` (p=2, q=1, r=1):

| configuración | `logelf` antes | `logelf` después |
|---|---|---|
| `-case 1` | −171.1085 | **−12.3042** |
| `-case 2` | 6.5053 | 6.2424 |
| `-case 3` | 5.9579 | **6.5075** |
| `p=3 -case 2` | 10.6483 | 10.4763 |

El caso 1 —el *por defecto*— mejora en 159 unidades: la siembra cruda lo estaba
lanzando lejísimos por la cresta. Los casos 2 y p=3 se mueven unas décimas en
sentido contrario, y **eso es lo esperado**: al no haberse quitado el parámetro
redundante, la dirección plana sigue ahí y el punto donde el line search se
rinde depende de por dónde se entre en la cresta.

### 3.6 Quitado el parámetro redundante (2026-08-17)

Se normaliza **Σ₁₁ = 1** y se saca de `x[]`: `npar` pasa de 15 a 14 en el caso
bivariante. La escala vive ya sólo en `sigma2`, con lo que **Σ₁₁ = sigma2
exactamente** y el reparto deja de ser arbitrario. Cambia `npar`, y por tanto
AIC/BIC y los grados de libertad de cualquier test LR: eso era lo que aconsejaba
decidirlo aparte.

Lo que consigue, medido:

| | antes (§3.5) | **después** |
|---|---|---|
| `npar` (bivariante, caso 2) | 15 | **14** |
| logL, legado caso 2 | 6.2424 | **6.7498** |
| logL, niveles caso 2 | 5.1803 | **6.4679** |
| logL, legado caso 1 | −12.3042 | **−11.7501** |
| **\|Σ̂\|, layout antiguo caso 2** | 0.002496 | **0.002373** ← el mejor de los cuatro |
| Λ̂₁, niveles caso 2 (Tabla 4: 0.8392) | 0.5610 | **0.8371** |

La log-verosimilitud sube en 5 de 6 configuraciones y **|Σ̂| se acerca mucho a la
banda de §5.11** (0.0022–0.0024), aunque sólo una de las cuatro configuraciones
probadas cae dentro. Los errores estándar salen ya de un Hessiano no singular.

**Lo que NO consigue:** el optimizador sigue parando en termcode 3 («last global
step failed to locate a lower point») en la mayoría de casos. Es decir, la
dirección **exactamente plana** ha desaparecido —eso está demostrado por el
álgebra y por `npar`— pero la superficie sigue siendo dura, que es justo lo que
`drtran/docs/OPTIMIZER_STOPPING_STUDY.md` §5 describe para `drvarma`: direcciones
planas genuinas donde el line search falla limpiamente *estando en el óptimo*.
Termcode 3 aquí ya es la lectura benigna, no el síntoma de antes.

**Dónde sí se nota de forma inequívoca es en el `-lrtest`** (§4.4): la
log-verosimilitud pasa a ser **monótona en r**, como exige el anidamiento, y los
estadísticos LR dejan de salir negativos.

---

## 4. Otros defectos

Ordenados por gravedad. Ninguno tiene marca `TODO`/`FIXME` en el código.

**4.1 — La impresora estructurada ignoraba `-diagar`/`-diagma`/`-diagcov` y leía
fuera de `x[]`. CORREGIDO el 2026-08-16.** El bloque de salida recorría siempre
M² por matriz y M(M+1)/2 para Σ, pero `calc_nparametrs` reserva menos con las
banderas diagonales. Con `-diagma` en el caso bivariante `npar` = 13 y la
impresora leía 15: desplazaba todos los bloques posteriores y **leía dos
posiciones más allá del final de `x[]` y `dev[]`** (lectura fuera de límites).

```
antes:  Theta[1] =  0.767213  0.995656 / 0.133498  0.068281   <- lee 4, hay 2
        Sigma    =  0.141667 / -0.382287  0.000000            <- corrido
        B2       =  0.000000                                   <- fuera de x[]
después: Theta[1] = 0.767213  0.000000 / 0.000000  0.995656   <- lo que hay
        Sigma     = 0.061590 / 0.031502  0.065359
        B2        = -0.382287                                  <- coincide con B
```

El recorrido consume ahora exactamente los parámetros que existen, y se añadió
una comprobación (`ERROR output: consumed X of Y parameters`) que grita si
vuelve a descuadrar — el análogo de la que ya tenía `init_guess`.

**4.2 — B₂ se imprimía traspuesta. CORREGIDO el 2026-08-16.**
`vec_shootx:398-400` e `init_guess:263` almacenan B₂ **por columnas**
(`for j { for i }`); la impresora `B2 (s x r)` lo recorría **por filas**. La
segunda impresora (`Cointegration matrix B`) sí usaba el índice por columnas:
las dos se contradecían. Invisible con s=r=1 (mink–muskrat), **visible en cuanto
r>1**, que es la mayoría del banco (denmark r=2, Raotbl1 r=2, Raotbl6 r=5…).

Comprobado sobre un caso M=5, r=2 construido con los datos de Denmark
(s=3, r=2, así que B₂ es 3×2 y la transposición se ve):

```
antes           B2 = [-0.198290  2.332606 ; 2.624435 -0.159570 ; -0.293424  2.638192]
                B  filas 3-5 = [-0.198290 -0.159570 ; 2.332606 -0.293424 ; 2.624435  2.638192]
                             ^ las dos impresoras discrepan

después         B2 = [-0.211914 -0.133889 ; 2.408537 -0.294613 ; 2.641312  2.648907]
                B  filas 3-5 = idénticas
```

Ahora las dos impresoras leen **la misma copia** de B₂, así que no pueden volver
a divergir. De paso se corrigió la etiqueta: `B = [I_r; B2]` es (M×r), no
`[I_r; B2']`.

*Ambos son cambios de salida solamente: la estimación no se toca (`-case 2`
sobre mink–muskrat da `logelf = 6.2424133802` antes y después). Verificado sobre
16 configuraciones —los tres casos deterministas, las banderas diagonales
sueltas y combinadas, M=2 y M=5, r=1/2/3— sin ningún descuadre.*

**4.3 — Y₂ se reconstruía acumulando desde cero. CORREGIDO: leer niveles, que
desde el 2026-08-17 es el layout por defecto.** `vec_shootx` e `init_guess` ponían `Y2_level[1] = 0` («initial level
unknown; offset absorbed in mean»). El argumento **es correcto en los casos 2 y
3** y lo verifiqué midiendo: con niveles verdaderos y con niveles acumulados
desde cero la log-verosimilitud es **idéntica** (8.002607 en ambos,
implementación independiente). Pero:

- **es falso en el caso 1**, que es el *por defecto*: sin media libre nada
  absorbe el desplazamiento `B₂'c`, y como depende de B₂ —que se está
  estimando— contamina la estimación;
- el `Ê[W]` reportado **no es comparable con el publicado**. En mink–muskrat el
  artículo da 8.1345 y `drvec` ~10.55; la equivalencia es
  `8.1345 + 0.2042·log(194682) = 10.6215`, y sólo se puede hacer conociendo el
  nivel inicial, que el `.inp` no contiene.

**Lo aplicado (en dos pasos).** El 2026-08-16, un flag `-levels`: el `.inp` trae
las M series en niveles (cols 1..s = Y₂, cols s+1..M = Y₁) y `drvec` diferencia
Y₂ internamente, consumiendo una observación. El 2026-08-17, **ese pasa a ser el
comportamiento por defecto** y el layout antiguo queda detrás de
**`-differenced`** — porque el modo correcto no debe ser el que hay que pedir.
El aviso del caso 1 sigue saliendo, ahora cuando se usa `-differenced`.

Para que un fichero leído con el layout equivocado no pase inadvertido, el
layout usado se imprime en consola **y en la cabecera del `.out`**:

```
Layout : all series in levels (61 of 62 observations used)
```

**Ficheros afectados por el cambio de defecto:**
`datasets/mauricio/mink_muskrat.inp` se ha **regenerado en niveles** (de paso se
corrigió su línea de nombres, que estaba en orden inverso a sus columnas);
`data/AL.inp` sigue en el layout antiguo y lleva anotado que necesita
`-differenced`. El resto de `data/*.inp` son ficheros de `drvarma`/legado y
conviene comprobarlos antes de usarlos con `drvec`.

De paso, los niveles de Y₂ se calculan **una sola vez** (`build_y2_levels`) en
lugar de reconstruirse en cada evaluación de la verosimilitud y otra vez en
`init_guess` — que era el desperdicio anotado en §2.1.

**Efecto medido** sobre `mink_muskrat` (p=2, q=1, r=1), que es exactamente lo
que §4.3 predecía:

| | layout antiguo | **niveles** | Mauricio Tabla 4 |
|---|---|---|---|
| Ê[W] | 10.5473 | **8.2103** | **8.1345** |
| B̂₂ (caso 2) | −0.2832 | **−0.1991** | **−0.2042** |
| B̂₂ (caso 3) | — | **−0.2047** | **−0.2042** |
| convergencia (caso 2) | termcode 3 | **converge (steptol)** | — |

Ê[W] pasa de no ser comparable con nada a caer a 0.08 del valor publicado, y B̂₂
en el caso 3 coincide hasta la milésima. **Es la mejora más grande de toda la
sesión en cercanía a los parámetros publicados**, y viene sólo de darle al
programa el nivel que le faltaba.

*(Las log-verosimilitudes de las dos rutas no coinciden —6.2424 frente a
5.1803— aunque la superficie sea la misma: el punto de partida difiere y la
cresta plana de §3.5 sigue ahí, así que cada ruta se para en un sitio distinto.
Otra razón más para tomar esa decisión pendiente.)*

**4.4 — `-lrtest` estaba documentado, se parseaba y no existía. IMPLEMENTADO el
2026-08-16.** `global_lrtest` se asignaba y **no se leía nunca**, mientras la
ayuda lo anunciaba y un mensaje de error remitía a él («use -lrtest to test»).

Ahora estima r = 1..M−1 y reporta, para cada rango, `npar`, logL, AIC y BIC, y
para cada par consecutivo el estadístico **2·[L(r+1) − L(r)]** (Mauricio 2006,
Remark 5 y Tabla 3) contra valores críticos asintóticos.

Tres decisiones que conviene dejar escritas:

1. **`-lrtest` es incompatible con `-differenced`.** La partición de columnas
   del `.inp` es s = M − r, así que un fichero pre-diferenciado **no se puede
   releer con otro rango**: el test estaría comparando modelos ajustados a datos
   distintos. El programa se niega y lo explica.
2. **El estadístico es de tipo máximo autovalor, no traza** (compara r con r+1,
   no r con M), así que los valores críticos son los de λ-max. Están tabulados
   para M−r = 1..11 en los casos 1 (sin constante) y 2 (constante restringida),
   **extraídos de `urca` 1.3.4** —la misma implementación de referencia que usa
   `benchmark/`— y no transcritos a mano. Para el **caso 3 no se tabulan**: el
   estadístico se imprime y los valores críticos salen como `-`, en vez de
   ofrecer los de otro caso y arriesgarse a inducir un rango equivocado.
3. **r = 0 sí entra; r = M no.** `r = 0` es la hipótesis nula de no
   cointegración (Π = 0, es decir un VARMA sobre ∇Y): con r = 0 se tiene
   C̄ = I, H̄ = 0, Λ y B₂ vacíos y Ȳ = ∇Y, así que el modelo es expresable sin
   más. `r = M` sería un proceso estacionario en niveles y no lo es, de modo
   que la secuencia termina en r = M−1. *(Inicialmente lo dejé fuera por error;
   el artículo BVECM lo usa como su H₀ y tenía razón — ver
   `ESTUDIO_BVECM_vs_DRVEC.md` §3.5. Añadido el 2026-08-17.)*

**El test destapó §3.6, y §3.6 lo arregló.** En su primera versión, sobre
Denmark (M=5), salió un **LR negativo** (−185.79) entre r=1 y r=2 — imposible
entre modelos anidados, y prueba de que uno de los dos ajustes no alcanzaba su
óptimo. Con el parámetro redundante fuera (§3.6), la log-verosimilitud pasa a ser
**monótona en r** y todos los estadísticos salen positivos:

```
antes de §3.6   logL:  844.93   752.04   870.19   879.43     <- no monótona
                LR:   -185.79   236.31    18.48
después         logL:  720.08   800.76   866.09   879.43     <- monótona ✔
                LR:    161.36   130.65    26.69
```

La salida **conserva la marca `NOT INTERPRETABLE` cuando LR < 0**, como red de
seguridad: es una comprobación barata que delata al instante un ajuste que no
convergió.

**Validado contra el banco, en dos casos.** Con r = 0 incluido, la secuencia
completa acierta el rango publicado en los dos sistemas con rango conocido:

```
mink_muskrat (M=2, caso 2)     r=0  logL  -6.3311     r=1  logL   6.4679
  LR(0->1) = 25.5981   crít. (M-r=2) 13.75 / 15.67 / 20.20  -> rechaza al 1%
  => r = 1, el rango que publica Mauricio (2006).

urca_UKconsumption (M=3, caso 2, n=76)   los TRES rangos convergen
     r=0  logL 522.4086    r=1  logL 557.4726    r=2  logL 570.2297
  LR(0->1) = 70.1280   crít. (M-r=3) 19.77 / 22.00 / 26.81  -> rechaza al 1%
  LR(1->2) = 25.5141   crít. (M-r=2) 13.75 / 15.67 / 20.20  -> rechaza al 1%
  => r = 2, el rango que reporta `ca.jo`.
```

AIC y BIC coinciden en ambos. Son las dos primeras validaciones de `drvec`
contra referencias del banco. Nótese que **sin r = 0 el caso bivariante no daba
ningún contraste** (no había par que comparar): el contraste que de verdad
importa —¿hay cointegración?— sólo existe desde que r = 0 está dentro.

En cambio sobre Denmark (M=5, n=54, 49–67 parámetros) los ajustes **no
convergen** —r=1 para a las 3 iteraciones, r=2 a las 17— y el rango que sale no
es fiable. No es un fallo del test: es un modelo demasiado rico para esa muestra,
y por eso la salida remite a los banners del optimizador de cada rango.

**4.5 — Desbordamiento de búfer con rutas largas. CORREGIDO el 2026-08-17.**
`inputf`, `outputf` y `base_name` eran `NEW_STR(80)` y se llenaban con `strcpy`/
`strcat` sin comprobar; con una ruta de ~110 caracteres el programa **abortaba**
(`*** buffer overflow detected ***`). Lo encontré al intentar ejecutar desde un
directorio temporal. Ahora se dimensionan a partir de `strlen(argv[1]) + 8`
(el sufijo `.inp`/`.out` más el terminador), con comprobación de asignación.
Verificado con una ruta de 140 caracteres, que da el mismo `logelf` que la corta.

**4.6 — Higiene.** `src/drvec.c.bak` y `src/drvec.c.mauricio` son **idénticos**
byte a byte (`cmp` sin diferencias): uno sobra. `drvec.c` no termina en salto de
línea. Los ficheros del motor (`elfvarma.c`, `drvmlest.c`) están en Latin‑1, no
en UTF‑8, lo que hace que `grep` los trate como binarios y **falle en silencio**
— conviene saberlo antes de concluir que algo «no está en el código».

---

## 5. El contraste con Mauricio (2006): la causa NO está en `drvec`

*Recorrido: §5.1–5.4 acotan el hueco y descartan que sea del programa; §5.5
descarta redondeo y datos; §5.6 lo localiza en |Σ̂|; §5.7 mide el techo real;
§5.8 calibra con Chan & Wallis; §5.9 saca del AddOn que el patrón es
sistemático en el artículo; §5.10 descarta la puerta de invertibilidad; §5.11
**fija el criterio de aceptación** y acota lo que queda sin saberse.*

### 5.1 Cuál es el número que hay que igualar

La **Tabla 4** (modelo (25), EML, sin la restricción B = [1,0]') es exactamente
lo que `drvec` implementa. Reporta **log-verosimilitud 15.1257**.

He confirmado la base de comparación despejando N y K de los AIC/BIC publicados
(con la convención `AIC = (−2L*+2K)/N`, `BIC = (−2L*+K·lnN)/N`; la fórmula
impresa en el artículo tiene el signo cambiado):

```
Tabla 2:  N=61, K=15  -> BIC 0.4990 (publicado 0.4990)  ✔
Tabla 4:  N=61, K=13  -> BIC 0.3802 (publicado 0.3802)  ✔
```

**El método está validado aparte**, en un caso donde N se conoce por otra vía: el
AddOn dice literalmente «*N = 112 effective observations*» (Fig. A3), y el
despeje sobre sus tablas devuelve exactamente eso, con recuentos de parámetros
que cuadran con los modelos:

```
AddOn Tabla A1 (modelo A.12):  N=112, K=9   -> BIC 12.2268 (publicado 12.2268) ✔
                               Φ₁(4) + Θ₁ diag(2) + Σ(3) = 9
AddOn Tabla A3 (modelo A.14):  N=112, K=8   -> BIC 12.1985 (publicado 12.1985) ✔
                               Λ(2) + B₂(1) + Θ₁ diag(2) + Σ(3) = 8
```

**N = 61**, la misma muestra que usa `drvec`. K = 13 frente a los 15 parámetros
de `drvec` porque la Tabla 4 fija dos a cero (`F̂₁[2][2]` y `Θ̂₁[1][1]`, marcados
«(—)»).

### 5.2 Qué da `drvec`

`logelf = 6.5053` (sin corregir) / `6.2896` (con Σ normalizada). Hueco ≈ **8.8**.

### 5.3 Los puntos publicados son NO INVERTIBLES tal como están impresos

Al evaluar la verosimilitud en los valores de las tablas, el motor rechaza:
`ifault = 4, MA operator non-invertible`. Los autovalores de Θ̂₁ son:

```
Tabla 2:  0.84608  y  -1.00628     (max |λ| = 1.0063)
Tabla 4:  0.99038  y  -1.00778     (max |λ| = 1.0078)
```

El artículo menciona sólo el positivo («one positive eigenvalue equal to
+0.9904», p. 3658, como indicio de sobrediferenciación) y no el otro. Los
óptimos publicados están **sobre la frontera de invertibilidad**, y el
redondeo a cuatro decimales basta para dejarlos fuera. `drvarma`, sobre estos
mismos datos, converge a una raíz inversa de módulo **1.000050**: el fenómeno es
de los datos, no del programa.

**Esto condiciona cualquier homologación de `drvec` contra Mauricio (2006)** y
hay que tenerlo previsto.

### 5.4 El hueco no lo causa `drvec`

Tres medidas independientes:

1. **El motor está validado.** La verosimilitud exacta AS 311 (puerto Python de
   `drvarma`) coincide con una normal multivariante calculada a mano
   —construyendo la matriz de covarianzas nm×nm y evaluando la densidad—
   **con error 2·10⁻⁸**, en ruido blanco y en VAR(1). La fórmula de
   `drvmlest.c:135-138` también la he verificado algebraicamente: es la
   log-verosimilitud gaussiana concentrada correcta.

2. **Una implementación independiente da lo mismo.** El puerto Python de
   `drvarma`, evaluado en los valores publicados de la **Tabla 2** (con Θ
   encogido lo justo para entrar en la región invertible), da **≈ 4.8 (n=62)** y
   **≈ 3.9 (n=61)** frente a los 15.6116 publicados.

3. **El desplazamiento de Y₂ no es la causa** (§4.3): logL idéntica en ambos
   marcos.

Además, el VARMA(2,1) **sin restringir** sobre Ȳ_t —cota superior de cualquier
modelo VEC con ese B₂— da logL = 8.0026 con el B₂ del artículo. Un modelo
restringido no puede superar a su versión libre, luego **15.1257 y ~8 no están
en la misma escala**.

### 5.5 Hipótesis descartadas

**(a) El redondeo a 4 decimales — descartada.** Optimizando localmente desde los
valores publicados de la Tabla 2 (Nelder–Mead reiniciado hasta estancarse):

```
n=62:  logL = 8.1477   max|eig(Θ₁)| = 1.00005   μ̂ = [10.7871, 12.9870]
n=61:  logL = 7.1613   max|eig(Θ₁)| = 1.00005   μ̂ = [10.7902, 12.9728]
```

Converge pegado a la frontera de invertibilidad y con μ̂ ≈ ĉ publicado —**es la
cuenca correcta**— pero da 7.16, no 15.61.

**(b) Los datos — descartada.** Se verificó contra la colección `mhsets` de
Hipel & McLeod (StatLib), que es la fuente canónica de esta serie:

```
mhsets_ecology-mink.arff     vs  datasets/mauricio  ->  IDÉNTICOS (62 obs)
mhsets_ecology-muskrat.arff  vs  datasets/mauricio  ->  IDÉNTICOS (62 obs)
```

También coincide `fma::mink` (R) en el tramo común. La procedencia declarada en
`MANIFEST.md` (Jones 1914 vía TSDL) es correcta y es la misma serie que usa la
literatura. **Los datos no son el problema.**

*(Nota: la desviación de +0.11 entre la media muestral del muskrat y el ĉ
publicado no es evidencia de nada: ĉ es la media **del modelo**, y en una serie
casi integrada como ésta puede alejarse legítimamente de la media muestral. El
mink, estacionario, sí coincide: 10.7880 vs 10.7976.)*

### 5.6 Dónde está realmente el hueco: en |Σ̂|

En los propios (μ̂, Φ̂, Θ̂) publicados de la Tabla 2, la covarianza de los
**residuos exactos AS 311** sobre estos datos es:

```
residuos implicados por los parámetros del artículo   |Σ| = 0.002414
Σ̂ que el artículo publica junto a ellos (EML)         |Σ| = 0.001788   ratio 1.35
Σ̂ que el artículo publica en la columna CML           |Σ| = 0.002312
```

`−½·n·log(1.35) = −9.16` sobre un hueco total de 11.7: **prácticamente todo el
hueco es esa diferencia de determinante.** Y obsérvese que **la columna CML del
artículo sí es coherente con estos datos** (0.002312 ≈ 0.002414); la que no
encaja es la columna EML.

Para calibrar cuánto ajuste exige ese 0.001788, VAR(p) por MCO sobre estas
mismas series:

| p | 1 | 2 | 3 | 4 | 5 | 6 | **7** | 8 |
|---|---|---|---|---|---|---|---|---|
| \|Σ\| | .00458 | .00362 | .00303 | .00251 | .00221 | .00197 | **.00184** | .00163 |

El |Σ̂| EML publicado equivale al de un **VAR(7)** sin restringir. Que un
VARMA(2,1) lo alcance es exigente, aunque no imposible: la ML exacta estima Σ
conjuntamente con las condiciones iniciales y su Σ̂ no tiene por qué coincidir
con la covarianza muestral de residuos —especialmente con una MA pegada a la no
invertibilidad, que es justo el caso y justo el argumento del artículo—.

### 5.7 Estado y qué hacer

**Lo establecido:** el hueco **no es atribuible a `drvec`**. Motor validado a
2·10⁻⁸ contra una normal multivariante; transformación verificada término a
término; implementación independiente de acuerdo; datos verificados contra la
fuente canónica. El VARMA(2,1) **sin restringir** sobre Ȳ_t —cota superior de
cualquier VEC con ese B₂— da 8.0026, y un modelo restringido no puede superar a
su versión libre: **15.1257 y ~8 no están en la misma escala**.

**El techo real, medido.** Búsqueda global multiarranque (40 arranques,
Nelder–Mead con reinicios, sobre la verosimilitud exacta del modelo (22)):

```
n=62:  máx logL = 8.1440   |Σ| = 0.002269   max|eig(Θ₁)| = 1.00005
n=61:  máx logL = 7.1701   |Σ| = 0.002311   max|eig(Θ₁)| = 1.00005
```

Sobre estos datos **15.61 no es alcanzable**. Y el detalle que más dice: el
|Σ| del óptimo global exacto en n=61, **0.002311**, coincide prácticamente con
el |Σ̂| que el artículo publica en su columna **CML**, 0.002312. Es decir, el
óptimo de la ML *exacta* que medimos aterriza justo donde el artículo sitúa su
ajuste *condicional*.

### 5.8 Calibración externa: Chan & Wallis (1978)

`literature/Chan-MultipleTimeSeries-1978.pdf` es la referencia clásica de estos
datos y da la comparación independiente que faltaba. Su modelo (13) se estima
sobre **(Δ log muskrat, log mink)** —exactamente el vector que lleva nuestro
`.inp`— con AR de **orden 4** y ML **exacta** para la parte MA vectorial
(Osborn, 1977), y reportan la «varianza generalizada» |Σ̂| explícitamente:

| fuente | modelo | \|Σ̂\| |
|---|---|---|
| Jenkins (1975), forma diagonal (12) | AR(4)+MA(1) diag. | 0.00299 |
| Chan & Wallis (1978), ec. (13) | AR(4)+MA(1), ML exacta | **0.00246** |
| Mauricio (2006) Tabla 2, columna **CML** | VARMA(2,1) | 0.002312 |
| **óptimo global ML exacta medido aquí** (n=61) | VARMA(2,1) | **0.002311** |
| cov. de residuos en los parámetros de Mauricio | — | 0.002414 |
| Mauricio (2006) Tabla 2, columna **EML** | VARMA(2,1) | **0.001788** |

Lo que esto añade: **nuestro número está en línea con la literatura y el de la
columna EML no lo está.** Chan & Wallis, con un modelo *más rico* (AR de orden 4
frente a 2) y ML exacta, se quedan en 0.00246; nuestro VARMA(2,1) llega a
0.002311, algo mejor y perfectamente plausible. La columna EML del artículo
está un **27 % por debajo** de lo que consigue el modelo de orden 4 publicado.

Dos avisos de procedencia que conviene anotar:

- Chan & Wallis dicen que Jones (1914) cubre **1848–1909**; Mauricio dice
  1850–1911 y nuestros datos son 1850–1911 (`fma::mink`, además, es 1848–1911).
  La ventana declarada no es consistente en la literatura.
- Chan & Wallis usan **y₁ = muskrat, y₂ = mink**, al revés que Mauricio. Es una
  trampa fácil al comparar matrices.

### 5.9 El AddOn: no trata mink–muskrat, pero explica el patrón

`518-2013-11-11-JAM106-AddOn.pdf` tiene dos secciones: **A1** es la teoría del
caso bivariante (usada ya en §1) y **A2** es el ejemplo del **Census Housing**
—el otro caso del banco—, no mink–muskrat. Aun así aporta lo que faltaba, porque
enseña el **patrón EML frente a CML** en un segundo conjunto de datos.

**Hecho 1: Mauricio afirma explícitamente que su EML reduce mucho las varianzas.**
AddOn p. 8: *«EML estimation implies … (ii) a substantial decrease in the
estimated variances of the error processes with respect to CML»*. Medido sobre
sus propias tablas:

| caso | \|Σ̂_EML\| | \|Σ̂_CML\| | razón |
|---|---|---|---|
| Census Housing, Tabla A1 | 270.35 | 537.45 | **1.99** |
| Census Housing, Tabla A3 (VEC) | 261.41 | 547.06 | **2.09** |
| mink–muskrat, Tabla 2 | 0.001788 | 0.002312 | **1.29** |

O sea: lo que en §5.6 parecía una anomalía aislada es **sistemático en el
artículo**, y en el Census Housing es aún más acusado (factor 2).

**Hecho 2: sus óptimos EML son MA no invertibles, y es deliberado.** El AddOn
declara desde el principio (p. 3) que el interés está en *«cointegrated systems
with possibly **noninvertible** moving average terms»*, y discute (p. 6) *«the
implications of the possibility that Θ₁ = I»* —una raíz unitaria en la MA
estacional— como algo *«clearly present when EML estimation is employed»*.
Las tablas lo confirman (MA estacional en lag 12, |raíz| = λ^(−1/12)):

| | Θ̂₁ | \|raíces\| | |
|---|---|---|---|
| Tabla A1 EML | diag(0.9641, 1.0844) | 1.00305, **0.99327** | **no invertible** |
| Tabla A3 EML | diag(0.9600, 1.1318) | 1.00341, **0.98974** | **no invertible** |
| Tabla A3 CML | diag(0.7531, 0.6918) | 1.02391, 1.03118 | invertible |

Encaja exactamente con lo visto en §5.3: los Θ̂₁ de las Tablas 2 y 4 tienen
autovalores de módulo 1.0063 y 1.0078. **La no invertibilidad no es un descuido
de impresión: es donde el método pretende operar.**

**Hecho 3, y es el que señala al mecanismo:** `chekma` —la comprobación de
invertibilidad del motor, `_as311.py:123-141` y `elfvarma.c:chekma`— **rechaza
de plano** todo punto con algún autovalor de módulo ≥ **1.00005**, devolviendo
`ifault = 4`; y `objcfunc` traduce eso en objetivo = 1.0, es decir una barrera
plana. Y aquí está el indicio: **todas** nuestras optimizaciones (local y
multiarranque, §5.5 y §5.7) convergieron a max|λ(Θ₁)| = **1.00005 exacto** — es
decir, empujando contra la barrera y parándose justo en la tolerancia.

La hipótesis de trabajo era por tanto: **el óptimo EML publicado vive fuera de la
región que `chekma` permite**, y por eso el motor no puede alcanzarlo.

### 5.10 La hipótesis de la puerta: comprobada y DESCARTADA

Se desactivó `chekma` (`_as311.chekma = lambda *_: 0`) y se repitió todo:

```
en el punto publicado de la Tabla 2:  con puerta  ifault = 4 (rechazado)
                                      sin puerta  logL = 4.7776 (n=62) / 3.7300 (n=61)

óptimo por multiarranque    con puerta                sin puerta
  n=62        logL 8.1440  |Σ| 0.002269      logL 8.2186  |Σ| 0.002242  max|λ| 1.00000
  n=61        logL 7.1701  |Σ| 0.002311      logL 7.2509  |Σ| 0.002294  max|λ| 0.99999
```

**La puerta cuesta 0.07, no 7.5.** Levantarla permite evaluar los puntos
publicados, pero **no acerca en absoluto** a los 15.61 ni a |Σ̂| = 0.001788. La
hipótesis queda descartada como explicación del hueco.

Y añade un dato que precisa el diagnóstico: sin puerta el óptimo se sitúa en
max|λ(Θ₁)| = **1.00000/0.99999**, es decir **justo en la frontera, no fuera**.
La verosimilitud exacta sobre estos datos tiene su máximo *en* la no
invertibilidad límite, no más allá; la tolerancia de `chekma` (1.00005) apenas
estaba mordiendo. Eso explica también por qué las corridas con puerta se paraban
clavadas en 1.00005: no era la barrera empujando, era el óptimo estando ahí.

*(Nota: sigue siendo una limitación real de `drvec` frente al método del
artículo —el motor no puede operar en la región no invertible donde Mauricio
dice trabajar—, sólo que no es la causa de esta discrepancia concreta.)*

### 5.11 Balance, criterio de aceptación, y lo que queda sin saberse

**Descartado como causa:** que sea de `drvec` (§5.4), el redondeo (§5.5a), los
datos (§5.5b), la muestra efectiva (§5.1, método validado en cuatro casos) y la
puerta de invertibilidad (§5.10).

**Establecido:** el hueco está entero en |Σ̂| (§5.6); nuestro lado concuerda con
la literatura independiente (§5.8); y **la reducción de Σ̂ bajo EML es
sistemática en el artículo** —factor 1.3 en mink–muskrat, factor 2 en Census
Housing (§5.9)—, luego no es un desliz aislado de una tabla.

**Sin saberse:** por qué el programa EML del autor obtiene, de forma
sistemática, varianzas de error en torno a la mitad de lo que la verosimilitud
exacta AS 311 —su propia `elf`, validada aquí a 2·10⁻⁸ contra una normal
multivariante— admite sobre los mismos datos. Es una propiedad de los números
publicados, no de `drvec`, y **no se puede cerrar con el material disponible**:
haría falta el programa original o correspondencia con el autor.

**Pero eso ya no bloquea**, porque el criterio de aceptación sí se puede fijar.

Y debe fijarse sobre **|Σ̂|, no sobre la log-verosimilitud**, por una razón
técnica: `drvec` calcula la verosimilitud de **Ȳ_t** (61 obs, 1851–1911),
mientras que el modelo (22) y la búsqueda global la calculan sobre **Y_t**. La
aplicación Y → Ȳ tiene jacobiano unidad (|C̄| = ±1) pero **condiciona en Y₁₈₅₀**,
así que los dos niveles no son directamente comparables. (El artículo sí los
compara entre sus Tablas 2 y 4, y los usa en el test LR de la Tabla 3; conviene
tenerlo presente.) **|Σ̂| en cambio es invariante**: es la misma covarianza de
innovaciones en las dos representaciones.

> **Criterio de aceptación para `mink_muskrat`:** un VARMA(2,1) sobre estos
> datos debe dar **|Σ̂| ≈ 0.0023**, y en ningún caso por debajo de ~0.0022.
> Respaldado por tres fuentes independientes: Chan & Wallis (0.00246, con AR de
> orden 4), la columna CML del propio artículo (0.002312) y la búsqueda global
> (0.002311 con puerta, 0.002294 sin ella).
>
> Lo que **no** sirve como objetivo son los 15.61 / 15.13 publicados, ni el
> |Σ̂| = 0.001788 que los acompaña.

Como referencia secundaria, y sólo dentro de su propia representación: la ML
exacta del modelo (22) sobre Y_t topa en **7.17** (n=61) / **8.14** (n=62).

**Dónde está `drvec`** (p=2, q=1, r=1):

| | \|Σ̂\| | ¿dentro de 0.0022–0.0024? |
|---|---|---|
| tras §3.5, layout antiguo, caso 2 | 0.002496 | no, un 8 % alto |
| tras §3.6, layout antiguo, caso 2 | **0.002373** | **sí** |
| tras §3.6, layout antiguo, caso 3 | 0.002483 | por poco, no |
| tras §3.6, **niveles (el defecto)**, caso 2 | 0.002461 | por poco, no |
| tras §3.6, niveles, caso 3 | 0.002569 | no |

Quitar el parámetro que no estaba identificado acercó todo al objetivo, pero
**el criterio no se cumple de forma fiable**: sólo lo cumple una de las cuatro
configuraciones, y las cuatro son matemáticamente equivalentes salvo
reparametrización. Esa dispersión **es en sí misma el diagnóstico**: el
optimizador sigue parando en termcode 3 en puntos ligeramente distintos según
por dónde entre. Cerrar eso es el punto 1 de §8, y es lo que decidirá si
`drvec` pasa o no el criterio.

---

## 6. Lo que la suite ya decidió y hay que respetar

`drtran/docs/OPTIMIZER_STOPPING_STUDY.md` (2026-08-04/05) es de lectura
obligada antes de tocar nada del optimizador. Resumen operativo:

- El optimizador es **el original de Mauricio, en todas partes**. Tres variantes
  de `typx` se implementaron y **se rechazaron con evidencia**.
- «last global step failed to locate a lower point» es **termcode 3**. La suite
  lo interpreta como *estar en el óptimo*, no como fallo
  (`estimate_py.py:328-332`), y anota como **pregunta abierta** si eso es cierto
  o si significa mal condicionamiento.
- El estudio atribuye el único caso de fuga de `drtran` a «**a covariance seeded
  at zero on an unbounded ridge**». Lo de `drvec` (§3) es la misma familia de
  problema, ahora **medido y con la cresta identificada exactamente**.

Es decir: el hallazgo de §3 no contradice al estudio, lo **precisa** en un caso
concreto. Y la corrección que propongo no toca el optimizador — sólo la siembra
y el informe, que es capa de `drvec`.

---

## 7. Correcciones al encuadre

Dos afirmaciones de `ENCUADRE_ESTUDIO.md` que conviene rectificar para que no se
arrastren:

1. **«`drvec.c` NO es idéntico al original. Lo añadido es documentación»** — lo
   añadido **no** es sólo documentación. `init_guess` está **reescrito por
   completo**: pasó de OLS sobre Ȳ_t a la regresión condicional tipo Johansen
   `∇Y_t = ΣFᵢ∇Y_{t−i} − Λ(W_{t−1}−E[W])`, y en el camino **se perdió** la
   inicialización de la MA por momentos que sí tenía el original (hoy Θ arranca
   en cero exacto). También cambiaron el manejo de los casos deterministas, el
   conteo `nf = p−1`, y todo el bloque de salida (que además perdió AIC/BIC).

2. **«`src/drvec.c.bak`»** — es byte a byte idéntico a `drvec.c.mauricio`.

---

## 8. Orden de trabajo propuesto

**Hecho el 2026-08-16:**

- [x] **§3.5** — sembrar en correlación y reportar `Sigma = sigma2 * Q`.
- [x] **§4.1 y §4.2** — salida corrida bajo las banderas diagonales (con lectura
      fuera de límites) y B₂ traspuesta.
- [x] **§4.3** — leer Y₂ en niveles, calculados una sola vez.
- [x] **§4.4** — `-lrtest` implementado.
- [x] **Degradado `mauricio/mink_muskrat`** en `benchmark/README.md`, para que
      nadie ajuste `drvec` contra un objetivo no establecido.

**Hecho el 2026-08-17:**

- [x] **§3.6** — quitado el parámetro redundante de Σ.
- [x] **§4.5** — búferes de nombre de fichero.
- [x] **Medido contra el criterio de §5.11**: |Σ̂| baja de 0.002496 a un rango
      de 0.00237–0.00257 según la configuración. **El criterio aún no se cumple
      de forma fiable**, y la dispersión entre configuraciones equivalentes
      señala al termcode 3 residual como la causa.
- [x] **Invertido el defecto del layout**: niveles por defecto, `-differenced`
      para el formato antiguo. `mink_muskrat.inp` regenerado en niveles,
      `data/AL.inp` anotado, layout impreso en consola y en el `.out`.
- [x] **Primera validación del `-lrtest` contra el banco**: `urca_UKconsumption`
      da r = 2, igual que `ca.jo` (§4.4).
- [x] **r = 0 añadido al `-lrtest`**, con lo que el caso bivariante pasa a tener
      contraste y `mink_muskrat` da r = 1 (§4.4).
- [x] **`-fixb2`**, del estudio comparado con `drv_project`/BVECM
      (`ESTUDIO_BVECM_vs_DRVEC.md` §3.3): B₂ fijo, con valor a priori (test LR
      válido) o en su valor OLS (arranque en caliente). El ajuste restringido
      **converge** donde el libre se para en termcode 3.

**Pendiente:**

1. **Investigar el termcode 3 residual.** La dirección exactamente plana ya no
   está, pero el optimizador sigue parando ahí. Antes de tocar nada, leer
   `drtran/docs/OPTIMIZER_STOPPING_STUDY.md`: tres arreglos ya se probaron y se
   rechazaron con evidencia, y la pregunta «¿termcode 3 es el óptimo o mal
   condicionamiento?» está abierta a nivel de suite, no sólo aquí.
2. **Ampliar la validación del `-lrtest`** a los demás casos de `benchmark/` con
   rango reproducido por `ca.jo`, ahora que hay uno que funciona.
3. **Revisar los `.inp` de `data/`** uno a uno: sólo `AL.inp` está identificado
   como layout antiguo; del resto no consta.
4. **§5.11**, el enigma de la columna EML de Mauricio, como tarea de fondo.

Nada de esto toca el motor ni el optimizador. En particular **`elf()` no se
toca**: es la AS 311 publicada y refereada, y está validada aquí a 2·10⁻⁸.
