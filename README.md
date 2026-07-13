# drtran

**Modelos de transferencia de Box–Jenkins por máxima verosimilitud exacta.**

drtran es el **puente** entre dos programas que ya funcionan:

- **[fue]** — identifica y estima modelos **univariantes** (ARIMA + Box–Cox +
  deterministas). Produce un `.pre` por serie.
- **[drvarma]** — evalúa la **verosimilitud exacta VARMA** de Mauricio (`elf`) y la
  maximiza con BFGS factorizado.

drtran lee dos modelos ya especificados en fue y los estima **conjuntamente**,
todos los parámetros a la vez:

```
Y_t  =  ω(B)/δ(B) · B^b · X_t  +  N_t
```

## Qué hace, en una orden

```sh
drtran IPC.pre WTI.pre
```

1. **Identifica**: preblanquea la entrada con su propio ARMA, aplica el mismo
   filtro a la salida y lee la CCF → propone (b, r, s).
2. **Estima**: máxima verosimilitud exacta sobre el VARMA bivariante — la
   transferencia, los dos ARMA, los deterministas, las medias y las varianzas.
3. **Valida**: CCF entre el ruido estimado y la entrada preblanqueada → adecuación
   de la transferencia y exogeneidad de la entrada.

Y con `-f 12`, prevé 12 periodos con bandas al 95%.

## Cómo funciona: el *cast*

El modelo de transferencia se reescribe como un **VARMA bivariante diagonal**, que
es lo que el `elf` de drvarma sabe puntuar:

```
serie 1 = w_Y − transferencia   (el ruido N)   AR/MA = el ARMA de Y
serie 2 = w_X                   (la entrada)   AR/MA = el ARMA de X
covarianza diagonal
```

Todo el acoplamiento vive en `transferencia_t = Σ_j ν_j · w_X(t−j)`. Con ω = 0 el
modelo se parte en dos univariantes independientes — y **ahí está la prueba de que
el puente es correcto** (ver *Validación*).

### Principio de diseño

> **El `elf` de drvarma se usa TAL CUAL.** Es la implementación de referencia de la
> verosimilitud exacta. Cualquier discrepancia con fue es un bug de drtran (del
> cast o de la serie estacionaria), **nunca** de `elf`.

## El `.pre` manda

drtran **no reinterpreta** el modelo: lo respeta. Cada coeficiente ARMA, cada
ω/δ de las deterministas y cada media son **libres o fijos según su flag** en el
`.pre` de fue. Un `0.0000  0` es un coeficiente **FIJO**, no un valor inicial.

Deterministas soportadas (las mismas que fue): `impulse`, `compimp`, `step`,
`ramp`, `easter`, `trend`, `cos`, `sin`, `alter` — con estructura racional
ω(B)/δ(B) por variable.

Las variables **no estándar** quedan fuera **por diseño**: son una versión
rudimentaria de un modelo de transferencia con input X, que es justo lo que drtran
estima bien. Especifícalas como transferencia.

## Uso

```
drtran salida.pre entrada.pre [opciones]
```

El **primer** fichero es la endógena (Y); el **segundo**, la entrada exógena (X).

| | |
|---|---|
| `-b N` `-r N` `-s N` | imponer el retardo puro, el denominador y el numerador (por defecto: identificados) |
| `-0` | **sin transferencia**: los dos univariantes estimados conjuntamente. Es el modo de homologación con fue |
| `-N` `-X` | fijar el ARMA del ruido / de la entrada |
| `-D` `-E` | fijar TODOS los deterministas de Y / de X |
| `-M` | fijar ambas medias en el valor del `.pre` |
| `-f L` | prever L periodos con bandas al 95% |
| `-o F` | escribir los resultados en F |
| `-v` | traza del optimizador |

## Validación

`./test_battery.sh` — **80 comprobaciones**, sobre tres fuentes de verdad:

**1. Homologación con fue.** Estimar conjuntamente dos modelos con estructura
diagonal y sin transferencia (`-0`) debe reproducir a fue ejecutado sobre cada
serie por separado. Si esto falla, nada de lo demás es creíble.

| | drtran conjunto | fue por separado |
|---|---|---|
| φ_N | 0.402839 | 0.402839 |
| φ_X | 0.299193 | 0.299193 |
| μ_Y | 0.154472 | 0.154472 |
| **logL** | **−767.424341** | **−767.4243** (suma) |

Se homologan seis modelos reales del estudio de inflación (`tests/cases/`), cada
uno ejercitando una parte distinta del motor: estacionalidad determinista completa,
Box–Jenkins airline (∇∇₁₂ + MA anual), y MEG (factores irreducibles y de frecuencia
fija). **Todos coinciden con fue al sexto decimal.**

**2. Verdad sintética** (`tests/gen_synthetic.py`). Se simula `Y = ν(B)X + N` con
(b, r, s, ω) conocidos y se exige que se recuperen: la identificación propone los
órdenes verdaderos, la estimación recupera los ω y la varianza del error de
previsión se descompone como manda la teoría.

**3. Pass-through.** Con `Y = X` la verdad es **ω₀ = 1** (drtran da 1.000011).

Además: **robustez al arranque** — se perturban las preestimaciones del `.pre` y el
modelo debe converger al **mismo** óptimo. El óptimo tiene que ser un *atractor*,
no un eco de los valores iniciales.

## Compilar e instalar

```sh
make                    # -> bin/drtran
make test               # la batería de comprobaciones
sudo make install       # -> /usr/local/bin/drtran   (como fue y drvarma)
```

Sin `sudo`, en tu propio `$HOME`:

```sh
make install PREFIX=$HOME/.local
```

`make uninstall` lo quita; `make help` resume los objetivos.

Sin dependencias externas: el motor de drvarma va incluido en `src/`.

## Estructura

```
src/     drtran.c        driver, CLI, identificación, previsión
         tran_shootx.c   el cast transferencia -> VARMA bivariante
         fue_pre_reader.c  lector del .pre de fue (puerto fiel de su parser)
         elfvarma.c drvmlest.c qnewtopt.c nlatools.c
                         motor de drvarma: verosimilitud exacta + optimizador
         diagnose.c forecast.c
                         diagnósticos y previsión de drvarma
tests/   cases/          .pre reales (SF_MEG), con su referencia de fue
         data/           casos sintéticos con verdad conocida
         gen_synthetic.py
```

## Documentación

- **[TODO.md]** — estado, hitos cerrados y hoja de ruta.
- **[BRIDGE_DESIGN.md]** — el diseño del puente, las decisiones y por qué.

[fue]: ../atws/fue
[drvarma]: ../drvarma_source
[TODO.md]: TODO.md
[BRIDGE_DESIGN.md]: BRIDGE_DESIGN.md
