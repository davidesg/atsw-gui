 /***************************************************************************
 *   Copyright (C) 2009-2026 by Arthur B. Treadway & David E. Guerrero     *
 *   abtreadway@telefonica.net                                             *
 *   davidesg@ucm.es                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef FUG_H
#define FUG_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#define FUG_VERSION "1.15"

#define TRUE    1
#define FALSE   0
#define OK      1
#define WRONG   0
#define MAXSTR 90
#define FREE_ARG char*


/*****************************************************************************/

typedef char * STRING;
typedef double real;

/*****************************************************************************/



struct Tseries                   /* Standard single time series structure:   */
    {
    STRING name;                  /* Time series name (string).               */
    int  nobs;                   /* Number of observations.                  */
    int  freq;                   /* Frequency (observations per year).       */
    int  begtime;                /* Beginning period.                        */
    int  begyear;                /* Beginning year.                          */
    int  endtime;                /* Ending period.                           */
    int  endyear;                /* Ending year.                             */
    int  outyear;                /* year before graph (only yearly data)     */
    double mean;                   /* Sample mean.                             */
    double var;                    /* Sample variance.                         */
    double skew;                   /* Skewness coefficient.                    */
    double kurt;                   /* Kurtosis coefficient.                    */
    double jarquebera;             /* Jarque Bera test statistic               */
    int  max;                    /* Maximum value.                           */
    int  min;                    /* Minimum value.                           */
    double *data;                  /* Vector of time series observations.      */
    };

struct Tusmodel          /* Seasonal US model with deterministic components: */
    {
    char *name;          /* Model name (string).                             */
    real boxlam;         /* Box-Cox parameter lambda.                        */
    real boxm;           /* Box-Cox parameter m (data + m must be > 0).      */
    int boxgeom;         /* Box-Cox with geometric mean normalization (0/1). */
    int  sper;           /* Seasonal period: either 1(A), 4(Q) or 12(M).     */
    int  nrdiff;         /* Number of regular differences.                   */
    int  nadiff;         /* Number of (complete) annual differences.         */
    int  *ifadf;         /* Individual factors of the annual difference.     */
    int  ornsop;         /* Order of the resulting non-stationary operator.  */
    real *rnsop;         /* Resulting non-stationary operator.               */
    real cbands;
    real mu;             /* Mean parameter.                                  */
    int  Imu;            /* Flag (0-1): estimation of mu.                    */
    real sigma2;         /* Estimated residual variance.                     */

 /* Section [1]: Deterministic variables (intervention + seasonal):          */

    int  NdetVar;        /* No. of deterministic variables (detvars).        */
    int  *Nomega;        /* Order of omega(B) for each detvar (order s).     */
    real **Omega;        /* Matrix of omegas: one row for each detvar.       */
    int  **Imega;        /* Matrix of flags (0-1): omegas to be estimated.   */
    int  *Ndelta;        /* Order of delta(B) for each detvar (order r).     */
    real **Delta;        /* Matrix of deltas: one row for each detvar.       */
    int  **Ielta;        /* Matrix of flags (0-1): deltas to be estimated.   */

 /* Section [2]: Standard regular-annual AR and MA factors:                  */

    int  NumAr1;         /* No. of regular AR factors.                       */
    int  *p1;            /* No. of phis for each factor (AR order).          */
    real **Ar1;          /* Matrix of phis: one row for each factor.         */
    int  **Ia1;          /* Matrix of flags (0-1): phis to be estimated.     */

    int  NumAr2;         /* No. of annual AR factors.                        */
    int  *p2;            /* No. of PHIS for each factor (AR order).          */
    real **Ar2;          /* Matrix of PHIS: one row for each factor.         */
    int  **Ia2;          /* Matrix of flags (0-1): PHIS to be estimated.     */

    int  NumMa1;         /* No. of regular MA factors.                       */
    int  *q1;            /* No. of thetas for each factor (MA order).        */
    real **Ma1;          /* Matrix of thetas: one row for each factor.       */
    int  **Im1;          /* Matrix of flags (0-1): thetas to be estimated.   */

    int  NumMa2;         /* No. of annual MA factors.                        */
    int  *q2;            /* No. of THETAS for each factor (MA order).        */
    real **Ma2;          /* Matrix of THETAS: one row for each factor.       */
    int  **Im2;          /* Matrix of flags (0-1): THETAS to be estimated.   */

 /* Section [3]: AR and MA factors of order 2 with fixed frequency:          */

    int  NumAr1f;        /* No. of regular AR factors (fixed frequency).     */
    real *pfre1;         /* Frequency for each factor.                       */
    real **Ar1f;         /* Matrix of phis: one row for each factor.         */
    int  *Ia1f;          /* Vector of flags (0-1): phis to be estimated.     */

    int  NumAr2f;        /* No. of annual AR factors (fixed frequency).      */
    real *pfre2;         /* Frequency for each factor.                       */
    real **Ar2f;         /* Matrix of PHIS: one row for each factor.         */
    int  *Ia2f;          /* Vector of flags (0-1): PHIS to be estimated.     */

    int  NumMa1f;        /* No. of regular MA factors (fixed frequency).     */
    real *qfre1;         /* Frequency for each factor.                       */
    real **Ma1f;         /* Matrix of thetas: one row for each factor.       */
    int  *Im1f;          /* Vector of flags (0-1): thetas to be estimated.   */

    int  NumMa2f;        /* No. of annual MA factors (fixed frequency).      */
    real *qfre2;         /* Frequency for each factor.                       */
    real **Ma2f;         /* Matrix of THETAS: one row for each factor.       */
    int  *Im2f;          /* Vector of flags (0-1): THETAS to be estimated.   */


/* DG 06/21/04 ***************************************************************/

 /* Section [4]: AR and MA seasonal factors of order S-1 :                   */

    int  NumAr1S;        /* No. of seasonal AR factors.                      */
    real *pSre1;         /* Frequency for each factor.                       */
    real **Ar1S;         /* Matrix of phis: one row for each factor.         */
    int  *Ia1S;          /* Vector of flags (0-1): thetas to be estimated.   */

    int  NumMa1S;        /* No. of seasonal MA factors.                      */
    real *qSre1;         /* Frequency for each factor.                       */
    real **Ma1S;         /* Matrix of thetas: one row for each factor.       */
    int  *Im1S;          /* Vector of flags (0-1): thetas to be estimated.   */

/* END DG ********************************************************************/
    };
void DiffGraph ( struct Tseries *ser, int nparma, double boxlam, double boxm, int nrdiff, int nadiff, int *ifadf, int lags, double cbands, int nog, char *outx11 );
/*****************************************************************************/


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

real rmax( real a, real b );
real rmin( real a, real b );
real cmacheps( void );
int iround( real num );
/*****************************************************************************/

STRING NEW_STR( int size );
void FREE_STR( STRING s );
int DELETE_STR( STRING s, int i, int n );
int COPY_STR( STRING source, int i, int n, STRING dest );
int INSERT_STR( STRING s1, STRING s, int i );
int POS_STR( STRING s1, STRING s2 );
int CHANGE_STR( STRING s1, int i, STRING s2 );
void BLANKS_STR( STRING s );
void UPCASE_STR( STRING s );

void Easter( int *day, int *month, int year );

/*****************************************************************************/
void choleskyg ( double **a, int n, double p[] );
void choleskyinverseg ( double **a, double *p, double **h, int n );
void productmatrixg ( double **a, double **b, double **t, int n, int m, int p, int q );
void transposeg ( double **a, double **at, int N, int M );

/****************************************************************************/
void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                int *per, int *sub );
void DateToObs( int beg_per, int beg_sub, int per, int sub, int freq,
                int *obs_no );
real Mean( real *data, int nobs );
real Stdev( real *data, int nobs );
int  MaxVal( real *data, int nobs );
int  MinVal( real *data, int nobs );
real Skew( real *data, int nobs );
real Kurt( real *data, int nobs );
real JarqueBera( real Skew, real Kurt, int nobs);
void Acf( struct Tseries *ser, int lags, real *corr );
void Pacf( int lags, real *pcorr );
real ChiTest( real *corr, int lags, int nobs );
int  default_lags( int nobs, int freq );
int  meandv( double *y, double *dts, double *ms, int nog, int ng );
int  annual_step( int years, double width );
int  first_multiple( int year, int step );

void File_StatSer( struct Tseries *ser );
void File_PlotSer( struct Tseries *ser );
void  DesvMed ( struct Tseries *ser, int nog );
void File_HistSer( struct Tseries *ser );
void File_CorrSer( struct Tseries *ser, int npar, int lags );

/*****************************************************************************/
void BoxCox  ( double *DataInput, double *DataOutput, double boxlam, double boxm, int nobs, double refactor, int geometric);
void interface_m_dt ( struct Tseries *ser, int nog, double boxlam, double boxm,  char *outx11 );
STRING file_plot ( int nrdiff, int nadiff, double boxlam, int freq, char *outx11 );
int  ifadf_order ( int freq, int *ifadf );

/*****************************************************************************/

double estimate_lambda_mle(struct Tseries *ser, double boxm, double lambda_min, double lambda_max, double step, int use_geometric);
double boxcox_log_likelihood(double *data, int nobs, double boxm, double lambda, int use_geometric);

#endif
