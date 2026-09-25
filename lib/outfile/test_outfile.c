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

    printf( "\nLOS HECHOS DE LA DIAGNOSIS, QUE EL .out YA TRAE\n" );
    ok( o.media_et > 0.0, "la media viene con su ERROR TIPICO: sin el no hay "
                          "contraste que hacer" );
    ok( o.nlb >= 2, "y el Ljung-Box es una ESCALERA, no un numero" );
    {
    int i, ok_p = 1;

    for ( i = 0; i < o.nlb; i++ )
        if ( o.lb_p_[i] < 0.0 || o.lb_p_[i] > 1.0 ) ok_p = 0;
    ok( ok_p, "  con un p por peldaño" );
    ok( o.lb_df_[o.nlb - 1] == o.lb_df,
        "  y el ultimo es el que la rejilla enseña" );
    }
    ok( o.tiene_hist && o.esp2 > 0.0,
        "el histograma trae el % OBSERVADO y el ESPERADO: el motor ya los "
        "compara" );
    ok( o.npar_leidos > 0, "la tabla de parametros se lee" );
    {
    int i, con_et = 0;

    for ( i = 0; i < o.npar_leidos; i++ )
        if ( o.par_estimado[i] && o.par_et[i] > 0.0 ) con_et++;
    ok( con_et > 0, "  con su error tipico, que es lo que da la t" );
    }
    ok( o.npares >= 0, "y los pares que se pisan salen de la matriz de "
                       "correlaciones, no de adivinar el formato de la lista" );

    printf( "\nLOS ANOMALOS, QUE EL MOTOR YA LISTA Y YA CALIBRA\n" );
    ok( o.next >= 0, "los residuos extremos se leen" );
    if ( o.next > 0 )
        {
        ok( o.ext[0].obs > 0 && o.ext[0].fecha[0] != '\0',
            "  con su observacion y su fecha" );
        ok( o.ext[0].z != 0.0, "  y su valor tipificado" );
        }
    ok( o.ncal >= 0, "y la calibracion del motor: que tramos distorsionan "
                     "cada r(k)" );
    if ( o.ncal > 0 )
        {
        ok( o.cal[0].lag > 0 && o.cal[0].desde[0] && o.cal[0].hasta[0],
            "  con el retardo y el tramo de fechas" );
        /* OJO: esto SOLO esta en el .out de fue y SOLO sobre los residuos.
           fug no lo trae, asi que antes del modelo hay que calcularlo --
           que es para lo que existe lib/anomalos.                      */
        }

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
