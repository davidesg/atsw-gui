# Ejemplo m6_EA — EA univariante, la diagonal del sistema

El **primer peldaño** de la escalera del caso m6 (`../m6/`): la identificación
**univariante** de **EA** (ocupados en agricultura, ganadería y pesca; EPA
trimestral, 1976:III–1993:II, 69 obs, niveles). El modelo univariante de EA es la
**diagonal** de la que arranca el multivariante de funciones de transferencia.

EA es la **única** serie del sistema con **estacionalidad puramente estocástica**
(no colapsa a determinista): fija el patrón de cómo se codifica un **MEG**
(modelo estructural generalizado de Gallego–Treadway) con las dos frecuencias
estacionales separadas, λ₁ (π/2) y λ₂ (π).

## Las piezas

| fichero | qué es |
|---|---|
| `EA.inp` | los datos crudos de EA (niveles), punto de partida |
| `EA_meg.pre` | el modelo final: ∇∇₄ + MEG estocástico + 5 escalones |
| `work/` | corridas de trabajo (efímeras, **no versionadas** — `.gitignore`) |

## El modelo — ∇∇₄ con MEG estocástico

```
∇∇₄ EA_t  =  I(intervenciones)  +  (1 − θB)·(1 − λ₂B)·(1 − λ₁·2cos(π/2)B + λ₁²B²) a_t
```

- **θ** (f=0): MA(1) regular — el **testigo de sobre-diferenciación** de d=1
  (invertible y lejos de 1 ⇒ d=1 correcto; ver el doc guiado, Paso 2).
- **λ₂** (f=2, π): MA en la frecuencia de Nyquist.
- **λ₁** (f=1, π/2): MA(2) de frecuencia fija (un solo coeficiente libre; el término
  en B se **deriva**, `fue.c:4064`).
- **5 escalones**: II/87 (cambio metodológico de la EPA, justificado
  extramuestralmente) + 4 incidentes influyentes (I/85, I/89, IV/89, I/92).

## Homologación con la Tabla 4 de Relloso (1997)

`drtran` estima EA junto a un segundo univariante (aquí P), con estructura diagonal
y sin transferencia (`-0`); el bloque de EA reproduce el ajuste univariante de `fue`.

```bash
../../bin/drtran EA_meg.pre ../../tests/data/m6/M6_P.pre -0
```

| operador | Tabla 4 (control) | drtran |
|---|---|---|
| θ (f=0) | .43 (.20) | 0.4306 (0.201) |
| λ₂ (f=2, π) | −.72 (.14) | −0.7209 (0.140) |
| λ₁ (f=1, π/2) | −.68 (.10) | −0.6839 (0.106) |
| σ̂ₐ | 27.3 | 27.33 |

Coeficientes, errores estándar y σ̂ₐ coinciden con el control. (σ̂ₐ = √Σ[1,1]
deshecho el reescalado ×100; converge en 11 iteraciones.)

## De aquí al multivariante

`EA_meg.pre` es la contrapartida **estimada** del baseline redondeado
`../../tests/data/m6/M6_EA.pre` que consume `../m6/run.sh`. En el sistema, las
correlaciones contemporáneas fuertes de EA (con EI y EC) son dos de las tres
covarianzas que el legacy libera en el modelo diagonal — el arranque del Paso 2 de
`../m6/`.

## Para profundizar

- `docs/M6_EA_GUIADO.md` — la **identificación guiada paso a paso** con ART
  (transformación, d, D/MEG, intervenciones, contraste por frecuencia): el criterio
  y la decisión de cada peldaño. *(Pasos 1–4 cerrados; 5–6 en curso.)*
- `../m6/README.md` — la escalera completa hasta el multivariante.
- `docs/M6_TABLA4_BASELINE.md` — la referencia de la Tabla 4 y la codificación
  `.pre` del MEG (incl. el bug de Nyquist en `CalcNonsOp`).
