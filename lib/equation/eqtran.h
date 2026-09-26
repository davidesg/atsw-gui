/*
 * eqtran.h -- la ecuacion de un modelo de transferencia con UN output.
 *
 * Con un solo output, el modelo de transferencia es el univariante de fue con
 * los terminos de transferencia delante:
 *
 *     Y_t  =  SUM_j [ w_j(B) / d_j(B) ] B^bj X_j,t  +  N_t
 *
 * y N_t es exactamente el ruido que fue ya sabe escribir. Por eso aqui no se
 * reescribe la ecuacion entera: se arma LA PARTE DE TRANSFERENCIA con el
 * mismo vocabulario de EqItem que usa fue, y la del ruido sale de donde ya
 * salia.
 *
 * El convenio de signo es el de Box-Jenkins, y es el de toda la escuela:
 *
 *     w(B) = w0 - w1 B - ... - ws B^s        el primero SUMA, el resto RESTAN
 *     d(B) = 1  - d1 B - ... - dr B^r
 *
 * Esta verificado en los tres peldanos --fue::calcnu, drtran/nlatools.c:728 y
 * cast.py:87 del puerto-- y medido sobre el m6: la ganancia de EP <- EC sale
 * 0.360712 - (-0.529417) - 0.158782 = 0.731347, que es lo que imprime drtran.
 * Con el convenio ingenuo saldria -0.0099.
 */

#ifndef ATSW_EQTRAN_H
#define ATSW_EQTRAN_H

#include <stdio.h>
#include "equation.h"

/* Un enlace, tal como lo describe el .dag y lo estima drtran. */
typedef struct {
   const char   *entrada;      /* nombre de la serie de entrada           */
   int           b;            /* retardo puro: B^b                       */
   int           s;            /* orden del numerador w(B)                */
   int           r;            /* orden del denominador d(B)              */
   const double *omega;        /* w[0..s]                                 */
   const double *delta;        /* d[1..r]  (delta[0] no se usa)           */
   const double *omega_se;     /* errores tipicos, o NULL si no los hay   */
   const double *delta_se;
} EqLink;

/* La parte de transferencia, en items. Devuelve cuantos escribio en item[],
 * que tiene que tener sitio para max items.                              */
int eqtran_items( EqItem *item, int max, const EqLink *lnk, int nlinks );

/* La ecuacion entera en texto plano, para una pantalla o una consola.
 * Si eq es NULL escribe solo la transferencia; si no, escribe
 *
 *     Y_t = <transferencia> + N_t ,   <la ecuacion de N_t>
 *
 * Devuelve cuantas letras habria hecho falta (como snprintf).            */
int eqtran_texto( char *out, size_t size,
                  const char *salida, const EqLink *lnk, int nlinks,
                  const Equation *eq );

/* LA ECUACION EN DOS LINEAS, con la desviacion tipica DEBAJO de cada
 * coeficiente y alineada -- que es como se escribe en el papel y en el
 * grafico, y como se lee.
 *
 * Y tiene una consecuencia que no es de estetica: escrito asi, el signo va
 * DELANTE del numero, de modo que la ganancia es la SUMA DE LO QUE SE VE.
 * Con los valores crudos del .out no lo es --el convenio de Box-Jenkins hace
 * que los retardos resten-- y presentar esa diferencia como si fuera un
 * hecho del modelo confunde: es un artefacto de escribirlos sin su signo.
 *
 * arriba y abajo tienen que tener sitio para size letras cada uno. La
 * alineacion cuenta LETRAS y no bytes, que en UTF-8 no es lo mismo.
 * Devuelve las letras que habria hecho falta (como snprintf).          */
int eq_items_texto_et( char *arriba, char *abajo, size_t size,
                       const EqItem *item, int n );

/* Los items de una parte de la ecuacion de fue, en texto plano. Es lo que
 * usa eqtran_texto para el ruido, y sirve por separado.                  */
int eq_part_texto( char *out, size_t size, const Equation *eq, EqPart p );

#endif /* ATSW_EQTRAN_H */
