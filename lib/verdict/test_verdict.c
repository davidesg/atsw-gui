/* test_verdict.c -- leer como acabo el optimizador.
 *
 * Los textos de abajo NO estan inventados: son los que drtran escribe de
 * verdad, copiados de sus .out. Los dos primeros se comprobaron corriendo el
 * motor sobre el m6 y sobre ES_CPI/WTI en diagonal; los otros se arman con las
 * frases exactas de drtran.c:3272-3280, que es donde el motor las elige.
 *
 * La prueba que importa no es la de convergencia: es la del CASO 3. El motor
 * escribe "STOPPED AT A POINT WITH NO IMPROVEMENT", que suena a fracaso y es
 * lo que sale cuando el .pre YA ERA el optimo. Tomarlo por fallo seria
 * confundir el exito con el fracaso, y ademas romperia la invariante del
 * contrato -- correr el motor sobre un .pre y que los numeros no se muevan.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "verdict.h"

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-60s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

/* --- lo que el motor escribe de verdad ---------------------------------- */

/* m6, seis series, red y restricciones. Corrido. */
static const char M6[] =
"Observations           : 64\n"
"Parameters             : 52\n"
"\n"
"**** CONVERGENCE OBTAINED AFTER 349 ITERATIONS (of 500)\n"
"**** parameter stopping criterium satisfied\n"
"\n"
"Log-likelihood = -1701.348826\n";

/* ES_CPI contra WTI en diagonal (-0). Corrido. */
static const char DIAG[] =
"Method                 : exact maximum likelihood\n"
"\n"
"**** CONVERGENCE OBTAINED AFTER 11 ITERATIONS\n"
"**** gradient stopping criterium satisfied\n"
"Log-likelihood         : -744.923779\n";

/* drtran.c:3276-3278 */
static const char OPTIMO[] =
"**** STOPPED AT A POINT WITH NO IMPROVEMENT AFTER 1 ITERATIONS (of 500)\n"
"**** last global step failed to locate a lower point (usual when starting "
"AT the optimum)\n"
"\nLog-likelihood = -718.287406\n";

/* drtran.c:3279-3280 */
static const char TOPE[] =
"**** *** NO CONVERGENCE *** AFTER 500 ITERATIONS (of 500)\n"
"**** ITERATION LIMIT REACHED\n"
"\nLog-likelihood = -1000.5\n";

/* drtran.c:3281-3282, con el ifault del evaluador */
static const char PASOS[] =
"**** *** NO CONVERGENCE *** AFTER 37 ITERATIONS (of 500)\n"
"**** five consecutive steps of max length\n"
"**** ifault = 3 (estimates not reliable)\n"
"\nLog-likelihood = -912.0\n";

/* Un .out que ni llego a estimar. */
static const char NADA[] =
"Error: the transfer network has a CYCLE: the system is\n"
"       simultaneous and cannot be cast as a triangular\n";

/* Lee un fichero entero. */
static char *slurp( const char *path )
{
   FILE  *f = fopen( path, "rb" );
   char  *b;
   long   n;

   if ( !f ) return NULL;
   fseek( f, 0, SEEK_END );  n = ftell( f );  fseek( f, 0, SEEK_SET );
   b = malloc( n + 1 );
   if ( b ) { n = (long) fread( b, 1, n, f ); b[n] = 0; }
   fclose( f );
   return b;
}

/* Con un fichero por argumento, no se prueba nada inventado: se lee lo que el
 * motor ACABA de escribir y se dice como lo interpreta. El guion comprueba
 * luego que coincide con lo que el motor puso.                          */
static int desde_fichero( const char *path )
{
   VerdictInfo v;
   char       *t = slurp( path );

   if ( !t ) { fprintf( stderr, "no pude leer %s\n", path ); return 2; }

   verdict_parse( t, &v );
   printf( "frase: %s\n", v.frase );
   printf( "ver: %d\niters: %d\nmaxits: %d\nifault: %d\nok: %d\n",
           (int) v.ver, v.iters, v.maxits, v.ifault, verdict_ok( &v ) );
   if ( v.tiene_logl ) printf( "logl: %.6f\n", v.logl );
   free( t );
   return 0;
}

int main( int argc, char **argv )
{
   VerdictInfo v;

   if ( argc >= 3 && !strcmp( argv[1], "--file" ) )
      return desde_fichero( argv[2] );

   printf( "El m6, que converge por el parametro:\n" );
   verdict_parse( M6, &v );
   printf( "   \"%s\"\n", v.frase );
   ok( v.ver == VER_PARAMETRO,   "  se lee como convergencia por el parametro" );
   ok( v.iters == 349 && v.maxits == 500, "  349 iteraciones de 500" );
   ok( v.tiene_logl && fabs( v.logl + 1701.348826 ) < 1e-6,
       "  y la verosimilitud, -1701.348826" );
   ok( verdict_ok( &v ), "  cuenta como optimo" );
   ok( v.ifault == 0, "  sin ifault" );

   printf( "\nEl diagonal, que converge por el gradiente y no dice de cuantas:\n" );
   verdict_parse( DIAG, &v );
   printf( "   \"%s\"\n", v.frase );
   ok( v.ver == VER_GRADIENTE,   "  convergencia por el gradiente" );
   ok( v.iters == 11 && v.maxits == -1,
       "  11 iteraciones, y el tope no se sabe (-1, no 0)" );
   ok( v.tiene_logl && fabs( v.logl + 744.923779 ) < 1e-6,
       "  la verosimilitud sale del \":\" y no del \"=\"" );

   /* --- EL CASO QUE IMPORTA --------------------------------------------- */
   printf( "\nParado sin mejora: el .pre YA ERA el optimo.\n" );
   verdict_parse( OPTIMO, &v );
   printf( "   \"%s\"\n", v.frase );
   ok( v.ver == VER_SIN_MEJORA, "  se lee como parada sin mejora" );
   ok( verdict_ok( &v ),
       "  y CUENTA COMO OPTIMO, aunque el titular suene a fracaso" );
   ok( strstr( v.frase, "NO IMPROVEMENT" ) != NULL,
       "  la frase del motor se conserva tal cual" );

   printf( "\nLos dos que no convergen:\n" );
   verdict_parse( TOPE, &v );
   printf( "   \"%s\"\n", v.frase );
   ok( v.ver == VER_ITERACIONES, "  tope de iteraciones" );
   ok( !verdict_ok( &v ),        "  y NO cuenta como optimo" );
   ok( v.iters == 500 && v.maxits == 500,
       "  gasto las 500, que es justo lo que dice que paso" );

   verdict_parse( PASOS, &v );
   printf( "   \"%s\"\n", v.frase );
   ok( v.ver == VER_PASOS,  "  cinco pasos de longitud maxima" );
   ok( !verdict_ok( &v ),   "  tampoco es optimo" );
   ok( v.ifault == 3,       "  y el ifault del evaluador, 3" );

   printf( "\nUn .out que no llego a estimar:\n" );
   verdict_parse( NADA, &v );
   ok( v.ver == VER_NADA,   "  no hay veredicto, y no se inventa" );
   ok( !verdict_ok( &v ),   "  ni cuenta como optimo" );
   ok( v.iters == -1 && !v.tiene_logl,
       "  ni iteraciones ni verosimilitud" );

   verdict_parse( NULL, &v );
   ok( v.ver == VER_NADA, "  y con NULL tampoco se cae" );

   /* Los cinco se distinguen entre si: si dos cayeran en el mismo cajon, la
    * pantalla diria lo mismo de cosas distintas.                        */
   {
   static const char *t[] = { M6, DIAG, OPTIMO, TOPE, PASOS };
   Veredicto vs[5];
   int i, j, distintos = 1;

   for ( i = 0; i < 5; i++ ) { verdict_parse( t[i], &v ); vs[i] = v.ver; }
   for ( i = 0; i < 5; i++ )
      for ( j = i + 1; j < 5; j++ )
         if ( vs[i] == vs[j] && !( i == 0 && j == 1 ) ) distintos = 0;
   ok( distintos,
       "los cinco desenlaces caen en cajones distintos (salvo los dos de convergencia)" );
   }

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
