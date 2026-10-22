# `equation` — la ecuación del modelo

Lo que el analista lee para saber qué está estimando. Sale del GUI de fue, y
ahora también del de drtran.

## Qué se movió aquí, y por qué sólo eso

| fichero | de dónde | acoplamiento con fue |
|---|---|---|
| `equation.h` | `engines/fue/include/` | — el vocabulario |
| `eqlatex.c` | `engines/fue/src/` | **cero** referencias a globales de fue |
| `eqtran.h/.c` | nuevo | — |
| `eqdraw.h/.c` | `engines/fue/src/report.c` | **cero**: la página entera |

`equation.c` **no** se movió: tiene 45 referencias a los globales del motor.
Es el *constructor* de la ecuación univariante y está donde tiene que estar.
Lo que sube a `lib/` es el **vocabulario** (`EqItem`) y los *escritores*, que
son los que se reutilizan.

## Un output es el univariante con términos delante

    Y_t  =  Σ_j [ w_j(B) / d_j(B) ] B^bj X_j,t  +  N_t

`N_t` es **exactamente** el ruido que fue ya sabe escribir, y no por analogía:
`struct Tusmodel` tiene los **mismos 19 campos con los mismos nombres** en fue
y en drtran. Por eso `eqtran_texto` recibe una `Equation` ya construida y sólo
le pone delante la transferencia.

## El convenio de signo

    w(B) = w0 − w1·B − … − ws·B^s     el primero SUMA, el resto RESTAN
    d(B) = 1  − d1·B − … − dr·B^r

Es Box-Jenkins, y es el de toda la escuela. Verificado en los tres peldaños:
`fue::calcnu`, `drtran/nlatools.c:728`, `cast.py:87`.

## La prueba no puede pasar por casualidad

`test_eqtran.c` no comprueba que la ecuación *se lea bien* — comprueba que el
número que se deduce de ella es el que imprime el motor. Con los ω del m6:

    ganancia BJ (w0 − w1 − w2) : 0.731347
    convenio ingenuo (la suma) : −0.009923
    lo que imprime drtran      : 0.731347

y además exige que el convenio ingenuo **no** dé 0.731347: si un día los dos
coincidieran, la prueba no estaría distinguiendo nada y lo dice.

    gcc -Ilib/equation lib/equation/test_eqtran.c lib/equation/eqtran.c -lm

## Salida

    EP_t  = [0.4495 - 0.0000B]B EI + [0.3607 + 0.5294B - 0.1588B^2]B EC  +  N_t

El `- 0.0000B` no es un hueco: en el formato, un coeficiente escrito con
bandera de FIJO **es especificación**, y se ve.

## La página del modelo: una sola, la de fue en LaTeX

`eqdraw.c` dibuja la página que enseña «Ver modelo…»: el gráfico de los
residuos, la ecuación y la tabla de los residuos de más de tres desviaciones.
La escriben **los dos**, con el mismo código:

- fue (`engines/fue/src/report.c`), para el modelo univariante;
- drtran_gui (`gui/drtran/src/pagina.c`), una por ecuación del caso.

El modelo de transferencia se escribe **exactamente** como el univariante; lo
único que se añade es la parte `EQ_TRANS`, en la primera línea y **delante de
los deterministas**. Cada término se escribe como fue escribe una variable
determinista con su ω(B)/δ(B), con la entrada (`EI_INPUT`, ln X_t) en el sitio
de ξ.

La composición es la de `fue -latex` (eqlatex.c compilado con pdflatex), medida
sobre su PDF: A4 apaisado con márgenes de 2.5 cm, `\footnotesize` de un
artículo de 12pt con `\linespread{1.6}`, los espacios de TeX entre átomos
(fino, medio, grueso), `\est` tal como lo define eqlatex.c, los saltos de
línea de TeX (después de un operador binario o de una relación) y la tabla de
`_res.tex`. Las posiciones caen a menos de medio punto de las de pdflatex. Lo
que no puede ser igual es **la letra**: fugdraw sólo usa las fuentes estándar
de PDF, sin incrustar ninguna, así que Computer Modern se sustituye por
Helvetica (`\mathsf`), Times (la redonda), Times Italic (la cursiva
matemática) y Symbol inclinada (las griegas), cada una en el avance del
glifo de Computer Modern al que sustituye.

`test_eqdraw.c` exige que, con un término de transferencia, el ruido salga
igual byte a byte y que la transferencia vaya delante de los deterministas:

    cc -Ilib/equation -Ilib/fugdraw lib/equation/test_eqdraw.c \
       lib/equation/eqdraw.c lib/fugdraw/fugdraw.c -lm
