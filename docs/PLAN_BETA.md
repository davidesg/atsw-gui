# `drvec`: plan hasta una versión beta

*Objetivos, criterios de salida medibles y contingencias. Escrito el 2026-08-17,
sobre lo establecido en `ANALISIS_PRELIMINAR.md` (diagnóstico y siete
correcciones) y `ESTUDIO_BVECM_vs_DRVEC.md` (el legado y su artículo).*

---

## 0. Qué significa «beta»

No «que compile y estime». La definición operativa que uso aquí:

> **Beta = alguien que no sea el autor puede usar `drvec` sobre datos propios,
> obtener un modelo VEC interpretable con errores estándar y un rango
> defendible, y saber cuándo no fiarse del resultado.**

De ahí salen cuatro requisitos, y el orden no es negociable:

1. **Fiabilidad numérica** — converge, o dice claramente que no.
2. **Interpretabilidad** — produce las formas BEC/Π y contrastes de exogeneidad
   débil. Sin esto `drvec` no responde a ninguna pregunta económica; da matrices.
3. **Homologación** — reproduce referencias del banco, y las que no reproduce
   están documentadas con evidencia.
4. **Reproducibilidad** — hay una batería de pruebas que falla cuando alguien
   rompe algo.

Hoy tenemos parte de (1) y (3), nada de (2) y **nada de (4)**, que es el riesgo
de proceso más grande: todo lo verificado en esta sesión se comprobó a mano.

---

## 1. Punto de partida

**Establecido y verificado:**

- La transformación de Mauricio es fiel al artículo, término a término, y
  coincide con la forma cerrada del AddOn con diferencia 0.000e+00.
- `elf()` (AS 311) validada a 2·10⁻⁸ contra una normal multivariante calculada
  a mano. **No se toca nunca.**
- Siete correcciones aplicadas (§0 de `ANALISIS_PRELIMINAR.md`), todas en
  `src/drvec.c`; el motor está byte a byte intacto.
- `-lrtest` con r = 0 acierta el rango en los dos casos con rango conocido
  (`mink_muskrat` → r = 1; `urca_UKconsumption` → r = 2, igual que `ca.jo`).
- `-fixb2` converge donde el modelo libre se para en termcode 3.

**Abierto:**

- **Termcode 3 residual.** La dirección exactamente plana ya no está, pero el
  optimizador sigue parando ahí en la mayoría de configuraciones.
- **El criterio de aceptación de |Σ̂| no se cumple de forma fiable**: sólo 1 de 4
  configuraciones equivalentes cae dentro de la banda 0.0022–0.0024.
- **La columna EML de Mauricio no es reproducible** y no se puede cerrar con el
  material disponible.
- No hay capa interpretable, ni siembra desde la suite, ni batería de pruebas.

---

## 2. El invariante que hace seguro todo lo demás

Antes de cualquier fase, esto es lo que permite avanzar sin romper nada. Es la
«puerta diagonal» de `drtran` (`drtran-python/src/drtran/cast.py`), adaptada:

> **Con r = 0 se tiene C̄ = I y H̄ = 0, luego Φ*ᵢ = Fᵢ y Θ* = Θ. Si además se
> activan `-diagar -diagma -diagcov`, el VARMA sobre ∇Y es diagonal, la
> verosimilitud exacta **factoriza**, y el logL conjunto debe ser **exactamente**
> la suma de los univariantes de ML exacta.**

**Verificado el 2026-08-17** sobre `mink_muskrat` (p=2, q=1, caso 1):

```
drvec conjunto, r=0, todo diagonal                      logL = -34.6278
ARMA(1,1) univariante sobre ∇log muskrat                logL = -20.0580
ARMA(1,1) univariante sobre ∇log mink                   logL = -14.5698
                                                  suma = -34.6278
                                            diferencia =   0.0000
```

Dos cosas que salen de aquí, y las dos importan:

- **Es un oráculo del cast.** Si el conjunto y la suma dejan de coincidir, el
  fallo está en `vec_shootx` o en la siembra, **nunca en `elf`**. Es exactamente
  el criterio con el que `drtran` valida el suyo.
- **Fija una convención que no estaba escrita:** `p` es el orden AR del VARMA
  **sobre Ȳ**, así que con r = 0 el orden AR efectivo **sobre ∇Y es p−1**
  (Φ*_p = −F_{p−1}C̄⁻¹H̄ = 0). Comparar contra un ARMA(p,q) univariante en vez de
  un ARMA(p−1,q) da un desajuste de 5.5 unidades que parece un fallo y no lo es.
  *(Me pasó al comprobarlo.)*

La puerta **no** aplica con r ≥ 1: ahí Φ* = C̄Φ̄ no es diagonal ni con las
banderas puestas, porque C̄ acopla. Es un invariante de un peldaño, no general —
pero es el peldaño desde el que se construye todo.

---

## 3. Fases

Cada fase declara objetivo, criterio de salida **medible**, y contingencia.

### F0 — Red de seguridad *(prerrequisito de todo)*

**Objetivo.** Convertir en script lo que hoy se comprueba a mano.

- `tests/run_tests.sh`: el barrido de configuraciones (tres casos deterministas,
  banderas diagonales sueltas y combinadas, M=2 y M=5, r=0..3, `-levels` y
  `-differenced`, `-lrtest`, `-fixb2` con y sin valor), comprobando que no
  aparece `ERROR output` / `ERROR init_guess`.
- **Números de oro**: los logL documentados, con tolerancia declarada.
- **El test de la puerta diagonal** (§2) automatizado.
- `valgrind` sobre las rutas degeneradas (`r=0` con `p=1`, banderas diagonales).

**Salida.** `make test` verde; mover un número documentado hace fallar.

**Contingencia.** Si el test de la puerta necesita `drvarma`/Python y eso
complica la ejecución, se fijan los dos logL univariantes como constantes
declaradas en el script (son estables y están medidos aquí), anotando que son
valores de referencia y no recalculados. Se pierde independencia y se gana que
el test exista — el trato correcto en esta fase.

---

### F1 — Σ endurecida, copiando a `drtran`

**Objetivo.** Cerrar el criterio de |Σ̂|. Tres cambios, todos práctica ya
establecida en `drtran` (`cast.py:build_sigma`):

1. `var_i = exp(x_i)` para i ≥ 2, con Q₁₁ = 1 (ya está). El parámetro pasa a ser
   `log(var_i/var_1)`, lo que **garantiza positividad** — hoy `drvec` lleva
   varianzas crudas que pueden irse a negativo y sólo las salva el Cholesky
   interno de `elf`.
2. Chequeo explícito de definida positiva sobre las covarianzas, devolviendo
   `ifault` en vez de dejar que falle abajo.
3. **Sembrar las razones de varianza del dato**, no en 1. Hoy `drvec` siembra Σ
   en la matriz de correlación, es decir todas las razones en 1 — que es el error
   que `drtran` midió como catastrófico (logL −1371 en vez de −767 con escalas
   que diferían 1098×). *Calibración honesta:* en nuestros casos el rango es
   modesto (razón 1.06 en `mink_muskrat`, hasta 15× entre componentes en UK
   consumption), así que **no es el problema dominante hoy**; es una bomba de
   relojería para sistemas que mezclen unidades, que es lo normal.

**Salida.** |Σ̂| dentro de 0.0022–0.0024 en **las cuatro** configuraciones del
criterio, no en una. Ninguna configuración del barrido rechazada por Σ no PD.

**Contingencia.** Si la convergencia no mejora, **se conserva igualmente**: la
positividad garantizada es estrictamente mejor que la actual, y el cambio es
barato y aislado. Si además el criterio sigue sin cumplirse en las cuatro, se
reformula la banda con la dispersión medida y se declara explícitamente que el
objetivo pasa a F2 (siembra), que es la otra causa candidata.

---

### F2 — Siembra desde la suite (`fue` / `.pre`)

**Objetivo.** Que `drvec` deje de arrancar en frío. Es el punto flaco real y la
razón de que `drvec` esté hoy fuera de la arquitectura de la suite.

El diseño de la suite es un óptimo binivel con los ficheros como interfaz
(`drtran-python/docs/LADDER_AS_OPTIMISATION.md`): **ART** identifica y emite
`.inp`; **fue** estima y emite `.pre`, que es *«un óptimo en forma
re-ejecutable»*; el consumidor lee los `.pre` y **sólo ensambla**. `drtran` lo
hace con `read_fue_pre` y `x0_from_pre`.

**Cadena para `drvec`:**

```
B₂ preliminar (OLS estático, ya está en init_guess)
   -> construir Ȳ = (∇Y₂ ; W)          <- observable una vez B₂ está fijado
      -> ART identifica cada componente; fue estima  ->  un .pre por componente
         -> drvec lee los .pre y arma la semilla:
              Θ, F propias y razones de Σ   <- de los .pre
              Λ                             <- de la regresión condicional
              B₂                            <- OLS, o congelado con -fixb2
```

**Salida.** El logL sembrado ≥ el de arranque en frío en **todas** las
configuraciones del banco, y la puerta diagonal (§2) sigue cuadrando a 1e-9.

**Contingencias, y una es un bloqueo real:**

- **`CAST.md` §9: el cast de `fue` no es reentrante y no pueden estar dos vivos a
  la vez.** Así que **no** se llama al cast de `fue` en tiempo de ejecución. Se
  leen sólo los **números** del `.pre`, que es lo que hace el C de `drtran`. Eso
  esquiva el problema por completo y además es el diseño preferible.
- Si reutilizar `read_fue_pre` arrastra demasiado de `drtran`, se escribe un
  lector mínimo de los campos que `drvec` necesita (φ, θ, μ, σ², λ, d, D). Menos
  elegante, pero desacoplado.
- **`.pre` no da Λ.** Los univariantes no llevan información de acoplamiento
  entre ecuaciones, y Λ *es* el acoplamiento: una semilla puramente univariante
  daría Λ ≈ 0, el peor sitio posible. Por eso la cadena mantiene la regresión
  condicional. Si aun así Λ sale mal, la alternativa es la rejilla concentrada
  para B₂ (el patrón de `preestimar_parametros` del legado: concentrar Λ en forma
  cerrada para cada B₂ candidato y buscar en rejilla sólo B₂).

---

### F3 — Capa interpretable: formas BEC y Π

**Objetivo.** Que `drvec` responda preguntas económicas. Hoy da Λ̂ y B̂₂ y para.

Hace falta la triangularización P con Σ = PDP′ y D diagonal (BVECM §4), que
descorrelaciona las innovaciones y da las ecuaciones desacopladas, más:

- forma BEC: `∇Y_t = Λ(B′Y_{t−1} − E[W]) + C(B)∇Y_{t−1} + u_t`;
- forma Π;
- **contrastes de exogeneidad débil** (filas de Λ nulas), por Wald.

`drv_project` lo tiene resuelto para el caso bivariante (`analisis_BEC`,
`calcular_chi2_weakex`, `calcular_chi2_joint`, con método delta y pseudoinversa
SVD), y `LEGACY_NOTES.md` §2 y §5 ya recogen las fórmulas.

**Salida.** Sobre un caso bivariante, reproducir los α, γ y los p-valores de
exogeneidad débil de `drv_project` con tolerancia declarada. `drvec` tiene
ventaja aquí: **Λ ya está en sus coordenadas**, así que sus errores estándar
salen del Hessiano y no del método delta.

**Contingencia.** Si el método delta para C(B) y Π resulta frágil, se publican
las estimaciones puntuales de la forma BEC **sin** errores estándar, marcando el
hueco. Es mejor que unos errores estándar que no se sostienen. La exogeneidad
débil, que es el contraste que de verdad interesa, sólo necesita Λ y su
covarianza — que ya son directas.

---

### F4 — Test de rango defendible

**Objetivo.** Que el rango que sale de `-lrtest` sea defendible en muestra
finita, no sólo asintóticamente.

BVECM §6.3–6.5 es tajante: bajo H₀ la distribución **no es χ²** y recomienda
**bootstrap paramétrico** — simular N muestras bajo H₀ con los parámetros
estimados, reestimar H₀ y H₁ en cada una, y tomar percentiles empíricos. Razones:
hay componentes MA bajo las dos hipótesis y (en su extensión) el operador de
convergencia añade deterministas que desplazan la distribución.

**Salida.** Reproducir el rango de `ca.jo` en ≥ 4 casos del banco, con los
valores asintóticos actuales **y** con el bootstrap, y que coincidan en esos
casos. Los asintóticos se conservan como opción rápida por defecto.

**Contingencia.** El coste es N × M estimaciones completas y puede ser
prohibitivo. En ese caso: (a) reducir N y **reportar el error de Monte Carlo**
en vez de esconderlo; o (b) limitar el bootstrap al contraste r = 0 frente a
r = 1, que es el que decide si hay cointegración y el único imprescindible.

---

### F5 — Cierre de beta

**Objetivo.** Empaquetar.

- Consolidar la documentación: hoy hay tres estudios (`ANALISIS_PRELIMINAR`,
  `ESTUDIO_BVECM_vs_DRVEC`, este plan) más `LEGACY_NOTES` y `ENCUADRE_ESTUDIO`.
  Beta necesita **un** documento de entrada que diga qué hace el programa y qué
  no, con los estudios como material de apoyo.
- **Registro de homologación**: una tabla de casos del banco, qué reproduce
  `drvec`, con qué tolerancia y con qué fecha.
- **Corregir `benchmark/README.md`** al nivel que quede: hoy ningún caso
  califica como «Verified».
- Revisar los `.inp` de `data/` uno a uno (§8.3 de `ANALISIS_PRELIMINAR`): sólo
  `AL.inp` está identificado como layout antiguo.

**Criterios de salida de beta:**

| # | criterio | estado hoy |
|---|---|---|
| 1 | `make test` verde, con la puerta diagonal dentro | ✘ (F0) |
| 2 | \|Σ̂\| dentro de la banda en las cuatro configuraciones | ✘ (F1) |
| 3 | Siembra desde `.pre`, con logL ≥ arranque en frío | ✘ (F2) |
| 4 | Formas BEC/Π y exogeneidad débil, con o sin s.e. declarado | ✘ (F3) |
| 5 | Rango correcto en ≥ 4 casos del banco | parcial: 2 de 2 probados |
| 6 | Todo termcode 3 residual **explicado**, no necesariamente eliminado | ✘ |
| 7 | Registro de homologación y documento de entrada | ✘ (F5) |

---

## 4. Restricciones transversales

Valen para todas las fases y no se negocian:

- **`elf()` no se toca.** Es la AS 311 publicada y refereada, validada aquí a
  2·10⁻⁸.
- **El optimizador no se toca.** `drtran/docs/OPTIMIZER_STOPPING_STUDY.md` ya
  probó y rechazó con evidencia tres variantes de `typx`, y la pregunta
  «¿termcode 3 es el óptimo o mal condicionamiento?» está abierta a nivel de
  suite. Todo lo de este plan se consigue por **cast, siembra o
  parametrización**.
- **Nada de cambios que muevan resultados sin declararlo.** El patrón de esta
  sesión (§0 de `ANALISIS_PRELIMINAR`: qué cambio mueve estimaciones y cuál no)
  se mantiene.

---

## 5. Registro de riesgos

| # | riesgo | probabilidad | contingencia |
|---|---|---|---|
| R1 | **El termcode 3 residual es intrínseco.** El artículo BVECM dice que «multicollinearity appears to be natural to CI», y el estudio del optimizador lo deja abierto para toda la suite | alta | No perseguirlo. Emitir una **nota de convergencia** como hace `drtran` (`Fit.convergence_note`) y declararlo en la documentación. Criterio 6 de beta se cumple con *explicado*, no con *eliminado* |
| R2 | **La columna EML de Mauricio no se cierra nunca.** Haría falta el programa original o correspondencia con el autor | alta | Beta se homologa contra el criterio de \|Σ̂\| (§5.11), respaldado por tres fuentes independientes, **no** contra los logL publicados. Ya está así en `benchmark/README.md` |
| R3 | **El cast de `fue` no es reentrante** (`CAST.md` §9) | cierta | Leer sólo los números del `.pre`, nunca llamar al cast en ejecución. Ya incorporado al diseño de F2 |
| R4 | **Los datos del Census Housing no aparecen** | alta | Beta se homologa con un solo caso de Mauricio más los reproducidos con `ca.jo`. Ya está declarado en el banco |
| R5 | **Trampas de convención** entre `drvec`, BVECM y `drv_project`: B₂ = −β, orden de Ȳ invertido en el código del legado, `p` como orden sobre Ȳ y no sobre ∇Y | media, y ya ha mordido dos veces | Están en `ESTUDIO_BVECM_vs_DRVEC.md` §2 y en §2 de este plan. Convertirlas en **un test** en F0, no sólo en documentación |
| R6 | **F3 se desborda** (la triangularización P general para M y r arbitrarios es más trabajo del que parece) | media | Beta con la capa interpretable **sólo para el caso bivariante** (M=2, r=1), que es el del banco y el del legado, y el caso general como post-beta |
| R7 | **Deriva de documentación**: cinco documentos y las correcciones se pisan | media, ya ocurrió en esta sesión | F5 consolida. Mientras tanto, cada corrección se fecha y se dice qué afirmación anterior invalida |

---

## 6. Lo que **no** entra en beta

Declararlo evita que el alcance se estire:

- **El operador de convergencia** (BVECM §5), que es la extensión de
  investigación y el punto donde `drvec` y el artículo convergerían de verdad.
- **El experimento de la parametrización dual** (optimizar en coordenadas VARMA
  y convertir al final). Es la hipótesis más informativa sobre el termcode 3 y
  también la más cara; y la evidencia de esta sesión apunta a que lo que compra
  convergencia es **fijar B₂**, no cambiar de coordenadas.
- **r = M** (proceso estacionario en niveles): no es expresable con este layout
  y para eso está `drvarma`.
- **La capa interpretable general** para M y r arbitrarios (ver R6).
- **Los tests de convergencia** (tipo de convergencia, τ, velocidad) del legado,
  que dependen del operador de convergencia.

---

## 7. Orden recomendado y por qué

**F0 → F1 → F2 → (F3 ∥ F4) → F5.**

F0 primero porque sin batería de pruebas cada fase siguiente es una apuesta: en
esta sesión encontré dos fallos (salida corrida, B₂ traspuesta) que un invariante
automático habría cazado gratis. F1 antes de F2 porque es barato, aislado y copia
práctica establecida — y porque si resuelve el criterio de |Σ̂|, F2 se aborda con
un objetivo más limpio. F2 antes de F3 porque no tiene sentido construir la capa
interpretable sobre estimaciones que no se sabe si son el óptimo. F3 y F4 son
independientes entre sí y pueden ir en paralelo.

**Y una advertencia sobre el orden que no seguiría:** empezar por F3 es tentador
porque es lo visible y lo que da valor económico. Sería un error. La capa
interpretable amplifica lo que haya debajo: si la estimación no es fiable, lo que
produce son p-valores de exogeneidad débil con pinta de resultado.
