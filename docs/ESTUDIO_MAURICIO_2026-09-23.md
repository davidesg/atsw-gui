# Mauricio (2006) frente a `drvec`: ¿representa el código el modelo del artículo?

*Estudio a fondo, demostraciones incluidas, hecho antes del porte a Python para no
depurar algo que no es lo que el programa quiere representar. Fuentes:
`literature/Mauricio.pdf` (CSDA 50, 3644–3662), `literature/518-2013-11-11-JAM106-AddOn.pdf`
(material adicional), y las que citan las demostraciones de `drvec`
(Yap–Reinsel 1995, Johansen 1991, Phillips 1991, BVECM). Los algoritmos de
Mauricio (`elfvarma.c`, `drvmlest.c`) no se revisan por dentro: sólo el modelo que
evalúan y con qué convenciones se les llama.*

*Tres estudios de detalle, con sus pruebas y los scripts que las comprueban, en
[estudio_mauricio_2026-09-23/](estudio_mauricio_2026-09-23/): `M1` el artículo y la
transformación, `M2` el AddOn, las tablas del ejemplo, el test de rango y la
previsión, `M3` las demostraciones propias de `drvec`. Los defectos de código que
salen de aquí están en [BUGS.md](BUGS.md) (BUG-46, 47, 48 y ampliaciones de 24,
26, 27 y 45).*

---

## 1. Veredicto

**La transformación es exactamente la del artículo.** `vec_shootx` construye
`C̄`, `C̄⁻¹`, `H̄`, `Λ̄` (ec. 10–14), `Φ̄₀…Φ̄_p` (ec. 16, incluido `p = 1`) y
`Φ*ₖ = C̄Φ̄ₖ`, `Θ*ₖ = C̄ΘₖC̄⁻¹`, `Σ* = C̄ΣC̄′` (ec. 18); `build_y2_levels` construye
`Ȳₜ = (∇Y₂ₜ′, Wₜ′)′` (ec. 17); la media de `Ȳ` sigue los tres casos de la
Observación 6. Comprobado contra una implementación independiente del artículo en
numpy, llamando a las funciones del propio `drvec`, en 7 configuraciones (M = 2, 3,
4; r = 1, 2; p = 1–3; q = 0–2; los tres casos; los dos layouts): `Φ*`, `Θ*`, `Σ*`
y `μ` coinciden **bit a bit**; `Ȳ` a 1.4e-14; la verosimilitud exacta de `elf`
coincide con una evaluación directa (Toeplitz por bloques y Kalman) a 2–4e-8, y
esa diferencia es entera la constante `LOG2PI = 1.837877066` truncada. La muestra
es la del artículo (`Ȳ₂…Ȳₙ`, 61 de 62 en mink–muskrat) y no hay jacobiano, ni debe
haberlo: `|det C̄| = 1`.

**El orden de filas del artículo es el «interno» de `drvec`.** El artículo define
el VEC (5) y todos sus parámetros (`Λ`, `Fᵢ`, `Θᵢ`, `Σ`) sobre
`Yₜ = [Y₁ₜ; Y₂ₜ]`, bloque cointegrado primero (ec. 7), y la serie transformada
`Ȳₜ = [∇Y₂ₜ; Wₜ]`, tendencias comunes primero (ec. 9–12, 17). El `.inp` de `drvec`
va en el orden de `Ȳ`. **Toda la familia de defectos de etiquetado (BUG-17, 18, 19,
23, 28, 29) es esto**: el informe imprime parámetros de la ec. (5) con los nombres
de `Ȳ`.

**Donde `drvec` se aparta del modelo del artículo** (§3): la clase MA por defecto
(`-marow`) es una restricción que el propio ejemplo del artículo no cumple; `-m 2`
no es la verosimilitud condicional del artículo; el test de rango se apoya en un
resultado que no cubre la verosimilitud exacta; y el diagnóstico de rango calcula
una matriz que no es la del teorema que lo justifica.

---

## 2. El artículo, redemostrado

Se rehicieron las ec. (9)–(18) paso a paso y se comprobaron a precisión de
máquina. **Ninguna derivación de la §3 es errónea.** Tiene huecos, que se cierran
aquí:

1. **La estacionariedad (pp. 3650–51).** El artículo la argumenta en tres puntos:
   (1) cada componente de `Ȳ` es estacionaria, (2) `Ȳ` sigue un VARMA unívocamente
   identificado, (3) un proceso estacionario que sigue un VARMA identificado tiene
   un modelo estacionario. Deduce la estacionariedad conjunta de la de cada
   bloque, y trata la unicidad de la transformación como si fuera la coprimidad
   por la izquierda del par transformado — lo que importa porque el artículo
   admite raíces MA en el círculo unidad. **Prueba directa** (M1 §2, M2 §1): de la
   identidad `Φ(x) = Φ̄(x) D(x) C̄` con `D(x) = diag((1−x)I_s, I_r)`,

   `det Φ*(x) = ± det Φ(x) / (1 − x)^s`,

   comprobada a 1e-14. Luego el modelo transformado es estacionario **si y sólo
   si** `|Φ(x)| = 0` tiene exactamente `s` raíces unitarias y el resto fuera del
   círculo, es decir, si `det(Λ⊥′F(1)B⊥) ≠ 0` (la condición I(1) de Johansen).
2. **La condición de rango no basta.** Contraejemplo con `rango Π = r` y la
   condición I(1) violada: `Φ*` tiene una raíz de módulo 1.000000. El artículo
   asume ambas en la §2 («`|Φ(x)| = 0` tiene D raíces unitarias» y
   «`rango Φ(1) = M − D`»), así que su resultado es correcto bajo sus hipótesis;
   lo que falta es decir que la segunda sola no alcanza.
3. **Lo que el artículo no dice y hace falta:** que el jacobiano es 1 (lo que
   legitima maximizar sobre una transformación de los datos que depende de `B₂`);
   que «exacta» significa la densidad marginal de `Ȳ₂…Ȳ_N`, con `W₁` descartado;
   que `Σ* = C̄ΣC̄′`; y el intercepto VEC equivalente a cada caso:
   `E[Ȳ] = m ⇔ μ = Φ̄(1)m`, con `Φ̄(1) = [F(1)B⊥, Λ]` no singular. Así, el caso 1 es
   el H₂ de Johansen (sin constante), el 2 el H₁* (constante restringida,
   `μ = Λm_W`) y el 3 el H₁ (constante libre, deriva `γ` con `B′γ = 0`). Los dos
   casos con tendencia no están ni en el artículo ni en `drvec`.
4. **«Unívocamente identificada»** establece la identificación de la
   *reparametrización*. El VARMA sobre `Ȳ` sigue necesitando las condiciones de
   Yap–Reinsel, que ni el artículo ni `drvec` imponen (§4, BUG-46).
5. **La ec. (16) con `p = 1`**: escrita tal cual no tiene sentido (`F₀` no existe);
   debe leerse `Φ̄₁ = C̄⁻¹H̄ − Λ̄`. `drvec` lo hace bien.
6. **El AIC** está impreso con el signo cambiado en la §4 (`AIC = −(2L* + 2K)/N`
   debe ser `(−2L* + 2K)/N`); las tablas usan el correcto.

### El AddOn

- **A1** (VAR(1) bivariante): correcto. Tres huecos cerrados — (A.4) necesita la
  primera columna de `Π` no nula (si no, `B` no se puede normalizar); «(A.5) es
  estacionario» supone que `W` arranca de su distribución estacionaria; y una
  identidad más limpia que la del AddOn, `1 − λ₁ − λ₂β₂ = μ₂`, probada para todo
  M y r. **`drvec` se reduce exactamente a A1**: `Φ*`, `Σ*` y `Ȳ` difieren en 0.0.
- **A2** (vivienda, modelo (A.14)): las tablas son internamente coherentes (AIC y
  BIC con N = 112, los LR, los autovalores). Pero el `Θ̂` EML **no es invertible**
  (θ̂₂₂ = 1.0844 y 1.1318), contra la hipótesis (1) del propio artículo. **`drvec`
  no puede expresar (A.14)**: no tiene máscara MA por retardo ni operador MA
  estacional, y su puerta de invertibilidad excluye el óptimo publicado. Los datos
  no están localizados. `DRVEC_REFERENCE.md` escribe (A.14) mal (signo del término
  de corrección, MA en el retardo 1 en vez del 12, doble diferencia).

### El ejemplo: por qué la columna EML nunca se reprodujo

Ya estaba documentado (`HOMOLOGATION.md`, `ANALISIS_PRELIMINAR.md` §5) que las
logL EML publicadas (15.6116, 15.1257) no se reproducen. Tres hechos nuevos:

1. **`drvec` no ajusta otro modelo.** Datos, orden (fijado por `E[W] = 8.13`),
   muestra (N = 61 por los criterios de información), layout, condicionamiento y
   caso coinciden con el artículo. En los valores publicados de la Tabla 4, la
   logL de `drvec` es la de una verosimilitud exacta independiente a 2.5e-8.
2. **Los puntos EML publicados de las Tablas 2 y 4 no son invertibles:** los
   autovalores de `Θ̂₁` son 0.8461 / **−1.0063** y 0.9904 / **−1.0078**. El artículo
   menciona 0.9904 y calla el otro. El exceso sobre 1 es muy superior a lo que
   puede producir el redondeo a cuatro decimales. La Tabla 5 (−0.9971) sí es
   invertible.
3. **De dónde salen las logL publicadas** (M2 §3.5). Evaluadas con el `Σ̂`
   publicado pero con la forma cuadrática igualada a `nM` — como si ese `Σ̂` fuera
   el que maximiza —, se obtienen las tres cifras publicadas al nivel del
   redondeo:

   | tabla | `L_conc + n·ln c` | publicada | diferencia |
   |---|---|---|---|
   | 2 | 4.7544 + 61·ln 1.1946 = 15.60 | 15.6116 | −0.009 |
   | 4 | 5.1756 + 61·ln 1.1765 = 15.09 | 15.1257 | −0.037 |
   | 5 | 0.5795 + 61·ln 1.2135 = 12.38 | 12.4001 | −0.018 |

   El álgebra es exacta: escalar `Σ` por `c` con `Φ`, `Θ` fijos da
   `|V(cΣ)| = c^{nM}|V(Σ)|` y `S(cΣ) = S(Σ)/c`, luego evaluar en `Σ̂_pub` con
   `S := nM` es `L_conc + (nM/2)·ln c`. Tres tablas independientes coinciden: **es
   evidencia fuerte** de que el `Σ̂` publicado es un 15–18 % pequeño para sus propios
   parámetros y de que las logL se calcularon como si fuera el óptimo. Las logL
   verdaderas en los puntos publicados son **4.75 / 5.18 / 0.58**. El mecanismo en
   el programa del autor queda **abierto**: omitir la parte de pre-muestra de la
   forma cuadrática se probó y no lo explica (es un 2–3 %).
4. **Consecuencia para el argumento del artículo:** con la verosimilitud exacta
   correcta, el contraste de `B = [1, 0]′` da LR = 7.90 (p = 0.005, 60 arranques),
   así que «no se rechaza al 1 %» no se sostiene — con la reserva de que los dos
   ajustes de `drvec` quedan sobre el círculo unidad del MA.
5. **La columna CML queda abierta:** sus pares (L, Σ̂) son coherentes y un CML de
   12.41 es alcanzable, pero el punto CML impreso no reproduce su propia L con
   ninguna de tres convenciones de arranque, y el artículo no dice cuál usó.
   `drvec` no tiene verosimilitud condicional con la que compararla (BUG-47).

Los tres hechos deben constar en `HOMOLOGATION.md` y `benchmark/README.md` cuando se
actualicen: cambian «no reproducible» por «explicado».

---

## 3. Dónde se aparta `drvec`

| | qué | ¿cambia el modelo? |
|---|---|---|
| clase MA por defecto `-marow` | anula las filas de `∇Y₂` de cada `Θₖ`: subclase propia del modelo del artículo, cuyo propio ejemplo cae fuera (fila muskrat de `Θ̂₁`: (−0.8953, −0.0174) en la Tabla 4, (−0.6039, −0.1837) en la 5). `-mafree` es el modelo del artículo | **sí** — BUG-48 |
| `-m 1` / `-m 2` | `-m 1` es la exacta truncada (ξ a 1e-3), `-m 2` la exacta sin truncar, rotulada «Conditional». La CML del artículo no existe | la etiqueta, no el modelo; la truncación mueve logL hasta 4.6e-3 — BUG-47 |
| test de rango | secuencial λmax sobre la verosimilitud **exacta**; la teoría citada es para la **condicional** | la distribución, sí — BUG-24, 26, 27 |
| layout `-differenced` | equivalente en los casos 2 y 3 (`E[W]` absorbe `−B₂′c`); en el caso 1 es otro modelo: `drvec` avisa pero no lo rechaza | caso 1 sí |
| `α = −Λ`, `β = B`, `Σ = σ²Q` con `Q₁₁ = 1` y `σ²` concentrada | equivalentes (probado) | no |
| `-warma` | exactamente el modelo del artículo restringido a `Fᵢ = MᵢB′`, `Θ = [T₁₁ T₁₁B₂′; 0 0]`, con inversa cerrada | restricción declarada |
| `r = 0` | el límite natural, un VARMA(p−1, q) sobre `∇Y` | no |
| `-interv` | aproximación en dos pasos, declarada, de la Observación 2 | declarada |

Menores: en el caso 3 la ecuación VEC impresa omite la deriva `γ` (la estimación es
correcta); `MODEL.md` §1/§5.3 da el signo de `β` al revés que `THEORY.md` y el
código; el `.out` cita AS 311 (Mauricio 1997) y la cabecera de `elfvarma.c` cita
Mauricio (1995) — no afecta a ningún número.

---

## 4. Las demostraciones propias de `drvec` (`DEMOSTRACIONES.md`)

| resultado | veredicto |
|---|---|
| §0, la clase 𝒞 = (a)(b)(c) | **incompleta**: (a)–(c) no implican rango r, y T3/T4 lo contradicen. Falta (d): `rango(Λ⊥′Θ(1)) = s` |
| T1, C1.1, C1.2 | prueba correcta (T1 es una biyección sobre su imagen) |
| T2 | correcta con hueco: el polo simple y la unicidad de C se afirman, no se derivan (suplidos) |
| **T3** | prueba correcta; **el código no la implementa** (BUG-46) |
| C3.1 | prueba correcta; su nota sobre el código es falsa |
| C3.2 | correcta con hueco (el punto 3 es impreciso; versión exacta suplida) |
| T2′ | conclusión correcta; la prueba se apoya en la identificación. Prueba directa suplida (§2.1); muestra además que la puerta AR del motor impone (b)+(c) |
| Lema A | coherente con el motor; sus cifras de precisión dependen de los datos |
| T4, T5 | correctos con huecos. «No está en 𝒞» contradice §0; prueba rigurosa de finitud suplida. «Invisible» significa «admitido», y vale en todas las clases MA, no sólo la libre |
| T6 | directo y recíproco correctos (simulado exacto a 3e-14); el «sólo si» necesita que la representación VEC esté identificada. El recíproco del BVECM es falso (contraejemplo VEC(2) con rango Γ₁ = 2) |
| C6.1, C6.2, T6b | correctos (C6.2 escribe mal Θ(L), sin consecuencia) |
| **C6.3** | (1)(2) correctos. **«La puerta ES la condición de admisibilidad» y «no alcanzable en -marow» son falsos**, con contraejemplos en los dos sentidos (BUG-48) |
| T7 | correcto (r raíces unitarias, no «una raíz») |
| **T8** | **el enunciado no coincide con la fuente y el código corre fuera de sus condiciones**: Yap–Reinsel Thm 3 es un test de traza (r frente a m), con Θ libre y estrictamente invertible, por ML condicional (BUG-27) |
| T9 | prueba correcta (confirmada con una verosimilitud exacta independiente); la tolerancia del código no es una cota (BUG-47) |
| P1 | heurística, razonable |
| P2 | correcta con hueco (el conjunto de condicionamiento: hace falta que la primera observación de `Y₁` no aporte información más allá de `Ȳ`); el mapa de errores en niveles del código coincide, y la previsión recalculada directamente en niveles coincide a 4e-7, las bandas a 0.3 % con 200 000 trayectorias |
| T10 | omite la hipótesis explícita de Phillips `Ω = 2πf(0) > 0` (Thm 1′), es decir `det Θ(1) ≠ 0` |
| T11 | coincide con Johansen Thm 5.1, pero es para el VAR; el resultado VARMA es Yap–Reinsel Thm 2 |
| §7, identificación | citas correctas. **El punto 2 es falso**: se prueba (y se comprueba en 20 000/20 000 extracciones) que `rango(Λ⊥′Θ(1)) = s ⇔ rango[Φ(1) Θ(1)] = M`, la coprimidad por la izquierda en `z = 1`. Todo punto inadmisible es no identificado: un factor `(1 − L)` común. Es una condición que Yap–Reinsel y Mauricio ya suponen; lo que falta es que nada la imponga |

**Qué decisiones de diseño descansan en algo falso o no probado:**

1. el diagnóstico de rango y todo lo que lo usa (`-rankadm`, `-specs`, el suelo
   0.2) — BUG-46;
2. la clase MA por defecto, justificada por C6.3 — BUG-48;
3. la validez del test de rango con `q ≥ 1` y de sus valores asintóticos para la
   verosimilitud exacta — BUG-26, 27;
4. el certificado de la puerta de entrada, cuya tolerancia es la truncación de ξ —
   BUG-47.

---

## 5. Qué hay que decidir antes del porte

El porte no debe copiar el C: debe implementar el modelo del artículo. Con lo
anterior, cinco decisiones que no son de programación:

1. **La clase MA por defecto.** `-marow` (lo de hoy, justificado en falso pero
   quizá defendible por las medidas) o `-mafree` (el modelo del artículo). Si se
   mantiene `-marow`, declararlo como restricción y construir el contraste frente
   al libre.
2. **La verosimilitud.** Exacta sin truncar como única, o mantener la truncación;
   y si se quiere comparar con las columnas CML, implementar la condicional.
3. **El test de rango.** La λmax asintótica no vale para la LR exacta (desplazada
   ≈ −log T bajo H0). Opciones: bootstrap como referencia (conservando todas las
   extracciones), o la LR condicional para el test y la exacta para la estimación.
   La tabla del caso 1 hay que cambiarla en cualquier caso.
4. **El diagnóstico de rango / identificación.** Sustituir `G` por
   `σ_M([Φ(1) Θ(1)])` (coprimidad en `z = 1`), que es la condición correcta y la de
   la literatura.
5. **`DEMOSTRACIONES.md`**: corregir §0 (añadir (d)), T3 (la nota del código), C6.3,
   T8, T10, T11 y §7 punto 2, e incorporar la prueba directa de estacionariedad.
   Las correcciones están escritas en `estudio_mauricio_2026-09-23/M3/STUDY_M3.md`.
