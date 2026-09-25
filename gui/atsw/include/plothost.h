/*
 * plothost.h -- lo que la madre le debe a lib/fugplot.
 *
 * fugplot.c es EL MISMO FICHERO en fue, en fug, en mtram y aqui: dibuja los
 * graficos de la escuela y para ello pide del programa anfitrion un puñado de
 * funciones. Cada programa pone las suyas en su propio plothost.h; este es el
 * de atsw_gui.
 *
 * POR QUE LA MADRE LO ENLAZA. Para calibrar anomalos hay que ver el grafico
 * de residuos + ACF/PACF, y ese grafico ya existe: es el que fue dibuja con
 * la opcion -c. Redibujarlo con cairo daria OTRO grafico parecido, y el
 * analista compararia dos dibujos distintos creyendo que compara dos
 * calibraciones. Aqui se dibuja el de fue, con su mismo lienzo, sus mismas
 * escalas y su mismo Q.
 */

#ifndef PLOTHOST_H
#define PLOTHOST_H

typedef double real;

/* La misma de engines/fuf/include/fuf.h. fugplot solo mira name, nobs, freq,
 * begtime, begyear, mean, var y data.                                      */
struct Tseries
    {
    char *name;
    int  nobs;
    int  freq;
    int  numbering;
    int  begtime;
    int  begyear;
    int  endtime;
    int  endyear;
    real mean;
    real var;
    real skew;
    real kurt;
    real jarquebera;
    int  max;
    int  min;
    real refactor;
    real *data;
    };

/* plotstats.c: las de diagnose.c y nlatools.c del motor */
double *vector( long nl, long nh );
void    free_vector( double *v, long nl, long nh );

real Mean( real *data, int nobs );
real Stdev( real *data, int nobs );
real Skew( real *data, int nobs );
real Kurt( real *data, int nobs );
real JarqueBera( real skew, real kurt, int nobs );
void Acf( struct Tseries *ser, int lags, real *corr );
void Pacf( int lags, real *pcorr );
real ChiTest( real *corr, int lags, int nobs );
int  iround( double num );

/* plotsupport.c (lib/fugplot) */
int  annual_step( int years, double width );
int  first_multiple( int year, int step );
int  default_lags( int nobs, int freq );
int  meandv( double *y, double *dts, double *ms, int nog, int ng );

#endif /* PLOTHOST_H */
