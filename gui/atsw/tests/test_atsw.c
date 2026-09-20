/*
 * test_atsw.c -- las REGLAS de la madre, sin widgets.
 *
 * Lo que se prueba aqui no es la ventana: es a QUE apuntan los botones. La
 * regla salio del widget a proposito -- «el elegido, y si no el ultimo» es una
 * decision del metodo, no de la interfaz, y en el widget no se podia probar.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "proyecto.h"

const char *atsw_modelo_por_defecto( const Proyecto *p, const char *serie );

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

int main( void )
{
    Proyecto *p = calloc( 1, sizeof *p );
    PrError   e;
    char      id[PR_ID], ruta[PR_RUTA];

    pr_nuevo( p, "P", "", "." );
    pr_serie_add( p, "EP", &e );

    printf( "A QUE APUNTAN LOS BOTONES\n" );
    es( atsw_modelo_por_defecto( p, "EP" ), "",
        "una serie sin modelos: a nada" );

    /* Los datos son m00 y son un nodo mas: recien cargada la serie, el boton
     * apunta a ellos. Es lo que quiere fug --se identifica sobre los datos--
     * y es lo que on_fue tiene que ver para derivar en vez de pisarlos.  */
    pr_deriva_rol( p, "EP", NULL, PR_DATOS, id, sizeof id, ruta, sizeof ruta, &e );
    es( atsw_modelo_por_defecto( p, "EP" ), "m00",
        "recien cargada: a los datos, que es lo unico que hay" );
    ok( pr_es_datos( p, "EP", atsw_modelo_por_defecto( p, "EP" ) ),
        "y se ve que lo son sin mirar el numero" );

    pr_deriva( p, "EP", "m00", id, sizeof id, ruta, sizeof ruta, &e );
    pr_deriva( p, "EP", "m01", id, sizeof id, ruta, sizeof ruta, &e );
    es( atsw_modelo_por_defecto( p, "EP" ), "m02",
        "con dos modelos: al ULTIMO, que es lo que casi siempre se quiere" );
    ok( !pr_es_datos( p, "EP", "m02" ), "y ese ya no son los datos" );

    pr_elige( p, "EP", "m01", "el SAR no se gana su sitio", &e );
    es( atsw_modelo_por_defecto( p, "EP" ), "m01",
        "y si hay ELEGIDO, al elegido: manda la decision, no la fecha" );

    es( atsw_modelo_por_defecto( p, "NO_ESTA" ), "",
        "una serie que no esta: a nada, sin reventar" );
    es( atsw_modelo_por_defecto( NULL, "EP" ), "",
        "sin proyecto: a nada" );

    /* El ULTIMO es el de VERSION mas alta, que es un CAMPO. Si se dedujera
     * del nombre, un id fuera de orden lo rompería.                     */
    printf( "\nEL ULTIMO SE DECIDE POR LA VERSION, NO POR EL NOMBRE\n" );
    {
    Proyecto *q = calloc( 1, sizeof *q );

    pr_nuevo( q, "Q", "", "." );
    pr_serie_add( q, "S", &e );
    /* a mano, con ids que NO van en orden alfabetico */
    snprintf( q->m[0].serie, PR_ID, "S" ); snprintf( q->m[0].id, PR_ID, "zzz" );
    q->m[0].version = 0;
    snprintf( q->m[1].serie, PR_ID, "S" ); snprintf( q->m[1].id, PR_ID, "aaa" );
    q->m[1].version = 1;
    q->nm = 2;
    es( atsw_modelo_por_defecto( q, "S" ), "aaa",
        "«aaa» es posterior a «zzz» porque su VERSION es mayor" );
    free( q );
    }

    printf( "\n%d fallos\n", fallos );
    free( p );
    return fallos ? 1 : 0;
}
