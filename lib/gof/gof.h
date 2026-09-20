/*
 * gof.h -- la bondad del ajuste, en la forma de la escuela.
 *
 * LAS TRES CIFRAS CON QUE SE CIERRA CADA CASO, y no una:
 *
 *   "La desviacion tipica residual estimada pasa de 0.53 % en el modelo
 *    univariante a 0.42 % en el Modelo rpu6.3. El R² en el modelo univariante
 *    de ru es 0.54, mientras que, en el Modelo rpu6.3, es 0.71."
 *                                                          (Brajin 2004, 6.4)
 *
 * y la tercera, que es la que dice si esa mejora se gana su sitio, el contraste
 * de razon de verosimilitudes contra el modelo DIAGONAL, que esta anidado.
 *
 * EL R² SI TIENE SENTIDO AQUI, Y ES ESTE (Brajin A.28):
 *
 *     R² = 1 - SUM (a_t - abar)² / SUM (w_t - wbar)²,   w_t = nabla^d nabla_s^D z_t
 *
 * Lo que NO tiene sentido es un R² sobre el NIVEL de una I(1): sale cerca de 1
 * por construccion y no dice nada. Sobre la serie ESTACIONARIA si lo dice, y es
 * lo que la escuela ha usado siempre.
 *
 * LO QUE HACE COMPARABLES LOS DOS R² ES QUE EL DENOMINADOR NO LLEVA PARAMETROS.
 * w_t es propiedad de los DATOS una vez fijados lambda, d y D: es identico en
 * las dos estimaciones y solo se mueve el residuo. Sacarlo en cambio de la W
 * del cast --que resta la parte determinista y por tanto depende de lo
 * estimado-- es un error que se esconde a si mismo: drtran-python lo
 * documenta, el R² BAJABA al añadir la transferencia mientras la desviacion
 * tipica residual bajaba tambien. Dos cifras del mismo ajuste apuntando en
 * sentidos opuestos es la señal de que el denominador se movio.
 *
 * Y DEJA DE SER COMPARABLE ENTRE d DISTINTAS, donde w_t es otra variable. Por
 * eso esto es una TRANSICION entre dos ajustes de UNA especificacion, nunca
 * una nota con la que ordenar modelos.
 *
 * Vive en lib/ y no dentro de la pantalla porque asi se puede contrastar
 * contra numeros conocidos sin levantar un GTK.
 */

#ifndef ATSW_GOF_H
#define ATSW_GOF_H

#include "main.h"          /* struct Tseries, struct Tusmodel, real */

/* w_t: el operador no estacionario aplicado a la serie transformada.
 *
 * Se usa rnsop --el polinomio que el .pre trae ya armado-- y no nrdiff/nadiff,
 * que NO son canonicos: en el m6 las seis series declaran nrdiff = 2 y EA es
 * en realidad (1-B)(1-B^4). El signo global de rnsop da igual: lo que se mide
 * despues es una varianza.
 *
 * Devuelve un vector nuevo de *n elementos (free() al acabar), o NULL si la
 * serie no da para tanto o la transformacion no se puede aplicar.        */
double *gof_estacionaria( const struct Tseries *ts, const struct Tusmodel *tm,
                          int *n );

/* SUM (x - xbar)² sobre las ULTIMAS n de las total observaciones.
 *
 * SOBRE LAS MISMAS OBSERVACIONES QUE LOS RESIDUOS, y no es un detalle: el
 * motor estima sobre la ventana COMUN, que fija el operador mas largo de todas
 * las series. En el m6, EA gasta 5 y las demas 2, asi que hay 64 residuos
 * aunque w_t de las otras cinco tenga 67 valores. Dividir 64 residuos por la
 * varianza de 67 w_t seria comparar cosas de distinto tamaño. Se recorta por
 * el FINAL, que es donde coinciden.                                      */
double gof_suma_cuad_cola( const double *x, int total, int n );

/* El R² de Brajin (A.28) y la desviacion tipica residual de una ecuacion.
 *
 * a[0..na-1] son los residuos; sw es SUM (w - wbar)², que viene de fuera
 * porque es COMUN a los dos ajustes -- es justo lo que los hace comparables.
 * Devuelve 0 si pudo.                                                    */
int gof_r2_brajin( const double *a, int na, double sw, double *r2, double *dt );

/* La reduccion de varianza residual, en tanto por uno: la cifra con la que la
 * escuela lo dice en voz alta ("una reduccion del 44 % en relacion a su modelo
 * univariante"). Dice lo mismo que el LR en las unidades del analista.
 * Negativa si empeora. NAN si alguna no esta.                            */
double gof_reduccion( double dt_base, double dt_nuevo );

#endif /* ATSW_GOF_H */
