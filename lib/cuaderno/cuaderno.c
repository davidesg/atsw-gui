/*
 * cuaderno.c -- ver cuaderno.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "cuaderno.h"
#include "proyecto.h"            /* el cerrojo, el mismo del manifiesto */

#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif
#ifndef O_BINARY
#  define O_BINARY 0
#endif

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

/* Una imagen reservada que nadie lleno: el fichero esta, pero vacio. */
static int vacia( const char *ruta )
{
   GStatBuf st;
   return g_stat( ruta, &st ) == 0 && st.st_size == 0;
}

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
   if ( g_mkdir_with_parents( img, 0755 ) != 0 )
      { g_free( img ); g_free( nom ); return 1; }

   /* Dos graficos en el mismo segundo no se pisan. */
   for ( k = 1; ; k++ )
      {
      gchar *f = k == 1 ? g_strdup_printf( "%s-%s.png", c->marca, nom )
                        : g_strdup_printf( "%s-%s-%d.png", c->marca, nom, k );
      gchar *r = g_build_filename( img, f, NULL );
      int    fd;

      /* EL NOMBRE SE RESERVA CREANDO EL FICHERO, vacio y con O_EXCL: mirar si
         existe y escribirlo despues dejaba que dos programas, en el mismo
         segundo, eligieran el mismo y uno pisara la imagen del otro
         (AUDITORIA-procesos A6). Lo llena quien dibuja; si no llega a
         llenarlo, cu_cierra y cu_descarta lo quitan.                   */
      fd = g_open( r, O_CREAT | O_EXCL | O_WRONLY | O_BINARY, 0644 );
      if ( fd >= 0 )
         {
         close( fd );
         base = f; p = g_new0( Parte, 1 ); p->ruta = r;
         break;
         }
      g_free( f ); g_free( r );
      if ( errno != EEXIST || k > 9999 )
         { g_free( img ); g_free( nom ); return 1; }
      }

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
   guint i;

   if ( c == NULL ) return;
   for ( i = 0; i < c->partes->len; i++ )
      {
      const Parte *p = g_ptr_array_index( c->partes, i );
      if ( p->tipo == P_IMAGEN && vacia( p->ruta ) ) g_unlink( p->ruta );
      }
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
      else if ( g_file_test( p->ruta, G_FILE_TEST_IS_REGULAR ) && !vacia( p->ruta ) )
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

   md = g_build_filename( c->dir, "cuaderno.md", NULL );
   s  = arma( c );

   /* CON EL CERROJO, EL MISMO DEL MANIFIESTO (pr_cerrojo_coge, sobre
      cuaderno.md.cerrojo), Y DE UNA SOLA ESCRITURA. Varios programas
      añaden al cuaderno: en Windows el modo «a» del CRT busca el final y
      luego escribe, y entre una cosa y otra puede escribir otro; en todos,
      stdio parte una entrada larga en varias escrituras, y dos que nacian a
      la vez ponian dos cabeceras (AUDITORIA-procesos A6).              */
   {
   PrError    e;
   PrCerrojo *k = pr_cerrojo_coge( md, &e );

   if ( k == NULL )
      {
      r = 1;
      if ( why ) g_snprintf( why, n, "Otro programa está escribiendo en el cuaderno: "
                             "no se añadió. Vuelve a intentarlo." );
      }
   else
      {
      nuevo = !g_file_test( md, G_FILE_TEST_EXISTS ) || vacia( md );
      if ( nuevo ) g_string_prepend( s, CABECERA );
      /* "ab": añadir, y en binario para que Windows no meta \r. */
      f = g_fopen( md, "ab" );
      if ( f == NULL ) r = 1;
      else
         {
         setvbuf( f, NULL, _IONBF, 0 );           /* una escritura, entera */
         if ( fwrite( s->str, 1, s->len, f ) != s->len ) r = 1;
         if ( fclose( f ) != 0 ) r = 1;
         }
      pr_cerrojo_suelta( k );
      if ( r && why ) g_snprintf( why, n, "No pude escribir en %s.", md );
      }
   }

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

void cu_relativa( const char *manifiesto, const char *f, char *out, size_t n )
{
   gchar *base;
   size_t l;
   char  *q;

   if ( out == NULL || n == 0 ) return;
   if ( f == NULL ) { out[0] = 0; return; }
   if ( manifiesto == NULL || !*manifiesto ) { g_snprintf( out, n, "%s", f ); return; }
   base = g_path_get_dirname( manifiesto );
   l    = strlen( base );
   if ( !strncmp( f, base, l ) && ( f[l] == '/' || f[l] == '\\' ) )
      {
      g_snprintf( out, n, "%s", f + l + 1 );
      for ( q = out; *q; q++ ) if ( *q == '\\' ) *q = '/';
      }
   else
      g_snprintf( out, n, "%s", f );
   g_free( base );
}

int cu_grafico_caso( const char *manifiesto, const char *proyecto,
                     const char *que, const char *caso, const char *corrida,
                     const char *informe, const char *eps,
                     int (*a_png)( const char *eps, const char *png ),
                     char *md, size_t nmd, char *why, size_t n )
{
   Cuaderno *c;
   gchar    *t, *pie, *base;
   char      rel[4096], png[4096];
   int       rc;

   if ( md && nmd ) md[0] = 0;
   if ( manifiesto == NULL || !*manifiesto )
      { g_snprintf( why, n, "Sin proyecto no hay cuaderno: vive al lado del manifiesto." );
        return 1; }
   if ( eps == NULL || !*eps )
      { g_snprintf( why, n, "No hay gráfico." ); return 1; }

   que  = ( que && *que ) ? que : "Gráfico";
   t    = ( caso && *caso && corrida && *corrida )
        ? g_strdup_printf( "%s — caso %s / %s", que, caso, corrida )
        : ( caso && *caso ) ? g_strdup_printf( "%s — caso %s", que, caso )
        : g_strdup( que );
   c = cu_nueva( manifiesto, t );
   g_free( t );
   if ( c == NULL ) { g_snprintf( why, n, "No pude abrir el cuaderno." ); return 1; }

   if ( proyecto && *proyecto ) cu_dato( c, "Proyecto", proyecto );
   if ( caso && *caso )         cu_dato( c, "Caso", caso );
   if ( corrida && *corrida )   cu_dato( c, "Corrida", corrida );
   if ( informe && *informe )
      {
      cu_relativa( manifiesto, informe, rel, sizeof rel );
      cu_dato( c, "Informe", rel );
      }
   /* El dibujo: relativo si es del proyecto; si vive en la cache, su nombre,
      que una ruta de cache no dice nada (como an_cuaderno_grafico).      */
   base = g_path_get_basename( eps );
   cu_relativa( manifiesto, eps, rel, sizeof rel );
   cu_dato( c, "Gráfico", g_path_is_absolute( rel ) ? base : rel );

   pie = ( caso && *caso && corrida && *corrida )
       ? g_strdup_printf( "%s del caso %s / %s", que, caso, corrida )
       : ( caso && *caso ) ? g_strdup_printf( "%s del caso %s", que, caso )
       : g_strdup( que );
   if ( cu_imagen( c, base, pie, png, sizeof png ) == 0 && a_png )
      a_png( eps, png );                 /* si falla, la entrada lo dice */
   g_free( pie );
   g_free( base );

   rc = cu_cierra( c, why, n );
   if ( rc == 0 && md && nmd && cu_ruta( manifiesto, rel, sizeof rel ) == 0 )
      cu_relativa( manifiesto, rel, md, nmd );
   return rc;
}
