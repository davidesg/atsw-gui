# drtran

> **Nota técnica:** [`docs/drtran-note.tex`](docs/drtran-note.tex) — *Re-implementing
> Box–Jenkins transfer function models on an exact VARMA likelihood: engineering
> notes, and one identification hazard.* Qué se aparta este programa de las
> soluciones originales, y por qué, con las cifras. `make doc` la compila.

**Modelos de transferencia de Box–Jenkins por máxima verosimilitud exacta.**

drtran es el **puente** entre dos programas que ya funcionan:

- **[fue]** — identifica y estima modelos **univariantes** (ARIMA + Box–Cox +
  deterministas). Produce un `.pre` por serie.
- **[drvarma]** — evalúa la **verosimilitud exacta VARMA** de Mauricio (`elf`) y la
  maximiza con BFGS factorizado.

drtran lee los modelos ya especificados en fue —una salida y **una o varias
entradas**— y los estima **conjuntamente**, todos los parámetros a la vez:

```
Y_t  =  Σⱼ  ωⱼ(B)/δⱼ(B) · B^bⱼ · Xⱼ,t  +  N_t
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

**Mirar antes de comprometerse.** `-p` hace **solo el preblanqueo**: filtra, dibuja
la CCF y sugiere (b, r, s). No estima ni itera.

```sh
drtran IPC.pre WTI.pre -p
```

**Salida.** La consola solo da lo esencial —convergencia y qué modelo se ha
estimado—; el detalle completo (tablas, gráficos de la CCF, diagnósticos,
previsiones) va al fichero `<modelo>.out`. El nombre del modelo se da con `-m`; si
no, se deriva de los dos `.pre`.

## Cómo funciona: el *cast*

El modelo de transferencia se reescribe como un **VARMA bivariante diagonal**, que
es lo que el `elf` de drvarma sabe puntuar:

```
serie 1     = w_Y − Σⱼ transferenciaⱼ   (el ruido N)   AR/MA = el ARMA de Y
serie j+1   = w_Xⱼ                     (la entrada j)  AR/MA = el ARMA de Xⱼ
covarianza diagonal
```

Hasta **7 entradas** (un cast de hasta 8 series).

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
drtran salida.pre entrada1.pre [entrada2.pre ...] [opciones]
```

El **primer** fichero es la endógena (Y); los demás, las entradas exógenas.

| | |
|---|---|
| `-m NAME` | nombre del modelo; los resultados van a `NAME.out` (por defecto: `<salida>_<entrada>`) |
| `-p` | **solo preblanqueo**: filtra, grafica la CCF y sugiere (b, r, s). No estima |
| `-b N` `-r N` `-s N` | imponer el retardo puro, el denominador y el numerador (por defecto: identificados). Con varias entradas, lista separada por comas: `-b 1,0 -s 0,1` |
| `-0` | **sin transferencia**: los dos univariantes estimados conjuntamente. Es el modo de homologación con fue |
| `-N` `-X` | fijar el ARMA del ruido / de la entrada |
| `-D` `-E` | fijar TODOS los deterministas de Y / de X |
| `-M` | fijar ambas medias en el valor del `.pre` |
| `-f L` | prever L periodos con bandas al 95% |
| `-c F` | **restricciones**: parámetros compartidos y fijos (ver abajo) |
| `-o F` | escribir los resultados en F en lugar de `NAME.out` |
| `-v` | traza del optimizador |

## Parámetros compartidos y fijos

Un parámetro puede aparecer en **varios sitios** de la estructura con **un solo
grado de libertad**. Eso es lo que hace **racional** a una transferencia dentro de
un sistema: en los modelos m6 de Mauricio, el mismo `x6` está en la dinámica propia
de EI **y** en la transferencia EI→EP.

Se declara en un fichero (`-c`), con los **mismos nombres que el programa imprime**:

```
delta1[1] = phi_2[B^1]   # el denominador de la transferencia ES el AR de la entrada
omega1[0] = omega2[0]    # compartir entre dos entradas
omega2[1] = 0.0          # fijar en un valor
```

El optimizador solo ve los parámetros libres; el cast expande ese vector corto a la
estructura completa. El informe marca cada parámetro como libre, `(fixed)` o
`(= otro)`.

> El mecanismo está rescatado del **TASTE** de Treadway (1991), cuyos registros
> `USMODEL` y `TFINPUT` llevaban un `point: Apuntador a parms` — un entero por
> parámetro que decía qué posición de la estructura ocupaba.

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

## Licencia

**GNU General Public License, versión 2 o posterior** (fichero `COPYING`).

drtran incorpora, literalmente, el motor de verosimilitud exacta VARMA de
**drvarma/ART** — `elfvarma.c`, `drvmlest.c`, `qnewtopt.c`, `nlatools.c`,
`diagnose.c`, `forecast.c` —, que es GPL. drtran es por tanto obra derivada y se
distribuye bajo la misma licencia.

El copyright del conjunto es de **A.B. Treadway, J.A. Mauricio y D.E. Guerrero**:
el núcleo numérico es de Mauricio (`elfvarma.c` *es* Mauricio, J.A. (1995), *JASA*
90, 282-291), y el diseño del *cast* parámetros → estructura VARMA procede de los
modelos escritos a mano de la línea ART.

## Documentación

- **[TODO.md]** — estado, hitos cerrados y hoja de ruta.
- **[BRIDGE_DESIGN.md]** — el diseño del puente, las decisiones y por qué.

[fue]: ../atws/fue
[drvarma]: ../drvarma_source
[TODO.md]: TODO.md
[BRIDGE_DESIGN.md]: BRIDGE_DESIGN.md
