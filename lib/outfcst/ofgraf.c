/*
 * ofgraf.c -- ver ofgraf.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "ofgraf.h"
#include "fufplot.h"

/* «4/1993» o «1993»: el periodo y el anio. */
static int fecha_de( const char *s, int *per, int *anio )
{
   if ( sscanf( s, "%d/%d", per, anio ) == 2 ) return 1;
   if ( sscanf( s, "%d", anio ) == 1 ) { *per = 1; return 1; }
   return 0;
}

FDFig *of_grafico( const OfSerie *S, double lam, int tiene_lam, int freq )
{
   double *y, *b1, *b2, *err, sigma;
   int     i, L, i0, per, anio, nivel;
   FDFig  *fig;

   if ( !S ) return NULL;
   L  = S->nprev;
   i0 = S->nf - 2 * L;                  /* la primera de las L observadas  */
   if ( L < 2 || i0 < 0 || !fecha_de( S->f[i0].fecha, &per, &anio ) ) return NULL;
   if ( freq < 1 ) freq = 1;
   nivel = tiene_lam && lam < 0.0;

   y   = calloc( 2 * L, sizeof *y );
   b1  = calloc( 2 * L, sizeof *b1 );
   b2  = calloc( 2 * L, sizeof *b2 );
   err = calloc( L, sizeof *err );
   if ( !y || !b1 || !b2 || !err ) { free( y ); free( b1 ); free( b2 ); free( err ); return NULL; }

   for ( i = 0; i < 2 * L; i++ ) {
      const OfFila *f = &S->f[i0 + i];
      /* la variacion anual; con datos anuales, la del periodo, que es ella */
      double v  = ( freq > 1 ) ? f->var_anu : f->var_per;
      double sd = ( freq > 1 ) ? f->sd_anu  : f->sd_per;

      if ( nivel ) {
         /* La banda, como fuf: la transformada mas/menos dos desviaciones,
          * deshecha. La tabla da el nivel y la desviacion EN LA ESCALA
          * TRANSFORMADA (/refactor), y el refactor se va: z = (x^l - 1)/l. */
         double z = ( pow( f->nivel, lam ) - 1.0 ) / lam;

         y[i] = f->nivel;
         if ( i >= L ) {
            b1[i] = pow( ( z + 2.0 * f->sd_nivel ) * lam + 1.0, 1.0 / lam );
            b2[i] = pow( ( z - 2.0 * f->sd_nivel ) * lam + 1.0, 1.0 / lam );
         }
      } else {
         y[i] = v;
         if ( i >= L ) { b1[i] = v + sd; b2[i] = v - sd; }
      }
      if ( i < L ) err[i] = f->tiene_err ? f->err : 0.0;
   }
   sigma = S->f[i0 + L].sd_nivel;

   fig = fp_forecast( y, b1, b2, err, L, sigma, freq, anio, per,
                      nivel ? "LEVEL"
                            : ( tiene_lam && fabs( lam ) < 1e-8 ) ? "LRC anual (%)"
                                                                  : "Annual change" );
   free( y ); free( b1 ); free( b2 ); free( err );
   return fig;
}
