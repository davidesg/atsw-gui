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

/* P( chi2_k > x ) para k entero, por la recurrencia exacta
 *
 *     sf(1) = erfc( sqrt(x/2) )
 *     sf(2) = exp( -x/2 )
 *     sf(k) = sf(k-2) + (x/2)^(k/2-1) e^(-x/2) / Gamma(k/2)
 *
 * Elemental y sin tablas, que es lo que este modulo puede permitirse. La
 * cabecera decia que aqui no se calculaba ninguna probabilidad; ahora se
 * calcula UNA, y es la que define lo que es un episodio.              */
static double chi2_cola( double x, int k )
{
   double sf, t;
   int    i;

   if ( x <= 0.0 ) return 1.0;
   if ( k < 1 ) return 0.0;

   if ( k % 2 )                      /* impar: se arranca en 1 */
      { sf = erfc( sqrt( x / 2.0 ) ); i = 1; }
   else
      { sf = exp( -x / 2.0 ); i = 2; }

   for ( i += 2; i <= k; i += 2 )
       {
       t = pow( x / 2.0, (double) i / 2.0 - 1.0 ) * exp( -x / 2.0 )
           / tgamma( (double) i / 2.0 );
       sf += t;
       }
   return ( sf > 1.0 ) ? 1.0 : ( ( sf < 0.0 ) ? 0.0 : sf );
}

int an_episodios( const double *z, int n, double umbral,
                  AnEpisodio *out, int max )
{
   double limite;
   char  *usado;
   int    ne = 0, lmax, i;

   if ( z == NULL || out == NULL || n < 1 || max < 1 ) return 0;
   if ( umbral <= 0.0 ) return 0;

   /* EL LISTON: lo que cuesta declarar un extremo aislado en el umbral,
      dividido por K. Ver la cabecera.                                */
   limite = erfc( umbral / sqrt( 2.0 ) ) / AN_K;

   lmax = ( n < AN_LMAX ) ? n : AN_LMAX;
   usado = calloc( (size_t) n, 1 );
   if ( !usado ) return 0;

   /* AVARICIA: gana el tramo mas significativo, y lo que queda no puede
      pisarlo. Es un convenio, no un teorema, y se dice.              */
   while ( ne < max )
       {
       double mejor_p = limite;
       int    mejor_i = -1, mejor_L = 0, L;

       for ( i = 0; i < n; i++ )
           {
           double S = 0.0;

           if ( usado[i] ) continue;
           for ( L = 1; L <= lmax && i + L <= n; L++ )
               {
               double p;

               /* EL TRAMO ES SOLIDO: un periodo callado no se traga, lo
                  parte. Ver la cabecera -- sin esto, dos picos separados
                  por silencio salian como un suceso de cuatro.       */
               if ( usado[i + L - 1] ) break;
               if ( fabs( z[i + L - 1] ) < AN_ACTIVO ) break;
               S += z[i + L - 1] * z[i + L - 1];
               p = chi2_cola( S, L );
               if ( p <= mejor_p )
                  { mejor_p = p; mejor_i = i; mejor_L = L; }
               }
           }
       if ( mejor_i < 0 ) break;

       out[ne].desde = mejor_i;
       out[ne].hasta = mejor_i + mejor_L - 1;
       out[ne].n     = mejor_L;
       out[ne].p     = mejor_p;
       out[ne].z_max = z[mejor_i];
       out[ne].i_max = mejor_i;
       for ( i = mejor_i; i <= out[ne].hasta; i++ )
           {
           if ( fabs( z[i] ) > fabs( out[ne].z_max ) )
               { out[ne].z_max = z[i]; out[ne].i_max = i; }
           usado[i] = 1;
           }
       ne++;
       }
   free( usado );

   /* Por posicion, que es como se leen en la ventana y en el .out. */
   for ( i = 1; i < ne; i++ )
       {
       AnEpisodio t = out[i];
       int        j = i - 1;

       while ( j >= 0 && out[j].desde > t.desde ) { out[j+1] = out[j]; j--; }
       out[j+1] = t;
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


/* --- la normalidad ------------------------------------------------------- */

/* Los momentos de lo que la mascara deja pasar. Ver anomalos.h: aqui NO se
 * rellenan los huecos, se quitan.                                        */
static void momentos( const double *z, int n, const char *fuera, AnNormal *r )
{
   double m2 = 0.0, m3 = 0.0, m4 = 0.0, s = 0.0;
   int    i, k = 0;

   memset( r, 0, sizeof *r );
   for ( i = 0; i < n; i++ ) if ( !fuera || !fuera[i] ) { s += z[i]; k++; }
   if ( k < 2 ) return;
   r->n = k;
   r->media = s / k;

   for ( i = 0; i < n; i++ ) if ( !fuera || !fuera[i] )
       {
       double d = z[i] - r->media;

       m2 += d * d;
       m3 += d * d * d;
       m4 += d * d * d * d;
       }
   m2 /= k; m3 /= k; m4 /= k;
   if ( m2 <= 0.0 ) return;

   r->sd   = sqrt( m2 );
   r->skew = m3 / ( m2 * r->sd );
   r->kurt = m4 / ( m2 * m2 ) - 3.0;
   r->jb   = (double) k / 6.0 * ( r->skew * r->skew + r->kurt * r->kurt / 4.0 );
}

int an_normalidad( const double *z, int n, const int *omitir, int nomitir,
                   AnNormal *con, AnNormal *sinellos )
{
   char *fuera;
   int   i;

   if ( !z || n < 2 || !con || !sinellos ) return 1;

   momentos( z, n, NULL, con );

   fuera = calloc( (size_t) n, 1 );
   if ( !fuera ) { *sinellos = *con; return 1; }
   for ( i = 0; i < nomitir; i++ )
       if ( omitir && omitir[i] >= 0 && omitir[i] < n ) fuera[omitir[i]] = 1;
   momentos( z, n, fuera, sinellos );
   free( fuera );
   return 0;
}
