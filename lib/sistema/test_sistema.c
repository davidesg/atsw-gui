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
#include "sistema.h"
#include "fue_pre_reader.h"

real macheps = 2.220446049250313e-16;   /* ver sistema.h, ENLACE */

static int fallos = 0;
static void ok( int c, const char *q ) { printf( "  %-58s %s\n", q, c ? "ok" : "FALLA" ); if ( !c ) fallos++; }

int main( int argc, char **argv )
{
   static Sistema s;
   SisVentana v;
   char why[1024], r[2048], a[32], b[32];
   const char *av[32];
   int k;

   if ( argc != 2 ) return 2;
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
