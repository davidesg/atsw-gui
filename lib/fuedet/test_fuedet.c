/*
 * test_fuedet.c -- el filtro determinista de la suite contra el de fue.c.
 *
 * LA PRUEBA QUE IMPORTA ES LA DE LOS BITS. lib/fuedet es el paso [5] de
 * cast_us de fue.c sacado a un fichero, y lo que tiene que dar es lo que
 * daba alli, bit a bit: la referencia de abajo (referencia_fue) es ese paso
 * tal como estaba en fue.c antes de la mudanza, con su calcnu. La bateria de
 * fue lo confirma por otro lado: sus 109 ejecuciones salen byte a byte.
 *
 * Y la regla, que es lo que decidio el BUG-40 (drvec): con delta, nu se
 * TRUNCA en 40 retardos. Un escalon con delta = 0,95 deja de crecer en el
 * retardo 40, y ahi se separa de la recursion exacta.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fuedet.h"

static int fallos = 0;

static void check( int ok, const char *que )
{
    printf( "  %-6s %s\n", ok ? "ok" : "FALLO", que );
    if ( !ok ) fallos++;
}

/* ---- fue.c, antes de lib/fuedet: calcnu y el paso [5] de cast_us ------- */

static void calcnu_fue( double *omega, int s, double *delta, int r,
                        double *nu, int lags )
{
   int  i, j;
   double sum1, sum2;

   nu[0] = omega[0];
   for ( j = 1; j <= lags; j++ )
       {
       sum1 = 0.0;
       if ( r > 0 )
          for ( i = 1; i <= j; i++ )
              if ( i <= r ) sum1 = sum1 + delta[i] * nu[j-i];
       sum2 = 0.0;
       if ( s > 0 )
          if ( j <= s ) sum2 = omega[j];
       nu[j] = sum1 - sum2;
       }
}

static void referencia_fue( int NdetVar, double **Omega, int *Nomega,
                            double **Delta, int *Ndelta,
                            double **DataMat, int nobs, double *vtmp1 )
{
   int i, j, k, NuLag[8];
   double Nu[8][64], tmp1, tmp2;

   for ( j = 1; j <= NdetVar; j++ )
       {
       if ( Ndelta[j] == 0 )
          NuLag[j] = Nomega[j];
       else
          NuLag[j] = 40;
       calcnu_fue( Omega[j], Nomega[j], Delta[j], Ndelta[j], Nu[j], NuLag[j] );
       }
   for ( i = 1; i <= nobs; i++ )
       {
       tmp1 = 0.0;
       for ( j = 1; j <= NdetVar; j++ )
           {
           tmp2 = 0.0;
           for ( k = 0; k <= NuLag[j]; k++ ) if ( i-k >= 1 )
               tmp2 += Nu[j][k] * DataMat[j][i-k];
           tmp1 += tmp2;
           }
       vtmp1[i] = DataMat[0][i] - tmp1;
       }
}

/* ------------------------------------------------------------------------ */

#define N 120

int main( void )
{
    /* tres deterministas: un armonico sin delta, un escalon con delta y un
       impulso con omega(B) de grado 1 y delta(B) de grado 2 */
    double om1[1] = { 0.37 };
    double om2[1] = { 1.419 };
    double de2[2] = { 0.0, 0.8434 };
    double om3[2] = { 0.61, -0.25 };
    double de3[3] = { 0.0, 0.5, -0.3 };
    double *Omega[4] = { NULL, om1, om2, om3 };
    double *Delta[4] = { NULL, NULL, de2, de3 };
    int Nomega[4] = { 0, 0, 0, 1 };
    int Ndelta[4] = { 0, 0, 1, 2 };
    double x0[N + 1], x1[N + 1], x2[N + 1], x3[N + 1];
    double *DataMat[4] = { x0, x1, x2, x3 };
    double ref[N + 1], out[N + 1], det[N + 1];
    int t, iguales;

    for ( t = 1; t <= N; t++ )
        {
        x0[t] = 100.0 + 0.3 * t + sin( 0.7 * t ) + 0.01 * ( ( t * 7919 ) % 101 );
        x1[t] = cos( 2.0 * 3.141592653589793 * t / 12.0 );
        x2[t] = ( t >= 2 ) ? 1.0 : 0.0;
        x3[t] = ( t == 30 ) ? 1.0 : 0.0;
        }

    printf( "lib/fuedet: el paso [5] de fue.c\n" );

    referencia_fue( 3, Omega, Nomega, Delta, Ndelta, DataMat, N, ref );
    fuedet_noise( 3, Omega, Nomega, Delta, Ndelta, DataMat, x0, N, out );
    iguales = 1;
    for ( t = 1; t <= N; t++ )
        if ( memcmp( &ref[t], &out[t], sizeof(double) ) != 0 ) iguales = 0;
    check( iguales, "fuedet_noise = fue.c [5], bit a bit (3 variables, 120 obs.)" );

    /* la suma, restada UNA vez por quien llama, da los mismos bits */
    fuedet_sum( 3, Omega, Nomega, Delta, Ndelta, DataMat, N, det );
    iguales = 1;
    for ( t = 1; t <= N; t++ )
        {
        double r = x0[t] - det[t];
        if ( memcmp( &r, &out[t], sizeof(double) ) != 0 ) iguales = 0;
        }
    check( iguales, "data - fuedet_sum = fuedet_noise, bit a bit" );

    check( fuedet_nulag( 2, 0 ) == 2 && fuedet_nulag( 0, 0 ) == 0,
           "sin delta, nu llega hasta Nomega" );
    check( fuedet_nulag( 0, 1 ) == 40 && fuedet_nulag( 3, 2 ) == 40,
           "con delta, nu llega hasta 40" );

    /* La regla: un escalon desde t = 1 con delta = 0,95 deja de crecer en el
       retardo 40 (sum_{k<=40} 0,95^k); la recursion exacta sigue. */
    {
        double om[1] = { 1.0 }, de[2] = { 0.0, 0.95 };
        double *O[2] = { NULL, om }, *D[2] = { NULL, de };
        int No[2] = { 0, 0 }, Nd[2] = { 0, 1 };
        double s[N + 1], d[N + 1];
        double *X[2] = { NULL, s };
        double tope = 0.0, p = 1.0, exacto;
        int k, plano = 1;

        for ( t = 1; t <= N; t++ ) s[t] = 1.0;
        fuedet_sum( 1, O, No, D, Nd, X, N, d );
        for ( k = 0; k <= 40; k++ ) { tope += p; p *= 0.95; }
        for ( t = 41; t <= N; t++ )
            if ( fabs( d[t] - tope ) > 1e-12 ) plano = 0;
        check( plano, "escalon con delta 0,95: constante desde t = 41" );
        exacto = ( 1.0 - pow( 0.95, N ) ) / 0.05;
        check( fabs( exacto - d[N] ) > 2.0,
               "y se separa de la recursion exacta (BUG-40)" );
    }

    /* sin deterministas: el dato tal cual, la suma a cero */
    {
        double z[N + 1];
        fuedet_noise( 0, NULL, NULL, NULL, NULL, NULL, x0, N, z );
        iguales = 1;
        for ( t = 1; t <= N; t++ ) if ( z[t] != x0[t] ) iguales = 0;
        fuedet_sum( 0, NULL, NULL, NULL, NULL, NULL, N, z );
        for ( t = 1; t <= N; t++ ) if ( z[t] != 0.0 ) iguales = 0;
        check( iguales, "sin deterministas: ruido = dato, suma = 0" );
    }

    printf( "%s\n", fallos ? "test_fuedet: FALLOS" : "test_fuedet: todo ok" );
    return fallos ? 1 : 0;
}
