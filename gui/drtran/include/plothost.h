/*
 * plothost.h -- lo que mtram le debe a lib/fugplot.
 *
 * fugplot.c es el MISMO fichero en fue, en fug y aqui: dibuja la bateria de
 * graficos de la escuela --serie, ACF/PACF, histograma, media-desviacion-- y
 * para ello pide del programa anfitrion un puñado de funciones. Cada programa
 * pone las suyas en su propio plothost.h; esto es el de mtram.
 *
 * CASI TODO YA ESTABA. El motor drtran trae Acf, Pacf, ChiTest, Skew, Kurt,
 * Stdev y Mean en diagnose.c, y vector/free_vector en nlatools.c, y son las
 * mismas funciones: los graficos de los residuos de mtram salen de los mismos
 * numeros que la diagnosis del .out, no de un calculo parecido.
 *
 * LA UNICA COSTURA es Acf, que en drtran recibe (data, nobs, lags, corr, mean,
 * var) y en fue (ser, lags, corr). El calculo es identico --se comprueba linea
 * por linea-- y lo unico que cambia es quien pasa la media y la varianza. Las
 * dos tienen que convivir en el mismo binario, asi que la de fugplot se
 * renombra aqui: el #define va DESPUES de main.h, para que la declaracion del
 * motor se lea con su nombre de siempre.
 */

#ifndef PLOTHOST_H
#define PLOTHOST_H

/* main.h de drtran trae las dos cosas: struct Tseries y real, y las
 * declaraciones de diagnose.c y nlatools.c. En drtran no hay nlatools.h
 * aparte -- los asignadores estan en main.h.                            */
#include "main.h"

/* La ACF con la firma que fugplot espera. Ver arriba por que se renombra. */
void fp_acf_ser( struct Tseries *ser, int lags, real *corr );
#define Acf fp_acf_ser

/* Lo que drtran no tenia. plotsupport.c (lib/fugplot) trae las cuatro
 * primeras; plotshim.c, las dos que estaban dentro de fue.              */
int  annual_step( int years, double width );
int  first_multiple( int year, int step );
int  default_lags( int nobs, int freq );
int  meandv( double *y, double *dts, double *ms, int nog, int ng );

real JarqueBera( real skew, real kurt, int nobs );
int  iround( double num );

#endif /* PLOTHOST_H */
