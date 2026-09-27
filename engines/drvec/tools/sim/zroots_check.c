/*  zroots_check.c — las raices del operador, contra la rutina de la casa.
 *
 *  drvec calcula los modulos de las raices de |Phi(x)| = 0 y |Theta(x)| = 0 por
 *  los autovalores de la matriz COMPANERA (eigenqr), que es lo que corresponde a
 *  un polinomio MATRICIAL.  La suite tiene ademas un buscador de raices de
 *  polinomios ESCALARES por el metodo de Laguerre -- zroots, en
 *  ~/Dropbox/SRC/Root/Root-1.01/root.c, y la misma rutina byte a byte en
 *  ART_18.1/src/root.c --, y con M = 2 las dos se pueden comparar: el polinomio
 *  |I - Theta x| = 1 - tr(Theta) x + det(Theta) x^2 es escalar.
 *
 *  Que las dos coincidan importa mas de lo normal aqui, porque el modulo de la
 *  raiz MA es el numero del que depende todo el diagnostico de admisibilidad
 *  (docs/THEORY.md, corolario 3.1).
 *
 *      gcc -O2 -c -Dmain=root_main_unused -o /tmp/rootlib.o \
 *          ~/Dropbox/SRC/Root/Root-1.01/root.c
 *      gcc -O2 -o /tmp/zdrv tools/sim/zroots_check.c /tmp/rootlib.o -lm
 *      /tmp/zdrv <c1> <c2> ...        para  P(B) = 1 - c1 B - c2 B^2 - ...
 *
 *  Medido sobre el ajuste libre de Milan, Theta1 = [[-0.507884, 0.211529],
 *  [-0.753138, 1.105408]], o sea c1 = 0.597524 y c2 = 0.402109:
 *
 *      drvec (companera + eigenqr)   1.00026    2.48624
 *      zroots (Laguerre)             1.000262   2.486237
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#define MP 20
#define MP1 (MP + 1)
typedef struct { double r, i; } Complex;
typedef Complex ComplexArrayMp1[MP1];
void zroots(ComplexArrayMp1 a, int m, ComplexArrayMp1 roots, bool polish);
int main(int argc, char **argv)
{
    ComplexArrayMp1 a, r;
    int m = argc - 1, j;
    for (j = 0; j <= MP; j++) { a[j].r = 0.0; a[j].i = 0.0; }
    a[0].r = 1.0;                                  /* el 1 de P(B) */
    for (j = 1; j <= m; j++) { a[j].r = -atof(argv[j]); a[j].i = 0.0; }
    zroots(a, m, r, true);
    for (j = 1; j <= m; j++)
        printf("  raiz %d: %12.6f %+12.6fi   modulo %12.6f\n",
               j, r[j].r, r[j].i, sqrt(r[j].r*r[j].r + r[j].i*r[j].i));
    return 0;
}
