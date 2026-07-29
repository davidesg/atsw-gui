# drtran — TODO & plan de desarrollo

**drtran** es el **puente** entre **fue** (modelos univariantes) y **drvarma**
(verosimilitud exacta VARMA). Lee dos modelos especificados en fue y los estima
**conjuntamente**, con todos los parámetros a la vez, usando el `elf` de drvarma
**sin modificarlo**. Sobre esa base se construyen los modelos de transferencia
de Box–Jenkins.

Revisado 2026-07-12. Ver `BRIDGE_DESIGN.md` para el diseño y la evidencia.

## BUGS ABIERTOS

- [x] **CORREGIDO (informe, era ALTO) — la RESPUESTA AL IMPULSO invertía el signo
      de los términos no líderes del numerador, y con ella la GANANCIA.**
      Arreglado el 2026-07-29 en `drtran.c:1371` (la recursión de `nu`) y en
      `drtran.c:1405` (el gradiente, que tenía el mismo signo y hay que mantener
      COHERENTE o los errores estándar salen mal). Verificado: con `-s 1` el
      informe pasa de publicar ν₁ = −0.010792 y ganancia 0.005610 a ν₁ = +0.010792
      y ganancia **0.027195**, que es ω₀−ω₁ y coincide con lo que calcula el
      puerto de Python. Batería: **296 PASS**, con una sección 10 nueva que fija
      el signo de ν₁ y la ganancia para s>0 — el caso que la batería no cubría y
      por el que el defecto sobrevivió.
      **La documentación tenía el mismo error**: la ecuación de la ganancia en
      `docs/drtran-note.tex` también sumaba los ω. Código y documento se
      confirmaban mutuamente. Corregidos los dos, y añadida la convención BJR
      como ecuación explícita.
      Descripción original:
      Encontrado 2026-07-29 validando el puerto a Python contra este binario.
      El cast usa la convención de Box-Jenkins, `omega(B) = w0 - w1 B - ...`:
      `compute_irf` (tran_shootx.c:37) hace
      `sum = (lag == 0) ? omega[0] : -omega[lag]`, y el cast empotrado lo mismo
      (tran_shootx.c:196-198). Pero el bloque de IMPULSE RESPONSE de
      drtran.c:1371 hace `if (lag >= 0 && lag <= sn) acc = xf[slot + lag];`
      **sin negar**. Reproducción, `-b 0 -r 0 -s 1 -S` sobre el caso canónico:

          omega1[0] = 0.016402      omega1[1] = -0.010792
          nu que reporta el informe : 0.016402, -0.010792   <- suma
          nu que usa el cast (BJR)  : 0.016402, +0.010792   <- resta

      Que el cast es el correcto está comprobado: el puerto a Python usa la
      convención BJR y reproduce el log-likelihood de este binario a 1e-9
      (-718.183933) en las cuatro combinaciones de b/r/s probadas.
      **Impacto:** la columna CUMULATIVE del informe ES la ganancia. Publica
      0.005610 (= w0 + w1) donde la ganancia real es w0 - w1 = **0.027194**, un
      factor de casi 5. Afecta a todo s > 0; con s = 0 no se nota.
      El propio `.cns` de m6 dice cuál es la buena: «nu_num(1)=0, i.e. en BJR
      w0 - w1 - w2 - w3 = 0 => omega3[0] = omega3[1] + omega3[2] + omega3[3]».

- [ ] **SOSPECHA (mismo origen, SIGUE ABIERTA) — la MEDIA del cast empotrado suma
      los omega sin alternar el signo.** Nota: la sección de gain/mean lag
      (`drtran.c:~3188`) SÍ usa la convención correcta (`sk = (k==0)?1.0:-1.0`),
      así que este es el único sitio que queda por revisar. `build_embedded_varma` (tran_shootx.c:288) calcula la
      ganancia que multiplica la media de la entrada con
      `for (kk = 0; kk <= lnk[k].s; kk++) w1 += omega[k][kk];`, es decir
      w0 + w1 + w2 + ..., cuando en BJR omega(1) = w0 - w1 - w2 - ...
      Con s = 0 coinciden, así que no se ve en el caso canónico. Habría que
      comprobarlo con s > 0: la diferencia entre `-V` y `-S` con s=1 es 0.103
      (-718.287406 vs -718.183933) y no está separado cuánto de eso es el
      truncamiento (esperado) y cuánto este signo.

## El objetivo, en una frase

Pasar de modelos **univariantes** en fue a modelos **multivariantes de
transferencia** (Box–Jenkins), empezando por el caso **bivariante**, sin
reimplementar nada: fue pone la especificación univariante, drvarma pone la
verosimilitud exacta, drtran pone el *cast* que los conecta.

```
Y_t  =  ν(B) X_t  +  N_t ,      ν(B) = ω(B)/δ(B) · B^b
```

recast como VARMA bivariante diagonal para que `elf` lo puntúe:

- serie 1 = `w_Y − transfer_t` (el ruido N_t), AR/MA = el ARMA de Y;
- serie 2 = `w_X` (la entrada), AR/MA = el ARMA de X;
- covarianza diagonal `diag(var_Y, var_X)`;
- todo el acoplamiento vive en `transfer_t = Σ_j ν_j · w_X(t−j)`.

Con ω = 0 el modelo se parte en dos univariantes independientes — y **ahí está la
prueba de que el puente es correcto**.

---

## M0 — Homologación con fue (LA PUERTA DE ENTRADA) ✅ CERRADA (2026-07-12)

> **Criterio:** estimar conjuntamente dos modelos de fue con estructura diagonal
> y **sin transferencia** debe reproducir a fue ejecutado sobre cada serie por
> separado. Mientras esto no cierre, ningún resultado de transferencia es creíble.

Caso canónico: `ES_CPI_m10` (AR(1) + 11 armónicos + media, 1ª dif. de log×100)
y `WTI_ar1` (AR(1), media 0, 1ª dif. de log×100).

Referencia fue: φ_N = 0.402839 · μ_Y = 0.154472 · σ²_Y = 0.062666 · logL = −7.3917
                φ_X = 0.299193 · μ_X = 0 (fijada) · σ²_X = 68.8381 · logL = −760.0326
                **suma logL = −767.4243**

- [x] **M0.1 — Diferenciar las series.** `CalcNonsOp` descartaba el polinomio de
      diferencias y devolvía solo los factores de frecuencia fija: **ningún modelo
      con d>0 o D>0 se diferenciaba**. Se estimaba sobre niveles. → convolución
      `op = -(pol1 · pol3)`. *(F1 en BRIDGE_DESIGN §8)*
- [x] **M0.2 — Escala.** El `refactor` de fue **multiplica** la serie transformada
      (`w = 100·Δlog`), no divide el dato crudo. Deja σ² en O(10), que es el rango
      en el que el optimizador puede trabajar. *(F2)*
- [x] **M0.3 — Usar el `elf` de drvarma sin tocar.** Eliminar el parche
      `(diag_cov ? 1 : varmax.m)` de `drvmlest.c`, que volvía impropia la
      verosimilitud concentrada y empujaba las varianzas a cero. *(F3)*
- [x] **M0.4 — Arranque de varianzas** en la varianza muestral de cada `w`. *(F4)*
- [x] **M0.5 — Medias por serie.** Cada serie hereda del `.pre` su media y su
      condición de libre/fija: fue marca con un flag las que estima
      (`0.154472 1`) y escribe un simple `0` para las que no forman parte del
      modelo. Antes había un `fix_mu` **global** que ignoraba el `.pre`. De paso se
      eliminó una mina en `fue_pre_reader.c`: una rama `hardcoded` que, para
      **cualquier** serie mensual con 11 deterministas, clavaba el φ de ES_CPI
      (0.402839) y μ = 0 en vez de parsear el fichero. Ahora se parsea de verdad
      (incluida la línea de `Ndelta`, que no se consumía).
- [x] **M0.6 — HOMOLOGACIÓN VERIFICADA.** Estimación conjunta diagonal sin
      transferencia (5 parámetros):

      | | drtran conjunto | fue separado |
      |---|---|---|
      | φ_N | 0.402839 | 0.402839 |
      | φ_X | 0.299193 | 0.299193 |
      | μ_Y | 0.154472 | 0.154472 |
      | μ_X | fijada en 0 | fijada en 0 |
      | **logL** | **−767.424341** | **−767.4243** |

      La verosimilitud conjunta reproduce la **suma** de las dos univariantes a 4
      decimales. Las SE de φ también coinciden (0.0622/0.0650 vs 0.0623/0.0650).
      Con los 11 armónicos libres (`-D`, 16 parámetros) el óptimo y el logL son los
      mismos: los deterministas convergen de vuelta a los valores de fue.

      Comprobaciones colaterales: pass-through `Y = X` recupera **ω₀ = 1.000011**
      (antes: 0.00003); con transferencia contemporánea ω₀ = 0.0149 (t = 8.0) y el
      logL sube a −739.01 (razón de verosimilitudes ≈ 57 con 1 g.l.).

- [x] **M0.10 — Deterministas especificados por el `.pre`.** drtran parsea ahora
      **todos** los tipos de fue: `impulse`, `compimp`, `step`, `ramp`, `easter`,
      `trend`, `cos`, `sin`, `alter` (con sus fechas: `impulse <periodo> <año>`),
      reutilizando `DateToObs`/`ObsToDate`/`Easter` del propio motor. Además:
      - **estructura racional** ω(B)/δ(B) por variable (`Nomega`/`Ndelta`), tanto
        en el parseo (fue escribe **un coeficiente por línea**, no todos en una)
        como en la estimación: `shootx` desempaqueta ω y δ.
      - **cada coeficiente es libre o fijo según su flag en el `.pre`**
        (`Imega`/`Ielta`), igual que la media. Ya no hay interruptor global: el
        modo por defecto es la **estimación conjunta completa** (16 parámetros en
        el caso canónico). `-D`/`-E` pasan a significar "fijar TODOS los
        deterministas" de Y/X.
      - **guarda de estabilidad de δ(B)**: el filtro 1/δ(B) es recursivo y con δ
        fuera del círculo unidad la contribución determinista explota (colgaba el
        optimizador). Se rechaza vía `chekma`, que usa la misma convención.
      - Las variables **no estándar** quedan fuera **por diseño**: son una versión
        rudimentaria de un modelo de transferencia con input X, que es justo lo
        que drtran estima bien. Se emite un error que remite a ω/δ/b.

- [x] **M0.7 — Reportar Σ = `sigma2 · Q`.** `Q` **no es la covarianza**: es la
      covarianza *normalizada*; la real es `Σ = sigma2 · Q`, con `sigma2` el factor
      que `est` concentra. drtran imprime ahora ambas, igual que drvarma
      (`drvarma.c:1548-1558`). Resultado: **Σ[1,1] = 0.062666 y Σ[2,2] = 68.838114**,
      exactamente los σ² de fue. Ver `BRIDGE_DESIGN.md §8b`: **no reparametrizar `qq`**.
- [x] **M0.8 — Errores estándar. CERRADA (2026-07-12).** La premisa era falsa: no
      era drtran quien fallaba en la SE de μ. Al investigarlo aparecieron **dos bugs
      reales** en las SE, y de paso una anomalía en fue.

      **(a) El hessiano venía acumulado por BFGS, no calculado en el óptimo.**
      `est` invertía el `mtmp` que `raxopt` acumula a lo largo de la trayectoria.
      Eso dirige la búsqueda, pero no es la curvatura en el óptimo: dependía del
      camino (dos arranques daban SE distintas para el mismo óptimo) y se degradaba
      en las direcciones más planas. Ahora se recalcula con `fdhess` + `choldcp`
      **en el óptimo** — la alternativa que el propio `drvmlest.c` dejaba comentada.

      **(b) `Q` tenía la escala redundante ⇒ hessiano SINGULAR.** `est` concentra
      `sigma2` (Σ = sigma2·Q), así que la escala global de Q **no está
      identificada**; meter `var_Y` y `var_X` como dos parámetros libres dejaba una
      dirección exactamente plana. El BFGS lo ocultaba; el hessiano exacto lo
      destapó con **SE de 4·10⁵** en `Q[2,2]`, y esa singularidad contaminaba las SE
      de φ. Ahora se normaliza `Q[1,1] = 1` y se estima `log(var_X/var_Y)`: un solo
      parámetro, bien escalado y positivo. **No se toca `elf`**, solo el cast de
      `shootx`. La logL no cambia (−767.424341093880); solo desaparece un parámetro
      que no estaba identificado (15 en vez de 16).

      Resultado — las SE cuadran con el GLS exacto sobre los datos:

      | | drtran | fue | GLS/teoría |
      |---|---|---|---|
      | SE(φ_N) | 0.062204 | 0.062333 | 0.062421 |
      | SE(φ_X) | 0.064980 | 0.064979 | 0.065075 |
      | SE(μ) | **0.028502** | 0.073304 | **0.028502** |
      | SE(det f=1) | 0.068328 | 0.056673 | 0.068328 |

      **(c) fue arrastra el MISMO bug del hessiano BFGS.** *(Corregido 2026-07-12: la
      primera lectura fue errónea. Se dijo que "la SE de μ de fue está mal", mirando
      una sola salida. No es que fue calcule mal la SE: es que la calcula de forma
      **inestable**.)* Hay dos salidas de fue del mismo `ES_CPI_m10`, con
      estimaciones puntuales **idénticas** y SE **radicalmente distintas**: SE(μ) =
      0.073304 en una y 0.028316 en otra; y en los deterministas la primera se
      equivoca por un factor **4-5×**. Es la firma exacta del hessiano acumulado por
      BFGS: depende del camino del optimizador. fue usa el **mismo `drvmlest.c`**,
      con las mismas dos líneas de `fdhess` comentadas. Documentado con evidencia y
      arreglo en **`atws/fue/fue-1.13.1/ERRORES_ESTANDAR.md`**. Afecta a la
      inferencia del estudio de inflación (qué armónicos entran en el modelo).

      **(d)** Es plausible que el caveat de drvarma sobre los SE/Wald cruzados poco
      fiables (`MODELS_RESULTS.md` §4) tenga **esta misma raíz** (hessiano BFGS +
      escala redundante). Merece comprobarse allí.
- [x] **M0.9 — Flag propio de "sin transferencia" — HECHO** (revisado jul-2026). Es
      **`-0`** (`drtran.c:3873`: `no_transfer=1` ⇒ `s_ord[j]=-1`), documentado en la ayuda
      ("NO transfer: fit the two univariate models jointly and diagonally") y con ejemplo
      ("homologation with fue"). El truco `-s -1` sigue funcionando como equivalente de bajo
      nivel, pero `-0` es el flag limpio. La nota "(hoy solo vía `-s -1`)" era anterior a `-0`.

## M1 — Honestidad del programa ✅ CERRADA (2026-07-12)

- [x] **Convergencia real.** Antes se imprimía
      `"OPTIMIZER CONVERGED after %d iterations"` pasando `maxits` (500) e
      ignorando `ifault`: **mentía siempre**. Resulta que `qnewtopt.c` ya calculaba
      el número real de iteraciones y el código de parada, pero `report()` estaba
      enteramente condicionado a `quiet_mode` y no imprimía nada. Ahora expone
      `opt_iters` / `opt_termcode` y drtran informa con honestidad, distinguiendo
      convergencia (termcode 1-2) de parada (3-5) y avisando si `ifault != 0`.
      *Dato:* el caso canónico **converge en 18 iteraciones**, no en 500.
- [x] **`test_battery.sh` reescrita.** La anterior **certificaba los bugs**:
      esperaba `ω₀ ≈ 0` en el pass-through cuando con Y = X la verdad es `ω₀ = 1`,
      y daba PASS. La nueva se apoya en tres fuentes de verdad —homologación con
      fue, verdad sintética (`tests/gen_synthetic.py` simula `Y = ν(B)X + N` con
      b=2, r=0, s=1, ω=(0.8, 0.4)) y pass-through exigiendo ω₀ = 1— más los tipos
      de determinista y el rechazo de las no estándar. **23 PASS, 0 FAIL.**
      Recuperación de la verdad sintética: ω₀ = 0.785, ω₁ = 0.406, φ_N = 0.276,
      φ_X = 0.529, Σ = (0.245, 1.068) — todo dentro del error de muestreo.
- [x] **`transfer_weights[5000]`.** Era un buffer de pila sin comprobar contra
      `n_stat`. Ahora se reserva dinámicamente, y **después** de
      `recompute_stationary_series()`, que es quien fija el `n_stat` definitivo
      (antes se reservaba `transfer` con un `n_stat` que podía cambiar después).

## M1b — Casos reales de SF_MEG ✅ CERRADA (2026-07-12)

Se incorporaron a la batería los tres tipos de modelo del estudio de inflación
(`SF_MEG/empirical/cases/`), copiados a `tests/cases/`. Cada uno ejercita una
parte del motor que **nunca se había probado**, y entre los tres destaparon seis
bugs de fondo:

| caso | estructura | qué ejercita |
|------|------------|--------------|
| `ES_CPI_m10` | AR(1) + 11 armónicos + media, ∇ | estacionalidad determinista completa |
| `ES_CPI_airAR_mu` | (1−φB)(∇∇₁₂ ln y + μ) = (1−ΘB¹²)a | Box–Jenkins airline: **D=1** y **MA anual** |
| `ES_CPI_airline` | ∇∇₁₂ ln y = (1−θB)(1−ΘB¹²)a | MA regular + MA anual, sin media |
| `ES_CORE_S3` | AR(3) + AR₁₂ + MA de frecuencia fija (f=3), ifadf[3]=1 | **MEG**: factor irreducible + frecuencia fija |
| `ES_CORE_S135b` | AR(3) + MA de frec. fija en **f=3, f=1 y f=5** | c₁ ≠ 0 y de **ambos signos** (+1.66 / −1.67) |
| `FR_CPI_f5` | MA de frec. fija (f=5) + **AR(1) fijado en 0** | c₁ = −1.69 y **flags de coeficiente fijo** |

Los seis homologan con fue **al sexto decimal** (coeficientes y Σ).

**Robustez al arranque (test añadido).** El óptimo debe ser un *atractor*, no un eco
de los valores iniciales — el pecado original del proyecto era justo ese. Se
perturban las preestimaciones ARMA del `.pre` de `ES_CORE_S3` (AR 0.19/0.14/0.21 →
0.05, MAf −0.95 → −0.50) y el modelo **converge por gradiente al mismo óptimo**, a
6 decimales. Esto explica además el `termcode 3` que reportaba "NO CONVERGIÓ":
arrancando en las preestimaciones de fue (que ya **son** el óptimo) la búsqueda
lineal no puede mejorar y para; no es un fallo. La clasificación se corrigió:
1-2 = convergencia, **3 = parada sin mejora (normal si se arranca en el óptimo)**,
4-5 = fallo real.

**Bugs que destaparon:**

- [x] **El lector solo parseaba los factores AR regulares.** AR anual, MA regular,
      MA anual y los de frecuencia fija se declaraban pero **no se leían** (puntero
      reservado, sin órdenes ni coeficientes) → *segfault* en cuanto el modelo
      tenía MA o estacionalidad. drtran **nunca había soportado** esos modelos.
- [x] **El lector era orientado a líneas; fue usa `fscanf`** (agnóstico al
      espaciado). Se portó fielmente el lector de fue (`fue.c` [3.2]-[3.3]), así que
      cualquier `.pre` válido para fue lo es ahora para drtran.
- [x] **Los factores ANUALES se expandían en el retardo 1**, no en múltiplos de
      `sper`: un MA anual de orden 1 es (1 − ΘB¹²), no (1 − ΘB).
- [x] **Los factores de FRECUENCIA FIJA estaban mal expandidos.** fue los
      parametriza con un único coeficiente libre c₂ (< 0) y **deriva** el término
      en B (`fue.c:4064`, `:4202`):
      `c₁ = 2·cos(2πf/s)·√(−c₂)` → `1 − c₁B − c₂B²` = `1 − 2r·cos(ω)B + r²B²`.
      drtran aplicaba solo `(1 − c₂B²)`. Con f=3 acierta por casualidad
      (cos(π/2) = 0), pero con f=1 o f=5 estaba mal. Añadida la guarda c₂ < 0.
- [x] **Diferenciación distinta entre Y y X.** Con ∇∇₁₂ la salida pierde 13
      observaciones y el WTI solo 1: drtran abortaba. Ahora `build_stationary_pair`
      recorta ambas a la **ventana común** (alinea por el final, que es alinear en
      el calendario).
- [x] **La impresión recorría los órdenes EXPANDIDOS** (`p_N`/`q_N`: un MA anual
      expande a 12) en vez de los parámetros libres de los factores → lectura fuera
      del vector y *segfault*.
- [x] **Los flags de los coeficientes ARMA se ignoraban.** fue lleva un flag por
      coeficiente (`Ia1`/`Ia2`/`Ia1f`, `Im1`/`Im2`/`Im1f`): `"0.0000  0"` es un
      coeficiente **FIJO**, no un valor inicial. drtran los estimaba todos. Lo
      destapó `FR_CPI_f5`, que fija su AR(1) en 0. Ahora se respetan, igual que ya
      se hacía con la media y los deterministas: **el `.pre` manda**.

**Nota:** las variables deterministas **no estándar** siguen fuera por diseño (son
una transferencia con input X hecha a mano). Y los `forecast_*.inp` de SF_MEG
**no** son `.pre` utilizables: están modificados para previsión.

## M2 — Identificación (preblanqueo + CCF) ✅ CERRADA (2026-07-12)

Además de operar sobre series sin diferenciar (arreglado en M0), la identificación
tenía un bug que la hacía **estructuralmente incapaz de encontrar nada**:

- [x] **La CCF estaba del revés.** Se calculaba `corr(a_{t+k}, β_t)`, que coloca la
      respuesta en los retardos **negativos**… y luego se buscaban los picos entre
      los **positivos**. Nunca encontraba nada. Ahora se usa la convención de
      Box–Jenkins, `r(k) = corr(β_t, a_{t−k})`: **k > 0 ⇒ Y responde a X con k
      periodos de retardo**.
- [x] **Se usa la CCF del motor** (`Ccf` de `diagnose.c`), llamada dos veces —
      `Ccf(a_X, β_Y)` da el lado k ≥ 0 (la transferencia) y `Ccf(β_Y, a_X)` el lado
      k < 0 (la retroalimentación)— y el **gráfico en caracteres** (`PlotCCF`), en
      vez de una tabla y un cálculo hechos a mano.
- [x] **Pesos de la respuesta impulso**: `ν̂(k) = r(k)·s_β/s_a`, que son
      preestimaciones directas de los ω.
- [x] **Chequeo de exogeneidad**: CCF significativa en k < 0 ⇒ posible feedback
      (Y → X), que invalida el modelo de una entrada. Se compara el número
      observado con el **esperado por azar** (5% de los retardos): con bandas al
      5%, "alguno significativo" salta casi siempre y el aviso sería inútil.
- [x] **El bloque significativo es el CONTIGUO desde b.** Tomar el último
      significativo de toda la CCF hacía que un pico espurio lejano (k=24) disparara
      `s=24` y reventara `MAX_S`.
- [x] **Propone varias especificaciones razonadas**, no una sola:
      **[A]** cada peso significativo como ω libre; **[B]** si la cola decae
      geométricamente, un denominador δ que la resume con un solo parámetro.

**Validación (en la batería).**

| caso | verdad | propuesta de drtran |
|---|---|---|
| sintético simple | b=2, r=0, s=1, ω=(0.8, 0.4) | **b=2, r=0, s=1** — y ν̂(2)=0.799, ν̂(3)=0.439 |
| sintético racional | b=1, r=1, s=0, ω=0.6, δ=0.6 | **b=1, r=1, s=0** — detecta la razón ~0.56 |
| ES_CPI ← WTI (real) | — | b=0, r=0, s=1 |

En el caso real el pass-through petróleo → IPC aparece en k=0 (r=0.492) y k=1
(0.310); los pesos ν̂ = (0.0150, 0.0094) preestiman los ω que luego da la máxima
verosimilitud (0.0164, 0.0108), y la logL sube de −767.42 a −718.18. Concuerda con
el estudio de drvarma (`MODELS_RESULTS.md` §4: β contemporáneo 0.0154, retardado
0.0104 para ES).

## M3 — Adecuación de la transferencia ✅ CERRADA (2026-07-12)

Cierra el ciclo de Box–Jenkins: **identificar → estimar → validar → reespecificar**.

La clave es que el cast ya entrega los dos residuos que hacen falta:
`a[·][2]` **es** la entrada preblanqueada (serie 2 = w_X con su propio ARMA) y
`a[·][1]` es la innovación del ruido. Su CCF **es** el test de adecuación: si
(b, r, s) es correcta, el ruido no puede conservar huella de la entrada.

- [x] `transfer_adequacy`: CCF ruido-vs-entrada preblanqueada, con el gráfico en
      caracteres (`PlotCCF`) y en la convención de Box–Jenkins.
- [x] **Portmanteau de la transferencia** (k ≥ 0, incluye el contemporáneo), con
      `ChiTestC` y g.l. = nº de correlaciones − parámetros de ν(B).
- [x] **Portmanteau de exogeneidad** (k < 0): detecta retroalimentación Y → X, que
      invalidaría el modelo de una entrada.
- [x] **Veredicto y guía de reespecificación**: si es inadecuada, dice en qué
      retardos el ruido conserva huella de la entrada y sugiere cómo ampliar ν(B).
- [x] El veredicto lo dicta el **test conjunto**, no un pico suelto: con bandas al
      5% se espera que ~1 de cada 20 retardos las cruce por azar, así que exigir
      cero significativos condenaba incluso a la especificación correcta. Mismo
      criterio en la identificación (M2).

**Validación (en la batería).**

| caso | (b, r, s) | veredicto |
|---|---|---|
| sintético, órdenes verdaderos | 2, 0, 1 | **ADECUADA** (p = 0.671); exogeneidad OK (p = 0.320) |
| sintético, órdenes MAL | 0, 0, 0 | **INADECUADA** (p = 0.0000) y señala **k = 2 y 3**, justo donde vive ν(B) |
| ES_CPI ← WTI (real) | 0, 0, 1 (identificados) | **ADECUADA** (p = 0.196); WTI exógeno (p = 0.915) |

En el caso real el ciclo completo cierra solo: la identificación propone
(0, 0, 1), la estimación da ω = (0.0164, 0.0108) y la validación confirma que el
modelo es adecuado y que el WTI se comporta como exógeno.

## M4 — Previsión ✅ CERRADA (2026-07-12)

`drtran Y.pre X.pre -f L` prevé Y (y X) L periodos, con bandas al 95%.

**Motor: el `forecast.c` de drvarma, sin tocar.** (El `include/forecast.h` de drtran
ya declaraba su firma exacta: alguien lo dejó preparado.) Se comprobó antes de
usarlo que su convención de índices es `w[tiempo][serie]`, coherente con su propio
`elf` — el `msfo.c` **legacy** usa la contraria (`w[serie][tiempo]`, y aloja
`matrix(1, m, 1, n)`); drvarma transpuso ambos de forma consistente.

- [x] El VARMA bivariante prevé sus dos series: la 1 es el **ruido** N, la 2 es la
      **entrada** w_X. De ahí se recompone la salida:
      `w_Y(n+l) = N̂(n+l) + Σ_k v_k·ŵ_X(n+l−k)`, con w_X **observada** para el pasado
      y **prevista** para el futuro. *Prever Y exige prever X*: esa es la diferencia
      con una regresión.
- [x] **Varianza del error, exacta.** Y se alimenta de dos fuentes de innovación,
      independientes por la covarianza diagonal:
      `w_Y = ν(B)·ψ_X(B)·a_X + ψ_N(B)·a_N`, luego con `g = ν * ψ_X`:
      `Var(l) = Σ_N·Σ_{i<l} ψ_N(i)² + Σ_X·Σ_{i<l} g(i)²`.
- [x] **Nivel**: se integra con el operador no estacionario general (`rnsop`, que
      cubre d, D y los factores irreducibles), se le suma el **determinista futuro**
      —que se *conoce*, no se prevé: son funciones del tiempo— y se deshace el
      reescalado y la Box–Cox. Para ello el lector ahora **guarda la especificación**
      de cada determinista (`Tm->detspec`), que antes se tiraba tras generar DataMat.
- [x] Bandas del nivel con los psi integrados: `U = u * ψ`, con `u` la respuesta
      impulso de `1/rnsop(B)`.

**Validación (en la batería).** La descomposición de la varianza da una predicción
precisa y no trivial: con retardo puro **b=2**, los dos primeros pasos solo usan X
**ya observada**, así que no cargan error de previsión de la entrada. Y se cumple
al cuarto decimal:

| l | sd(w) drtran | teoría |
|---|---|---|
| 1 | 0.4951 | √Σ_N = 0.4951 |
| 2 | 0.5136 | √(Σ_N(1+φ_N²)) = 0.5136 |
| 3 | **0.9610** | salta: ya entra el error de X |

En el caso real (IPC ← WTI, 12 meses) la previsión de enero cae **por debajo** del
último dato observado (82.02 vs 82.84): la caída de las rebajas que capturan los
armónicos deterministas, aplicados correctamente en fechas futuras.

- [ ] Informes homologables con fue/drvarma; empaquetado y documentación.

## M5 — Horizonte: más allá de una entrada

Los modelos escritos a mano de `../drv-source` (`m6-1/2/3`, decodificados en
`M6-*_DECODED.md`: sistema macro de 6–7 variables, ~60 parámetros) marcan el
techo: operadores factorizados y racionales, parámetros compartidos y fijos,
covarianza **no** diagonal, ecuaciones contemporáneas, diferenciación por serie.
drvarma puntúa *cualquier* estructura VARMA; la potencia estaba en el `shootx`
escrito a mano. El destino de drtran es **generar ese mapa automáticamente** —
un pequeño DSL parámetros → estructura VARMA— no solo ω/δ de una entrada.

- [x] Múltiples entradas (m = 1 + #entradas).
- [x] Parámetros compartidos/fijos (tabla de slots, `-c`).
- [x] **La RED**: un DAG de transferencias (`-n`). Una serie puede recibir
      transferencias y ser a la vez entrada de otra — que es lo que son de verdad
      los sistemas de Mauricio. El cast resta a cada serie lo que recibe; la
      previsión recorre la red en orden topológico; un ciclo se rechaza.
- [x] Covarianza **no diagonal** — HECHO (revisado jul-2026). El motor ya estima
      Σ general: diagonal `log(var_i/var_1)` con `var_1=1` (respeta M0.8),
      off-diagonal **crudas**, y **rechazo si Σ no es PSD** (elf → 1.0), la
      estrategia del propio Mauricio (§3). **m6 lo prueba**: el diagonal libera las
      **15** covarianzas y converge con Σ plenamente no diagonal (correlaciones EA·EC
      −0.41, EI·EU +0.35, EA·EI −0.31…); la red usa las 3 del legacy. Nunca ha fallado
      en los casos reales (el más correlacionado, −0.41, converge).
      > **Nota — el Cholesky no es necesario.** La línea original pedía reparametrizar
      > con Cholesky de diagonal normalizada para eliminar la región rechazada por PSD.
      > Es robustez, no capacidad: esa región **nunca ha mordido**, y el estudio del
      > init de Q (§10 de `M6_TABLA4_BASELINE.md`) ya mostró que la eficiencia del
      > optimizador **no** está en la parametrización de la covarianza (portar
      > `init_varma` apenas cambió las iter 334→323 y rompió un test → revertido; las
      > ~250-330 iter son propiedad de `qnewtopt`, no del arranque). Queda como refactor
      > OPCIONAL, solo si algún día aparece un caso que se atasque en el borde PSD.
- [ ] Puerto a Python reutilizando los paquetes `fue` y `drvarma` (drvarma 0.1.0
      está en PyPI y su motor de ML exacta es Python puro). **← el hito que mueve la
      aguja**, condicionado a la decisión de lenguaje (ver "Decisiones abiertas").

- [ ] **BUG DE ROBUSTEZ — el optimizador CUELGA con datos sin reescalar (refactor=1).**
      Descubierto revisando `DVR_rstar` (CPI_EA ← Brent, 2026-07-23). `drtran -0`
      (diagonal libre, ruido AR(2)×AR(2) = orden 26 expandido, 11 armónicos) sobre un
      `.pre` con **refactor=1** (Δlog ~0.002) **cuelga >2 min sin converger**; el
      **MISMO modelo y datos** con **refactor=100** (Δlog ~0.2) **converge en 23 iters,
      1 segundo**. Aislado limpio: μ y deterministas ×100, los AR son scale-invariant →
      la única variable es la escala. Es **condicionamiento numérico**: los gradientes
      de diferencias finitas del VARMA cast (paso ~6e-6) tienen relación señal/paso
      pésima a escala raw. Confirma la **guía de Mauricio: reescalar SIEMPRE a 100
      porque el optimizador trabaja mejor** (es la razón de M0.2). Evidencia
      reproducible: `ART/Data/cases/DVR_2026/drtran_work/{CPI,BRENT}_{r1,r100}.pre`.
      **A revisar (dos frentes):**
      1. **Por qué cuelga sin reescalar en vez de avisar/condicionar.** drtran no
         debería colgarse ante datos mal condicionados que lleguen con refactor=1: o
         **condiciona internamente** (escala data-driven, p.ej. `refactor≈1/std(∇BoxCox)`)
         o **rechaza/avisa** con un límite de iteraciones. Colgarse en silencio es el
         peor modo de fallo. Nota de diseño: el fix P1 de fue/ART (refactor=100 fuente
         única) hace que el `.pre` regenerado ya condicione ×100 → el hang desaparece al
         regenerar; pero el C subyacente sigue siendo frágil.
      2. **Armónicos + precisión insuficiente = mala situación de estimación.** Con la
         parte determinista estacional (armónicos cos/sin + Nyquist) presente, si la
         precisión de los datos/gradientes no alcanza, la superficie de verosimilitud
         queda mal condicionada (colinealidad determinista + AR largo) → estimación
         inestable. Revisar la interacción armónicos ↔ escala ↔ precisión del gradiente.

---

## M6 — El caso m6 corrido (pasos 1-4), y la escalera como PRODUCTO

El "techo" de M5 —los sistemas escritos a mano de `drv-source` (m6-1)— ya no es techo:
se corrió **m6 de punta a punta** con drtran, con la **Tabla 4 de Relloso (1997)** y el
**legacy m6-1 como regla de medida**. Detalle: `docs/M6_TABLA4_BASELINE.md`.

- [x] **Pasos 1-2 — univariantes y diagonal.** Los seis `.pre` grabados de la Tabla 4
      (representaciones de la **diagonal del legacy**: solo EA estocástica; EP/EI/EC ∇²
      determinista; P/EU ∇²), verificados uno a uno con fue. **6.º bug destapado: el
      signo de Nyquist en `CalcNonsOp`** (`pol4[1]` +1→−1, un carácter mal portado de
      `fue.c:4449`) rompía toda serie con raíz estacional en π (σ ~8×). Regresión añadida.
- [x] **Paso 2 leído.** Las 3 correlaciones contemporáneas fuertes = las 3 covarianzas
      que el legacy libera (EA·EI, EA·EC, EI·EU).
- [x] **Pasos 3-4 — la red.** DAG **EC→EU→EI→EP + EC→EP** decodificado de `drv.c`
      (`shootx`, NO `multshea.c`), montado con `-n`, validado contra el legacy: **EP←EI
      CLAVA** (0.750,−0.300 vs 0.78,−0.382). La verosimilitud gana ~12 (ℓ: −1709.6→−1697.6).
- [x] **La carencia de los PRODUCTOS, MEDIDA.** Los enlaces limpios (EP←EI) clavan; los
      de numerador factorizado + parámetro compartido (EP←EC, EI←EU, EU←EC) aciertan lo
      dominante pero no clavan. Es exactamente lo que se pierde sin los productos.
- [x] **Iteraciones — regla de medida.** El legacy convergió en **258 iter arrancando
      EN la solución** (F plano 1.000000→0.999659): las ~250-330 iter son propiedad del
      optimizador `qnewtopt`, no de la distancia de arranque. (Se probó portar el init de
      Q desde la cov. muestral de residuos como en drvarma `init_varma`: apenas cambió
      las iter y alteró crestas planas → revertido.)

### El producto: la escalera de la escuela, automatizada

m6 no es m6: es la **escalera metodológica de Treadway** (univariantes → MS diagonal con
covarianzas libres → leer CCF → añadir dinámica → validar). Convertirla en producto =
automatizar lo que hoy es manual. Candidatos, por palanca:

- [x] **(1) Identificación de red MULTIVARIANTE — HECHO (v1).** Flag `-i`:
      `identify_network` lee las ccf de los residuos del diagonal (doble preblanqueo) y
      propone el DAG (enlaces dirigidos + `b/s`) y las covarianzas contemporáneas.
      Validado en m6 (recupera las 3 covarianzas del legacy y el enlace limpio EI→EP b=1).
      Es una GUÍA de candidatos (poda por exogeneidad/aciclicidad/retardo). *Pendiente
      de refinar:* `r=1` (denominador racional), detección de ciclos, mejor filtrado del
      ruido de retardos lejanos.
- [x] **(2) PRODUCTOS en la tabla de slots — HECHO (v1).** El `-c` acepta ahora
      `x = [-]y * z`: un coeficiente ES el producto (con signo) de otros dos slots
      (`SLOT_PRODUCT`, resuelto en `expand_params`; el gradiente lo maneja `cdgrad` por
      diferencias finitas, sin regla de cadena). Reproduce los numeradores factorizados +
      compartidos del legacy: p.ej. `omega1[1] = -omega1[0] * theta_2[B^1]` = `-x5(1-x6B)`
      con `x6` compartido con la MA del input. Validado (bloque m6 EP←EI: el producto se
      impone exacto y quita 1 g.l.). **Cierra la carencia MEDIDA** (`M6_EJERCICIO.md` §6).
- [x] **(2-bis) La mini-expresión — HECHA (`SLOT_LINCOMB`, jul-2026).** El `-c` acepta ahora
      **combinaciones lineales** `x = [±]t1 [±]t2 …`, con cada término un slot **o** un producto
      `slot*slot`. Generaliza el producto a sumas/diferencias: cubre el factor **fijo `(1−B)`**
      de una FLT (`ν_num(1)=0` ⇒ `omega[0]=omega[1]+omega[2]+…`) y los coeficientes `y*z − w` de
      un numerador factorizado (`x12·x14 − x13`). Resuelto en `expand_params` (iterativo,
      gradiente por `cdgrad`); se imprime con su fórmula. Batería sección 7b (4 checks: (1−B)
      exacto, estructura completa no rechazada, camino producto-en-suma).
- [x] **(1-bis) La red m6 RE-CORRIDA con los productos — HECHO/MEDIDO (jul-2026).** Los 2
      enlaces de **MA compartida** impuestos: EP←EI `omega1[1]=omega1[0]*theta_2` (x6=MA de
      EI) y EU←EC `omega4[1]=omega4[0]*theta_3` (x16=MA de EU); en `tests/data/m6/m6_net_prod.cns`.
      **Hallazgo:** correr libre-todo cae a ℓ=−1729 y se atasca — NO es el producto, son las
      **compuestas débilmente identificadas** (mode-hopping, §5) que dominan la ℓ. Fijando los
      deterministas (`-D -E`, Relloso) la comparación aísla la transferencia: **los 2 productos
      cuestan solo Δℓ=0.4** (estructura consistente) y mueven el ω₁ hacia el legacy (EP←EI
      0.255→0.330 vs 0.382; EU←EC 0.016→0.104 vs 0.31). Batería +4 (sección 7).
      **Estructura COMPLETA** (con la mini-expresión (2-bis)): 2 productos + el `(1−B)` de
      EI←EU (`omega3[0]=omega3[1]+omega3[2]+omega3[3]`), en `tests/data/m6/m6_net_full.cns`:
      ℓ=−1731.4, 24 par libres, Δℓ=2.5 sobre 3 g.l. ⇒ **LR no rechazada** (χ²(3)₀.₉₅=7.81,
      p≈0.17). EP←EC queda libre por diseño (x12,x13,x14 locales, sin g.l. que ganar).
      Batería 276 PASS. Detalle: `M6_TABLA4_BASELINE.md` §11.
- [x] **(3) La escalera como driver guiado — HECHO (`-g NAME`, jul-2026).** Como `-i`, pero
      además **escribe** `NAME.dag` y `NAME.cns` listos para `-n`/`-c` (las covarianzas con
      índices numéricos del triángulo inferior `q[i,j]`, i>j — no los nombres que `-c` no lee)
      y emite el **plan** de la escalera con el comando exacto para estimar. Deja al usuario
      confirmar/podar y estimar (la red identificada es una guía, no la final — la doctrina de
      la escuela). En m6 identifica las 3 covarianzas del legacy (q[3,2], q[5,2], q[5,4]) y el
      **round-trip** (red podada + el `.cns` escrito) reproduce la ℓ canónica −1697.6. Batería
      sección 9 (4 checks). Documentado en la ayuda y `examples/m6/`.
- [x] **(4) Empaquetar m6 — HECHO (jul-2026).** `examples/m6/` con `run.sh` (recorre la
      escalera de punta a punta: univariantes → diagonal → `-i` → red libre/productos/completa,
      imprimiendo la progresión de ℓ) y `README.md` (tutorial). **Regresión del sistema
      completo** en la batería §8: los ℓ canónicos (diagonal −1709.5, red libre −1697.6, la red
      mejora ~12) guardan que todo el pipeline no deriva. Batería **280 PASS**.

### Auditoría del signo/normalización — EN CURSO

**Veredicto de la auditoría.** El signo `−` del empotrado (`tran_shootx.c:203`) es INTERNO
(la off-diagonal de la Φ del VARMA) y **NO se filtra a lo reportado**: drtran reporta la ω
de la transferencia, limpia. φ/θ/δ están en convención B-J (`1 − coef·B`). **La única
discrepancia:** el **numerador ω** se aplicaba con `+` (`ω₀ + ω₁B`), pero **fue lo aplica en
BJR `ω₀ − ω₁B`** (`calcnu`, fue.c:4505: `nu[j] = Σδ·nu[j-i] − ω[j]`; y el display de fue,
:1624-1637, lo confirma). Era un **bug de porting latente** — la homologación solo tiene
`Nomega=0`, así que el signo de los ω no-líderes nunca se probó; **m6 lo destapó** con las
intervenciones compuestas (donde se negaba ω₁ a mano para compensar).

- [x] **Verificado:** el motor de fue (`calcnu`) es BJR (`ω₀ − ω₁B`).
- [x] **ω DETERMINISTA corregido** a BJR (`drtran.c:623`, `fue_pre_reader.c:363`): el líder
      suma, los demás restan. `build_m6.py` graba ahora los ω₁ de Relloso DIRECTOS (sin la
      negación-parche). Test-safe (batería 263 PASS; m6 da el mismo ℓ, consistente).
- [x] **ω de TRANSFERENCIA a BJR** (`compute_irf`, `build_embedded_varma`, y la
      caracterización `transfer_characteristics`: ganancia `g=ω(1)` y retardo medio). El
      líder suma, los demás restan. La **respuesta ν no cambia** (física), solo la
      parametrización: la data sintética es byte-idéntica; `gen_synthetic.py` etiqueta la
      verdad en BJR; los tests de ω₁⁺ pasan a negativo. m6 da el MISMO ℓ=−1697.6 (pura
      reparametrización). Batería 263 PASS.
- [x] **Documentada** la convención (ω BJR) en la ayuda (`-b/-r/-s`).
- [x] **Chequeos de signo en la batería** (`test_battery.sh`): guardas de que la ganancia
      `g=ω(1)` NO es la del convenio + (0.4 en SYN), ni el retardo medio (1.0); y de que la
      respuesta `ν(3)=−ω₁` sale con el signo físico.
- [x] **CIERRE SINTÉTICO — caso determinista (`SYND.pre`).** Intervención COMPUESTA (escalón
      con dos ω deterministas) sobre input AR(1); verdad ω₀=10, ω₁=6 en BJR. Es el camino que
      la homologación de m6 (todo `Nomega=0`) NUNCA ejercía: el **numerador determinista**.
      drtran recupera ω₀=9.91, **ω₁=+5.86 (POSITIVO)** — con el bug `+` saldría −5.86. Bloque
      `2h-bis` en la batería (3 checks, incl. guarda `ω₁>0`). Sub-bug encontrado y corregido:
      `build_intervention` grababa un banner de 3 líneas y el lector salta EXACTO 5 → leía
      `nobs=0`; banner alineado a 4+blanco como `write_pre`. Batería **268 PASS, 0 FAIL**.
- [x] **Tabla §9 de `M6_TABLA4_BASELINE.md` actualizada** a los ω BJR (re-corrida la red:
      ℓ=−1697.6; ω₀ idénticos, ω_k k≥1 con signo BJR en ambas columnas; verdictos intactos).
      También §5/§6: la ganancia determinista `g=ω₀−ω₁` (Relloso directo, no `ω₀+ω₁_fue`).

**Auditoría CERRADA:** todos los operadores (φ, θ, δ, ω) en convención Box-Jenkins
normalizada; el `−` del empotrado es interno y no se filtra. drtran coincide con fue. El
signo del **numerador determinista** queda blindado con un caso sintético de verdad conocida
(`SYND`), además de las guardas de la ganancia/retardo en la transferencia.

## Decisiones abiertas

- [x] **Lenguaje — RESUELTO (22-jul-2026): PUERTO A PYTHON.** El C queda como
      **implementación de referencia**, funcionalmente terminado y **homologado** (292 PASS/0
      FAIL, cero TODO/FIXME reales, CLI completo, `make install`, nota técnica) — no era "camino
      corto pendiente de homologar", ya homologa. El puerto se hace por PRODUCTO (paquete-hermano
      de la suite ATSW en PyPI), reutilizando `fue` (spec univariante + lector `.pre`) y
      `drvarma` (motor de ML exacta en **Python puro**, PyPI 0.1.0). **Arranca la sesión del
      2026-07-23.** El C es el oráculo de regresión: cada paso del puerto reproduce el C al
      decimal sobre la batería. Ver `M6 — puerto a Python` (arriba) y `examples/m6`, `m6_EA`.
- [ ] **Interfaz**: ficheros `.pre` vs. objetos `fue.Model` vs. ambos. *(A fijar al arrancar
      el puerto.)*
- [ ] **ARMA conjunto o fijo**: por defecto libre (estimación conjunta, que es el
      objetivo) con opción de fijar en los valores de fue. *(Traslada la decisión del C, donde
      el default es libre con `-N`/`-X` para fijar.)*

## Notas

- `xitol = -1e-3` selecciona la verosimilitud **exacta** (no la aproximada) en `elf`.
- El paso de diferencias finitas de `cdgrad` es `eta^(1/3)·max(|x|,1)` ≈ **6e-6
  absoluto**: cualquier parámetro por debajo de ~1e-4 es invisible para el
  optimizador. De ahí que la escala de las series (M0.2) sea crítica.
- La covarianza diagonal y el AR/MA diagonal son lo que permite que la
  verosimilitud exacta se factorice y homologue con fue. Mantener esa estructura.

---

## Posible, NO objetivo: un estudio de comparación de estimadores

**El objetivo del proyecto es un drtran funcional a partir del legacy y un puerto a
Python, como drvarma. No es un artículo.** Un programa que se presenta como
*contribución a la literatura* es un pasivo: hay que defenderlo, y se vuelve
experimental. Un programa que se presenta como *implementación cuidadosa de métodos
publicados* es un activo: se puede usar. La nota técnica está encuadrada así
(«Nothing in this note is new») y ahí debe quedarse.

Dicho eso, del estudio de §mcstudy salieron cosas que **podrían** sostener una nota de
estadística computacional. Se apuntan con su alcance honesto, no como plan.

### Lo que le falta al estudio para sostenerse

Tal como está, compara la **ML exacta contra un truncamiento propio**. Es una
conclusión anunciada, y un evaluador lo diría: la literatura YA tiene un método exacto
para estos modelos — espacio de los estados con el estado inicial del filtro estimado
(Gómez 2019, §3.2.1, campo `ser.inc`). Faltan **dos** comparaciones:

1. **Contra la ML exacta por Kalman** (SSMMATLAB). La eficacia debería ser
   *idéntica* — los dos son exactos. La comparación se vuelve entonces puramente
   **computacional**, que es donde sí tenemos algo:
   - **O(n²) frente a O(n)**: el cast por resta convoluciona la transferencia en cada
     evaluación de la objetivo; el empotrado no convoluciona nada. Se cruzan, y a
     n=400 el empotrado es 2× más rápido. Es complejidad, no estadística.
   - La **realización no mínima** (el factor común D(B) en |P| y en |M|) y su coste en
     convergencia cerca de la raíz unitaria: 85% con δ=0.95.
2. **Contra el método condicional / de retropredicción de Box–Jenkins**, que es lo que
   TASTE hace y lo que la gente usa. Ésa es la comparación con contenido práctico.

### Sobre las asintóticas: se espera un resultado NULO, y se sabe por qué

El truncamiento afecta a un número de observaciones que **no crece con n**: su
contribución a la log-verosimilitud es O(1) frente a O(n). Luego el estimador
condicional y el exacto son **asintóticamente equivalentes** — es el resultado estándar
de ARMA (condicional vs exacta) y se traslada. **Un estudio asintótico confirmaría que
no hay nada que ver.**

Lo que sí tendría contenido es la asintótica **local a la unidad**, δ = 1 − c/n: ahí el
tramo contaminado es una *fracción fija* de la muestra y la diferencia **no se
desvanece**. Y es exactamente lo que se observa: la celda δ=0.95, n=400 es la **única**
en la que la ventaja sobrevive a una muestra grande (−60% en el RMSE de la ganancia).
Ese es el indicio. Sería un proyecto de teoría econométrica en serio, no un apéndice.

