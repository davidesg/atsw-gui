/*
 * outdiag.c -- ver outdiag.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "outdiag.h"

/* ------------------------------------------------------------------------ */
/* Recorrer el texto por lineas                                              */
/* ------------------------------------------------------------------------ */

/* Copia la linea que empieza en p a b[size]; devuelve el principio de la
 * siguiente, o NULL al acabarse el texto.                                */
static const char *linea( const char *p, char *b, size_t size )
{
   const char *fin;
   size_t      n;

   if ( !p || !*p ) { if ( size ) b[0] = 0; return NULL; }

   fin = strchr( p, '\n' );
   n   = fin ? (size_t)( fin - p ) : strlen( p );
   if ( n >= size ) n = size - 1;
   memcpy( b, p, n );
   b[n] = 0;
   return fin ? fin + 1 : p + strlen( p );
}

/* El numero que sigue a una etiqueta dentro de la linea. */
static int tras( const char *l, const char *etiqueta, double *v )
{
   const char *p = strstr( l, etiqueta );

   if ( !p ) return 0;
   return sscanf( p + strlen( etiqueta ), "%lf", v ) == 1;
}

/* ------------------------------------------------------------------------ */

static void test_cero( OdTest *t )
{
   t->hay = 0; t->q = 0.0; t->df = 0; t->p = 0.0; t->tiene_p = 0;
}

int od_parse( const char *texto, Diagnosis *d )
{
   const char *p = texto;
   char        l[512];
   OdSerie    *ser = NULL;
   OdEnlace   *enl = NULL;
   int         en_acf = 0, i;

   memset( d, 0, sizeof *d );
   for ( i = 0; i < OD_MAX_SER;  i++ ) test_cero( &d->s[i].lb );
   for ( i = 0; i < OD_MAX_LINK; i++ ) {
      test_cero( &d->e[i].transfer );
      test_cero( &d->e[i].exogen );
      d->e[i].exogen_signif = -1;
   }
   test_cero( &d->hosking );
   test_cero( &d->jb );

   if ( !texto ) return 1;

   while ( ( p = linea( p, l, sizeof l ) ) != NULL ) {
      double v1, v2;
      int    n1, n2;

      /* --- una serie de residuos nueva ---------------------------------- */
      if ( strstr( l, "--- Residual series " ) ) {
         char nm[OD_NOMBRE];

         en_acf = 0;
         if ( d->ns < OD_MAX_SER &&
              sscanf( l, " --- Residual series %63s", nm ) == 1 ) {
            ser = &d->s[d->ns++];
            snprintf( ser->nombre, sizeof ser->nombre, "%s", nm );
         }
         continue;
      }

      /* --- la ACF: interesa el ULTIMO Ljung-Box de su columna ----------- */
      if ( strstr( l, "Autocorrelation function" ) ) { en_acf = 1; continue; }
      if ( strstr( l, "Partial autocorrelation" ) )  { en_acf = 0; continue; }

      if ( en_acf && ser ) {
         /* Las lineas de la ACF acaban en "| <Q> <DF>" cuando toca escalon.
          * Se lee de derecha a izquierda: la ultima barra y lo que sigue. */
         char *barra = strrchr( l, '|' );

         if ( barra && sscanf( barra + 1, "%lf %d", &v1, &n1 ) == 2 ) {
            ser->lb.hay = 1;  ser->lb.q = v1;  ser->lb.df = n1;
         }
         continue;
      }

      /* --- los estadisticos de la serie --------------------------------- */
      if ( ser ) {
         /* Ojo con sscanf: " %d observations:" devuelve 1 en cuanto lee el
          * numero, aunque el literal que sigue NO case. Cualquier linea que
          * empiece por un numero --el grafico de la serie, el histograma, la
          * CCF-- se colaba por aqui. El literal se comprueba aparte.      */
         if ( strstr( l, " observations:" ) &&
              sscanf( l, " %d observations:", &n1 ) == 1 ) {
            ser->nobs = n1; continue;
         }
         if ( tras( l, "Mean:", &v1 ) && !strstr( l, "error" ) ) {
            ser->media = v1; ser->tiene_stats = 1; continue;
         }
         if ( tras( l, "Standard deviation:", &v1 ) ) { ser->sd   = v1; continue; }
         if ( tras( l, "Skewness:", &v1 ) )           { ser->skew = v1; continue; }
         if ( tras( l, "Kurtosis:", &v1 ) )           { ser->kurt = v1; continue; }

         /* "  20 values outside (-1,+1): 31.25 % (31.74 % expected)" */
         if ( strstr( l, "values outside (-1,+1)" ) &&
              sscanf( strstr( l, ":" ) + 1, "%lf %% (%lf", &v1, &v2 ) == 2 ) {
            ser->fuera1 = v1; ser->esp1 = v2; ser->tiene_hist = 1; continue;
         }
         if ( strstr( l, "values outside (-2,+2)" ) &&
              sscanf( strstr( l, ":" ) + 1, "%lf %% (%lf", &v1, &v2 ) == 2 ) {
            ser->fuera2 = v1; ser->esp2 = v2; continue;
         }
      }

      /* Las CCF entre residuos ya no son de ninguna serie: si no se suelta
       * aqui, sus lineas --que empiezan por el retardo-- se leen como si
       * fueran de la ultima serie.                                      */
      if ( strstr( l, "Cross-correlation functions between residuals" ) ) {
         ser = NULL;  en_acf = 0;
         continue;
      }

      /* --- el portmanteau multivariante --------------------------------- */
      if ( strstr( l, "Hosking's Multivariate Portmanteau" ) ) {
         ser = NULL;  en_acf = 0;
         if ( sscanf( strstr( l, "(lag" ), "(lag %d", &n1 ) == 1 )
            d->hosking_lag = n1;
         /* la siguiente linea trae Q(df) = x, p-value = y */
         p = linea( p, l, sizeof l );
         if ( sscanf( l, " Q(%d) = %lf, p-value = %lf", &n1, &v1, &v2 ) == 3 ) {
            d->hosking.hay = 1;  d->hosking.df = n1;
            d->hosking.q = v1;   d->hosking.p = v2;  d->hosking.tiene_p = 1;
         }
         p = linea( p, l, sizeof l );
         d->hosking_blanco = strstr( l, "Cannot reject" ) != NULL;
         continue;
      }

      if ( strstr( l, "Multivariate Jarque-Bera" ) ) {
         ser = NULL;  en_acf = 0;
         p = linea( p, l, sizeof l );
         if ( sscanf( l, " JB(%d) = %lf, p-value = %lf", &n1, &v1, &v2 ) == 3 ) {
            d->jb.hay = 1;  d->jb.df = n1;
            d->jb.q = v1;   d->jb.p = v2;  d->jb.tiene_p = 1;
         }
         p = linea( p, l, sizeof l );
         d->jb_normal = strstr( l, "REJECT" ) == NULL;
         continue;
      }

      /* --- un enlace ---------------------------------------------------- */
      if ( strstr( l, "TRANSFER FUNCTION ADEQUACY" ) ) {
         char nm[OD_NOMBRE];

         ser = NULL;  en_acf = 0;
         if ( d->ne < OD_MAX_LINK ) {
            enl = &d->e[d->ne++];
            enl->adecuado = enl->exogeno = -1;
            if ( sscanf( strstr( l, "input" ), "input %d (%63[^)])",
                         &n1, nm ) == 2 ) {
               enl->num = n1;
               snprintf( enl->entrada, sizeof enl->entrada, "%s", nm );
            }
         }
         continue;
      }

      if ( !enl ) continue;

      /* "    Q(15) = 5.2644   [17 lags - 2 parameters of nu(B)]"          */
      if ( sscanf( l, " Q(%d) = %lf", &n1, &v1 ) == 2 ) {
         OdTest *t = strstr( l, "p-value" ) ? &enl->exogen
                   : ( enl->transfer.hay ? &enl->exogen : &enl->transfer );

         t->hay = 1;  t->df = n1;  t->q = v1;
         if ( tras( l, "p-value =", &v2 ) ) { t->p = v2; t->tiene_p = 1; }
         if ( strstr( l, "significant]" ) ) {
            const char *b = strrchr( l, '[' );
            if ( b ) sscanf( b + 1, "%d", &enl->exogen_signif );
         }
         continue;
      }
      /* la p de la transferencia va en su propia linea */
      if ( !strncmp( l, "    p-value = ", 14 ) &&
           enl->transfer.hay && !enl->transfer.tiene_p &&
           tras( l, "p-value =", &v1 ) ) {
         enl->transfer.p = v1;  enl->transfer.tiene_p = 1;
         continue;
      }

      if ( strstr( l, "The transfer is" ) ) {
         const char *q = l;

         while ( *q == ' ' ) q++;
         snprintf( enl->veredicto, sizeof enl->veredicto, "%.199s", q );
         enl->adecuado = strstr( l, "NOT adequate" ) == NULL;
         continue;
      }
      if ( strstr( l, "behaves as exogenous" ) )      { enl->exogeno = 1; continue; }
      if ( strstr( l, "not exogenous" ) ||
           strstr( l, "FEEDBACK" ) )                  {
         /* la linea de la cabecera tambien dice FEEDBACK: solo cuenta si es
          * el veredicto, que va despues del contraste.                  */
         if ( enl->exogen.hay ) enl->exogeno = 0;
         continue;
      }

      (void) n2;
   }

   /* La verosimilitud, si esta */
   {
   const char *q = strstr( texto, "Log-likelihood" );

   if ( q ) {
      const char *eq = strchr( q, '=' ), *dp = strchr( q, ':' );

      if ( !eq || ( dp && dp < eq ) ) eq = dp;
      if ( eq && sscanf( eq + 1, "%lf", &d->logl ) == 1 ) d->tiene_logl = 1;
   }
   }

   return ( d->ns || d->ne || d->hosking.hay ) ? 0 : 1;
}

int od_parse_file( const char *path, Diagnosis *d )
{
   FILE *f = fopen( path, "rb" );
   char *b;
   long  n;
   int   rc;

   memset( d, 0, sizeof *d );
   if ( !f ) return -1;

   fseek( f, 0, SEEK_END );  n = ftell( f );  fseek( f, 0, SEEK_SET );
   b = malloc( n + 1 );
   if ( !b ) { fclose( f ); return -1; }
   n = (long) fread( b, 1, n, f );
   b[n] = 0;
   fclose( f );

   rc = od_parse( b, d );
   free( b );
   return rc;
}

int od_no_adecuados( const Diagnosis *d )
{
   int i, n = 0;

   for ( i = 0; i < d->ne; i++ ) if ( d->e[i].adecuado == 0 ) n++;
   return n;
}

int od_no_exogenos( const Diagnosis *d )
{
   int i, n = 0;

   for ( i = 0; i < d->ne; i++ ) if ( d->e[i].exogeno == 0 ) n++;
   return n;
}
