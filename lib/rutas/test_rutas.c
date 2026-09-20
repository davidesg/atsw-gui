/*
 * test_rutas.c -- los casos que de verdad fallaron, y los bordes.
 *
 * Los tres primeros no son inventados: son las tres invocaciones que se
 * reprodujeron en el inventario (INVENTARIO-madre.md §2).
 */

#include <stdio.h>
#include <string.h>

#include "rutas.h"

static int fallos = 0, pasos = 0;

static void es( const char *que, const char *dio, const char *debe )
{
    if ( strcmp( dio, debe ) == 0 ) { printf( "  ok    %s\n", que ); pasos++; }
    else {
        printf( "  FALLO %s\n        dio  \"%s\"\n        debe \"%s\"\n",
                que, dio, debe );
        fallos++;
    }
}

int main( void )
{
    char b[256];

    printf( "los tres fallos del inventario\n" );

    ruta_componer( "caso/X", "A", ".eps", b, sizeof b );
    es( "fue: el grafico de residuos de caso/X", b, "caso/AX.eps" );

    ruta_componer( "caso/X", "forecast_", ".inp", b, sizeof b );
    es( "fue -f: el fichero de prevision de caso/X", b, "caso/forecast_X.inp" );

    ruta_dir( "dt/M6_EP.pre", b, sizeof b );
    es( "drtran: el .out va al directorio de los datos", b, "dt" );

    printf( "\nlo que no debe cambiar: un nombre a secas\n" );

    ruta_componer( "X", "A", ".eps", b, sizeof b );
    es( "sin directorio, como lo usan fue y fug", b, "AX.eps" );

    ruta_componer( "X", NULL, ".out", b, sizeof b );
    es( "sin prefijo", b, "X.out" );

    ruta_componer( "X", "pre", NULL, b, sizeof b );
    es( "sin sufijo", b, "preX" );

    printf( "\nlos bordes\n" );

    ruta_componer( "/abs/caso/X", "A", ".eps", b, sizeof b );
    es( "ruta absoluta", b, "/abs/caso/AX.eps" );

    /* ruta_componer NO quita la extension, y es a proposito: sus usuarios le
     * pasan un nombre base que ya no la lleva (fue compone sobre x11out). Si
     * hay que quitarla, ruta_sin_ext primero -- explicito.               */
    ruta_componer( "caso/X.pre", "A", ".eps", b, sizeof b );
    es( "no quita la extension: eso es ruta_sin_ext", b, "caso/AX.pre.eps" );

    ruta_componer( "..\\caso\\X", "A", ".eps", b, sizeof b );
    es( "barra de Windows (la suite se compila con MXE)", b, "..\\caso\\AX.eps" );

    ruta_sin_ext( "caso/X.pre", b, sizeof b );
    es( "sin extension, CONSERVANDO el directorio", b, "caso/X" );

    ruta_sin_ext( "caso.v2/X", b, sizeof b );
    es( "un punto en el DIRECTORIO no es una extension", b, "caso.v2/X" );

    /* LA DISTINCION QUE COSTO 72 PRUEBAS DE ORO. En fue "DE.2" es el nombre
     * entero: el punto no es una extension.                             */
    ruta_base( "caso/DE.2", b, sizeof b );
    es( "ruta_base conserva un punto que NO es extension", b, "DE.2" );

    ruta_base( "DE.2", b, sizeof b );
    es( "y sin directorio no toca nada", b, "DE.2" );

    ruta_nombre( "caso/DE.2", b, sizeof b );
    es( "ruta_nombre SI se lo come: para eso esta el aviso", b, "DE" );

    ruta_nombre( "caso/M6_EP.pre", b, sizeof b );
    es( "el nombre a secas: es IDENTIDAD, no ruta", b, "M6_EP" );

    ruta_nombre( "M6_EP", b, sizeof b );
    es( "y sin extension ni directorio", b, "M6_EP" );

    ruta_dir( "X.pre", b, sizeof b );
    es( "sin directorio, el directorio es .", b, "." );

    ruta_dir( "/X.pre", b, sizeof b );
    es( "en la raiz", b, "/" );

    ruta_componer( ".oculto", "A", "", b, sizeof b );
    es( "un punto inicial es parte del nombre", b, "A.oculto" );

    printf( "\nel truncamiento avisa\n" );
    {
    char corto[8];
    int  r = ruta_componer( "caso/XXXXXXXXXX", "A", ".eps", corto, sizeof corto );

    es( "devuelve 1 si no cabe", r ? "1" : "0", "1" );
    es( "y deja la cadena terminada", corto[sizeof corto - 1] ? "no" : "si", "si" );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
