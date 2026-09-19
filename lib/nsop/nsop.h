/*
 * nsop.h -- el operador no estacionario, en forma canonica.
 *
 * Un .pre trae el operador de dos maneras a la vez: los enteros nrdiff/nadiff/
 * ifadf, y el polinomio expandido rnsop. LOS ENTEROS NO SON CANONICOS: la
 * escuela escribe grad grad_4 como nrdiff=2 con ifadf={1,2}, que es el MISMO
 * operador que nrdiff=1, nadiff=1.
 *
 * Esta medido en el m6: las SEIS series traen nrdiff = 2. Cinco son (1-B)^2 y
 * la sexta, EA, es (1-B)(1-B^4). Quien lea nrdiff dira que las seis son
 * iguales, y EA es justo la que esta ANIDADA con las otras cinco.
 *
 * Asi que aqui se lee EL POLINOMIO y se factoriza:
 *
 *     1. fuera (1-B^s) mientras se pueda      -> D   (el maximo)
 *     2. el resto entre (1-B) mientras se pueda -> d
 *     3. lo que sobre, entre los factores irreducibles de (1-B^s) -> f
 *
 * que es la forma en que la escuela lo escribe, grad^d grad_s^D, y tiene la
 * propiedad que hacia falta: DOS ESCRITURAS DEL MISMO OPERADOR DAN LO MISMO.
 *
 * LOS FACTORES IRREDUCIBLES, indexados como los indexa el .pre en ifadf:
 *
 *     f = 0        (1 - B)                        grado 1, frecuencia 0
 *     0 < f < s/2  (1 - 2cos(2*pi*f/s) B + B^2)   grado 2
 *     f = s/2      (1 + B)                        grado 1, frecuencia pi
 *
 * El f = 0 no se lista: ese es d.
 *
 * EL CONVENIO DE SIGNO de rnsop es el del fichero: el operador es
 *
 *     p(B) = SUM_j ( -rnsop[j] ) B^j          con rnsop[0] = -1, o sea p(0)=1
 *
 * que es como lo aplica apply_univariate_model.
 */

#ifndef ATSW_NSOP_H
#define ATSW_NSOP_H

#include <stddef.h>

#define NSOP_MAX_F  16

typedef struct {
   int d;                    /* veces (1-B)                              */
   int D;                    /* veces (1-B^s)                            */
   int f[NSOP_MAX_F];        /* las frecuencias irreducibles que sobran  */
   int nf;
   int resto;                /* grado de lo que no se supo nombrar, 0 si
                                no sobro nada. Si es > 0, d/D/f NO cuentan
                                la historia entera y hay que decirlo.    */
} NsopForm;

/* Factoriza. Devuelve 0 si lo explico todo (resto == 0), 1 si sobro algo.
 * Con ornsop == 0 --el operador es 1-- devuelve todo a cero.             */
int nsop_canon( const double *rnsop, int ornsop, int sper, NsopForm *o );

/* El polinomio escrito a mano: "(1-B)(1-B^4)". En out[size]. */
void nsop_texto( const NsopForm *o, int sper, char *out, size_t size );

/* Lo que pone la columna f: "1,2" o "—". */
void nsop_texto_f( const NsopForm *o, char *out, size_t size );

/* TRUE si la forma canonica NO coincide con lo que dicen los enteros del
 * fichero. Sirve para decirlo donde toca --en el panel-- y no dejar al
 * analista preguntandose por que su nrdiff=2 sale como d=1.              */
int nsop_difiere( const NsopForm *o, int nrdiff, int nadiff );

#endif /* ATSW_NSOP_H */
