# drvec: los procesos del análisis, y lo que el porte y su MCP tienen que hacer

*Escrito el 2026-10-11 desde la práctica: la revisión empírica del artículo de
dolarización de Ecuador (7–11 oct. 2026), donde drvec se usó para todos los
sistemas —rango, VEC adoptados, contrastes de α y β, exógenas, respuestas al
impulso con bandas, robustez— y se generaron las tablas del artículo desde los
modelos. Cada proceso dice para qué sirve, qué decide, cómo se lanza, qué se lee
en el `.out`, dónde se equivoca uno y qué tiene que hacer la herramienta del MCP
que lo sustituya.*

*No repite lo que ya está escrito: el modelo y su teoría están en
`engines/drvec/docs/` (`MODEL.md`, `THEORY.md`, `INFERENCE.md`, `USAGE.md`), lo
que el porte tiene que cargar en `REVIEW_2026-09-23.md` §5, las decisiones de
modelo previas al porte en `ESTUDIO_MAURICIO_2026-09-23.md` §5, y el método de
porte del precedente en `drtran-python/docs/PORTE.md`. Esto es la otra mitad:
**cómo se usa el programa en un análisis real**, que es lo que el MCP tiene que
saber hacer.*

---

## 0. El oráculo

| | |
|---|---|
| **referencia** | etiqueta **`drvec-v0.12`** de atsw-gui (`4a89c00`, `main`), «the oracle for the Python port»: la 0.11 con BUG-41 y BUG-42 cerrados, `<base>.boot` y goldens de la superficie del porte (VERSIONS.md, 0.12) |
| cómo se construye sin tocar el árbol de trabajo | `git archive drvec-v0.12 engines/drvec lib \| tar -x -C <dir>` y `make` en `<dir>/engines/drvec`. En el proyecto del artículo lo hace `Python/art_drvec/tables/build_drvec_ref.sh`, en `~/.local/share/atsw/<etiqueta>/`, con el enlace `~/.local/share/atsw/drvec-ref` a la etiqueta en uso y la etiqueta y el hash en `SOURCE`. **Fuera de Dropbox**: un binario construido dentro lo borró la sincronización justo después de compilarlo (§4) |
| sha256 del binario (Linux, gcc, GSL del sistema) | `9cf9d8ea…9aea5d76` (0.12); `c40c6a02…dbadfec1a` (0.11) |
| lo que **no** es el oráculo | `SRC/drvec-ARCHIVADO` (la copia autónoma, 0.10 + MEJORA-1..3 + `-irfboot`, sin publicar), ni `drvec-v0.11`, que fue el oráculo durante un día |

La revisión se hizo con la copia autónoma (`SRC/drvec`, 0.10, rama
`feature/irfboot`, `94bcffb`). El 2026-10-11 se **repitió entera con
`drvec-v0.11`** y salió **idéntica**: los 71 ajustes, los 3 bootstraps de rango,
el `-irfboot` y las tablas (§9). Ese mismo día la 0.12 pasó a ser el oráculo.
Cambia la diagnosis de residuos (la Q de Hosking y el JB), así que la réplica se
repite con ella (§9).

**Regla para el MCP**: cada `.out`, cada tabla y cada resultado cacheado lleva
la versión y el hash del motor que lo produjo. Durante la revisión convivieron
dos copias de drvec con motores distintos (`drvmlest.c` y `qnewtopt.c`), y sólo
se supo porque alguien preguntó cuál se estaba usando.

---

## 1. El material de entrada y los convenios

**Un `.pre` por serie**, el óptimo del univariante adoptado en art, como en
drtran:

```
drvec s1.pre … sM.pre  p q r  [opciones]  -name BASE
```

- **Orden de las columnas** (el error más caro de todos): los **primeros M − r**
  ficheros son el bloque ∇Y₂ y los **últimos r** son el bloque Y₁, en el que se
  normaliza β. En un bivariante con r = 1, **la variable normalizada va la
  última**: `GY.pre GAP.pre` normaliza en GAP y da θ = −β_{G/Y}. Si se cambia el
  orden se obtiene otro modelo, igual de válido pero con otra lectura, y no da
  ningún error.
- drvec **resta a cada serie sus deterministas** (escalones, impulsos,
  estacionalidad determinista) y aplica su Box-Cox y su reescalado, tal y como
  vienen en el `.pre`. Las series se alinean por fecha en la muestra común.
- **El ARMA del `.pre` no se impone** al VEC: la dinámica la estima drvec con p
  y q, y del `.pre` sólo pueden salir valores iniciales (la diagonal del MA,
  `load_seed_pre`). El orden p se decide a partir de ese ARMA (§3.2).
- **Unidades**: 100·ln en todo el estudio (`refactor` en el `.pre`). Las
  respuestas al impulso y las semividas salen en esas unidades.
- `q = 0` en todos los sistemas del artículo. El MA del VEC queda para
  `-specs`, y su clase por defecto ha cambiado entre versiones (VERSIONS 0.11).

**Qué se lee en el `.out`** (lo que `tables/vecm.py:parse` extrae, y por tanto
lo que el porte tiene que escribir igual o servir estructurado):

| línea / bloque | contenido |
|---|---|
| `logelf :` | logL exacta **final** (la que se compara) |
| `npar :` | número de parámetros |
| tabla `Parameter … Estimate` | estimación, s.e., t, p de cada parámetro, hasta `Signif. codes` |
| `alpha matrix` | α (M × r) |
| β normalizado | el bloque B₂ y la identidad en Y₁ |
| `r M-r LR 10% 5% 1%` | test de rango asintótico (`-lrtest`) |
| `r M-r LR 10% 5% 1% p-value reps` | el mismo con bootstrap |
| Hosking Q(k), Jarque-Bera multivariante | diagnosis |
| Wald de exogeneidad débil y de exclusión | se imprimen siempre; **no se citan** (§3.6) |

---

## 2. El recorrido completo

Es el orden que siguió la revisión en cada sistema. Cada paso queda escrito en
una bitácora (`revision_empirica_2026-10.md`), con su evidencia y la decisión
del analista:

```
.pre adoptados (art)
  │
  ├─ 3.1 puerta: el escalón diagonal reproduce los univariantes (identidad de cruce)
  ├─ 3.2 orden p          (desde los univariantes)
  ├─ 3.3 caso determinista (2 / 3 / pre / -nodrift / -trend)
  ├─ 3.4 RANGO            -lrtest -bootstrap N        ← el proceso largo
  ├─ 3.5 ajuste adoptado  r fijo, -fdhess
  ├─ 3.6 restricciones    -weakex / -alpha / -fixb2 / -fixb2row, por LR
  ├─ 3.7 exógenas         -xpre + -xlink (resta)  |  -xsys (empotrada)
  ├─ 3.8 cantidades derivadas  θ, semivida, diagnosis
  ├─ 3.9 respuestas al impulso y FEVD con bandas  -irfboot N   ← largo
  ├─ 3.10 robustez        caso, univariantes alternativos, submuestra, recursivo
  └─ 3.11 tablas, fichas y réplica
```

---

## 3. Los procesos

### 3.1 La puerta: la identidad de cruce

**Para qué.** Antes de creer nada del sistema hay que comprobar que la ruta
`.pre` transporta bien los univariantes. Con r = 0 y todo diagonal, el sistema
**es** M modelos univariantes, y la logL de cada serie tiene que ser la de su
`.out` de art.

```
drvec GY_M1.pre GAP.pre 4 0 0 -case 2 -diagar -diagma -diagcov -name gate_p4_c2
```

**Qué se lee.** La logL de cada serie en el escalón diagonal frente a la de su
`.out` de art. En la revisión (TODO A11) coincidieron con diferencias ≤ 1e-8
(G/Y −223.41885, GAP −64.02644, PE, PU, GYB). Hay un caso que enseña por qué la
puerta es necesaria: **GYB tiene deriva y sólo cruza con `-case pre`**, porque el
caso 2 no la admite. Con `-case pre` el escalón más bajo es, por construcción,
el de los univariantes.

**MCP.** Es lo primero que hace `load_pre`, como en mtram («prove the bridge»).
Si falla, la herramienta **se niega** a seguir y dice qué serie no cruza.

### 3.2 El orden p

**Regla del analista** (adoptada en la revisión): **p = orden AR del univariante
en ∇ + 1**. Se reduce si el último Γ no es significativo. El rango es sensible a
sobreparametrizar: con p de más, el test pierde potencia. En el artículo, el
bivariante central usa p = 4 y los sistemas fiscal y de balanzas p = 2. Los
`-lrtest` con p = 1…4 se corrieron para dejar documentada la sensibilidad.

**MCP.** `propose_order` lee el ARMA de cada `.pre`, propone p con la regla
escrita y enseña el último Γ. La decisión es del analista.

### 3.3 El caso determinista

| opción | E[∇Y₂] | E[W] | cuándo se usó |
|---|---|---|---|
| `-case 2` | 0 | ≠ 0 | bivariante central, aumentado, variante B, WTI |
| `-case 3` | ≠ 0 | ≠ 0 | robustez del trivariante |
| `-case pre` | **la que diga cada `.pre`** | ≠ 0 | trivariante, fiscal, balanzas («derivas libres») |
| `-nodrift i` | anula la deriva de la serie i dentro del caso 3 | | |
| `-trend` | caso 4: tendencia en W restringida a β | | validación (solo un ajuste: sin `-lrtest` ni `-f`) |

**Para qué en la práctica.** La tabla de validación (`tab:validation`) se basa
en esto: el mismo contraste (A, B, C, §3.6) bajo el caso 3, con las derivas
libres y con tendencia. Si el resultado cambia al cambiar el caso, eso es un
hallazgo y va al texto.

**MCP.** `-case pre` es el valor por defecto razonable para la ruta `.pre`, y la
herramienta tiene que explicar la elección. `-trend` es incompatible con
`-lrtest`, `-f` y `-estwin`. El C lo rechaza y el MCP también tiene que
rechazarlo, con el motivo.

### 3.4 El rango, por bootstrap paramétrico

**Por qué no basta la tabla asintótica.** La LR de la ML exacta no es la de
Johansen. Bajo H₀ está desplazada (≈ −log T, `ESTUDIO_MAURICIO` §5), y a n ≈ 120
el test secuencial sobrerrechaza varias veces su nivel nominal (HOMOLOGATION
2.3). Con derivas libres (`-case pre`) no hay tabla publicada. **Toda decisión
de rango del artículo se toma con el bootstrap**, y la tabla asintótica sólo se
imprime al lado.

```
drvec GY_M1.pre GAP.pre 4 0 0 -case 2 -lrtest -bootstrap 999 -name biv_rank
```

`-lrtest` ajusta r = 0, …, M−1 e informa de 2[L(r+1) − L(r)]. Con
`-bootstrap N`, para cada comparación r → r+1 (`drvec.c`, `bootstrap_rank`,
`simulate_h0`):

1. **El modelo bajo H₀** es el ajuste observado con rango r. B₂ sale del final
   de x[] (o de `B2_fixed` con `-fixb2`). Σ* = σ²·Q se completa evaluando `elf`,
   porque `vec_shootx` no lo rellena.
2. **Simular**. El modelo ajustado es un VARMA **estacionario sobre Ȳ**. Se
   simula ahí con el convenio de `elf`,
   (w − μ)ₜ = Σ φⱼ (w − μ)ₜ₋ⱼ + aₜ − Σ θⱼ aₜ₋ⱼ, con aₜ = L·z (L la Cholesky de
   Σ*), z normales, y un **calentamiento de 50 + 10p** observaciones que se
   descartan.
3. **Volver a niveles invirtiendo la transformación** desde el **nivel inicial
   real**: ∇Y₂ = Ȳ[1..s], Y₂ acumulando desde `rawmat[1]`, y Y₁ = W − B₂′Y₂. La
   muestra simulada tiene el formato de los datos y se reestima por **la misma
   ruta que los datos**: no hay código paralelo que pueda desviarse.
4. **Reajustar con r y con r+1**, cada uno con el arranque de los datos (la
   escalera en todos los rangos, BUG-50). Un arranque peor que el de los datos
   metería L(r) en un óptimo local, inflaría LR* y subiría los críticos.
5. **Se guardan todas las extracciones que convergen, también las LR
   negativas**. La LR exacta no está anidada en Λ = 0 y sale negativa bajo H₀ en
   torno al 40 % de las muestras. Quitarlas censuraba la nula por abajo
   (BUG-26). Las réplicas en las que falla alguno de los dos ajustes se
   descartan y se cuentan en la columna `reps` (en el trivariante, 491 de 499 en
   r = 1 → 2).
6. **Críticos**: ordenar y tomar el índice ⌈(1−a)·nok⌉, acotado. Antes se
   redondeaba al más cercano, lo que daba un 2 % donde se pedía un 1 %.
   **p-valor** = (#{LR* ≥ LR_obs} + 1)/(nok + 1). Con menos de 10 réplicas
   útiles no se informa.
7. **El veredicto se lee en los críticos, no en el p-valor**, porque el p-valor
   tiene un suelo de 1/(B+1). El `.out` imprime el error Monte Carlo,
   √(p(1−p)/B): a p = 0.05 y B = 499 es 0.0098, así que un p-valor cerca del
   umbral es «indeciso», no una decisión.

**El generador, que es parte del oráculo.** Un LCG propio de 64 bits
(`s ← s·6364136223846793005 + 1442695040888963407`, u = ((s ≫ 33) & 0x7FFFFFFF)/2³¹,
con u₁ > 1e-12) y Box-Muller con el **segundo normal guardado** (`spare`). La
semilla es fija: `boot_rng = 987654321`. Tres consecuencias para el porte:

- **El estado no se reinicia** entre comparaciones. r = 1 → 2 sigue la corriente
  donde la dejó r = 0 → 1, y el `spare` pasa de una réplica a la siguiente.
- Los números se consumen todos en la simulación, y los ajustes no consumen
  ninguno. Por eso una réplica cuyo ajuste falla ha gastado sus T·M normales
  igual que una buena. Una simulación que no se puede hacer (Cholesky de Σ* no
  definida) sale **antes** de consumir. Por réplica, el orden es: T = n + 50 +
  10p instantes, M normales en cada uno, t por fuera.
- `-irfboot` usa **el mismo generador y el mismo estado global**. Hoy empieza
  siempre en la semilla, porque `-irfboot` no se combina con ningún modo que
  consuma antes (`-lrtest`, `-matest`, `-artest`). Si el porte deja combinarlos,
  las bandas dependerán de lo que haya corrido antes.

Desde la 0.12, `-bootstrap` escribe cada réplica en `BASE.boot` (CSV): rango
de H₀, número de réplica, `ok`/`simfail`/`fitfail` y LR* con 17 dígitos.
Escribirlo no consume números del generador. Es lo que permite homologar el
porte réplica a réplica (§6).

Por eso un bootstrap es **determinista**: con las mismas entradas, opciones y
binario sale idéntico byte a byte. En eso se apoya la caché (§3.11) y es la
prueba de la réplica (§9).

**B y coste.** B = 499 por defecto en el artículo, y **B = 999 en el sistema
central**, donde importa poder resolver un 1 %. Cada réplica son dos ajustes
completos con escalera. El coste crece sobre todo con M y p, más que con B: con la
v0.11, en una máquina de 4 núcleos compartida, el bivariante central (p = 4,
B = 999, una comparación) tardó 13 min, el trivariante p = 2 (B = 499, dos
comparaciones) 26 min, y el aumentado (M = 3, p = 4, B = 499) 82 min (§9). Se lanzan **en segundo plano, uno por
núcleo**, y nunca se repiten si nada ha cambiado.

**La decisión.** r̂ se lee en los críticos al 5 %, **y la teoría puede
prevalecer** si se escribe. En el trivariante el bootstrap da r = 2 en el límite
(p = 0.035 con 491 réplicas, a 1.5 errores Monte Carlo del 5 %), y el analista
adoptó **r = 1 por teoría** (TODO D2). La tabla del artículo enseña los dos
números y el r adoptado.

**MCP.** `rank_test` es una **tarea asíncrona**: devuelve un identificador, se
consulta con `job_status` y su resultado se cachea con una clave. Informa de la
tabla completa (asintótica y bootstrap, `reps` incluido) y del error Monte
Carlo, y **no decide el rango**: lo propone, y la decisión del analista con su
razón queda en el guion (§5).

### 3.5 El ajuste adoptado

```
drvec GY_M1.pre GAP.pre 4 0 1 -case 2 -fdhess -name biv_free
```

- **`-fdhess` siempre que se vaya a citar un error estándar.** Los s.e. por
  defecto vienen de la aproximación BFGS del optimizador y **no son** los del
  hessiano en el óptimo. Pasó lo mismo en art con el CLI de fue (BFGS) frente a
  fue de Python (fdhess): las tablas se rehicieron por eso.
- **α se lee en la matriz `alpha matrix`.** Con `-weakex`, el ψ que se imprime
  lleva **el signo contrario**.
- **Signos**: α negativo en la variable normalizada corrige el desequilibrio;
  en las demás corrige un α del signo de −β. Con β = (1, −1): α > 0 en el
  débito y α < 0 en el crédito.
- **θ** del sistema central = −β_{G/Y}, con β normalizado en el GAP.

### 3.6 Restricciones sobre α y β: LR, no Wald

Los Wald que el `.out` imprime siempre se calculan sobre un coeficiente
normalizado y **dependen de en qué serie se normalice**. Todas las restricciones
del artículo se contrastan con la LR = 2[L(libre) − L(restringido)]:

| opción | restricción | ejemplo |
|---|---|---|
| `-weakex i` | αᵢ = 0 (exogeneidad débil) | central: α_{G/Y} = 0, LR 1.55 (p 0.213) → **modelo del texto** |
| `-alpha FILE` | α con forma dada (filas a cero) | trivariante: sólo GAPc ajusta (`A_gap.txt`: `3 1` y la columna `0 0 1`) |
| `-fixb2 v` | todo B₂ = v (**con valor**; sin valor no es un contraste) | fiscal y balanzas: β = (1, −1), LR con s·r gl |
| `-fixb2row i v` | una fila de B₂ = v; ajusta también el libre e informa de la LR | trivariante: homogeneidad β_PU = −1 |
| combinaciones | contraste conjunto | `-fixb2row 2 -1 -weakex 2` (contraste C) |

La validación (`tab:validation`) es **A** = α_PU = 0, **B** = β_PU = −1 y **C** =
ambas, bajo los casos 3, `pre` y `pre -trend`. LR con el caso 3: 0.30, 6.21
(p 0.013) y 6.27.

**Regla del analista**: **β = (1, −1) impuesto** en los VEC fiscal y de
balanzas, porque un vector distinto no tiene lectura económica. El libre se
estima y se informa de la LR, pero el modelo adoptado es el restringido.

**MCP.** `test_restriction` ajusta el par (libre y restringido) con la misma
escalera, devuelve LR, gl y p, y **se niega** a dar un p-valor con `-fixb2` sin
valor (el C ya lo avisa). Los Wald los enseña marcados como no citables.

### 3.7 Exógenas: por resta o empotradas

El despacho es el de drtran y depende del **orden de integración** de la
exógena frente a las salidas:

| caso | opción | qué hace |
|---|---|---|
| orden distinto (p. ej. BXE I(0) sobre GAP I(1)) | `-xpre X.pre -xlink red.txt` | Y_out − ω(B)/δ(B)·Bᵇ·X_in es el proceso VEC: **por resta, en niveles**; X queda congelada en su `.pre` |
| mismo orden (p. ej. IT I(1)) | `-xsys i` | la serie i entra en el sistema con αᵢ = 0 y sin el corto plazo de las demás; LR de exogeneidad fuerte |

- **`-xpre` y `-xlink` van juntas**: una sin la otra da error («-xpre and -xlink
  go together»). El fichero de red tiene una línea `OUT <- IN b r s` por enlace
  (p. ej. `GAP <- BXE 0 0 0`), con `r ≤ 2`.
- Con `-xlink` se ajusta primero el VEC sin entradas, ω = 0 tiene que
  reproducirlo (sale en el `.out`), y la LR frente a él es el contraste. Es un
  único ajuste, en el caso 2 o 3, por la ruta `.pre`.
- Lagunas declaradas en la v0.11 (`TODO.md` de drvec): `-xsys` con q > 0, el test
  de rango con exógenas y `-xlink` con `-f`.

**Lo que salió de aquí en el artículo**: el texto decía que NX es exógena y el
análisis lo contradice (las importaciones corrigen).

**MCP.** `add_exogenous` decide el despacho a partir de los `.pre` (d de cada
serie), lo dice y escribe la red. Hereda las negativas del C.

### 3.8 Cantidades derivadas

| cantidad | fórmula | dónde |
|---|---|---|
| θ | −β_{G/Y} (β normalizado en GAP) | central: 0.386 |
| s.e. de α y β | tabla de parámetros con `-fdhess` (`vecm.py:alpha_se`, `beta_se`) | |
| semivida | ln 0.5 / ln(1 − \|α\|) de la variable que corrige, en trimestres | central 6.0 t.; fiscal 1.4; comercio 2.6; c. corriente 2.3 |
| diagnosis | Q(32) de Hosking multivariante, JB multivariante | fiscal: JB p 0.00 (se dice en la nota) |

**La diagnosis cambió en la 0.12** (BUG-42, en `lib/diagmv`, que comparten
drvarma y drtran). La Q de Hosking se refiere ahora a χ²(m²s − k) con
s = freq + 2, cuando antes era χ²(m²s) con s = ⌊√n⌋. El JB es el de Lütkepohl, y
cada Ljung-Box resta sus propios coeficientes. Los p-valores de Q y JB de
`tab:vecm` se toman de la 0.12 (§9).

**MCP.** Las calcula `estimate`, que las devuelve **estructuradas**, sin obligar
a analizar el texto. `vecm.py:parse` existe porque el C sólo escribe texto.

### 3.9 Respuestas al impulso y FEVD, con bandas por bootstrap

```
drvec GY.pre PU.pre GAPc.pre 2 0 1 -case pre -alpha A_gap.txt -irfboot 999 -name tri_A_boot
```

- **Por qué no el método delta**: las respuestas en niveles son una cadena no
  lineal larga (pesos Ψ, el mapa a niveles por B₂, LDL′ de Σ) que converge al
  efecto permanente, cuya distribución cerca de la frontera no es normal. A
  estos tamaños de muestra, una linealización es justo lo que no hay que creer.
- **Qué hace**: simula N veces **el modelo ajustado** con el generador del rango
  (`simulate_h0`), **reajusta cada réplica como se ajustaron los datos** (mismo
  rango y mismas restricciones: `-weakex`/`-alpha`, `-fixb2row`, `-xsys`, la
  máscara de derivas, la misma escalera) y calcula respuestas y FEVD con el
  `level_irf_R` del informe. Las bandas son percentiles (Efron) al 90 y al 95 %.
  **Son condicionales a r.**
- **Salida**: en el `.out` y entera en `BASE.irfboot`, un CSV con
  `what,shock,response,k,point,p2.5,p5,p50,p95,p97.5`.
- **Negativas** (que el C mantiene y el porte tiene que mantener): no se combina
  con `-differenced`, `-xlink`, `-trend`, `-warma`, `-estwin`, `-writeinp`,
  `-writeres` ni con los modos `-lrtest`, `-specs`, `-rungs`, `-matest`,
  `-artest` y `-eval`.
- La descomposición transitoria y permanente (Gonzalo-Ng) del trivariante se
  calculó **fuera** de drvec, en Python, desde el ajuste
  (`plot_irf_transitory.py` → `irf_transitory.csv`). Es candidata a
  herramienta.

**MCP.** `impulse_response` (puntual e inmediata) e `irf_bands` (asíncrona,
como `rank_test`).

### 3.10 Robustez

Todas son corridas ordinarias. Lo que hay que conservar es **la lista de qué se
corrió y por qué**:

- **caso**: el central con los casos 2 y 3; el trivariante con 3, `pre` y
  `pre -trend` (`VECM_TRI/run_case*.sh`);
- **univariantes alternativos**: GY con M1 (escalón de la CFDD) o M2 (escalón e
  impulso), con o sin intervenciones (`GY_noint`, `GAP_noint`), la variante B
  (`GYB`), los deflactores del PIB (`VECM_GDPD*`);
- **orden**: p = 1…4 en el rango;
- **submuestra y estimación recursiva** de θ y λ (`VECM_SUB`, `VECM_REC`, con
  scripts de Python que llaman al binario en bucle);
- **otros países y medidas** (`VECM_SLV*`, `VECM_PCE`), y el WTI.

**MCP.** Un `sweep` que corra una rejilla de especificaciones sobre los mismos
`.pre` y devuelva la tabla (logL, LR, θ, α), con caché. Es lo que hacían a mano
los `run_*.sh`.

### 3.11 Tablas, fichas, caché y réplica

Lo que se montó en el proyecto del artículo y que el MCP tiene que poder hacer
sin scripts propios:

- **Registro de corridas** (`tables/vecm_registry.py`): cada corrida es
  `nombre → (carpeta, argumentos)`, y cada tabla es una lista de filas que
  apuntan a corridas.
- **Caché por clave** (`tables/vecm.py:run`): sha256 de los argumentos, de cada
  fichero que se lee (`.pre`, `-xpre`, `-xlink`) **y del binario**. Si la clave
  coincide no se repite; si cambia cualquier cosa, se repite. Como los
  bootstraps son deterministas, la caché es exacta, no aproximada. Cada corrida
  se hace en su propia carpeta (`build/drvec/<nombre>/`), porque drvec escribe
  `BASE.out` en la carpeta de trabajo y dos corridas en la misma carpeta se
  pisan.
- **Tablas LaTeX desde los modelos** (`make_tables.py`): `tab:rank`,
  `tab:vecm` y `tab:validation`, con un `.json` que tiene todos los números y una
  comparación con el borrador.
- **Fichas por sistema** (`make_docs.py`): la procedencia de cada `.pre` por
  contenido (qué univariante es), las corridas y sus resultados, y la sección de
  la síntesis correspondiente.
- **Réplica** (`replicate.sh`, `replicate_boot.sh`, `compare_replica.py`): se
  rehacen todos los ajustes con un binario dado y se comparan con los `.out`
  originales. Criterios: `logelf` a 1e-6, estimaciones a 1e-4 relativo, las
  estadísticas de los contrastes a 1e-4; los s.e. se enseñan pero no se juzgan.
  Los bootstraps tienen que salir idénticos.

---

## 4. Trampas encontradas (para el MCP: cada una es una comprobación)

| trampa | consecuencia | qué tiene que hacer la herramienta |
|---|---|---|
| orden de los ficheros | se obtiene otro modelo sin ningún error | pedir los roles (∇Y₂ / Y₁) explícitamente y repetirlos en la salida |
| ψ de `-weakex` con signo contrario | α con el signo cambiado en el texto | servir α desde la matriz, nunca desde ψ |
| s.e. BFGS | errores estándar citados que no son los del óptimo | `-fdhess` por defecto en todo lo que se vaya a citar |
| `-fixb2` sin valor | una «LR» que no es un contraste | negarse a dar el p-valor |
| Wald impreso | depende de la normalización | marcarlo como no citable y ofrecer la LR |
| p-valor del bootstrap cerca del umbral | falsa precisión | dar el error Monte Carlo y leer en los críticos |
| `reps` < B | réplicas que fallan | enseñar la columna y avisar si baja mucho |
| `-xpre` sin `-xlink` | error | pedir la red o construirla (§3.7) |
| dos copias del motor | resultados de binarios distintos | versión y hash en todo (§0) |
| dos corridas en una carpeta | `.out` pisados | carpeta por corrida |
| scripts con la ruta del binario escrita a mano | se rompen al archivar o mover el motor (24 scripts el 2026-10-11) | un único sitio que resuelve el motor |
| motor construido dentro de Dropbox | la sincronización borró el binario recién compilado y una réplica arrancó sin él | construir fuera de la carpeta sincronizada; comprobar que el binario existe y su hash antes de lanzar nada |

---

## 5. El MCP que maneja drvec-python

### 5.1 ¿Un asistente nuevo o dentro de sima?

`drtran-python/docs/ARCHITECTURE_MCP.md` separa los asistentes según la
pregunta que contestan, el material del que parten y lo que afirman. drvec:

- **parte de `.pre`**, como mtram, no de series crudas como sima;
- **pregunta por el largo plazo**: el rango, β, quién corrige y la
  exogeneidad débil. Ni drvarma (VARMA estacionario) ni drtran (red recursiva)
  hacen esa pregunta;
- **afirma una relación de equilibrio** con su inferencia no estándar (rango por
  bootstrap, β superconsistente).

**Recomendación: un cuarto asistente**, el peldaño que sigue a mtram cuando la
red tiene un ciclo **y** las series son I(1). sima queda para el VARMA en
diferencias o en niveles estacionarios. El nombre está por decidir. La regla de
paso que habría que añadir a mtram y a sima es: «≥ 2 series I(1) con
realimentación → contrastar el rango».

### 5.2 Herramientas, a partir de los procesos

| herramienta | proceso | síncrona | notas |
|---|---|---|---|
| `load_pre` | §1, §3.1 | sí | roles (∇Y₂ / Y₁) **confirmados**, alineación por fechas, identidad de cruce; se niega si no cruza |
| `propose_order` | §3.2 | sí | p = AR en ∇ + 1; último Γ |
| `set_case` | §3.3 | sí | por defecto `pre`; explica el porqué; incompatibilidades |
| `rank_test` | §3.4 | **no** (tarea) | B, tabla asintótica y bootstrap, error MC, `reps`; propone r̂ |
| `job_status` / `job_result` / `job_cancel` | — | sí | para las tareas largas; progreso por réplicas |
| `estimate` | §3.5, §3.8 | sí | `-fdhess` por defecto; α, β, s.e., θ, semividas y diagnosis estructurados |
| `test_restriction` | §3.6 | sí | par libre/restringido, LR, gl, p; negativas |
| `add_exogenous` | §3.7 | sí | despacho resta/empotrada por d; red |
| `impulse_response` | §3.9 | sí | puntual, en niveles; permanente/transitoria |
| `irf_bands` | §3.9 | **no** (tarea) | percentiles 90/95; CSV como `.irfboot` |
| `variance_decomposition` | §3.9 | sí | |
| `sweep` | §3.10 | **no** (tarea) | rejilla de especificaciones con caché |
| `forecast` / `evaluate` | `-f`, `-estwin` | sí | |
| `specs` | `-specs` | sí | la clase MA, para q > 0 |
| `record_decision` / `export_guion` | toda decisión | sí | **guion como el de art**: nodo, evidencia, decisión, por qué, versiones adoptadas y abandonadas |
| `export_tables` | §3.11 | sí | LaTeX + JSON desde un registro de corridas |
| `plot_*` | | sí | IRF con bandas, residuos, CCF de residuos, distribución bootstrap de la LR frente a la observada |

### 5.3 Reglas de comportamiento (las de la escuela, aplicadas aquí)

- **Pregunta inicial**: ¿guiado o autónomo? En el guiado, el rango, el caso y
  cada restricción son nodos de decisión con la evidencia a la vista. En el
  autónomo se recorren los mismos nodos y se escribe la razón de cada decisión.
- **Las negativas del motor no se quitan sin consultar** al analista. Cada una
  protege una afirmación (p. ej. `-irfboot` con `-xlink` necesitaría el futuro
  de X).
- **El bootstrap decide el rango, no la tabla asintótica.** La teoría puede
  prevalecer, y entonces se escribe en el guion.
- **LR para las restricciones**; Wald sólo como orientación.
- **Cada resultado lleva la versión del motor, la clave de caché y la semilla.**
- **Salida mixta como en mtram**: los bloques del motor en inglés (el formato del
  `.out` del C) y la narrativa en el idioma del usuario.

### 5.4 Las tareas largas

En la revisión, un bootstrap se lanzaba en segundo plano, se esperaba con un
bucle y se volvía a leer el `.out`. El MCP tiene que hacer eso mismo como
servicio:

- una cola con un trabajo por núcleo; cada trabajo en su carpeta;
- la clave (entradas + opciones + motor + semilla) como identificador: si existe
  un resultado con esa clave se devuelve sin correr, y si hay uno en marcha se
  devuelve el que ya existe;
- progreso legible (réplica k de B, en qué comparación r → r+1);
- un resultado parcial no se informa como final. El C informa cada comparación
  al terminarla, y así tiene que seguir.

---

## 6. Lo que el porte tiene que reproducir para que la réplica pública sea posible

**El alcance, decidido el 2026-10-11.** VERSIONS.md (0.11) dejaba fuera de la
primera versión del porte `-fixb2row`, `-xpre`/`-xlink`, `-xsys`,
`-case pre`/`-trend` e `-irfboot`. **El analista decidió incluirlas**, porque el
artículo usa las cinco y la réplica pública tiene que poder hacerse con
drvec-python:

| opción | qué del artículo depende de ella |
|---|---|
| `-case pre` | trivariante, fiscal, balanzas: rango y modelos adoptados |
| `-fixb2row` | homogeneidad del trivariante (contrastes B y C, `tab:validation`) |
| `-trend` | columna de tendencia de `tab:validation` |
| `-xpre`/`-xlink`, `-xsys` | sistema aumentado con BXE, exogeneidad de IT y NX |
| `-irfboot` | bandas de las respuestas del trivariante |
| `-alpha FILE`, `-weakex`, `-fixb2 v` | modelo del texto (α_{G/Y} = 0), α restringido del trivariante, β = (1, −1) |
| `-lrtest -bootstrap N` | todo `tab:rank` |
| `-fdhess` | todos los errores estándar citados |

**Lo que se hizo con esa decisión, en la 0.12** (VERSIONS.md; `TODO.md`,
«PRIORIDAD»):

- VERSIONS.md enmienda la entrada de la 0.11: esa superficie **entra** en la
  primera versión del porte.
- **Goldens** propios: 34 casos, diez de ellos de la superficie del porte
  (`-fixb2row`, `-xpre`/`-xlink`, `-xsys`, `-nodrift`, `-case pre`, `-trend`,
  `-irfboot` con su `.irfboot` y el bootstrap del rango con su `.boot`).
- **Las lagunas se portan como negativas** (`-xsys` con q > 0, el rango con
  exógenas o con `-trend`, `-xlink` con `-f`) y son la primera prioridad de la
  versión siguiente: primero en el C y luego en el porte. El artículo no las
  necesita (todo es q = 0, y no hay rango con exógenas ni previsión con
  `-xlink`).
- BUG-41 y BUG-42 cerrados, para que el porte no tenga que reproducir defectos
  conocidos.

**Homologación de los bootstraps.** Los ajustes se homologan como en drtran
(logL a 1e-6, estimaciones a 1e-4). Para los bootstraps hacen falta dos cosas:

1. **Portar el generador bit a bit**: el LCG, Box-Muller con `spare`, la semilla,
   el estado global sin reinicios y el orden de consumo (§3.4). Sin eso, las
   cifras del bootstrap sólo coinciden en distribución, y la réplica de una
   tabla pasa a ser «dentro del error Monte Carlo».
2. Aun con el generador idéntico, **cada réplica es un reajuste**: una
   diferencia de 1e-8 en el optimizador puede hacer que una réplica límite
   converja en un lado y no en el otro. Criterio propuesto, por niveles: (a) la
   misma decisión a 5 %; (b) críticos y p-valor dentro de 2 errores Monte Carlo;
   (c) `reps` dentro de ±1 %; y, para depurar, (d) **réplica a réplica**,
   contra el `BASE.boot` de la 0.12: el estado de cada réplica tiene que
   coincidir, y la LR* a la tolerancia de los ajustes.

**Lo demás que carga el porte** está en `REVIEW_2026-09-23.md` §5: los dos
órdenes de filas (la familia BUG-17), el vector de parámetros, el estado global,
la numérica que se reproduce exactamente y el lector `fue.load()`. Antes de
escribir código hay que decidir lo de `ESTUDIO_MAURICIO_2026-09-23.md` §5: la
clase MA por defecto, la verosimilitud, el test de rango y el diagnóstico de
rango.

---

## 7. Dónde está todo en el proyecto del artículo

`~/Dropbox/dolarization/revision-suite/` (rama `revision-suite`; remoto privado):

| | |
|---|---|
| `Python/art_drvec/VECM_*/` | los sistemas: `.pre` adoptados, redes, `run_*.sh`, `.out` y README por directorio |
| `Python/art_drvec/tables/` | `build_drvec_ref.sh`, `vecm.py`, `vecm_registry.py`, `make_tables.py`, `make_docs.py` |
| `Python/art_drvec/replicate*.sh`, `compare_replica.py` | la réplica; `replica/` (0.10), `replica_v011/` y `replica_v012/`, cada una con su `REPORT.md` |
| `borrador/analisis/` | `README.md` (método y decisiones), `mapa_paper.md`, `multivariantes.md` (fichas generadas) |
| `borrador/tables_suite/` | `tab_rank.tex`, `tab_vecm.tex`, `tab_valid.tex` y sus `.json` |
| `borrador/revision_empirica_2026-10.md` | la bitácora (§4.x por sistema) |

---

## 8. Pendiente

- [x] Alcance de la primera versión: **incluye** `-fixb2row`, `-xpre`/`-xlink`,
      `-xsys`, `-case pre`/`-trend` e `-irfboot` (2026-10-11, §6).
- [x] VERSIONS.md corregido (0.11 enmendada, 0.12).
- [x] Goldens propios de cada opción de esa superficie (0.12).
- [x] Lagunas: se portan como negativas; prioridad de la versión siguiente.
- [x] `BASE.boot` con la LR* de cada réplica (0.12).
- [ ] Nombre del asistente y regla de paso desde mtram y sima (§5.1).
- [ ] La descomposición transitoria y permanente como herramienta (§3.9).

---

## 9. La réplica con las etiquetas (2026-10-11)

### 9.1 Con `drvec-v0.11`

Se rehízo todo con el binario de la etiqueta (`replica_v011/`, criterios del
§3.11):

| bloque | resultado |
|---|---|
| 71 ajustes (puertas, rangos asintóticos, ajustes, restricciones, `-xlink`, `-xsys`, casos 3/pre/4) | **71 idénticos**, 0 con diferencias (`replica_v011/REPORT.md`) |
| 3 bootstraps de rango: central (B = 999, p 0.009), aumentado (B = 499, p 0.034 / 0.384), trivariante caso 3 (B = 499, p 0.040 / 0.0345, 492 réplicas) | **idénticos** a los de la 0.10: críticos, p-valores y `reps` (13, 82 y 26 min) |
| `-irfboot 999` del trivariante (999 reajustadas, 0 fallidas) | `.irfboot` **idéntico byte a byte** (17 min) |
| tablas multivariantes (`tab_rank`, `tab_vecm`, `tab_valid`) regeneradas desde cero con la v0.11 | `.json` **idénticos byte a byte** |

**Conclusión.** Para todo lo que usa el artículo, los cambios de la etiqueta
frente a la copia autónoma (`drvmlest.c`, `qnewtopt.c`) no mueven ninguna cifra,
ni siquiera en las réplicas de los bootstraps, que reajustan miles de modelos
desde puntos simulados.

### 9.2 Con `drvec-v0.12`

`replica_v012/` (motor compilado en `~/.local/share/atsw/drvec-v0.12`):

- **Ajustes:** 73 de 73 idénticos (logL, parámetros, iteraciones).
- **Bootstraps del rango:** críticos, p-valores y réplicas idénticos a la
  v0.11 en los tres (central 999, aumentado 499, trivariante caso 3 499). El
  cambio de `lnsrch` no mueve ninguna réplica en estos sistemas. La única
  diferencia en el `.out` es la línea que remite al `BASE.boot`, que ahora se
  guarda junto al `.out` (`replicate_boot.sh` lo mueve). Tiempos: 13, 89 y
  23 min.
- **`-irfboot`:** el `.irfboot` es idéntico byte a byte.
- **Tablas:** `tab_rank` y `tab_valid.tex` idénticas. `tab_vecm.tex` cambia
  sólo en la diagnosis, como se esperaba: Q de Hosking con χ²(m²s−k) baja los
  grados de libertad (32→10–18, 72→21–42) y los p-valores (0,57–1,00 →
  0,22–0,66); el JB de Lütkepohl se mueve poco salvo en el trivariante
  (0,91 → 0,54). Ningún veredicto cambia: el único rechazo, JB p 0,00 del
  sistema fiscal, ya lo era. `valid.json` cambia en la diagnosis que no se imprime.
- **Consecuencia:** las tablas oficiales del artículo se regeneran con la
  v0.12. Las fichas que citan la Q de corridas antiguas (anteriores a la
  v0.11) conservan la fórmula vieja hasta que esas corridas se repitan.
