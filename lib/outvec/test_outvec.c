/*
 * test_outvec.c -- lib/outvec contra salidas DE VERDAD de drvec.
 *
 *   test_outvec DIR DATOS
 *
 * DIR lo llena run_test.sh corriendo el drvec del arbol con los .pre de
 * Johansen y Juselius (datos/, los del ejemplo johansen_juselius de sima):
 *
 *   rango.out   la prueba del rango, sin bootstrap (los valores asintoticos)
 *   vec1.out    el VEC con r = 1, normalizado sobre LRM
 *   wx.out      el mismo, con IBO debilmente exogena (-weakex 2)
 *
 * DATOS trae rango_b99.out y .boot, una prueba con 99 replicas: el bootstrap
 * tarda (6 minutos), asi que su tabla se lee de una salida guardada.
 *
 * Las cifras son las del tutorial de sima. Con tolerancia: fuera de Linux
 * el ultimo digito baila.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "outvec.h"

static int fallos = 0;

static void ok( int c, const char *que )
{
   printf( c ? "  ok     %s\n" : "  FALLA  %s\n", que );
   if ( !c ) fallos++;
}

static int cerca( double a, double b, double tol ) { return fabs( a - b ) <= tol; }

static void lee( const char *dir, const char *f, VecOut *v )
{
   char p[1024];
   snprintf( p, sizeof p, "%s/%s", dir, f );
   if ( ovec_parse_file( p, v ) != 0 ) { printf( "  FALLA  no pude leer %s\n", p ); fallos++; }
}

int main( int argc, char **argv )
{
   static VecOut v;
   const char   *dir = argc > 1 ? argv[1] : ".", *datos = argc > 2 ? argv[2] : "datos";
   int           al;
   char          p[1024];

   /* --- la prueba del rango, sin bootstrap -------------------------------- */
   lee( dir, "rango.out", &v );
   ok( !strncmp( v.programa, "DRVEC ", 6 ), "rango: la cabecera dice DRVEC" );
   ok( v.es_rango && v.r == -1 && v.p == 2 && v.q == 0, "rango: es la prueba del rango, VARMA-VECM(2,0)" );
   ok( v.m == 4 && !strcmp( v.serie[0], "LRM" ) && !strcmp( v.serie[3], "IDE" ) && v.ny2 == 3,
       "rango: cuatro series, en el orden de la orden, tres en nabla Y2" );
   ok( !strcmp( v.caso, "2" ) && v.nobs == 54, "rango: caso 2, 54 observaciones" );
   ok( v.puerta == 1 && fabs( v.puerta_dif ) < 1e-6 && cerca( v.puerta_suma, -376.0852, 1e-3 ),
       "rango: la puerta, verificada (suma -376.0852)" );
   ok( v.nrango == 4 && cerca( v.rango[0].logL, -335.7764, 2e-3 ) &&
       cerca( v.rango[1].logL, -322.8205, 2e-3 ) && cerca( v.rango[3].logL, -310.1766, 2e-3 ) &&
       v.rango[0].npar == 25 && v.rango[3].npar == 43,
       "rango: logL y npar de r = 0..3 (los de sima)" );
   ok( v.nasint == 3 && cerca( v.asint[0].lr, 25.9118, 5e-3 ) && cerca( v.asint[1].lr, 21.2158, 5e-3 ) &&
       cerca( v.asint[2].lr, 4.0722, 5e-3 ) && v.asint[0].hay_cv && cerca( v.asint[0].cv5, 28.14, 1e-6 ),
       "rango: la LR y los valores asintoticos" );
   ok( v.nboot == 0 && v.B == 0, "rango: sin -bootstrap no hay tabla del bootstrap" );
   ok( ovec_rango_propuesto( &v, &al ) == 0 && !al, "rango: sin bootstrap, propone r = 0 (los asintoticos)" );
   ok( v.ultimo_conv == 1 && v.n_parado == 0, "rango: cada ajuste convergio" );

   /* --- la prueba con 99 replicas (guardada) ------------------------------ */
   lee( datos, "rango_b99.out", &v );
   ok( v.B == 99 && v.nboot == 3 && v.boot[0].reps == 99, "bootstrap: 99 replicas, tres comparaciones" );
   ok( cerca( v.boot[0].cv5, 29.78, 1e-6 ) && cerca( v.boot[0].p, 0.10, 1e-6 ) &&
       cerca( v.boot[2].p, 0.65, 1e-6 ), "bootstrap: valores criticos y p-valores" );
   ok( ovec_rango_propuesto( &v, &al ) == 0 && !al, "bootstrap: propone r = 0 (25.91 <= 29.78)" );
   snprintf( p, sizeof p, "%s/rango_b99.boot", datos );
   ok( ovec_boot_hechas( p ) == 297, "bootstrap: el .boot tiene 3 x 99 replicas" );
   ok( ovec_boot_hechas( "/no/existe.boot" ) == -1, "bootstrap: un .boot que no esta, -1" );

   /* --- el VEC con r = 1 -------------------------------------------------- */
   lee( dir, "vec1.out", &v );
   ok( !v.es_rango && v.r == 1 && !strcmp( v.serie[3], "LRM" ), "vec: r = 1, normalizado sobre LRM (el ultimo)" );
   ok( v.ultimo_conv == 1 && v.arranques >= 1 && v.arranques_conv == v.arranques,
       "vec: convergio, desde todos los arranques" );
   ok( cerca( v.logL, -324.3522, 2e-3 ) && v.npar == 33, "vec: logL y npar" );
   ok( v.hay_vec && v.nr == 1, "vec: el bloque del VEC, con una relacion" );
   ok( cerca( v.beta[0][0], -1.1196, 2e-3 ) && cerca( v.beta[1][0], 5.1557, 5e-3 ) &&
       cerca( v.beta[2][0], -4.2617, 5e-3 ) && cerca( v.beta[3][0], 1.0, 1e-9 ),
       "vec: beta, LRM = 1.12 LRY - 5.16 IBO + 4.26 IDE (sima)" );
   ok( cerca( v.alpha[3][0], -0.1656, 2e-3 ) && cerca( v.alpha[0][0], 0.1420, 2e-3 ),
       "vec: alpha, -0.17 en LRM y +0.14 en LRY (sima)" );
   {
   const OvecPar *a = ovec_param( &v, "D.LRM <- ec1" ), *b = ovec_param( &v, "LRY in ec1" );
   ok( a && cerca( a->t, -2.269, 0.02 ) && strstr( a->seccion, "alpha" ),
       "vec: la t de alpha en LRM (-2.27), en su seccion" );
   ok( b && cerca( b->est, -1.1196, 2e-3 ), "vec: beta en la tabla de parametros" );
   ok( ovec_param( &v, "D.LRY <- D.LRM(-1)" ) != NULL, "vec: un nombre con parentesis y numeros" );
   ok( v.npar_t == 33, "vec: la tabla tiene los 33 parametros" );
   }
   ok( v.new_ == 1 && cerca( v.ew[0], 552.79, 0.05 ), "vec: E[W]" );
   ok( cerca( v.sigma[3][3], 4.5508, 2e-3 ) && v.sigma[0][3] == v.sigma[3][0], "vec: Sigma, simetrica" );
   ok( v.nlr == 0, "vec: sin restriccion, no hay LR" );

   /* --- una restriccion ---------------------------------------------------- */
   lee( dir, "wx.out", &v );
   ok( v.nlr == 1 && strstr( v.lr[0].que, "alpha = A*psi" ) != NULL, "weakex: el bloque de la LR" );
   ok( v.nlr == 1 && cerca( v.lr[0].lr, 0.0003, 2e-3 ) && v.lr[0].gl == 1 && cerca( v.lr[0].p, 0.986, 0.01 ),
       "weakex: IBO debilmente exogena, LR 0.0003 con 1 gl, p 0.99 (sima)" );
   ok( v.nlr == 1 && cerca( v.lr[0].libre, -324.3522, 2e-3 ), "weakex: la logL libre es la del VEC" );

   /* --- la prevision y la evaluacion (S10c) ------------------------------- */
   {
      static OvecPrev pv[OVEC_MAX_SER];
      OvecRec rc;
      char *irf, *fevd;
      int n, i, k40 = 0;

      snprintf( p, sizeof p, "%s/prev.forecast", dir );
      n = ovec_forecast_file( p, pv, OVEC_MAX_SER );
      ok( n == 4 && !strcmp( pv[0].nombre, "LRY" ) && !strcmp( pv[3].nombre, "LRM" ) && pv[0].nf == 8,
          "prevision: cuatro series, en el orden de la orden, 8 periodos" );
      ok( n == 4 && !strcmp( pv[0].fecha[0], "4/1987" ) && pv[0].lo[0] < pv[0].nivel[0] &&
          pv[0].nivel[0] < pv[0].hi[0] && pv[0].se[0] > 0, "prevision: desde 4/1987, con su banda" );
      snprintf( p, sizeof p, "%s/ev.recursive", dir );
      ok( ovec_recursive_file( p, &rc ) == 0 && rc.estwin == 39 && rc.horizonte == 4,
          "evaluacion: el .recursive, con estwin y horizonte" );
      for ( i = 0; i < rc.nf; i++ ) k40 += rc.f[i].origen == 39 && rc.f[i].h == 1;
      ok( rc.nf > 100 && k40 == 4 && fabs( rc.f[0].real - rc.f[0].prevision - rc.f[0].error ) < 1e-6,
          "evaluacion: una fila por origen, horizonte y serie; error = real - prevision" );
      ovec_recursive_free( &rc );
      lee( dir, "prev.out", &v );
      ok( v.nobs_raw == 55 && v.nobs == 54, "la muestra: 54 observaciones de 55 en los ficheros" );
      snprintf( p, sizeof p, "%s/prev.out", dir );
      irf  = ovec_seccion_file( p, "Impulse Response of the Levels" );
      fevd = ovec_seccion_file( p, "Forecast Error Variance Decomposition, in Levels" );
      ok( irf && strlen( irf ) > 200 && !strstr( irf, "Long-Run Gain" ), "la IRF en niveles, sin la seccion siguiente" );
      ok( fevd && strlen( fevd ) > 200, "la descomposicion de la varianza, en niveles" );
      ok( ovec_seccion_file( p, "No existe" ) == NULL, "una seccion que no esta: NULL" );
      free( irf ); free( fevd );
   }

   /* --- lo que no viene de drvec ------------------------------------------ */
   {
   const char *todas =
      "Model            : VARMA-VECM(2,0), M = 3 series, cointegration rank r = 0..2 (sequential test)\n"
      "Series           : A B C   (2 in the nabla Y2 block, 1 in Y1)\n"
      "  r    M-r        LR      10%      5%      1%   p-value  reps\n"
      "  -----------------------------------------------------------------\n"
      "  0       3    40.0000    25.82    29.78    33.69   0.0100    99   reject H0 at 1%\n"
      "  1       2    30.0000    18.22    22.87    30.78   0.0400    99   reject H0 at 5%\n"
      "\n";
   ovec_parse( todas, &v );
   ok( ovec_rango_propuesto( &v, &al ) == 2 && al, "todas rechazadas: «al menos» m - 1" );
   ovec_parse( "Series           : A B C   (2 in the nabla Y2 block, 1 in Y1)\n", &v );
   ok( ovec_rango_propuesto( &v, &al ) == -1, "sin tabla, no se propone nada" );
   ok( ovec_parse_file( "/no/existe.out", &v ) == 1 && v.puerta == -1, "un .out que no esta: 1" );
   }

   printf( "outvec: %d fallos\n", fallos );
   return fallos ? 1 : 0;
}
