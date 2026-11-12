/*
 * fcomp.c -- ver fcomp.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gsl/gsl_cdf.h>

#include "fcomp.h"

typedef struct { int o, h; double e; } Fila;

/* Las filas de un CSV de drtran -C, en su orden. NULL si no se puede. */
static Fila *lee_csv( const char *path, int *n )
{
   FILE *fp = fopen( path, "r" );
   char  l[512];
   Fila *f = NULL;
   int   cap = 0;

   *n = 0;
   if ( !fp ) return NULL;
   while ( fgets( l, sizeof l, fp ) ) {
      Fila   x;
      double act, fc;

      /* la cabecera y cualquier linea que no sea de numeros, fuera */
      if ( sscanf( l, "%d,%d,%lf,%lf,%lf", &x.o, &x.h, &act, &fc, &x.e ) != 5 )
         continue;
      if ( *n == cap ) {
         Fila *g;

         cap = cap ? 2 * cap : 256;
         g = realloc( f, cap * sizeof *f );
         if ( !g ) { free( f ); fclose( fp ); *n = 0; return NULL; }
         f = g;
      }
      f[(*n)++] = x;
   }
   fclose( fp );
   return f;
}

/* Diebold-Mariano sobre d = perdida_A - perdida_B. dbar > 0: B mejor. */
static int dm( const double *la, const double *lb, int n, int h,
               double *stat, double *p )
{
   double dbar = 0.0, lrv = 0.0, *dc;
   int    i, k;

   if ( n < 2 ) return 0;
   dc = malloc( n * sizeof *dc );
   if ( !dc ) return 0;
   for ( i = 0; i < n; i++ ) dbar += la[i] - lb[i];
   dbar /= n;
   for ( i = 0; i < n; i++ ) dc[i] = la[i] - lb[i] - dbar;

   for ( i = 0; i < n; i++ ) lrv += dc[i] * dc[i];
   lrv /= n;
   {
   double l0 = lrv;

   for ( k = 1; k < h && k < n; k++ ) {
      double s = 0.0;

      for ( i = k; i < n; i++ ) s += dc[i] * dc[i - k];
      lrv += 2.0 * s / n;
   }
   /* varianza degenerada: se cae a la iid, como el guion de Python */
   if ( lrv <= 0.0 ) lrv = l0;
   }
   free( dc );
   if ( lrv <= 0.0 ) return 0;

   *stat = dbar / sqrt( lrv / n );
   {
   double hln = ( n + 1.0 - 2.0 * h + h * ( h - 1.0 ) / n ) / n;

   *stat *= sqrt( hln > 1e-9 ? hln : 1e-9 );
   }
   *p = 2.0 * gsl_cdf_tdist_Q( fabs( *stat ), n - 1.0 );
   return 1;
}

int fc_compara_csv( const char *csv_a, const char *csv_b, FcComp *c )
{
   Fila   *a, *b;
   int     na, nb, i, j, hmax = 0, rc = 1;
   int    *par;            /* para cada fila de A, la de B con su clave, o -1 */
   double *la, *lb;

   memset( c, 0, sizeof *c );
   a = lee_csv( csv_a, &na );
   b = lee_csv( csv_b, &nb );
   if ( !a || !b || !na || !nb ) { free( a ); free( b ); return 1; }

   /* Los pares por (origen, horizonte), en el orden de A: el de los origenes,
    * que es el que importa a la varianza HAC.                          */
   par = malloc( na * sizeof *par );
   la  = malloc( na * sizeof *la );
   lb  = malloc( na * sizeof *lb );
   if ( !par || !la || !lb ) goto fin;
   for ( i = 0; i < na; i++ ) {
      par[i] = -1;
      for ( j = 0; j < nb; j++ )
         if ( b[j].o == a[i].o && b[j].h == a[i].h ) { par[i] = j; break; }
      if ( par[i] >= 0 && a[i].h > hmax ) hmax = a[i].h;
   }
   if ( hmax > FC_MAX_HOR ) hmax = FC_MAX_HOR;

   /* los origenes comunes: los del horizonte 1 con pareja */
   for ( i = 0; i < na; i++ ) if ( a[i].h == 1 && par[i] >= 0 ) c->origenes++;

   for ( j = 1; j <= hmax; j++ ) {
      FcHoriz *H = &c->h[c->nh];
      double   sa = 0.0, sb = 0.0;
      int      n = 0;

      for ( i = 0; i < na; i++ ) {
         if ( a[i].h != j || par[i] < 0 ) continue;
         la[n] = a[i].e * a[i].e;
         lb[n] = b[par[i]].e * b[par[i]].e;
         sa += la[n];  sb += lb[n];
         n++;
      }
      if ( !n ) continue;
      H->h = j;
      H->n = n;
      H->rmse_a = sqrt( sa / n );
      H->rmse_b = sqrt( sb / n );
      H->ratio  = H->rmse_a > 0.0 ? H->rmse_b / H->rmse_a : 0.0;
      H->dm_ok  = dm( la, lb, n, j, &H->dm, &H->p );
      c->nh++;
   }
   rc = c->nh ? 0 : 1;

fin:
   free( par ); free( la ); free( lb ); free( a ); free( b );
   return rc;
}
