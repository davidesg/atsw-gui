# EA — identificación guiada con ART, paso a paso

**Serie:** EA (ocupados en agricultura, ganadería y pesca). EPA trimestral,
1976:III–1993:II, 69 observaciones. Niveles (miles de personas).

**Propósito de este documento.** Es un ejemplo canónico —para el MCP de ART— de
cómo se construye un modelo univariante UTI/MEG **paso a paso**, registrando cada
**decisión** y el **criterio** que la sostiene. EA es el primer eslabón del caso
del empleo por sectores (ver `CASO_EMPLEO.md` y `M6_EJERCICIO.md`); su modelo
univariante es la diagonal de la que arranca el multivariante.

**Control (no fuente).** Relloso (1997), Tabla 4 —representación MEG— da para EA:

| operador | θ̂ (f0) | λ̂₁ (f1, π/2) | λ̂₂ (f2, π) | σ̂ₐ | incidentes |
|----------|---------|---------------|-------------|------|------------|
| ∇∇₄ | .43 (.20) | −.68 (.10) | −.72 (.14) | 27.3 | I/85, II/87, I/89, IV/89, I/92 |

EA es la única serie del sistema con **estacionalidad puramente estocástica** (una
sola representación posible; no colapsa a determinista). Por eso fija bien el patrón
de cómo se codifica un MEG estocástico con λ₁ y λ₂ separados.

La Tabla 4 es **control**: lo que ART identifique debe parecerse a esto; si no, hay
que entender por qué antes de seguir.

---

## Paso 1 — Transformación (Box-Cox): ¿λ?

**Lo que dice ART.** El diagrama media–desviación (rango-media) da:
- correlación con λ=1 (niveles): **0.677**
- correlación con λ=0 (log): **0.363**
- recomendación automática: **log (λ=0)**.

**La decisión: λ = 1 (niveles).** Sobreescribimos la recomendación automática.

**Criterio — preservación de las identidades contables.** EA no se modela sola: es
un **componente** de un sistema ligado por identidades exactas y **lineales**
(`E = EA+EI+EC+EP+EU`, `A = E+D`). El log rompe la linealidad —
`log(ΣEᵢ) ≠ Σ log Eᵢ`— y con ella la identidad, que después hay que propagar a los
agregados con varianza `c′Vc`. Una serie destinada a un sistema con identidades
contables **debe trabajarse en la escala en que la identidad es lineal: niveles.**

> **Heurística (transformación en sistemas con identidades).** El criterio Box-Cox
> univariante mira *una* serie; en un sistema ligado por identidades contables manda
> el sistema. Si los componentes se van a sumar (o combinar linealmente) para formar
> un agregado, todos van en **niveles**, aunque el rango-media de alguno sugiera log.
> Aquí, además, la mejora del log es modesta (0.68 → 0.36, sigue positiva): ni
> siquiera es un caso fuerte en lo univariante.

Coincide con el control: Relloso trabaja **todo el sistema en niveles** (Tabla 4, sin
Box-Cox). ✓

## Paso 2 — Diferenciación regular: ¿d?

**Lo que dice ART.** La serie tiene tendencia clara (agricultura en declive secular),
ACF que decae lentamente y PACF con un único pico en el retardo 1 (~0.95): raíz
unitaria en frecuencia cero. Los contrastes exploratorios:

| d | ADF p | KPSS p | veredicto |
|---|-------|--------|-----------|
| 0 | 0.909 | 0.010 | raíz unitaria ✗ |
| 1 | 0.102 | 0.100 | **ambiguo** ⚠ |
| 2 | 0.000 | 0.100 | estacionaria ✓ |

Recomendación automática: **d=2** (consenso ADF+KPSS).

**La decisión: d = 1.** Sobreescribimos.

**Criterio — baja potencia de ADF/KPSS ante estacionalidad.** ADF y KPSS contrastan
la raíz unitaria en **frecuencia cero**; no distinguen una *segunda* raíz regular de
una raíz *estacional*, y **pierden potencia** cuando hay estacionalidad no modelada.
Que la serie con d=1 quede *ambigua* (ADF p=0.10, en el borde) en lugar de
estacionaria limpia es la **firma de una raíz estacional residual**, no de un segundo
cero regular. Se elige **d=1** y se difiere al Paso 3 (análisis estacional) decidir si
lo que falta es estacional (→ D=1, ∇∇₄) o regular (→ d=2). d=1 mantiene ∇∇₄ abierto;
d=2 lo cerraría por sobre-diferenciación regular.

> **Heurística (d ante estacionalidad).** Cuando los contrastes de raíz unitaria
> sugieren un orden alto de diferenciación regular y la serie es estacional, sospecha
> **baja potencia**: parte de esa "no estacionariedad" es estacional. Confirma el d
> **menor** que quite la tendencia y resuelve el resto con la diferencia estacional /
> el MEG, no con más diferencias regulares.

> **Mejora pendiente de ART.** `guided_identification` recomienda hoy d=2 sin
> matizar. Debería **emitir él mismo** el caveat: «los contrastes sugieren d=2, pero
> tienen baja potencia por posible estacionalidad; se sugiere d=1 y decidir la raíz
> estacional en el paso siguiente». La lógica de recomendación debería mirar la señal
> estacional antes de fijar d. (Anotado para art-python.)

Coincide con el control: Relloso usa d=1 regular para EA (∇∇₄). ✓

> **Refinamiento (la forma rigurosa de fijar d — DCD en frecuencia cero).** La
> escuela ya no decide el orden de integración regular solo con ADF/KPSS. Aplica a
> **f=0 la misma lógica del MEG** que usa en las frecuencias estacionales: diferencia
> regularmente (impone la raíz unitaria en frecuencia cero), añade un **MA(1) regular
> como testigo de sobre-diferenciación**, y contrasta ese MA con el **DCD (H₀: θ=1)**.
> - θ **invertible** (holgadamente < 1) ⇒ la raíz unitaria es genuina ⇒ **d=1 correcto**.
> - θ **→ 1** (no invertible, cancela la diferencia) ⇒ **sobre-diferenciación** ⇒ d menor.
>
> **El disparador es diagnóstico, no la ACF/PACF.** El síntoma que lleva a hacer esta
> comprobación aunque las ACF/PACF no muestren mala especificación es que **los
> residuos no quedan bien centrados: tienen forma de ∩ (U invertida)**. Ésa es la
> señal de un orden de integración mal fijado que los tests de ruido blanco no ven.
>
> En EA esto se cierra en el Paso 6: el MA(1) regular estimado θ≈.43 **es ese testigo**
> — invertible y lejos de 1 ⇒ d=1 confirmado.
>
> **Estado en ART.** `formal_tests` **sí** trae el **DCD regular (H₀: θ=1)** y el
> **Shin-Fuller**, así que el contraste f=0 se puede hacer estimando ∇+MA(1) y leyendo
> el DCD. Lo que **falta**: (a) un paso guiado simétrico tipo `meg_frequency(freq=0)`
> —`meg_frequency` hoy rechaza freq=0 (`1 ≤ f ≤ s/2`)— que imponga ∇, meta el testigo
> y devuelva el DCD en una sola llamada; y (b) el **diagnóstico de residuos en ∩ (U
> invertida)** como disparador automático. (Anotado para art-python.)

## Paso 3 — Diferenciación estacional / MEG: ¿D? ¿frecuencias estocásticas?

**Lo que dice ART.** La serie con solo diferencia regular (∇EA) conserva
autocorrelación estacional **fuerte y positiva** en los retardos 4, 8, 12
(ACF ≈ 0.41, 0.47, 0.30 — el 8 es *mayor* que el 4). HAC conjunto: F(3,64)=13.3,
p=0.0000, con **ambas** frecuencias significativas: f=1 (π/2, χ²=26.1) y f=2 (π,
χ²=16.8). Estacionalidad detectada, inequívoca.

**Decisión de D — D=1 (∇∇₄).**

**Criterio — raíz unitaria estacional.** Autocorrelación estacional persistente y
**positiva** en múltiplos de s *tras* la diferencia regular ⇒ raíz unitaria en las
frecuencias estacionales ⇒ diferencia estacional, D=1. Esto **cierra el Paso 2**: la
ambigüedad de d=1 era esta raíz estacional, no un segundo cero regular.

**Decisión de método — cómo llegar al MEG (B1-base → contraste → reformular).** La
Tabla 4 da λ₁ y λ₂ **separados por frecuencia**, no un único Θ multiplicativo
(airline). El camino disciplinado no es *asumir* ∇∇₄ estocástico (ruta B2 airline),
sino **ganárselo**:

1. **B1**: línea base con D=0 + armónicos deterministas en ambas frecuencias.
2. **Contraste MEG por frecuencia** (`meg_frequency`, el DCD_f): el testigo MA sale
   invertible ⇒ raíz estacional genuina ⇒ **estocástica**; se pega a −1 ⇒
   **determinista**.
3. **`meg_reformulate`** las frecuencias que resulten estocásticas.

> **Heurística (ganarse lo estocástico).** No se asume estacionalidad estocástica: se
> **contrasta**. Se parte del modelo determinista (armónicos) y se pasa a estocástico
> **solo** al rechazar la no invertibilidad del testigo MA de esa frecuencia. Es la
> dirección que fija Relloso (1997): «a los modelos con mayor grado de comportamiento
> determinista se llega tras haber contrastado y no rechazado la no invertibilidad».

Para EA se **espera** que ambas frecuencias rechacen (salgan estocásticas) → ∇∇₄ con
λ₁, λ₂, que es el control de la Tabla 4. Se verifica en los pasos siguientes.

> **Mejora / bug a revisar en ART.** El subgráfico HAC rotula "EA [100·log, d=1]" pese
> a haber fijado λ=1 (niveles). Posible artefacto de display en `seasonal_analysis`
> (¿transforma a log internamente para la detección?). No cambia la decisión —la
> detección estacional es robusta a escala— pero conviene revisarlo en art-python.

## Paso 4 — Modelo base m00 (armónicos, sin ARMA) e intervenciones

**m00 estimado** (∇, 1 par de armónicos + Nyquist, sin ARMA): diagnosis APROBADA
(Q(15)=10.9 ruido blanco, JB p=0.55 normal), σ̂ₐ ≈ 31.5 (aún por encima del control
27.3: falta ARMA, estacional estocástica e intervenciones). Aviso de
sobreparametrización: corr(cos₁, sin₁)=+1.00 en la frecuencia π/2 — pista hacia la
reformulación estocástica (que elimina ese par cos/sin).

**Escaneo de anómalos.** A umbral 3.5σ y también a 2.5σ: **ningún extremo** en ∇EA
(las mayores desviaciones son ~±2σ). Los 5 escalones del control (magnitudes
94/59/−85/−73/50 sobre σ≈40 ⇒ z≈2.0–2.4) son **reales pero sutiles**, por debajo del
umbral. ART confirma que las ACF/PACF **no están distorsionadas**.

**Decisión de intervenciones — por información extramuestral.** El criterio no es el
tamaño del residuo, sino si el evento está **justificado fuera de la muestra**.
Relloso (1997, pp. 3-4) documenta tres eventos:

| evento | fecha | tratamiento | justificación |
|--------|-------|-------------|---------------|
| cambio metodológico EPA | **II/1987** | **escalón en el modelo** | series no homogéneas en definiciones |
| eventuales agrarios (Andalucía/Extremadura) | II/1984 | corrección de datos (III/76–IV/83) | Informe Anual BdE 1984 |
| Ceuta y Melilla en la EPA | II/1988 | corrección de datos (III/76–I/88) | Informe Anual BdE 1988; Boletín INE 29 |

- **II/87 se AÑADE** (escalón): justificado por información extramuestral (cambio
  metodológico), aunque el escaneo no lo marque.
- **I/85, I/89, IV/89, I/92**: sin justificación extramuestral documentada → son
  incidentes influyentes empíricos; se difieren al t-test sobre el modelo refinado.
- **1984 y 1988 no son términos del modelo**: son correcciones ya aplicadas a los
  datos de `m1.inp` (por eso no hay escalones en esas fechas).

> **Heurística (intervención justificada extramuestralmente).** Una intervención
> respaldada por un evento conocido (cambio metodológico, ruptura documentada) se
> añade **aunque el escaneo de residuos no la marque**: el conocimiento del evento es
> evidencia más fuerte que un t-ratio sub-umbral. Distínguela del *incidente
> influyente* puramente empírico, que sí se decide por significancia — y del evento
> que se corrige **en los datos** (no como término del modelo). Tres categorías, tres
> tratamientos.

**m01 = m00 + escalón II/87.** El escalón queda con **t ≈ 0.99** (coef ≈ 39, SE ≈ 39),
**no significativo todavía**, y el AIC empeora ligeramente. No se descarta: la
justificación es extramuestral. En el modelo final de Relloso el mismo escalón
(SII/87 = 63.3, SE 20.1) tiene **t ≈ 3.1** — su SE se estrechará de ~39 a ~20 al añadir
el estacional estocástico y el ARMA regular. Se mantiene y se reevalúa al final.

## Paso 5 — Contraste MEG por frecuencia: ¿estocástica o determinista?

*(en curso)*
