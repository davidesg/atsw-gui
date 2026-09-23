/*
 * test_genera.c -- el dato y lo que se deriva de el.
 *
 * Dos cosas, y las dos son la misma regla vista por sus dos lados:
 *
 *   1. EL IDA Y VUELTA. Escribir un .csv y volver a leerlo tiene que dar los
 *      mismos numeros, la misma frecuencia y las mismas fechas. Es lo que
 *      permite decir que el .csv es el dato: si el viaje pierde algo, no lo
 *      es.
 *
 *   2. LA VENTANA. "hasta 12/2019" tiene que traducirse al numero de
 *      observaciones correcto. Es lo unico de generar un .inp que hay que
 *      hacer bien -- lo demas es copiar.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "datos.h"
#include "atsw.h"



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

static void es( const char *dio, const char *debe, const char *que )
{
    int c = strcmp( dio, debe ) == 0;

    printf( c ? "  ok    %s\n" : "  FALLO %s\n        dio  \"%s\"\n"
                                 "        debe \"%s\"\n", que, dio, debe );
    if ( !c ) fallos++;
}

int main( int argc, char **argv )
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char        path[1024], b[32];
    DtDatos    *d, *r;
    DtError     e;
    int         i;

    printf( "LA FECHA DE LA OBSERVACION i\n" );
    es( dt_fecha( 12, 1996, 1, 0, b, sizeof b ), "1/1996",   "la primera" );
    es( dt_fecha( 12, 1996, 1, 11, b, sizeof b ), "12/1996", "la doce" );
    es( dt_fecha( 12, 1996, 1, 12, b, sizeof b ), "1/1997",  "y la trece cambia de año" );
    es( dt_fecha( 12, 1996, 7, 6, b, sizeof b ), "1/1997",
        "empezando en julio, la septima es enero del siguiente" );
    es( dt_fecha( 4, 2000, 3, 2, b, sizeof b ), "1/2001",    "trimestral" );
    es( dt_fecha( 1, 1980, 0, 5, b, sizeof b ), "1985",      "anual: solo el año" );
    es( dt_fecha( 0, 1996, 1, 3, b, sizeof b ), "",
        "sin frecuencia NO HAY fecha, y no se inventa una" );

    printf( "\nLA VENTANA: DE UNA FECHA A UN NUMERO DE OBSERVACIONES\n" );
    /* mensual, 1/1996 .. 7/2026 = 367 observaciones */
    esn( atsw_hasta_n( 12, 1996, 1, 367, "" ), 367,
         "sin ventana, la muestra entera" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "12/2019" ), 288,
         "hasta 12/2019 son 288: 24 años completos" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "1/1996" ), 1,
         "hasta la primera, una" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "7/2026" ), 367,
         "hasta la ultima, todas" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "12/2030" ), 367,
         "una ventana que se pasa da lo que hay, no mas" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "12/1995" ), 0,
         "y una que acaba antes de empezar da CERO, que es un hecho" );
    esn( atsw_hasta_n( 1, 1980, 0, 40, "1989" ), 10, "anual" );
    esn( atsw_hasta_n( 0, 0, 0, 367, "12/2019" ), 367,
         "sin fechas no se puede cortar: se dan todas" );
    /* LO QUE NO SE ENTIENDE ES -1, NO «TODO».
     *
     * Antes devolvia nobs, asi que «2019-12» daba la muestra entera en
     * silencio: se declaraba una ventana hasta 2019 y se estimaba sobre
     * todo. Tres cosas distintas contestaban lo mismo.             */
    esn( atsw_hasta_n( 12, 1996, 1, 367, "esto no es una fecha" ), -1,
         "lo que no es una fecha se RECHAZA" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "2019-12" ), -1,
         "y el formato de al lado también: era el que engañaba" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "12/2019xyz" ), -1,
         "ni con basura detrás, que sscanf daba por buena" );
    esn( atsw_hasta_n( 12, 1996, 1, 367, "13/2019" ), -1,
         "13 no es un mes" );
    esn( atsw_hasta_n( 4, 2000, 1, 40, "5/2005" ), -1,
         "ni 5 un trimestre" );

    printf( "\nEL IDA Y VUELTA DEL .csv\n" );
    d = calloc( 1, sizeof *d );
    r = calloc( 1, sizeof *r );
    d->ncol = 1;
    d->nobs = 40;
    d->freq = 12;
    d->anio = 1996;
    d->per  = 7;
    snprintf( d->nombre[0], DT_NOMBRE, "Alemania" );
    for ( i = 0; i < d->nobs; i++ ) d->v[0][i] = 55.12 + i * 0.37;

    snprintf( path, sizeof path, "%s/ida.csv", dir );
    ok( dt_escribir( path, d, &e ) == 0, "se escribe" );
    ok( dt_leer( path, r, &e ) == 0, "y lo lee el MISMO lector de siempre" );
    esn( r->nobs, d->nobs, "  las observaciones" );
    esn( r->freq, 12, "  la frecuencia, que viajaba con el fichero" );
    esn( r->anio, 1996, "  el año de comienzo" );
    esn( r->per,  7,    "  y el periodo, que no era enero" );
    ok( r->tiene_fechas, "  y hay columna de fechas" );
    es( r->fecha[0], "7/1996", "  la primera es la que toca" );

    {
    double peor = 0.0;

    for ( i = 0; i < d->nobs; i++ )
        { double x = fabs( r->v[0][i] - d->v[0][i] );
          if ( x > peor ) peor = x; }
    ok( peor == 0.0, "Y LOS NUMEROS VUELVEN EXACTOS: si el viaje perdiera "
                     "algo, el .csv no seria el dato" );
    }

    printf( "\nUNA SERIE SIN FRECUENCIA SE ESCRIBE IGUAL, SIN INVENTARLA\n" );
    d->freq = d->anio = d->per = 0;
    d->tiene_fechas = 0;
    snprintf( path, sizeof path, "%s/sinfreq.csv", dir );
    ok( dt_escribir( path, d, &e ) == 0, "se escribe" );
    {
    DtDatos *z = calloc( 1, sizeof *z );

    ok( dt_leer( path, z, &e ) == 0, "y se lee" );
    esn( z->freq, 0, "la frecuencia sigue siendo 0: no consta" );
    esn( z->nobs, 40, "y los datos estan todos" );
    free( z );
    }

    printf( "\nEL TRAMO DEL PROYECTO ES LA UNION DE LAS SERIES\n" );
    {
    /* Las muestras son DEL proyecto y los datos de CADA serie, asi que el
       tramo que se ofrece tiene que ser el de todas juntas.          */
    Proyecto *p = calloc( 1, sizeof *p );
    PrError   pe;
    AtTramo   t;
    DtDatos  *x = calloc( 1, sizeof *x );
    char      csv[1024];
    int       k;

    pr_nuevo( p, "P", "", dir );
    snprintf( p->path, sizeof p->path, "%s/proyecto.yaml", dir );

    /* CORTA: 1/2000 .. 12/2004 ; LARGA: 7/1996 .. 6/2010 */
    for ( k = 0; k < 2; k++ )
        {
        const char *nom = k ? "LARGA" : "CORTA";

        pr_serie_add( p, nom, &pe );
        memset( x, 0, sizeof *x );
        x->ncol = 1;
        x->freq = 12;
        x->anio = k ? 1996 : 2000;
        x->per  = k ? 7 : 1;
        x->nobs = k ? 168 : 60;
        snprintf( x->nombre[0], DT_NOMBRE, "%s", nom );
        atsw_csv_de( p, nom, csv, sizeof csv );
        { char *dd = g_path_get_dirname( csv );
          g_mkdir_with_parents( dd, 0700 ); g_free( dd ); }
        dt_escribir( csv, x, &e );
        }

    ok( atsw_tramo( p, &t ) == 0, "se lee el tramo" );
    esn( t.nseries, 2, "  de las dos series" );
    esn( t.freq, 12, "  la frecuencia, que es una sola" );
    ok( !t.mezcla, "  y no hay mezcla" );
    esn( t.anio, 1996, "el comienzo es el MAS TEMPRANO: el año" );
    esn( t.per, 7, "  y su periodo" );
    esn( t.fin_anio, 2010, "y el final el MAS TARDIO: el año" );
    esn( t.fin_per, 6, "  y su periodo" );

    printf( "\nY UNA FRECUENCIA DISTINTA SE DETECTA, NO SE PROMEDIA\n" );
    pr_serie_add( p, "TRIM", &pe );
    memset( x, 0, sizeof *x );
    x->ncol = 1; x->freq = 4; x->anio = 2000; x->per = 1; x->nobs = 40;
    snprintf( x->nombre[0], DT_NOMBRE, "TRIM" );
    atsw_csv_de( p, "TRIM", csv, sizeof csv );
    { char *dd = g_path_get_dirname( csv );
      g_mkdir_with_parents( dd, 0700 ); g_free( dd ); }
    dt_escribir( csv, x, &e );

    atsw_tramo( p, &t );
    ok( t.mezcla, "se dice que las frecuencias no coinciden" );

    free( x ); free( p );
    }

    free( d ); free( r );
    printf( "\n%d fallos\n", fallos );
    return fallos ? 1 : 0;
}
