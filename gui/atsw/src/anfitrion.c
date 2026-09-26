/*
 * anfitrion.c -- la madre, vista por lib/analisis.
 *
 * Cuatro funciones de tres lineas. Ese es exactamente el tamaño de lo que
 * las ventanas de analisis necesitaban de la madre, y por eso estaban mal
 * donde estaban: no eran de la madre, tenian su direccion.
 */

#include <string.h>

#include "anfitrion.h"

void barra_pub( Atsw *a, const char *s );
void atsw_editor_cierra( const char *serie, const char *muestra,
                         const char *id );

static void md_di( void *d, const char *s )
{
    barra_pub( (Atsw *) d, s );
}

static int md_guarda( void *d )
{
    PrError e;

    return atsw_guarda( (Atsw *) d, &e );
}

static void md_refresca( void *d )
{
    atsw_refresca( (Atsw *) d );
}

/* ABRIR EL HIJO CON LO QUE SE HAYA PEDIDO. Son las dos puertas a la misma
 * iteracion: fue especifica por FORMULARIO y el editor especifica el
 * FICHERO. Por defecto fue, porque lo que un nodo recien derivado necesita
 * a continuacion es estimarse.                                         */
static void md_abre( void *d, const char *serie, const char *muestra,
                     const char *id, AnHerramienta con )
{
    Atsw *a = (Atsw *) d;
    char  ruta[PR_RUTA];

    if ( con == AN_CON_EDITOR )
        { atsw_editor( a, serie, muestra, id ); return; }

    if ( pr_ruta( a->p, serie, muestra, id, ".inp", ruta, sizeof ruta ) != 0 )
        { barra_pub( a, "No pude componer la ruta del modelo nuevo." ); return; }
    atsw_lanza( a, "fue_gui", ruta );
}

static void md_cierra( void *d, const char *serie, const char *muestra,
                       const char *id )
{
    (void) d;
    atsw_editor_cierra( serie, muestra, id );
}

AnHost atsw_host( Atsw *a )
{
    AnHost h;

    memset( &h, 0, sizeof h );
    h.p        = a->p;
    h.padre    = GTK_WINDOW( a->ventana );
    h.preview  = (PreviewApp *) a;
    h.dueno    = a;
    h.di       = md_di;
    h.guarda   = md_guarda;
    h.refresca = md_refresca;
    h.abre     = md_abre;
    h.cierra   = md_cierra;
    return h;
}
