/*
 * gof.c -- ver gof.h.
 */

#include <stdlib.h>
#include <math.h>

#include "gof.h"

double *gof_estacionaria( const struct Tseries *ts, const struct Tusmodel *tm,
                          int *n )
{
    double *z, *w;
    int     N, o, t, j;

    *n = 0;
    if ( !ts || !tm || !ts->data || !tm->rnsop ) return NULL;

    N = ts->nobs;
    o = tm->ornsop;
    if ( N <= o + 1 ) return NULL;

    z = (double *) malloc( (size_t)( N + 1 ) * sizeof( double ) );
    if ( !z ) return NULL;

    for ( t = 1; t <= N; t++ )
        {
        double v = ts->data[t];

        if ( tm->boxlam == 0.0 )
            {
            if ( v <= 0.0 ) { free( z ); return NULL; }
            v = log( v );
            }
        else if ( tm->boxlam != 1.0 )
            {
            if ( v < 0.0 ) { free( z ); return NULL; }
            v = pow( v, tm->boxlam );
            }
        z[t] = v * ( ts->refactor != 0.0 ? ts->refactor : 1.0 );
        }

    *n = N - o;
    w  = (double *) malloc( (size_t) *n * sizeof( double ) );
    if ( !w ) { free( z ); *n = 0; return NULL; }

    for ( t = o + 1; t <= N; t++ )
        {
        double acc = 0.0;

        for ( j = 0; j <= o; j++ ) acc += tm->rnsop[j] * z[t - j];
        w[t - o - 1] = acc;
        }

    free( z );
    return w;
}

double gof_suma_cuad_cola( const double *x, int total, int n )
{
    double m = 0.0, s = 0.0;
    int    i, d;

    if ( !x || total < 2 ) return 0.0;
    if ( n < 2 || total < n ) n = total;
    d = total - n;

    for ( i = 0; i < n; i++ ) m += x[d + i];
    m /= n;
    for ( i = 0; i < n; i++ ) s += ( x[d + i] - m ) * ( x[d + i] - m );
    return s;
}

int gof_r2_brajin( const double *a, int na, double sw, double *r2, double *dt )
{
    double m = 0.0, sa = 0.0;
    int    i;

    if ( !a || na < 2 || sw <= 0.0 ) return 1;

    for ( i = 0; i < na; i++ ) m += a[i];
    m /= na;
    for ( i = 0; i < na; i++ ) sa += ( a[i] - m ) * ( a[i] - m );

    if ( r2 ) *r2 = 1.0 - sa / sw;
    if ( dt ) *dt = sqrt( sa / na );
    return 0;
}

double gof_reduccion( double dt_base, double dt_nuevo )
{
    if ( !( dt_base > 0.0 ) ) return NAN;
    return 1.0 - ( dt_nuevo * dt_nuevo ) / ( dt_base * dt_base );
}
