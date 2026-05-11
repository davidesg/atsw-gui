 /***************************************************************************
 *   Copyright (C) 2009 by Arthur B. Treadway & David Guerrero             *
 *   abtreadway@telefonica.net                                             *
 *   warriord@rocketmail.com                                               *
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
#ifndef NLUTILS_H
#define NLUTILS_H

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
//#include <gtk/gtk.h>



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
    const char *name;                  /* Time series name (string).               */
    int  nobs;                   /* Number of observations.                  */
    int  freq;                   /* Frequency (observations per year).       */
    int  numbering;              /* only numbering option (binary)           */
    int  begtime;                /* Beginning period.                        */
    int  begyear;                /* Beginning year.                          */
    int  endtime;                /* Ending period.                           */
    int  endyear;                /* Ending year.                             */
    int outyear;
    double mean;                   /* Sample mean.                             */
    double var;                    /* Sample variance.                         */
    double skew;                   /* Skewness coefficient.                    */
    double kurt;                   /* Kurtosis coefficient.                    */
    int  max;                    /* Maximum value.                           */
    int  min;                    /* Minimum value.                           */
    double *data;                  /* Vector of time series observations.      */
    double refactor;          /* refactor parameter  */
    };

/*****************************************************************************/

struct Tusmodel          /* Seasonal US model with deterministic components: */
    {
    char *name;          /* Model name (string).                             */
    const char *residuals;     /* Model residuals name                             */
    double boxlam;         /* Box-Cox parameter lambda: either 0.0 or 1.0.     */
    double boxm;	      /* Box-Cox parameter M                             */
    int  sper;           /* Seasonal period: either 1(A), 4(Q) or 12(M).     */
    int  nrdiff;         /* Number of regular differences.                   */
    int  nadiff;         /* Number of (complete) annual differences.         */
    int  *ifadf;         /* Individual factors of the annual difference.     */
    int  ornsop;         /* Order of the resulting non-stationary operator.  */
    double *rnsop;         /* Resulting non-stationary operator.               */
    double cbands;
    double mu;             /* Mean parameter.                                  */
    int  Imu;            /* Flag (0-1): estimation of mu.                    */
    double sigma2;         /* Estimated residual variance.                     */

 /* Section [1]: Deterministic variables (intervention + seasonal):          */

    int  NdetVar;        /* N§ of deterministic variables (detvars).         */
    int  *Nomega;        /* Order of omega(B) for each detvar (order s).     */
    double **Omega;        /* Matrix of omegas: one row for each detvar.       */
    int  **Imega;        /* Matrix of flags (0-1): omegas to be estimated.   */
    int  *Ndelta;        /* Order of delta(B) for each detvar (order r).     */
    double **Delta;        /* Matrix of deltas: one row for each detvar.       */
    int  **Ielta;        /* Matrix of flags (0-1): deltas to be estimated.   */

 /* Section [2]: Standard regular-annual AR and MA factors:                  */

    int  NumAr1;         /* N§ of regular AR factors.                        */
    int  *p1;            /* N§ of phis for each factor (AR order).           */
    double **Ar1;          /* Matrix of phis: one row for each factor.         */
    int  **Ia1;          /* Matrix of flags (0-1): phis to be estimated.     */

    int  NumAr2;         /* N§ of annual AR factors.                         */
    int  *p2;            /* N§ of PHIS for each factor (AR order).           */
    double **Ar2;          /* Matrix of PHIS: one row for each factor.         */
    int  **Ia2;          /* Matrix of flags (0-1): PHIS to be estimated.     */

    int  NumMa1;         /* N§ of regular MA factors.                        */
    int  *q1;            /* N§ of thetas for each factor (MA order).         */
    double **Ma1;          /* Matrix of thetas: one row for each factor.       */
    int  **Im1;          /* Matrix of flags (0-1): thetas to be estimated.   */

    int  NumMa2;         /* N§ of annual MA factors.                         */
    int  *q2;            /* N§ of THETAS for each factor (MA order).         */
    double **Ma2;          /* Matrix of THETAS: one row for each factor.       */
    int  **Im2;          /* Matrix of flags (0-1): THETAS to be estimated.   */

 /* Section [3]: AR and MA factors of order 2 with fixed frequency:          */

    int  NumAr1f;        /* N§ of regular AR factors (fixed frequency).      */
    double *pfre1;         /* Frequency for each factor.                       */
    double **Ar1f;         /* Matrix of phis: one row for each factor.         */
    int  *Ia1f;          /* Vector of flags (0-1): phis to be estimated.     */

    int  NumAr2f;        /* N§ of annual AR factors (fixed frequency).       */
    double *pfre2;         /* Frequency for each factor.                       */
    double **Ar2f;         /* Matrix of PHIS: one row for each factor.         */
    int  *Ia2f;          /* Vector of flags (0-1): PHIS to be estimated.     */

    int  NumMa1f;        /* N§ of regular MA factors (fixed frequency).      */
    double *qfre1;         /* Frequency for each factor.                       */
    double **Ma1f;         /* Matrix of thetas: one row for each factor.       */
    int  *Im1f;          /* Vector of flags (0-1): thetas to be estimated.   */

    int  NumMa2f;        /* N§ of annual MA factors (fixed frequency).       */
    double *qfre2;         /* Frequency for each factor.                       */
    double **Ma2f;         /* Matrix of THETAS: one row for each factor.       */
    int  *Im2f;          /* Vector of flags (0-1): THETAS to be estimated.   */
    };


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
double dround (double num, int places);
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

STRING file_plot ( int nrdiff, int nadiff, double boxlam, int freq, char *outx11 );
void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                int *per, int *sub );
void DateToObs( int beg_per, int beg_sub, int per, int sub, int freq,
                int *obs_no );

#endif /* NLUTILS_H */
