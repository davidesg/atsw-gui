/*
 * test_xlsx.c -- LOS .xlsx DEL CASO DEL MANUAL, comparados con su CSV.
 *
 *   test_xlsx ipc_wti.csv libro.xlsx [libro.xlsx...]
 *   test_xlsx ipc_wti.csv @fichero
 *
 * Cada libro (lib/datos/pruebas, ver fabrica.py) guarda lo mismo que el CSV
 * escrito de otra manera: fechas de Excel con estilo, el formato de un Excel
 * en espanol, la fecha como texto, otro escritor. Lo que se exige es que la
 * puerta de los datos los lea IGUAL: las mismas columnas con los mismos
 * nombres, la misma fecha de inicio y frecuencia, y cada valor, uno a uno.
 *
 * @fichero: la ruta del libro va DENTRO de un fichero, en UTF-8. Es como
 * llegan las rutas del selector de ficheros de GTK, y en Windows argv no
 * sirve para probarlo: el CRT ya las convierte a la pagina de codigos. Asi
 * se ve si una carpeta con acentos («Econometría») se puede abrir.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datos.h"

static int fallos;

static void ok( int c, const char *libro, const char *que )
{
   printf( "  %s  %s: %s\n", c ? "ok  " : "FALLA", libro, que );
   if ( !c ) fallos++;
}

/* La ruta que hay dentro de @fichero, sin el salto de linea. */
static const char *ruta( const char *arg, char *buf, size_t n )
{
   FILE *f;
   size_t l;

   if ( arg[0] != '@' ) return( arg );
   if ( (f = fopen( arg + 1, "rb" )) == NULL ) return( arg );
   l = fread( buf, 1, n - 1, f );
   fclose( f );
   buf[l] = '\0';
   while ( l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r') ) buf[--l] = '\0';
   return( buf );
}

int main( int argc, char **argv )
{
   /* Dos megas cada uno: en la pila no caben en Windows. */
   static DtDatos ref, d;
   DtError e;
   char msg[256], buf[4096];
   int i, c, t;

   if ( argc < 3 )
      { fprintf( stderr, "uso: test_xlsx ref.csv libro.xlsx...\n" ); return( 2 ); }

   printf( "\nLOS .xlsx DEL CASO DEL MANUAL, CONTRA SU CSV\n" );
   if ( dt_leer( argv[1], &ref, &e ) != 0 )
      {
      printf( "  FALLA  %s: %s\n", argv[1], dt_error_es( &e, msg, sizeof msg ) );
      return( 1 );
      }
   printf( "  el CSV: %d columnas, %d observaciones, desde %d/%d, frecuencia %d\n",
           ref.ncol, ref.nobs, ref.per, ref.anio, ref.freq );

   for ( i = 2; i < argc; i++ )
      {
      const char *p = ruta( argv[i], buf, sizeof buf );
      const char *nom = strrchr( p, '/' ) ? strrchr( p, '/' ) + 1 : p;
      int distintos = 0, primero = -1, col = 0;

      if ( argv[i][0] == '@' ) nom = "en una carpeta con acentos (ruta UTF-8)";
      if ( dt_leer( p, &d, &e ) != 0 )
         {
         ok( 0, nom, dt_error_es( &e, msg, sizeof msg ) );
         continue;
         }
      ok( d.ncol == ref.ncol, nom, "las mismas columnas" );
      for ( c = 0; c < ref.ncol && c < d.ncol; c++ )
         if ( strcmp( d.nombre[c], ref.nombre[c] ) != 0 )
            {
            snprintf( msg, sizeof msg, "la columna %d se llama «%s» y no «%s»",
                      c + 1, d.nombre[c], ref.nombre[c] );
            ok( 0, nom, msg );
            }
      ok( d.nobs == ref.nobs, nom, "las mismas observaciones" );
      snprintf( msg, sizeof msg, "empieza en %d/%d con frecuencia %d", d.per, d.anio, d.freq );
      ok( d.anio == ref.anio && d.per == ref.per && d.freq == ref.freq, nom, msg );
      for ( c = 0; c < ref.ncol && c < d.ncol; c++ )
         for ( t = 0; t < ref.nobs && t < d.nobs; t++ )
            if ( d.v[c][t] != ref.v[c][t] )
               {
               if ( primero < 0 ) { primero = t; col = c; }
               distintos++;
               }
      if ( distintos )
         snprintf( msg, sizeof msg, "%d valores distintos; el primero, %s en %d: %.17g y no %.17g",
                   distintos, ref.nombre[col], primero + 1, d.v[col][primero], ref.v[col][primero] );
      else
         snprintf( msg, sizeof msg, "los %d valores, idénticos", ref.ncol * ref.nobs );
      ok( distintos == 0, nom, msg );
      }

   if ( fallos ) { printf( "\n%d fallos\n", fallos ); return( 1 ); }
   printf( "\ntodo bien\n" );
   return( 0 );
}
