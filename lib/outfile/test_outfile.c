/*
 * test_outfile.c -- que el .out de fue se lea, contra .out DE VERDAD.
 *
 * Los ficheros son los GOLDEN del motor: los escribio fue y estan en control
 * de versiones. Si el formato de la salida cambia, cambia el golden y esta
 * prueba se entera el mismo dia. Es la misma disciplina que lib/outdiag, y
 * por la misma razon: un lector de texto formateado es fragil por naturaleza
 * y probarlo contra un fichero inventado no prueba nada.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "outfile.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static void es( const char *dio, const char *debe, const char *que )
{
    int c = strcmp( dio, debe ) == 0;

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  \"%s\"\n"
                                 "        debe \"%s\"\n", que, dio, debe );
    if ( !c ) fallos++;
}

static void cerca( double dio, double debe, double tol, const char *que )
{
    int c = fabs( dio - debe ) <= tol;

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  %g\n"
                                 "        debe %g\n", que, dio, debe );
    if ( !c ) fallos++;
}

int main( int argc, char **argv )
{
    const char *golden = argc > 1 ? argv[1] : "engines/fue/tests/golden";
    char        path[1024], b[64];
    FueOut      o;

    printf( "LA CHI-CUADRADO: DOS NUMEROS DEL MOTOR, TRADUCIDOS\n" );
    /* Los cuantiles al 5%% de las tablas. La cola es lo unico que se calcula
       aqui, asi que es lo unico que hay que comprobar contra algo externo. */
    cerca( chisq_cola( 3.841,   1 ), 0.05, 1e-4, "X2(1)  = 3.841  -> p = .05" );
    cerca( chisq_cola( 5.991,   2 ), 0.05, 1e-4, "X2(2)  = 5.991  -> p = .05" );
    cerca( chisq_cola( 18.307, 10 ), 0.05, 1e-4, "X2(10) = 18.307 -> p = .05" );
    cerca( chisq_cola( 55.758, 40 ), 0.05, 1e-4, "X2(40) = 55.758 -> p = .05" );
    cerca( chisq_cola( 0.0,     5 ), 1.00, 1e-9, "en cero, p = 1" );
    ok( chisq_cola( 1.0, 0 ) < 0.0, "sin grados de libertad, no hay p" );

    printf( "\nUN MODELO CON PARTE ANUAL: (3,1,0)(1,0,0)12 EN LOGARITMOS\n" );
    snprintf( path, sizeof path, "%s/DE.3/DE.3.out", golden );
    ok( fueout_read( path, &o ), "se lee el .out" );
    fueout_estructura( &o, b, sizeof b );
    es( b, "(3,1,0)(1,0,0)12  log",
        "los ordenes salen de SUMAR los factores, que el motor separa" );
    ok( o.factores == 2, "y eran dos factores" );
    ok( o.p == 3 && o.d == 1 && o.q == 0, "  la parte regular" );
    ok( o.P == 1 && o.D == 0 && o.Q == 0, "  la anual" );
    ok( o.s == 12, "  el periodo" );
    ok( o.lambda == 0.0, "  y la lambda, que 0 es el logaritmo" );
    ok( o.nobs == 216, "las observaciones" );
    ok( o.tiene_res, "hay bloque de residuos" );
    ok( o.sd > 0.0, "  con su desviacion tipica" );
    ok( o.tiene_lb && o.lb_df > 0, "y el Ljung-Box de la ACF, con sus g.l." );
    cerca( o.lb_p, chisq_cola( o.lb_q, o.lb_df ), 1e-12,
           "  cuyo p es el de su propio estadistico" );
    ok( o.tiene_jb && o.jb > 0.0, "y el Jarque-Bera" );
    cerca( o.jb_p, chisq_cola( o.jb, 2 ), 1e-12, "  con sus DOS grados" );

    printf( "\nUNO SIN PARTE ANUAL NO LA ENSEÑA\n" );
    snprintf( path, sizeof path, "%s/CPI_USA/CPI_USA.out", golden );
    if ( fueout_read( path, &o ) )
        {
        fueout_estructura( &o, b, sizeof b );
        ok( strstr( b, ")(" ) == NULL || o.P || o.D || o.Q,
            "«(0,1,1)» a secas se lee mejor que «(0,1,1)(0,0,0)0»" );
        printf( "        [%s]\n", b );
        }
    else
        printf( "  (sin %s: me la salto)\n", path );

    printf( "\nLO QUE NO ES UN .out DE fue SE DICE, NO SE INVENTA\n" );
    ok( !fueout_read( "/no/existe/nada.out", &o ), "un fichero que no esta" );
    ok( !o.hay, "  y la estructura queda vacia" );
    fueout_estructura( &o, b, sizeof b );
    es( b, "", "  y no se compone una estructura de la nada" );

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
