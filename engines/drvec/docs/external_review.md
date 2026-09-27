# Evaluación externa de `drvec`: diagnóstico y soluciones propuestas

> **Alcance.** Revisión externa del estado actual de `drvec` (estimador EML de
> VARMA-VEC, Mauricio 2006), centrada en la **parte científica**: el problema del
> MA, la superficie de verosimilitud bimodal, y si la especificación tiene un
> defecto de origen teórico. Método: lectura completa de la literatura
> (`literature/`), de la documentación (`docs/`), del código científico
> (`src/drvec.c`, `elfvarma.c`, `drvmlest.c`, `qnewtopt.c`, `fue_bridge.c`), y
> ejecución de diagnóstico aislada en `/tmp` (el repositorio no se modificó).
>
> **Sustento.** Toda afirmación remite a (i) un teorema de
> [`DEMOSTRACIONES.md`](DEMOSTRACIONES.md) (números T1…T11, corolarios y lemas,
> coincidentes con los de [`THEORY.md`](THEORY.md)) y/o (ii) a un artículo de la
> literatura. Al final, el mapa completo.

---

## 1. Veredicto (resumen)

1. **La transformación de Mauricio no tiene el defecto.** Está bien implementada
   y verificada (T1; `vec_shootx` contra las ecs. (10)–(18) del paper, 4·10⁻¹⁵).
2. **El motor no tiene el defecto.** AS 311 (Lema A) y el optimizador BFGS son
   código publicado intacto; el comportamiento anómalo desaparece cuando la
   superficie es la correcta (en simulación el estimador libre recupera la
   estructura heredada por sí solo).
3. **El defecto es de especificación, y es teórico.** El modelo con `Θ` libre
   estima `q·M²` parámetros MA donde la clase que justifica el modelo —la forma
   triangular WARMA del BVECM (T6, T6b)— solo admite `q·r²`. Los parámetros
   sobrantes son los que llevan el ajuste a la superficie `det Θ(1) = 0` en la
   dirección `Λ⊥`, donde el modelo estimado **niega el rango para el que se
   estimó** (T3, T4, T5).
4. **La bimodalidad percibida es real** y tiene dos componentes: el modo admisible
   interior frente al modo de frontera (con `B̂₂` distintos), y modos locales
   numéricos menores. Medido y explicado (§4).
5. **La identificación y la admisibilidad se asumen en la literatura, nunca se
   imponen en el espacio paramétrico** (Identificación; Yap–Reinsel 1995; Mauricio
   2006). Ese es el hueco, y las soluciones de §5 lo cierran.

---

## 2. Qué es correcto (y por qué no debe tocarse)

- **El cast** (`vec_shootx`): es la derivación de T1, verificada contra el AddOn de
  Mauricio a **0.000e+00** y numéricamente a **4.2·10⁻¹⁵**. El signo del MA
  (`Θ(B) = I − Θ₁B − …`) es consistente en `elf`, fue y drvec.
- **La condición de rango** (`granger_smin`): calcula `σ_min(Λ⊥′Θ(1)B⊥)`
  correctamente (T3), y el programa la reporta en cada ajuste — es el instrumento
  que la literatura no da (Mauricio y Yap–Reinsel la asumen sobre el proceso, no
  la escriben en los parámetros).
- **`Σ[1][1] = 1`**: la corrección del parámetro de escala no identificado, ya
  aplicada (el objetivo concentrado es invariante a escala en `Σ`; ver el driver
  `objcfunc`, Lema A).
- **La factorización del peldaño diagonal** (T9): es la base correcta de la
  escalera y del contrato `.pre`; verificada hoy en `joint − sum = −1.77·10⁻⁵`.
- **El protocolo de especificación** (`-specs`, `-rankadm`, `-marow`, `-mawarma`,
  `-warma`, `-matest`, `-multistart`): es, en conjunto, la respuesta correcta al
  problema; lo que falta es *la decisión de fondo* (S1–S2), no más opciones.

---

## 3. El problema, en tres capas

### 3.1 Capa de especificación (teórica)

El modelo VEC (1) con `Θ` libre es **más general que la teoría que lo justifica**.
El Teorema 6 y el Teorema 6b prueban que un proceso con representación triangular
(WARMA) produce un MA de VEC con la estructura

```
Θ̃₁ = [ Θ_w , Θ_w B₂′ ; 0 , 0 ],
```

es decir, `q·r²` parámetros libres, bloque cruzado determinado por `B₂`, y `s`
filas inferiores cero. Con `M=2, r=1`: **1** parámetro MA donde `drvec` estima
**4**. El recíproco del BVECM ("todo VEC de rango `r` se escribe en WARMA") es
falso tal como está enunciado (T6): un `Θ` libre tiene la fila inferior y el
bloque `T₂₂` que la clase triangular no permite. El código lo mide (§4): las
entradas que explotan (−1.32, 1.51, **5.09**) son las de la fila inferior, y el
`T₂₂` libre va a uno — el factor `(1−B)` sobre `∇Y₂`, el modelo **deshaciendo su
propia diferenciación** (C6.2; Plosser–Schwert 1977; Hillmer–Tiao 1979).

**Consecuencia:** el optimizador, premiado por la verosimilitud (LR 13–26 a favor
del modelo libre), visita la superficie `det Θ(1)=0`. Por T4 esa superficie está
permitida y la verosimilitud es finita y continua allí; por T5 ningún chequeo de
raíces del motor la detecta (solo se ve como raíz MA unitaria). Por T3, en la
dirección `Λ⊥` esa degeneración destruye el rango.

### 3.2 Capa de identificación

La identificación del VARMA (coprimidad izquierda de `(Φ, Θ)` + condiciones de
Hannan) es **asumida por referencia**, no traducida ni impuesta: Mauricio (2006)
dice *"conditions … such as those considered by Yap and Reinsel (1995) are also
assumed"*; Yap–Reinsel lo asume explícitamente. El hueco es doble (Identificación,
§7 de DEMOSTRACIONES):

1. la coprimidad identifica el VARMA **de niveles**, pero no garantiza que el
   modelo **ajustado** sea I(1) de rango `r` — eso es la condición de rango (T3);
2. la condición de Granger con MA (`rango(Λ⊥′Θ(1))=s`) es **más fuerte** que la
   coprimidad: hay puntos perfectamente identificados como VARMA que son
   inadmisibles como modelo de rango `r`.

### 3.3 Capa de inferencia en la frontera

Toda la inferencia χ²/mixto-normal sobre `B₂` y `Λ` está probada **sobre 𝒞** (T10,
Phillips 1991a; T11, Johansen 1991; Ahn–Reinsel 1990). La condición de Phillips —
*"all unit roots eliminated by specification and data transformation"* — falla
exactamente donde caen los ajustes libres (Θ̂(1) singular) y en la nula del test de
rango (Λ=0, donde `B₂` no está identificado, T7). En un óptimo de `𝒫 \ 𝒞` los
errores estándar y los LR no tienen su distribución habitual. La asintótica del
test de rango no la cambia el MA (T8, Yap–Reinsel Theorem 3), pero en muestra
finita sobre-rechaza (30% contra 5% nominal, n=120) — medido en el repo.

### 3.4 Contribuciones numéricas (no son la causa, pero amplifican)

- **H1. `-m 2` no es el ML condicional.** En todo el código, `met == 2` solo cambia
  el signo de `xitol`, desactivando la truncación de `ξ`; la verosimilitud
  condicional de Mauricio (Remark 4: "a simple function has been coded expressly")
  **no está implementada**. La documentación ("Exact (1) or approximate (2) ML")
  es engañosa.
- **H2. Truncación de `ξ` no suave** (`xitol = 1e−3`): el índice de truncación
  cambia discontinuamente a lo largo de la trayectoria; en direcciones planas crea
  modos locales espurios y paradas falsas (Lema A; HOMOLOGATION §1b: `1.4·10⁻⁴`
  de desacuerdo entre configuraciones equivalentes).
- **H3. Barrera blanda + gradiente por diferencias finitas.** `objcfunc` devuelve
  1.0 en puntos inadmisibles; el gradiente cerca de la puerta evalúa a ambos lados
  → ruido → termcode 3 / parada en `steptol`.
- **H4. La puerta admite no-invertibilidad.** `chekma` rechaza con eigenvalor
  `≥ 1.00005` (raíces hasta `0.99995`, *dentro* del círculo por 5·10⁻⁵); los
  ajustes libres se estacionan exactamente ahí.

---

## 4. Evidencia

### Medida en esta revisión (mink–muskrat, p=2, q=1, r=1, caso 2; en `/tmp`)

- **Multimodalidad**: `-multistart 30` → logL de **−10.771 a 6.637** (30/30 "ok");
  el mejor termina en `steptol`, raíz MA mínima **0.99995\***, `G = 0.190`.
- **Dos regímenes de `B̂₂`** según se permita la vecindad degenerada:
  `-rankadm` 0.05 → −0.241; 0.20 → −0.210; 0.30 → −0.198; `-m 2` → **−0.2048**;
  libre (mejor de 30) → −0.2476. El régimen admisible reproduce el valor
  publicado de Mauricio (2006, Tabla 4): **B₂ = −0.2042**.
- **`-diagma`**: `Θ̂₁₁ = 1.000044` — la puerta, en la ecuación de `∇Y₂` (el bloque
  diferenciado), la forma más pura de la sobrediferenciación; `B̂₂ = −0.087`.
- **Peldaño diagonal** (`r=0`, diag): factorización `joint − sum = −1.77·10⁻⁵`,
  VERIFIED (T9).
- **El propio paper**: el `Θ̂₁` EML publicado de Mauricio (Tabla 2) tiene raíces de
  `det(I − Θ̂₁x)` en x = 1.182 y **x = −0.994** (no invertible); su CML es
  invertible (±1.058). La atracción a la frontera está **en los números publicados
  del artículo que el programa implementa** (Hillmer–Tiao 1979; T4).

### Del registro del repo (HOMOLOGATION §4g–§4p, DEVELOPMENT_RECORD §8d–§8k)

- Ocho pares de trigo: libre `G` de 0.016 a 0.133, `det Θ̂(1) ≈ 0`, dirección nula
  alineada con `Λ⊥` al coseno 0.946–0.9996 (el caso fatal de C3.2).
- `-mawarma`/`-marow`/`-warma`: raíces MA 1.38–12.55, convergencia por gradiente
  8/8, `B̂₂` en la región canónica de Johansen (0.001–0.052) — los tres síntomas
  desaparecen juntos.
- El LR favorece al modelo libre por 13–26 (3 gl), pero **esa comparación no tiene
  distribución**: el óptimo libre está en frontera (T10).

---

## 5. Soluciones propuestas

Cada solución: problema que ataca, respaldo (teorema/literatura), efecto esperado,
riesgo.

### S1 — Hacer de la admisibilidad parte del problema de estimación (barrera dura)

**Problema.** El optimizador visita `det Θ(1)=0` porque está permitido y premiado
(T4, T5).

**Propuesta.** Reemplazar la barrera blanda (devolver 1.0) por una **región
admisible estricta** con dos condiciones, en el cast: (i) `Σ` definida positiva
(ya está), y (ii) `det Θ(1) ≠ 0` — o su forma fuerte y direccional,
`σ_min(Λ⊥′Θ(1)B⊥) ≥ tol` (T3), con `tol` fijado por calibración, no dejado a la
discreción. La condición (ii) es barata de evaluar (C3.1) y es **exactamente la
que falta en la literatura**.

**Respaldo.** T3, C3.1, C3.2; el hueco de identificación (Yap–Reinsel/Mauricio).

**Efecto.** Los ajustes libres dejan de poder negar el rango; `B̂₂` entra en el
régimen admisible sin restringir la clase MA.

**Riesgo.** Bajo: `-rankadm` ya existe; esto lo convierte en un rechazo **dentro**
del cast (como el de `Σ` no-PD), no en un comentario. Hay que elegir `tol` con una
calibración empírica (el registro §4h ya la tiene: en la vecindad `G < 0.1` es
degenerado, `G ≈ 1` es el valor de una verdad conocida).

### S2 — Decidir formalmente la clase MA (triangular vs. libre bajo suelo)

**Problema.** Dos modelos en disputa: el libre (inadmisible en su óptimo) y el
heredado de T6b (demasiado estrecho: rechazado por bootstrap en 8/11, §4i del
registro). Ninguno, tal cual, es "el" modelo.

**Propuesta.** Usar la estructura de T6b **no como restricción impuesta sino como
hipótesis testable**: el test correcto es el bootstrap paramétrico bajo la clase
triangular (el `-matest` existente), y el resultado medido —la clase se rechaza—
**licencia** un MA libre **bajo el suelo de S1**, no un MA libre sin más. La
decisión queda entonces en el dato, con la distribución correcta.

**Respaldo.** T6, T6b, T8 (la asintótica no cambia con el MA, pero el finito sí:
bootstrap); Cappuccio–Lubian (1996) — el VECM debe llevar MA, pero estructurado.

**Efecto.** Cierra la ambigüedad "heredado vs. libre": el programa entrega un
modelo *admissible y no más estrecho de lo que los datos exigen*.

**Riesgo.** Medio: requiere que `-matest` sea parte del flujo por defecto, no una
opción de diagnóstico.

### S3 — Inferencia de frontera como comportamiento por defecto

**Problema.** En el óptimo de frontera (0.99995\*) los errores estándar y los LR no
tienen su distribución (T10, T11); hoy el usuario tiene que saberlo.

**Propuesta.** Cuando el ajuste termina con una raíz MA en la puerta (o con
`G < tol`), el programa debe (i) marcar el óptimo como restringido, (ii) no
reportar errores estándar no restringidos sin cualificar (o reportar los del
bootstrap), y (iii) recomendar el bootstrap para cualquier LR. `-fdhess` ya
detecta el caso; esto lo convierte en política.

**Respaldo.** T10 (condición LAMN), T11, T7; Phillips 1991a.

**Efecto.** Nadie lee un `χ²` inválido como si lo fuera.

**Riesgo.** Bajo (solo cambia lo que se imprime y cómo).

### S4 — Corregir el MA numérico (H1–H4)

- **H1**: o bien implementar la verosimilitud **condicional** real de Mauricio
  (Remark 4) para `-m 2`, o renombrar la opción a algo como `-xitolfull`. Hoy
  `-m 2` es EML sin truncar, no CML.
- **H2**: exponer `xitol` como opción (la evidencia muestra que el punto de parada
  depende de la truncación: `B̂₂ = −0.248` con `xitol=1e−3` frente a `−0.205` sin
  truncar). Para la identidad de factorización, el residuo `1.4·10⁻⁴` es
  exactamente la truncación (T9).
- **H4**: hacer la puerta **estrictamente** invertible (rechazar eigenvalor
  `≥ 1.0`, no `≥ 1.00005`), o documentar y reportar el margen.
- **H3**: sustituir la barrera blanda por una región admisible (S1) que el
  optimizador respeta sin ruido de gradiente (p. ej., devolver un objetivo
  monótonamente creciente al alejarse, en vez de una constante 1.0).

**Respaldo.** Lema A; Hillmer–Tiao 1979 (la exacta es la correcta cerca de la
frontera, no la condicional truncada); las mediciones §4.

**Riesgo.** Bajo-medio; H1 y H2 son documentación/opción, H3–H4 tocan el cast pero
no el motor.

### S5 — Traducir la identificación de Hannan a un diagnóstico

**Problema.** La coprimidad izquierda se asume; no hay ningún chequeo.

**Propuesta.** Añadir un **diagnóstico** (no una restricción) que informe si el
VARMA estimado `(Φ̂, Θ̂)` es coprimo por la izquierda (o si hay un factor común
numéricamente cancelable), y documentar en el `.out` que la identificación del
VARMA de niveles no implica la admisibilidad de rango (que es la condición de T3).

**Respaldo.** Identificación (Hannan 1969/1975; Yap–Reinsel 1995); T3.

**Efecto.** El hueco de identificación deja de ser silencioso.

**Riesgo.** Bajo (es un reporte).

### S6 — El trabajo teórico que falta (no es depuración)

1. **Derivar y enunciar la condición completa de admisibilidad/identificación en el
   espacio paramétrico VEC** `(Λ, B₂, Fᵢ, Θⱼ, Σ)`: la condición de rango (T3) más
   las condiciones de Hannan traducidas. Es un resultado que la literatura **no
   escribe** — el proyecto ya tiene los ingredientes (T3, C3.2, Identificación).
2. **Publicar/registrar la corrección del recíproco del BVECM** (T6): el
   "todo VEC se escribe en WARMA" es falso sin (5) y (6); la forma correcta es el
   Teorema 6, probado en `DEMOSTRACIONES.md`.
3. **Inferencia en la frontera**: desarrollar el límite del MLE de VECMA cuando
   `det Θ(1)=0` (o decidir que siempre se evita por S1, en cuyo caso se documenta
   la decisión y no se necesita).

### S7 — Para el port a Python de `drvec`

No portar la superficie tal cual: portar el **protocolo** — condición de rango
dentro del problema (S1), puerta estricta (S4), multistart con dispersión como
salida de primera clase, y bootstrap de frontera (S3). El port de los hermanos
existe; este es el aprendizaje específico de `drvec`.

---

## 6. Priorización y riesgo

Por relación beneficio/riesgo:

1. **S4** (numérico: H1, H2, H4) — cambios acotados, sin tocar el motor, cierran
   falsas impresiones y artefactos.
2. **S1** (suelo de admisibilidad dentro del cast) — cierra el defecto de
   especificación en su forma operativa.
3. **S3** (inferencia de frontera por defecto) — cierra lecturas inválidas.
4. **S5** (diagnóstico de identificación) — hace visible el hueco que la
   literatura deja.
5. **S2** (decisión de clase MA vía bootstrap) — la decisión metodológica de
   fondo, ya instrumentada con `-matest`.
6. **S6** (teoría) — el cierre formal; produce el artículo/nota, no el código.
7. **S7** (port) — depende de que S1–S3 estén decididos.

Lo que **no** se propone tocar: `elfvarma.c`, `drvmlest.c`, `qnewtopt.c`,
`nlatools.c` (motor publicado), ni la transformación de `vec_shootx` (T1,
verificada). El defecto no está ahí.

---

## 7. Qué no se ha modificado en esta revisión

La revisión fue de solo lectura; las ejecuciones de diagnóstico se hicieron sobre
una copia en `/tmp`. El único artefacto nuevo en el repositorio es la presente
nota y su compañero de demostraciones, ambos documentación.

---

## 8. Mapa de sustento

**Teoremas** (`docs/DEMOSTRACIONES.md`): T1 (transformación), C1.1 (raíces MA
invariantes), T2 (largo plazo `CΘ(1)`), T3 (condición de rango), C3.1/C3.2
(sobrediferenciación), T2′ (estacionariedad), Lema A (AS 311 + puerta), T4/T5
(`𝒞 ⊊ 𝒫`, invisibilidad), T6/T6b (clase triangular + herencia del MA), C6.1/C6.2
(coordenadas transformadas, el cero `T₂₂`), T7 (Λ=0), T8 (asintótica del rango),
T9 (factorización), P1 (un peldaño), T10/T11 (inferencia), Identificación.

**Literatura** (`literature/`): Mauricio (2006) — el artículo del código, Θ libre,
identificación asumida, Remark 5 (Phillips), Tabla 2 (raíz MA −0.994). Yap–Reinsel
(1995) — coprimidad izquierda + Hannan, Theorem 3 (asintótica del rango). Phillips
(1991a) — LAMN y su condición, Remark (j). Johansen (1991) — Granger (Thm 4.1) y
límite mixto-normal (Thm 5.1). Ahn–Reinsel (1990) — caso AR. Cappuccio–Lubian
(1996) — VECM de ARMA cointegrado, `d(L)=det Φ(L)`, "no VECM con errores
independientes". Hillmer–Tiao (1979) y Plosser–Schwert (1977) — raíz MA unitaria =
sobrediferenciación; exacta vs. condicional. Mélard–Roy–Saidi (2004) — unicidad de
la transformación (Thm 3.1). Johansen–Swensen (2024) — `α = Aψ` (Prop. 1–3).
BVECM (2026, inédito, `literature/BVECM_models.pdf`) — Theorem 1 y Corolario 2
(la herencia del MA).
