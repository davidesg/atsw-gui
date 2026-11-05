/*
 * plothost.h -- lo que sim_gui le da a lib/fugplot (el grafico de los
 * residuos de la pagina del modelo). El mismo contrato que drtran_gui.
 */
#ifndef PLOTHOST_H
#define PLOTHOST_H

#include "main.h"

void fp_acf_ser( struct Tseries *ser, int lags, real *corr );
#define Acf fp_acf_ser

int  annual_step( int years, double width );
int  first_multiple( int year, int step );
int  default_lags( int nobs, int freq );
int  meandv( double *y, double *dts, double *ms, int nog, int ng );

real JarqueBera( real skew, real kurt, int nobs );
int  iround( double num );

#endif /* PLOTHOST_H */
