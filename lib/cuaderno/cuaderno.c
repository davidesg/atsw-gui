/*
 * cuaderno.c -- ver cuaderno.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "cuaderno.h"

typedef enum { P_TEXTO, P_IMAGEN } Tipo;

typedef struct {
   Tipo   tipo;
   gchar *txt;                    /* el Markdown, o el pie del grafico     */
   gchar *ruta;                   /* la imagen: donde se escribe           */
   gchar *rel;                    /* la imagen: como se enlaza             */
} Parte;

struct Cuaderno {
   gchar     *dir;                /* <manifiesto>/cuaderno                 */
   gchar     *titulo;
   gchar     *sello;              /* «2026-10-17 20:15»                    */
   gchar     *marca;              /* «20261017-201503», para las imagenes  */
   GString   *datos;
   GPtrArray *partes;
};

static void parte_free( gpointer d )
{
   Parte *p = d;

   g_free( p->txt ); g_free( p->ruta ); g_free( p->rel );
   g_free( p );
}

static gchar *dir_de( const char *manifiesto )
{
   gchar *base, *d;

   if ( manifiesto == NULL || !*manifiesto ) return NULL;
   base = g_path_get_dirname( manifiesto );
   d    = g_build_filename( base, "cuaderno", NULL );
   g_free( base );
   return d;
}

int cu_ruta( const char *manifiesto, char *out, size_t n )
{
   gchar *d = dir_de( manifiesto ), *f;

   if ( d == NULL || out == NULL || n == 0 ) { g_free( d ); return 1; }
   f = g_build_filename( d, "cuaderno.md", NULL );
   g_snprintf( out, n, "%s", f );
   g_free( f ); g_free( d );
   return 0;
}

Cuaderno *cu_nueva( const char *manifiesto, const char *titulo )
{
   Cuaderno  *c;
   GDateTime *t;
   gchar     *d = dir_de( manifiesto );

   if ( d == NULL ) return NULL;
   c = g_new0( Cuaderno, 1 );
   c->dir    = d;
   c->titulo = g_strdup( titulo && *titulo ? titulo : "Sin título" );
   t = g_date_time_new_now_local();
   c->sello = g_date_time_format( t, "%Y-%m-%d %H:%M" );
   c->marca = g_date_time_format( t, "%Y%m%d-%H%M%S" );
   g_date_time_unref( t );
   c->datos  = g_string_new( "" );
   c->partes = g_ptr_array_new_with_free_func( parte_free );
   return c;
}

void cu_dato( Cuaderno *c, const char *clave, const char *valor )
{
   if ( c == NULL || clave == NULL || valor == NULL || !*valor ) return;
   g_string_append_printf( c->datos, "- **%s:** %s\n", clave, valor );
}

void cu_texto( Cuaderno *c, const char *md )
{
   Parte *p;

   if ( c == NULL || md == NULL || !*md ) return;
   p = g_new0( Parte, 1 );
   p->tipo = P_TEXTO;
   p->txt  = g_strdup( md );
   g_ptr_array_add( c->partes, p );
}

void cu_tabla( Cuaderno *c, const Tabla *t )
{
   char *m;

   if ( c == NULL || t == NULL ) return;
   if ( ( m = tb_md( t ) ) == NULL ) return;
   cu_texto( c, m );
   free( m );
}

/* Un nombre de fichero sin sorpresas: letras, cifras y guiones. */
static gchar *limpio( const char *s )
{
   GString    *g = g_string_new( "" );
   const char *p;

   for ( p = s ? s : ""; *p; p++ )
      {
      unsigned char u = (unsigned char) *p;

      if ( g_ascii_isalnum( u ) ) g_string_append_c( g, g_ascii_tolower( u ) );
      else if ( g->len && g->str[g->len-1] != '-' ) g_string_append_c( g, '-' );
      }
   while ( g->len && g->str[g->len-1] == '-' ) g_string_truncate( g, g->len - 1 );
   if ( !g->len ) g_string_append( g, "grafico" );
   return g_string_free( g, FALSE );
}

int cu_imagen( Cuaderno *c, const char *nombre, const char *pie,
               char *ruta, size_t n )
{
   Parte *p = NULL;
   gchar *img, *nom, *base = NULL;
   int    k;

   if ( c == NULL || ruta == NULL || n == 0 ) return 1;
   img = g_build_filename( c->dir, "img", NULL );
   nom = limpio( nombre );

   /* Dos graficos en el mismo segundo no se pisan. */
   for ( k = 1; ; k++ )
      {
      gchar *f = k == 1 ? g_strdup_printf( "%s-%s.png", c->marca, nom )
                        : g_strdup_printf( "%s-%s-%d.png", c->marca, nom, k );
      gchar *r = g_build_filename( img, f, NULL );
      guint  i;
      int    usado = g_file_test( r, G_FILE_TEST_EXISTS );

      for ( i = 0; !usado && i < c->partes->len; i++ )
         {
         Parte *q = g_ptr_array_index( c->partes, i );

         usado = q->tipo == P_IMAGEN && !strcmp( q->ruta, r );
         }
      if ( !usado ) { base = f; p = g_new0( Parte, 1 ); p->ruta = r; break; }
      g_free( f ); g_free( r );
      }

   if ( g_mkdir_with_parents( img, 0755 ) != 0 )
      { g_free( img ); g_free( nom ); g_free( base ); parte_free( p ); return 1; }

   p->tipo = P_IMAGEN;
   p->txt  = g_strdup( pie && *pie ? pie : "" );
   p->rel  = g_strdup_printf( "img/%s", base );
   g_ptr_array_add( c->partes, p );
   g_snprintf( ruta, n, "%s", p->ruta );

   g_free( img ); g_free( nom ); g_free( base );
   return 0;
}

void cu_descarta( Cuaderno *c )
{
   if ( c == NULL ) return;
   g_free( c->dir ); g_free( c->titulo ); g_free( c->sello ); g_free( c->marca );
   g_string_free( c->datos, TRUE );
   g_ptr_array_free( c->partes, TRUE );
   g_free( c );
}

/* La entrada entera, en memoria. */
static GString *arma( const Cuaderno *c )
{
   GString *s = g_string_new( "" );
   guint    i;

   g_string_append_printf( s, "\n## %s · %s\n\n", c->sello, c->titulo );
   if ( c->datos->len ) g_string_append_printf( s, "%s\n", c->datos->str );

   for ( i = 0; i < c->partes->len; i++ )
      {
      const Parte *p = g_ptr_array_index( c->partes, i );

      if ( p->tipo == P_TEXTO )
         {
         g_string_append( s, p->txt );
         if ( s->len && s->str[s->len-1] != '\n' ) g_string_append_c( s, '\n' );
         g_string_append_c( s, '\n' );
         }
      else if ( g_file_test( p->ruta, G_FILE_TEST_IS_REGULAR ) )
         {
         g_string_append_printf( s, "![%s](%s)\n", p->txt, p->rel );
         if ( *p->txt ) g_string_append_printf( s, "*%s*\n", p->txt );
         g_string_append_c( s, '\n' );
         }
      else
         g_string_append( s, "*(El gráfico no se pudo guardar.)*\n\n" );
      }

   g_string_append( s, "---\n" );
   return s;
}

static const char CABECERA[] =
   "# Cuaderno del proyecto\n\n"
   "Lo que se miró, en orden: cada entrada es una ventana de análisis que se\n"
   "mandó aquí, con su fecha y de qué modelo sale. Lo escribe ATSW y nunca se\n"
   "trunca; se puede leer y anotar a mano.\n\n---\n";

int cu_cierra( Cuaderno *c, char *why, size_t n )
{
   gchar   *md;
   GString *s;
   FILE    *f;
   int      nuevo, r = 0;

   if ( why && n ) why[0] = '\0';
   if ( c == NULL )
      { if ( why ) g_snprintf( why, n, "No hay proyecto: el cuaderno vive a su lado." );
        return 1; }

   if ( g_mkdir_with_parents( c->dir, 0755 ) != 0 )
      {
      if ( why ) g_snprintf( why, n, "No pude crear %s.", c->dir );
      cu_descarta( c );
      return 1;
      }

   md    = g_build_filename( c->dir, "cuaderno.md", NULL );
   nuevo = !g_file_test( md, G_FILE_TEST_EXISTS );
   s     = arma( c );

   /* "ab": añadir, y en binario para que Windows no meta \r. */
   f = g_fopen( md, "ab" );
   if ( f == NULL ) r = 1;
   else
      {
      if ( nuevo && fputs( CABECERA, f ) < 0 ) r = 1;
      if ( !r && fwrite( s->str, 1, s->len, f ) != s->len ) r = 1;
      if ( fclose( f ) != 0 ) r = 1;
      }
   if ( r && why ) g_snprintf( why, n, "No pude escribir en %s.", md );

   g_string_free( s, TRUE );
   g_free( md );
   cu_descarta( c );
   return r;
}

int cu_nota( const char *manifiesto, const char *texto, char *why, size_t n )
{
   Cuaderno *c;
   gchar    *t = g_strstrip( g_strdup( texto ? texto : "" ) );

   if ( !*t )
      {
      if ( why ) g_snprintf( why, n, "La nota está vacía." );
      g_free( t );
      return 1;
      }
   c = cu_nueva( manifiesto, "Nota" );
   cu_texto( c, t );
   g_free( t );
   return cu_cierra( c, why, n );
}
