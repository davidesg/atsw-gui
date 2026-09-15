/*****************************************************************************/
/* plothost.h -- what fugplot.c needs from the program that uses it:         */
/* struct Tseries, vector(), Acf(), Pacf(), ChiTest(), the statistics (Skew, */
/* Kurt, JarqueBera, Stdev), iround(), default_lags(), annual_step(),        */
/* first_multiple() and meandv(). fugdraw.c/h, fd_metrics.h and fugplot.c/h  */
/* are the same files in fug and fue; each program has its own plothost.h.  */
/*****************************************************************************/

#ifndef PLOTHOST_H
#define PLOTHOST_H

#include "fue.h"
#include "nlatools.h"

void Pacf( int lags, real *pcorr );
real ChiTest( real *corr, int lags, int nobs );
real Skew( real *data, int nobs );
real Kurt( real *data, int nobs );
real JarqueBera( real Skew, real Kurt, int nobs );
real Stdev( real *data, int nobs );

/* plotsupport.c (the same functions as in diagnose.c of fug)                */
int  annual_step( int years, double width );
int  first_multiple( int year, int step );
int  default_lags( int nobs, int freq );
int  meandv( double *y, double *dts, double *ms, int nog, int ng );

#endif
