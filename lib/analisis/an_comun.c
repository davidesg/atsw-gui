/*
 * an_comun.c -- lo poco que las tres ventanas comparten.
 */

#include <stdarg.h>
#include <sys/stat.h>

#include "analisis.h"
#include "inpdet.h"

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


/* Ver analisis.h: al día no es existe. */
AnEstado an_estado( const Proyecto *p, const char *serie, const char *muestra,
                    const char *id, char *porque, size_t n )
{
   char        inp[PR_RUTA], out[PR_RUTA];
   struct stat si, so;
   int         hay_inp;

   if ( porque && n ) porque[0] = '\0';

   if ( !p || !serie || !*serie || !id || !*id ||
        pr_ruta( p, serie, muestra, id, ".out", out, sizeof out ) != 0 )
      {
      if ( porque ) g_snprintf( porque, n,
          "Este fichero no es un modelo de este proyecto: sin clave no hay "
          "linaje que mirar." );
      return AN_SIN_CLAVE;
      }

   if ( stat( out, &so ) != 0 )
      {
      if ( porque ) g_snprintf( porque, n,
          "«%s» todavía no está estimado. Estímalo y esto se enciende.", id );
      return AN_SIN_OUT;
      }

   hay_inp = ( pr_ruta( p, serie, muestra, id, ".inp", inp, sizeof inp ) == 0 &&
               stat( inp, &si ) == 0 );

   /* EL SEGUNDO DE DIFERENCIA NO CUENTA. El motor escribe el .inp y el .out
      en la misma corrida, y en un sistema de ficheros con resolución de un
      segundo pueden salir con la misma marca o invertidos por redondeo.
      Se pide que el informe sea MAS VIEJO DE VERDAD, no que no empate.  */
   if ( hay_inp && si.st_mtime > so.st_mtime + 1 )
      {
      if ( porque ) g_snprintf( porque, n,
          "El informe de «%s» es de antes que su especificación: lo que dijera "
          "sería de otro modelo. Reestímalo.", id );
      return AN_OUT_VIEJO;
      }
   return AN_LISTO;
}


/* Ver analisis.h. Se prueba el .pre y luego el .inp: los dos tienen el mismo
 * bloque de deterministas, y el .pre es el que hay cuando esta estimado. */
int an_deterministas( const Proyecto *p, const char *serie,
                      const char *muestra, const char *id,
                      char det[][AN_LDET], int max )
{
   static const char *EXT[] = { ".pre", ".inp" };
   char  ruta[PR_RUTA], porque[256];
   char  tmp[AN_MAX_DET][ID_LINEA];
   int   e, n, i;

   if ( !p || !det || max < 1 ) return 0;
   for ( e = 0; e < 2; e++ )
       {
       if ( pr_ruta( p, serie, muestra, id, EXT[e], ruta, sizeof ruta ) != 0 )
          continue;
       if ( !g_file_test( ruta, G_FILE_TEST_EXISTS ) ) continue;
       n = id_nombres( ruta, tmp, AN_MAX_DET, porque, sizeof porque );
       if ( n < 0 ) continue;
       if ( n > max ) n = max;
       for ( i = 0; i < n; i++ ) g_snprintf( det[i], AN_LDET, "%s", tmp[i] );
       return n;
       }
   return 0;
}
