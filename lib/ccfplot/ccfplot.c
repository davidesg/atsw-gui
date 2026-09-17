/*
 * ccfplot.c -- ver ccfplot.h.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ccfplot.h"

/* Margenes del panel, en puntos. Salen del prototipo aprobado: 360 x 126 con
 * el dibujo dentro de un marco que deja sitio a las etiquetas.            */
#define M_IZQ   34.0
#define M_DER    8.0
#define M_ARR   14.0
#define M_ABA   16.0

#define FUENTE  FD_HELV
#define PT       7.0

int ccf_lags_por_defecto( int freq )
{
   switch ( freq ) {
      case 1:  return  7;      /* anual      */
      case 4:  return 15;      /* trimestral */
      case 12: return 12;      /* mensual: GraphMaker deja elegir 8..39 */
      default: return freq > 1 ? 3 * freq : 7;
   }
}

/* Cada cuantos retardos se rotula el eje. El prototipo pone 2 al anio y 12 al
 * mes; al trimestre, 4.                                                   */
static int paso_eje( int lags )
{
   if ( lags <= 8  ) return 2;
   if ( lags <= 20 ) return 4;
   return 12;
}

/* La escala vertical: la del prototipo es +-0.4 con marcas cada 0.2, pero si
 * la ccf o la banda se salen, se agranda para que quepan.                 */
static double escala( const double *corr, int n, double banda )
{
   double m = banda, e;
   int    i;

   for ( i = 0; i < n; i++ )
      if ( fabs( corr[i] ) > m ) m = fabs( corr[i] );

   for ( e = 0.4; e < 1.05; e += 0.2 )
      if ( m <= e * 0.98 ) return e;
   return 1.0;
}

void ccf_draw( FDFig *f, double x, double y, double w, double h,
               const double *corr, int lags, int nobs,
               const char *entrada, const char *salida,
               double q, int df )
{
   double px = x + M_IZQ, py = y + M_ABA;
   double pw = w - M_IZQ - M_DER, ph = h - M_ABA - M_ARR;
   double banda = nobs > 0 ? 2.0 / sqrt( (double) nobs ) : 0.0;
   double esc   = escala( corr, 2 * lags + 1, banda );
   double cy    = py + ph / 2.0;                 /* el cero                */
   double dx    = pw / ( 2.0 * lags + 1.0 );     /* ancho de un retardo    */
   char   s[128];
   int    k, paso;

   /* --- el cero, y las bandas de +-2/sqrt(N) ---------------------------- */
   fd_gray( f, 0.0 );
   fd_linewidth( f, 0.4 );
   fd_line( f, px, cy, px + pw, cy );

   if ( banda > 0.0 && banda < esc ) {
      double b = ( banda / esc ) * ( ph / 2.0 );

      fd_dash( f, 2.0, 2.0 );
      fd_line( f, px, cy + b, px + pw, cy + b );
      fd_line( f, px, cy - b, px + pw, cy - b );
      fd_dash( f, 0.0, 0.0 );
   }

   /* --- la vertical del retardo cero, que separa los dos lados ---------- */
   fd_dash( f, 1.5, 1.5 );
   fd_line( f, px + pw / 2.0, py, px + pw / 2.0, py + ph );
   fd_dash( f, 0.0, 0.0 );

   /* --- las barras ------------------------------------------------------ */
   /* GraphMaker las pone al 21% del paso; aqui, el 21% a cada lado.        */
   fd_linewidth( f, dx * 0.42 );
   for ( k = -lags; k <= lags; k++ ) {
      double v  = corr[k + lags];
      double bx = px + pw / 2.0 + k * dx;
      double by = cy + ( v / esc ) * ( ph / 2.0 );

      if ( by > py + ph ) by = py + ph;
      if ( by < py )      by = py;
      if ( fabs( v ) > 1e-12 ) fd_line( f, bx, cy, bx, by );
   }
   fd_linewidth( f, 0.4 );

   /* --- el marco -------------------------------------------------------- */
   fd_line( f, px, py, px + pw, py );
   fd_line( f, px, py + ph, px + pw, py + ph );
   fd_line( f, px, py, px, py + ph );
   fd_line( f, px + pw, py, px + pw, py + ph );

   /* --- el eje de retardos, simetrico ----------------------------------- */
   paso = paso_eje( lags );
   for ( k = -lags; k <= lags; k++ )
      if ( k % paso == 0 ) {
         double bx = px + pw / 2.0 + k * dx;

         fd_line( f, bx, py, bx, py + 2.0 );
         snprintf( s, sizeof s, "%d", k );
         fd_text( f, bx, py - 9.0, FUENTE, PT, FD_CENTER, s );
      }

   /* --- el eje de correlacion ------------------------------------------- */
   {
   double v;

   for ( v = -esc; v <= esc + 1e-9; v += esc / 2.0 ) {
      double vy = cy + ( v / esc ) * ( ph / 2.0 );

      fd_line( f, px - 2.0, vy, px, vy );
      snprintf( s, sizeof s, "%.1f", fabs( v ) < 1e-9 ? 0.0 : v );
      fd_text( f, px - 4.0, vy - 2.4, FUENTE, PT, FD_RIGHT, s );
   }
   }

   /* --- el titulo: la ENTRADA primero ----------------------------------- */
   /* En los retardos positivos se representa la influencia de la segunda
    * serie sobre la primera; por eso el nombre de la entrada va delante.  */
   snprintf( s, sizeof s, "%s \055 %s",
             entrada ? entrada : "?", salida ? salida : "?" );
   fd_text( f, px, y + h - 8.0, FUENTE, PT + 0.5, FD_LEFT, s );

   /* --- el estadistico de Hosking --------------------------------------- */
   if ( df >= 0 ) {
      snprintf( s, sizeof s, "P ( %d ) = %.1f", df, q );
      fd_text( f, px + pw, y + h - 8.0, FUENTE, PT + 0.5, FD_RIGHT, s );
   }

   /* --- que lado es cual, dicho en el propio grafico -------------------- */
   /* A los extremos: en el centro se pisaban. Y es donde toca, porque lo que
    * nombran es el lado entero, no el retardo cero.                       */
   fd_gray( f, 0.45 );
   fd_text( f, px + 3.0,      py + 3.0, FUENTE, PT - 1.0, FD_LEFT,
            "exogeneidad" );
   fd_text( f, px + pw - 3.0, py + 3.0, FUENTE, PT - 1.0, FD_RIGHT,
            "transferencia" );
   fd_gray( f, 0.0 );
}

int ccf_write_eps( const char *filename,
                   const double *corr, int lags, int nobs,
                   const char *entrada, const char *salida,
                   double q, int df )
{
   /* El tamano del prototipo aprobado: BoundingBox 57 50 417 176. */
   FDFig *f = fd_fig_new( 360.0, 126.0 );
   int    rc;

   if ( !f ) return 1;
   ccf_draw( f, 0.0, 0.0, 360.0, 126.0, corr, lags, nobs,
             entrada, salida, q, df );
   rc = fd_write_eps( f, filename );
   fd_fig_free( f );
   return rc;
}
