/*
 * an_comun.c -- lo poco que las tres ventanas comparten.
 */

#include <stdarg.h>

#include "analisis.h"

/* DECIR ALGO AL QUE LLAMO. Sin anfitrión no se pierde el mensaje: se va al
 * log. Una ventana que se niega en silencio es un botón que no funciona, y
 * eso ya costó una sesión entera de «no pasa nada».                     */
void an_di( const AnHost *h, const char *fmt, ... )
{
   va_list ap;
   gchar  *s;

   va_start( ap, fmt );
   s = g_strdup_vprintf( fmt, ap );
   va_end( ap );

   if ( h && h->di ) h->di( h->dueno, s );
   else              g_message( "analisis: %s", s );
   g_free( s );
}

gchar *an_cache_dir( void )
{
   gchar *d = g_build_filename( g_get_user_cache_dir(), "atsw", NULL );

   g_mkdir_with_parents( d, 0700 );
   return d;
}

/* UN NOMBRE DE FICHERO POR MODELO, y no uno fijo.
 *
 * lib/preview reutiliza la ventana POR RUTA. Con un nombre fijo, abrir los
 * anómalos de dos modelos pisaría el dibujo del primero en su propia
 * ventana: la misma ruta es la misma ventana. Con la clave dentro, cada
 * modelo tiene la suya.                                                 */
gchar *an_fichero( const char *que, const char *serie, const char *muestra,
                   const char *id )
{
   gchar *dir = an_cache_dir();
   gchar *base, *ruta;
   gchar *s;

   base = g_strdup_printf( "%s_%s_%s_%s", que, serie ? serie : "",
                           ( muestra && *muestra ) ? muestra : "completa",
                           id ? id : "" );
   for ( s = base; *s; s++ )
       if ( !g_ascii_isalnum( *s ) && *s != '_' && *s != '-' ) *s = '_';

   ruta = g_build_filename( dir, base, NULL );
   g_free( base );
   g_free( dir );
   return ruta;
}
