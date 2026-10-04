/*
 * test_datos.c -- que la puerta unica conteste UNA cosa, y la correcta.
 *
 * El caso que manda es el primero: el fichero de dos columnas con el que fue y
 * fug daban DOS SERIES DISTINTAS. Aqui se fija cual es la buena.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "datos.h"

static int fallos = 0;
static char DIR[256];

static void ok( int c, const char *que )
{
    printf( c ? "  ok    %s\n" : "  FALLO %s\n", que );
    if ( !c ) fallos++;
}

static void cerca( double dio, double debe, const char *que )
{
    int c = fabs( dio - debe ) < 1e-9;

    printf( c ? "  ok    %s\n" : "  FALLO %s  (dio %g, debe %g)\n",
            que, dio, debe );
    if ( !c ) fallos++;
}

/* Escribe un fichero de prueba y devuelve su ruta (estatica). */
static const char *pon( const char *nombre, const char *contenido )
{
    static char path[512];
    FILE       *f;

    snprintf( path, sizeof path, "%s/%s", DIR, nombre );
    f = fopen( path, "w" );
    if ( f ) { fputs( contenido, f ); fclose( f ); }
    return path;
}

int main( int argc, char **argv )
{
    static DtDatos d;            /* 2,2 MB: en la pila de Windows (1 MB) no cabe */
    DtError e;
    char    b[256];

    snprintf( DIR, sizeof DIR, "%s", argc > 1 ? argv[1] : "." );

    /* ------------------------------------------------------------------ */
    printf( "EL CASO QUE DIVIDIA A fue Y A fug: dos columnas\n" );
    printf( "   fug las APLANABA en un vector; fue tomaba la 1.\n" );
    {
    const char *p = pon( "dos.txt", "1.0 10.0\n2.0 20.0\n3.0 30.0\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee" );
    ok( d.ncol == 2, "DOS columnas, no una serie de seis" );
    ok( d.nobs == 3, "tres observaciones" );
    cerca( d.v[0][0], 1.0, "la columna 1 es la SERIE: 1.0" );
    cerca( d.v[0][2], 3.0, "y acaba en 3.0" );
    cerca( d.v[1][0], 10.0, "la columna 2 es un REGRESOR: 10.0" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\nla cabecera se acepta Y SE USA para nombrar\n" );
    printf( "   fug la saltaba; fue fallaba con ella.\n" );
    {
    const char *p = pon( "cab.txt", "IPC  WTI\n101.2  70.1\n102.0  71.4\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee un fichero CON cabecera" );
    ok( d.tiene_cabecera, "y se sabe que la tenia" );
    ok( strcmp( d.nombre[0], "IPC" ) == 0, "la serie se llama IPC" );
    ok( strcmp( d.nombre[1], "WTI" ) == 0, "y el regresor, WTI" );
    ok( d.nobs == 2, "la cabecera NO cuenta como observacion" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\nun CSV de verdad, con comas\n" );
    printf( "   el filtro de fug anunciaba *.csv y un CSV de verdad fallaba.\n" );
    {
    const char *p = pon( "coma.csv", "IPC,WTI\n101.2,70.1\n102.0,71.4\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee" );
    ok( d.sep == ',', "el separador es la coma" );
    ok( d.dec == '.', "y entonces el decimal TIENE que ser el punto" );
    cerca( d.v[0][0], 101.2, "101.2" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\ncoma decimal, que es lo normal aqui\n" );
    {
    const char *p = pon( "eu.csv", "IPC;WTI\n101,2;70,1\n102,0;71,4\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee con ';' y coma decimal" );
    ok( d.sep == ';', "separador ';'" );
    ok( d.dec == ',', "decimal ','" );
    cerca( d.v[0][0], 101.2, "101,2 es 101.2" );
    cerca( d.v[1][1], 71.4,  "71,4 es 71.4" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\nLA FRECUENCIA Y LA FECHA, DEL FICHERO\n" );
    printf( "   hoy salen de un combo, con defaults distintos por programa:\n" );
    printf( "   fug freq=1; fue y drvarma freq=12 y año 2000.\n" );
    {
    /* el convenio de "drtran -e" */
    const char *p = pon( "cab.dat",
        "# freq 4\n# start 1/1977\n20.6\n21.1\n22.0\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee con cabecera de '#'" );
    ok( d.freq == 4, "la frecuencia sale del fichero: 4" );
    ok( d.per == 1 && d.anio == 1977, "y la fecha: 1/1977" );
    }
    {
    /* una columna de fechas, que es lo que trae un CSV de verdad */
    const char *p = pon( "fechas.csv",
        "fecha,IPC\n1/1977,20.6\n2/1977,21.1\n3/1977,22.0\n4/1977,22.5\n"
        "1/1978,23.0\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee con columna de fechas" );
    ok( d.tiene_fechas, "y se sabe que la traia" );
    ok( d.ncol == 1, "la columna de fechas NO es una serie" );
    ok( strcmp( d.nombre[0], "IPC" ) == 0,
        "y los nombres se corren: la serie es IPC" );
    ok( d.per == 1 && d.anio == 1977, "arranca en 1/1977" );
    ok( d.freq == 4, "y la frecuencia se DEDUCE del salto de año: 4" );
    cerca( d.v[0][0], 20.6, "el primer dato" );
    ok( strcmp( d.fecha[4], "1/1978" ) == 0, "la ultima fecha" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\ncuando el fichero NO lo dice, se dice que no lo dice\n" );
    {
    const char *p = pon( "muda.txt", "1.0\n2.0\n3.0\n" );

    ok( dt_leer( p, &d, &e ) == 0, "se lee" );
    ok( d.freq == 0 && d.anio == 0,
        "freq y año salen a 0: NO se inventa un default" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\nel error es un HECHO, y cada idioma lo redacta\n" );
    {
    const char *p = pon( "mala.txt", "1.0 2.0\n3.0 hola\n" );

    ok( dt_leer( p, &d, &e ) != 0, "un campo que no es numero falla" );
    ok( e.cod == DT_EVALOR, "y el codigo lo dice" );
    ok( e.linea == 2 && e.campo == 2, "con la linea y el campo" );
    ok( strcmp( e.texto, "hola" ) == 0, "y el campo que fallo" );

    dt_error_es( &e, b, sizeof b );
    ok( strstr( b, "no es un número" ) != NULL, "en castellano para el GUI" );
    dt_error_en( &e, b, sizeof b );
    ok( strstr( b, "is not a number" ) != NULL, "en ingles para el motor" );
    }
    {
    const char *p = pon( "corta.txt", "1.0 2.0\n3.0\n" );

    ok( dt_leer( p, &d, &e ) != 0, "una fila con menos campos falla" );
    ok( e.cod == DT_ECOLS, "y se distingue del anterior" );
    ok( e.esperaba == 2 && e.encontro == 1, "dice cuantos habia y cuantos hay" );
    }
    {
    ok( dt_leer( "/no/existe/xyz", &d, &e ) != 0, "un fichero que no esta" );
    ok( e.cod == DT_ENOFILE, "se distingue de un fichero malo" );
    }
    {
    const char *p = pon( "vacia.txt", "\n\n   \n" );

    ok( dt_leer( p, &d, &e ) != 0, "un fichero sin numeros falla" );
    ok( e.cod == DT_EVACIO, "y lo dice" );
    }

    /* ------------------------------------------------------------------ */
    printf( "\nUN .xlsx, SI HAY UNO A MANO\n" );
    printf( "   Se reconoce POR SU CONTENIDO (PK\\3\\4), no por la extensión.\n" );
    if ( argc > 2 )
        {
        ok( dt_leer( argv[2], &d, &e ) == 0, "se lee el libro" );
        ok( d.ncol >= 1, "y trae columnas" );
        ok( d.nobs > 2, "y observaciones" );
        /* LA FRECUENCIA SALE DEL SALTO DE MESES. Una fecha de Excel es un
         * numero con estilo de fecha; con fechas de fin de mes el salto en
         * DIAS no dice nada (31, 29, 31) y el de MESES si.             */
        ok( d.freq == 12 || d.freq == 4 || d.freq == 1,
            "y una frecuencia reconocible, del salto de MESES" );
        ok( d.anio > 1800 && d.anio < 2200, "y un año que tiene sentido" );
        }
    else
        printf( "  (sin .xlsx de prueba: pásame uno como segundo argumento)\n" );

    /* Un texto llamado .xlsx NO se toma por un libro. */
    {
    const char *p = pon( "mentira.xlsx", "1.0\n2.0\n3.0\n" );

    ok( dt_leer( p, &d, &e ) == 0 && d.nobs == 3,
        "un texto llamado .xlsx se lee como texto: manda el CONTENIDO" );
    }

    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
