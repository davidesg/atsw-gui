/* BUG-0003: pr_escribir pisa en silencio un manifiesto que no puede leer.
 * Lo usa repro.py. Sale con 1 si lo piso callado, 0 si se nego o lo
 * conservo, 2 si la prueba no pudo ni empezar.                         */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proyecto.h"

static int contiene( const char *f, const char *s )
{
   char  buf[16384];
   size_t n;
   FILE *h = fopen( f, "rb" );
   if ( !h ) return 0;
   n = fread( buf, 1, sizeof buf - 1, h ); buf[n] = '\0'; fclose( h );
   return strstr( buf, s ) != NULL;
}

int main( int argc, char **argv )
{
   static Proyecto p;
   PrError e;
   char    y[1024];
   FILE   *f;
   int     rc;

   if ( argc < 2 ) return 2;
   snprintf( y, sizeof y, "%s/proyecto.yaml", argv[1] );
   remove( y );
   pr_nuevo( &p, "b3", "BUG-0003", "." );
   snprintf( p.path, sizeof p.path, "%s", y );
   pr_serie_add( &p, "IPC", &e );
   if ( pr_escribir( &p, y, &e ) != 0 ) { puts( "no se pudo escribir el de partida" ); return 2; }

   /* OTRO PROGRAMA, MAS NUEVO, anade una clave que este no conoce. */
   f = fopen( y, "a" );
   if ( !f ) return 2;
   fprintf( f, "clave_de_una_version_nueva: algo\n" );
   fclose( f );

   /* Y este, que tenia el proyecto abierto, guarda un cambio suyo. */
   pr_serie_add( &p, "PIB", &e );
   rc = pr_escribir( &p, y, &e );
   if ( rc == 0 && !contiene( y, "clave_de_una_version_nueva" ) )
      {
      puts( "pr_escribir devolvio 0 y la clave nueva ya no esta: lo piso callado" );
      return 1;
      }
   printf( "pr_escribir devolvio %d; la clave nueva %s\n", rc,
           contiene( y, "clave_de_una_version_nueva" ) ? "sigue" : "no esta" );
   return 0;
}
