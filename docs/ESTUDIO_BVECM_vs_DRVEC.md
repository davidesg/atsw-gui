# `drv_project` / BVECM frente a `drvec`: estudio comparado

*Estudio del código experimental `~/Dropbox/SRC/drv_project` y del artículo que
lo acompaña (`Article_Multivariate Convergence/cointegration_convergence/Legacy/
BVECM_models.pdf`, «Cointegration Analysis with Gradual Adjustment Dynamics: A
Unified Framework from Triangular Representation to Behavioral Error Correction
Models»), contrastados con la refactorización de `drvec` documentada en
`ANALISIS_PRELIMINAR.md`.*

**Fecha:** 2026-08-17.

El estudio en sí no modifica código. La única acción que salió de él y ya está
aplicada es **§3.5 (r = 0 en `-lrtest`)**; el resto de §6 sigue pendiente.

---

## 0. La tesis, en una frase

Los dos programas estiman **la misma familia de modelos por la misma
verosimilitud exacta** (la AS 311 de Mauricio), pero **recorren la
transformación de Mauricio (2005/2006) en sentidos opuestos**. Esa es la
intuición que había sobre la mesa, y es correcta. Además, el sentido que recorre
el legado es **el mejor condicionado de los dos**, lo que lo convierte en la
pista más concreta que tenemos para el termcode 3 residual.

---

## 1. Las dos direcciones

| | `drvec` | `drv_project` / BVECM |
|---|---|---|
| **Qué parametriza `x[]`** | el modelo **VEC**: Λ, B₂, F_i, Θ_i, Σ | el **VARMA estacionario** sobre Ȳ: φ, θ, Σ, más β, μ y convergencia |
| **Qué hace el cast** | VEC → VARMA: construye C̄, H̄, Ā, todos los Φ̄ᵢ (ecs. 10–18) **en cada evaluación** | nada de eso: escribe φ y θ **tal cual** desde `x[]` y construye `w` |
| **Cuándo se invierte** | nunca; los parámetros VEC ya son los estimados | **después** de converger, en `analisis_BEC()` |
| **Qué se obtiene al final** | Λ̂, B̂₂ directamente | φ̂, θ̂ → se derivan α, γ, Π, forma BEC |

`drvec` **casta hacia el VARMA en cada iteración**; el legado **estima el VARMA
y casta una sola vez hacia el VEC al final**. El artículo lo dice tal cual
(§3.1.3–3.1.4): la aplicación es *uno a uno*, así que estimar (13) por EML ya da
todos los parámetros de interés, y luego «the inverse transformation is applied
to recover the parameters in the original VEC or WARMA form».

**Dónde convergen:** en `elf()`. Los dos acaban entregando un VARMA estacionario
sobre el mismo Ȳ_t a la misma rutina AS 311. Son dos parametrizaciones de la
misma superficie de verosimilitud, relacionadas por un cambio de variables
biyectivo. **No compiten: son duales.**

---

## 2. Diccionario de correspondencias

Imprescindible antes de comparar un solo número entre los dos. Hay tres trampas.

### 2.1 El signo de β (trampa mayor)

| | normalización | relación de equilibrio |
|---|---|---|
| Mauricio (2006) / `drvec` | `B = [I_r ; B₂]` | `W_t = Y_{1t} + B₂'Y_{2t}` |
| BVECM / `drv_project` | `α = (I_r ; −β)` | `w_t = z_{1t} − β'z_{2t}` |

> **B₂ = −β.** El `B₂ = −0.2042` de la Tabla 4 de Mauricio es el mismo objeto
> que un `β = +0.2042` del artículo BVECM.

Compárese C̄⁻¹ en ambos: `drvec` (ec. 14) `[−B₂' I_r ; I_s 0]`; artículo (ec. 10)
`[β' I_r ; I_{m−r} 0]`. Idénticas bajo B₂ = −β.

### 2.2 El orden de Ȳ

- Artículo, ec. (9): `Ỹ_t = (Δz_{2t} ; w_t)` — **igual que `drvec`**.
- **El código del legado hace lo contrario**: `shootx` pone
  `w[t][1] = ECT` y `w[t][2] = ΔL`, es decir `(w_t ; Δz_{2t})`.

O sea que **el artículo y su propio código no coinciden en el orden**. Da igual
para estimar (es un reetiquetado del VARMA bivariante), pero invalida cualquier
comparación directa de matrices φ, θ o Σ entre los dos programas si no se
permuta antes.

### 2.3 El caso determinista

`shootx` fija `mu1[1] = μ` (media en la ecuación del ECT) y `mu1[2] = 0` (media
cero en la de Δz₂). Eso es **exactamente el caso 2 de Remark 6** —
`E[∇Y₂] = 0`, `E[W] ≠ 0`— cableado. El legado no tiene casos 1 ni 3.

---

## 3. Lo que el legado tiene y a `drvec` le falta

### 3.1 Una parametrización mejor condicionada — la pista para el termcode 3

Es el hallazgo importante. En `drvec`, §2.1 de `ANALISIS_PRELIMINAR.md`
estableció que **B₂ es el parámetro caliente**: toca C̄, C̄⁻¹, todos los Φ̄ᵢ, Θ*,
`qq` **y los `nobs`×M datos**. Un solo parámetro acoplado a todo.

En el legado, β entra **sólo por los datos** (`w[t][1] = A − ν(B)ξ − β·L`), y
φ, θ, Σ son parámetros VARMA planos que no dependen de β en absoluto. El
acoplamiento que hace dura la superficie de `drvec` **es en buena medida un
artefacto de castear en dirección VEC→VARMA**.

Esto no demuestra que la parametrización dual elimine el termcode 3 —el artículo
avisa de lo contrario, §5— pero sí dice dónde mirar: **una variante de `drvec`
que optimice en coordenadas VARMA y convierta al final** es una hipótesis
concreta y comprobable, no una intuición.

### 3.2 Rejilla concentrada para el parámetro no lineal

`preestimar_parametros` busca el operador de convergencia así: para cada
φ ∈ {0.01,…,0.99}, construye `z_t = Σ_k φ^k X_{t−k}`, obtiene **α en forma
cerrada** por mínimos cuadrados (`α = Σz·ect / Σz²`) y se queda con el par de
menor RSS. Es decir: **concentra analíticamente lo que puede y busca en rejilla
sólo el único escalar genuinamente no lineal**.

`LEGACY_NOTES.md` §3 despachó esto como «ineficiente». **Es una lectura
equivocada**: 99 evaluaciones de un problema unidimensional con el otro
parámetro concentrado es barato y robusto, y es justo el patrón que `drvec`
podría usar para B₂ — el parámetro que más daño hace.

### 3.3 β fijo y luego libre (homotopía) — APLICADO en parte el 2026-08-17

`global_beta_fijo` fija β = 1 y estima el resto. El artículo lo prescribe
explícitamente (p. 7): empezar con q = 0 y ajustes AR escalonados, luego añadir
MA, y **«relax the restrictions that fix the values of the CI coefficients …
and jointly estimate all parameters»**.

`drvec` no tenía nada de esto: arrancaba en frío con todo libre. **Añadido
`-fixb2`** (2026-08-17), que quita B₂ del vector de parámetros:

- `-fixb2 <v>` lo fija en un valor **a priori**, con lo que
  `2·[L(libre) − L(fijo)]` **sí** es un test LR válido, χ² con s·r grados de
  libertad. Es como Mauricio (2006, Tabla 5) contrasta `B = [1,0]′`, que en esta
  parametrización es `-fixb2 0`.
- `-fixb2` a secas lo fija en el valor OLS estático de `init_guess`. Sirve de
  arranque en caliente y de diagnóstico de condicionamiento, pero la restricción
  la elige el dato, así que el LR **no** sería un test válido — y la salida lo
  dice explícitamente.

Efecto medido sobre `mink_muskrat` (p=2, q=1, r=1, caso 2):

```
libre        OPTIMIZER STOPPED   53 iter   logL  6.4679   B2 = -0.2392
-fixb2       OPTIMIZER CONVERGED 49 iter   logL  5.2137   B2 = -0.1910
-fixb2 0     OPTIMIZER CONVERGED 106 iter  logL -7.4652   B2 =  0
             -> LR contra el libre = 27.87, 1 gl: se rechaza B2 = 0
```

**El ajuste restringido converge donde el libre se para en termcode 3**, que es
justo el síntoma que se quería atacar. Lo que **falta** es la segunda pierna de
la homotopía: reusar la solución restringida como semilla de una pasada con todo
libre. Eso exige poder sembrar `x[]` desde fuera de `init_guess`, y es la
continuación natural.

### 3.4 Valores críticos por Monte Carlo para el test de rango

El artículo (§6.3–6.5) es tajante: bajo H₀ la distribución **no es χ²** y
recomienda **bootstrap paramétrico** — simular N muestras bajo H₀ con los
parámetros estimados, reestimar H₀ y H₁ en cada una, y tomar los percentiles
empíricos como valores críticos de muestra finita.

Mi `-lrtest` (§4.4) usa valores asintóticos de λ-max sacados de `urca`. El
artículo dice que eso es la aproximación, no lo correcto, sobre todo porque:
(i) hay componentes MA bajo ambas hipótesis, y (ii) el operador de convergencia
añade términos deterministas que desplazan la distribución en muestra finita.
**Es una mejora identificada y bien especificada para `-lrtest`.**

### 3.5 r = 0 sí es expresable — APLICADO el 2026-08-17

El artículo define H₀ como «w_t = Δz_t, es decir, no se forma ninguna
combinación cointegrante» — o sea **r = 0**, estimando un VARMA sobre las
diferencias. Yo puse en `-lrtest` que «r = 0 queda fuera de lo que este layout
puede expresar». **Con el layout en niveles ya no es cierto**: r = 0 da s = M,
C̄ = I, H̄ = 0, Λ y B₂ vacíos, y Ȳ = ∇Y. El guardián `if (global_r < 1)` es lo
único que lo impide.

**Comprobado, no supuesto.** Levantando ese guardián en una copia instrumentada
(sin tocar el repo), r = 0 estima sin más: la secuencia queda completa, es
**monótona en r** como exige el anidamiento, y **los tres ajustes convergen** en
el sistema bien condicionado:

```
mink_muskrat (M=2, caso 2)   r=0  logL  -6.3311  CONVERGED
                             r=1  logL   6.4679
   LR(0->1) = 25.598   crít. (M-r=2) 13.75 / 15.67 / 20.20  -> rechaza al 1%
   => r >= 1, que es el rango publicado por Mauricio y el del banco.

UK consumption (M=3, caso 2) r=0  logL 522.4086  CONVERGED
                             r=1  logL 557.4726  CONVERGED
                             r=2  logL 570.2297  CONVERGED
   LR(0->1) = 70.128   crít. (M-r=3) 19.77 / 22.00 / 26.81  -> rechaza al 1%
   LR(1->2) = 25.514   crít. (M-r=2) 13.75 / 15.67 / 20.20  -> rechaza al 1%
   => r = 2, que es el rango que reporta `ca.jo`.
```

Es decir: **añadir r = 0 no sólo completa el test, sino que da por primera vez el
contraste que de verdad importa** —«¿hay cointegración?»— y lo hace acertando en
los dos casos con rango conocido. Sale prácticamente gratis.

*(Con `-case 1` la secuencia sale al revés, L(0) > L(1), pero eso es la
configuración de caso 1 que ya sabemos que no converge; con `-case 2` el
comportamiento es el correcto.)*

### 3.6 La vuelta: formas BEC y Π, y exogeneidad débil

`analisis_BEC()` invierte hacia los parámetros interpretables, y
`calcular_chi2_weakex` / `calcular_chi2_joint` hacen tests de Wald de
exogeneidad débil por el método delta con pseudoinversa SVD. La aplicación al
trigo (Tabla 2 del artículo) los usa para concluir que Londres es débilmente
exógena frente a Viena y Estrasburgo pero no frente a Arnhem y Pensilvania.

`drvec` no produce nada de esto: da Λ̂ y B̂₂ y para. Como `drvec` **ya estima en
coordenadas VEC**, la mitad del trabajo está hecha: lo que falta es la
triangularización P (Σ = PDP′) que descorrelaciona las innovaciones y da las
ecuaciones desacopladas.

### 3.7 El operador de convergencia

`w̃_t = w_t − c_t`, con `c_t = ν(B)ξ_t^{t*}`, `ν_k(B) = ω_k(B)/δ_k(B)` sobre un
escalón en `t*`. Entra como **regresor determinista** dentro de la relación de
cointegración; el Corolario 3 del artículo prueba que no altera la equivalencia
WARMA–VEC. En `drvec` encajaría como una ampliación del bloque de media.

---

## 4. Lo que el legado **corrobora** de nuestro diagnóstico

Tres confirmaciones independientes, y una de ellas es casi una confesión.

**4.1 — El `/100` de la covarianza.** `preestimar_parametros` siembra
`x[idx++] = sigma11/100`, y lo mismo σ₁₂ y σ₂₂, en las dos ramas. **Es una
división arbitraria por 100 de la semilla de covarianza.** Sólo se sostiene
porque el objetivo es exactamente invariante a la escala de `qq` (§3.1 del
análisis): alguien ajustó empíricamente un factor que la verosimilitud no puede
ver, para que el optimizador se portara mejor. Es la evidencia más directa de
que **la dirección plana de §3.5/§3.6 es real, se sufría, y se estaba
compensando a mano sin saber por qué**.

**4.2 — El mismo triángulo completo de Σ.** `calcular_nparametros` cuenta 3
parámetros de covarianza para m = 2, igual que `drvec` antes de §3.6, y la
«normalización» de `shootx` es la de Φ₀/Θ₀ (que aquí es la identidad), no una de
escala. Así que el legado **arrastra exactamente el mismo parámetro no
identificado**. Ya son tres programas de la suite con el mismo defecto:
`drvarma`, `drvec` (corregido) y `drv_project`.

**4.3 — El artículo lo dice, aunque no lo llame así.** p. 7: *«One will normally
find high correlations between the estimated CI parameters, which would appear
to suggest lack of definition in the estimation situation, but this is mostly
unavoidable in this context. **Multicollinearity appears to be natural to
CI**.»* Es la lectura benigna del termcode 3 residual, escrita por los autores:
tras quitar la dirección exactamente plana, lo que queda es mal condicionamiento
**intrínseco al problema**, no un defecto del programa. Concuerda con lo que
`drtran/docs/OPTIMIZER_STOPPING_STUDY.md` §5 observó para `drvarma`.

---

## 5. Lo que **no** conviene tomar del legado

- **Está cableado a m = 2, r = 1.** `armax->m = 2` literal en `shootx`, columnas
  fijas `datamat[t][1..4]`, MA restringida a θ₂₂(1). `drvec` ya es general en
  M y r; eso no se toca.
- **Un solo caso determinista** (el 2). `drvec` cubre los tres de Remark 6.
- **`datamat` con 4 columnas fijas** (dL, A, L, X). El `data/AL.inp` de `drvec`
  es una versión reducida a 2 columnas del mismo caso del trigo: **no son
  intercambiables**.
- Es **código experimental**, con bloques comentados a medias (dummies en la
  relación de cointegración, `global_modelo_completo`) y `x[3]=0;` comentado
  suelto. Se estudia por sus ideas, no se copia.

---

## 6. Acciones concretas que salen de aquí

Por orden de relación beneficio/riesgo:

1. ~~**Permitir r = 0 en `-lrtest`** (§3.5).~~ **HECHO el 2026-08-17.** El
   bucle recorre ahora r = 0..M−1; hubo que hacer tolerantes a dimensión cero
   las asignaciones de Λ, B₂, W y E[W], y saltar la regresión de `init_guess`
   cuando `nreg = 0` (que ocurre con r = 0 y p ≤ 1). Valgrind limpio en esa
   ruta degenerada. `r = 0` sigue rechazándose para una estimación de rango
   único: es una hipótesis nula, no un modelo que se quiera ajustar con
   `drvec` — para un VARMA sobre diferencias está `drvarma`.
2. **Valores críticos por bootstrap paramétrico** en `-lrtest` (§3.4), como
   alternativa a los asintóticos que ya están. El artículo especifica el
   procedimiento completo.
3. **Segunda pierna de la homotopía** (§3.3): `-fixb2` ya existe y su ajuste
   converge; falta encadenar una pasada libre sembrada con esa solución. No
   toca el optimizador, que es la restricción que impone
   `OPTIMIZER_STOPPING_STUDY.md`.
4. **Rejilla concentrada para B₂** en `init_guess` (§3.2), con Λ resuelta en
   forma cerrada para cada B₂ candidato. Sustituiría el arranque en frío actual.
5. **Formas BEC/Π y tests de exogeneidad débil** (§3.6), cuando haya estimación
   fiable. Es lo que convierte a `drvec` en algo interpretable económicamente.
6. **Investigar la parametrización dual** (§3.1) como hipótesis para el
   termcode 3 residual: optimizar en coordenadas VARMA, convertir al final.
   Es el experimento más informativo y también el más caro.

El operador de convergencia (§3.7) es la extensión natural una vez lo anterior
esté firme, y es donde los dos proyectos **convergerían de verdad**: `drvec`
aportaría la generalidad en M y r y la transformación verificada; el legado, la
capa interpretativa y la dinámica de ajuste gradual.

---

## 7. Corrección a `LEGACY_NOTES.md`

`LEGACY_NOTES.md` §3 dice de la rejilla de convergencia: «The legacy grid search
with 100 φ values and K=20 lags is inefficient. A better approach would use
`scipy.optimize.minimize_scalar` … or a golden-section search».

**Conviene matizarlo.** La rejilla no es una búsqueda ingenua: α se concentra
analíticamente y sólo φ se recorre. Cambiarla por una búsqueda escalar local
sacrificaría la propiedad que la hace valiosa —**es global en φ**— en un
problema donde el RSS perfectamente puede ser multimodal. 99 evaluaciones de un
producto escalar no son un cuello de botella.
