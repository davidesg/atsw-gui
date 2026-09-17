/*
 * verdict.c -- ver verdict.h.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "verdict.h"

/* La primera aparicion de que, o NULL. */
static const char *busca( const char *t, const char *que )
{
   return t ? strstr( t, que ) : NULL;
}

Veredicto verdict_parse( const char *texto, VerdictInfo *v )
{
   const char *p;

   memset( v, 0, sizeof *v );
   v->ver    = VER_NADA;
   v->iters  = -1;
   v->maxits = -1;

   if ( !texto ) return VER_NADA;

   /* --- el verdict, por la frase que lo acompaña ------------------------ */
   /* Los dos primeros comparten titular ("CONVERGENCE OBTAINED") y se
    * distinguen por el criterio; por eso se mira el criterio y no el
    * titular.                                                           */
   if ( busca( texto, "gradient stopping criterium" ) )       v->ver = VER_GRADIENTE;
   else if ( busca( texto, "parameter stopping criterium" ) ) v->ver = VER_PARAMETRO;
   else if ( busca( texto, "failed to locate a lower point" ) ) v->ver = VER_SIN_MEJORA;
   else if ( busca( texto, "ITERATION LIMIT REACHED" ) )      v->ver = VER_ITERACIONES;
   else if ( busca( texto, "steps of max length" ) )          v->ver = VER_PASOS;
   else if ( busca( texto, "**** " ) )                        v->ver = VER_OTRO;

   /* --- el titular, tal cual -------------------------------------------- */
   /* LA PRIMERA linea "****", este al principio del texto o despues de un
    * salto: el motor escribe dos o tres seguidas --el titular, el criterio y
    * a veces el ifault-- y la que resume es la primera.                  */
   p = busca( texto, "**** " );
   if ( p ) {
      const char *fin = strchr( p, '\n' );
      size_t      n   = fin ? (size_t)( fin - p ) : strlen( p );

      if ( n >= sizeof v->frase ) n = sizeof v->frase - 1;
      memcpy( v->frase, p, n );
      v->frase[n] = 0;
   }

   /* --- las iteraciones:  AFTER 349 ITERATIONS (of 500) ----------------- */
   p = busca( texto, " AFTER " );
   if ( p ) {
      int a, b;

      if ( sscanf( p, " AFTER %d ITERATIONS (of %d)", &a, &b ) == 2 ) {
         v->iters = a; v->maxits = b;
      } else if ( sscanf( p, " AFTER %d ITERATIONS", &a ) == 1 )
         v->iters = a;
   }

   /* --- el ifault del evaluador ---------------------------------------- */
   p = busca( texto, "ifault = " );
   if ( p ) sscanf( p, "ifault = %d", &v->ifault );

   /* --- la verosimilitud ------------------------------------------------ */
   p = busca( texto, "Log-likelihood" );
   if ( p ) {
      const char *eq = strchr( p, '=' );
      const char *dp = strchr( p, ':' );

      if ( !eq || ( dp && dp < eq ) ) eq = dp;
      if ( eq && sscanf( eq + 1, "%lf", &v->logl ) == 1 ) v->tiene_logl = 1;
   }

   return v->ver;
}

int verdict_ok( const VerdictInfo *v )
{
   /* VER_SIN_MEJORA cuenta: pararse sin mejora arrancando de un .pre es que
    * el .pre YA ERA el optimo, que es la invariante del contrato y no un
    * fracaso. Tomarlo por fallo seria confundir el exito con el fracaso. */
   return v->ver == VER_GRADIENTE || v->ver == VER_PARAMETRO ||
          v->ver == VER_SIN_MEJORA;
}
