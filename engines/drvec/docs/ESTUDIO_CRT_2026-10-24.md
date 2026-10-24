# El bootstrap del rango frente a Cavaliere, Rahbek y Taylor (2012)

*Estudio del 2026-10-24. Qué respalda la teoría del bootstrap del rango que
implementa `-bootstrap`, en qué se aparta drvec de ella, y cuánto mide su tamaño
con el diseño de Monte Carlo del artículo. La procedencia está resumida en
[INFERENCE.md](INFERENCE.md) §7; aquí va la investigación completa.*

---

## 1. La pregunta

`-bootstrap` se construyó con la receta del BVECM (§6.3–6.5): simular bajo H₀
con los parámetros estimados al rango r, reestimar los dos rangos y leer los
percentiles. El BVECM no prueba nada sobre esa distribución: sus teoremas son de
representación (la equivalencia WARMA–VEC) y su afirmación asintótica se apoya
en Yap y Reinsel (1995, Teorema 3), que es una traza sobre la verosimilitud
condicional (BUG-27). La única medida del tamaño,
[HOMOLOGATION.md](HOMOLOGATION.md) §2.3 (2026-08-19), daba un sobre-rechazo del
20 % con el bootstrap frente al 5 % nominal, y es anterior a BUG-24, 26, 27 y 50,
que cambiaron la tabla, el estadístico y los reajustes.

Las preguntas son tres:

1. ¿El esquema de drvec es el que la literatura valida?
2. ¿Comprueba lo que esa literatura comprueba antes de simular?
3. ¿Qué tamaño tiene hoy, en el diseño donde la literatura publica el suyo?

## 2. La literatura

| | esquema | resultado |
|---|---|---|
| **Swensen (2006)** | α, β al rango r; Γᵢ y residuos del ajuste **sin restringir** | primera teoría: bootstrap consistente y procedimiento secuencial que no elige un rango menor que el verdadero |
| **Cavaliere, Rahbek y Taylor (2012)** (CRT) | **todos** los parámetros bajo H(r) | las estimaciones convergen a valores pseudo-verdaderos que cumplen las condiciones I(1, r) aunque r < r₀: la muestra bootstrap es I(1) con rango r sin condiciones auxiliares. El esquema de Swensen puede dar muestras explosivas o I(2) cuando r < r₀, también asintóticamente |
| **Cavaliere, Nielsen y Rahbek (2015)** | bajo H₀: β = τ, todo restringido | el χ² de la LR sobre β tiene mal tamaño en muestras finitas; el bootstrap restringido es válido y consistente |

El esquema de drvec es el de CRT: `bootstrap_rank` recupera el modelo entero al
rango r (Λ, F, Θ, B₂, Σ) y simula de él. La lectura secuencial de sima, el primer
rango no rechazado, es su Algoritmo 2.

Dónde se aparta drvec de CRT:

| | CRT (2012) | drvec |
|---|---|---|
| modelo | VAR, errores i.i.d. | VARMA en `Ȳ`; Θ forma parte de H₀ |
| verosimilitud | gaussiana, regresión de rango reducido | exacta, incondicional |
| estadístico | traza, H(r) frente a H(p) | λ-max, r frente a r+1 |
| perturbaciones | remuestreo i.i.d. de los residuos restringidos, recentrados | gaussianas con Σ̂ del ajuste al rango r |
| valores iniciales | los observados | `Ȳ` desde su media con un burn-in de 50 + 10p; `Y₂` acumulada desde su primer nivel observado |
| comprobación I(1, r) | explícita, paso (ii) | §3 |
| réplicas descartadas | las que fallan la comprobación de raíces | las que no convergen (`fitfail`) |

## 3. La comprobación I(1, r)

CRT (Algoritmo 1, paso ii) comprueban antes de simular que el VAR en niveles
tiene p − r raíces en 1 y las demás fuera del círculo. En drvec la muestra se
simula en `Ȳ = [∇Y₂ ; W]` y se integra (`simulate_h0`), así que la misma
condición son dos sobre el VARMA de `Ȳ`, `Φ(B)Ȳ = Θ(B)a`:

1. **`Ȳ` estacionario**: todas las raíces de `|Φ(z)|` fuera del círculo. Entonces
   `W` es estacionario y las r relaciones se cumplen.
2. **`Y₂` lleva s = M − r tendencias**: la covarianza de largo plazo de `∇Y₂`,
   las s primeras filas de `K = Φ(1)⁻¹Θ(1)` por Σ por su traspuesta, es no
   singular. Es la condición (d) de
   [ESTUDIO_MAURICIO_2026-09-23.md](ESTUDIO_MAURICIO_2026-09-23.md),
   `rango(Λ⊥′Θ(1)) = s`, leída donde se hace la muestra.

**Implementado** en `i1r_check` (`src/drvec.c`), llamado por `bootstrap_rank`
antes de la primera réplica. La medida de 2 es invariante a la escala: el menor
valor singular de `D^{-1/2} K_s L`, con L el factor de Cholesky de Σ y D las
varianzas de largo plazo; es la raíz del menor autovalor de la matriz de
correlación de largo plazo de `∇Y₂`, 1 para tendencias incorreladas y 0 para una
tendencia perdida. Si 1 falla o la medida baja de `GRANGER_ZERO` (1e-6, el
umbral que ya usa la condición de rango), la comparación no da valores críticos y
el `.out` dice por qué:

```
  0       3    30.4682   the model under H0 is not I(1, 0): not reported
             smallest AR root of Ybar ... (> 1 is stationary); nabla Y2's long-run
             rank measure ... (0 = a trend lost; Cavaliere, Rahbek and Taylor 2012)
```

Cuando la comprobación se cumple, la salida no cambia: los 36 goldens salen
idénticos byte a byte.

**Lo que se encontró al implementarla.** En esta parametrización el paso (ii) de
CRT está garantizado casi por construcción:

* la verosimilitud exacta sólo existe con `Ȳ` estacionario, así que un ajuste
  que convergió cumple 1;
* con q = 0, `Θ(1) = I` y `Φ(1)` es invertible, así que 2 se sigue de 1.

Es decir: **con q = 0 la columna RC de CRT (el porcentaje de muestras
explosivas) es cero por construcción en drvec**, mientras que el esquema de
Swensen llega al 91.6 % con δ = 0.3 y T = 200 (CRT, tabla II). La comprobación
queda como guarda exacta; en los datos no salta.

**Pero con MA aparece algo que una comprobación binaria no ve.** Medido con una
compilación de prueba que imprime la medida en cada comparación, `-lrtest
-bootstrap` con q = 1 y caso 2:

| datos (rango verdadero) | H₀ | medida de 2 | raíces MA de Θ̂ |
|---|---|---|---|
| `synthetic/rank0` (r₀ = 0) | r = 0 | 0.77 | — |
| `synthetic/rank0` | r = 1 | 0.90 | — |
| `synthetic/rank2` (r₀ = 2) | r = 0 | **0.19** | 0.99999 ± 0.011i, sobre la puerta (0.99995*) |
| `synthetic/rank2` | r = 1 | **0.16** | |

Con H₀ por debajo del rango verdadero, el ajuste absorbe las direcciones
estacionarias que sobran con raíces MA junto al círculo unidad (la
sobrediferenciación de Plosser y Schwert), y los valores singulares de Θ̂(1)
bajan a 0.025 y 0.005. La muestra que se simula de ese modelo lleva *casi* menos
tendencias de las que dice H₀. Es el problema que CRT resuelven en el VAR,
reaparecido por el MA: su Lema 1 (los valores pseudo-verdaderos cumplen I(1, r))
está probado para el VAR y no cubre este caso. No se pone umbral, porque la
medida también baja con tendencias muy correlacionadas, que es I(1, r) legítimo;
lo que hay que medir es su efecto en la potencia (§6).

## 4. El diseño de Monte Carlo

El de CRT, sección 4, ecuación (8):

```
ΔX_t = a b′ X_{t−1} + Γ ΔX_{t−1} + ε_t,   ε_t ~ i.i.d. N(0, I₄),   X₀ = ΔX₀ = 0,
b = (1, 0, 0, 0)′,  a = (−0.4, 0, 0, 0)′,
Γ = [[γ, δ, 0, 0], [δ, γ, 0, 0], [0, 0, γ, 0], [0, 0, 0, γ]],  γ = 0.8.
```

Panel (a): r₀ = 1, δ = 0 (δ sólo afecta al esquema de Swensen; el de CRT no
depende de él). Panel (b): a = 0, Γ = γI₄, r₀ = 0. Constante restringida, que es
el caso 2 de drvec (`E[W]` libre, sin deriva en `∇Y₂`).

drvec: `drvec <inp> 2 0 0 -case 2 -lrtest -bootstrap 99`, con las columnas en el
orden (X₂, X₃, X₄, X₁) para que la normalización `B = [I_r ; B₂]` caiga sobre X₁.
El generador y la lectura del `.out` están en `tools/mc_crt.py`; una fila del CSV
por (réplica, comparación).

Diferencias con el diseño de CRT, todas declaradas:

* **λ-max (r frente a r+1) en lugar de traza (r frente a 4).** Bajo r₀ = 1 las
  dos son contrastes de la misma nula en r = 1, y los tamaños son comparables; la
  potencia en r = 0 no lo es del todo.
* **B = 99 en lugar de 399**, por coste: drvec hace los bootstraps de las tres
  comparaciones, 2·B ajustes exactos cada una. Con B = 99 el contraste al 5 % es
  exacto ((B+1)·0.05 entero), con algo menos de potencia.
* **150 réplicas con T = 50 y 100 con T = 100, en lugar de 10 000**: error
  estándar de unos 1.8 y 2.2 puntos en torno al 5 %, y 3.3 y 4 en torno al
  20 %; suficiente para distinguir uno de otro, no para afinar décimas. Una
  réplica cuesta unos 500 s con T = 50 (los cuatro ajustes del `-lrtest`, con su
  búsqueda de arranques, y 3 × 2 × 99 reajustes), con dos procesos a la vez.
* La muestra es X₀ = 0 y X₁ … X_T (T + 1 filas en niveles, T diferencias); CRT
  condicionan en los dos valores iniciales.

## 5. Las cifras de referencia (CRT, tablas I y II)

Tamaño (porcentaje de rechazos al 5 %), δ = 0:

| | T = 50 | T = 100 | T = 200 |
|---|---|---|---|
| asintótico, r = 1 (panel a) | 45.5 | 22.6 | 13.3 |
| bootstrap CRT, r = 1 (panel a) | 5.2 | 5.4 | 5.0 |
| asintótico, r = 0, γ = 0.8 (panel b) | 78.8 | 44.3 | 21.6 |
| bootstrap CRT, r = 0, γ = 0.8 (panel b) | 8.1 | 6.6 | 5.3 |

Procedimiento secuencial, bootstrap CRT, δ = 0 (porcentaje de r̂ = 0 / 1 / 2 / 3–4):
T = 50: 44.1 / 50.6 / 4.4 / 0.9; T = 100: 0.6 / 94.0 / 4.7 / 0.7;
T = 200: 0.0 / 95.0 / 4.3 / 0.7.

## 6. Resultados

*En curso.*

## 7. Qué queda abierto

*Se completa con los resultados.*

## Referencias

Swensen (2006); Cavaliere, Rahbek y Taylor (2012); Cavaliere, Nielsen y
Rahbek (2015); Yap y Reinsel (1995); Plosser y Schwert (1977). Las entradas completas están en
[REFERENCES.md](REFERENCES.md).
