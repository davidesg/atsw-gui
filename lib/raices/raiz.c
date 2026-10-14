/*
 * raiz.c -- ADF y KPSS para d = 0, 1, 2: el unit_root_analysis de art.
 *
 * LOS CONTRASTES NO SE ESCRIBEN AQUI. adf_test y kpss_test son los de
 * engines/art/src/unit_root_tests.c, que el Makefile compila tal cual: los
 * reescritos el 2026-09-28 con MacKinnon y Hobijn-Franses-Ooms y cotejados
 * con art-python y statsmodels. Una segunda copia seria una segunda
 * respuesta esperando a divergir.
 *
 * Lo que se hace aqui es lo que hace art.identification.unit_root_tests:
 * BoxCox(x), d diferencias regulares, y los dos contrastes sobre eso. Sin
 * quitar estacionalidad: ese es el contraste que art tabula, y su sesgo con
 * series muy estacionales (art BUG-0023) lo dice la ventana, no se esconde
 * aqui.
 */

#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "raices.h"
#include "unit_root_tests.h"

/* unit_root_tests.c habla por art_log (su printf). En el GUI, callado. */
int art_log( const char *fmt, ... )
{
    (void) fmt;
    return 0;
}

int rz_raiz_unitaria( const double *x, int n, double lam, int max_d, RzRaiz *r )
{
    double *z;
    int     i, d, m, nr = 0;

    if ( !x || n < 10 || !r ) return 0;
    if ( max_d > RZ_MAX_D ) max_d = RZ_MAX_D;
    z = malloc( (size_t) n * sizeof *z );
    if ( !z ) return 0;

    for ( i = 0; i < n; i++ )
        {
        if ( lam != 1.0 && !( x[i] > 0.0 ) ) { free( z ); return 0; }
        z[i] = lam == 0.0 ? log( x[i] )
             : lam == 1.0 ? x[i]
             : ( pow( x[i], lam ) - 1.0 ) / lam;
        }

    m = n;
    for ( d = 0; d <= max_d; d++ )
        {
        RzRaiz *q = &r[nr];
        double  crit[3] = { 0.0, 0.0, 0.0 };
        int     nobs = 0;

        if ( d > 0 )
            {
            for ( i = 0; i + 1 < m; i++ ) z[i] = z[i + 1] - z[i];
            m--;
            }
        if ( m < 10 ) break;

        memset( q, 0, sizeof *q );
        q->d = d;
        q->n = m;
        if ( adf_test( z, m, &q->adf, &q->adf_p, &q->adf_lags, &nobs, crit ) == 0 )
            {
            q->ok = 1;
            q->adf_crit5 = crit[1];
            kpss_test( z, m, &q->kpss, &q->kpss_p, &q->kpss_lags );
            q->adf_rechaza  = q->adf_p  < 0.05;
            q->kpss_rechaza = q->kpss_p < 0.05;
            }
        nr++;
        }
    free( z );
    return nr;
}

int rz_d_inicial( const RzRaiz *r, int nr )
{
    int i;

    if ( nr <= 0 ) return 0;
    for ( i = 0; i < nr; i++ )
        if ( r[i].ok && r[i].adf_rechaza ) return r[i].d;
    for ( i = 0; i < nr; i++ )
        if ( r[i].ok && r[i].adf_rechaza && !r[i].kpss_rechaza ) return r[i].d;
    return r[nr - 1].d;
}
