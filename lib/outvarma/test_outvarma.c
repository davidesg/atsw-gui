/* test_outvarma.c -- leer lo que escribe drvarma en la escalera.
 *
 *   test_outvarma DIR
 *
 * DIR tiene las salidas que run_test.sh acaba de pedir a drvarma con los
 * .pre de Jenkins y Alavi (ratas almizcleras y visones):
 *
 *   puerta.out     0 0 -diagcov                       la puerta, sola
 *   ar2.out        2 0 -forecast 10  (+ ar2.forecast)  el AR(2) cruzado
 *   rec.out        2 0 -forecast 4 -estwin 48 (+ rec.recursive)
 *   pared.out      1 1 -diagcov -links "MUSKRAT<-MINK" para en la pared MA
 *   noopt.out      0 0 -diagcov con un .pre retocado:  «no es un optimo»
 *
 * Lo que se exige son sobre todo HECHOS DE FORMA (cuantos parametros, que
 * series, que la puerta pasa, que las bandas rodean al nivel, que el .out y
 * el .forecast dicen lo mismo) y unos pocos numeros con tolerancia amplia:
 * fuera de Linux otra libm puede acabar en otro sitio (docs/PRUEBAS.md §3).
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "outvarma.h"

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-62s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

static void okf( double visto, double esperado, double tol, const char *que )
{
   int bien = fabs( visto - esperado ) <= tol;
   printf( "  %-46s %12.4f (%11.4f)  %s\n", que, visto, esperado, bien ? "ok" : "FALLA" );
   if ( !bien ) fallos++;
}

static char ruta[1024];
static const char *en( const char *dir, const char *f )
{
   snprintf( ruta, sizeof ruta, "%s/%s", dir, f );
   return ruta;
}

static VarmaOut v;

static void puerta( const char *dir )
{
   int r = ov_parse_file( en( dir, "puerta.out" ), &v );
   printf( "puerta.out\n" );
   ok( r == 0, "es un .out de la escalera" );
   ok( v.m == 2, "dos series" );
   ok( !strcmp( v.uni[0].nombre, "MUSKRAT" ) && !strcmp( v.uni[1].nombre, "MINK" ),
       "MUSKRAT y MINK, en el orden de la orden" );
   ok( strstr( v.uni[0].pre, "MUSKRAT_m03.pre" ) && strstr( v.uni[1].pre, "MINK_m02.pre" ),
       "cada una con su .pre" );
   ok( v.uni[0].nobs == 62 && v.uni[0].lambda == 0 && !strcmp( v.uni[0].operador, "D" ),
       "MUSKRAT: 62 obs, lambda 0, operador D" );
   ok( !strcmp( v.uni[0].ar, "(6)" ) && !strcmp( v.uni[0].ma, "(1)" ), "MUSKRAT: AR (6), MA (1)" );
   ok( !strcmp( v.uni[1].ma, "-" ) && !strcmp( v.uni[1].media, "estimated" ), "MINK: sin MA, media estimada" );
   ok( v.p == 0 && v.q == 0 && v.sigma_diagonal, "p = q = 0, Sigma diagonal" );
   ok( !v.det_reestimados && !v.arma_fijos, "deterministas fijos, ARMA reestimados" );
   ok( !strcmp( v.desde, "1/1851" ) && !strcmp( v.hasta, "1/1911" ) && v.nobs == 61,
       "ventana comun 1/1851 - 1/1911, 61 obs" );
   ok( v.puerta.hay && v.puerta.ns == 2, "la puerta, con sus dos series" );
   ok( v.puerta.pasa, "la puerta pasa" );
   okf( v.puerta.diferencia, 0, 1e-8, "conjunta - suma" );
   okf( v.puerta.suma, v.puerta.s[0].logl + v.puerta.s[1].logl, 1e-5, "suma = logL1 + logL2" );
   ok( !v.puerta.s[0].recortada && !v.puerta.s[0].no_optimo, "MUSKRAT no se movio" );
   ok( v.puerta.s[1].recortada && !v.puerta.s[1].no_optimo, "MINK se movio por la ventana recortada" );
   ok( v.solo_diagonal, "el modelo estimado es el sistema diagonal" );
   ok( v.optim == OV_OPT_NADA && !v.fallo, "sin bloque del optimizador (p = q = 0), sin fallo" );
   ok( !v.tiene_lr, "sin LR (no hay contra que)" );
   ok( v.tiene_diag && v.diag.hosking.hay && v.diag.ns == 2, "la diagnosis, por lib/outdiag" );
   {
      const OvCcf *c = ov_ccf( &v, 0, 1 );
      ok( v.nccf == 1 && c && c == ov_ccf( &v, 1, 0 ), "una CCF de residuos, la del par (en cualquier orden)" );
      if ( c ) {
         ok( c->i == 0 && c->j == 1 && !strcmp( c->a, "MUSKRAT" ) && !strcmp( c->b, "MINK" ),
             "a[2] (MINK) - a[1] (MUSKRAT): A = MUSKRAT, B = MINK" );
         ok( c->nlags == 9, "retardos -9..9" );
         okf( c->banda, 2 / sqrt( 61 ), 1e-4, "la banda es 2/sqrt(61)" );
         ok( fabs( c->r[c->nlags] ) > c->banda, "la contemporanea sale de la banda (Sigma completa)" );
         ok( fabs( c->r[c->nlags + 1] ) > c->banda, "y k = 1: MINK entra en la ecuacion de MUSKRAT" );
         ok( c->q_ge0 > 0 && c->q_le0 > 0 && c->df_ge0 == 10 && c->df_gt0 == 9, "los cuatro Q y sus grados" );
         ok( ov_ccf_r( &v, 0, 1, 1, NULL ) == c->r[c->nlags + 1] && ov_ccf_r( &v, 1, 0, 1, NULL ) == c->r[c->nlags - 1],
             "ov_ccf_r: MINK en MUSKRAT es k = +1; MUSKRAT en MINK, k = -1" );
         ok( ov_ccf_r( &v, 0, 1, 0, NULL ) == ov_ccf_r( &v, 1, 0, 0, NULL ), "la contemporanea, la misma en los dos sentidos" );
      }
   }
}

static void ar2( const char *dir )
{
   static OvPrev fc[OV_MAX_SER];
   const OvParam *x;
   int r = ov_parse_file( en( dir, "ar2.out" ), &v ), i, j, h, n, bandas = 1, iguales = 1;
   double s;

   printf( "ar2.out\n" );
   ok( r == 0, "es un .out de la escalera" );
   ok( v.p == 2 && v.q == 0 && !v.sigma_diagonal && v.enlaces[0] == 0,
       "p = 2, q = 0, Sigma completa, todos los pares" );
   ok( v.horizonte == 10, "horizonte 10" );
   ok( v.puerta.pasa && !v.solo_diagonal, "la puerta pasa y el modelo es el cruzado" );
   ok( v.optim == OV_OPT_CONVERGIO && v.iteraciones > 0 && v.criterio[0], "convergio, con iteraciones y criterio" );
   ok( v.npar == 18, "18 parametros" );
   x = ov_param( &v, "AR1[MUSKRAT<-MINK]" );
   ok( x && x->tiene_se, "AR1[MUSKRAT<-MINK], con su error tipico" );
   if ( x ) okf( x->est, -0.538, 0.05, "AR1[MUSKRAT<-MINK]" );
   ok( ov_param( &v, "phi_MUSKRAT[B^6]" ) && ov_param( &v, "mu_MINK" ) &&
       ov_param( &v, "Q[MINK,MUSKRAT]" ), "la diagonal, la media y Q, por su nombre" );
   ok( !strcmp( v.se_como, "fdhess" ), "errores tipicos por fdhess" );
   ok( v.tiene_logl, "la logL" );
   ok( v.tiene_lr && v.lr_df == 5, "la LR contra los univariantes, df = 5" );
   okf( v.lr, 2 * ( v.lr_logl_full - v.lr_logl_diag ), 1e-3, "LR = 2 (logL full - logL diag)" );
   okf( v.lr_logl_full, v.logl, 1e-5, "logL full = la logL del modelo" );
   okf( v.lr, 41.17, 1.0, "LR" );
   ok( v.tiene_sigma, "Sigma" );
   okf( v.sigma[0][1], v.sigma[1][0], 1e-9, "Sigma simetrica" );
   okf( v.correl[0][1], v.sigma[0][1] / sqrt( v.sigma[0][0] * v.sigma[1][1] ), 1e-3,
        "correlacion = Sigma12 / sqrt(Sigma11 Sigma22)" );
   ok( v.n_raices_ar == 10 && v.raiz_ar_max > 0 && v.raiz_ar_max < 1, "10 raices AR, todas dentro" );
   ok( v.n_raices_ma == 1 && v.raiz_ma_max < 1, "1 raiz MA, dentro" );
   ok( v.nh_irf == 21, "IRF: horizontes 0..20" );
   /* BUG-0004 de drvarma: el choque de una d.t. es el Cholesky de Sigma,
      no el de Q. Antes la escalera daba 1 aqui (factorizaba Q).       */
   okf( v.irf[0][0][0], sqrt( v.sigma[0][0] ), 1e-4,
        "IRF: el choque propio en h = 0 es sqrt(Sigma11)" );
   okf( v.irf[0][0][1], v.sigma[0][1] / sqrt( v.sigma[0][0] ), 1e-4,
        "IRF: MINK responde a MUSKRAT en h = 0 con Sigma12 / sqrt(Sigma11)" );
   okf( v.irf[1][0][0], 0, 1e-9, "IRF: Cholesky, MINK no mueve a MUSKRAT en h = 0" );
   /* los errores tipicos por el metodo delta, si el .out los trae (la
      escalera de hoy): positivos donde la respuesta se mueve, y a h = 0 el
      propio, sqrt(Sigma11 / 2n), que solo depende de Sigma            */
   if ( v.tiene_irf_se ) {
      ok( v.irf_se[0][1][1] > 0 && v.irfac_se[0][1][1] > 0, "IRF: con errores típicos (delta) y su acumulada" );
      okf( v.irf_se[1][0][0], 0, 1e-9, "IRF: el cero de Cholesky no tiene error" );
   }
   ok( v.nh_fevd == 20, "FEVD: horizontes 1..20" );
   for ( i = 0, n = 0; i < v.m; i++ )
      for ( h = 0; h < v.nh_fevd; h++ ) {
         for ( j = 0, s = 0; j < v.m; j++ ) s += v.fevd[i][h][j];
         n += fabs( s - 100 ) < 0.05;
      }
   ok( n == v.m * v.nh_fevd, "FEVD: cada fila suma 100" );
   okf( v.fevd[0][0][0], 100, 1e-9, "FEVD: MUSKRAT en h = 1 es toda suya" );
   ok( v.tiene_diag && v.diag.ns == 2 && v.diag.hosking.hay && v.diag.jb.hay,
       "la diagnosis: dos series, Hosking y Jarque-Bera" );
   {
      static OvResiduos r;
      double med = 0;
      int k;
      ok( ov_residuos_file( en( dir, "ar2.out" ), &r ) == 0, "los residuos, del grafico del .out" );
      ok( r.m == 2 && r.n == 61 && !strcmp( r.nombre[1], "MINK" ), "dos series, 61 observaciones" );
      for ( k = 0; k < r.n; k++ ) med += r.v[0][k];
      okf( med / r.n, v.diag.s[0].media, 1e-4, "su media es la que dice el .out" );
      ok( !strcmp( r.fecha[0], "1851" ) && !strcmp( r.fecha[60], "1911" ), "con sus fechas, 1851..1911" );
      okf( r.v[0][57], -103.1112, 1e-3, "el residuo grande de 1908 (MUSKRAT)" );
   }
   {
      /* Trimestrales: la primera estacion de cada año lleva '+' de borde,
         no '|', y el '*' puede tapar el eje del cero (drvec, Johansen y
         Juselius). Se leia el eje: un 0, o la fila saltada.            */
      static OvResiduos r;
      const char *trim =
         "--- Residual series a[1] (IBO) ---\n"
         "Standardized time series plot (original values on right-side column):\n"
         "              -3      -2      -1       0      +1      +2      +3  \n"
         "-------------+-+-------+-------+-------+-------+-------+-------+-+--------------\n"
         "   1  2/1974>|         :       :       |       :       :   *     |> 1.9796243157\n"
         "   2  3/1974 |         :      *:       |       :       :         | -1.0741136926\n"
         "   3  4/1974 +         :       *       |       :       :         + -0.9858876860\n"
         "   4  1/1975 +         :       :       *       :       :         + -0.0369605345\n"
         "   5  2/1975 |         :       :       |       *       :         |  0.7765019084\n"
         "-------------+-+-------+-------+-------+-------+-------+-------+-+--------------\n";
      FILE *f = fopen( en( dir, "trim.out" ), "w" );
      if ( f ) { fputs( trim, f ); fclose( f ); }
      ok( ov_residuos_file( en( dir, "trim.out" ), &r ) == 0 && r.n == 5,
          "trimestrales: las cinco filas, también las del borde '+'" );
      okf( r.v[0][2], -0.9858876860, 1e-9, "la fila de borde '+': su valor, no el eje" );
      okf( r.v[0][3], -0.0369605345, 1e-9, "y con el '*' sobre el eje" );
      okf( r.v[0][0], 1.9796243157, 1e-9, "la marcada con '>'" );
   }
   ok( v.nprev == 2 && !strcmp( v.prev[0].nombre, "MUSKRAT" ) && !strcmp( v.prev[1].nombre, "MINK" ),
       "la prevision de las dos series" );
   ok( v.prev[0].nf == 10 && v.prev[1].nf == 10, "diez filas cada una" );
   ok( !strcmp( v.prev[0].origen, "1/1911" ) && !strcmp( v.prev[0].fecha[0], "1/1912" ),
       "desde 1/1911, la primera 1/1912" );
   for ( i = 0; i < v.nprev; i++ )
      for ( h = 0; h < v.prev[i].nf; h++ )
         bandas &= v.prev[i].lo95[h] < v.prev[i].nivel[h] && v.prev[i].nivel[h] < v.prev[i].hi95[h];
   ok( bandas, "la banda del 95 % rodea al nivel en todas las filas" );

   n = ov_forecast_file( en( dir, "ar2.forecast" ), fc, OV_MAX_SER );
   ok( n == 2, "el .forecast: dos series" );
   for ( i = 0; i < n && i < v.nprev; i++ ) {
      iguales &= !strcmp( fc[i].nombre, v.prev[i].nombre ) && fc[i].nf == v.prev[i].nf;
      for ( h = 0; h < fc[i].nf && h < v.prev[i].nf; h++ )
         iguales &= fc[i].nivel[h] == v.prev[i].nivel[h] && fc[i].hi95[h] == v.prev[i].hi95[h] &&
                    !strcmp( fc[i].fecha[h], v.prev[i].fecha[h] );
   }
   ok( iguales, "el .forecast dice lo mismo que el .out" );
}

static void rec( const char *dir )
{
   OvRecursive r;
   int k, i, bien = 1, porserie = 0;

   printf( "rec.out\n" );
   ok( ov_parse_file( en( dir, "rec.out" ), &v ) == 0, "es un .out de la escalera" );
   ok( v.nev == 2 && !strcmp( v.ev[0].nombre, "MUSKRAT" ) && !strcmp( v.ev[1].nombre, "MINK" ),
       "la evaluacion de las dos series" );
   ok( v.ev_origenes == 15, "15 origenes" );
   ok( v.ev[0].nh == 4 && v.ev[0].h[0].h == 1 && v.ev[0].h[3].h == 4, "horizontes 1..4" );
   for ( k = 0; k < v.nev; k++ )
      for ( i = 0; i < v.ev[k].nh; i++ )
         bien &= v.ev[k].h[i].n == v.ev_origenes - v.ev[k].h[i].h &&
                 v.ev[k].h[i].rmse >= v.ev[k].h[i].mae && v.ev[k].h[i].mae > 0;
   ok( bien, "n = origenes - h, y RMSE >= MAE > 0" );

   k = ov_recursive_file( en( dir, "rec.recursive" ), &r );
   printf( "rec.recursive\n" );
   ok( k == 0, "se lee" );
   ok( r.estwin == 48 && r.horizonte == 4, "estwin 48, horizonte 4" );
   ok( r.nf == v.ev_origenes * 2 * 4, "una fila por origen, serie y horizonte" );
   for ( i = 0; i < r.nf; i++ ) porserie += !strcmp( r.f[i].serie, "MINK" );
   ok( porserie == r.nf / 2, "la mitad son de MINK" );
   ok( r.nf && !strcmp( r.f[0].origen, "1/1897" ) && r.f[0].h == 1 && r.f[0].nivel > 0,
       "la primera: origen 1/1897, h = 1" );
   ov_recursive_free( &r );
}

static void pared( const char *dir )
{
   printf( "pared.out\n" );
   ok( ov_parse_file( en( dir, "pared.out" ), &v ) == 0, "es un .out de la escalera" );
   ok( !strcmp( v.enlaces, "MUSKRAT<-MINK" ), "los enlaces, tal cual" );
   ok( v.p == 1 && v.q == 1 && v.sigma_diagonal, "p = q = 1, Sigma diagonal" );
   ok( v.optim == OV_OPT_PARED_MA, "parado en la pared MA (no es un optimo)" );
   ok( v.pared_n >= 1 && v.pared_raices >= v.pared_n, "cuantas raices en la pared, y de cuantas" );
   ok( ov_param( &v, "MA1[MUSKRAT<-MINK]" ) && !ov_param( &v, "AR1[MINK<-MUSKRAT]" ),
       "solo el enlace pedido" );
   ok( v.tiene_lr && v.lr_df == 2, "la LR, df = 2" );
}

static void noopt( const char *dir )
{
   printf( "noopt.out\n" );
   ok( ov_parse_file( en( dir, "noopt.out" ), &v ) == 0, "es un .out de la escalera" );
   ok( v.puerta.s[0].no_optimo && !v.puerta.s[0].recortada,
       "MUSKRAT: se movio en su propia muestra, no es un optimo" );
   ok( v.puerta.s[0].mov > 1e-3, "y cuanto se movio" );
   ok( v.puerta.pasa, "la puerta pasa igual: el cast es el mismo" );
}

int main( int argc, char **argv )
{
   if ( argc != 2 ) { fprintf( stderr, "uso: test_outvarma DIR\n" ); return 2; }
   ok( ov_parse( "Program : drtran 3.0\n", &v ) == 1, "un .out que no es de la escalera se rechaza" );
   ok( ov_parse_file( "/no/existe.out", &v ) == -1, "un fichero que no existe: -1" );
   puerta( argv[1] );
   ar2( argv[1] );
   rec( argv[1] );
   pared( argv[1] );
   noopt( argv[1] );
   printf( "outvarma: %d fallos\n", fallos );
   return fallos ? 1 : 0;
}
