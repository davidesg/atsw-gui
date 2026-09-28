/*****************************************************************************/
/*  main.h -- part of drtran (Box-Jenkins transfer function models).
 *
 *  Exact VARMA likelihood engine, from drvarma/ART.
 *
 *  Copyright (C) 1995-2026 A.B. Treadway, J.A. Mauricio & D.E. Guerrero.
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.  Distributed WITHOUT ANY WARRANTY; see the
 *  GNU General Public License (file COPYING) for details.
 *****************************************************************************/

/***************************************************************************/
/*  MAIN.H                                                                 */
/*  Header file for maximum-likelihood estimation modules.                 */
/*  Copyright (C) Jose Alberto Mauricio, 1995.                             */
/*  Revised for DRTRAN (2025) – transfer function models.                  */
/***************************************************************************/
#ifndef MAIN_H
#define MAIN_H

#ifdef __APPLE__
#include <stdlib.h>
#elif defined(_WIN32)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#define TRUE    1
#define FALSE   0
#define MAXSTR 200                /* increased for FUE .pre files */
#define OK      1
#define WRONG   0
#define FREE_ARG char*

/* Mathematical constant PI if not defined */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef char * STRING;
typedef double real;

/* -------------------------------------------------------------------------- */
/* Estructura para modelo VARMA (normalizado) – común con drvarma            */
/* -------------------------------------------------------------------------- */
struct Tvarma
{
    int  m; int n; int p; int q;
    real *mu; real ***phi; real ***theta;
    real **qq; real **w; real **a;
    real sigma2; real logelf; real xitol;
};

/* -------------------------------------------------------------------------- */
/* Estructura para series temporales univariantes (usada en diagnósticos)     */
/* -------------------------------------------------------------------------- */
struct Tseries
{
    char *name;                  /* Nombre de la serie */
    int  nobs;                   /* Numero de observaciones */
    int  freq;                   /* Frecuencia (observaciones por anio) */
    int  begtime;                /* Periodo inicial */
    int  begyear;                /* Anio inicial */
    int  endtime;                /* Periodo final */
    int  endyear;                /* Anio final */
    int  tmornsop;               /* tiempo muerto (no usado) */
    real mean;                   /* Media muestral */
    real var;                    /* Varianza muestral */
    real skew;                   /* Coeficiente de asimetria */
    real kurt;                   /* Coeficiente de curtosis */
    int  max;                    /* Indice del maximo */
    int  min;                    /* Indice del minimo */
    real *data;                  /* Vector de datos */
    int  numbering;              /* 1 = numeracion simple, 0 = fechas */
    real refactor;               /* factor de reescalado (se añade para FUE) */
};

/* struct Tusmodel --el modelo univariante de un .pre-- vive en lib/fuepre:
   lo comparten drtran, su GUI y drvarma, y es el lector quien lo llena. */
#include "tusmodel.h"

/*****************************************************************************/
/*  Funciones de estimacion (de elfvarma.c y drvmlest.c)                     */
/*****************************************************************************/

/* Which Hessian gave the standard errors (est_se_how, set by est):         */
#define EST_SE_BFGS      0    /* the one BFGS accumulated                      */
#define EST_SE_FDHESS    1    /* fdhess at the optimum                         */
#define EST_SE_BOUNDARY  2    /* BFGS kept: the optimum is on the boundary     */
#define EST_SE_NOTPD     3    /* BFGS kept: fdhess not positive definite       */
#define EST_SE_NONE_BOUNDARY 4 /* neither: boundary, and BFGS never built       */
#define EST_SE_NONE_NOTPD    5 /* neither: not PD, and BFGS never built         */
extern int est_fixed, est_se_how;
const char *est_se_label( int how );

void est( void (*cast)( real *, struct Tvarma *, int *, int, int ),
          int npar, real *par, real *dev, real **cov, int maxits, int nrits,
          real grtol, real sptol, real xitol, real **a, real *sigma2,
          real *logelf, int *ifault );

extern int  est_ma_boundary, est_ma_nroots;   /* drvmlest.c: MA roots at the stop */
#include "lik.h"            /* lib/lik: Shea (AS 242) beside elf, -l */

void elf( int m, int n, int p, int q, real *mu, real ***phi, real ***theta,
          real **qq, real **w, real sigma2, real delta, int atf, real **a,
          real *f1, real *f2, real *logelf, int *ifault );

void chekma( int m, int q, real ***theta, real *wr, real *wi,
             real *wmod, int *ifault );

/*****************************************************************************/
/*  Funciones de algebra lineal y gestion de memoria (de nlatools.c)         */
/*****************************************************************************/

void ludcp( real **a, int n, int *ip );
void lusol( real **a, real *b, int n, int *ip );
void choldcp( real **mat, int n, real *d1, real *d2, int *ifault );
void cholfor( real **matl, int n, real *rhsol );
void cholbak( real **matl, int n, real *rhsol );
void cholsol( real **matl, int n, real *rhsol );
/* eigenql/balanc/elmhes/tred2 eran los internos de la eigenqr de Numerical
   Recipes; la version libre de NR resuelve con GSL y no los necesita. */
void eigenqr( real **a, int n, real *wr, real *wi );
void svdcp( real **a, int m, int n, real *w, real **v );
void svsol( real **u, real *w, real **v, int n, real *x );

void nrerror( char error_text[] );
real *vector( long nl, long nh );
int  *ivector( long nl, long nh );
real **matrix( long nrl, long nrh, long ncl, long nch );
int  **imatrix( long nrl, long nrh, long ncl, long nch );
real ***tensor( long nrl, long nrh, long ncl, long nch, long ndl, long ndh );
void free_vector( real *v, long nl, long nh );
void free_ivector( int *v, long nl, long nh );
void free_matrix( real **m, long nrl, long nrh, long ncl, long nch );
void free_imatrix( int **m, long nrl, long nrh, long ncl, long nch );
void free_tensor( real ***t, long nrl, long nrh, long ncl, long nch,
                  long ndl, long ndh );

/*---------------------------------------------------------------------------*/
/*  Funciones auxiliares matriciales                                         */
/*---------------------------------------------------------------------------*/
 void matrix_multiply(real **A, real **B, real **C, int n, int m, int p);
 void matrix_transpose(real **A, real **B, int n, int m);
 void matrix_inverse(real **A, real **invA, int n);

real rmax( real a, real b );
real rmin( real a, real b );
real cmacheps( void );
int  round_local( real num );

/*****************************************************************************/
/*  Funciones de manejo de cadenas (de nlatools.c)                           */
/*****************************************************************************/

STRING NEW_STR( int size );
void   FREE_STR( STRING s );
int    COPY_STR( STRING source, int i, int n, STRING dest );

/*****************************************************************************/
/*  Funciones de analisis de series (diagnose.c)                             */
/*****************************************************************************/

void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                int *per, int *sub );
void Easter( int *day, int *month, int year );
real Mean( real *data, int nobs );
real Stdev( real *data, int nobs );

/* Correlacion cruzada (diagnose.c): corr[j] = corr( data1_t , data2_{t+j-1} ),
   j = 1..lags+1, es decir el retardo k = j-1 >= 0. Para el lado k < 0 se llama
   con los argumentos intercambiados.                                          */
void Ccf( real *data1, real *data2, int nobs, int lags, real *corr,
          real mean1, real mean2, real sd1, real sd2 );
real Stdev( real *data, int nobs );
int  MaxVal( real *data, int nobs );
int  MinVal( real *data, int nobs );
real Skew( real *data, int nobs );
real Kurt( real *data, int nobs );
void Acf(real *data, int nobs, int lags, real *corr, real mean, real var);
void Pacf( int lags, real *pcorr );
real ChiTest( real *corr, int lags, int nobs );

void File_StatSer( struct Tseries *ser );
void File_PlotSer( struct Tseries *ser );
void File_HistSer( struct Tseries *ser );
void File_CorrSer( struct Tseries *ser, int npar );
void PlotCor( real *corr, int lags, int isacf, struct Tseries *ser, int npar );
void PlotCCF( real *corr, int lags, struct Tseries *ser );
void ccf( struct Tseries *ser1, struct Tseries *ser2, int lags, real *corr );
real ChiTestC( real *corr, int lags, int nobs );
void hosking_test(real **res, int nobs, int m, int s, real *Q, real *pval);
void jarque_bera_multivariate(real **res, int nobs, int m, real *JB, real *pval);
void multivariate_diagnostics(real **res, int nobs, int m, FILE *outputv);
void diagnose(struct Tvarma *varma, int freq);

void test_last_lag_significance(real *x, real **cov, int npar,
                                int m, int p, int q,
                                int include_mean,
                                int diag_ar, int diag_ma, int diag_cov,
                                real *chi2, int *df, real *pval);
void test_last_ar_significance(real *x, real **cov, int npar,
                               int m, int p, int q,
                               int include_mean,
                               int diag_ar, int diag_ma, int diag_cov,
                               real *chi2, int *df, real *pval);
void test_last_ma_significance(real *x, real **cov, int npar,
                               int m, int p, int q,
                               int include_mean,
                               int diag_ar, int diag_ma, int diag_cov,
                               real *chi2, int *df, real *pval);
void test_cross_effects_significance(real *x, real **cov, int npar,
                                     int m, int p, int q,
                                     int include_mean,
                                     int diag_ar, int diag_ma, int diag_cov,
                                     real *chi2, int *df, real *pval);
void all_hypothesis_tests(real *x, real **cov, int npar,
                          int m, int p, int q,
                          int include_mean,
                          int diag_ar, int diag_ma, int diag_cov,
                          FILE *outputv);

void impulse_response(struct Tvarma *varma, int horizon, FILE *outputv);
void variance_decomposition(struct Tvarma *varma, int horizon, FILE *outputv);

void AnalyzeOneSeries(real *res, int nobs, int idx, int freq);

/*****************************************************************************/
/*  Funciones de distribucion estadistica (de nlatools.c)                    */
/*****************************************************************************/

real chisq( real x, int df );          /* Distribucion Chi-cuadrado acumulada */
real tdist( real t, int df );          /* Distribucion t-Student acumulada   */
real normal_cdf( real z );             /* Distribucion normal estandar acumulada */

/*****************************************************************************/
/*  Funcion auxiliar para la respuesta al impulso de un filtro racional      */
/*  (antes solo para un parametro, ahora tambien para vectores)              */
/*****************************************************************************/

void calcnu( double omega, int s, double delta, int r, double *nu, int lags );
/* version para vectores (omega[], delta[]) */
void calcnu_vec( double *omega, int s, double *delta, int r, double *nu, int lags );

/*****************************************************************************/
/*  Funciones propias de drtran (nuevas)                                     */
/*****************************************************************************/

/* Lector de archivos .pre de FUE */
int read_fue_pre( const char *filename,
                  struct Tusmodel *Tm,
                  struct Tseries *Ts,
                  real ***DataMat );

/* Aplica las transformaciones de un modelo univariante (Box-Cox, deterministas,
   diferencias) y devuelve la serie estacionaria w */
void apply_univariate_model( struct Tusmodel *Tm, struct Tseries *Ts,
                             real **DataMat, real **w_out, int *nstat_out );

/* Preblanqueo y sugerencia de ordenes de la funcion de transferencia */
void prewhiten_and_identify( int j, FILE *outputv );

/* Funcion shootx para el modelo de transferencia (cumple con la firma de cast) */
void shootx_tran( real *x, struct Tvarma *armax, int *ifaultx, int firstx, int lastx );

/*****************************************************************************/
/*  Funciones de volatilidad (heredadas de drvarma)                          */
/*****************************************************************************/

/*void compute_exponential_volatility(struct Tvarma *varma, real alpha,
                                    int window, const char *basename);
void compute_moving_window_volatility(struct Tvarma *varma, int window,
                                      const char *basename);
*/

#endif /* MAIN_H */
