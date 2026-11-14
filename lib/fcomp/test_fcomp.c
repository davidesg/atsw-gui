/* test_fcomp.c -- la comparacion fuera de muestra, contra forecast_compare.py.
 *
 *   test_fcomp <datos/>
 *
 * Los dos CSV de datos/ los escribio drtran sobre tests/cases: el airline de
 * ES_CPI con WTI (B) y sin el, el diagonal -0 (A), con -estwin 180 -f 6. Los
 * numeros esperados son los de forecast_compare.py sobre esos mismos CSV.
 */

#include <stdio.h>
#include <math.h>

#include "fcomp.h"

static int fallos = 0;

static void okf( double v, double e, double tol, const char *que, int h )
{
   int bien = fabs( v - e ) <= tol;

   printf( "  h=%d %-10s %10.4f (%10.4f)  %s\n", h, que, v, e, bien ? "ok" : "FALLA" );
   if ( !bien ) fallos++;
}

int main( int argc, char **argv )
{
   /* forecast_compare.py diag.csv ev.csv --horizons 1,2,3,4,5,6 */
   static const double rA[6] = { 0.2185, 0.3389, 0.3855, 0.4397, 0.4817, 0.5111 };
   static const double rB[6] = { 0.2551, 0.4300, 0.5470, 0.6465, 0.7142, 0.7499 };
   static const double ra[6] = { 1.168, 1.269, 1.419, 1.470, 1.483, 1.467 };
   static const double dm[6] = { -1.41, -2.17, -2.20, -2.29, -2.30, -1.99 };
   static const double p[6]  = { 0.1679, 0.0379, 0.0359, 0.0295, 0.0286, 0.0555 };
   char   a[1024], b[1024];
   FcComp c;
   int    i;

   if ( argc < 2 ) { fprintf( stderr, "uso: test_fcomp <datos/>\n" ); return 2; }
   snprintf( a, sizeof a, "%s/airline_uni.csv", argv[1] );
   snprintf( b, sizeof b, "%s/airline_wti.csv", argv[1] );

   if ( fc_compara_csv( a, b, &c ) != 0 ) { printf( "no pude comparar\n" ); return 1; }
   printf( "%d horizontes, %d origenes comunes\n", c.nh, c.origenes );
   if ( c.nh != 6 || c.origenes != 31 ) { printf( "  FALLA: 6 y 31\n" ); fallos++; }

   for ( i = 0; i < c.nh && i < 6; i++ ) {
      okf( c.h[i].rmse_a, rA[i], 5e-5,  "RMSE A", i + 1 );
      okf( c.h[i].rmse_b, rB[i], 5e-5,  "RMSE B", i + 1 );
      okf( c.h[i].ratio,  ra[i], 5e-4,  "ratio",  i + 1 );
      okf( c.h[i].dm,     dm[i], 5e-3,  "DM",     i + 1 );
      okf( c.h[i].p,      p[i],  5e-5,  "p",      i + 1 );
   }

   /* fc_dm_errores, con los errores de los dos CSV emparejados a mano:
      el mismo DM y la misma p que fc_compara_csv, horizonte a horizonte */
   {
      FILE  *fa = fopen( a, "r" ), *fb = fopen( b, "r" );
      static double ea[6][256], ob[6][256];
      static int    oa[6][256], na[6], nb[6], obi[6][256];
      char   l[512];
      int    o, h, j, k, bien = 1;
      double x, y, e;

      while ( fa && fgets( l, sizeof l, fa ) )
         if ( sscanf( l, "%d,%d,%lf,%lf,%lf", &o, &h, &x, &y, &e ) == 5 && h >= 1 && h <= 6 && na[h-1] < 256 ) {
            oa[h-1][na[h-1]] = o; ea[h-1][na[h-1]++] = e;
         }
      while ( fb && fgets( l, sizeof l, fb ) )
         if ( sscanf( l, "%d,%d,%lf,%lf,%lf", &o, &h, &x, &y, &e ) == 5 && h >= 1 && h <= 6 && nb[h-1] < 256 ) {
            obi[h-1][nb[h-1]] = o; ob[h-1][nb[h-1]++] = e;
         }
      if ( fa ) fclose( fa );
      if ( fb ) fclose( fb );
      fc_compara_csv( a, b, &c );
      for ( h = 0; h < 6; h++ ) {
         double pa[256], pb[256], st, pv;
         int    n = 0;
         for ( j = 0; j < na[h]; j++ )
            for ( k = 0; k < nb[h]; k++ )
               if ( obi[h][k] == oa[h][j] ) { pa[n] = ea[h][j]; pb[n] = ob[h][k]; n++; break; }
         if ( !fc_dm_errores( pa, pb, n, h + 1, &st, &pv ) ||
              fabs( st - c.h[h].dm ) > 1e-12 || fabs( pv - c.h[h].p ) > 1e-12 ) bien = 0;
      }
      printf( "  fc_dm_errores: el mismo DM y la misma p, h = 1..6  %s\n", bien ? "ok" : "FALLA" );
      if ( !bien ) fallos++;
   }

   /* el mismo fichero contra si mismo: ratio 1 y sin contraste */
   if ( fc_compara_csv( a, a, &c ) == 0 ) {
      int bien = fabs( c.h[0].ratio - 1.0 ) < 1e-12 && !c.h[0].dm_ok;

      printf( "  un modelo contra si mismo: ratio 1, DM no se da  %s\n", bien ? "ok" : "FALLA" );
      if ( !bien ) fallos++;
   }
   if ( fc_compara_csv( a, "/no/existe.csv", &c ) == 1 ) printf( "  un fichero que no esta: 1  ok\n" );
   else { printf( "  un fichero que no esta: FALLA\n" ); fallos++; }

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
