/*
 * sistema.c -- ver sistema.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sistema.h"
#include "fue_pre_reader.h"

static void copia( char *d, size_t n, const char *s )
{
   size_t k = strlen( s ? s : "" );
   if ( !n ) return;
   if ( k >= n ) k = n - 1;
   memcpy( d, s ? s : "", k );
   d[k] = 0;
}

/* Los ordenes de una lista de factores: «6», «1·12» (regular por anual).
 * Los vectores del lector van de 1 a n, como los del motor.             */
static void ordenes( char *out, size_t n, int nr, const int *pr, int na, const int *pa, int s )
{
   char b[32];
   int i;

   out[0] = 0;
   for ( i = 1; i <= nr; i++ ) {
      snprintf( b, sizeof b, "%s%d", out[0] ? "+" : "", pr[i] );
      strncat( out, b, n - strlen( out ) - 1 );
   }
   for ( i = 1; i <= na; i++ ) {
      snprintf( b, sizeof b, "%s%d", out[0] ? "·" : "", pa[i] * ( s > 0 ? s : 1 ) );
      strncat( out, b, n - strlen( out ) - 1 );
   }
   if ( !out[0] ) copia( out, n, "-" );
}

static int termina( const char *s, const char *suf )
{
   size_t a = strlen( s ), b = strlen( suf );
   return a >= b && strcmp( s + a - b, suf ) == 0;
}

int sis_anade( Sistema *s, const char *ruta, char *why, size_t n )
{
   struct Tusmodel tm;
   struct Tseries  ts;
   real          **dm = NULL;
   SisSerie       *x;
   int             i;

   if ( why && n ) why[0] = 0;
   if ( s->n >= SIS_MAX_SER ) {
      if ( why ) snprintf( why, n, "Un sistema lleva como mucho %d series.", SIS_MAX_SER );
      return 1;
   }
   memset( &tm, 0, sizeof tm );
   memset( &ts, 0, sizeof ts );
   if ( read_fue_pre( ruta, &tm, &ts, &dm ) != 0 ) {
      if ( why ) snprintf( why, n, "%s no es un fichero de fue que drvarma sepa leer.", ruta );
      return 1;
   }
   for ( i = 0; i < s->n; i++ )
      if ( ts.name && strcmp( s->s[i].nombre, ts.name ) == 0 ) {
         if ( why ) snprintf( why, n, "%s ya está en el sistema (de %s).", ts.name, s->s[i].ruta );
         free_fue_pre( &tm, &ts, dm );
         return 1;
      }

   x = &s->s[s->n];
   memset( x, 0, sizeof *x );
   copia( x->ruta, sizeof x->ruta, ruta );
   copia( x->nombre, sizeof x->nombre, ts.name ? ts.name : "?" );
   x->es_pre   = termina( ruta, ".pre" );
   x->freq     = ts.freq;
   x->nobs     = ts.nobs;
   x->ini_per  = ts.begtime;  x->ini_anio = ts.begyear;
   {  /* el lector no rellena el final: se cuenta desde el principio */
      int f = ts.freq > 0 ? ts.freq : 1;
      long fin = (long) ts.begyear * f + ts.begtime - 1 + ts.nobs - 1;
      x->fin_per = (int)( fin % f ) + 1;
      x->fin_anio = (int)( fin / f );
   }
   x->lambda   = tm.boxlam;
   x->d        = tm.nrdiff;
   x->D        = tm.nadiff;
   x->ndet     = tm.NdetVar;
   x->media    = tm.Imu != 0 || tm.mu != 0;
   ordenes( x->ar, sizeof x->ar, tm.NumAr1, tm.p1, tm.NumAr2, tm.p2, tm.sper );
   ordenes( x->ma, sizeof x->ma, tm.NumMa1, tm.q1, tm.NumMa2, tm.q2, tm.sper );
   s->n++;
   free_fue_pre( &tm, &ts, dm );
   return 0;
}

void sis_quita( Sistema *s, int i )
{
   if ( i < 0 || i >= s->n ) return;
   memmove( &s->s[i], &s->s[i + 1], (size_t)( s->n - i - 1 ) * sizeof s->s[0] );
   s->n--;
}

void sis_mueve( Sistema *s, int i, int j )
{
   SisSerie t;

   if ( i < 0 || i >= s->n || j < 0 || j >= s->n || i == j ) return;
   t = s->s[i];
   if ( i < j ) memmove( &s->s[i], &s->s[i + 1], (size_t)( j - i ) * sizeof t );
   else         memmove( &s->s[j + 1], &s->s[j], (size_t)( i - j ) * sizeof t );
   s->s[j] = t;
}

static long absoluta( int freq, int anio, int per )
{
   return (long) anio * freq + per - 1;
}

/* La primera observacion ESTACIONARIA: las diferencias se comen d + s*D. */
static long primera( const SisSerie *x )
{
   return absoluta( x->freq, x->ini_anio, x->ini_per ) + x->d + (long) x->D * x->freq;
}

void sis_fecha( int freq, long abs, char *out, size_t n )
{
   if ( freq < 1 ) freq = 1;
   snprintf( out, n, "%d/%d", (int)( abs % freq ) + 1, (int)( abs / freq ) );
}

int sis_ventana( const Sistema *s, SisVentana *v, char *why, size_t n )
{
   int i;

   memset( v, 0, sizeof *v );
   if ( why && n ) why[0] = 0;
   if ( s->n < 1 ) {
      if ( why ) snprintf( why, n, "No hay series." );
      return 1;
   }
   v->freq = s->s[0].freq;
   v->hasta = absoluta( v->freq, s->s[0].fin_anio, s->s[0].fin_per );
   v->desde = primera( &s->s[0] );
   for ( i = 1; i < s->n; i++ ) {
      const SisSerie *x = &s->s[i];
      long fin;
      char a[32], b[32];
      if ( x->freq != v->freq ) {
         if ( why ) snprintf( why, n, "%s tiene %d observaciones por año y %s %d: "
                              "no se pueden modelar juntas. Rehaz una de las dos "
                              "con la misma frecuencia.",
                              x->nombre, x->freq, s->s[0].nombre, v->freq );
         return 1;
      }
      fin = absoluta( v->freq, x->fin_anio, x->fin_per );
      if ( fin != v->hasta ) {
         sis_fecha( v->freq, v->hasta, a, sizeof a );
         sis_fecha( v->freq, fin, b, sizeof b );
         if ( why ) snprintf( why, n, "Las series no acaban en la misma fecha: %s acaba "
                              "en %s y %s en %s. drvarma alinea por el final y "
                              "emparejaría observaciones de fechas distintas. Rehaz "
                              "los modelos sobre la misma muestra.",
                              s->s[0].nombre, a, x->nombre, b );
         return 1;
      }
      if ( primera( x ) > v->desde ) v->desde = primera( x );
   }
   v->nobs = (int)( v->hasta - v->desde + 1 );
   for ( i = 0; i < s->n; i++ )
      v->recorta[i] = (int)( v->desde - primera( &s->s[i] ) );
   return 0;
}

int sis_diferenciadas( const Sistema *s )
{
   int i, k = 0;
   for ( i = 0; i < s->n; i++ ) k += s->s[i].d + s->s[i].D > 0;
   return k;
}

int sis_orden_puerta( const Sistema *s, const char *salida, const char **argv, int max )
{
   int i, k = 0;

   if ( s->n + 5 > max ) return -1;
   for ( i = 0; i < s->n; i++ ) argv[k++] = s->s[i].ruta;
   argv[k++] = "0";
   argv[k++] = "0";
   argv[k++] = "-diagcov";
   argv[k++] = "-o";
   argv[k++] = salida;
   return k;
}
