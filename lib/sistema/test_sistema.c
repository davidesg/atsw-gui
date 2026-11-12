/* test_sistema.c -- lib/sistema con los .pre de Jenkins y Alavi.
 *
 *   test_sistema DATOS      (DATOS = lib/outvarma/datos)
 *
 * La ventana comun tiene que ser la que escribe drvarma para esos dos
 * ficheros: «1/1851 - 1/1911 (61 stationary observations)», con MINK
 * recortada en una (lib/outvarma/test_outvarma.c lo comprueba en el .out).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sistema.h"
#include "fue_pre_reader.h"

real macheps = 2.220446049250313e-16;   /* ver sistema.h, ENLACE */

#define G_N( a ) ( (int) ( sizeof (a) / sizeof (a)[0] ) )

static int fallos = 0;
static void ok( int c, const char *q ) { printf( "  %-58s %s\n", q, c ? "ok" : "FALLA" ); if ( !c ) fallos++; }

int main( int argc, char **argv )
{
   static Sistema s;
   SisVentana v;
   char why[1024], r[2048], a[32], b[32];
   const char *av[32];
   int k;

   if ( argc < 2 ) return 2;   /* argv[2], opcional: los .pre de Johansen y Juselius */
   snprintf( r, sizeof r, "%s/MUSKRAT_m03.pre", argv[1] );
   ok( sis_anade( &s, r, why, sizeof why ) == 0, "MUSKRAT_m03.pre se lee" );
   snprintf( r, sizeof r, "%s/MINK_m02.pre", argv[1] );
   ok( sis_anade( &s, r, why, sizeof why ) == 0, "MINK_m02.pre se lee" );
   ok( sis_anade( &s, r, why, sizeof why ) != 0 && strstr( why, "ya está" ), "la misma serie dos veces, no" );
   snprintf( r, sizeof r, "%s/no_existe.pre", argv[1] );
   ok( sis_anade( &s, r, why, sizeof why ) != 0 && why[0], "un fichero que no existe, con motivo" );
   ok( s.n == 2, "dos series" );
   ok( !strcmp( s.s[0].nombre, "MUSKRAT" ) && !strcmp( s.s[1].nombre, "MINK" ), "sus nombres, del fichero" );
   ok( s.s[0].es_pre && s.s[0].freq == 1 && s.s[0].nobs == 62, "MUSKRAT: .pre, anual, 62 obs" );
   ok( s.s[0].ini_anio == 1850 && s.s[0].fin_anio == 1911, "MUSKRAT: 1850-1911" );
   ok( s.s[0].lambda == 0 && s.s[0].d == 1 && s.s[0].D == 0, "MUSKRAT: lambda 0, d = 1" );
   ok( !strcmp( s.s[0].ar, "6" ) && !strcmp( s.s[0].ma, "1" ), "MUSKRAT: AR 6, MA 1" );
   ok( !strcmp( s.s[1].ar, "4" ) && !strcmp( s.s[1].ma, "-" ) && s.s[1].media, "MINK: AR 4, sin MA, con media" );
   ok( sis_ventana( &s, &v, why, sizeof why ) == 0, "hay ventana comun" );
   sis_fecha( v.freq, v.desde, a, sizeof a );
   sis_fecha( v.freq, v.hasta, b, sizeof b );
   printf( "    ventana %s - %s, %d obs, recorta %d %d\n", a, b, v.nobs, v.recorta[0], v.recorta[1] );
   ok( !strcmp( a, "1/1851" ) && !strcmp( b, "1/1911" ) && v.nobs == 61, "la de drvarma: 1/1851 - 1/1911, 61" );
   ok( v.recorta[0] == 0 && v.recorta[1] == 1, "MINK pierde una, MUSKRAT ninguna" );
   ok( sis_diferenciadas( &s ) == 1, "una serie diferenciada" );
   k = sis_orden_puerta( &s, "puerta", av, 32 );
   ok( k == 7 && !strcmp( av[2], "0" ) && !strcmp( av[4], "-diagcov" ) && !strcmp( av[6], "puerta" ),
       "la orden de la puerta: A B 0 0 -diagcov -o puerta" );
   {
      SisModelo mo;
      char buf[1024], lk[256];
      int j;
      sis_modelo_defecto( &mo, s.n );
      ok( mo.p == 1 && mo.q == 0 && !mo.diagcov && sis_enlaces( &mo, 2 ) == 2,
          "el modelo de partida: p = 1, Sigma completa, todos los enlaces" );
      ok( sis_enlaces_texto( &s, &mo, lk, sizeof lk ) == 0 && !lk[0], "con todos, sin -links" );
      ok( sis_retroalimenta( &mo, 2 ), "con todos, hay retroalimentacion" );
      ok( sis_parametros_cruzados( &mo, 2 ) == 3, "2 enlaces x (1 + 0) + 1 de Sigma = 3" );
      mo.enlace[1][0] = 0;
      ok( sis_enlaces_texto( &s, &mo, lk, sizeof lk ) > 0 && !strcmp( lk, "MUSKRAT<-MINK" ),
          "un enlace: -links MUSKRAT<-MINK" );
      ok( !sis_retroalimenta( &mo, 2 ), "un solo sentido: sin retroalimentacion (es una red)" );
      {
         SisModelo m2;
         sis_modelo_defecto( &m2, 2 );
         ok( sis_enlaces_de_texto( &s, &m2, "MUSKRAT<-MINK" ) == 0 && m2.enlace[0][1] && !m2.enlace[1][0],
             "de vuelta: MUSKRAT<-MINK a la matriz" );
         ok( sis_enlaces_de_texto( &s, &m2, "" ) == 0 && m2.enlace[1][0], "vacio: todos" );
         ok( sis_enlaces_de_texto( &s, &m2, "MUSKRAT<-NADIE" ) != 0, "un nombre que no esta: 1" );
      }
      mo.p = 2; mo.q = 1; mo.diagcov = 1; mo.fixarma = 1; mo.lik = 2;
      k = sis_orden_modelo( &s, &mo, "c01", buf, sizeof buf, av, 32 );
      {
         char o[2048] = "";
         for ( j = 0; j < k; j++ ) { strcat( o, j ? " " : "" ); strcat( o, strrchr( av[j], '/' ) ? strrchr( av[j], '/' ) + 1 : av[j] ); }
         printf( "    %s\n", o );
         ok( !strcmp( o, "MUSKRAT_m03.pre MINK_m02.pre 2 1 -diagcov -links MUSKRAT<-MINK -fixarma -lik both -o c01" ),
             "la orden del modelo" );
      }
   }
   {
      SisModelo mo, r;
      char buf[2048], z[512];
      int perm[2] = { 1, 0 }, j;
      ok( fabs( sis_chi2_p( 3.841459, 1 ) - 0.05 ) < 1e-5, "chi2: P(chi2_1 > 3.84) = 0.05" );
      ok( fabs( sis_chi2_p( 11.0705, 5 ) - 0.05 ) < 1e-5, "chi2: P(chi2_5 > 11.07) = 0.05" );
      ok( fabs( sis_chi2_p( 0.4761, 1 ) - 0.4902 ) < 1e-3, "chi2: P(chi2_1 > 0.476) = 0.49" );
      sis_modelo_defecto( &mo, 2 );
      mo.p = 2;
      snprintf( mo.zero, sizeof mo.zero, "AR2[MINK<-MUSKRAT],AR3[MINK<-MUSKRAT],MA1[MINK<-MUSKRAT]" );
      ok( sis_ceros_validos( &s, &mo, z, sizeof z ) == 1 && !strcmp( z, "AR2[MINK<-MUSKRAT]" ),
          "-zero: solo los coeficientes que el modelo tiene (AR3 y MA1 fuera)" );
      k = sis_orden_modelo( &s, &mo, "c", buf, sizeof buf, av, 32 );
      for ( j = 0; j < k - 1; j++ ) if ( !strcmp( av[j], "-zero" ) ) break;
      ok( j < k - 1 && !strcmp( av[j + 1], "AR2[MINK<-MUSKRAT]" ), "la orden lleva -zero" );
      sis_modelo_sin_par( &mo, &r, 1, 0 );
      ok( !r.enlace[1][0] && r.enlace[0][1] && sis_ceros_validos( &s, &r, z, sizeof z ) == 0,
          "sin el par MINK <- MUSKRAT, su cero ya no va" );
      ok( sis_modelo_triangular( &mo, &r, perm, 2 ) == 1 && r.enlace[0][1] && !r.enlace[1][0],
          "triangular MINK, MUSKRAT: MUSKRAT recibe de MINK y no al revés" );
   }
   /* --- el rango (S10b) ------------------------------------------------ */
   {
      SisRango rr;
      const char *av[SIS_MAX_SER + 16];
      char buf[64];
      int k, j, tiene_b = 0, tiene_name = 0;

      ok( !sis_rango_admite( &s, why, sizeof why ) && strstr( why, "MINK" ) && strstr( why, "d = 0" ),
          "rango: con MINK (d = 0) no, y dice por qué (el caso mixto)" );
      sis_rango_defecto( &rr );
      ok( rr.p == 2 && rr.q == 0 && !strcmp( rr.caso, "pre" ) && rr.B == 199,
          "rango: por omisión p = 2, q = 0, el caso de cada .pre, B = 199" );
      rr.B = 99; snprintf( rr.caso, sizeof rr.caso, "2" );
      k = sis_orden_rango( &s, &rr, "C1_rango", buf, sizeof buf, av, G_N( av ) );
      for ( j = 0; j < k; j++ ) {
         if ( !strcmp( av[j], "-bootstrap" ) && j + 1 < k && !strcmp( av[j + 1], "99" ) ) tiene_b = 1;
         if ( !strcmp( av[j], "-name" ) && j + 1 < k && !strcmp( av[j + 1], "C1_rango" ) ) tiene_name = 1;
      }
      ok( k == s.n + 10 && !strcmp( av[s.n], "2" ) && !strcmp( av[s.n + 1], "0" ) &&
          !strcmp( av[s.n + 2], "0" ) && !strcmp( av[s.n + 4], "2" ) && !strcmp( av[s.n + 5], "-lrtest" ) &&
          tiene_b && tiene_name, "rango: la orden de drvec, con -bootstrap y -name" );
      rr.B = 0;
      k = sis_orden_rango( &s, &rr, "C1_rango", buf, sizeof buf, av, G_N( av ) );
      ok( k == s.n + 8, "rango: B = 0, sin -bootstrap (los valores asintóticos)" );
      if ( argc > 2 ) {
         Sistema jj;
         const char *nom[] = { "LRM", "LRY", "IBO", "IDE" };
         memset( &jj, 0, sizeof jj );
         for ( j = 0; j < 4; j++ ) {
            snprintf( r, sizeof r, "%s/%s_u.pre", argv[2], nom[j] );
            sis_anade( &jj, r, why, sizeof why );
         }
         ok( jj.n == 4 && sis_rango_admite( &jj, why, sizeof why ),
             "rango: Johansen y Juselius (cuatro con d = 1) sí" );
         {
            SisModelo mo;
            char nb[256];
            sis_modelo_defecto( &mo, 4 );
            mo.clase = SIS_VEC; mo.p = 2; mo.r = 1; snprintf( mo.caso, sizeof mo.caso, "2" );
            sis_vec_y1_defecto( &mo, 4 );
            sis_normalizacion( &jj, &mo, nb, sizeof nb );
            ok( sis_vec_ny1( &mo, 4 ) == 1 && !strcmp( nb, "IDE" ), "vec: por omisión, normaliza sobre la última" );
            memset( mo.y1, 0, sizeof mo.y1 ); mo.y1[0] = 1;   /* sobre LRM, como sima */
            k = sis_orden_vec( &jj, &mo, "C1_c00", buf, sizeof buf, av, G_N( av ) );
            ok( k == 12 && strstr( av[0], "LRY_u.pre" ) && strstr( av[3], "LRM_u.pre" ) &&
                !strcmp( av[4], "2" ) && !strcmp( av[5], "0" ) && !strcmp( av[6], "1" ) &&
                !strcmp( av[8], "2" ) && !strcmp( av[9], "-fdhess" ) && !strcmp( av[11], "C1_c00" ),
                "vec: la orden, LRM al final (Y1), -case 2 -fdhess -name" );
            mo.y1[1] = 1;
            ok( sis_orden_vec( &jj, &mo, "x", buf, sizeof buf, av, G_N( av ) ) == -1,
                "vec: con r = 1 y dos en Y1, no hay orden" );
         }
      }
   }
   sis_mueve( &s, 1, 0 );
   ok( !strcmp( s.s[0].nombre, "MINK" ) && !strcmp( s.s[1].nombre, "MUSKRAT" ), "mover cambia el orden" );
   s.s[1].fin_anio = 1910;
   ok( sis_ventana( &s, &v, why, sizeof why ) != 0 && strstr( why, "misma fecha" ), "finales distintos: sin ventana" );
   s.s[1].fin_anio = 1911; s.s[1].freq = 4;
   ok( sis_ventana( &s, &v, why, sizeof why ) != 0 && strstr( why, "frecuencia" ), "frecuencias distintas: sin ventana" );
   sis_quita( &s, 0 );
   ok( s.n == 1 && !strcmp( s.s[0].nombre, "MUSKRAT" ), "quitar" );
   printf( "sistema: %d fallos\n", fallos );
   return fallos != 0;
}
