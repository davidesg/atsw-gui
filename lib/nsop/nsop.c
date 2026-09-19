/*
 * nsop.c -- ver nsop.h.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "nsop.h"

#define NSOP_MAX_GRADO 128
#define TOL 1e-8

/* Division exacta de polinomios con div[0] == 1, que es el caso de todos los
 * factores de aqui. Deja el cociente en q[0..n-m] y devuelve 1 si el resto es
 * cero; si no, no toca q.                                                 */
static int divide( const double *c, int n, const double *div, int m,
                   double *q )
{
   double tmp[NSOP_MAX_GRADO + 1], back[NSOP_MAX_GRADO + 1];
   double esc = 0.0;
   int    i, k, gq = n - m;

   if ( gq < 0 ) return 0;

   for ( i = 0; i <= gq; i++ ) {
      double v = c[i];

      for ( k = 1; k <= m && k <= i; k++ ) v -= div[k] * tmp[i - k];
      tmp[i] = v;
   }

   /* Multiplicar de vuelta y comparar: es la unica forma de saber que la
    * division fue exacta y no solo que el cociente cabia.               */
   for ( i = 0; i <= n; i++ ) {
      double v = 0.0;

      for ( k = 0; k <= m && k <= i; k++ )
         if ( i - k <= gq ) v += div[k] * tmp[i - k];
      back[i] = v;
      if ( fabs( c[i] ) > esc ) esc = fabs( c[i] );
   }
   if ( esc < 1.0 ) esc = 1.0;

   for ( i = 0; i <= n; i++ )
      if ( fabs( back[i] - c[i] ) > TOL * esc ) return 0;

   for ( i = 0; i <= gq; i++ ) q[i] = tmp[i];
   return 1;
}

int nsop_canon( const double *rnsop, int ornsop, int sper, NsopForm *o )
{
   double c[NSOP_MAX_GRADO + 1], q[NSOP_MAX_GRADO + 1], div[NSOP_MAX_GRADO + 1];
   int    n = ornsop, i, k;

   memset( o, 0, sizeof *o );
   if ( ornsop <= 0 || ornsop > NSOP_MAX_GRADO ) return ornsop > 0;

   /* El convenio del fichero: el operador es SUM (-rnsop[j]) B^j. */
   for ( i = 0; i <= n; i++ ) c[i] = -rnsop[i];

   /* --- 1. (1-B^s), EL MAXIMO ------------------------------------------- */
   if ( sper > 1 && sper <= NSOP_MAX_GRADO ) {
      for ( i = 0; i <= sper; i++ ) div[i] = 0.0;
      div[0] = 1.0;  div[sper] = -1.0;

      while ( n >= sper && divide( c, n, div, sper, q ) ) {
         n -= sper;
         for ( i = 0; i <= n; i++ ) c[i] = q[i];
         o->D++;
      }
   }

   /* --- 2. (1-B) --------------------------------------------------------- */
   div[0] = 1.0;  div[1] = -1.0;
   while ( n >= 1 && divide( c, n, div, 1, q ) ) {
      n -= 1;
      for ( i = 0; i <= n; i++ ) c[i] = q[i];
      o->d++;
   }

   /* --- 3. los irreducibles que sobran ----------------------------------- */
   if ( sper > 1 ) {
      int cambio = 1;

      while ( n > 0 && cambio && o->nf < NSOP_MAX_F ) {
         cambio = 0;

         for ( k = 1; k <= sper / 2 && !cambio; k++ ) {
            int m;

            if ( k == sper - k ) {              /* f = s/2 : (1 + B) */
               div[0] = 1.0;  div[1] = 1.0;  m = 1;
            } else {                            /* el par complejo   */
               div[0] = 1.0;
               div[1] = -2.0 * cos( 2.0 * M_PI * k / sper );
               div[2] = 1.0;  m = 2;
            }
            if ( n >= m && divide( c, n, div, m, q ) ) {
               n -= m;
               for ( i = 0; i <= n; i++ ) c[i] = q[i];
               o->f[o->nf++] = k;
               cambio = 1;
            }
         }
      }
   }

   o->resto = n > 0 ? n : 0;
   return o->resto != 0;
}

void nsop_texto( const NsopForm *o, int sper, char *out, size_t size )
{
   char b[64];
   int  i, pos = 0;

   out[0] = 0;

#define PON(s) do { int _n = (int) strlen(s); \
                    if ( pos + _n < (int) size ) { strcpy( out + pos, s ); \
                                                   pos += _n; } } while (0)

   if ( o->d == 1 ) PON( "(1-B)" );
   else if ( o->d > 1 ) { snprintf( b, sizeof b, "(1-B)^%d", o->d ); PON( b ); }

   if ( o->D == 1 ) { snprintf( b, sizeof b, "(1-B^%d)", sper ); PON( b ); }
   else if ( o->D > 1 ) {
      snprintf( b, sizeof b, "(1-B^%d)^%d", sper, o->D ); PON( b );
   }

   for ( i = 0; i < o->nf; i++ ) {
      snprintf( b, sizeof b, "[f=%d]", o->f[i] );
      PON( b );
   }
   if ( o->resto ) { snprintf( b, sizeof b, "[?grado %d]", o->resto ); PON( b ); }
   if ( pos == 0 ) PON( "1" );
#undef PON
}

void nsop_texto_f( const NsopForm *o, char *out, size_t size )
{
   int i, pos = 0;

   out[0] = 0;
   if ( o->resto ) { snprintf( out, size, "?" ); return; }
   if ( o->nf == 0 ) { snprintf( out, size, "\xe2\x80\x94" ); return; }  /* — */

   for ( i = 0; i < o->nf && pos + 4 < (int) size; i++ )
      pos += snprintf( out + pos, size - pos, "%s%d", i ? "," : "", o->f[i] );
}

int nsop_difiere( const NsopForm *o, int nrdiff, int nadiff )
{
   return o->d != nrdiff || o->D != nadiff;
}
