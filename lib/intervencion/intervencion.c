/*
 * intervencion.c -- la lectura escalar del peldaño 1. Ver intervencion.h.
 *
 * Es el puerto de art.escalera.lectura_escalar. Las frases de la razón son
 * las mismas porque son parte del resultado: un informe que dice «escalón»
 * sin decir por qué no se puede revisar.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "intervencion.h"

static int por_obs( const void *a, const void *b )
{
   const IvExtremo *x = (const IvExtremo *) a, *y = (const IvExtremo *) b;

   return ( x->obs > y->obs ) - ( x->obs < y->obs );
}

int iv_primer_extremo( const IvExtremo *ext, int next )
{
   int i, mejor = -1;

   for ( i = 0; i < next; i++ )
       if ( mejor < 0 || ext[i].obs < ext[mejor].obs ) mejor = i;
   return mejor;
}

const char *iv_palabra( IvForma f )
{
   switch ( f )
       {
       case IV_ESCALON: return "step";
       case IV_IMPULSO: return "impulse";
       case IV_RAMPA:   return "ramp";
       case IV_COMPIMP: return "compimp";
       }
   return "step";
}

const char *iv_nombre_es( IvForma f )
{
   switch ( f )
       {
       case IV_ESCALON: return "escalón";
       case IV_IMPULSO: return "impulso";
       case IV_RAMPA:   return "rampa";
       case IV_COMPIMP: return "impulso compensado";
       }
   return "escalón";
}

int iv_linea( IvForma f, int freq, int per, int anno, char *out, size_t n )
{
   if ( !out || n < 8 ) return 1;
   if ( freq <= 1 )
      snprintf( out, n, "%s %d", iv_palabra( f ), anno );
   else
      {
      if ( per < 1 || per > freq ) return 1;
      snprintf( out, n, "%s %d %d", iv_palabra( f ), per, anno );
      }
   return 0;
}

int iv_lectura( const IvExtremo *ext, int next, int d, IvLectura *out )
{
   IvExtremo e[64];
   int       i, n = next;

   if ( !out ) return 1;
   memset( out, 0, sizeof *out );
   out->forma   = IV_ESCALON;
   out->peldano = 1;

   if ( n > (int)( sizeof e / sizeof e[0] ) ) n = (int)( sizeof e / sizeof e[0] );
   if ( n < 1 || !ext )
      {
      snprintf( out->razon, sizeof out->razon,
                "no hay extremos que leer; se toma el escalón, que es la "
                "lectura por defecto de un suceso sin firma de reversión" );
      return 0;
      }
   memcpy( e, ext, (size_t) n * sizeof e[0] );
   qsort( e, (size_t) n, sizeof e[0], por_obs );

   if ( d >= 1 )
      {
      /* Los residuos viven en ∇: un impulso de nivel deja DOS extremos que
         se cancelan, y un escalón de nivel deja UNO solo.              */
      if ( n == 2 && e[1].obs == e[0].obs + 1 )
         {
         double z0 = e[0].z, z1 = e[1].z;
         double pico = fabs( z0 ) > fabs( z1 ) ? fabs( z0 ) : fabs( z1 );
         double resto = fabs( z0 + z1 );

         if ( z0 * z1 < 0.0 && pico > 0.0 && resto <= IV_TOL_CANCELA * pico )
            {
            out->forma = IV_IMPULSO;
            snprintf( out->razon, sizeof out->razon,
                "dos extremos contiguos que se CANCELAN (%+.2f y %+.2f, suma "
                "%+.2f = %.0f%% del pico): en ∇ ésa es la firma de un IMPULSO "
                "de nivel, que revierte", z0, z1, z0 + z1, 100.0 * resto / pico );
            return 0;
            }
         if ( z0 * z1 < 0.0 && pico > 0.0 )
            {
            snprintf( out->razon, sizeof out->razon,
                "dos extremos contiguos de signo opuesto que NO se cancelan "
                "(%+.2f y %+.2f, suma %+.2f = %.0f%% del pico, por encima del "
                "%.0f%%): lo que no revierte es un ESCALÓN. El segundo extremo "
                "es cola del suceso, no la mitad compensadora de un impulso",
                z0, z1, z0 + z1, 100.0 * resto / pico,
                100.0 * IV_TOL_CANCELA );
            return 0;
            }
         }
      if ( n == 1 )
         {
         snprintf( out->razon, sizeof out->razon,
             "un extremo aislado (%+.2f) sin vecino que lo compense: en ∇ un "
             "impulso solo es la firma de un ESCALÓN de nivel, que no revierte",
             e[0].z );
         return 0;
         }
      snprintf( out->razon, sizeof out->razon,
          "%d extremos que no forman un par compensado: la lectura escalar es "
          "el escalón, y la forma de verdad está más arriba en la escalera", n );
      out->peldano = 2;
      snprintf( out->aviso, sizeof out->aviso,
          "El episodio no se resuelve con una sola intervención escalar: mira "
          "si el escalón deja un vecino anómalo al reestimar." );
      return 0;
      }

   /* d = 0 -- los residuos YA están en el nivel, y el diccionario se
      invierte: lo que allí era un par compensado, aquí es un extremo solo. */
   if ( n == 1 )
      {
      out->forma = IV_IMPULSO;
      snprintf( out->razon, sizeof out->razon,
          "un solo extremo en el NIVEL (%+.2f), sin diferenciar: eso es un "
          "impulso, no un escalón", e[0].z );
      return 0;
      }
   {
   int positivos = 0, negativos = 0;

   for ( i = 0; i < n; i++ ) { if ( e[i].z > 0.0 ) positivos++; else negativos++; }
   if ( positivos == 0 || negativos == 0 )
      {
      snprintf( out->razon, sizeof out->razon,
          "%d extremos del mismo signo en el NIVEL: una racha sostenida es un "
          "escalón", n );
      return 0;
      }
   snprintf( out->razon, sizeof out->razon,
       "%d extremos de signos mezclados en el NIVEL; se toma el escalón como "
       "lectura por defecto", n );
   out->peldano = 2;
   snprintf( out->aviso, sizeof out->aviso,
       "Signos mezclados sin diferenciar: la lectura escalar es la de menos "
       "compromiso, no la que dice el dato." );
   }
   return 0;
}


/* --- la superposición ---------------------------------------------------- */

/* Los coeficientes de (1−B)^d (1−B^s)^D en c[0..*grado]. */
#define IV_MAX_OP  80

static int operador( int d, int D, int s, double *c )
{
   double t[IV_MAX_OP + 1];
   int    grado = 0, i, k;

   for ( i = 0; i <= IV_MAX_OP; i++ ) c[i] = 0.0;
   c[0] = 1.0;
   if ( d < 0 ) d = 0;
   if ( D < 0 ) D = 0;
   if ( s < 1 ) s = 1;
   if ( d + s * D > IV_MAX_OP ) return -1;

   for ( k = 0; k < d; k++ )
       {
       for ( i = 0; i <= grado + 1; i++ ) t[i] = 0.0;
       for ( i = 0; i <= grado; i++ ) { t[i] += c[i]; t[i+1] -= c[i]; }
       grado++;
       for ( i = 0; i <= grado; i++ ) c[i] = t[i];
       }
   for ( k = 0; k < D; k++ )
       {
       for ( i = 0; i <= grado + s; i++ ) t[i] = 0.0;
       for ( i = 0; i <= grado; i++ ) { t[i] += c[i]; t[i+s] -= c[i]; }
       grado += s;
       for ( i = 0; i <= grado; i++ ) c[i] = t[i];
       }
   return grado;
}

/* El regresor EN EL NIVEL, en el índice u. */
static double nivel( IvForma f, int t0, int u )
{
   switch ( f )
       {
       case IV_ESCALON: return ( u >= t0 ) ? 1.0 : 0.0;
       case IV_IMPULSO: return ( u == t0 ) ? 1.0 : 0.0;
       case IV_RAMPA:   return ( u >= t0 ) ? (double)( u - t0 + 1 ) : 0.0;
       case IV_COMPIMP: return ( u == t0 ) ? 1.0 : ( ( u == t0 + 1 ) ? -1.0 : 0.0 );
       }
   return 0.0;
}

int iv_huella( IvForma f, int t0, int d, int D, int s, int base,
               double *h, int n )
{
   double c[IV_MAX_OP + 1];
   int    grado, i, k;

   if ( !h || n < 1 ) return 1;
   grado = operador( d, D, s, c );
   if ( grado < 0 ) return 1;

   for ( i = 0; i < n; i++ )
       {
       double v = 0.0;

       for ( k = 0; k <= grado; k++ )
           if ( c[k] != 0.0 ) v += c[k] * nivel( f, t0, base + i - k );
       h[i] = v;
       }
   return 0;
}

int iv_ajusta( const double *h, const double *z, int n, IvAjuste *out )
{
   double hh = 0.0, hz = 0.0, zz = 0.0, ss = 0.0;
   int    i;

   if ( !out ) return 1;
   memset( out, 0, sizeof *out );
   if ( !h || !z || n < 1 ) return 1;

   for ( i = 0; i < n; i++ ) { hh += h[i] * h[i]; hz += h[i] * z[i]; zz += z[i] * z[i]; }
   out->escala = ( hh > 0.0 ) ? hz / hh : 0.0;

   for ( i = 0; i < n; i++ )
       {
       double r = z[i] - out->escala * h[i];

       ss += r * r;
       if ( fabs( r ) > fabs( out->resto ) ) { out->resto = r; out->i_resto = i; }
       }
   /* EL R2 CONTRA CERO, no contra la media: lo que la forma tiene que
      explicar es el residuo entero, y su media ya debería ser cero. Contra
      la media, una ventana con media distinta de cero regalaria ajuste. */
   out->r2 = ( zz > 0.0 ) ? 1.0 - ss / zz : 0.0;
   return 0;
}
