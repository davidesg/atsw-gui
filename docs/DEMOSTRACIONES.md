# Demostraciones completas de los teoremas de `drvec`

> **Documento de demostraciones, compañero de [`THEORY.md`](THEORY.md).**
> `THEORY.md` enuncia los teoremas y los etiqueta `[standard]` / `[new here]` /
> `[empirical]`; este documento los **prueba completos**, y añade los que
> `THEORY.md` usa sin probar (la herencia del MA del BVECM, la factorización de la
> escalera, la identificación, y la teoría asintótica que sostiene la inferencia).
> Cada teorema lleva su **relevancia para el código**. No se pisa `THEORY.md`: los
> números de teorema (T1…T7 y sus corolarios) coinciden con los suyos para poder
> leerlos en paralelo.
>
> Convención de notación: la de [`MODEL.md`](MODEL.md) y [`THEORY.md`](THEORY.md).
> `L` es el operador de retardos; las matrices van en mayúscula, los polinomios
> matriciales en `Φ(L)`, `Θ(L)`, `F(L)`.

---

## 0. El modelo y la notación

El modelo estimado es el VEC de Mauricio (2006, ec. 5), en la forma de `drvec`:

**(1)**  `F(L) ∇Yₜ = −Λ B′Yₜ₋₁ + Θ(L) Aₜ`,

con

```
F(L) = I_M − Σ_{i=1}^{p−1} Fᵢ Lⁱ
Θ(L) = I_M − Σ_{j=1}^{q}  Θⱼ Lʲ
Π    = Λ B′,   rango(Π) = r,   B = [I_r ; B₂],   B₂ de tamaño s × r,  s = M − r
Aₜ   ~ iid N(0, Σ),  Σ > 0
```

`Λ` es `M × r` de rango columna completo, `B₂` es libre, `Wₜ = B′Yₜ = Y₁ₜ + B₂′Y₂ₜ`.
Recogiendo términos, (1) es el VARMA `Φ(L)Yₜ = Θ(L)Aₜ` con

**(2)**  `Φ(L) = F(L)(1−L) + ΛB′L`,  de modo que  `Φ(1) = ΛB′ = Π`.

`Λ⊥` y `B⊥` son `M × s` de rango columna completo con `Λ⊥′Λ = 0` y `B⊥′B = 0`.
La partición es `Yₜ = (Y₁ₜ′ , Y₂ₜ′)′` con `Y₁ₜ` de dimensión `r` (el bloque
normalizado) y `Y₂ₜ` de dimensión `s`.

**Hipótesis de la clase 𝒞** (el conjunto de parámetros para el que el modelo es,
de verdad, un I(1) de rango exactamente `r`):

- **(a)** `|Θ(x)| ≠ 0` para `|x| < 1` (el MA es invertible);
- **(b)** `|Φ(x)| = 0` tiene exactamente `s = M − r` raíces en `x = 1` y el resto
  fuera del disco unitario cerrado;
- **(c)** `det(Λ⊥′F(1)B⊥) ≠ 0`.

---

## 1. La transformación de Mauricio

### Teorema 1 (equivalencia VEC ⟺ VARMA estacionario)  `[standard]`

**Enunciado.** Sean

```
C̄    = [ 0_{s×r}  I_s ;  I_r  B₂′ ],      C̄⁻¹ = [ −B₂′  I_r ;  I_s  0 ],
H̄    = [ 0_{s×s}  0_{s×r} ;  0_{r×s}  I_r ],
Λ̄    = [ 0_{M×s} ,  Λ ],
Ȳₜ    = ( ∇Y₂ₜ′ , Wₜ′ )′.
```

Entonces el modelo (1) es **equivalente** al VARMA

**(3)**  `Φ*(L) Ȳₜ = Θ*(L) A*ₜ`,

con

```
Φ̄₀ = C̄⁻¹,
Φ̄₁ = C̄⁻¹H̄ − Λ̄ + F₁C̄⁻¹,
Φ̄ᵢ = FᵢC̄⁻¹ − Fᵢ₋₁C̄⁻¹H̄    (i = 2,…,p−1),
Φ̄ₚ = −Fₚ₋₁C̄⁻¹H̄,
Φ*ᵢ = C̄Φ̄ᵢ,    Θ*ⱼ = C̄ΘⱼC̄⁻¹,    A*ₜ = C̄Aₜ,    Σ* = C̄ΣC̄′.
```

**Prueba.** Dos pasos.

*Paso 1 — las identidades algebraicas.* Como `Wₜ = Y₁ₜ + B₂′Y₂ₜ`, se tiene
`ΛB′Yₜ₋₁ = ΛWₜ₋₁ = Λ̄ Ȳₜ₋₁`, que es la ec. (9) del paper. Además

```
C̄∇Yₜ = [ 0 I_s ; I_r B₂′ ] [ ∇Y₁ₜ ; ∇Y₂ₜ ]
      = [ ∇Y₂ₜ ; ∇Y₁ₜ + B₂′∇Y₂ₜ ]
      = [ ∇Y₂ₜ ; Wₜ − Wₜ₋₁ ]
      = Ȳₜ − H̄ Ȳₜ₋₁,
```

pues `H̄Ȳₜ₋₁ = (0 ; Wₜ₋₁)`. Esto es la ec. (12) del paper. Verifiquemos
`C̄C̄⁻¹ = I_M`:

```
C̄C̄⁻¹ = [ 0 I_s ; I_r B₂′ ] [ −B₂′ I_r ; I_s 0 ]
      = [ 0·(−B₂′) + I_s·I_s ,  0·I_r + I_s·0 ;
          I_r·(−B₂′) + B₂′·I_s , I_r·I_r + B₂′·0 ]
      = [ I_s 0 ; −B₂′ + B₂′ I_r ] = I_M.  ∎
```

En particular `det C̄ = (−1)^{sr}`, luego `C̄` es no singular y la transformación es
reversible.

*Paso 2 — la derivación del VARMA.* Partimos de (1) con `ΛB′Yₜ₋₁ = Λ̄Ȳₜ₋₁`:

```
F(L)∇Yₜ = −Λ̄Ȳₜ₋₁ + Θ(L)Aₜ.
```

Premultiplicando por `C̄`, y usando `C̄∇Yₜ = Ȳₜ − H̄Ȳₜ₋₁`:

```
Ȳₜ − H̄Ȳₜ₋₁ = −C̄Λ̄Ȳₜ₋₁ + Σᵢ C̄Fᵢ∇Yₜ₋ᵢ + C̄Θ(L)Aₜ.
```

Escribiendo `C̄Fᵢ∇Yₜ₋ᵢ = C̄FᵢC̄⁻¹·C̄∇Yₜ₋ᵢ = C̄FᵢC̄⁻¹(Ȳₜ₋ᵢ − H̄Ȳₜ₋ᵢ₋₁)`:

```
Ȳₜ − H̄Ȳₜ₋₁ = −C̄Λ̄Ȳₜ₋₁ + Σᵢ C̄FᵢC̄⁻¹(Ȳₜ₋ᵢ − H̄Ȳₜ₋ᵢ₋₁) + C̄Θ(L)Aₜ.
```

Premultiplicando por `C̄⁻¹` y agrupando los coeficientes de `Ȳₜ₋ₖ` se obtiene
exactamente

```
Φ̄₀Ȳₜ = Σ_{k=1}^{p} Φ̄ₖ Ȳₜ₋ₖ + Θ(L)Aₜ,
```

con los `Φ̄ₖ` del enunciado (ecs. 15–16 del paper): el coeficiente de `Ȳₜ` es
`Φ̄₀ = C̄⁻¹`; el de `Ȳₜ₋₁` recoge `C̄⁻¹H̄` (del `H̄Ȳₜ₋₁`), `−C̄⁻¹C̄Λ̄ = −Λ̄`
(del término de corrección de error) y `F₁C̄⁻¹` (del sumando `i=1`); los de
`Ȳₜ₋ᵢ`, `i=2…p−1`, recogen `FᵢC̄⁻¹ − Fᵢ₋₁C̄⁻¹H̄`; y el de `Ȳₜ₋ₚ` es
`−Fₚ₋₁C̄⁻¹H̄`. Normalizando por `C̄ = Φ̄₀⁻¹` y sustituyendo `Aₜ = C̄⁻¹A*ₜ`:

```
Φ̄₀⁻¹Φ̄₀Ȳₜ = Σₖ Φ̄₀⁻¹Φ̄ₖȲₜ₋ₖ + Φ̄₀⁻¹Θ(L)C̄⁻¹A*ₜ
⟹ Ȳₜ = Σₖ Φ*ₖȲₜ₋ₖ + Θ*(L)A*ₜ,
```

que es (3). ∎

*Biyección.* Para datos fijos, el paso `(Λ, B₂, Fᵢ, Θⱼ, Σ) ↦ (Φ*ₖ, Θ*ⱼ, Σ*, Ȳₜ)` es
una biyección: `C̄` es no singular, de modo que cada etapa se invierte
(`Θⱼ = C̄⁻¹Θ*ⱼC̄`, `Σ = C̄⁻¹Σ*C̄⁻¹′`, `Fᵢ` se recuperan resolviendo el sistema
triangular de los `Φ̄ᵢ` hacia atrás). Para `B₂` fijo, `Θ ↦ Θ*` es una semejanza.

**Relevancia para el código.** `vec_shootx` en `src/drvec.c` es exactamente esta
derivación: construye `C̄`, `H̄`, `Λ̄` (ecs. 10, 13), los `Φ̄ᵢ` (ec. 16) y luego
`Φ*ₖ = C̄Φ̄ₖ`, `Θ*ⱼ = C̄ΘⱼC̄⁻¹`, `Σ* = C̄ΣC̄′` y `Ȳₜ` a partir de `B₂`. La
verificación del repo contra la forma cerrada del AddOn (M=2, r=1) da
**0.000e+00**, y la verificación numérica con un Θ general no escalar da
**4.2·10⁻¹⁵**. Los corolarios siguientes son los que permiten transferir
afirmaciones sobre `Θ` al sistema transformado que el motor estima.

### Corolario 1.1 (invariancia de las raíces determinantes del MA)  `[new here]`

**Enunciado.** `Θ*(x)` y `Θ(x)` tienen las mismas raíces determinantes; en
particular `det Θ*(1) = det Θ(1)`.

**Prueba.** `Θ*(x) = C̄Θ(x)C̄⁻¹` para todo `x` (semejanza). El determinante es
invariante por semejanza: `det Θ*(x) = det(C̄)·det Θ(x)·det(C̄⁻¹) = det Θ(x)`. ∎

**Relevancia para el código.** Cualquier enunciado de invertibilidad del sistema
transformado es un enunciado sobre `Θ`, y la patología de la Sección 3 es visible
en las dos coordenadas. Es lo que permite leer la raíz MA unitaria en la salida
(`MA (Theta) … 0.99995*`) directamente como una propiedad del MA del VEC.

### Corolario 1.2 (una identidad no crea estructura)  `[new here]`

**Enunciado.** Apelar al Teorema 1 no establece que un VEC con `Fᵢ` y `Θⱼ` libres
admita una representación triangular (WARMA): el Teorema 1 mapea cada punto
paramétrico a exactamente un punto transformado, y la clase triangular es un
subconjunto propio de la imagen (Sección 4).

**Prueba.** El Teorema 1 es una biyección; no restringe. La clase triangular
(Teorema 6) impone `Γᵢ = Mᵢα′` y `Θ̃₁ = [Θ_w, Θ_wB₂′; 0, 0]`, que no son
consecuencia de la biyección. ∎

**Relevancia para el código.** Es la razón de que estimar `Θ` libre no esté
justificado por el Teorema 1: la sobre-parametrización del MA no desaparece por
transformar. Es el punto de partida del diagnóstico del §4 del registro.

---

## 2. Representación de Granger y condición de rango

### Teorema 2 (impacto de largo plazo, Granger con MA)  `[standard para el VAR; reescrito aquí para (1)]`

**Enunciado.** Bajo (a)–(c), existe una descomposición

**(4)**  `Yₜ = C Θ(1) Σ_{u=1}^{t} Aᵤ + ξₜ + ζ`,

con `C = B⊥(Λ⊥′F(1)B⊥)⁻¹Λ⊥′`, `{ξₜ}` estacionario, y `ζ` dependiendo de las
condiciones iniciales.

**Prueba.** Escribimos `Yₜ = Φ(L)⁻¹Θ(L)Aₜ`. Por (b), `Φ(x)⁻¹` tiene un polo
simple en `x = 1`; por tanto

```
Φ(x)⁻¹ = C (1−x)⁻¹ + H(x),
```

con `H(x)` analítica en un entorno del disco unitario cerrado (los demás ceros de
`Φ` están fuera, por (b)). Determinamos `C` imponiendo `Φ(x)Φ(x)⁻¹ = I`.

Desarrollamos `Φ` en `x = 1`: `Φ(1) = ΛB′`, `Φ′(1) = −F(1) + ΛB′`. Entonces

```
Φ(x)[C/(1−x) + H(x)] = I
⟹ [ΛB′ + (x−1)(−F(1)+ΛB′) + O((x−1)²)] C/(1−x) + Φ(x)H(x) = I.
```

El término `Φ(x)C/(1−x)` es analítico si y solo si `ΛB′C = 0`; con ello,

```
Φ(x)C/(1−x) = F(x)C + O(x−1) → F(1)C   cuando x → 1.
```

(se usó `ΛB′C = 0` para cancelar el polo). En `x = 1` la identidad da

```
F(1)C + ΛB′H(1) = I.      (∗)
```

Definimos `C = B⊥(Λ⊥′F(1)B⊥)⁻¹Λ⊥′`, bien definida por (c). Comprobamos (∗):

1. `ΛB′C = Λ(B′B⊥)(Λ⊥′F(1)B⊥)⁻¹Λ⊥′ = 0` (pues `B′B⊥ = 0`). ✓
2. `F(1)C = F(1)B⊥(Λ⊥′F(1)B⊥)⁻¹Λ⊥′`. Falta ver que `I − F(1)C` se escribe como
   `ΛB′H(1)`. Basta ver `Λ⊥′(I − F(1)C) = 0`, porque entonces las columnas de
   `I − F(1)C` están en el espacio columna de `Λ` (ortogonal a `Λ⊥`), y como
   `B′` es de rango fila completo `r`, `col(Λ) = col(ΛB′)`. Calculamos

   ```
   Λ⊥′(I − F(1)C) = Λ⊥′ − Λ⊥′F(1)B⊥(Λ⊥′F(1)B⊥)⁻¹Λ⊥′ = Λ⊥′ − Λ⊥′ = 0.  ✓
   ```

   Luego existe `H(1)` con `I − F(1)C = ΛB′H(1)`, y `H(x)` se extiende
   analíticamente a todo el disco (los ceros de `Φ` están fuera; `F(x)C` es un
   polinomio). Por tanto (∗) se cumple y `Φ(x)⁻¹ = C/(1−x) + H(x)` es válido.

Sustituyendo en `Yₜ = Φ(L)⁻¹Θ(L)Aₜ`:

```
Yₜ = C(1−L)⁻¹Θ(L)Aₜ + H(L)Θ(L)Aₜ.
```

Descomponemos `Θ(L) = Θ(1) + (1−L)Θ̃(L)` con `Θ̃` polinomio de grado `q−1`. Entonces

```
(1−L)⁻¹Θ(L)Aₜ = Θ(1)(1−L)⁻¹Aₜ + Θ̃(L)Aₜ = Θ(1) Σ_{u=1}^{t} Aᵤ + (constante de inicio).
```

Definiendo `ξₜ = [CΘ̃(L) + H(L)Θ(L)]Aₜ` y `ζ` el término de condiciones iniciales,
se obtiene (4). `ξₜ` es estacionario: `Θ̃`, `Θ` son polinomios finitos y `H(L)` tiene
coeficientes que decaen exponencialmente (sus polos están fuera del círculo), luego
`CΘ̃(L)+H(L)Θ(L)` es un filtro absolutamente sumable aplicado a ruido blanco. ∎

**Relevancia para el código.** `granger_smin()` en `src/drvec.c` calcula la
condición que hace que este teorema valga con rango exacto (Teorema 3), y el
factor `Θ(1)` del teorema es lo que distingue el caso MA del VAR puro: el largo
plazo es `C·Θ(1)`, no `C`. La condición de Johansen `α⊥′Γβ⊥` de rango completo es
el caso `Θ(1)=I`.

### Teorema 3 (el rango es lo que dicen los parámetros)  `[new here]`

**Enunciado.** Bajo (a)–(c), el espacio de cointegración de `{Yₜ}` es
`{b : b′CΘ(1) = 0}`, de dimensión `M − rango(Λ⊥′Θ(1))`. Por tanto

```
{Yₜ} tiene rango de cointegración exactamente r  ⟺  rango(Λ⊥′Θ(1)) = s,
```

es decir, `Λ⊥′Θ(1)` de rango fila completo; equivalentemente, el espacio nulo
izquierdo de `Θ(1)` corta `col(Λ⊥)` solo en cero.

**Prueba.** Por (4), la tendencia estocástica de `Yₜ` es `CΘ(1)ΣAᵤ`, con `Σ`
no singular. Un vector `b` aniquila la tendencia (i.e. `b′Yₜ` es I(0)) si y solo si
`b′CΘ(1) = 0`. El espacio de esos `b` es el aniquilador de `col(CΘ(1))`, de
dimensión `M − rango(CΘ(1))`. Ahora `C = B⊥KΛ⊥′` con `K = (Λ⊥′F(1)B⊥)⁻¹`
no singular (s×s) y `B⊥` de rango columna completo `s`; luego
`rango(CΘ(1)) = rango(KΛ⊥′Θ(1)) = rango(Λ⊥′Θ(1))`. Las columnas de `B` están
siempre en el espacio de cointegración (`B′C = B′B⊥KΛ⊥′ = 0`), así que la
dimensión es al menos `r`; es exactamente `r` si y solo si
`rango(Λ⊥′Θ(1)) = s`. Para la última frase: `rango(Λ⊥′Θ(1)) < s` si y solo si
existe `c ≠ 0` (s-vector) con `c′Λ⊥′Θ(1) = 0`, es decir, `Λ⊥c` es un vector nulo
izquierdo de `Θ(1)`. ∎

**Relevancia para el código.** Es la condición que el programa reporta como
`Rank condition (Granger): sigma_s(alpha_perp′ Theta(1))` y que
`-rankadm [tol]` puede imponer. (Hasta BUG-46 el programa reportaba
`σ_min(Λ⊥′Θ(1)B⊥)`, una condición más fuerte: suficiente, no necesaria.) Es **la** condición que la literatura asume sobre
el proceso pero no escribe en los parámetros estimados (ver §7). Su violación es
exactamente el lugar donde los ajustes libres se estacionan.

### Corolario 3.1 (condición suficiente y barata)  `[new here]`

**Enunciado.** Si `det Θ(1) ≠ 0` entonces la condición de rango del Teorema 3 se
cumple automáticamente. Toda la dificultad vive en la superficie `det Θ(1) = 0`
(una raíz MA unitaria en frecuencia cero).

**Prueba.** `Λ⊥′` tiene rango fila completo `s` y `Θ(1)` es no singular, luego el
producto tiene rango `s`. ∎

**Relevancia para el código.** `-rankadm` impone
`σ_s(Λ⊥′Θ(1)) ≥ tol` (la del Teorema 3 desde BUG-46); el corolario recuerda que la condición más simple
`det Θ(1) ≠ 0` bastaría y es más barata de enunciar — aunque no distingue
*direcciones* como sí hace la condición de rango (Corolario 3.2).

### Corolario 3.2 (sobrediferenciación, forma rigurosa)  `[new here]`

**Enunciado.** Sea `w ≠ 0` con `w′Θ(1) = 0` (un vector nulo izquierdo de `Θ(1)`).
Entonces:

1. `w′Θ(L) = (1−L)·w′Θ*(L)` — el MA en la dirección `w` lleva una raíz unitaria;
   es la firma de sobrediferenciación (Plosser–Schwert 1977; Hillmer–Tiao 1979).
2. Si `w ∈ col(Λ⊥)`, entonces `rango(Λ⊥′Θ(1)) < s` y, por el Teorema 3, el rango
   verdadero **excede** `r`: el modelo ajustado niega el rango para el que se
   estimó. **Este es el caso fatal.**
3. Si `w ∉ col(Λ⊥)`, la deficiencia no está en la dirección `Λ⊥`;
   `rango(Λ⊥′Θ(1)) = s` (genéricamente) y el modelo conserva rango `r`: la raíz
   MA unitaria vive en la dirección del término de corrección de error (col(Λ)).

**Prueba.** (1) es álgebra: si el polinomio matricial `P(L) = w′Θ(L)` cumple
`P(1) = 0`, entonces `P(L) = (1−L)Q(L)` con `Q` polinomio (factorizar `(1−L)`
coeficiente a coeficiente). (2) es el Teorema 3 aplicado a `c` con `w = Λ⊥c`:
`c′Λ⊥′Θ(1) = w′Θ(1) = 0`, luego `rango(Λ⊥′Θ(1)) < s`. (3): descomponiendo
`w = Λd + Λ⊥c` (suma directa `col(Λ) ⊕ col(Λ⊥) = ℝ^M`), `w ∉ col(Λ⊥)` fuerza
`d ≠ 0`, y la condición `w′Θ(1) = 0` se lee como `d′Λ′Θ(1) = −c′Λ⊥′Θ(1)`, que no
exige ninguna deficiencia de `Λ⊥′Θ(1)`. ∎

**Relevancia para el código.** En los ocho pares de trigo, los ajustes libres caen
con `det Θ̂(1) ≈ 0` y la dirección nula de `Θ̂(1)` alineada con `Λ⊥` al coseno
0.946–0.9996 — exactamente el caso (2), el fatal. El corolario dice *qué* medir
para distinguir un MA unitario inocuo (3) de uno que niega el rango (2): el
alineamiento de la dirección nula con `Λ⊥`, no la raíz unitaria por sí sola.

### Teorema 2′ (estacionariedad del sistema transformado)  `[standard; argumento de Mauricio p. 3650, aquí riguroso]`

**Enunciado.** Bajo (a)–(c), `Ȳₜ = (∇Y₂ₜ′, Wₜ′)′` es estacionario y el VARMA (3)
tiene `|Φ*(x)|` con todas sus raíces fuera del círculo unitario (salvo que la
representación admita una cancelación común no identificada; ver §7).

**Prueba.** Por el Teorema 2, `Yₜ = CΘ(1)ΣAᵤ + ξₜ + ζ` con `ξₜ` estacionario.
Entonces:

- `Wₜ = B′Yₜ = B′ξₜ + B′ζ` (pues `B′C = 0`), que es estacionario.
- `∇Y₂ₜ` es la diferencia de la componente no estacionaria de `Y₂ₜ`; como
  `Y₂ₜ` es I(1) con la tendencia en `col(CΘ(1))`, `∇Y₂ₜ` es estacionario.

Luego `Ȳₜ` es estacionario. Como `Ȳₜ` satisface (3) por álgebra exacta (Teorema 1)
y (3) es un VARMA **únicamente identificado** (sin factor común cancelable entre
`Φ*` y `Θ*`; condición de coprimidad, §7), las raíces de `|Φ*(x)|` están fuera del
círculo unitario: un proceso estacionario que sigue un VARMA identificado tiene su
AR estacionario. ∎

**Relevancia para el código.** Es lo que garantiza que `elf()` (AS 311, pensado
para VARMA estacionarios) sea aplicable al sistema transformado. El matiz de
"únicamente identificado" es el que conecta con la Sección 7: si la identificación
falla, la conclusión de estacionariedad del `Φ*` estimado no está garantizada, y
el motor solo puede chequear raíces *dentro* del círculo (Teorema 5).

---

## 3. La patología: 𝒞 ⊊ 𝒫, y la frontera de invertibilidad

### Lema A (forma de la verosimilitud exacta, AS 311)  `[standard; Mauricio 1997]`

**Enunciado.** La verosimilitud exacta del VARMA estacionario
`Φ(L)wₜ = Θ(L)aₜ`, `aₜ ~ N(0, Σ)`, es

```
log L = −(n m/2)(log 2π + log σ²) − (n/2) log det Σ − (1/2) log det Ω − f₁/(2σ²),
```

donde `f₁` es la forma cuadrática corregida por las condiciones iniciales,
`Ω = I + M′H′HM` es la matriz de covarianza exacta de las `g = max(p,q)`
observaciones iniciales (el "determinante" de la corrección), y `f₁`/`Ω` se
construyen con la secuencia `ξ_r` (los coeficientes de `Θ(L)⁻¹`) truncada cuando
`Σ|ξ_r[i][j]| < xitol`.

En el código (`elfvarma.c`): `f₁ = Σaₜ² − Σvechhᵢ²`, `f₂ = detq·exp(log(detom)/n)`
con `detom = det(I + M′H′HM)`, `detq = det Σ`, y la puerta de invertibilidad
(`chekma`) rechaza el punto si algún valor propio de la compañera del MA satisface
`|λ| ≥ 1.00005`.

**Prueba.** Es la construcción de Mauricio (1995, 1997): la densidad conjunta de
`(w₁,…,wₙ)` se factoriza como la densidad de las primeras `g` observaciones (con
covarianza exacta `Ω`) por la densidad condicional del resto, y el truco del
algoritmo es escribir la corrección de las iniciales como un término cuadrático
`vechh` resoluble por Cholesky. No se reproduce aquí en detalle; se da la forma
porque los Teoremas 4–5 y 9 la usan explícitamente.

**Relevancia para el código.** (i) `det Ω > 0` y `det Σ > 0` mantienen `log L`
finito **incluso con raíz MA unitaria** (Teorema 4); (ii) la truncación de `ξ`
con `xitol = 1e−3` es la única aproximación del motor, y es la fuente del residuo
`1.4·10⁻⁴` en la identidad de factorización (Teorema 9); (iii) `chekma` con
umbral `1.00005` admite raíces hasta `0.99995` (ligeramente dentro del círculo) y
es la "puerta" donde los ajustes libres se estacionan.

### Teorema 4 (𝒞 es un subconjunto propio de 𝒫)  `[new here]`

**Enunciado.** `det Θ(1) = 0` está permitido por la definición de 𝒫; la
verosimilitud gaussiana exacta es finita y continua allí; y los puntos de esa
superficie con `nulo(Θ(1)′) ∩ col(Λ⊥) ≠ {0}` **no** están en 𝒞. Por tanto el
maximizador de la verosimilitud sobre 𝒫 no tiene por qué estar en 𝒞.

**Prueba.** 𝒫 admite raíces *sobre* el círculo unitario (la puerta rechaza solo
los puntos estrictamente *dentro*; `chekma` permite `|λ| < 1.00005`), y
`det Θ(1) = 0` es una raíz en `x = 1`. La no invertibilidad **en** la frontera
deja la matriz de covarianza de `(Ȳ₁,…,Ȳₙ)` no singular: una raíz MA unitaria
anula la densidad espectral en frecuencia cero, que es un evento de medida cero
para una forma de Toeplitz finita. Caso escalar `(1−B)aₜ`: la covarianza es la
matriz tridiagonal `(2, −1)`, cuyos valores propios son `2 + 2cos(kπ/(n+1)) > 0`
para todo `k`, luego no singular para todo `n`. Por el Lema A, `log L` es finito y
continuo hasta la superficie inclusive; el optimizador no tiene ninguna razón para
no visitarla. La última afirmación es el Teorema 3. ∎

**Relevancia para el código.** Es la explicación teórica de por qué los ajustes
libres caen en `det Θ̂(1) ≈ 0` con la dirección nula en `Λ⊥`: la verosimilitud no
penaliza la superficie, y el LR la *premia* (13–26 unidades). Ningún chequeo de
raíces lo impide (Teorema 5).

### Teorema 5 (la patología es invisible a los chequeos del motor)  `[new here]`

**Enunciado.** Sea `θ ∈ 𝒫 \ 𝒞` como en el Teorema 4. Entonces `Ȳₜ` sigue siendo
estacionario, así que ningún test sobre las raíces de `Φ*` detecta el fallo. El
fallo es visible **solo** como raíz unitaria de `Θ*`, que 𝒫 permite.

**Prueba.** `Wₜ = B′Yₜ` es estacionario por construcción del modelo (la fila de
`B′` aniquila la tendencia: `B′C = 0`, independientemente de `Θ(1)`), y `∇Y₂ₜ` es
una diferencia de un bloque I(1), luego estacionario; por tanto `Ȳₜ` es
estacionario y `Φ*` tiene todas sus raíces fuera del círculo. Por el Corolario 1.1,
`det Θ*(1) = det Θ(1) = 0`. ∎

**Relevancia para el código.** La condición tiene que calcularse sobre
`(Λ, B, Θ)` directamente, que es lo que hace `granger_smin()` en **cada**
evaluación. La salida empírica lo confirma: raíces AR `1.996, 1.996, ∞, 1.715`
(estacionarias) junto a una raíz MA mínima `1.00026` — estacionario y en la
frontera de invertibilidad a la vez, la firma exacta del Teorema 5.

---

## 4. La clase triangular (WARMA)

### Teorema 6 (imagen de la clase triangular, condición necesaria y suficiente)  `[new here; el BVECM enuncia el sentido directo y afirma el recíproco]`

**Enunciado.** Sea la forma triangular (Phillips 1991) en notación del BVECM:
`Φ_w(B)wₜ = Θ_w(B)aₜ`, `Δz₂ₜ = Σ_{j}Ψ_j wₜ₋₁₋ⱼ + ηₜ`, con `wₜ = α′zₜ`,
`α = [I_r; −β]`. La representación VEC de tal proceso satisface:

**(5)**  `Γᵢ = Mᵢ α′` para todo `i` — cada matriz de corto plazo tiene rango ≤ r
y espacio de filas contenido en `fila(α′)`; y

**(6)**  `Θ̃₁ = [Θ_w, −Θ_wβ′; 0, 0] = [Θ_w, Θ_wB₂′; 0, 0]` (con `B₂ = −β`),

es decir, el bloque diferenciado no lleva MA propio y el bloque cruzado está
determinado. Recíprocamente, un VEC (1) admite una representación triangular de
orden finito **si y solo si** sus parámetros cumplen (5) y (6). En particular, el
recíproco afirmado en el apéndice del BVECM — que *cualquier* VEC de rango `r`
se escribe en forma triangular — es **falso** tal como está enunciado.

**Prueba.** *Sentido directo.* Es la construcción del BVECM (Theorem 1 + Steps 1–5).
Con `p* = max(p, k+1)`, definiendo `M₁ = [β′γ + Φ₁ − I_r ; γ]` y
`Mₗ = [β′Ψₗ₋₁ + Φₗ ; Ψₗ₋₁]` para `l ≥ 2`, la identidad telescópica
`wₜ₋ₗ = wₜ₋₁ − Σ_{i=1}^{l−1}α′Δzₜ₋ᵢ` da

```
Δzₜ = Σₗ Mₗ wₜ₋ₗ + εₜ = (Σₗ Mₗ) α′zₜ₋₁ − Σᵢ (Σ_{l>i} Mₗ) α′ Δzₜ₋ᵢ + εₜ,
```

con `εₜ = (aₜ + β′ηₜ; ηₜ)`. Esto da `A = ΣₗMₗ` y `Γᵢ = −Σ_{l>i}Mₗα′`, es decir
(5). Con MA (`aₜ ↦ Θ_w(B)aₜ`), `εₜ = (β′ηₜ + Θ_w(B)aₜ; ηₜ)`; reagrupando sobre
`Aₜ = (aₜ + β′ηₜ; ηₜ)′` (transformación invertible del ruido) se obtiene
`εₜ = Aₜ − Θ̃₁Aₜ₋₁` con `Θ̃₁` como en (6). ∎

*Sentido recíproco.* Supóngase un VEC con (5) y (6). Por (5) y `A = ΣMₗ`, la
identidad telescópica se invierte término a término (es una biyección sobre los
retardos), así que

```
Δzₜ = Σₗ Mₗ wₜ₋ₗ + Aₜ − Θ̃₁Aₜ₋₁.
```

Por (6), `Θ̃₁Aₜ₋₁ = [Θ_w aₜ₋₁; 0]` (pues `Aₜ = (aₜ + β′ηₜ; ηₜ)` y `β′ = −B₂′`
cancela el bloque cruzado). Luego `Aₜ − Θ̃₁Aₜ₋₁ = (Θ_w(B)aₜ + β′ηₜ; ηₜ)`. La
fila inferior de la ecuación da

```
Δz₂ₜ = Σₗ Ψₗ₋₁ wₜ₋ₗ + ηₜ = γ wₜ₋₁ + Σ_{j≥1} Ψ_j wₜ₋₁₋ⱼ + ηₜ.
```

La fila superior, restando `β′Δz₂ₜ` y usando `Δz₁ₜ = Δwₜ + β′Δz₂ₜ`, cancela
exactamente los términos en `β′` y deja

```
Δwₜ = (Φ₁ − I_r) wₜ₋₁ + Σ_{j≥2} Φ_j wₜ₋ⱼ + Θ_w(B) aₜ,
```

es decir `Φ_w(B)wₜ = Θ_w(B)aₜ`. Se ha reconstruido la forma triangular. ∎

*Necesidad de (5) y (6).* Si (5) falla para algún `i`, existe `v` con `α′v = 0` y
`Γᵢ v ≠ 0`: entonces `Δzₜ` depende de `Δzₜ₋ᵢ` fuera del espacio generado por `w`,
lo que ninguna ecuación de la forma triangular (que solo usa `w` como argumento
dinámico) puede producir. Si (6) falla, el bloque diferenciado lleva MA propio, que
la forma triangular no tiene. ∎

**Relevancia para el código.** (6) es la estructura que `-mawarma` impone
(bloque `r×r` libre, cruzado `T₁₁B₂′`, filas inferiores cero) y `-marow` la mitad
(filas inferiores cero, cruzado libre). El recíproco falso del BVECM es la raíz del
"Θ libre" de Mauricio. El Teorema 6 es, con el Teorema 3, el fundamento teórico de
la escalera de especificaciones (`-specs`).

### Corolario 6.1 (las restricciones en coordenadas transformadas)  `[new here]`

**Enunciado.** En las coordenadas `Ȳ = [∇Y₂; W]` las restricciones del Teorema 6
son un patrón de ceros y `B₂` desaparece de ellas:

| restricción sobre `Θ` | `Θ*` sobre `Ȳ = [∇Y₂ ; W]` |
|---|---|
| libre | libre |
| `[T₁₁ T₁₂ ; 0 T₂₂]` | `[[T₂₂, 0],[·, T₁₁]]` — la ecuación de `∇Y₂` conserva MA |
| `[T₁₁ T₁₂ ; 0 0]` | `[[0, 0],[·, T₁₁]]` |
| `[T₁₁ T₁₁B₂′ ; 0 0]` | `[[0, 0],[0, T₁₁]]` — diagonal |

**Prueba.** `Θ* = C̄ΘC̄⁻¹` con `C̄ = [0 I_s; I_r B₂′]`; es un cómputo directo de
semejanza por bloques. Para la última fila: `Θ = [T₁₁ T₁₁B₂′; 0 0]` da
`Θ* = C̄[T₁₁ T₁₁B₂′; 0 0]C̄⁻¹ = [[0,0],[0,T₁₁]]`. ∎

**Relevancia para el código.** Es la razón teórica de `-warma`: estimar la clase
en coordenadas transformadas, donde las restricciones son ceros (y `B₂` entra solo
por `W = Y₁ + B₂′Y₂`, "por sustracción, como el input de una función de
transferencia"). El resultado medido: ocho convergencias por gradiente de ocho,
frente a cero en la versión libre.

### Corolario 6.2 (qué cero lleva la admisibilidad)  `[new here]`

**Enunciado.** Bajo (6), `Θ(1) = [I − Θ_w, −Θ_wB₂′; 0, I]` (triangular por bloques
con identidad en el bloque inferior), luego `det Θ(1) = det(I − Θ_w) ≠ 0` salvo que
`Θ_w` tenga un valor propio unitario. La degeneración del Teorema 4 es por tanto
**imposible en el bloque diferenciado** y posible solo en el cointegrado. Anular el
cero inferior-izquierdo manteniendo `T₂₂` libre no compra esto; anular `T₂₂` sí.

**Prueba.** Bajo (6), `Θ(L) = [Θ_w(L), Θ_w(L)B₂′; 0, I_s]`; evaluando en `L = 1`
y usando que `I − Θ_w` y `I_s` forman los bloques diagonales, `det Θ(1) =
det(I−Θ_w(1))·det(I_s) = det(I−Θ_w(1))`. El resto es el Teorema 3. ∎

**Relevancia para el código.** Es lo que midió §4j del registro: `-matri`
(libera `T₂₂`, anula solo el bloque (2,1)) **no** elimina la patología, mientras
que `-marow` (anula `T₂₂`) sí. Con `T₂₂` libre el optimizador lo lleva a uno — el
factor `(1−B)` sobre el bloque ya diferenciado, el modelo deshaciendo su propia
diferenciación.

### Corolario 6.3 (la puerta del motor **es** la condición de admisibilidad)  `[new here]`

> **FALSO tal como está enunciado (2026-09-23).** Los puntos (1) y (2) son
> correctos; la conclusión —«la puerta del motor ES la condición de
> admisibilidad» y «el punto degenerado no es alcanzable en `-marow`»— no lo es,
> en los dos sentidos: `Θ₁ = [[1, .7], [0, 0]]` pasa `chekma` y es inadmisible;
> `Θ₁ = [[3, .4], [0, 0]]` es rechazado y es admisible. La puerta comprueba
> invertibilidad; la admisibilidad es una condición en `z = 1`. Contraejemplos y
> la versión correcta en `estudio_mauricio_2026-09-23/M3/STUDY_M3.md`. El defecto
> por defecto que se apoyaba aquí se ha revertido (BUGS.md BUG-48).

**Enunciado.** Sea `Θ̃(L) = I − Σ_{k=1}^{q}Θ̃_kL^k` con las `s` filas inferiores de
**cada** `Θ̃_k` nulas — la condición (6) del Teorema 6, pero también la clase más
débil que solo anula esas filas y deja libre el bloque cruzado. Escríbase
`T_k = Θ̃_k[1..r, 1..r]`. Entonces, para todo `q ≥ 1` y todo `(r, s)`:

1. `det Θ̃(1) = det(I_r − Σ_k T_k)`;
2. los valores propios de la matriz compañera `Mq × Mq` de `Θ̃(L)` son los de la
   compañera `rq × rq` del bloque `r × r`, más `sq` ceros;
3. por tanto `Θ̃(L)` es invertible **si y solo si** lo es el bloque `r × r`, y por
   el Corolario 3.1 la condición de rango del Teorema 3 se cumple
   automáticamente en cuanto lo sea.

**Consecuencia.** El punto de `𝒫 \ 𝒞` que el Teorema 4 muestra que la
verosimilitud alcanza y premia, y que el Teorema 5 muestra que **ningún chequeo
de raíces del motor puede ver, no es alcanzable en esta clase**: ahí las dos
condiciones coinciden y el chequeo que el motor ya hace —`chekma` sobre `Θ̃`— es
exactamente el chequeo sobre el bloque `r × r`. El Teorema 5 es un enunciado
**sobre la clase libre**.

**Prueba.** (1) Con las filas inferiores nulas, `Σ_kΘ̃_k = [ΣT_k, ΣC_k; 0, 0]`
para bloques cruzados `C_k` cualesquiera, luego
`Θ̃(1) = I − Σ_kΘ̃_k = [I_r − ΣT_k, −ΣC_k; 0, I_s]`, triangular superior por
bloques, y su determinante es el producto de los de la diagonal:
`det(I_r − ΣT_k)·det(I_s)`. El bloque cruzado no interviene.

(2) La compañera de `Θ̃(L)` es `𝒞 = [Θ̃_1 … Θ̃_q ; I 0 … ; ⋱]`. Permútense
filas y columnas agrupando, dentro de cada bloque de retardo, las `r` primeras
coordenadas antes que las `s` últimas. En esa base `𝒞` es triangular por
bloques: el bloque diagonal superior es la compañera `rq × rq` del bloque
`r × r` — porque las filas inferiores de cada `Θ̃_k` son nulas, ninguna
coordenada del bloque `s` alimenta a las del bloque `r` —, y el bloque diagonal
inferior es la compañera de un operador **idénticamente nulo** en esas `s`
filas, es decir nilpotente, con `sq` valores propios en cero. Los valores
propios de una matriz triangular por bloques son los de sus bloques diagonales. ∎

(3) es (2) más el Corolario 3.1.

**Comprobado numéricamente**, y no solo demostrado, para `(r,s,q)` = (1,1,1),
(2,3,1), (2,1,2), (3,2,3) y (1,4,2), con bloques superiores aleatorios: la
diferencia en (1) es **0.00e+00** y la de (2) es a lo sumo **1.3·10⁻¹⁵**
(`tools/sim/ma_identification.py algebra`).

**Qué añade sobre el Corolario 6.2.** C6.2 da (1) para `q = 1` bajo la
estructura completa de (6). Lo nuevo es que (1) vale para todo `q` y **sin** el
bloque cruzado heredado —basta la fila inferior nula, que es lo que `-marow`
impone y lo que §4j del registro había localizado midiendo—, y sobre todo (2),
que es lo que conecta la condición con el chequeo que el motor ya ejecuta.

**Relevancia para el código.** Cambia el estatus de `-mawarma` y `-marow`. No son
restricciones sobre `𝒫` cuyo cumplimiento haya que comprobar y cuyo óptimo esté
sesgado por una cota que liga: son **parametrizaciones en las que `𝒞` es el
espacio entero**. El argumento de `SPECIFICATION_PLAN.md` §8 para no hacer
admisible el defecto —que `-rankadm` da una cota sesgada a la baja— es correcto
sobre `-rankadm` y **no se aplica** aquí, porque aquí no hay cota. Con `M = 2`,
`r = 1` la condición de admisibilidad pasa de ser un rango de una matriz `2 × 2`
que el motor no ve a ser `|θ_w| < 1`, un escalar que el motor ya comprueba.

### Teorema 6b (Corolario 2 del BVECM: la herencia del MA)  `[BVECM, inédito]`

**Enunciado.** Sea `{zₜ}` la WARMA completa `Φ_w(B)wₜ = Θ_w(B)aₜ`,
`Δz₂ₜ = ΣΨⱼwₜ₋₁₋ⱼ + ηₜ`, con `Θ_w(0) = I_r` y raíces de `|Θ_w|` fuera del
círculo. Entonces la VEC equivalente tiene la **misma** `α, A, Γᵢ` que en el caso
sin MA (Teorema 6) e innovación `εₜ = (β′ηₜ + Θ_w(B)aₜ; ηₜ)`. Reagrupando sobre
`Aₜ = (aₜ + β′ηₜ; ηₜ)′`, invertible,

```
εₜ = Aₜ − Θ̃₁ Aₜ₋₁,   con   Θ̃₁ = [[Θ_w, −Θ_wβ′],[0,0]] = [[Θ_w, Θ_wB₂′],[0,0]].
```

**Prueba.** Pasos 1–3 del Teorema 6 con `aₜ` reemplazado por `Θ_w(B)aₜ`. El único
cambio es que la ecuación de `Δwₜ` pasa a ser

```
Δwₜ = (Φ₁ − I_r)wₜ₋₁ + Σ_{j≥2}Φⱼwₜ₋ⱼ + Θ_w(B)aₜ,
```

de modo que la innovación del sistema apilado es `εₜ = (β′ηₜ + Θ_w(B)aₜ; ηₜ)′`.
Las matrices `Mₗ` (incluido el `M₁` corregido) no cambian, porque el MA entra solo
por el término de error; por tanto `A` y `Γᵢ` son idénticos al caso sin MA. El
reagrupamiento: `aₜ = A₁ₜ − β′A₂ₜ` (pues `Aₜ = (aₜ + β′ηₜ; ηₜ)`), luego

```
ε₁ₜ = β′ηₜ + Θ_w(B)aₜ
    = A₁ₜ − aₜ + Θ_w(B)aₜ
    = A₁ₜ − (I_r − Θ_w(B))aₜ
    = A₁ₜ − (I_r − Θ_w(B))(A₁ₜ − β′A₂ₜ)
    = A₁ₜ − ΣⱼΘ_w,ⱼ(A₁,ₜ₋ⱼ − β′A₂,ₜ₋ⱼ),
```

que en forma matricial es `εₜ = Aₜ − Θ̃₁Aₜ₋₁` con `Θ̃₁ = [[Θ_w, −Θ_wβ′],[0,0]]`.
Sustituyendo `β′ = −B₂′` se obtiene `[[Θ_w, Θ_wB₂′],[0,0]]`. ∎

**Relevancia para el código.** Es **la** herencia del MA: con `M=2, r=1` deja
**un** parámetro MA libre (el `Θ_w₁₁`) donde el `Θ` libre estimaba **cuatro**. El
código lo implementa en `-mawarma` (`Theta[k][i][r+jj] = Σ T₁₁[k][i][ii]·B₂[jj][ii]`,
filas inferiores cero) y está verificado numéricamente a **4.4·10⁻¹⁶** contra 2.0
para la forma con signo invertido. Es la generalización, a `r ≥ 1`, de la
factorización del peldaño diagonal (Teorema 9): en `r = 0`, `W = Y₁` es una sola
serie y `Θ_w` **es** su MA univariante.

---

## 5. La frontera Λ = 0 y el test de rango

### Teorema 7 (la frontera en Λ = 0)  `[new here]`

**Enunciado.** En `Λ = 0`: (i) `Π = ΛB′ = 0` para todo `B₂`, luego `B₂` no está
identificado; (ii) `Φ(x) = F(x)(1−x)` tiene `M` raíces unitarias, luego `Ȳₜ`
contiene `Wₜ = B′Yₜ`, que es entonces I(1), y el sistema transformado tiene una
raíz AR exactamente en uno; (iii) la verosimilitud exacta del sistema transformado
es, por tanto, **indefinida** en ese punto.

**Prueba.** (i) inmediato. (ii): con `Λ = 0`, (2) da `Φ(x) = F(x)(1−x)`, cuyo
determinante se anula con orden `M` en `x = 1`; `Wₜ` es una combinación lineal de
un vector I(1) sin cointegración impuesta. (iii): la verosimilitud del VARMA
estacionario exige las raíces de `|Φ*(x)|` fuera del círculo. ∎

**Relevancia para el código.** Es la razón de que la semilla de la escalera no
transporte más de un peldaño (`gate_profile_seed` lo documenta): el peldaño `r=0`
vive **en** la frontera del espacio de `r=1`, y `-seedgate` cruza estimando solo
`Λ` y `B₂` (la verosimilitud elige el paso fuera de la frontera). Es también la
razón de que la nula del test secuencial de rango tenga distribución no estándar.

### Teorema 8 (asintótica del test de rango; Yap–Reinsel 1995, Theorem 3)  `[standard, citado]`

**Enunciado.** En un ARMA parcialmente no estacionario, el estadístico de razón de
verosimilitudes para `H₀ : rango(C) = r` contra `H₁ : rango(C) = m` converge, bajo
`H₀`, al funcional mixto-browniano

```
tr{ [∫ B_d dB_d′]′ [∫ B_d B_d′]⁻¹ [∫ B_d dB_d′] }
  + a′[∫ B_d dB_m′ − ½ B_m(1)…] …
```

(el mismo límite que en el caso AR puro), donde `B_d` es un movimiento browniano
estándar de dimensión `d = m − r`. En particular, **la adición de términos MA no
altera la distribución asintótica del estadístico del test de rango**.

**Prueba.** Es el Teorema 3 de Yap y Reinsel (1995), cuyo Lemma 1 muestra que el
score se aproxima por `−(Z₁,ₜ₋₁ ⊗ Θ⁻¹)`, de modo que el MA entra solo a través de
`Θ(1)` — y el funcional límite, por el Lemma 2, queda idéntico al del AR. La prueba
completa está en ese paper (§3–5 y apéndices); aquí se enuncia porque es la base
asintótica de `-lrtest` con `q ≥ 1`.

**Relevancia para el código.** Justifica `-lrtest` con MA **asintóticamente**. El
matiz finito lo aporta la medición del repo (§2.3 de HOMOLOGATION): a `n = 120` el
test asintótico sobre-rechaza (30% contra 5% nominal), por lo que el programa
ofrece el **bootstrap paramétrico** (`-bootstrap N`), que simula bajo `H₀` y usa
los cuantiles empíricos. La nula en frontera (Teorema 7) es la causa estructural de
que la tabla asintótica no baste.

---

## 6. La escalera y el contrato `.pre`

### Teorema 9 (factorización de la verosimilitud exacta diagonal)  `[new here; la base del contrato de entrada]`

**Enunciado.** Si `Fⱼ`, `Θⱼ` (`j = 1,…,max(p,q)`) y `Σ` son todas diagonales, la
verosimilitud exacta del VARMA `M`-variante es igual a la **suma** de las
verosimilitudes exactas univariantes de sus `M` componentes. En aritmética exacta
la igualdad es exacta; en la máquina el residuo es, únicamente, la truncación de
`ξ` (Lema A), que es `≈ 0` con `q = 0` y `~1.4·10⁻⁴` con `q ≥ 1` (xitol = 1e−3).

**Prueba.** En el peldaño diagonal (`r = 0`) la transformación colapsa: `C̄ = I`,
`H̄ = 0`, `Ȳₜ = ∇Yₜ`, `Φ* = F`, `Θ* = Θ`, `Σ* = Σ` (Teorema 1). Con operadores
diagonales, la ecuación `aₜ = wₜ − Σφⱼwₜ₋ⱼ + Σθⱼaₜ₋ⱼ` se desacopla en `M`
ecuaciones univariantes independientes (sin términos cruzados), con innovaciones
independientes (`Σ` diagonal). Tres pasos:

1. *Desacoplamiento.* La densidad conjunta factoriza: `p(w₁,…,wₙ) = Πᵢ pᵢ(wᵢ,₁…wᵢ,ₙ)`.
2. *Corrección de iniciales.* En el Lema A, `det Σ = Πσ²ᵢ` factoriza; la secuencia
   `ξ_r = Σⱼθⱼξ_{r−j}` con `θⱼ` diagonal **permanece diagonal**, de modo que `f₁`
   y `Ω = I + M′H′HM` se descomponen bloque a bloque (ecuación a ecuación).
3. *Conclusión.* `log L = Σᵢ log Lᵢ` exactamente. ∎

*Residuo de máquina.* `cxi` trunca cuando `Σ|ξ_r[i][j]| < xitol`; el sistema
conjunto suma `m²` entradas y cada univariante suma una, luego **no cortan en el
mismo término**. Con `q = 0` no hay recursión `ξ` (`ξ₀ = I` solamente) y la
identidad vale al redondeo (3·10⁻¹¹); con `q ≥ 1` vale a `~1.4·10⁻⁴`. Es
exactamente el §1b de HOMOLOGATION, reproducido por el programa en su
"Entry gate: the factorisation contract" (medido hoy en mink–muskrat, q=1:
`joint − sum = −1.77·10⁻⁵`, VERIFIED).

**Relevancia para el código.** Es la justificación de la escalera y del contrato
`.pre`: la identidad de cruce `Σᵢ logL(serie i) = logL(ajuste DIAGONAL conjunto)`
certifica la factorización, y el certificado de optimalidad
`logL(diagonal) ≥ logL(en los valores traídos)` (con igualdad sii los valores son
óptimas) es lo que distingue un `.pre` (óptimo) de un `.inp` (especificación). El
programa lo imprime en todo ajuste del peldaño diagonal.

### Proposición 1 (la información univariante viaja exactamente un peldaño)  `[medido + estructural]`

**Enunciado.** Para `r ≥ 1` el MA (y el AR) de los `.pre` univariantes **no**
transporta a las coordenadas VEC. Medido: sembrar el bloque univariante completo en
`r = 1` baja el log-verosimilitud de partida 17 unidades.

**Prueba (tres razones estructurales).**

1. *El marginal no es el bloque diagonal.* Marginalizar un VARMA mezcla AR y MA e
   infla los órdenes; el MA univariante de un componente de `Ȳ` no es, ni siquiera
   en orden, el bloque MA del sistema conjunto.
2. *Acoplamiento por `C̄` y `Λ`.* Lo que un univariante de `Ȳ` "ve" es
   `Θ̄ = C̄ΘC̄⁻¹` (Teorema 1), una semejanza no diagonal aunque `Θ` lo sea.
   Sembrar `θ` directamente en `Θ` solo es correcto si `C̄ = I` (`r = 0`).
3. *`Φ̄ₚ = −Fₚ₋₁C̄⁻¹H̄` está determinado.* Imponer las `p` matrices AR de los
   univariantes sobredetermina el sistema.

∎ (Cada punto es un cálculo directo a partir del Teorema 1; el tercero es la
fórmula de `Φ̄ₚ`.)

**Relevancia para el código.** Es el motivo de `-seedybar` (válido en `r = 0`,
advertido por encima) y de que `init_guess` siembre `Θ` en **cero exacto** y use la
regresión condicional para `Λ`, `F` y `Σ`: por encima del peldaño la semilla
correcta es la del acoplamiento, no la del `.pre` univariante.

---

## 6b. La previsión

### Proposición 2 (la previsión puede calcularse sobre `Ȳ` e invertirse)  `[new here]`

**Por qué hace falta enunciarla.** El programa prevé corriendo la recursión
estándar de un VARMA sobre el sistema **transformado** y deshaciendo después la
transformación. Que eso dé la esperanza condicional **del proceso original** no
es automático: exige que la transformación no pierda información. Nada de
`literature/` lo escribe — Ahn y Reinsel (1990), Mauricio (2006) y Yap y Reinsel
(1995) mencionan la previsión como motivación y ninguno da la recursión, y la
referencia a la que los dos últimos remiten para ello (*JTSA* 13, 353–375) no
está en el banco.

**Enunciado.** Bajo (a)–(c), sea `Ȳₜ = (∇Y₂ₜ′, Wₜ′)′` con `Wₜ = Y₁ₜ + B₂′Y₂ₜ`.
Entonces para todo `h ≥ 1`:

1. `σ(Y₁,…,Yₙ) = σ(Y₁, Ȳ₂,…,Ȳₙ)` — las dos filtraciones **coinciden**;
2. `E[Y_{n+h} | Y₁,…,Yₙ]` se obtiene previendo `Ȳ` y aplicando
   `Y₂_{n+h} = Y₂ₙ + Σ_{i≤h} ∇Y₂_{n+i}`, `Y₁_{n+h} = W_{n+h} − B₂′Y₂_{n+h}`;
3. el error de nivel es **afín** en `(A*_{n+1},…,A*_{n+h})` con coeficientes
   `G_m` conocidos, de modo que su covarianza es `Σ_{m<h} G_m Σ* G_m′`.

**Prueba.** (1) `Ȳₜ` es función de `(Y_{t−1}, Yₜ)`, luego `σ(Ȳ) ⊆ σ(Y)`. Al
revés: de `Y₂₁` y `∇Y₂₂,…,∇Y₂ₙ` se recupera `Y₂₂,…,Y₂ₙ` por acumulación, y de
`Wₜ` y `Y₂ₜ` se recupera `Y₁ₜ = Wₜ − B₂′Y₂ₜ`. La correspondencia es biyectiva
dado el ancla `Y₁`, luego las σ-álgebras son la misma y las esperanzas
condicionales respecto de una y otra coinciden.

(2) Por (1) se puede condicionar en `Ȳ`. La esperanza condicional es lineal, y
`Y₂_{n+h}` e `Y₁_{n+h}` son funciones **afines** de `(Ȳ_{n+1},…,Ȳ_{n+h})` con
`Y₂ₙ` conocido y `B₂` fijo, luego la esperanza pasa a través de ellas.

(3) Restando (2) de su valor realizado, el error de nivel es la misma función
afín aplicada a los errores de previsión de `Ȳ`, que por la representación
MA(∞) son `Σ_{t≤h} Ψ_{h−t}A*_{n+t}`. Acumulando el bloque diferenciado aparece
`C_m = Σ_{k≤m}Ψ_k`, y la fila cointegrada resta `B₂′` por la misma acumulación:
es `G_m` de `level_error_map()`. Las `A*` son incorreladas y de covarianza `Σ*`,
luego la covarianza es la suma. ∎

**Lo que la proposición NO cubre, y es donde vive el error real.** `B₂` se
**estima**, no se conoce, de modo que el ancla de (2) y los coeficientes de (3)
son funciones de los datos. Las bandas tratan los parámetros como conocidos, que
es la salvedad habitual y aquí está **medida**: el oráculo de
[HOMOLOGATION.md](HOMOLOGATION.md) §4s da un 2 % de una desviación de previsión
a `n = 20000`, y las bandas dentro del 0.5 %.

**Relevancia para el código.** Es lo que licencia `forecast_vec()`. Y (3) es la
razón de que el mapa de niveles se escriba una sola vez: la media y la varianza
son **la misma función afín**, aplicada al punto una y a los choques la otra.
Calcularlas por caminos distintos es el defecto BUG-10 del programa hermano.

---

## 7. Inferencia: la condición que la sostiene, y la identificación

### Teorema 10 (Phillips 1991a: optimalidad LAMN y su condición)  `[standard, citado]`

**Enunciado.** En el sistema triangular `y₁ₜ = By₂ₜ + u₁ₜ`, `Δy₂ₜ = u₂ₜ`, el MLE
de sistema completo de `B` tiene límite **localmente asintóticamente mixto-normal
(LAMN)** — simétrico, mediana-insesgado, con tests Wald/LR/LM χ² asintóticos —
**siempre que todas las raíces unitarias del sistema hayan sido eliminadas por
especificación y transformación**. Con errores ARMA (Remark (j)), el ML de sistema
completo exige la verosimilitud exacta de un ARMA general estimada conjuntamente
con `B`.

**Prueba.** Es el contenido de Phillips (1991a), Theorem 1 y Remarks (i), (j), (m).
No se reproduce; se enuncia porque la frase condicional — "provided that all unit
roots in the system have been eliminated" — es la que decide cuándo la inferencia
χ² del programa está licenciada.

**Relevancia para el código.** La condición falla exactamente donde caen los
ajustes libres (Θ̂(1) singular, frontera de invertibilidad) y en la nula del test de
rango (Λ = 0). Por eso `-fdhess` cuenta las evaluaciones fuera de la región
admisible y la nota de convergencia distingue "óptimo en frontera" de "óptimo
interior"; en la frontera los errores estándar reportados son los BFGS, con las
reservas del §5 de INFERENCE.md.

### Teorema 11 (Johansen 1991: límite mixto-normal de β̂)  `[standard, citado]`

**Enunciado.** Bajo `H₃ : β = Hφ, α = Aψ`, el límite de `T(β̂ − β)` es mixto-normal
(Johansen 1991, Theorem 5.1; Ahn–Reinsel 1990, Theorem 4 para el caso AR),
independiente del movimiento browniano que mueve las tendencias comunes. La
consecuencia operativa es que Wald/LR sobre `β` (aquí `B₂`) son χ² asintóticos, y
que `β̂` es superconsistente (su desviación típica cae más rápido que `T^{−1/2}`).

**Prueba.** Es el Teorema 5.1 de Johansen (1991) (la factorización del límite en el
mixto-normal `(β⊥′Γβ⊥…, ∫G₂G₂′)` y la independencia de `V_α` y `G₂`). No se
reproduce; se cita como el fundamento de los errores estándar de `B₂` que el
programa reporta.

**Relevancia para el código.** INFERENCE.md lo verificó en simulación: Wald y LR
convergen entre sí (diferencia 0.0001 en n=200), y la desviación típica de `B̂₂` cae
un factor 27 al multiplicar `n` por 16 (≈ `T^{−1.2}`). **Todo esto está probado
sobre 𝒞**; en un maximizador de `𝒫 \ 𝒞` (Teorema 4) ninguno de los límites está
licenciado, que es la lectura que cierra el diagnóstico.

### Identificación (coprimidad izquierda; Hannan 1969, 1975; Yap–Reinsel 1995)  `[standard, citado]`

**Enunciado.** El VARMA en niveles `Φ(L)Yₜ = Θ(L)Aₜ` está identificado si `Φ(L)` y
`Θ(L)` son **coprimos por la izquierda** (no comparten factor matricial común con
determinante no constante) y se cumplen las condiciones de Hannan/Dunsmuir–Hannan.
Yap–Reinsel (1995) lo asume explícitamente: *"it is assumed that Φ(L) and Θ(L) are
left coprime … conditions necessary for the identification of parameters …
(Dunsmuir and Hannan 1976; Hannan 1975)."*

**Prueba.** Es el resultado clásico de identificación de VARMA; se enuncia, no se
reproduce.

**Relevancia para el código.** Es lo que Mauricio (2006) *"asume"* ("conditions …
such as those considered by Yap and Reinsel (1995) are also assumed") sin traducir
al espacio `(Λ, B₂, Fᵢ, Θⱼ, Σ)` que se optimiza. Dos huecos quedan, y son los que
el resto de este documento cierra:

1. La coprimidad identifica el VARMA **de niveles**, pero no garantiza que el
   modelo **ajustado** sea I(1) de rango `r`: eso es la condición de rango del
   Teorema 3, que la literatura no escribe en los parámetros.
2. La condición de Granger con MA (`rango(Λ⊥′Θ(1)) = s`) es *más fuerte* que la
   coprimidad: un punto puede ser un VARMA perfectamente identificado y, a la vez,
   inadmisible como modelo de rango `r` (det Θ(1) = 0 en dirección Λ⊥).

---

## 8. Lectura conjunta: qué prueba cada teorema y para qué sirve en el código

| Teorema | Qué prueba | Dónde vive en el código |
|---|---|---|
| T1 | la transformación es una biyección (VEC ⟺ VARMA estacionario) | `vec_shootx` (verificado 4·10⁻¹⁵) |
| C1.1 | el MA patológico se ve en ambas coordenadas | lectura de raíces MA en el `.out` |
| T2 | largo plazo = `CΘ(1)` | base de `granger_smin` |
| T3 | condición de rango `rango(Λ⊥′Θ(1))=s` | `granger_smin`, `-rankadm` |
| C3.2 | cuándo el MA unitario es fatal (dirección `Λ⊥`) | diagnóstico de la dirección nula |
| T2′ | `elf()` es aplicable al sistema transformado | motor AS 311 |
| Lema A | forma de la verosimilitud exacta + truncación `ξ` + puerta | `elfvarma.c`, `chekma` |
| T4, T5 | por qué el optimizador cae en `det Θ(1)=0` | explica el 0.99995* y el termcode 3 |
| T6, 6b | la estructura del MA (herencia) | `-mawarma`, `-marow`, `-matri`, `-warma` |
| C6.2 | el cero que importa es `T₂₂` | §4j del registro |
| **C6.3** | **en la clase de T6, la puerta del motor ES la condición de admisibilidad** | `-mawarma`, `-marow`: hace `𝒞` el espacio entero |
| T7 | frontera en Λ=0 (B₂ no identificado) | `-seedgate`, nula del test de rango |
| T8 | asintótica del test de rango (MA no cambia el límite) | `-lrtest` + `-bootstrap` |
| T9 | factorización del peldaño diagonal | contrato `.pre`, identidad de cruce |
| P1 | la información univariante viaja un peldaño | `-seed`/`-seedybar`, `init_guess` |
| **P2** | **la previsión puede calcularse sobre `Ȳ` e invertirse** | `forecast_vec`, `level_error_map` |
| T10, T11 | inferencia χ²/mixto-normal, condicionada a 𝒞 | errores estándar, `-fdhess` |
| Ident. | identificación del VARMA (coprimidad) | el hueco que T3 cierra |

La línea que une todos los resultados es el **Teorema 3**: la condición
`rango(Λ⊥′Θ(1)) = s` es la única que la literatura no escribe en los parámetros, y
es exactamente la que distingue el modo admisible del modo de frontera que los
ajustes libres visitan.

Y la línea que la **cierra** es el Corolario 6.3. Los Teoremas 4 y 5 dicen que en
la clase libre esa condición es alcanzable, premiada e invisible; C6.3 dice que en
la clase del Teorema 6 es automática y que el chequeo que el motor ya hace la
impone. El problema no se diagnostica: se elige una parametrización en la que no
existe.
