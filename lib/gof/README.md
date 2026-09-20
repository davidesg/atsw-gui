# `gof` — la bondad del ajuste, en la forma de la escuela

La escuela cierra cada caso con **tres** cifras, no con una:

> «La desviación típica residual estimada pasa de 0.53 % en el modelo
> univariante a 0.42 % en el Modelo rpu6.3. El **R²** en el modelo univariante
> de `ru` es 0.54, mientras que, en el Modelo rpu6.3, es 0.71.»
> — Brajín (2004), 6.4

y la tercera, el contraste LR contra el modelo diagonal, dice si esa mejora se
gana su sitio. Este módulo calcula las dos primeras. La tercera sale del `logL`
que ya escribe el motor.

## El R² **sí** tiene sentido aquí

Este módulo existe porque la primera versión de la pantalla de Diagnosis
**se negaba a dar un R²**, y el razonamiento estaba mal.

Lo que no tiene sentido es un R² sobre el **nivel** de una I(1): sale cerca de
1 por construcción y no dice nada. El de la escuela no es ése — es el de
Brajín (A.28), definido sobre la serie **estacionaria**:

    R² = 1 − SUM (a_t − ā)² / SUM (w_t − w̄)²,      w_t = ∇ᵈ ∇ₛᴰ z_t

`mtram`, el servidor MCP de `drtran-python`, ya lo daba
(`school.py:r2_brajin`). El lado C no.

## Lo que hace comparables los dos R²: **el denominador no lleva parámetros**

Ésta es toda la sutileza del módulo, y es la que se puede perder sin darse
cuenta.

`w_t` es **propiedad de los datos** una vez fijados λ, d y D. No depende de lo
estimado, así que es **idéntico** en el ajuste diagonal y en el de
transferencia: sólo se mueve el residuo. Por eso los dos R² se pueden poner uno
al lado del otro.

`drtran-python` documenta el error opuesto, y merece leerse porque **se
esconde a sí mismo**: al sacar el denominador de la `W` del cast —que resta la
parte determinista, y por tanto **sí** depende de los parámetros— su varianza
salía 606.75 bajo el ajuste diagonal y 356.47 bajo el conjunto. Con un
denominador que se mueve, el **R² bajaba** al añadir la transferencia mientras
la desviación típica residual **también bajaba**.

> **Dos cifras del mismo ajuste apuntando en sentidos opuestos es la señal.**
> Si el denominador fuera de verdad compartido, eso no puede pasar.

Hay una prueba dedicada a esto (`gui/drtran/tests/test_gof.c`): comprueba que
el denominador sale **idéntico** en las dos corridas.

## Y deja de ser comparable entre `d` distintas

Ahí `w_t` es **otra variable**. Por eso esto se presenta como una **transición
entre dos ajustes de una especificación**, nunca como una nota con la que
ordenar modelos.

## Dos trampas de implementación

**El operador se toma de `rnsop`, no de `nrdiff`/`nadiff`.** Esos enteros **no
son canónicos**: en el m6 las seis series declaran `nrdiff = 2` y EA es en
realidad `(1−B)(1−B⁴)`. `rnsop` es el polinomio ya armado, y es el que el
motor usa. (Su signo global es `−1`; da igual, lo que se mide después es una
varianza.)

**`w_t` se recorta por el final, a las mismas observaciones que los residuos.**
El motor estima sobre la ventana **común**, que fija el operador más largo de
todas las series. En el m6 hay 64 residuos aunque `w_t` de cinco de las seis
series tenga 67 valores — porque EA gasta 5 y las demás 2. Dividir 64 residuos
por la varianza de 67 `w_t` sería comparar cosas de distinto tamaño. Cambia el
número: EP pasa de 0.8643 a 0.8610.

## Lo que hay

```c
double *gof_estacionaria( const struct Tseries *ts,
                          const struct Tusmodel *tm, int *n );
double  gof_suma_cuad_cola( const double *x, int total, int n );
int     gof_r2_brajin( const double *a, int na, double sw,
                       double *r2, double *dt );
double  gof_reduccion( double dt_base, double dt_nuevo );
```

`gof_reduccion` da la reducción de varianza residual en tanto por uno: la cifra
con la que la escuela lo dice en voz alta («una reducción del 44 % en relación
a su modelo univariante», Muñoz 6.4.1). Dice lo mismo que el LR en las unidades
en que piensa el analista.

## Por qué vive en `lib/` y no dentro de la pantalla

Para poder contrastarlo **sin levantar un GTK**. El oráculo de la prueba no es
una constante inventada: son los residuos que `drtran -e` escribe en esa misma
corrida, y las series salen de los `.pre`.

Sobre el m6 el R² sube **donde están las transferencias** y se queda quieto
donde no las hay:

| | R² diagonal | R² con transferencia |
|---|---|---|
| **EP** | 0.7830 | **0.8610** |
| **EI** | 0.7707 | **0.8211** |
| EU | 0.7784 | 0.7743 |
| EC | 0.7633 | 0.7593 |
| EA | 0.5969 | 0.5844 |
| P | 0.3778 | 0.3778 |

EP y EI son las dos que reciben entradas. P no tiene enlaces y no se mueve ni
una milésima — la prueba lo exige.

## El baseline hay que poder producirlo

Comprobar esto destapó que **el modo diagonal era inalcanzable desde la
interfaz**, por dos motivos, y los dos están arreglados:

1. `-0` fijaba `s_ord[j] = −1` sobre la red estrella por defecto, y **el `.dag`
   se leía después** y reponía todos los enlaces. Con `-n`, `-0` se ignoraba
   *en silencio*: misma verosimilitud, mismos residuos, LR = 0. Ahora `-0` gana
   —es un **modo**, no una opción— y lo anuncia.

2. **El diagonal no lleva `.cns`.** Un `.cns` que ata `omega1[0]` ni siquiera
   nombra slots que existan sin enlaces. El baseline es las series con sus
   univariantes del `.pre` y nada más — que es exactamente el ajuste cuya
   verosimilitud debe coincidir con la suma de las de fue.
