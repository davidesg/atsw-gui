/*
 * factor.c -- las raices de cada factor AR regular: el ar_factorization
 * de art (art/roots.py, que a su vez porta la herramienta Root de la UCM).
 *
 * Un factor AR normalizado
 *
 *     P(B) = 1 - c1 B - c2 B^2 - ... - cp B^p
 *
 * se parte en factores reales (1 - a1 B) y pares complejos
 * (1 - a1 B - a2 B^2). Para un par con raiz z de P(B) = 0:
 *
 *     r       = 1/|z|              el amortiguamiento
 *     omega   = |arg z|            la frecuencia, en radianes
 *     a1      = 2 Re z / |z|^2,    a2 = -1/|z|^2 = -r^2
 *     armonico = omega s / (2 pi), periodo = 2 pi / omega
 *
 * Un par con el armonico en un entero 1..s/2 y r cerca de 1 es un AR
 * estacional escondido dentro de un AR(p) libre. ESO ES LO QUE SE ENSEÑA;
 * si lo es o no, lo dice el analista (y, mas adelante, el MEG).
 *
 * DE DONDE SALEN LOS NUMEROS. El .out lista cada factor con sus
 * coeficientes, y los estimados llevan su numero de parametro:
 *
 *     Coefficients for regular AR factor 1:
 *           0.402725  (0.062259) [12]
 *           0.000000                       <- fijo: sin parentesis
 *
 * El numero es el indice en la matriz de covarianzas, que lee lib/outfile.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gsl/gsl_poly.h>

#include "contrastes.h"
#include "outfile.h"

#define TOL_IMAG     1e-6
#define TOL_ARMONICO 0.15
#define TOL_UNITARIA 0.05
#define UMBRAL_MODULOS 0.20

/* Los errores tipicos de un par complejo por el metodo delta, sobre la
 * covarianza 2x2 de (a1, a2). Es complex_factor_se de art/roots.py, que se
 * valido contra la hoja ABTreadway-Dperar2.xls.                          */
static void et_delta( double a1, double a2, const double cov[2][2],
                      double *r_et, double *per_et )
{
    double v1 = cov[0][0], v2 = cov[1][1], c12 = cov[0][1];
    double d  = sqrt( -a2 );
    double t  = a1 / ( 2.0 * d );
    double w  = acos( t );
    double dp = 2.0 * M_PI / ( w * w * sqrt( 1.0 - t * t ) );
    double fx = dp * ( 1.0 / ( 2.0 * d ) );
    double fy = dp * ( a1 / ( 4.0 * d * d * d ) );
    double vp = fx * fx * v1 + fy * fy * v2 + 2.0 * fx * fy * c12;

    *r_et   = sqrt( v2 / ( 4.0 * -a2 ) );
    *per_et = vp > 0.0 ? sqrt( vp ) : NAN;
}

int ct_factoriza( const double *c, int p, int s, const double cov2[2][2], CtAr *a )
{
    double coef[CT_MAX_ORDEN + 1], z[2 * CT_MAX_ORDEN];
    int    orden[CT_MAX_ORDEN], usado[CT_MAX_ORDEN];
    int    i, j, k, n, ncomp = 0, p0 = p;

    if ( p < 0 || p > CT_MAX_ORDEN ) return 1;
    a->orden = p0;
    for ( i = 0; i < p; i++ ) a->c[i] = c[i];
    a->nf = 0;

    /* Los ceros del final no son raices: un AR(12) con c12 = 0 es de orden
       menor, y gsl no admite el coeficiente principal nulo.            */
    while ( p > 0 && c[p - 1] == 0.0 ) p--;
    if ( p == 0 ) return 0;

    coef[0] = 1.0;
    for ( i = 1; i <= p; i++ ) coef[i] = -c[i - 1];
    if ( p == 1 )
        { z[0] = 1.0 / c[0]; z[1] = 0.0; }
    else
        {
        gsl_poly_complex_workspace *w = gsl_poly_complex_workspace_alloc( (size_t) p + 1 );
        int                         e;

        if ( !w ) return 1;
        e = gsl_poly_complex_solve( coef, (size_t) p + 1, w, z );
        gsl_poly_complex_workspace_free( w );
        if ( e != 0 ) return 1;
        }

    /* por modulo, de menor a mayor: el mas cercano a la unidad primero */
    for ( i = 0; i < p; i++ ) orden[i] = i;
    for ( i = 1; i < p; i++ )
        for ( j = i; j > 0; j-- )
            {
            int    x = orden[j], y = orden[j - 1];
            double mx = hypot( z[2 * x], z[2 * x + 1] );
            double my = hypot( z[2 * y], z[2 * y + 1] );

            if ( mx < my ) { orden[j] = y; orden[j - 1] = x; } else break;
            }
    for ( i = 0; i < p; i++ )
        {
        a->zre[i] = z[2 * orden[i]];
        a->zim[i] = z[2 * orden[i] + 1];
        usado[i]  = 0;
        }
    for ( i = p; i < p0; i++ ) a->zre[i] = a->zim[i] = 0.0;

    n = 0;
    for ( i = 0; i < p; i++ )
        {
        CtFactor *f = &a->f[n];
        double    re = a->zre[i], im = a->zim[i];

        if ( usado[i] ) continue;
        usado[i] = 1;
        memset( f, 0, sizeof *f );
        f->modulo = hypot( re, im );
        if ( fabs( im ) <= TOL_IMAG )
            {
            f->a1 = 1.0 / re;
            n++;
            continue;
            }
        for ( k = i + 1; k < p; k++ )
            if ( !usado[k] && hypot( a->zre[k] - re, a->zim[k] + im ) < 1e-6 )
                { usado[k] = 1; break; }
        {
        double m2 = re * re + im * im;

        f->complejo = 1;
        f->a1       = 2.0 * re / m2;
        f->a2       = -1.0 / m2;
        f->r        = sqrt( -f->a2 );
        f->omega    = fabs( atan2( im, re ) );
        f->armonico = f->omega * s / ( 2.0 * M_PI );
        f->periodo  = f->omega > 0.0 ? 2.0 * M_PI / f->omega : INFINITY;
        f->vida_media = ( f->r > 0.0 && f->r < 1.0 ) ? log( 0.5 ) / log( f->r ) : INFINITY;
        {
        double kr = floor( f->armonico + 0.5 );

        f->estacional = s > 1 && fabs( f->armonico - kr ) <= TOL_ARMONICO
                        && kr >= 1 && kr <= s / 2;
        }
        f->casi_unitaria = 1.0 - f->r < TOL_UNITARIA;
        /* Los ± solo para un AR(2) estimado entero: ahi (a1, a2) SON
           (c1, c2) y su covarianza es la del .out. En un orden mayor el
           par sale de las raices y no tiene una covarianza propia.   */
        if ( cov2 && p == 2 && ncomp == 0 )
            {
            f->con_et = 1;
            et_delta( f->a1, f->a2, cov2, &f->r_et, &f->periodo_et );
            }
        ncomp++;
        n++;
        }
        }
    a->nf = n;
    return 0;
}

/* ¿Es una linea de coeficiente? « 0.402725  (0.062259) [12]» o « 0.000000». */
static int coeficiente( const char *l, double *v, int *idx )
{
    double et;
    int    k, usados = 0;
    char   resto;

    if ( sscanf( l, " %lf ( %lf ) [ %d ]", v, &et, &k ) == 3 )
        { *idx = k; return 1; }
    if ( sscanf( l, " %lf%n", v, &usados ) == 1 && sscanf( l + usados, " %c", &resto ) != 1 )
        { *idx = 0; return 1; }
    return 0;
}

int ct_factores_ar( const char *out, int s, int sin_et, CtFactores *f,
                    char *why, size_t nwhy )
{
    FueOut *o;
    gchar  *txt = NULL;
    gchar **lin;
    int     i, k, cur = -1;

    memset( f, 0, sizeof *f );
    f->s = s;
    f->sin_et = sin_et;
    if ( !g_file_get_contents( out, &txt, NULL, NULL ) )
        { g_snprintf( why, nwhy, "No pude leer %s.", out ); return 1; }

    lin = g_strsplit( txt, "\n", -1 );
    g_free( txt );
    for ( i = 0; lin[i]; i++ )
        {
        double v;
        int    idx;

        g_strchomp( lin[i] );
        if ( sscanf( lin[i], "Coefficients for regular AR factor %d", &k ) == 1 )
            {
            cur = f->nar < CT_MAX_AR ? f->nar++ : -1;
            if ( cur >= 0 ) f->ar[cur].orden = 0;
            continue;
            }
        if ( cur >= 0 && coeficiente( lin[i], &v, &idx ) )
            {
            CtAr *a = &f->ar[cur];

            if ( a->orden < CT_MAX_ORDEN )
                { a->c[a->orden] = v; a->idx[a->orden] = idx; a->orden++; }
            continue;
            }
        cur = -1;
        }
    g_strfreev( lin );
    if ( f->nar == 0 ) return 0;

    o = g_new0( FueOut, 1 );
    if ( !fueout_read( out, o ) ) memset( o, 0, sizeof *o );

    for ( k = 0; k < f->nar; k++ )
        {
        CtAr   *a = &f->ar[k];
        double  c[CT_MAX_ORDEN], cov[2][2];
        int     p = a->orden, idx[CT_MAX_ORDEN], con = 0;

        memcpy( c, a->c, sizeof c );
        memcpy( idx, a->idx, sizeof idx );
        if ( !sin_et && p == 2 && idx[0] > 0 && idx[1] > 0 && o->ncov >= idx[1] )
            {
            cov[0][0] = fo_cov( o, idx[0], idx[0] );
            cov[1][1] = fo_cov( o, idx[1], idx[1] );
            cov[0][1] = cov[1][0] = fo_cov( o, idx[0], idx[1] );
            con = cov[0][0] > 0.0 && cov[1][1] > 0.0;
            }
        if ( ct_factoriza( c, p, s, con ? (const double (*)[2]) cov : NULL, a ) != 0 )
            {
            g_snprintf( why, nwhy, "No pude sacar las raíces del factor AR %d.", k + 1 );
            g_free( o );
            return 1;
            }
        memcpy( a->idx, idx, sizeof idx );
        }
    g_free( o );

    /* art BUG-0103: unos modulos casi iguales son la firma de (1 - Θ B^N). */
    f->mod_min = INFINITY; f->mod_max = 0.0;
    {
    double suma = 0.0;

    for ( k = 0; k < f->nar; k++ )
        for ( i = 0; i < f->ar[k].orden; i++ )
            {
            double m = hypot( f->ar[k].zre[i], f->ar[k].zim[i] );

            if ( m <= 1e-12 ) continue;
            f->nmod++;
            suma += m;
            if ( m < f->mod_min ) f->mod_min = m;
            if ( m > f->mod_max ) f->mod_max = m;
            }
    if ( f->nmod >= 4 )
        {
        f->mod_disp = ( f->mod_max - f->mod_min ) / ( suma / f->nmod );
        f->modulos_parecidos = f->mod_disp < UMBRAL_MODULOS;
        }
    }
    return 0;
}
