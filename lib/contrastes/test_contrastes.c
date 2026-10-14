/*
 * test_contrastes.c -- LOS CONTRASTES FORMALES, contra art-python.
 *
 *   test_contrastes ipc_wti.csv DIR_GOLDEN_FUE
 *
 * Los numeros de referencia los dio art-python (art._raiz_unitaria.adf/kpss
 * y art.roots.factor_ar) el 2026-10-14, sobre los MISMOS ficheros: los datos
 * del manual y los .out golden del motor fue. Si uno de los dos cambia, la
 * referencia se rehace con art, no a mano.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contrastes.h"

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

/* Las dos columnas del CSV del manual: fecha,ipc,wti. */
static int lee_csv( const char *ruta, double *ipc, double *wti, int max )
{
    FILE *f = fopen( ruta, "r" );
    char  l[256];
    int   n = 0;

    if ( !f ) return 0;
    if ( !fgets( l, sizeof l, f ) ) { fclose( f ); return 0; }
    while ( n < max && fgets( l, sizeof l, f ) )
        {
        char *c = strchr( l, ',' );

        if ( !c || sscanf( c + 1, "%lf,%lf", &ipc[n], &wti[n] ) != 2 ) break;
        n++;
        }
    fclose( f );
    return n;
}

typedef struct { int n; double adf, adf_p; int adf_lags; double crit, kpss, kpss_p; int kpss_lags; } Ref;

static void raiz( const char *nom, const double *x, int n, const Ref *ref, int d_ini )
{
    CtRaiz r[CT_MAX_D + 1];
    char   m[256];
    int    nr = ct_raiz_unitaria( x, n, 0.0, 2, r ), d;

    snprintf( m, sizeof m, "%s: tres filas, d = 0, 1, 2", nom );
    ok( nr == 3, m );
    for ( d = 0; d < nr; d++ )
        {
        snprintf( m, sizeof m, "%s d = %d: n %d, ADF %.4f (p %.3g, %d ret., crít. %.4f), "
                  "KPSS %.4f (p %.3g, %d)", nom, d, r[d].n, r[d].adf, r[d].adf_p,
                  r[d].adf_lags, r[d].adf_crit5, r[d].kpss, r[d].kpss_p, r[d].kpss_lags );
        ok( r[d].ok && r[d].n == ref[d].n
            && cerca( r[d].adf, ref[d].adf, 1e-7 ) && cerca( r[d].adf_p, ref[d].adf_p, 1e-6 )
            && r[d].adf_lags == ref[d].adf_lags && cerca( r[d].adf_crit5, ref[d].crit, 1e-7 )
            && cerca( r[d].kpss, ref[d].kpss, 1e-7 ) && cerca( r[d].kpss_p, ref[d].kpss_p, 1e-6 )
            && r[d].kpss_lags == ref[d].kpss_lags, m );
        }
    snprintf( m, sizeof m, "%s: el punto de partida de art es d = %d", nom, d_ini );
    ok( ct_d_inicial( r, nr ) == d_ini, m );
}

static char *ruta( const char *dir, const char *f )
{
    static char b[1024];

    snprintf( b, sizeof b, "%s/%s", dir, f );
    return b;
}

int main( int argc, char **argv )
{
    static double ipc[1000], wti[1000];
    static const Ref r_ipc[3] = {
        { 288, -0.2825664826, 0.9279374052, 15, -2.872225321, 2.490687846, 0.01, 10 },
        { 287, -3.44485248,   0.009521635417, 16, -2.872304929, 0.1506929362, 0.1, 35 },
        { 286, -6.255337065,  4.35286158e-08, 16, -2.872345179, 0.03118223556, 0.1, 26 } };
    static const Ref r_wti[3] = {
        { 288, -3.684303519,  0.004339435298, 1, -2.871699419, 0.4391883359, 0.06026364831, 10 },
        { 287, -11.61417205,  2.471781251e-21, 1, -2.87173526, 0.1426306945, 0.1, 3 },
        { 286, -8.02606245,   2.015916873e-12, 14, -2.872264977, 0.1733119792, 0.1, 107 } };
    CtAr       a;
    CtFactores f;
    char       why[256];
    int        n;

    if ( argc < 3 )
        { fprintf( stderr, "uso: test_contrastes ipc_wti.csv DIR_GOLDEN_FUE\n" ); return 2; }

    printf( "\nADF Y KPSS PARA d = 0, 1, 2, CONTRA art-python\n" );
    n = lee_csv( argv[1], ipc, wti, 1000 );
    ok( n == 288, "el CSV del manual: 288 meses" );
    raiz( "ipc", ipc, n, r_ipc, 1 );
    /* El WTI en logaritmos: ADF ya rechaza con d = 0. Es lo que dicen los
       contrastes; el manual usa d = 1 por el correlograma. */
    raiz( "wti", wti, n, r_wti, 0 );
    {
    double x[5] = { 1, 2, 3, 4, 5 };
    CtRaiz r[3];

    ok( ct_raiz_unitaria( x, 5, 0.0, 2, r ) == 0, "una serie de 5 datos no se contrasta" );
    static double y[1000];

    memcpy( y, wti, sizeof y );
    y[5] = -1.0;
    ok( ct_raiz_unitaria( y, n, 0.0, 2, r ) == 0 && ct_raiz_unitaria( y, n, 1.0, 2, r ) == 3,
        "con logaritmos, un dato no positivo no se contrasta; en niveles, sí" );
    }

    printf( "\nLA FACTORIZACION DEL AR, CONTRA art.roots\n" );
    {
    /* (1 - 0.5B)(1 + 0.81B^2): el autotest de roots.py */
    double c[3] = { 0.5, -0.81, 0.405 };

    ok( ct_factoriza( c, 3, 12, NULL, &a ) == 0 && a.nf == 2, "AR(3): dos factores" );
    /* por módulo: el par (raíz 1/0.9) antes que el real (raíz 2) */
    ok( a.f[0].complejo && cerca( a.f[0].r, 0.9, 1e-9 ) && cerca( a.f[0].armonico, 3.0, 1e-9 )
        && cerca( a.f[0].periodo, 4.0, 1e-9 ) && a.f[0].estacional,
        "el par complejo: r = 0.9, armónico 3, periodo 4, estacional" );
    ok( !a.f[1].complejo && cerca( a.f[1].a1, 0.5, 1e-9 ) && cerca( a.f[1].modulo, 2.0, 1e-9 ),
        "el real: (1 - 0.5 B), raíz 2" );
    }
    {
    double c[2] = { 1.2, -0.6 }, cov[2][2] = { { 0.004, -0.003 }, { -0.003, 0.0035 } };

    ok( ct_factoriza( c, 2, 12, (const double (*)[2]) cov, &a ) == 0 && a.nf == 1
        && a.f[0].con_et && cerca( a.f[0].r, 0.7745966692414834, 1e-9 )
        && cerca( a.f[0].periodo, 9.17629486602647, 1e-9 )
        && cerca( a.f[0].r_et, 0.038188130791298666, 1e-9 )
        && cerca( a.f[0].periodo_et, 0.5297425596273325, 1e-9 )
        && cerca( a.f[0].vida_media, 2.7138308977134487, 1e-9 ),
        "AR(2) con covarianza: d ± e.t. y periodo ± e.t. por el método delta" );
    }

    /* Los .out golden del motor */
    ok( ct_factores_ar( ruta( argv[2], "RIPC.2/RIPC.2.out" ), 12, 0, &f, why, sizeof why ) == 0
        && f.nar == 1 && f.ar[0].orden == 2 && f.ar[0].nf == 2
        && !f.ar[0].f[0].complejo && cerca( f.ar[0].f[0].modulo, 1.375448102629401, 1e-6 )
        && cerca( f.ar[0].f[1].modulo, 1.6829259514947221, 1e-6 ) && !f.modulos_parecidos,
        "RIPC.2: un AR(2) con dos raíces reales, 1.375 y 1.683" );

    ok( ct_factores_ar( ruta( argv[2], "en4_fac9x2/en4_fac9x2.out" ), 1, 0, &f, why, sizeof why ) == 0
        && f.nar == 9, "en4_fac9x2: nueve factores AR(2)" );
    ok( f.ar[0].nf == 1 && f.ar[0].f[0].con_et
        && cerca( f.ar[0].f[0].r, 0.9272459220724565, 1e-6 )
        && cerca( f.ar[0].f[0].periodo, 41.65516907276633, 1e-6 )
        && cerca( f.ar[0].f[0].r_et, 0.029509930920949782, 1e-5 )
        && cerca( f.ar[0].f[0].periodo_et, 7.114262437260074, 1e-5 ),
        "en4_fac9x2, factor 1: d = 0.927 ± 0.030, periodo 41.7 ± 7.1" );
    ok( f.ar[7].f[0].casi_unitaria && cerca( f.ar[7].f[0].r, 0.9510835925406348, 1e-6 )
        && cerca( f.ar[7].f[0].periodo_et, 0.01731498210301508, 1e-5 ),
        "en4_fac9x2, factor 8: d = 0.951, casi unitario" );
    ok( f.nmod == 18 && cerca( f.mod_disp, 0.11713729793514732, 1e-6 ) && f.modulos_parecidos,
        "en4_fac9x2: 18 módulos con dispersión 11.7 %: el aviso de art BUG-0103" );
    ok( ct_factores_ar( ruta( argv[2], "en4_fac9x2/en4_fac9x2.out" ), 1, 1, &f, why, sizeof why ) == 0
        && !f.ar[0].f[0].con_et, "con la covarianza de semilla, sin ±" );

    ok( ct_factores_ar( ruta( argv[2], "en4_ar18/en4_ar18.out" ), 1, 0, &f, why, sizeof why ) == 0
        && f.nar == 1 && f.ar[0].orden == 18 && f.ar[0].nf == 9, "en4_ar18: un AR(18), nueve pares" );
    ok( f.ar[0].f[0].complejo && !f.ar[0].f[0].con_et
        && cerca( f.ar[0].f[0].r, 0.947855023151267, 1e-6 )
        && cerca( f.ar[0].f[0].periodo, 2.401372000084791, 1e-6 )
        && cerca( f.ar[0].f[8].r, 0.828939879831602, 1e-6 ),
        "en4_ar18: el par más cercano a la unidad, d = 0.948 y periodo 2.40; sin ± (orden > 2)" );
    ok( cerca( f.mod_disp, 0.13544260412123446, 1e-6 ) && f.modulos_parecidos,
        "en4_ar18: dispersión de los módulos 13.5 %" );

    if ( fallos ) { printf( "\n%d fallos\n", fallos ); return 1; }
    printf( "\ntodo bien\n" );
    return 0;
}
