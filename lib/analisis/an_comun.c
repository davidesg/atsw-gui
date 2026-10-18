/*
 * an_comun.c -- lo poco que las tres ventanas comparten.
 */

#include <stdarg.h>
#include <string.h>
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


/* ------------------------------------------------------------------------ */
/* El cuaderno                                                               */
/* ------------------------------------------------------------------------ */

/* La ruta, relativa al manifiesto si cae debajo: el cuaderno viaja con el
   proyecto, y una ruta absoluta dejaría de valer al moverlo. La relativa,
   con '/' también en Windows: es la que entiende Markdown.              */
static void relativa( const Proyecto *p, const char *f, char *out, size_t n )
{
   gchar *base = g_path_get_dirname( p->path );
   size_t l = strlen( base );
   char  *q;

   if ( !strncmp( f, base, l ) && ( f[l] == '/' || f[l] == '\\' ) )
      {
      g_snprintf( out, n, "%s", f + l + 1 );
      for ( q = out; *q; q++ ) if ( *q == '\\' ) *q = '/';
      }
   else
      g_snprintf( out, n, "%s", f );
   g_free( base );
}

Cuaderno *an_cuaderno( const AnHost *h, const char *que, const char *serie,
                       const char *muestra, const char *id )
{
   Cuaderno *c;
   gchar    *t;
   char      out[PR_RUTA], rel[PR_RUTA];

   if ( h == NULL || h->p == NULL || !h->p->path[0] ) return NULL;

   t = ( id && *id ) ? g_strdup_printf( "%s — %s / %s", que, serie ? serie : "", id )
     : ( serie && *serie ) ? g_strdup_printf( "%s — %s", que, serie )
     : g_strdup( que );
   c = cu_nueva( h->p->path, t );
   g_free( t );
   if ( c == NULL ) return NULL;

   if ( h->p->id[0] ) cu_dato( c, "Proyecto", h->p->id );
   cu_dato( c, "Serie", serie );
   cu_dato( c, "Modelo", id );
   if ( serie && *serie )
      cu_dato( c, "Muestra", ( muestra && *muestra ) ? muestra : "completa" );
   if ( serie && *serie && id && *id &&
        pr_ruta( h->p, serie, muestra, id, ".out", out, sizeof out ) == 0 )
      {
      relativa( h->p, out, rel, sizeof rel );
      cu_dato( c, "Informe", rel );
      }
   return c;
}

void an_cuaderno_cierra( const AnHost *h, Cuaderno *c )
{
   char why[PR_RUTA + 64], md[PR_RUTA];

   if ( c == NULL )
      { an_di( h, "Sin proyecto no hay cuaderno: vive al lado del manifiesto." );
        return; }
   if ( cu_cierra( c, why, sizeof why ) != 0 )
      { an_di( h, "%s", why ); return; }
   if ( h && h->p && cu_ruta( h->p->path, md, sizeof md ) == 0 )
      {
      char rel[PR_RUTA];

      relativa( h->p, md, rel, sizeof rel );
      an_di( h, "Al cuaderno: %s.", rel );
      }
}

GtkWidget *an_boton_cuaderno( const AnHost *h, GCallback cb, gpointer datos )
{
   GtkWidget *b;

   if ( h == NULL || h->p == NULL || !h->p->path[0] ) return NULL;
   b = gtk_button_new_with_label( "Al cuaderno" );
   gtk_widget_set_tooltip_text( b,
      "Lo que enseña esta ventana, al cuaderno del proyecto "
      "(cuaderno/cuaderno.md, al lado del manifiesto), con la fecha y de qué "
      "modelo sale.\n\nEl guion guarda lo que decidiste; el cuaderno, lo que "
      "mirabas al decidirlo. Nunca se trunca, y se lee sin ATSW: es Markdown." );
   g_signal_connect( b, "clicked", cb, datos );
   return b;
}

void an_cuaderno_grafico( const AnHost *h, const char *eps, const char *que,
                          const char *serie, const char *muestra,
                          const char *id, const char *const *datos )
{
   Cuaderno *c = an_cuaderno( h, que, serie, muestra, id );
   char      png[PR_RUTA], rel[PR_RUTA];
   gchar    *base, *pie;
   int       i;

   if ( c == NULL ) { an_cuaderno_cierra( h, NULL ); return; }
   for ( i = 0; datos && datos[i] && datos[i+1]; i += 2 )
      cu_dato( c, datos[i], datos[i+1] );

   /* El fichero del dibujo: relativo si es del proyecto; si vive fuera (la
      cache del vistazo) basta su nombre, que una ruta de cache no dice nada. */
   base = g_path_get_basename( eps );
   relativa( h->p, eps, rel, sizeof rel );
   cu_dato( c, "Gráfico", g_path_is_absolute( rel ) ? base : rel );

   pie = ( id && *id ) ? g_strdup_printf( "%s de %s / %s", que, serie ? serie : "", id )
       : ( serie && *serie ) ? g_strdup_printf( "%s de %s", que, serie )
       : g_strdup( que );
   if ( cu_imagen( c, base, pie, png, sizeof png ) == 0 )
      preview_save_as( eps, png, NULL );       /* si falla, la entrada lo dice */
   g_free( pie );
   g_free( base );
   an_cuaderno_cierra( h, c );
}


gchar *an_markup_md( const char *m )
{
   GString    *o = g_string_new( "" );
   const char *p;
   gboolean    inicio = TRUE;          /* al principio de una línea        */

   for ( p = m ? m : ""; *p; )
      {
      if ( inicio && *p == ' ' ) { p++; continue; }
      if ( *p == '<' )
         {
         const char *f = strchr( p, '>' );
         gchar      *tag;

         if ( f == NULL ) { g_string_append( o, p ); break; }
         tag = g_strndup( p + 1, (gsize) ( f - p - 1 ) );
         if ( !strcmp( tag, "b" ) || !strcmp( tag, "/b" ) )
            g_string_append( o, "**" );
         else if ( !strcmp( tag, "i" ) || !strcmp( tag, "/i" ) )
            g_string_append( o, "*" );
         else if ( !strcmp( tag, "sub" ) || !strcmp( tag, "/sub" ) ||
                   !strcmp( tag, "sup" ) || !strcmp( tag, "/sup" ) )
            g_string_append_printf( o, "<%s>", tag );
         /* el resto (small, span, tt) se quita */
         g_free( tag );
         p = f + 1;
         inicio = FALSE;
         continue;
         }
      if ( *p == '&' )
         {
         static const struct { const char *e, *c; } ent[] = {
            { "&amp;", "&" }, { "&gt;", ">" }, { "&quot;", "\"" },
            { "&apos;", "'" }, { "&#39;", "'" } };
         size_t k;
         int    hecho = 0;

         for ( k = 0; k < G_N_ELEMENTS( ent ) && !hecho; k++ )
            if ( !strncmp( p, ent[k].e, strlen( ent[k].e ) ) )
               { g_string_append( o, ent[k].c ); p += strlen( ent[k].e ); hecho = 1; }
         /* &lt; se queda: un «<» suelto en Markdown puede abrir HTML. */
         if ( !hecho ) { g_string_append_c( o, *p ); p++; }
         inicio = FALSE;
         continue;
         }
      if ( *p == '\n' )
         {
         /* Un salto suelto, de verdad (dos espacios); uno doble, párrafo. */
         while ( o->len && o->str[o->len-1] == ' ' ) g_string_truncate( o, o->len - 1 );
         if ( p[1] == '\n' || p[1] == '\0' ) g_string_append_c( o, '\n' );
         else g_string_append( o, "  \n" );
         p++;
         inicio = TRUE;
         continue;
         }
      g_string_append_c( o, *p++ );
      inicio = FALSE;
      }
   return g_string_free( o, FALSE );
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
