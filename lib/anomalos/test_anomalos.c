/*
 * test_anomalos.c -- los episodios y la calibración, con series construidas.
 *
 * Las series se fabrican con un generador propio y determinista: la prueba
 * tiene que dar lo mismo en cualquier máquina, y rand() no lo garantiza.
 *
 * Y se construyen los DOS casos que dan sentido al módulo -- una señal
 * FABRICADA por el anómalo y una ENMASCARADA por él -- porque calibrar en un
 * solo sentido es lo que deja media identificación a ciegas.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "anomalos.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static void esn( int dio, int debe, const char *que )
{
    int c = ( dio == debe );

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  %d\n"
                                 "        debe %d\n", que, dio, debe );
    if ( !c ) fallos++;
}

/* Un generador propio: determinista y portátil. */
static unsigned long semilla = 20260925UL;

static double ruido( void )
{
    double u, v;

    semilla = semilla * 1103515245UL + 12345UL;
    u = (double) ( ( semilla >> 16 ) & 0x7fff ) / 32768.0 + 1e-9;
    semilla = semilla * 1103515245UL + 12345UL;
    v = (double) ( ( semilla >> 16 ) & 0x7fff ) / 32768.0;
    return sqrt( -2.0 * log( u ) ) * cos( 6.28318530718 * v );
}

int main( void )
{
    double     z[400], e[401];
    AnEpisodio ep[16];
    AnCalibra  c;
    int        omit[8], i, n = 300;

    printf( "EL UMBRAL DEPENDE DE n\n" );
    ok( an_umbral( 80 ) > 2.5 - 1e-9, "con 80 observaciones, al menos 2.5" );
    ok( an_umbral( 600 ) > an_umbral( 80 ),
        "y con 600 es MAS ALTO: el máximo de n normales crece con n" );
    ok( an_umbral( 10 ) >= 2.5,
        "con muy pocas no baja del suelo: si no, todo el mundo es noticia" );

    printf( "\nUN SUCESO NO SON TRES ATIPICOS\n" );
    memset( z, 0, sizeof z );
    z[10] = 4.0; z[11] = -3.5; z[13] = 3.2;      /* hueco de 1 */
    z[50] = 5.0;                                  /* aparte     */
    {
    int ne = an_episodios( z, 100, 3.0, AN_VENTANA, ep, 16 );

    esn( ne, 2, "tres extremos con un hueco de 1 y uno lejos: DOS episodios" );
    esn( ep[0].n, 3, "  el primero lleva los tres" );
    esn( ep[0].desde, 10, "  desde el 10" );
    esn( ep[0].hasta, 13, "  hasta el 13" );
    esn( ep[0].i_max, 50 > 0 ? 50 - 40 : 0, "  (colocación)" );
    ok( fabs( ep[0].z_max - 4.0 ) < 1e-9, "  y el mayor |z| es el 4.0" );
    esn( ep[1].n, 1, "el de lejos va solo" );
    }
    {
    /* CON VENTANA 0 SON TRES, y eso es lo que hacía la comprobación de
       adyacencia: un suceso de tres períodos, tres atípicos sueltos. */
    int ne = an_episodios( z, 100, 3.0, 0, ep, 16 );

    esn( ne, 3, "con ventana 0 el hueco los separa: la ventana es lo que "
                "convierte tres extremos en un suceso" );
    }

    printf( "\nUNA SEÑAL FABRICADA POR EL ANOMALO\n" );
    /* Ruido blanco -- sin estructura -- y un par de picos contiguos de
       signo opuesto, que fabrican una r(1) muy negativa.             */
    semilla = 20260925UL;
    for ( i = 0; i < n; i++ ) z[i] = ruido();
    z[150] += 9.0; z[151] -= 9.0;

    omit[0] = 150; omit[1] = 151;
    ok( an_calibra( z, n, omit, 2, 12, &c ) == 0, "calibra" );
    ok( c.l[0].acf == AN_FABRICADA,
        "la r(1) que se ve NO EXISTE sin el anómalo: fabricada" );
    ok( fabs( c.l[0].acf_con ) > c.banda_con,
        "  con él se sale de banda" );
    ok( fabs( c.l[0].acf_sin ) < c.banda_sin,
        "  y sin él se queda dentro" );
    ok( c.cambia > 0, "y el resumen dice que SÍ cambia algo" );

    printf( "\nUNA SEÑAL ENMASCARADA POR EL ANOMALO\n" );
    /* Un MA(1) de verdad -- r(1) del orden de 0,45 -- y un pico enorme que
       infla la varianza y encoge TODAS las r(k) hacia cero. Es el mecanismo
       del enmascaramiento: el anómalo no borra la señal, la diluye.   */
    semilla = 777UL;
    for ( i = 0; i <= n; i++ ) e[i] = ruido();
    for ( i = 0; i < n; i++ ) z[i] = e[i + 1] + 0.9 * e[i];
    /* El pico tiene que inflar la varianza lo bastante para meter la r(1)
       DENTRO de banda: con 40 la encogía de 0,50 a 0,13 y la banda está en
       0,113 -- se quedaba fuera por poco, y entonces el veredicto correcto
       es «igual», porque la decisión de identificación no cambia.     */
    z[100] += 70.0;

    omit[0] = 100;
    ok( an_calibra( z, n, omit, 1, 12, &c ) == 0, "calibra" );
    ok( fabs( c.l[0].acf_sin ) > fabs( c.l[0].acf_con ),
        "sin el anómalo la r(1) es MAYOR: la estaba tapando" );
    ok( fabs( c.l[0].acf_con ) < c.banda_con,
        "  con él, la r(1) se queda DENTRO de banda: no se ve" );
    ok( fabs( c.l[0].acf_sin ) > c.banda_sin,
        "  y sin él se sale: la señal estaba ahí" );
    ok( c.l[0].acf == AN_ENMASCARADA,
        "y eso es una señal MA enmascarada -- FALTA, no sobra" );

    printf( "\nY LA PACF NO SALE DE MIRAR LA ACF\n" );
    /* Durbin-Levinson sobre un AR(1): la PACF tiene que ser rho en el 1 y
       practicamente cero despues. Si esto falla, todo lo de arriba miente. */
    semilla = 4242UL;
    for ( i = 0; i < n; i++ ) e[i] = ruido();
    z[0] = e[0];
    for ( i = 1; i < n; i++ ) z[i] = 0.7 * z[i - 1] + e[i];
    an_calibra( z, n, NULL, 0, 10, &c );
    ok( fabs( c.l[0].pacf_con - c.l[0].acf_con ) < 1e-9,
        "en el retardo 1, PACF = ACF, siempre" );
    ok( fabs( c.l[1].pacf_con ) < 0.15,
        "y en un AR(1) la PACF se corta en el 2, aunque la ACF siga alta" );
    ok( fabs( c.l[1].acf_con ) > 0.3,
        "  (la ACF del 2 sigue alta: son funciones distintas)" );

    printf( "\nUN EXTREMO MODESTO NO COMPRA NADA\n" );
    /* Lo que se afirma es del RETARDO, no del recuento: con doce retardos y
       una banda al 95%, uno justo en el filo puede cruzarla porque quitar
       observaciones ENSANCHA la banda -- y eso es azar, no señal. Afirmar
       «cambia == 0» sería afirmar algo que el azar decide.           */
    semilla = 99UL;
    for ( i = 0; i < n; i++ ) z[i] = ruido();
    z[200] += 3.2;                       /* un extremo modesto y solo */
    omit[0] = 200;
    an_calibra( z, n, omit, 1, 12, &c );
    ok( c.l[0].acf == AN_IGUAL && c.l[0].pacf == AN_IGUAL,
        "el retardo 1 no cambia de lado: quitarlo no compra identificación" );
    ok( c.cambia <= 2, "y casi ningún retardo se mueve de lado" );

    printf( "\nY EL Q TAMBIEN CAMBIA\n" );
    /* Con el par de picos que FABRICA la r(1), el Q tiene que bajar mucho
       al quitarlos: es la misma noticia contada con un solo numero.   */
    semilla = 20260925UL;
    for ( i = 0; i < n; i++ ) z[i] = ruido();
    z[150] += 9.0; z[151] -= 9.0;
    omit[0] = 150; omit[1] = 151;
    an_calibra( z, n, omit, 2, 12, &c );
    {
    double qc = 0.0, qs = 0.0;

    an_q( &c, 12, &qc, &qs );
    ok( qc > 0.0 && qs > 0.0, "se calcula con y sin" );
    ok( qs < qc, "y quitando lo que fabricaba la r(1), el Q BAJA" );
    printf( "        Q(12) con = %.1f, sin = %.1f\n", qc, qs );
    }

    printf( "\nLA BANDA SE ENSANCHA AL QUITAR OBSERVACIONES\n" );
    ok( c.banda_sin > c.banda_con,
        "quitar una observación ensancha la banda, y por eso se comparan "
        "DOS bandas y no una" );

    printf( "\nLA NORMALIDAD: OTRO ESTIMADOR, Y POR ESO\n" );
    /* Un atipico dispara la curtosis. Quitarlo tiene que BAJARLA -- y si se
       rellenara el hueco con un cero, la SUBIRIA, porque un cero exacto en
       el centro es una punta que no estaba. Ahi esta el motivo de que la
       ACF y la normalidad no compartan estimador.                   */
    {
    AnNormal con, sinellos;
    int      i, omit2[2];

    semilla = 20260926UL;
    for ( i = 0; i < n; i++ ) z[i] = ruido();
    z[100] += 12.0;                       /* un atipico grande y solo */
    omit2[0] = 100;

    ok( an_normalidad( z, n, omit2, 1, &con, &sinellos ) == 0, "se calcula" );
    ok( sinellos.n == con.n - 1, "el «sin» tiene una observacion menos" );
    ok( con.kurt > sinellos.kurt,
        "quitar el atipico BAJA la curtosis, que es lo que tiene que pasar" );
    ok( con.jb > sinellos.jb, "y con ella baja el Jarque-Bera" );
    printf( "        con: JB %.1f (S %+.2f, K %+.2f, n %d)\n",
            con.jb, con.skew, con.kurt, con.n );
    printf( "        sin: JB %.1f (S %+.2f, K %+.2f, n %d)\n",
            sinellos.jb, sinellos.skew, sinellos.kurt, sinellos.n );

    /* Y LA PRUEBA DEL ESTIMADOR: rellenar con cero en vez de quitar daria
       una curtosis MAYOR que la de verdad.                          */
    {
    double relleno[600];
    AnNormal falso, basura;

    for ( i = 0; i < n; i++ ) relleno[i] = z[i];
    relleno[100] = sinellos.media;        /* la desviacion a cero */
    an_normalidad( relleno, n, NULL, 0, &falso, &basura );
    ok( falso.kurt > sinellos.kurt,
        "rellenar el hueco deja MAS curtosis que quitarlo: por eso no se "
        "rellena" );
    printf( "        rellenando: K %+.2f  frente a %+.2f quitando\n",
            falso.kurt, sinellos.kurt );
    }

    /* Sin nada omitido, los dos son el mismo */
    an_normalidad( z, n, NULL, 0, &con, &sinellos );
    ok( con.jb == sinellos.jb && con.n == sinellos.n,
        "sin omitir nada, «con» y «sin» son el mismo numero" );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
