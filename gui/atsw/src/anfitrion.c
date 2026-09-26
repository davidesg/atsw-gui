/*
 * anfitrion.c -- la madre, vista por lib/analisis.
 *
 * Cuatro funciones de tres lineas. Ese es exactamente el tamaño de lo que
 * las ventanas de analisis necesitaban de la madre, y por eso estaban mal
 * donde estaban: no eran de la madre, tenian su direccion.
 */

#include "anfitrion.h"

void barra_pub( Atsw *a, const char *s );

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

static void md_abre( void *d, const char *serie, const char *muestra,
                     const char *id )
{
    atsw_editor( (Atsw *) d, serie, muestra, id );
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
    return h;
}
