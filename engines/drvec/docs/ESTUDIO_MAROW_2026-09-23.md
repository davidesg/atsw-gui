# De dónde sale `-marow`: el artículo BVECM y el código legado, estudiados antes de decidir

*Estudio pedido antes de decidir la clase MA por defecto. Fuentes: el artículo
«Cointegration Analysis with Gradual Adjustment Dynamics…» (BVECM, versión del
2 de mayo de 2026, con su `.tex`, las demostraciones `proofs.tex`, el
`bvecm_example_addon` del 13 de mayo, la presentación y las notas `CI11_notes`,
en `Article_Multivariate Convergence/cointegration_convergence/Legacy/`), y el
programa legado `~/Dropbox/SRC/drv_project`. Los dos estudios de detalle, con las
citas por línea, las pruebas y los scripts, están en
[estudio_bvecm_marow_2026-09-23/](estudio_bvecm_marow_2026-09-23/) (`A/` el
artículo, `B/` el código). Viene después de
[ESTUDIO_MAURICIO_2026-09-23.md](ESTUDIO_MAURICIO_2026-09-23.md), que encontró
falso el Corolario 6.3 en que se apoyaba el defecto (BUG-48).*

---

## 1. En una frase

**`-marow` no es el modelo del artículo ni el del código legado.** El artículo
usa una clase restringida sólo como marco de las demostraciones del apéndice
(`-warma`, no `-marow`), y **lo que estima**, en el texto y en el programa que lo
acompaña, lleva la media móvil justo en el bloque que `-marow` anula.

---

## 2. Qué dice el artículo

«WARMA» designa tres cosas distintas en el artículo:

1. **Texto principal, ec. (2.1)–(2.8)** (`BVECM_models.tex:119-153`, copiado de
   `OnCointegration.tex`): **no es consistente**. (2.3) hace `Δz₁` idénticamente
   nula, (2.4) contradice a (2.2), `γ` tiene las dimensiones equivocadas, y su
   `Θ = [[0, 0], [0, γ]]` pone la MA en el bloque `Δz₂`.
2. **Apéndice, Definición 3 + Corolario 2** (`proofs`, `:52-72`, `:325-336`): es
   la **única** fuente de las filas nulas. `Δz₂` depende sólo de `w` retardado,
   con ruido blanco: sin retardos de `Δz₂` y sin MA ahí; la MA sólo en `w`. Es un
   **supuesto** del apéndice, sin motivación económica («basado en Phillips» no lo
   impone: en Phillips (1991) `u` es un proceso estacionario general). En
   coordenadas VEC es la clase de `-warma` (`Γᵢ = NᵢB′`, `Θⱼ = [[Tⱼ, TⱼB₂′], [0, 0]]`).
3. **Lo que se estima**: el propio artículo dice tres veces que **no es** la clase
   del apéndice — `proofs :74-78`, nota al pie de `:226`, y `:266`: *«While the
   formal derivations in the Appendix start from a restricted WARMA specification
   (Definition 3), the estimation procedure and the above example adopt a full VAR
   model for the stationary vector (Δz₂′, w′)′»* —, y la presentación lo repite
   («full VAR … not restricted triangular», `:209-210`) y lo deja como **cuestión
   abierta** (`:493-497`). Las notas de Treadway (`CI11_notes`, TECF (7.1)–(7.3))
   usan `Φ″` y `Θ″` completos: el modelo libre de Mauricio.

**Las demostraciones del apéndice:** el Teorema 1 en sentido directo (con la
corrección `−I_r`) y el Corolario 2 en sentido directo son correctos (simulación
exacta, error ≤ 7e-15). El **recíproco del Teorema 1** es falso (contraejemplo
VEC(2) con rango Γ₁ = 2); el **paso 6** de su prueba sólo vale con `p = 1` y
`k = 0`; el **recíproco del Corolario 2** («todo VEC con MA admite forma WARMA»)
es falso — un VEC(1) con MA en la fila de `Y₂` tiene retardos de `Δz₂` con
t de −216 a −3.3, donde la Definición 3 exige cero. La afirmación de `:266` de que
«las fórmulas del apéndice valen para el VAR completo» es falsa para `Γ`.

**No hay en el artículo ningún argumento para usar una clase restringida como
defecto para datos genéricos.** Su método de construcción del modelo (`:155`) es
empírico: empezar con `q = 0`, ajustar AR por pasos, y añadir MA sólo cuando se
ve.

**El trabajo empírico** (tablas del trigo, el add-on) no es internamente
coherente: el texto de `:561` y `:607` corresponde a una ejecución antigua (1848),
no a las tablas actuales (Londres «débilmente exógeno» con p = 0.025 y 0.035); el
retardo medio `1/(1−δ)` es un año más largo que la definición del propio artículo
`δ/(1−δ)`; el add-on toma la «raíz AR» (0.651) como `φ₁₁` y obtiene `α̂₁ = −0.305`
donde la Tabla 2 da −0.457, y arrastra otros cuatro errores. Detalle en `A/`.

---

## 3. Qué estima el código legado (`drv_project`)

Verificado leyendo el código y reproduciendo sus salidas (el binario publicado y
una recompilación limpia reproducen exactamente las cuatro logL de `gui/*.out`:
AL −1488.4742, PL −1459.6081, SL −1480.6918, VL −1536.2347):

- un VARMA(p, 1) sobre `Ȳ_leg = (w, ∇L)`, con `w = A − βL − ν(B)X`;
- **AR completo**: los cuatro `φᵢⱼ(k)` libres en cada retardo (`main.c` 1667–1673);
- **MA: un solo coeficiente, `θ₂₂(1)`, el de la ecuación de `∇L`** — el bloque de
  tendencias comunes, `∇Y₂` —, con los otros tres a cero (1676–1681), por
  construcción (`global_q = 1` fijo, nunca leído de la línea de órdenes);
- media sólo en `w` (el caso 2 de `drvec`); `β` entra sólo por los datos.

En coordenadas VEC esa MA es `Θ₁ = [[0, −B₂′θ], [0, θ]]`: **la fila de `Y₂` es la
única no nula, el complemento exacto de `-marow`**. Ninguna clase de `drvec` es
ésa; está dentro de `-matri`. La inversión a la forma BEC (`analisis_BEC`) es
correcta en lo esencial (el VAR en niveles coincide a 1e-16); imprime `α` por
retardo en vez de `Σα(k)`, su test de exogeneidad débil usa p grados de libertad
en vez de 1, y la forma estructural (opción `struct`) factoriza la covarianza
equivocada.

**Correspondencia de clases** (comprobada: en `p = 1` los 24 ajustes coinciden en
logL a menos de 1e-6): todas las clases MA de `drvec` son patrones de ceros sobre
`Θ* = C̄ΘC̄⁻¹ = [[a, b], [c, d]]` (a = `∇Y₂←∇Y₂`, b = `∇Y₂←W`, c = `W←∇Y₂`,
d = `W←W`): `-marow` anula a y b; `-mawarma`/`-warma` dejan sólo d; `-matri` anula
b; el legado deja sólo **a**.

---

## 4. La evidencia empírica sobre ese coeficiente

Sobre los pares de trigo (364 ajustes: el mejor de 7 arranques del legado más el
defecto y `-seedjoh` de `drvec`):

- **Siempre que `∇Y₂` tiene un retardo AR propio**, el coeficiente `a` — el que
  `-marow` anula — es grande y significativo: a = 0.67–1.00, |t| de 5 a 20 donde es
  interior; `-matri` gana a `-marow` con LR ≥ 5.96 (1 g.l.) en **37 de 40**
  configuraciones, y en las tres configuraciones publicadas del legado con
  `p = 2` el LR es 16.6–20.7. `b` rara vez hace falta (free frente a tri, LR < 3.84
  en 34/40).
- **Con el `p = 1` de `drvec`**, `∇Y₂` no tiene retardo propio y `|a| ≤ 0.22`.
- **Salvedad**: en las muestras cortas (90–113 obs.) `a` se va a 1.000 en muchos
  ajustes, y ahí la referencia χ² no vale; falta un bootstrap.
- **Por qué**: las autocorrelaciones de `∇Londres` son −0.03, −0.23, −0.19, −0.08:
  la serie está casi sobrediferenciada, y eso es una MA en el bloque de tendencias.

Comprobado aparte con el binario de `drvec` (un arranque en frío por clase,
`2 1 1 -case 2`): en VILL y SLL `-matri` supera al defecto en 11.8 y 12.9 de logL;
en AL la ganancia es 0.17; en PLL `-matri` queda **por debajo** de `-marow`, y
`-mafree` por debajo de `-matri`, pese a estar anidadas — óptimos locales de un
único arranque (BUG-25). Es la otra cara de la misma evidencia: en series cortas
el modelo libre no se estima bien desde un solo arranque.

Y el propio registro de `drvec` ya lo había medido (`HOMOLOGATION.md` §4s/§4t): con
un proceso que tiene esa MA, imponer `-marow` cuesta un 10 % de desviación típica
de previsión; y en el banco real, con `q = 1`, **la clase libre prevé mejor que el
defecto en 8 de 9 casos** — «on this bank the data want that entry». El registro
lo dejó como «tensión, enunciada y no resuelta».

---

## 5. Para la decisión — hechos y opciones

**Hechos:**

1. El argumento teórico del defecto (Corolario 6.3) es falso; el que se atribuía
   al artículo (BVECM, Teorema 6b = su Corolario 2) apoya, si acaso, `-warma` /
   `-mawarma`, no `-marow`, y el artículo mismo no estima esa clase.
2. El único argumento que queda es **empírico y de estimabilidad**: §4q (la `Θ`
   libre no se recupera con n = 120–250 en este régimen) y §4r (la clase
   triangular recupera θ = 0.9).
3. **En contra, también empírico y del propio registro**: los datos de la suite
   quieren precisamente el coeficiente que `-marow` anula (37/40 por LR, 8/9 en
   previsión, y el programa legado lo estimaba).
4. La teoría del test de rango citada (Yap–Reinsel) supone `Θ` sin restringir;
   `-marow` hace además el `-lrtest` no anidado (BUG-27).

**Opciones** (no son excluyentes con arreglar la búsqueda, BUG-25):

| | a favor | en contra |
|---|---|---|
| **`-mafree`** (el modelo de Mauricio) | fiel al artículo de referencia; anida todas las demás clases (cada una se vuelve un contraste); es el supuesto del test de rango | mal identificada en muestras cortas; ajustes en la frontera de invertibilidad |
| **`-matri`** (libera `a`, anula `b`) | recoge el coeficiente que los datos piden (37/40) y contiene el modelo del legado; un parámetro más que `-marow` en M = 2 | restricción propia de `drvec`, sin fuente; `a` se va a 1 en muestras cortas |
| **`-marow`** (lo de hoy) | parsimonia; elimina la ruta `T₂₂ → 1` | sin fuente teórica; los datos de la suite la rechazan; hace el `-lrtest` no anidado |
| **sin defecto con `q ≥ 1`** | obliga a elegir la clase, como el método del artículo (`:155`) | cambia la interfaz |

Con cualquiera de ellas: dejar de citar el BVECM como fuente de `-marow`,
corregir el comentario de `drvec.c:333` (el legado coincide con `-warma` sólo en
cómo entra `B₂`; sus patrones AR y MA son los contrarios) y
`ESTUDIO_BVECM_vs_DRVEC.md` §5 (dice bien que la MA del legado es sólo `θ₂₂`, pero
no que es la ecuación de `∇Y₂`), y construir el contraste bootstrap de una clase
frente a otra, que es lo que falta para que la elección sea una medida y no un
supuesto.
