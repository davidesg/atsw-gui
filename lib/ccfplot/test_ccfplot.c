/* test_ccfplot.c -- la CCF bidireccional, dibujada con datos de verdad.
 *
 *   test_ccfplot <salida.pre> <entrada.pre> <fichero.eps>
 *
 * Lee los dos .pre con el lector del motor, calcula la CCF en LOS DOS
 * SENTIDOS con el Ccf de drtran y el estadistico con su hosking_test, y
 * dibuja el panel. Los numeros no son mios: son los del motor.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "main.h"
#include "fue_pre_reader.h"
#include "ccfplot.h"

real macheps = 2.220446049250313e-16;
/* diagnose.c escribe su informe a este global, que en drtran es el .out.
 * La prueba no quiere informe: se le da stdout y ya. */
FILE *outputv;

/* media y desviacion de v[1..n] */
static void mds( real *v, int n, real *m, real *sd )
{
   int i; real s = 0, s2 = 0;

   for ( i = 1; i <= n; i++ ) s += v[i];
   *m = s / n;
   for ( i = 1; i <= n; i++ ) s2 += ( v[i] - *m ) * ( v[i] - *m );
   *sd = sqrt( s2 / n );
}

int main( int argc, char **argv )
{
   struct Tusmodel tm1, tm2;
   struct Tseries  ts1, ts2;
   real          **dm1 = NULL, **dm2 = NULL;
   real           *c1, *c2, **res;
   double         *corr;
   real            m1, m2, sd1, sd2, Q, p;
   int             lags, n, k, i;

   outputv = stderr;

   if ( argc < 4 ) {
      fprintf( stderr, "uso: test_ccfplot <salida.pre> <entrada.pre> <eps>\n" );
      return 2;
   }

   if ( read_fue_pre( argv[1], &tm1, &ts1, &dm1 ) != 0 ||
        read_fue_pre( argv[2], &tm2, &ts2, &dm2 ) != 0 ) {
      fprintf( stderr, "no pude leer los .pre\n" );
      return 1;
   }

   n    = ts1.nobs < ts2.nobs ? ts1.nobs : ts2.nobs;
   lags = ccf_lags_por_defecto( ts1.freq );
   if ( lags > n / 4 ) lags = n / 4;

   printf( "salida  : %-10s %d obs, frecuencia %d\n", ts1.name, ts1.nobs, ts1.freq );
   printf( "entrada : %-10s %d obs\n", ts2.name, ts2.nobs );
   printf( "retardos: %d a cada lado\n", lags );

   mds( ts1.data, n, &m1, &sd1 );
   mds( ts2.data, n, &m2, &sd2 );

   c1 = vector( 1, lags + 1 );
   c2 = vector( 1, lags + 1 );

   /* LOS DOS SENTIDOS, como hace drtran en diagnose.c:419-420 */
   Ccf( ts1.data, ts2.data, n, lags, c1, m1, m2, sd1, sd2 );  /* k >= 0 */
   Ccf( ts2.data, ts1.data, n, lags, c2, m2, m1, sd2, sd1 );  /* k <= 0 */

   corr = malloc( ( 2 * lags + 1 ) * sizeof( double ) );
   for ( k = 0; k <= lags; k++ ) {
      corr[lags + k] = c1[k + 1];
      corr[lags - k] = c2[k + 1];
   }

   /* El estadistico del motor */
   res = matrix( 1, n, 1, 2 );
   for ( i = 1; i <= n; i++ ) { res[i][1] = ts1.data[i]; res[i][2] = ts2.data[i]; }
   hosking_test( res, n, 2, lags, &Q, &p );
   printf( "Hosking : P(%d) = %.1f   p = %.4f\n", 4 * lags, (double) Q, (double) p );

   printf( "ccf     :" );
   for ( k = -lags; k <= lags; k += lags ) printf( " %+.3f", corr[k + lags] );
   printf( "   (extremos y centro)\n" );

   if ( ccf_write_eps( argv[3], corr, lags, n, ts2.name, ts1.name,
                       (double) Q, 4 * lags ) != 0 ) {
      fprintf( stderr, "no pude escribir %s\n", argv[3] );
      return 1;
   }
   printf( "escrito : %s\n", argv[3] );
   return 0;
}
