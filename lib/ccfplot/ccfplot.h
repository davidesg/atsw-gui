/*
 * ccfplot.h -- la CCF BIDIRECCIONAL, el grafico de identificacion de la
 * funcion de transferencia.
 *
 * EL FORMATO NO ES UNA ELECCION: viene de dos prototipos aprobados por
 * Treadway que coinciden entre si --el de GraphMaker
 * (Projects/graphmakertri2/ccfgrafico.cpp) y el de drvus
 * (drv4.040804/drvus/ccf2_1.eps, con su ccf.c)-- y esta funcion los reproduce.
 *
 * QUE ES.  Un panel ancho y bajo con los retardos de -k a +k, simetrico
 * alrededor de cero. Los dos lados dicen cosas distintas y es TODO el interes
 * del grafico:
 *
 *     k > 0   la influencia de la ENTRADA sobre la SALIDA: la transferencia
 *     k < 0   la influencia de la salida sobre la entrada: si ahi hay algo
 *             que no sea ruido, la entrada no es exogena y el modelo de
 *             transferencia no se sostiene
 *
 * Por eso el titulo pone la SEGUNDA serie primero: "entrada - salida". Es el
 * convenio de GraphMaker y esta razonado en su codigo: en los retardos
 * positivos se representa la influencia de la segunda sobre la primera.
 *
 * EL ESTADISTICO es el portmanteau multivariante de HOSKING (1980),
 *
 *     Q = N * SUM_k  tr( C_k' C_0^-1 C_k C_0^-1 )
 *
 * que drtran ya tiene implementado (diagnose.c: hosking_test). Sus grados de
 * libertad son m^2 (k - p - q), y con m = 2 eso es 4(k - p - q).
 *
 * GraphMaker lo etiqueta P y no Q, y deja dicho por que: "la llamamos P para
 * no confundirlo con el Q de Ljung-Box". El prototipo de drvus lo etiqueta Q.
 * Se sigue a GraphMaker, que es el que da la razon.
 */

#ifndef ATSW_CCFPLOT_H
#define ATSW_CCFPLOT_H

#include "fugdraw.h"

/* Cuantos retardos, segun la frecuencia -- los de GraphMaker: 7 al anio, 15
 * al trimestre, 12 al mes (ahi el usuario puede pedir entre 8 y 39).      */
int ccf_lags_por_defecto( int freq );

/* Dibuja el panel en f, con su esquina inferior izquierda en (x, y) y el
 * tamano dado.
 *
 *   corr[0 .. 2*lags]   las correlaciones, de -lags a +lags; corr[lags] es
 *                       la contemporanea
 *   nobs                para la banda +-2/sqrt(N)
 *   entrada, salida     los nombres; el titulo sale "entrada - salida"
 *   q, df               el estadistico de Hosking y sus grados de libertad.
 *                       df < 0 lo omite.
 */
void ccf_draw( FDFig *f, double x, double y, double w, double h,
               const double *corr, int lags, int nobs,
               const char *entrada, const char *salida,
               double q, int df );

/* El panel entero en su propio fichero EPS, con el tamano del prototipo
 * aprobado (360 x 126 puntos). 0 si fue bien.                            */
int ccf_write_eps( const char *filename,
                   const double *corr, int lags, int nobs,
                   const char *entrada, const char *salida,
                   double q, int df );

#endif /* ATSW_CCFPLOT_H */
