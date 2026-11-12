/*
 * outfcst.c -- ver outfcst.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "outfcst.h"

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

/* El siguiente campo separado por espacios. Devuelve donde sigue, o NULL.
 * Un campo que sea exactamente "-" es un HUECO, no un numero negativo: es la
 * unica ambiguedad de este formato y hay que resolverla asi.            */
static const char *campo( const char *p, char *b, size_t size, int *hueco )
{
   size_t n = 0;

   while ( *p == ' ' || *p == '\t' ) p++;
   if ( !*p ) return NULL;
   while ( *p && *p != ' ' && *p != '\t' && n + 1 < size ) b[n++] = *p++;
   b[n] = 0;
   *hueco = ( n == 1 && b[0] == '-' );
   return p;
}

/* Una fila de la tabla:  FECHA  nivel sd  varper sd  varanu sd  err
 * donde cada sd y el err pueden ser "-". La fecha es "4/1993" o " 4/1993". */
static int lee_fila( const char *l, OfFila *f )
{
   char        b[64];
   const char *p = l;
   int         hueco, i;
   double     *num[7];
   int        *tiene[7];
   double      basura;
   int         basura_i;

   memset( f, 0, sizeof *f );

   p = campo( p, b, sizeof b, &hueco );
   if ( !p || hueco ) return 0;
   /* la fecha: digitos, una barra, digitos; o, en una serie anual, el anio
    * solo, con sus cuatro cifras (y no otra cosa: «1» es la fila de un
    * horizonte de la evaluacion, no un anio).                          */
   {
   char *barra = strchr( b, '/' );

   if ( barra ) {
      if ( !isdigit( (unsigned char) b[0] ) ||
           !isdigit( (unsigned char) barra[1] ) ) return 0;
   } else if ( strlen( b ) != 4 || strspn( b, "0123456789" ) != 4 ||
               atoi( b ) < 1000 )
      return 0;
   snprintf( f->fecha, sizeof f->fecha, "%.15s", b );
   }

   num[0] = &f->nivel;    tiene[0] = &basura_i;
   num[1] = &f->sd_nivel; tiene[1] = &f->tiene_sd;
   num[2] = &f->var_per;  tiene[2] = &basura_i;
   num[3] = &f->sd_per;   tiene[3] = &basura_i;
   num[4] = &f->var_anu;  tiene[4] = &basura_i;
   num[5] = &f->sd_anu;   tiene[5] = &basura_i;
   num[6] = &f->err;      tiene[6] = &f->tiene_err;
   (void) basura;

   for ( i = 0; i < 7; i++ ) {
      char *fin;

      p = campo( p, b, sizeof b, &hueco );
      if ( !p ) return i >= 1;          /* con el nivel basta para ser fila */
      if ( hueco ) { *tiene[i] = 0; continue; }
      *num[i] = strtod( b, &fin );
      if ( fin == b ) return 0;         /* no era un numero: no es una fila */
      *tiene[i] = 1;
   }
   return 1;
}

int of_parse( const char *texto, Forecast *f )
{
   const char *p = texto;
   char        l[512];
   OfSerie    *ser = NULL;
   int         en_eval = 0;

   memset( f, 0, sizeof *f );
   f->decomp = -1;
   if ( !texto ) return 1;

   while ( ( p = linea( p, l, sizeof l ) ) != NULL ) {
      char   nm[OF_NOMBRE];
      int    n1, n2, n3;
      double v1, v2, v3;

      /* --- una serie nueva ---------------------------------------------- */
      if ( strstr( l, "VARIABLE NAME" ) ) {
         const char *dp = strchr( l, ':' );

         if ( dp && sscanf( dp + 1, " %63s", nm ) == 1 &&
              f->ns < OF_MAX_SER ) {
            ser = &f->s[f->ns++];
            memset( ser, 0, sizeof *ser );
            snprintf( ser->nombre, sizeof ser->nombre, "%s", nm );
         }
         continue;
      }
      if ( ser && strstr( l, "FORECAST ORIGIN" ) ) {
         const char *dp = strchr( l, ':' );

         if ( dp && sscanf( dp + 1, " %15s", nm ) == 1 )
            snprintf( ser->origen, sizeof ser->origen, "%.15s", nm );
         if ( strstr( l, "LEAD TIME" ) )
            sscanf( strstr( l, "LEAD TIME" ), "LEAD TIME: %d", &ser->lead );
         continue;
      }
      /* el formato de fuf lo da en su linea: «LEAD TIME FOR FORECASTING: 8» */
      if ( ser && strstr( l, "LEAD TIME" ) && strchr( l, ':' ) ) {
         sscanf( strchr( l, ':' ) + 1, " %d", &ser->lead );
         continue;
      }

      /* Una fila se reconoce PORQUE EMPIEZA POR UNA FECHA, no por estar entre
       * lineas de "+---": la cabecera de la tabla lleva dos de esas por
       * dentro, asi que un interruptor queda invertido justo para los datos.
       * Con la fecha por delante no hay ambiguedad y no hay estado.      */
      if ( ser ) {
         OfFila fila;

         if ( lee_fila( l, &fila ) ) {
            if ( ser->nf < OF_MAX_FILA ) {
               ser->f[ser->nf++] = fila;
               if ( fila.tiene_sd ) ser->nprev++;
            }
            continue;
         }
      }

      /* --- la descomposicion de la varianza ----------------------------- */
      if ( strstr( l, "FORECAST ERROR VARIANCE DECOMPOSITION" ) ) {
         ser = NULL;
         f->decomp = 1;
         continue;
      }
      if ( f->decomp == 1 && strstr( l, "NOT REPORTED" ) ) { f->decomp = 0; continue; }

      /* --- la evaluacion fuera de muestra ------------------------------- */
      if ( strstr( l, "RECURSIVE FORECAST EVALUATION" ) ) {
         ser = NULL;  en_eval = 1;
         f->tiene_ev = 1;
         continue;
      }
      if ( !en_eval ) continue;

      if ( strstr( l, "Output" ) && strchr( l, ':' ) &&
           sscanf( strchr( l, ':' ) + 1, " %63s", nm ) == 1 ) {
         snprintf( f->ev.salida, sizeof f->ev.salida, "%s", nm );
         continue;
      }
      if ( strstr( l, "Origins" ) &&
           sscanf( strchr( l, ':' ) + 1, " %d (from obs %d to %d)",
                   &n1, &n2, &n3 ) == 3 ) {
         f->ev.origenes = n1;  f->ev.desde = n2;  f->ev.hasta = n3;
         continue;
      }
      if ( strstr( l, "Horizon" ) && strchr( l, ':' ) &&
           sscanf( strchr( l, ':' ) + 1, " %d", &n1 ) == 1 ) {
         f->ev.horizonte = n1;
         continue;
      }
      /* "    1     31     0.182023     0.255132      0.2244" */
      if ( sscanf( l, " %d %d %lf %lf %lf", &n1, &n2, &v1, &v2, &v3 ) == 5 &&
           f->ev.nh < OF_MAX_HOR ) {
         OfHoriz *h = &f->ev.h[f->ev.nh++];

         h->h = n1;  h->n = n2;  h->mae = v1;  h->rmse = v2;  h->mape = v3;
         continue;
      }
      if ( strstr( l, "Per-origin errors written" ) ) { en_eval = 0; continue; }
   }

   return ( f->ns || f->tiene_ev ) ? 0 : 1;
}

static char *lee_todo( const char *path )
{
   FILE *fp = fopen( path, "rb" );
   char *b;
   long  n;

   if ( !fp ) return NULL;
   fseek( fp, 0, SEEK_END );  n = ftell( fp );  fseek( fp, 0, SEEK_SET );
   b = malloc( n + 1 );
   if ( !b ) { fclose( fp ); return NULL; }
   n = (long) fread( b, 1, n, fp );
   b[n] = 0;
   fclose( fp );
   return b;
}

int of_parse_file( const char *path, Forecast *f )
{
   char *b = lee_todo( path );
   int   rc;

   memset( f, 0, sizeof *f );
   f->decomp = -1;
   if ( !b ) return -1;

   rc = of_parse( b, f );
   free( b );
   return rc;
}

/* Una linea en blanco: solo espacios (y el \r de un .out de Windows). */
static int en_blanco( const char *l )
{
   while ( *l == ' ' || *l == '\t' || *l == '\r' ) l++;
   return !*l;
}

int of_tabla( const char *texto, const char *nombre, char *buf, size_t size )
{
   const char *p = texto, *ini = NULL, *fin = NULL, *q;
   char        l[512];
   int         filas = 0;

   if ( size ) buf[0] = 0;
   if ( !texto || !nombre || size < 2 ) return 1;

   /* «FORECAST REPORT» y, unas lineas despues, «VARIABLE NAME: nombre». El
    * principio del bloque es el primero; la serie la dice el segundo.   */
   while ( p && *p ) {
      q = linea( p, l, sizeof l );
      if ( strstr( l, "FORECAST REPORT" ) ) ini = p;
      else if ( ini && strstr( l, "VARIABLE NAME" ) ) {
         char nm[OF_NOMBRE];
         const char *dp = strchr( l, ':' );

         if ( dp && sscanf( dp + 1, " %63s", nm ) == 1 && !strcmp( nm, nombre ) ) {
            p = q;
            break;
         }
         ini = NULL;
      }
      p = q;
   }
   if ( !ini || !p ) return 1;

   /* El final: la linea que cierra despues de las filas. En el formato de
    * drtran es un «+---»; en el de fuf, una linea en blanco.           */
   while ( p && *p ) {
      OfFila fila;

      q = linea( p, l, sizeof l );
      if ( lee_fila( l, &fila ) ) filas++;
      else if ( filas > 0 ) {
         const char *t = l;

         while ( *t == ' ' ) t++;
         fin = ( *t == '+' ) ? q : p;
         if ( *t == '+' || en_blanco( l ) ) break;
         fin = NULL;
      }
      p = q;
   }
   if ( !filas ) return 1;
   if ( !fin ) fin = p ? p : texto + strlen( texto );

   {
   size_t n = (size_t)( fin - ini );

   if ( n >= size ) n = size - 1;
   memcpy( buf, ini, n );
   /* sin el salto de linea del final: la ventana pone el suyo */
   while ( n > 0 && ( buf[n - 1] == '\n' || buf[n - 1] == '\r' ) ) n--;
   buf[n] = 0;
   }
   return 0;
}

int of_tabla_file( const char *path, const char *nombre, char *buf, size_t size )
{
   char *b = lee_todo( path );
   int   rc;

   if ( size ) buf[0] = 0;
   if ( !b ) return 1;
   rc = of_tabla( b, nombre, buf, size );
   free( b );
   return rc;
}

const OfSerie *of_serie( const Forecast *f, const char *nombre )
{
   int i;

   for ( i = 0; i < f->ns; i++ )
      if ( !strcmp( f->s[i].nombre, nombre ) ) return &f->s[i];
   return NULL;
}
