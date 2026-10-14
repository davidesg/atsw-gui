/*
 * test_formales.c -- LOS CONTRASTES FORMALES, contra art-python.
 *
 *   test_formales FUE DIR_GOLDEN_FUE DIR_PRUEBAS DIR_TRABAJO
 *
 * Para cada modelo: su .pre se estima con fue (es lo que hace art con
 * estimar(pre)), se plantean los contrastes, fue estima cada restringido y
 * se comparan las log-verosimilitudes y los estadisticos con los que dio
 * art.formal_tests el 2026-10-14 sobre los MISMOS .pre: los golden del motor
 * y AIR.pre (lib/formales/pruebas), un «airline» del IPC con MA anual.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#ifdef G_OS_UNIX
#include <sys/wait.h>
#endif

#include "formales.h"

static int fallos;

static void ok( int c, const char *que )
{
    printf( "  %s  %s\n", c ? "ok  " : "FALLA", que );
    if ( !c ) fallos++;
}

static int cerca( double a, double b, double tol )
{
    return fabs( a - b ) <= tol * ( 1.0 + fabs( b ) );
}

static const char *FUE, *TRABAJO;

/* fue BASE en dir; TRUE si estimo (0) */
static int corre( const char *dir, const char *base )
{
    gchar *argv[3] = { (gchar *) FUE, (gchar *) base, NULL };
    gint   st = -1;

    if ( !g_spawn_sync( dir, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                        NULL, NULL, NULL, NULL, &st, NULL ) ) return 0;
#ifdef G_OS_UNIX
    if ( WIFEXITED( st ) ) st = WEXITSTATUS( st );
#endif
    return st == 0;            /* con un 3 deja los valores iniciales */
}

static double logelf( const char *out )
{
    gchar *t = NULL, *p;
    double v = NAN;

    if ( g_file_get_contents( out, &t, NULL, NULL ) && ( p = strstr( t, "logelf:" ) ) )
        v = g_ascii_strtod( p + 7, NULL );
    g_free( t );
    return v;
}

typedef struct { FmTipo tipo; double coef, lr, l_restr, c5; int rechaza; } Ref;

static void modelo( const char *pre_origen, const char *nombre, int nobs, int s, int d,
                    double l_ref, const Ref *ref, int nref )
{
    gchar   *dir = g_build_filename( TRABAJO, nombre, NULL );
    gchar   *pre, *out, *txt = NULL;
    FmPlan   pl;
    char     why[256], m[512];
    double   l;
    int      i, k;

    printf( "\n%s\n", nombre );
    g_mkdir_with_parents( dir, 0700 );
    pre = g_strdup_printf( "%s/%s.inp", dir, nombre );
    out = g_strdup_printf( "%s/%s.out", dir, nombre );
    if ( !g_file_get_contents( pre_origen, &txt, NULL, NULL ) ||
         !g_file_set_contents( pre, txt, -1, NULL ) )
        { ok( 0, "copiar el .pre" ); goto fin; }
    ok( corre( dir, nombre ), "fue estima el .pre" );
    l = logelf( out );
    snprintf( m, sizeof m, "la log-verosimilitud del estimado, %.6f (art %.6f)", l, l_ref );
    ok( cerca( l, l_ref, 1e-7 ), m );

    /* el .pre que sale es el optimo: de ahi parten los restringidos */
    g_free( pre );
    pre = g_strdup_printf( "%s/%s.pre", dir, nombre );
    ok( fm_plan( pre, out, dir, nobs, s, d, &pl, why, sizeof why ) == 0, "plantear los contrastes" );
    snprintf( m, sizeof m, "%d contrastes, los mismos que art", pl.n );
    ok( pl.n == nref, m );
    for ( i = 0; i < pl.n && i < nref; i++ )
        {
        FmPrueba *t = &pl.t[i];
        static const char *nom[] = { "simplificación estacional", "Shin-Fuller", "DCD",
                                     "DCD estacional" };

        fm_resultado( t, l, nobs, s, corre( dir, t->base ) );
        snprintf( m, sizeof m, "%s: L0 %.6f (art %.6f), estadístico %.6f (art %.6f), "
                  "crítico 5%% %.4f, %s", nom[t->tipo], t->l_restr, ref[i].l_restr, t->est,
                  ref[i].lr, t->c5, t->rechaza ? "rechaza" : "no rechaza" );
        ok( t->tipo == ref[i].tipo && t->calculable && cerca( t->l_restr, ref[i].l_restr, 1e-6 )
            && cerca( t->est, ref[i].lr, 1e-5 ) && cerca( t->c5, ref[i].c5, 1e-5 )
            && t->rechaza == ref[i].rechaza
            /* el coeficiente sale del .out, con seis decimales */
            && ( t->tipo == FM_ESTACIONAL || cerca( t->coef, ref[i].coef, 2e-6 ) ), m );
        }
    for ( k = 0; k < 4; k++ ) if ( pl.no_aplica[k][0] ) printf( "        (%s)\n", pl.no_aplica[k] );
fin:
    g_free( txt ); g_free( pre ); g_free( out ); g_free( dir );
}

int main( int argc, char **argv )
{
    gchar *f;

    if ( argc < 5 )
        { fprintf( stderr, "uso: test_formales FUE DIR_GOLDEN DIR_PRUEBAS DIR_TRABAJO\n" ); return 2; }
    FUE = g_canonicalize_filename( argv[1], NULL );     /* fue corre en otro directorio */
    TRABAJO = argv[4];
    printf( "\nLOS CONTRASTES FORMALES, CONTRA art.formal_tests\n" );

    {   /* AR(1) con armónicos: la simplificación y Shin-Fuller simple */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 358.88774308, -186.8465201887, 19.67513757, 1 },
        { FM_SF, 0.40272317, 37.53626609, -44.9389147360, 1.757733, 1 } };
    f = g_build_filename( argv[2], "IPC_ES_m01_ar1", "IPC_ES_m01_ar1.pre", NULL );
    modelo( f, "IPC_ES_m01_ar1", 216, 12, 1, -7.4026486474, r, 2 );
    g_free( f );
    }
    {   /* MA(1) con armónicos: el DCD regular, muy lejos de la frontera */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 359.58413947, -186.6359942551, 19.67513757, 1 },
        { FM_DCD, -0.43127924, 1057.36081526, -535.5243321509, 1.94, 1 } };
    f = g_build_filename( argv[2], "IPC_ES_m01_ma1", "IPC_ES_m01_ma1.pre", NULL );
    modelo( f, "IPC_ES_m01_ma1", 216, 12, 1, -6.8439245194, r, 2 );
    g_free( f );
    }
    {   /* AR(2) con armónicos: Shin-Fuller PARTE el factor (art BUG-0065) */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 15.77767809, -47.4221134029, 19.67513757, 0 },
        { FM_SF, 0.72703297, 1.69946526, -41.2327396237, 1.75, 0 } };
    f = g_build_filename( argv[2], "RIPC.2", "RIPC.2.pre", NULL );
    modelo( f, "RIPC.2", 78, 12, 0, -39.5332743594, r, 2 );
    g_free( f );
    }
    {   /* trimestral, AR(1) y un MA(1) con θ = 1 exacto: el DCD da LR = 0 */
    static const Ref r[] = {
        { FM_ESTACIONAL, 0, 9.13194846, 166.1470940066, 7.814727903, 1 },
        { FM_SF, 0.84669164, 0.82005753, 169.8930107062, 1.75, 0 },
        { FM_DCD, 1.0, 0.0, 170.7130682380, 1.94, 0 } };
    f = g_build_filename( argv[2], "R.6", "R.6.pre", NULL );
    modelo( f, "R.6", 68, 4, 1, 170.7130682380, r, 3 );
    g_free( f );
    }
    {   /* el airline: el DCD estacional, al borde (2.20 frente a 2.31) */
    static const Ref r[] = {
        { FM_DCD, -0.40622301, 623.58028854, -335.4654220017, 1.94, 1 },
        { FM_DCD_S, 0.86250753, 2.19569975, -24.7731276093, 2.31, 0 } };
    f = g_build_filename( argv[3], "AIR.pre", NULL );
    modelo( f, "AIR", 216, 12, 1, -23.6752777327, r, 2 );
    g_free( f );
    }

    if ( fallos ) { printf( "\n%d fallos\n", fallos ); return 1; }
    printf( "\ntodo bien\n" );
    return 0;
}
