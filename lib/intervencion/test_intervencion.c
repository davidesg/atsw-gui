/*
 * test_intervencion.c -- el diccionario de la FLT, en los dos sentidos.
 *
 * Lo que hay que comprobar no es que el código corra: es que la MISMA firma
 * se lea distinto según la d, porque ahí está todo el contenido del módulo.
 * Un par compensado es un impulso en ∇ y no significa nada en el nivel; un
 * extremo solo es un escalón en ∇ y un impulso en el nivel. Si esas cuatro
 * casillas están bien, el módulo está bien.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "intervencion.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static void es_forma( const IvLectura *l, IvForma debe, const char *que )
{
    int c = ( l->forma == debe );

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  %s\n"
                                 "        debe %s\n", que,
            iv_nombre_es( l->forma ), iv_nombre_es( debe ) );
    if ( !c ) fallos++;
}

int main( void )
{
    IvLectura l;
    IvExtremo e[8];

    printf( "\nCON d >= 1 LOS RESIDUOS VIVEN EN EL OPERADOR\n" );

    e[0].obs = 100; e[0].z = -4.10;
    e[1].obs = 101; e[1].z = +3.98;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_IMPULSO, "dos extremos contiguos que se cancelan: IMPULSO" );
    ok( strstr( l.razon, "CANCELAN" ) != NULL, "y la razón lo dice con el número" );
    ok( l.peldano == 1, "la lectura escalar basta" );
    printf( "        %s\n", l.razon );

    e[0].obs = 100; e[0].z = +3.21;
    e[1].obs = 101; e[1].z = -1.02;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_ESCALON, "dos contiguos de signo opuesto que NO se cancelan: ESCALON" );
    ok( strstr( l.razon, "cola del suceso" ) != NULL,
        "y dice que el segundo es COLA, no la mitad compensadora" );
    printf( "        %s\n", l.razon );

    e[0].obs = 100; e[0].z = +3.21;
    iv_lectura( e, 1, 1, &l );
    es_forma( &l, IV_ESCALON, "un extremo aislado en ∇: ESCALON de nivel" );
    ok( l.peldano == 1, "y basta con la lectura escalar" );

    e[0].obs = 100; e[0].z = +3.0;
    e[1].obs = 101; e[1].z = +2.9;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_ESCALON, "dos contiguos del MISMO signo: ESCALON" );
    ok( l.peldano == 2, "pero no lo resuelve: peldaño 2" );

    e[0].obs = 100; e[0].z = -4.0;
    e[1].obs = 105; e[1].z = +4.0;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_ESCALON, "dos que se cancelan pero NO son contiguos: no es un impulso" );
    ok( l.peldano == 2, "son dos sucesos, no uno: peldaño 2" );

    e[0].obs = 100; e[0].z = -4.0;
    e[1].obs = 101; e[1].z = +3.0;
    e[2].obs = 102; e[2].z = -3.5;
    iv_lectura( e, 3, 1, &l );
    es_forma( &l, IV_ESCALON, "tres extremos: la escalar es el escalón" );
    ok( l.peldano == 2 && l.aviso[0], "y avisa de que la forma está más arriba" );

    printf( "\nCON d = 0 EL DICCIONARIO SE INVIERTE\n" );

    e[0].obs = 100; e[0].z = +4.0;
    iv_lectura( e, 1, 0, &l );
    es_forma( &l, IV_IMPULSO, "un extremo solo EN EL NIVEL: IMPULSO" );
    ok( strstr( l.razon, "NIVEL" ) != NULL, "y la razón dice que no se ha diferenciado" );

    e[0].obs = 100; e[0].z = +3.0;
    e[1].obs = 101; e[1].z = +2.8;
    e[2].obs = 102; e[2].z = +3.4;
    iv_lectura( e, 3, 0, &l );
    es_forma( &l, IV_ESCALON, "una racha del mismo signo en el nivel: ESCALON" );
    ok( l.peldano == 1, "y eso sí lo resuelve" );

    e[0].obs = 100; e[0].z = +3.0;
    e[1].obs = 101; e[1].z = -2.8;
    iv_lectura( e, 2, 0, &l );
    es_forma( &l, IV_ESCALON, "signos mezclados en el nivel: escalón por defecto" );
    ok( l.peldano == 2, "pero dicho como lo que es: la de menos compromiso" );

    printf( "\nLA MISMA FIRMA, LEIDA AL REVES SEGUN LA d\n" );
    /* ES EL PUNTO DEL MODULO: el par compensado es un IMPULSO en ∇ y el
       extremo solo es un ESCALON; sin diferenciar, justo al contrario. */
    e[0].obs = 100; e[0].z = -4.10;
    e[1].obs = 101; e[1].z = +3.98;
    iv_lectura( e, 2, 1, &l );
    ok( l.forma == IV_IMPULSO, "par compensado con d=1 -> impulso" );
    iv_lectura( e, 2, 0, &l );
    ok( l.forma == IV_ESCALON, "el MISMO par con d=0 -> ya no es un impulso" );

    e[0].obs = 100; e[0].z = +4.0;
    iv_lectura( e, 1, 1, &l );
    ok( l.forma == IV_ESCALON, "extremo solo con d=1 -> escalón" );
    iv_lectura( e, 1, 0, &l );
    ok( l.forma == IV_IMPULSO, "el MISMO extremo con d=0 -> impulso" );

    printf( "\nEL ORDEN DE LOS EXTREMOS NO PUEDE CAMBIAR LA LECTURA\n" );
    e[0].obs = 101; e[0].z = +3.98;
    e[1].obs = 100; e[1].z = -4.10;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_IMPULSO, "desordenados, la misma lectura" );
    ok( iv_primer_extremo( e, 2 ) == 1, "y el primer extremo es el de la obs menor" );

    printf( "\nEL UMBRAL DE CANCELACION ES UNA CONVENCION DECLARADA\n" );
    /* justo en el borde: resto = 35 % del pico, que ENTRA (<=) */
    e[0].obs = 100; e[0].z = -1.00;
    e[1].obs = 101; e[1].z = +0.65;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_IMPULSO, "resto exactamente en el 35 % del pico: entra" );
    e[1].z = +0.60;
    iv_lectura( e, 2, 1, &l );
    es_forma( &l, IV_ESCALON, "un poco más de resto: ya no cancela" );

    printf( "\nLA LINEA DEL .inp, COMO LA LEE EL MOTOR\n" );
    {
    char b[64];

    ok( iv_linea( IV_ESCALON, 12, 10, 2008, b, sizeof b ) == 0 &&
        strcmp( b, "step 10 2008" ) == 0, "mensual: «step 10 2008»" );
    ok( iv_linea( IV_IMPULSO, 4, 3, 2020, b, sizeof b ) == 0 &&
        strcmp( b, "impulse 3 2020" ) == 0, "trimestral: «impulse 3 2020»" );
    ok( iv_linea( IV_ESCALON, 1, 1, 1766, b, sizeof b ) == 0 &&
        strcmp( b, "step 1766" ) == 0, "anual: el año SOLO, sin período" );
    ok( iv_linea( IV_ESCALON, 12, 13, 2008, b, sizeof b ) != 0,
        "un período fuera de la frecuencia se rechaza, no se escribe" );
    }

    printf( "\nLA HUELLA ES EL DICCIONARIO, CALCULADO\n" );
    /* NO ES UNA TABLA APARTE: la huella es (1-B)^d (1-B^s)^D aplicado al
       regresor de nivel, y de ahi SALE el diccionario. Si esto cuadra, la
       regla de arriba no es una convencion: es una cuenta.            */
    {
    double h[24];
    int    i, base = 5, t0 = 10;

    iv_huella( IV_ESCALON, t0, 1, 0, 12, base, h, 24 );
    ok( h[t0-base] == 1.0, "escalon con d=1: UN pico de +1 en T" );
    {
    double suma = 0.0;

    for ( i = 0; i < 24; i++ ) suma += h[i];
    ok( suma == 1.0, "y la suma de la huella es 1 -- no revierte" );
    }

    iv_huella( IV_IMPULSO, t0, 1, 0, 12, base, h, 24 );
    ok( h[t0-base] == 1.0 && h[t0-base+1] == -1.0,
        "impulso con d=1: DOS picos, +1 y -1" );
    {
    double suma = 0.0;

    for ( i = 0; i < 24; i++ ) suma += h[i];
    ok( suma == 0.0, "y suman CERO -- revierte, que es la definicion" );
    }

    iv_huella( IV_ESCALON, t0, 0, 0, 12, base, h, 24 );
    ok( h[t0-base] == 1.0 && h[t0-base+1] == 1.0,
        "escalon con d=0: se queda arriba, no es un pico" );

    iv_huella( IV_ESCALON, t0, 1, 1, 12, base, h, 24 );
    ok( h[t0-base] == 1.0 && h[t0-base+12] == -1.0,
        "escalon con d=1 y D=1: el pico se REPITE a los 12, cambiado de signo" );
    ok( h[t0-base+1] == 0.0, "y entre medias no deja nada" );
    }

    printf( "\nLOS TRES NUMEROS SEPARAN TRES PREGUNTAS\n" );
    {
    double h[16], z[16];
    IvAjuste aj;
    int      i, base = 0, t0 = 6;

    /* un escalon de nivel de tamaño 3, visto en ∇, y nada mas */
    iv_huella( IV_ESCALON, t0, 1, 0, 12, base, h, 16 );
    for ( i = 0; i < 16; i++ ) z[i] = 3.0 * h[i];
    iv_ajusta( h, z, 16, &aj );
    ok( aj.escala > 2.99 && aj.escala < 3.01, "la escala es el tamaño del suceso" );
    ok( aj.r2 > 0.999, "y el R2 es 1: la forma lo explica entero" );
    ok( aj.resto < 1e-9 && aj.resto > -1e-9, "no queda nada" );

    /* la forma EQUIVOCADA: el dato es un impulso y se prueba un escalon */
    iv_huella( IV_IMPULSO, t0, 1, 0, 12, base, h, 16 );
    for ( i = 0; i < 16; i++ ) z[i] = 4.0 * h[i];
    iv_huella( IV_ESCALON, t0, 1, 0, 12, base, h, 16 );
    iv_ajusta( h, z, 16, &aj );
    ok( aj.r2 < 0.6, "con el perfil equivocado el R2 se cae" );
    ok( aj.resto <= -3.0,
        "y SOBREVIVE el vecino que la forma no explica: el criterio de "
        "Treadway, visto antes de estimar" );
    printf( "        escala %.2f, R2 %.2f, mayor resto %+.2f en %d\n",
            aj.escala, aj.r2, aj.resto, aj.i_resto );
    }

    printf( "\nLA DURACION DEL SUCESO NO ES LA QUE SE VE\n" );
    /* L impulsos en el NIVEL se ven como L+d extremos. Contar sobre los
       residuos pedia un escalon de mas por cada orden de diferenciacion. */
    {
    IvExtremo e2[4];

    e2[0].obs = 100; e2[0].z = +3.0;
    e2[1].obs = 101; e2[1].z = -1.0;
    ok( iv_duracion_nivel( e2, 2, 1 ) == 1,
        "dos extremos contiguos con d=1: UN periodo alterado en el nivel" );
    ok( iv_duracion_nivel( e2, 2, 0 ) == 2,
        "los mismos dos sin diferenciar: DOS" );
    e2[2].obs = 102; e2[2].z = +2.9;
    ok( iv_duracion_nivel( e2, 3, 1 ) == 2, "tres con d=1: dos en el nivel" );
    e2[0].obs = 100; e2[1].obs = 100; e2[2].obs = 100;
    ok( iv_duracion_nivel( e2, 3, 2 ) == 1, "nunca menos de uno" );
    }

    printf( "\nEL PELDAÑO 2 ES UNA «step» CON L+1 OMEGAS, NO L+1 «step»\n" );
    {
    double H[3*24], z[24], coef[3];
    IvAjuste aj;
    int      i, base = 0, t0 = 8;

    /* un suceso de DOS periodos en el nivel: sube 3 en T y baja 1 en T+1.
       Ninguna forma escalar puede con eso; la general, si.            */
    iv_huella_esc( t0, 2, 1, 0, 12, base, H, 24 );
    for ( i = 0; i < 24; i++ ) z[i] = 3.0 * H[i] - 1.0 * H[24 + i];

    iv_ajusta_esc( H, 1, z, 24, &aj, NULL );
    ok( fabs( aj.resto ) > 0.5,
        "con UN escalon queda un vecino: la forma se queda corta" );
    printf( "        escalar:  R2 %.2f, mayor resto %+.2f\n", aj.r2, aj.resto );

    iv_ajusta_esc( H, 2, z, 24, &aj, coef );
    ok( aj.r2 > 0.999 && fabs( aj.resto ) < 1e-9,
        "con DOS escalones no queda nada" );
    ok( coef[0] > 2.99 && coef[0] < 3.01 && coef[1] > -1.01 && coef[1] < -0.99,
        "y los coeficientes son los que se metieron" );
    printf( "        episodio: R2 %.2f, mayor resto %+.2f, coef %+.2f %+.2f\n",
            aj.r2, aj.resto, coef[0], coef[1] );

    /* con nesc == 1 tiene que dar EXACTAMENTE lo de iv_ajusta */
    {
    double h[24];
    IvAjuste a1, a2;

    iv_huella( IV_ESCALON, t0, 1, 0, 12, base, h, 24 );
    iv_ajusta( h, z, 24, &a1 );
    iv_ajusta_esc( h, 1, z, 24, &a2, NULL );
    ok( fabs( a1.escala - a2.escala ) < 1e-12 && fabs( a1.r2 - a2.r2 ) < 1e-12,
        "un escalon por las dos vias da lo mismo" );
    }
    }

    printf( "\nSUBIR DE PELDAÑO NO LO DECIDE EL AJUSTE\n" );
    {
    IvPlan   pl;
    IvExtremo e3[4];

    /* razon 1: el episodio dura mas de un periodo EN EL NIVEL */
    e3[0].obs = 100; e3[0].z = +3.0;
    e3[1].obs = 101; e3[1].z = +2.8;
    e3[2].obs = 102; e3[2].z = -2.7;
    iv_plan( e3, 3, 1, 0.0, 0.0, &pl );
    ok( pl.peldano == 2 && pl.nesc == 3,
        "tres extremos con d=1: dos periodos en el nivel, TRES escalones" );
    ok( pl.subir[0] != 0, "y se dice por que" );
    printf( "        %s\n", pl.subir );

    /* razon 2: Treadway -- la forma escalar deja un vecino */
    e3[0].obs = 100; e3[0].z = +3.2;
    e3[1].obs = 101; e3[1].z = -1.0;
    iv_plan( e3, 2, 1, 0.0, 2.5, &pl );
    ok( pl.peldano == 1 && pl.nesc == 1,
        "sin resto que sobreviva, la escalar basta" );
    iv_plan( e3, 2, 1, -2.9, 2.5, &pl );
    ok( pl.peldano == 2 && pl.nesc == 2,
        "con un resto de -2.9 sobre umbral 2.5, se sube: es Treadway" );
    ok( strstr( pl.subir, "Treadway" ) != NULL, "y se dice cual de las razones" );
    printf( "        %s\n", pl.subir );

    /* y el peldaño 1 sigue diciendo la forma escalar */
    ok( pl.forma == IV_ESCALON, "la lectura escalar no se pierde al subir" );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
