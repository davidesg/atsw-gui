# Encuadre del estudio de `drvec`

*Inventario de las cuatro fuentes, con rutas verificadas el 2026-08-16, la
pregunta que responde cada una y el orden de lectura. Este documento **no** es el
estudio: existe para que el estudio arranque en frío sin repetir el
reconocimiento.*

## Por qué este estudio, y cuándo

`drvec` es la pieza que le falta a la suite: el driver que estima **modelos de
corrección de error** sobre `drvarma`. Va a usar **el cast de `fue`**, igual que
`drtran` — no es eficiente, pero es sólido, y eso es lo que importa mientras una
pieza se construye.

De ahí que el refactor del cast propio de `drtran` esté **aplazado a propósito**
(ver `drtran-python/TODO.md` §APLAZADO): no se diseña un cast especializado para
un consumidor cuando el segundo aún no existe. El orden acordado es `drvec`
primero con el cast de `fue`, y después una investigación que integre a los dos.

**Lo accionable durante este estudio**, y que se pierde si no se toma en su
momento: anotar **qué le pide `drvec` al cast** — cuántos casts por evaluación,
qué parte de su vector cambia entre evaluaciones, qué necesita constante. Es la
entrada del paso 2 de aquel plan, y es lo que en `drtran` hubo que reconstruir a
posteriori.

## Las cuatro fuentes

### 1. Mauricio (2006) — el fundamento

`~/Dropbox/SRC/drvec/literature/`

| fichero | qué es |
|---|---|
| `Mauricio.pdf` | Mauricio, J.A. (2006). «Exact maximum likelihood estimation of partially nonstationary vector ARMA models». *Computational Statistics & Data Analysis*, 50, 3644–3662 |
| `518-2013-11-11-JAM106-AddOn.pdf` | material adicional (AddOn) |
| `Hillmer-LikelihoodFunctionStationary-1979.pdf` | Hillmer (1979), la verosimilitud del caso estacionario — el antecedente sobre el que se apoya la transformación |

**Responde:** qué transformación convierte un VARMA parcialmente no estacionario
en algo que la ML exacta puede evaluar, y **qué debe poder expresar el modelo**.
Es lo que fija los requisitos; todo lo demás se juzga contra esto.

### 2. `drvec` — el código de Mauricio, y lo que se le ha añadido

`~/Dropbox/SRC/drvec/`

```
src/drvec.c            818 líneas — frontend VEC (vec_shootx, init_guess, main)
src/drvec.c.mauricio   818 líneas — el original
src/drvec.c.bak
src/elfvarma.c   drvmlest.c   qnewtopt.c   nlatools.c   ← declarados «untouched»
docs/LEGACY_NOTES.md   145 líneas — «solutions rescued from legacy drv_project»
data/  datasets/  benchmark/  tools/  Makefile  README.md
```

Dos hechos comprobados que conviene no volver a averiguar:

* **La arquitectura ya sigue el patrón de `drtran`**: pieza propia arriba
  (`drvec.c`), motor publicado intacto abajo. Encaja sin fricción con el resto de
  la suite.
* **`drvec.c` NO es idéntico al original.** Lo añadido es documentación, y
  concretamente **el layout del vector de parámetros**, citando «Mauricio 2006,
  Remark 1 and 6»: los tres casos de la media (ninguna; `E[W_j]`, j=1..r;
  `E[∇Y₂_i]` más `E[W_j]`), la matriz de ajuste **Λ (M×r)**, y sigue.

  Eso **es el contrato del cast de `drvec` en embrión** — el análogo de
  `fue/docs/CAST.md` §4. Es el mejor punto de entrada del estudio y probablemente
  lo que más trabajo ahorra.

**Responde:** hasta dónde llega la implementación y qué falta *de verdad* — que no
es lo mismo que lo que falta *aparentemente*. El código no lleva marcas `TODO` ni
`FIXME`, así que lo incompleto habrá que establecerlo contrastándolo con el
artículo, no leyéndolo declarado.

### 3. `drv_project` — el legado

`~/Dropbox/SRC/drv_project/`

```
src/main.c    3.393 líneas
src/drvmlest.c  elfvarma.c  nlatools.c  gnuplot_i.c  src/olds/
bin/ build/ data/ gui/ include/ Makefile
```

**Responde:** qué se resolvió ya una vez. `drvec/docs/LEGACY_NOTES.md` afirma
rescatar soluciones de aquí, así que **leer las notas antes que el código**: si
cubren lo que hace falta, `main.c` (3.393 líneas, con GUI y gnuplot dentro) no
necesita lectura completa.

### 4. El artículo propio — la pregunta empírica

`~/Dropbox/Article_Multivariate Convergence/`

```
cointegration_convergence/   Convergence_and_Cointegration_WPICAE1122.pdf
                             convergence_and_cointegration.tex~, appendix/, biblio.bib
Matlab_Simulation/           Main_Program.m, cointegration.pdf, var13.pdf,
                             Convergencia_a_LOP.ods
Eviews_Strg/
```

**Responde:** para qué se quiere el modelo — convergencia y cointegración, con la
convergencia a la ley de un precio único como objeto. `Main_Program.m` y el
material de Eviews dicen **qué se estimó y cómo** antes de que existiera `drvec`,
que es la referencia empírica contra la que comparar.

⚠ Hay ficheros `~` (respaldos de editor) y un `.zip` junto a `appendix/`: la
versión buena es el **PDF del working paper**, y el `.tex` sólo si hace falta el
detalle.

## Orden de lectura

1. **Mauricio 2006 + AddOn.** Sin esto, lo demás no se puede juzgar. Del AddOn
   interesa sobre todo si desarrolla los *Remarks* que cita el comentario de
   `drvec.c`.
2. **El comentario del layout en `drvec.c`**, contra los Remarks 1 y 6. Es el
   contrato, y es donde se ve de un vistazo qué parametriza el modelo.
3. **`drvec.c` completo** (`vec_shootx`, `init_guess`) contra el artículo: qué
   está, qué falta, qué está a medias.
4. **`LEGACY_NOTES.md`**, y sólo entonces `drv_project/src/main.c` en lo que las
   notas no cubran.
5. **El working paper propio** y `Main_Program.m`: la pregunta empírica y el
   contraste de resultados.

## La pregunta de diseño que ya está sobre la mesa

Anotada al cerrar el plan del cast, y este estudio debería contestarla:

> **¿Dónde vive la restricción de rango sobre la matriz de largo plazo?**

El cast de `fue` es univariante y no sabe nada de rango reducido. La restricción
—que es lo que distingue un VEC de un VAR en diferencias— tiene que vivir o en el
cast propio de `drvec` o por encima de él, en el driver. La respuesta condiciona
si `drvec` puede usar el patrón de `drtran` tal cual (`cast_diagonal` /
`cast_embedded`, que reciben el vector y devuelven la estructura VARMA) o
necesita algo más.

La pista está en el layout ya documentado: **Λ es (M×r)**, con `r` explícito. Si
el rango entra por la *forma* de los parámetros y no por una restricción impuesta
sobre ellos, entonces el cast puede seguir siendo una traducción pura y el patrón
de `drtran` vale sin cambios. **Es una hipótesis, no un resultado**: confirmarla o
descartarla es el primer entregable útil del estudio.
