/*
 * anomalos.c -- episodios y calibración.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "anomalos.h"

/* ------------------------------------------------------------------------ */
/* El umbral                                                                 */
/* ------------------------------------------------------------------------ */

/* El máximo de n normales tipificadas crece como sqrt(2 ln n). Se usa eso y
 * no una constante, con un suelo de 2,5: por debajo, "extremo" deja de
 * significar nada y todo el mundo es noticia.                            */
double an_umbral( int n )
{
   double u;

   if ( n < 2 ) return 2.5;
   u = sqrt( 2.0 * log( (double) n ) );
   return ( u < 2.5 ) ? 2.5 : u;
}


/* ------------------------------------------------------------------------ */
/* Episodios                                                                 */
/* ------------------------------------------------------------------------ */

int an_episodios( const double *z, int n, double umbral, int ventana,
                  AnEpisodio *out, int max )
{
   int i, ne = 0;

   if ( z == NULL || out == NULL || n < 1 || max < 1 ) return 0;
   if ( ventana < 0 ) ventana = 0;

   for ( i = 0; i < n; i++ )
       {
       if ( fabs( z[i] ) < umbral ) continue;

       /* ¿Cuelga del episodio anterior? El hueco se mide entre el extremo
          de antes y éste, no entre el principio del episodio y éste: un
          suceso largo no debe tragarse lo que venga detrás.           */
       if ( ne > 0 && i - out[ne - 1].hasta <= ventana + 1 )
           {
           out[ne - 1].hasta = i;
           out[ne - 1].n++;
           if ( fabs( z[i] ) > fabs( out[ne - 1].z_max ) )
               { out[ne - 1].z_max = z[i]; out[ne - 1].i_max = i; }
           continue;
           }

       if ( ne >= max ) break;
       out[ne].desde = out[ne].hasta = out[ne].i_max = i;
       out[ne].n     = 1;
       out[ne].z_max = z[i];
       ne++;
       }
   return ne;
}


/* ------------------------------------------------------------------------ */
/* La ACF, con omisiones                                                     */
/* ------------------------------------------------------------------------ */

/* z~[t] = (z[t]-mu) si se retiene, 0 si no. mu sobre las RETENIDAS.
 * Devuelve cuantas se retuvieron.                                        */
static int desvia( const double *z, int n, const char *fuera, double *zt )
{
   double mu = 0.0;
   int    i, k = 0;

   for ( i = 0; i < n; i++ )
       if ( !fuera[i] ) { mu += z[i]; k++; }
   if ( k > 0 ) mu /= k;

   for ( i = 0; i < n; i++ )
       zt[i] = fuera[i] ? 0.0 : z[i] - mu;
   return k;
}

/* r(k) = suma z~[t] z~[t+k] / suma z~[t]^2, con el MISMO denominador para
 * todos los retardos: es lo que los hace comparables entre si.         */
static void acf_de( const double *zt, int n, int lags, double *r )
{
   double c0 = 0.0;
   int    k, t;

   for ( t = 0; t < n; t++ ) c0 += zt[t] * zt[t];
   for ( k = 1; k <= lags; k++ )
       {
       double c = 0.0;

       for ( t = 0; t + k < n; t++ ) c += zt[t] * zt[t + k];
       r[k] = ( c0 > 0.0 ) ? c / c0 : 0.0;
       }
   r[0] = 1.0;
}

/* LA PACF SALE DE LA ACF -- Durbin-Levinson. Esa es la propiedad que hace
 * barato todo esto: UNA omision da LAS DOS funciones.                    */
static void pacf_de( const double *r, int lags, double *phi )
{
   double a[AN_MAX_LAG + 1], prev[AN_MAX_LAG + 1];
   double v;
   int    k, j;

   memset( a, 0, sizeof a );
   memset( prev, 0, sizeof prev );
   v = 1.0;

   for ( k = 1; k <= lags; k++ )
       {
       double num = r[k];

       for ( j = 1; j < k; j++ ) num -= prev[j] * r[k - j];
       a[k] = ( v > 1e-15 ) ? num / v : 0.0;
       phi[k] = a[k];

       for ( j = 1; j < k; j++ ) a[j] = prev[j] - a[k] * prev[k - j];
       v *= ( 1.0 - a[k] * a[k] );
       memcpy( prev, a, sizeof a );
       }
   phi[0] = 1.0;
}

/* El veredicto de un retardo: lo que decide es CRUZAR LA BANDA, no cuanto
 * se mueve. Un movimiento grande que deja el retardo dentro de banda las dos
 * veces no cambia ninguna decision de identificacion.                    */
static AnVeredicto juzga( double con, double b_con, double sin_, double b_sin )
{
   int fc = fabs( con )  > b_con;
   int fs = fabs( sin_ ) > b_sin;

   if ( fc == fs ) return AN_IGUAL;
   return fs ? AN_ENMASCARADA : AN_FABRICADA;
}

int an_calibra( const double *z, int n, const int *omitir, int nomitir,
                int lags, AnCalibra *out )
{
   char   *fuera;
   double *zt;
   double  r_con[AN_MAX_LAG + 1], p_con[AN_MAX_LAG + 1];
   double  r_sin[AN_MAX_LAG + 1], p_sin[AN_MAX_LAG + 1];
   int     i, k;

   if ( out == NULL ) return 1;
   memset( out, 0, sizeof *out );
   if ( z == NULL || n < 4 ) return 1;
   if ( lags < 1 ) lags = 1;
   if ( lags > AN_MAX_LAG ) lags = AN_MAX_LAG;
   if ( lags > n / 2 ) lags = n / 2;

   fuera = calloc( (size_t) n, 1 );
   zt    = calloc( (size_t) n, sizeof *zt );
   if ( fuera == NULL || zt == NULL ) { free( fuera ); free( zt ); return 1; }

   /* --- con el anomalo: nadie fuera --- */
   out->n_con = desvia( z, n, fuera, zt );
   acf_de( zt, n, lags, r_con );
   pacf_de( r_con, lags, p_con );

   /* --- sin el --- */
   for ( i = 0; i < nomitir; i++ )
       if ( omitir[i] >= 0 && omitir[i] < n ) fuera[omitir[i]] = 1;
   out->n_sin = desvia( z, n, fuera, zt );
   acf_de( zt, n, lags, r_sin );
   pacf_de( r_sin, lags, p_sin );

   free( fuera );
   free( zt );

   /* LA BANDA CAMBIA CON n, y por eso son dos: quitar observaciones
      ensancha la banda, asi que comparar contra una sola bandaharia
      parecer que algo sale de banda cuando lo que paso fue que la banda
      se estrecho.                                                     */
   out->banda_con = ( out->n_con > 0 ) ? 1.96 / sqrt( (double) out->n_con ) : 0;
   out->banda_sin = ( out->n_sin > 0 ) ? 1.96 / sqrt( (double) out->n_sin ) : 0;

   out->nlags = lags;
   for ( k = 1; k <= lags; k++ )
       {
       AnLag *l = &out->l[k - 1];

       l->lag      = k;
       l->acf_con  = r_con[k];  l->acf_sin  = r_sin[k];
       l->pacf_con = p_con[k];  l->pacf_sin = p_sin[k];
       l->acf  = juzga( r_con[k], out->banda_con, r_sin[k], out->banda_sin );
       l->pacf = juzga( p_con[k], out->banda_con, p_sin[k], out->banda_sin );
       if ( l->acf != AN_IGUAL || l->pacf != AN_IGUAL ) out->cambia++;
       }
   return 0;
}

void an_q( const AnCalibra *c, int m, double *q_con, double *q_sin )
{
   double qc = 0.0, qs = 0.0;
   int    k;

   if ( c == NULL ) return;
   if ( m > c->nlags ) m = c->nlags;

   for ( k = 1; k <= m; k++ )
       {
       const AnLag *l = &c->l[k - 1];

       if ( c->n_con > k )
           qc += l->acf_con * l->acf_con / ( c->n_con - k );
       if ( c->n_sin > k )
           qs += l->acf_sin * l->acf_sin / ( c->n_sin - k );
       }

   if ( q_con ) *q_con = (double) c->n_con * ( c->n_con + 2 ) * qc;
   if ( q_sin ) *q_sin = (double) c->n_sin * ( c->n_sin + 2 ) * qs;
}

const char *an_veredicto_es( AnVeredicto v )
{
   switch ( v )
       {
       case AN_ENMASCARADA: return "enmascarada";
       case AN_FABRICADA:   return "fabricada";
       default:             return "igual";
       }
}
