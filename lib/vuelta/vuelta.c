/*
 * vuelta.c -- lo que drtran devuelve al univariante. Ver vuelta.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vuelta.h"
#include "inpdet.h"
#include "inpcheck.h"

#define VU_MAX  (64 * 130)

/* Los del .out que son de la serie k y de esa clase, en su orden. */
static int recoge( const OdParams *par, const char *pref1, const char *pref2,
                   int k, double *v, int max )
{
   char p1[32], p2[32];
   int  i, n = 0;

   snprintf( p1, sizeof p1, "%s%d[", pref1, k );
   snprintf( p2, sizeof p2, "%s%d[", pref2 ? pref2 : "", k );
   for ( i = 0; i < par->n; i++ )
       if ( !strncmp( par->p[i].nombre, p1, strlen( p1 ) ) ||
            ( pref2 && !strncmp( par->p[i].nombre, p2, strlen( p2 ) ) ) )
          {
          if ( n >= max ) return -1;
          v[n++] = par->p[i].valor;
          }
   return n;
}

/* Copia el resultado de un paso sobre el fichero de trabajo. */
static int paso( const char *de, const char *a )
{
   if ( remove( a ) != 0 ) { FILE *f = fopen( a, "rb" ); if ( f ) { fclose( f ); return 1; } }
   return rename( de, a ) != 0;
}

int vu_escribe( const char *pre, const OdParams *par, int k,
                const char *destino, int *movidos, char *porque, size_t n )
{
   IdArma  a;
   IdMaf   mf;
   double *v = NULL, *dv = NULL, mu = 0.0;
   int    *dfl = NULL;
   int     nd, nlib, i, s, f, j, c, narf = 0, fmu = 0, hay_mu;
   int     mov = 0, rc = 1;
   char    tmp[4096], tmp2[4096], msg[512];

   if ( movidos ) *movidos = 0;
   if ( porque && n ) porque[0] = '\0';
   if ( !pre || !par || !destino || k < 1 )
      { snprintf( porque, n, "faltan datos" ); return 1; }
   snprintf( tmp,  sizeof tmp,  "%s.vu1", destino );
   snprintf( tmp2, sizeof tmp2, "%s.vu2", destino );

   v   = malloc( VU_MAX * sizeof *v );
   dv  = malloc( VU_MAX * sizeof *dv );
   dfl = malloc( VU_MAX * sizeof *dfl );
   if ( !v || !dv || !dfl ) { snprintf( porque, n, "sin memoria" ); goto fin; }

   if ( id_arma_lee( pre, &a, porque, n ) ) goto fin;
   if ( id_maf_lee( pre, &mf, porque, n ) ) goto fin;
   if ( id_arf_cuenta( pre, &narf, porque, n ) ) goto fin;

   /* --- el ARMA: AR (regular, anual), luego MA (regular, anual, maf) --- */
   for ( s = 0; s < 2; s++ )
       {
       const int sec[2][2] = { { 0, 1 }, { 2, 3 } };
       int       m = recoge( par, s ? "theta_" : "phi_", NULL, k, v, VU_MAX );

       nlib = 0;
       for ( c = 0; c < 2; c++ )
           for ( f = 0; f < a.nf[sec[s][c]]; f++ )
               for ( j = 0; j < a.ord[sec[s][c]][f]; j++ )
                   nlib += a.fl[sec[s][c]][f][j] == 1;
       if ( s == 1 ) for ( f = 0; f < mf.n; f++ ) nlib += mf.fl[f] == 1;
       if ( s == 0 && narf > 0 && m > 0 )
          { snprintf( porque, n, "la serie %d tiene AR(2) de frecuencia fija: esta "
                                 "vuelta todavía no sabe escribirlos", k ); goto fin; }
       if ( m < 0 || ( m > 0 && m != nlib ) )
          {
          snprintf( porque, n, "la serie %d: el .out trae %d coeficientes %s y el "
                               ".pre declara %d libres. No casan: no se escribe nada.",
                    k, m, s ? "MA" : "AR", nlib );
          goto fin;
          }
       if ( m == 0 ) continue;                /* clavados por la orden */
       i = 0;
       for ( c = 0; c < 2; c++ )
           for ( f = 0; f < a.nf[sec[s][c]]; f++ )
               for ( j = 0; j < a.ord[sec[s][c]][f]; j++ )
                   if ( a.fl[sec[s][c]][f][j] == 1 )
                      {
                      double *w = &a.v[sec[s][c]][f][j];

                      mov += fabs( *w - v[i] ) > 1e-12;
                      *w = v[i++];
                      }
       if ( s == 1 )
          for ( f = 0; f < mf.n; f++ )
              if ( mf.fl[f] == 1 )
                 { mov += fabs( mf.v[f] - v[i] ) > 1e-12; mf.v[f] = v[i++]; }
       }
   if ( id_escribe_arma( pre, tmp, &a, porque, n ) ) goto fin;
   if ( mf.n > 0 )
      {
      if ( id_escribe_maf( tmp, tmp2, &mf, porque, n ) || paso( tmp2, tmp ) )
         { if ( !porque[0] ) snprintf( porque, n, "no pude escribir el MA(2)" ); goto fin; }
      }

   /* --- los deterministas, por variable: omegas y deltas --- */
   nd = id_det_coefs( pre, dv, dfl, VU_MAX, porque, n );
   if ( nd < 0 ) goto fin;
   {
   int m = recoge( par, "omega_d", "delta_d", k, v, VU_MAX );

   nlib = 0;
   for ( i = 0; i < nd; i++ ) nlib += dfl[i] == 1;
   if ( m < 0 || ( m > 0 && m != nlib ) )
      {
      snprintf( porque, n, "la serie %d: el .out trae %d coeficientes de "
                           "deterministas y el .pre declara %d libres. No casan: "
                           "no se escribe nada.", k, m, nlib );
      goto fin;
      }
   if ( m > 0 )
      {
      for ( i = 0, j = 0; i < nd; i++ )
          if ( dfl[i] == 1 ) { mov += fabs( dv[i] - v[j] ) > 1e-12; dv[i] = v[j++]; }
      if ( id_escribe_det_coefs( tmp, tmp2, dv, nd, porque, n ) || paso( tmp2, tmp ) )
         { if ( !porque[0] ) snprintf( porque, n, "no pude escribir los deterministas" );
           goto fin; }
      }
   }

   /* --- la media --- */
   if ( id_media_lee( pre, &mu, &fmu, porque, n ) ) goto fin;
   {
   char nm[32];

   snprintf( nm, sizeof nm, "mu[%d]", k );
   hay_mu = 0;
   for ( i = 0; i < par->n; i++ )
       if ( !strcmp( par->p[i].nombre, nm ) )
          if ( fabs( mu - par->p[i].valor ) > 1e-12 )
             { mov++; mu = par->p[i].valor; hay_mu = 1; }
   if ( hay_mu )
      {
      if ( id_fija_media( tmp, tmp2, mu, fmu, porque, n ) || paso( tmp2, tmp ) )
         { if ( !porque[0] ) snprintf( porque, n, "no pude escribir la media" ); goto fin; }
      }
   }

   /* LA PUERTA DEL MOTOR: un .inp que fue no acepta es peor que ninguno. */
   if ( inp_check_fue( tmp, msg, sizeof msg ) != 0 )
      { snprintf( porque, n, "fue no aceptaría el fichero: %s", msg ); goto fin; }
   if ( paso( tmp, destino ) )
      { snprintf( porque, n, "no pude escribir «%s»", destino ); goto fin; }
   if ( movidos ) *movidos = mov;
   rc = 0;

fin:
   if ( rc ) { remove( tmp ); remove( tmp2 ); }
   free( v ); free( dv ); free( dfl );
   return rc;
}

int vu_con_transferencia( const OdParams *par )
{
   int i;

   for ( i = 0; par && i < par->n; i++ )
       if ( !strncmp( par->p[i].nombre, "omega", 5 ) &&
            strncmp( par->p[i].nombre, "omega_d", 7 ) ) return 1;
   return 0;
}

static OdParams vu_par;           /* ~70 KB: fuera de la pila (Windows) */

int vu_caso( Proyecto *p, const char *caso, const char *corrida,
             char *hechos, size_t nh, char *porque, size_t n )
{
   const PrCaso *c;
   char          out[PR_RUTA];
   size_t        l = 0;
   int           k, malos = 0;

   if ( hechos && nh ) hechos[0] = '\0';
   if ( porque && n ) porque[0] = '\0';
   if ( !p || !caso || !corrida || ( c = pr_caso_ver( p, caso ) ) == NULL )
      { snprintf( porque, n, "ese caso no está" ); return 1; }
   if ( pr_corrida_ruta( p, caso, corrida, ".out", out, sizeof out ) != 0 ||
        od_params_file( out, &vu_par ) != 0 )
      { snprintf( porque, n, "el modelo %s del caso %s no tiene .out: no está "
                             "estimado", corrida, caso ); return 1; }
   if ( !vu_con_transferencia( &vu_par ) )
      { snprintf( porque, n, "el modelo %s es el diagonal: sin transferencia, los "
                             "bloques son los de fue y no hay nada que devolver",
                  corrida ); return 1; }

   for ( k = 0; k < c->nen; k++ )
       {
       char    pre[PR_RUTA], hijo[PR_ID], ruta[PR_RUTA], razon[PR_RAZON], w[512];
       PrError e;
       int     mov = 0;

       if ( pr_ruta( p, c->en[k].serie, c->muestra, c->en[k].modelo, ".pre",
                     pre, sizeof pre ) != 0 ||
            pr_deriva( p, c->en[k].serie, c->muestra, c->en[k].modelo,
                       hijo, sizeof hijo, ruta, sizeof ruta, &e ) != 0 )
          {
          snprintf( porque, n, "%s: no pude derivar de %s", c->en[k].serie,
                    c->en[k].modelo );
          malos++;
          continue;
          }
       if ( vu_escribe( pre, &vu_par, k + 1, ruta, &mov, w, sizeof w ) != 0 )
          {
          /* UN MODELO SIN SU FICHERO NO SE QUEDA: se quita. */
          pr_borra( p, c->en[k].serie, c->muestra, hijo, NULL );
          snprintf( porque, n, "%s", w );
          malos++;
          continue;
          }
       snprintf( razon, sizeof razon, "El bloque univariante de %s reestimado con "
                 "la transferencia en drtran (caso %s, modelo %s): %d coeficientes "
                 "movidos. Es una especificación: estímalo con fue.",
                 c->en[k].modelo, caso, corrida, mov );
       pr_razon( p, c->en[k].serie, c->muestra, hijo, razon, NULL );
       if ( hechos && l < nh )
          l += (size_t) snprintf( hechos + l, nh - l, "%s%s/%s", l ? " · " : "",
                                  c->en[k].serie, hijo );
       }
   return malos ? 1 : 0;
}
