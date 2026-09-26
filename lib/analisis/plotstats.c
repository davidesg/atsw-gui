/*
 * plotstats.c -- la aritmetica que fugplot.c pide del anfitrion.
 *
 * DE DONDE SALE, LINEA POR LINEA: engines/fue/src/diagnose.c (Mean, Stdev,
 * Skew, Kurt, JarqueBera, Acf, Pacf, ChiTest) y engines/fue/src/nlatools.c
 * (vector, free_vector, matrix, free_matrix, iround). Son las MISMAS
 * formulas, no unas parecidas: el grafico que sale de aqui tiene que ser el
 * que sale de fue, y eso se consigue copiando el calculo, no reescribiendolo.
 *
 * NO SE COMPILA diagnose.c DIRECTAMENTE --como si se hace con inpcheck.c--
 * porque ese fichero arrastra medio motor. El precio es este: si el calculo
 * cambia en el motor, cambia aqui. La bateria lo vigila comparando el EPS de
 * fue con el de la madre sobre los mismos residuos.
 *
 * LO UNICO QUE NO SE COPIA es como se pide la memoria: el Pacf del motor usa
 * matrix()/free_matrix() de nlatools, con la base desplazada, y aqui la
 * matriz de Durbin-Levinson va plana. La ARITMETICA es la misma, indice por
 * indice; lo otro era arrastrar dos asignadores para una sola funcion.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "plothost.h"

/* --- nlatools.c ---------------------------------------------------------
 *
 * SOLO SI EL ANFITRION NO LOS TRAE. fue_gui ya tiene vector, free_vector e
 * iround en su propio nlutils.c --son los del motor, que arrastro al portar
 * el formulario-- y enlazar dos definiciones no compila. La madre no los
 * tiene, asi que los pone aqui. Quien los tenga compila con
 * -DPLOTSTATS_TIENE_NLA y se queda con los suyos: son los mismos.      */

static void fallo( const char *que )
{
   fprintf( stderr, "plotstats: %s\n", que );
   exit( 1 );
}

#ifndef PLOTSTATS_TIENE_NLA

double *vector( long nl, long nh )
{
   double *v = (double *) calloc( (size_t)( nh >= nl ? nh - nl + 1 : 1 ), sizeof( double ) );

   if ( !v ) fallo( "ALLOCATION FAILURE in vector()" );
   return( v - nl );
}

void free_vector( double *v, long nl, long nh )
{
   (void) nh;
   if ( v ) free( v + nl );
}

int iround( double num )
{
   return( (int) ( ( num < 0.0 ) ? ceil( num - 0.5 ) : floor( num + 0.5 ) ) );
}

#endif /* PLOTSTATS_TIENE_NLA */

/* --- diagnose.c --------------------------------------------------------- */

real Mean( real *data, int nobs )
{
   int  i;
   real sum = 0.0;

   for ( i = 1; i <= nobs; i++ ) sum += data[i];
   return( sum / nobs );
}

real Stdev( real *data, int nobs )
{
   int  i;
   real sum = 0.0, ave = Mean( data, nobs );

   for ( i = 1; i <= nobs; i++ ) sum += ( data[i] - ave ) * ( data[i] - ave );
   return( sqrt( sum / nobs ) );
}

real Skew( real *data, int nobs )
{
   int  i;
   real sum = 0.0, ave = Mean( data, nobs ), std = Stdev( data, nobs );

   if ( std < 1.0e-20 ) return( 0.0 );
   for ( i = 1; i <= nobs; i++ )
       sum += ( ( data[i] - ave ) * ( data[i] - ave ) * ( data[i] - ave ) ) /
              ( std * std * std );
   return( sum / nobs );
}

real Kurt( real *data, int nobs )
{
   int  i;
   real sum = 0.0, ave = Mean( data, nobs ), std = Stdev( data, nobs );

   if ( std < 1.0e-20 ) return( 0.0 );
   for ( i = 1; i <= nobs; i++ )
       sum += ( ( data[i] - ave ) * ( data[i] - ave ) *
                ( data[i] - ave ) * ( data[i] - ave ) ) /
              ( std * std * std * std );
   return( sum / nobs - 3.0 );
}

real JarqueBera( real skew, real kurt, int nobs )
{
   /* n/6.0, como el motor desde que se le corrigio la division entera. */
   return( nobs / 6.0 * ( skew * skew + kurt * kurt / 4 ) );
}

void Acf( struct Tseries *ser, int lags, real *corr )
{
   int  i, j;
   real rtmp1 = ser->mean, rtmp2 = ser->var;

   for ( i = 1; i <= lags; i++ ) corr[i] = 0.0;
   for ( j = 1; j <= lags; j++ )
       for ( i = 1; i <= ser->nobs - j; i++ )
           corr[j] += ( ser->data[i] - rtmp1 ) * ( ser->data[i+j] - rtmp1 ) /
                      ( ser->nobs * rtmp2 );
}

void Pacf( int lags, real *pcorr )
{
   int   i, j;
   real  sum1, sum2, *M, *corr;

   /* La matriz de Durbin-Levinson, plana y con su indice a mano. El motor
      usa matrix()/free_matrix() de nlatools; aqui no hace falta arrastrar
      los asignadores con base desplazada para una sola funcion -- la
      aritmetica es la misma, indice por indice.                        */
#define MP( a, b )  M[ ( (a) - 1 ) * lags + ( (b) - 1 ) ]

   M    = (real *) calloc( (size_t) lags * lags, sizeof( real ) );
   corr = vector( 1, lags );
   if ( !M ) fallo( "ALLOCATION FAILURE in Pacf()" );

   for ( i = 1; i <= lags; i++ )
       corr[i] = pcorr[i];          /* entra la acf, sale la pacf */

   MP( 1, 1 ) = corr[1];
   for ( i = 2; i <= lags; i++ )
       {
       sum1 = 0.0;
       sum2 = 0.0;
       for ( j = 1; j <= i-1; j++ )
           {
           sum1 += MP( i-1, j ) * corr[i-j];
           sum2 += MP( i-1, j ) * corr[j];
           }
       MP( i, i ) = ( corr[i] - sum1 ) / ( 1.0 - sum2 );
       for ( j = 1; j <= i-1; j++ )
           MP( i, j ) = MP( i-1, j ) - MP( i, i ) * MP( i-1, i-j );
       }
   for ( i = 1; i <= lags; i++ ) pcorr[i] = MP( i, i );

   free_vector( corr, 1, lags );
   free( M );
#undef MP
}

real ChiTest( real *corr, int lags, int nobs )
{
   int  i;
   real chisqr = 0.0;

   for ( i = 1; i <= lags; i++ ) chisqr += ( corr[i] * corr[i] ) / ( nobs - i );
   chisqr *= nobs;
   chisqr *= ( nobs + 2 );
   return( chisqr );
}
