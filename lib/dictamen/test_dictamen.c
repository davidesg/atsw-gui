/*
 * test_dictamen.c -- los veredictos, SIN fichero.
 *
 * Esta es la ventaja de haber separado leer de juzgar: los umbrales se
 * prueban con números inventados, sin depender de que exista un .out con
 * justo ese caso. Lo que se prueba aquí es MÉTODO.
 *
 * El lector se prueba aparte, contra .out de verdad (test_outfile.c).
 */

#include <stdio.h>
#include <string.h>

#include "dictamen.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

/* La línea con ese título, o NULL. */
static const DxLinea *linea( const Dictamen *d, const char *t )
{
    int i;

    for ( i = 0; i < d->n; i++ )
        if ( strcmp( d->l[i].titulo, t ) == 0 ) return &d->l[i];
    return NULL;
}

/* Un .out leído, de mentira: los hechos mínimos para que haya dictamen. */
static void base( FueOut *o )
{
    memset( o, 0, sizeof *o );
    o->hay = 1;
    o->nobs = 200;
    o->tiene_res = 1;
    o->media = 0.0001; o->media_et = 0.001;     /* t = 0.1 */
    o->nlb = 2;
    o->lb_df_[0] = 12; o->lb_q_[0] = 10.0; o->lb_p_[0] = 0.60;
    o->lb_df_[1] = 24; o->lb_q_[1] = 20.0; o->lb_p_[1] = 0.70;
    o->tiene_jb = 1; o->jb = 1.0; o->jb_p = 0.60;
    o->npar_leidos = 2;
    o->par[0] = 0.5; o->par_et[0] = 0.1; o->par_estimado[0] = 1;  /* t = 5 */
    o->par[1] = 0.4; o->par_et[1] = 0.1; o->par_estimado[1] = 1;  /* t = 4 */
}

int main( void )
{
    FueOut      o;
    Convergence c;
    Dictamen    d;

    memset( &c, 0, sizeof c );

    printf( "UN MODELO QUE CUADRA, CUADRA\n" );
    base( &o );
    c.kind = CONV_GRADTOL; c.iterations = 12; c.brief = (gchar *) "converged";
    dx_dictamen( &o, &c, &d );
    ok( d.n == 5, "cinco bloques: estimación, media, autocorrelación, "
                  "normalidad y parámetros" );
    ok( linea( &d, "Autocorrelación" )->estado == DX_CUADRA, "los residuos son blancos" );
    ok( linea( &d, "Normalidad" )->estado == DX_CUADRA, "y normales" );
    ok( d.peor == DX_CUADRA, "y el resumen lo dice" );

    printf( "\nEL UMBRAL TIENE DOS ESCALONES, NO UNO\n" );
    /* Un p de .04 y uno de .000 no son la misma noticia; y uno de .07
       tampoco es un aprobado limpio.                                  */
    base( &o ); o.lb_p_[1] = 0.04;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Autocorrelación" )->estado == DX_NO, "p = .04 rechaza" );

    base( &o ); o.lb_p_[1] = 0.07;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Autocorrelación" )->estado == DX_MIRAR,
        "p = .07 no rechaza, pero no es lo mismo que .60" );

    printf( "\nEL PEOR PELDAÑO ES EL DIAGNOSTICO, Y SE DICE CUAL\n" );
    base( &o );
    o.lb_p_[0] = 0.001;                 /* cerca: Q(12) */
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Autocorrelación" )->estado == DX_NO, "manda el peor" );
    ok( strstr( linea( &d, "Autocorrelación" )->dice, "12" ) != NULL,
        "y se dice EN QUE retardo: cerca o lejos son dos problemas" );
    ok( strstr( linea( &d, "Autocorrelación" )->dato, "Q(24)" ) != NULL,
        "y la escalera entera sigue a la vista" );

    printf( "\nLO QUE NO CONSTA NO ES UN APROBADO\n" );
    base( &o );
    o.nlb = 0; o.tiene_jb = 0;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Autocorrelación" )->estado == DX_NO_CONSTA,
        "sin Ljung-Box en el informe, no consta" );
    ok( linea( &d, "Normalidad" )->estado == DX_NO_CONSTA,
        "sin Jarque-Bera, tampoco" );
    ok( d.peor != DX_CUADRA,
        "y el resumen NO puede decir que cuadra: no es lo mismo no saber "
        "que saber que está bien" );

    printf( "\nLA MEDIA CON mu ESTIMADA ES UNA TAUTOLOGIA, NO UN APROBADO\n" );
    base( &o );
    o.tiene_mu = 1; o.media = -0.000001; o.media_et = 0.000171;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Media" )->estado == DX_NO_CONSTA,
        "con mu, la media es cero POR CONSTRUCCION: no hay contraste" );
    ok( strstr( linea( &d, "Media" )->dice, "construcción" ) != NULL,
        "y se dice por qué, en vez de callarse" );

    base( &o );
    o.tiene_mu = 0; o.media = 0.005; o.media_et = 0.001;   /* t = 5 */
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Media" )->estado == DX_NO,
        "sin mu, el contraste vale -- y aquí la media NO es cero" );

    printf( "\nPARARSE SIN MEJORA NO ES FRACASAR\n" );
    base( &o );
    c.kind = CONV_NO_LOWER; c.brief = (gchar *) "stopped";
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Estimación" )->estado == DX_CUADRA,
        "es lo que sale cuando el .pre YA ERA el óptimo" );

    c.kind = CONV_MAXITS;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Estimación" )->estado == DX_NO,
        "pero quedarse sin iteraciones sí es no converger" );

    dx_dictamen( &o, NULL, &d );
    ok( linea( &d, "Estimación" )->estado == DX_NO_CONSTA,
        "y sin saber cómo acabó, no consta" );

    printf( "\nDOS PARAMETROS QUE SE PISAN SE ENUNCIAN, NO SE SENTENCIAN\n" );
    base( &o );
    c.kind = CONV_GRADTOL;
    o.npares = 1; o.par_a[0] = 2; o.par_b[0] = 3; o.par_r[0] = 0.93;
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Parámetros" )->estado == DX_MIRAR,
        "correlación alta: hay que mirarlo" );
    ok( strstr( linea( &d, "Parámetros" )->dice, "puede" ) != NULL,
        "pero NO se dicta que sobre uno: entre un AR y un MA puede ser la "
        "forma del modelo" );

    printf( "\nUN PARAMETRO QUE NO SE GANA SU SITIO\n" );
    base( &o );
    o.par[1] = 0.05; o.par_et[1] = 0.1;         /* t = 0.5 */
    dx_dictamen( &o, &c, &d );
    ok( linea( &d, "Parámetros" )->estado == DX_MIRAR, "se avisa" );
    ok( strstr( linea( &d, "Parámetros" )->dice, "[2]" ) != NULL,
        "y se dice cuál" );

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
