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

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
