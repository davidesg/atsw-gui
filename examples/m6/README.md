# Ejemplo m6 — el empleo español por sectores (Relloso 1997)

Un recorrido completo de la **escalera metodológica** de la escuela de Treadway
(Muñoz Polo 2001) con `drtran`, de los univariantes al modelo multivariante de
funciones de transferencia. Es el caso canónico para aprender el flujo del sistema.

```bash
./run.sh                      # usa ../../bin/drtran
DRTRAN=/ruta/a/drtran ./run.sh
```

## El caso

Seis series trimestrales del empleo (EPA, 1976:III–1993:II, 69 obs), de la Tabla 4
de Relloso (1997, ICAE 9720): **P** (población activa), **EA** (agricultura),
**EP** (parados), **EI** (industria), **EU** (construcción), **EC** (servicios).
Los datos y modelos están en `../../tests/data/m6/`.

## La escalera, peldaño a peldaño

### Paso 1 — univariantes (`.pre`)

Los seis `.pre` **son** los ajustes UTI/MEG de la Tabla 4, construidos por
`tests/build_m6.py`. Cada uno homologa con `fue`: `drtran X.pre Y.pre -0` reproduce
el ajuste univariante. Solo **EA** lleva estacionalidad estocástica (∇∇₄ con un MEG
de Gallego–Treadway); las demás son ∇² con estacionalidad determinista, como el
legacy m6-1.

### Paso 2 — el modelo DIAGONAL (`drtran -0`)

```bash
drtran M6_EP.pre … M6_P.pre -0 -c m6.cns
```

Estima los seis univariantes **juntos**, con `Σ = σ²·Q` y las **tres covarianzas
contemporáneas** que libera el legacy (EA·EI, EA·EC, EI·EU). Es el arranque del
multivariante: la diagonal ya está en su óptimo univariante.
**log-likelihood ≈ −1709.5.**

### Paso 3 — IDENTIFICAR la red (`drtran -i`)

```bash
drtran … -0 -c m6.cns -i
```

El *"-p del sistema entero"*: tras el diagonal, `drtran` lee las **CCF de los
residuos** y propone la red — covarianzas contemporáneas (k=0), enlaces dirigidos
(k>0 ⇒ i→j) y feedbacks. Recupera las 3 covarianzas del legacy y el enlace limpio
**EI→EP con b=1**. Es una **guía** de candidatos: hay que podar por exogeneidad,
aciclicidad y verosimilitud del retardo (Muñoz Polo 2001, §2.6).

### Paso 4 — la RED de transferencias (`drtran -n`)

La red del legacy m6-1: **EC→EU→EI→EP** con el atajo **EC→EP** (en
`m6_net.dag`).

**(a) numeradores libres** — `drtran … -n m6_net.dag -c m6_net.cns`
→ **ℓ ≈ −1697.6**: los transfers ganan ~12 sobre el diagonal.

Los numeradores del legacy están **factorizados con parámetros compartidos**. Para
medirlos hay que aislarlos de las **intervenciones compuestas** (débilmente
identificadas: saltan de modo y dominan la ℓ), fijando los deterministas en sus
valores Relloso con **`-D -E`**.

**(b) con los PRODUCTOS** — `-c m6_net_prod.cns`
La MA compartida: `EP←EI` usa la MA de EI, `EU←EC` la de EU. En Box–Jenkins
`ω(B)=ω₀−ω₁B`, el factor `−x(1−m·B)` da `ω₁ = ω₀·m`:

```
omega1[1] = omega1[0] * theta_2[B^1]     # EP<-EI, m = MA de EI
omega4[1] = omega4[0] * theta_3[B^1]     # EU<-EC, m = MA de EU
```

Coste: **Δℓ ≈ 0.4** por dos restricciones — casi gratis; la estructura es
consistente con los datos, y mueve los coeficientes hacia el legacy.

**(c) con la ESTRUCTURA COMPLETA** — `-c m6_net_full.cns`
Añade el **factor fijo `(1−B)`** de `EI←EU = −(1−B)(x8+x9B+x10B²)`, que impone
`ν_num(1)=0`, i.e. una **combinación lineal**:

```
omega3[0] = omega3[1] + omega3[2] + omega3[3]
```

Total: 2 productos + 1 combinación lineal, **Δℓ = 2.5 sobre 3 g.l.** frente al
transfer libre (con `-D -E`). LR = 5.0; χ²(3)₀.₉₅ = 7.81 ⇒ **no rechazada**
(p≈0.17). Toda la estructura del legacy expresable con la tabla de slots es
consistente con los datos.

> `EP←EC = (1−x14B)(x12+x13B)` **no** se impone: sus tres parámetros son locales
> (no compartidos), así que el numerador libre ya cubre el mismo espacio — factorizar
> solo restringiría a raíces reales, sin ganar grados de libertad.

## Las piezas

| fichero | qué es |
|---|---|
| `M6_*.pre` | los seis univariantes de la Tabla 4 |
| `m6.cns` | las 3 covarianzas libres del diagonal |
| `m6_net.dag` | la red EC→EU→EI→EP + EC→EP |
| `m6_net.cns` | red con numeradores libres |
| `m6_net_prod.cns` | + los 2 productos de MA compartida |
| `m6_net_full.cns` | + el factor fijo (1−B): la estructura completa |

## Para profundizar

- `docs/M6_TABLA4_BASELINE.md` — la referencia completa (§1–§11): Tabla 4, la
  codificación `.pre` del MEG, el bug de Nyquist, la lectura de CCF, la red, las
  iteraciones y los productos/combinaciones lineales.
- `docs/CASO_EMPLEO.md` — los criterios destilados.
- `docs/M6_EA_GUIADO.md` — la identificación guiada de EA, paso a paso.
- `M6_EJERCICIO.md` — la metodología y lo que reconstruye `build_m6.py`.
