/*
 * netfile.c -- ver netfile.h.
 *
 * net_read y net_topo vienen de engines/drtran/src/drtran.c (read_network,
 * series_index, topo_sort). La cuenta es la misma; lo unico que cambia es que
 * los globales pasan a ser argumentos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "netfile.h"

int net_series_index( const char * const *nombre, int nser, const char *tok )
{
   char *end;
   long  v = strtol( tok, &end, 10 );
   int   i;

   if ( *end == '\0' && v >= 1 && v <= nser ) return (int) v;

   for ( i = 1; i <= nser; i++ )
      if ( nombre[i] && strcasecmp( nombre[i], tok ) == 0 ) return i;
   return 0;
}

static int falla( NetError *e, NetErr k, int line, const char *tok )
{
   if ( e ) {
      e->err  = k;
      e->line = line;
      e->b = e->r = e->s = 0;
      snprintf( e->token, sizeof e->token, "%.63s", tok ? tok : "" );
   }
   return -1;
}

const char *net_error_en( const NetError *e, char *out, size_t size )
{
   if ( !e ) { if ( size ) out[0] = 0; return out; }

   switch ( e->err ) {
   case NET_OK:
      snprintf( out, size, "ok" ); break;
   case NET_ENOFILE:
      snprintf( out, size, "cannot open the network file %s", e->token ); break;
   case NET_ESYNTAX:
      snprintf( out, size, "bad line in the network file: %s", e->token ); break;
   case NET_EARROW:
      snprintf( out, size, "expected '<-' in the network file, found '%s'",
                e->token ); break;
   case NET_EUNKNOWN:
      snprintf( out, size, "unknown series in the network file: '%s'",
                e->token ); break;
   case NET_ESELF:
      snprintf( out, size, "a series cannot feed itself (%s)", e->token ); break;
   case NET_ENEG:
      snprintf( out, size, "b, r and s cannot be negative (%d %d %d)",
                e->b, e->r, e->s ); break;
   case NET_EMANY:
      snprintf( out, size, "too many links" ); break;
   }
   return out;
}

int net_read( const char *path, const char * const *nombre, int nser,
              NetLink *lnk, int max, NetError *e )
{
   FILE *f = fopen( path, "r" );
   char  line[256];
   int   n = 0, nlin = 0;

   if ( e ) { e->err = NET_OK; e->line = 0; e->token[0] = 0; }

   if ( !f ) return falla( e, NET_ENOFILE, 0, path );

   while ( fgets( line, sizeof line, f ) ) {
      char  lhs[64], flecha[8], rhs[64];
      int   b, r, s, io, ii;
      char *h = strchr( line, '#' );

      nlin++;
      if ( h ) *h = '\0';

      if ( sscanf( line, "%63s %7s %63s %d %d %d",
                   lhs, flecha, rhs, &b, &r, &s ) != 6 ) {
         char *c;
         int   blanca = 1;

         for ( c = line; *c; c++ )
            if ( !isspace( (unsigned char) *c ) ) blanca = 0;
         if ( blanca ) continue;
         {
         char corta[48];

         for ( c = line; *c; c++ ) if ( *c == '\n' ) *c = 0;
         snprintf( corta, sizeof corta, "%.47s", line );   /* recortar es lo que toca */
         fclose( f ); return falla( e, NET_ESYNTAX, nlin, corta );
         }
      }

      if ( strcmp( flecha, "<-" ) != 0 ) {
         fclose( f ); return falla( e, NET_EARROW, nlin, flecha );
      }

      io = net_series_index( nombre, nser, lhs );
      ii = net_series_index( nombre, nser, rhs );
      if ( io == 0 || ii == 0 ) {
         fclose( f ); return falla( e, NET_EUNKNOWN, nlin, io == 0 ? lhs : rhs );
      }
      if ( io == ii ) {
         fclose( f ); return falla( e, NET_ESELF, nlin, lhs );
      }
      if ( b < 0 || r < 0 || s < 0 ) {
         falla( e, NET_ENEG, nlin, lhs );
         if ( e ) { e->b = b; e->r = r; e->s = s; }
         fclose( f ); return -1;
      }
      if ( n >= max ) {
         fclose( f ); return falla( e, NET_EMANY, nlin, "" );
      }

      lnk[n].out = io;  lnk[n].inp = ii;
      lnk[n].b = b;  lnk[n].r = r;  lnk[n].s = s;
      n++;
   }

   fclose( f );
   return n;
}

int net_write( const char *path, const char * const *nombre,
               const NetLink *lnk, int nlinks, const char *cabecera )
{
   FILE *f = fopen( path, "w" );
   int   k;

   if ( !f ) return 1;

   if ( cabecera ) fprintf( f, "# %s\n", cabecera );
   fprintf( f, "# Formato:  SALIDA <- ENTRADA  b r s\n" );

   for ( k = 0; k < nlinks; k++ )
      fprintf( f, "%-8s <- %-8s   %d %d %d\n",
               nombre[lnk[k].out] ? nombre[lnk[k].out] : "?",
               nombre[lnk[k].inp] ? nombre[lnk[k].inp] : "?",
               lnk[k].b, lnk[k].r, lnk[k].s );

   return fclose( f ) != 0;
}

void net_perm_move( int nser, int de, int a, int *perm )
{
   int i;

   for ( i = 1; i <= nser; i++ ) perm[i] = i;
   if ( de < 1 || a < 1 || de > nser || a > nser || de == a ) return;

   perm[de] = a;
   if ( de < a ) for ( i = de + 1; i <= a; i++ ) perm[i] = i - 1;
   else          for ( i = a; i <= de - 1; i++ ) perm[i] = i + 1;
}

void net_perm_drop( int nser, int i, int *perm )
{
   int k;

   for ( k = 1; k <= nser; k++ ) perm[k] = k < i ? k : ( k == i ? 0 : k - 1 );
}

int net_remap( NetLink *lnk, int nlinks, const int *perm )
{
   int k, n = 0;

   for ( k = 0; k < nlinks; k++ ) {
      int o = perm[lnk[k].out], e = perm[lnk[k].inp];

      if ( o == 0 || e == 0 ) continue;      /* nombraba a la que se fue */
      lnk[n] = lnk[k];
      lnk[n].out = o;
      lnk[n].inp = e;
      n++;
   }
   return n;
}

int net_indegree( const NetLink *lnk, int nlinks, int i )
{
   int k, n = 0;

   for ( k = 0; k < nlinks; k++ ) if ( lnk[k].out == i ) n++;
   return n;
}

int net_outdegree( const NetLink *lnk, int nlinks, int i )
{
   int k, n = 0;

   for ( k = 0; k < nlinks; k++ ) if ( lnk[k].inp == i ) n++;
   return n;
}

/* Una serie solo se puede construir despues de TODAS las que la alimentan. Se
 * van pelando las que ya no tienen a nadie esperando delante; si al final
 * quedan series sin colocar, esas estan en un ciclo.                      */
int net_topo( const NetLink *lnk, int nlinks, int nser, int *topo )
{
   int  indeg[NET_MAX_SER + 1], i, k, nt = 0, cambio;
   char hecho[NET_MAX_SER + 1];

   if ( nser > NET_MAX_SER ) return 0;

   for ( i = 1; i <= nser; i++ ) { indeg[i] = 0; hecho[i] = 0; }
   for ( k = 0; k < nlinks; k++ ) indeg[lnk[k].out]++;

   do {
      cambio = 0;
      for ( i = 1; i <= nser; i++ ) {
         if ( hecho[i] || indeg[i] > 0 ) continue;
         topo[++nt] = i;
         hecho[i] = 1;
         cambio = 1;
         for ( k = 0; k < nlinks; k++ )
            if ( lnk[k].inp == i ) indeg[lnk[k].out]--;
      }
   } while ( cambio );

   return nt == nser;
}

/* Un ciclo concreto. Las que sobreviven al pelado estan todas en algun ciclo o
 * cuelgan de uno; desde cualquiera de ellas, seguir aristas hacia atras
 * --quien me alimenta-- acaba por repetir un nodo, y el tramo entre las dos
 * apariciones ES un ciclo.                                              */
int net_cycle( const NetLink *lnk, int nlinks, int nser, int *ciclo, int *n )
{
   int  indeg[NET_MAX_SER + 1];
   char sobra[NET_MAX_SER + 1];       /* no se pudo colocar: esta en un ciclo
                                         o cuelga de uno                    */
   int  orden[NET_MAX_SER + 1];       /* posicion en el camino, 0 = no visto */
   int  camino[NET_MAX_SER + 2];
   int  i, k, cambio, largo = 0, cur = 0;

   *n = 0;
   if ( nser > NET_MAX_SER ) return 0;

   for ( i = 1; i <= nser; i++ ) { indeg[i] = 0; sobra[i] = 1; orden[i] = 0; }
   for ( k = 0; k < nlinks; k++ ) indeg[lnk[k].out]++;

   /* el mismo pelado que net_topo, quedandonos con lo que sobra */
   do {
      cambio = 0;
      for ( i = 1; i <= nser; i++ ) {
         if ( !sobra[i] || indeg[i] > 0 ) continue;
         sobra[i] = 0;
         cambio = 1;
         for ( k = 0; k < nlinks; k++ )
            if ( lnk[k].inp == i ) indeg[lnk[k].out]--;
      }
   } while ( cambio );

   for ( i = 1; i <= nser; i++ ) if ( sobra[i] ) { cur = i; break; }
   if ( !cur ) return 0;                    /* la red es aciclica */

   /* Hacia atras --quien me alimenta-- por los que sobraron. Cada uno tiene
    * por fuerza alguno, porque si no habria caido en el pelado. Con un numero
    * finito de nodos, se repite uno, y el tramo entre las dos apariciones ES
    * un ciclo.                                                          */
   while ( cur && !orden[cur] ) {
      int siguiente = 0;

      orden[cur] = ++largo;
      camino[largo] = cur;
      for ( k = 0; k < nlinks; k++ )
         if ( lnk[k].out == cur && sobra[lnk[k].inp] ) {
            siguiente = lnk[k].inp;
            break;
         }
      cur = siguiente;
   }
   if ( !cur ) return 0;                    /* no deberia pasar */

   /* El camino se recorrio hacia atras: se devuelve del reves, para que se lea
    * en el sentido en que la senal viaja. Cerrado: el primero se repite.  */
   {
   int desde = orden[cur], j;

   for ( j = largo; j >= desde; j-- ) ciclo[(*n)++] = camino[j];
   ciclo[(*n)++] = camino[largo];
   }
   return 1;
}
