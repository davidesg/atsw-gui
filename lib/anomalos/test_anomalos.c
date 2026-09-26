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

    printf( "\nUN PERIODO TRANQUILO SEPARA DOS SUCESOS\n" );
    /* ESTA PRUEBA CAMBIO DE SIGNO A PROPOSITO.
     *
     * Antes habia un parametro de HUECO --dos extremos separados por hasta
     * dos periodos callados eran el mismo suceso-- y esta misma entrada
     * daba UN episodio de cuatro. Con el escaner no: un periodo por debajo
     * de una desviacion tipica parte el tramo.
     *
     * Y es mejor, que es lo que importa: (4.0, -3.5) es un par compensado
     * --un impulso de nivel-- y el 3.2 de tres periodos despues es otra
     * cosa. Leerlos juntos pediria cuatro escalones donde hacen falta dos
     * intervenciones distintas. El hueco era una convencion en periodos;
     * esto es una condicion sobre el dato.                            */
    memset( z, 0, sizeof z );
    z[10] = 4.0; z[11] = -3.5; z[13] = 3.2;      /* el 12 esta callado */
    z[50] = 5.0;                                  /* y este, aparte     */
    {
    int ne = an_episodios( z, 100, 3.0, ep, 16 );

    esn( ne, 3, "el par, el de despues y el de lejos: TRES sucesos" );
    esn( ep[0].desde, 10, "el par empieza en el 10" );
    esn( ep[0].n, 2, "  y dura dos: el hueco no se traga" );
    ok( fabs( ep[0].z_max - 4.0 ) < 1e-9, "  con su mayor |z| bien puesto" );
    esn( ep[1].desde, 13, "el de despues va solo" );
    esn( ep[2].desde, 50, "y el de lejos, tambien" );
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

    printf( "\nSE PUNTUA EL TRAMO, NO LOS PUNTOS\n" );
    /* EL CASO QUE LO DESTAPO, en IPC_ES m04: tres periodos contiguos con
       firma +,-,+ que el motor marca los tres y que punto a punto se leian
       como un suceso de uno.                                         */
    {
    AnEpisodio ep2[16];
    int        i, ne;
    double     u = an_umbral( 261 );

    for ( i = 0; i < 100; i++ ) z[i] = 0.0;
    z[50] = +3.01;
    z[51] = -3.50;
    z[52] = +2.36;

    ne = an_episodios( z, 100, u, ep2, 16 );
    esn( ne, 1, "un episodio" );
    esn( ep2[0].n, 3, "de TRES periodos, que es lo que hay" );
    esn( ep2[0].desde, 50, "empieza en el primero" );
    esn( ep2[0].hasta, 52, "y acaba en el tercero" );
    ok( ep2[0].z_max < -3.4, "el mayor sigue siendo el del medio, con su signo" );
    ok( ep2[0].p < 1e-5, "y es mucho mas improbable que un extremo suelto" );
    printf( "        p del tramo = %.2e\n", ep2[0].p );

    printf( "\nCON L = 1 ES EXACTAMENTE LA REGLA DE SIEMPRE\n" );
    /* Si esto falla, la regla nueva no contiene a la vieja y todo lo que
       se decidio con ella deja de valer.                             */
    {
    double lim = 3.45;             /* el umbral efectivo con K = 1.5 */

    for ( i = 0; i < 100; i++ ) z[i] = 0.0;
    z[30] = lim + 0.05;
    esn( an_episodios( z, 100, u, ep2, 16 ), 1, "justo por encima: episodio" );
    z[30] = lim - 0.05;
    esn( an_episodios( z, 100, u, ep2, 16 ), 0, "justo por debajo: no" );
    }

    printf( "\nUN 2 SIGMA SUELTO NO ES NADA, Y VARIOS TAMPOCO\n" );
    /* Doce falsos a 2 sigma se esperan en 261 observaciones: si el modulo
       los declarara, la lista seria inservible.                      */
    for ( i = 0; i < 100; i++ ) z[i] = 0.0;
    z[20] = 2.4;
    esn( an_episodios( z, 100, u, ep2, 16 ), 0, "uno de 2.4, nada" );
    z[21] = 2.2;
    esn( an_episodios( z, 100, u, ep2, 16 ), 0, "dos seguidos, tampoco" );
    z[22] = 2.3;
    esn( an_episodios( z, 100, u, ep2, 16 ), 0, "tres seguidos, tampoco" );
    z[23] = 2.5; z[24] = 2.4; z[25] = 2.6;
    ok( an_episodios( z, 100, u, ep2, 16 ) == 1,
        "pero seis seguidos SI: eso ya no es ruido" );

    printf( "\nEL TRAMO SE AJUSTA A LO QUE HAY, NI MAS NI MENOS\n" );
    for ( i = 0; i < 100; i++ ) z[i] = 0.0;
    z[40] = 5.0;                   /* uno enorme y solo */
    z[43] = 5.0;                   /* otro, a tres de distancia */
    ne = an_episodios( z, 100, u, ep2, 16 );
    esn( ne, 2, "dos extremos separados por silencio son DOS sucesos" );
    esn( ep2[0].n, 1, "cada uno de un periodo..." );
    esn( ep2[1].n, 1, "...y no un tramo que se traga el silencio" );
    ok( ep2[0].desde == 40 && ep2[1].desde == 43, "y en su sitio, por posicion" );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
