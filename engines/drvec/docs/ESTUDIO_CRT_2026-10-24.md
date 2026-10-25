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
* **150 réplicas con T = 50 y 100 con T = 100 (300 en la celda a), en lugar
  de 10 000**: error estándar de unos 1.8, 2.2 y 1.3 puntos en torno al 5 %, y
  3.3, 4 y 2.3 en torno al 20 %; suficiente para distinguir uno de otro, no para afinar décimas. Una
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

Tres celdas, δ = 0, γ = 0.8, B = 99, con `bin/drvec` de esta rama (la
comprobación I(1, r) incluida). Porcentaje de rechazos al 5 % ± error estándar;
entre paréntesis, la cifra de CRT (§5). Cada réplica es reproducible: su
semilla es `100000·seed + rep` (seed = 1), y `tools/mc_crt.py run --panel a
--delta 0 --T 50 --reps 150 --B 99` rehace la celda en un CSV, que
`tools/mc_crt.py summary` resume como abajo.

**Tamaño** (H₀ = rango verdadero):

| celda | réplicas | asintótico | bootstrap |
|---|---|---|---|
| a, T = 50, r = 1 | 150 | 11.3 ± 2.6 (45.5) | **4.0 ± 1.6** (5.2) |
| a, T = 100, r = 1 | 300 | 2.7 ± 0.9 (22.6) | **6.3 ± 1.4** (5.4) |
| b, T = 100, r = 0 | 100 | 9.0 ± 2.9 (44.3) | **7.0 ± 2.6** (6.6) |

**Potencia** en el panel a (H₀: r = 0, falsa): asintótico 88.7 y 99.7, bootstrap
68.7 y 99.7, a T = 50 y 100.

**Procedimiento secuencial** (porcentaje de r̂ = 0 / 1 / 2 / 3):

| celda | asintótico | bootstrap | bootstrap CRT |
|---|---|---|---|
| a, T = 50 | 11.3 / **78.0** / 9.3 / 1.3 | 31.3 / **65.3** / 3.3 / 0 | 44.1 / **50.6** / 4.4 / 0.9 |
| a, T = 100 | 0.3 / **97.0** / 2.7 / 0 | 0.3 / **93.3** / 6.0 / 0.3 | 0.6 / **94.0** / 4.7 / 0.7 |
| b, T = 100 | **91.0** / 9.0 / 0 / 0 | **93.0** / 7.0 / 0 / 0 | — |

**La comprobación I(1, r)** no falló en ninguna de las 1647 comparaciones, como
anticipaba §3 para q = 0.

### Lectura

1. **El bootstrap tiene el tamaño de CRT** en las tres celdas: 4.0 frente a
   5.2, 6.3 frente a 5.4 y 7.0 frente a 6.6, dentro del error. En a, T = 100
   las 100 primeras réplicas daban 9.0, con un indicio de sesgo: el crítico
   bootstrap (mediana 19.5) quedaba por debajo del percentil 95 del LR en el
   Monte Carlo (21.9). La ampliación a 300 lo descarta: las 200 réplicas nuevas
   dan 5.0 % por sí solas, el percentil 95 del LR baja a 20.5 con el crítico
   igual (19.5), y 19 o más rechazos de 300 tienen probabilidad 0.18 si el
   tamaño es del 5 %. Queda un exceso de uno o dos puntos, del orden del que
   CRT documentan en sus propias celdas, que no se distingue del ruido con este
   número de réplicas. Los p-valores se reparten bien (proporción con p ≤ 0.05
   / 0.10 / 0.20 / 0.50: 0.063 / 0.12 / 0.21 / 0.48). A T = 50 el crítico y el
   percentil coinciden (25.2 frente a 24.5).
2. **El asintótico de drvec se desvía mucho menos que el de CRT**: 11 frente a
   46 a T = 50, 3 frente a 23 a T = 100, y 9 frente a 44 en el panel b. Las
   dos diferencias declaradas en §4 apuntan en esa dirección: el λ-max compara
   r con r + 1 y no con p, y la verosimilitud exacta da LR menores que la
   condicional frente a las mismas tablas (BUG-26: infradimensionado a T
   grande). En consecuencia, en este diseño el bootstrap de drvec **no corrige
   un sesgo grande, porque no lo hay**; lo que hace es llevar el 11 % de T = 50
   al nominal. Esto no se generaliza a modelos con MA: es lo que §3 dejó fuera
   del diseño.
3. **La selección del rango funciona**, y a T = 50 mejor que la de CRT (65 %
   de aciertos frente a 51 %), porque deja r̂ = 0 menos veces: el λ-max tiene
   más potencia contra r = 0 que la traza en este diseño. El precio es que el
   bootstrap, que sube el crítico a T = 50 (35.5 frente a 28.1 en r = 0), cede
   potencia frente al asintótico (68.7 frente a 88.7), y el secuencial
   asintótico acierta más (78 %), con un tamaño del 11 % en r = 1. Es el
   intercambio habitual, no un defecto.
4. **La afirmación «seis veces / cuatro veces»** de HOMOLOGATION §2.3, que el
   `.out` citaba, no describe el programa actual. El texto del `.out` se ha
   cambiado por las cifras de esta tabla (`3110fa1`).

### Lo que acompaña a las cifras

* **Tres réplicas de 550 sin la comparación r = 2** (a, T = 100: réplicas 50 y
  55; b: réplica 26). El ajuste de rango 3 no encontraba ningún arranque
  admisible: con un rango verdadero de 1 o 0, las semillas dan a las relaciones
  sobrantes un ajuste del signo equivocado, y `make_admissible` sólo sabía
  escalar Λ. Era un defecto, **BUG-64**, corregido en esta rama; las cifras de
  esta sección son del binario anterior a la corrección, que no cambia nada en
  r = 0 ni en r = 1, donde se leen el tamaño y la potencia. Con la corrección,
  las réplicas 50 y 55 dan LR(2→3) = 2.66 y −0.40 (este último dentro de la
  tolerancia de un ajuste de rango 2 que paró por criterio 3).
* **El aviso `[check: rank k stopped by criterion 3]`** (BUG-25: el ajuste paró
  por paso y no por gradiente, a menudo un óptimo verdadero sobre una
  superficie plana, CONVERGENCE.md) aparece en 102 de las 150 comparaciones
  r = 1 de T = 50, 245 de 300 a T = 100 y 3 de 100 en el panel b. Separando
  por él, el tamaño del bootstrap no cambia (T = 50: 3.9 % con aviso, 4.2 %
  sin él; T = 100: 6.9 % con aviso, 3.6 % en las 55 réplicas sin él, pocas
  para leer la diferencia).
* **Coste**: mediana de 490 s por réplica en el panel a y 765 s en el b, con
  cuatro procesos y la máquina a carga 5–7.

## 7. Qué queda abierto

1. **El 9 % de a, T = 100**: *resuelto*. Con 300 réplicas el tamaño es
   6.3 ± 1.4 %, compatible con el 5 % y con el 5.4 % de CRT (§6, Lectura 1).
   Si un diseño posterior mostrara un exceso persistente, lo primero que se
   mira es arrancar desde los valores observados en lugar de desde la media
   con burn-in, la diferencia de §2 que más directamente toca la persistencia
   de la muestra simulada.
2. **El caso con MA.** Este diseño tiene q = 0, donde la comprobación I(1, r)
   se cumple por construcción. El riesgo de §3, una raíz MA junto al círculo
   unidad que casi quita una tendencia cuando H₀ está por debajo del rango
   verdadero, sólo se mide con un DGP con q ≥ 1. Es el diseño siguiente, y
   la pregunta es la potencia en r < r₀.
3. **La traza.** drvec da λ-max. Para comparar la potencia con CRT directamente
   haría falta la traza, que con la verosimilitud exacta son p − r ajustes más
   por comparación. No es necesario para el tamaño.
4. **El rango 3 sin arranque** de las tres réplicas de §6: *resuelto*. El
   arranque en frío sí se intentaba (es la fila `seed` de la tabla de
   búsqueda, que no lista `cold` aparte cuando coincide con la semilla); el
   defecto estaba en `make_admissible`, que no podía arreglar un Λ del signo
   equivocado. Registrado y corregido como BUG-64, con su repro.
5. **El porte.** drvec-python transcribe el texto del bloque del bootstrap
   (`report.py` y cuatro oráculos) y tiene que reproducir el camino de fallo de
   `i1r_check`. Va en un PR aparte, al re-vendorizar `csrc/` desde esta rama.

## Referencias

Swensen (2006); Cavaliere, Rahbek y Taylor (2012); Cavaliere, Nielsen y
Rahbek (2015); Yap y Reinsel (1995); Plosser y Schwert (1977). Las entradas completas están en
[REFERENCES.md](REFERENCES.md).
