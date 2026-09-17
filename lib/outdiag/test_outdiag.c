/* test_outdiag.c -- leer la diagnosis del .out que escribe drtran.
 *
 *   test_outdiag <un .out del m6>
 *
 * Esto es un lector de TEXTO FORMATEADO, que es fragil por naturaleza: no hay
 * forma de compartir codigo con el motor porque el motor no emite nada legible
 * por maquina. La defensa es que la prueba se corre contra un .out DE VERDAD,
 * recien escrito por el motor en el mismo banco. Si el formato cambia, se sabe
 * el mismo dia.
 *
 * Los numeros que se exigen abajo estan copiados del .out del m6 con su red y
 * sus restricciones -- el mismo caso que estima la pantalla.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "outdiag.h"

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-58s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

static void okf( double visto, double esperado, double tol, const char *que )
{
   int bien = fabs( visto - esperado ) <= tol;

   printf( "  %-42s %10.4f (%9.4f)  %s\n", que, visto, esperado,
           bien ? "ok" : "FALLA" );
   if ( !bien ) fallos++;
}

int main( int argc, char **argv )
{
   Diagnosis d;
   int       i;

   if ( argc < 2 ) { fprintf( stderr, "uso: test_outdiag <.out>\n" ); return 2; }

   if ( od_parse_file( argv[1], &d ) != 0 ) {
      fprintf( stderr, "no encontre diagnosis en %s\n", argv[1] );
      return 1;
   }

   printf( "%s\n\n", argv[1] );

   /* --- las seis ecuaciones --------------------------------------------- */
   ok( d.ns == 6, "seis series de residuos, una por ecuacion" );
   ok( !strcmp( d.s[0].nombre, "a[1]" ) && !strcmp( d.s[5].nombre, "a[6]" ),
       "se llaman a[1]..a[6], como las titula el motor" );

   printf( "\n  los residuos, serie a serie:\n" );
   for ( i = 0; i < d.ns; i++ )
      printf( "     %-6s n=%2d  sd=%7.3f  asim=%+6.3f  curt=%+6.3f   "
              "L-B Q(%d)=%6.2f\n",
              d.s[i].nombre, d.s[i].nobs, d.s[i].sd, d.s[i].skew, d.s[i].kurt,
              d.s[i].lb.df, d.s[i].lb.q );
   printf( "\n" );

   /* EL FALLO QUE ESTO PILLO. sscanf(" %d observations:") devuelve 1 en cuanto
    * lee el numero, aunque el literal que sigue NO case; asi que el grafico de
    * la serie, el histograma y la CCF --que empiezan por un numero-- se
    * colaban por ahi y pisaban nobs. Estas dos lo cierran.              */
   ok( d.s[0].nobs == 64 && d.s[5].nobs == 64,
       "64 observaciones, y no el ultimo numero que paso por delante" );
   ok( d.s[0].tiene_hist && d.s[0].fuera1 > 0.0,
       "y el histograma se lee, que era lo otro que se comia ese fallo" );

   okf( d.s[0].sd,   28.040111, 1e-5, "  a[1]: desviacion tipica" );
   okf( d.s[0].skew,  0.036795, 1e-5, "  a[1]: asimetria" );
   okf( d.s[0].kurt, -0.416468, 1e-5, "  a[1]: curtosis" );

   /* El histograma con el porcentaje observado CONTRA EL ESPERADO es el
    * contraste de normalidad mas barato que hay, y el motor lo pone al pie
    * del dibujo -- justo lo que el estudio de TASTE señalo como lo mejor de
    * su diseño.                                                         */
   okf( d.s[0].fuera1, 31.25, 1e-2, "  a[1]: % fuera de (-1,+1)" );
   okf( d.s[0].esp1,   31.74, 1e-2, "         lo esperado" );
   okf( d.s[0].fuera2,  4.69, 1e-2, "  a[1]: % fuera de (-2,+2)" );
   okf( d.s[0].esp2,    4.56, 1e-2, "         lo esperado" );

   /* El ULTIMO Ljung-Box de la ACF, el que resume. */
   ok( d.s[0].lb.hay && d.s[0].lb.df == 18, "  a[1]: el L-B de la ACF, con 18 gl" );
   okf( d.s[0].lb.q, 22.51, 1e-2,           "  a[1]: y su Q" );

   /* --- lo multivariante ------------------------------------------------ */
   printf( "\n  Hosking (retardo %d): Q(%d) = %.4f, p = %.4f  ->  %s\n",
           d.hosking_lag, d.hosking.df, d.hosking.q, d.hosking.p,
           d.hosking_blanco ? "ruido blanco" : "NO es ruido blanco" );
   printf( "  Jarque-Bera: JB(%d) = %.4f, p = %.4f  ->  %s\n\n",
           d.jb.df, d.jb.q, d.jb.p,
           d.jb_normal ? "normal" : "NO normal" );

   ok( d.hosking.hay, "el portmanteau multivariante de Hosking se lee" );
   okf( d.hosking.q, 275.9164, 1e-3, "  Q" );
   ok( d.hosking.df == 288 && d.hosking_lag == 8, "  288 gl al retardo 8" );
   okf( d.hosking.p, 0.6854, 1e-4,   "  p" );
   ok( d.hosking_blanco, "  y el motor NO rechaza: los residuos son blancos" );

   ok( d.jb.hay && d.jb.df == 12, "el Jarque-Bera multivariante, con 12 gl" );
   okf( d.jb.p, 0.0128, 1e-4,     "  p" );
   ok( !d.jb_normal,
       "  y SI rechaza la normalidad -- los dos veredictos van al reves" );

   /* --- los enlaces, que es lo accionable -------------------------------- */
   printf( "  los cuatro enlaces:\n" );
   for ( i = 0; i < d.ne; i++ )
      printf( "     %d (%-4s)  transferencia Q(%2d)=%7.4f p=%.4f  %-14s"
              "   exogeneidad Q(%2d)=%7.4f p=%.4f  %s\n",
              d.e[i].num, d.e[i].entrada,
              d.e[i].transfer.df, d.e[i].transfer.q, d.e[i].transfer.p,
              d.e[i].adecuado ? "ADECUADO" : "*** NO ADECUADO",
              d.e[i].exogen.df, d.e[i].exogen.q, d.e[i].exogen.p,
              d.e[i].exogeno ? "exogena" : "NO exogena" );
   printf( "\n" );

   ok( d.ne == 4, "cuatro enlaces, los de la red del m6" );
   ok( !strcmp( d.e[0].entrada, "EI" ) && !strcmp( d.e[2].entrada, "EU" ),
       "nombrados por su entrada: EI, EC, EU, EC" );

   /* EL CONTRASTE QUE IMPORTA: en esta corrida DOS de los cuatro enlaces NO
    * son adecuados. Si el lector diera que los cuatro estan bien, no estaria
    * leyendo el veredicto sino inventandolo.                            */
   ok( od_no_adecuados( &d ) == 2,
       "DOS enlaces no son adecuados -- y se ven como tales" );
   ok( d.e[0].adecuado == 1, "  el 1 (EI) si lo es, p = 0.9896" );
   ok( d.e[1].adecuado == 0, "  el 2 (EC) NO, p = 0.0253" );
   ok( d.e[2].adecuado == 0, "  el 3 (EU) NO, p = 0.0128" );
   okf( d.e[1].transfer.p, 0.0253, 1e-4, "  la p del 2" );
   okf( d.e[2].transfer.p, 0.0128, 1e-4, "  la p del 3" );

   /* Y la exogeneidad va por separado: los cuatro la pasan. Son dos
    * diagnosticos opuestos y se arreglan de forma opuesta.              */
   ok( od_no_exogenos( &d ) == 0,
       "y ninguno falla la exogeneidad: son contrastes distintos" );
   ok( d.e[1].exogen_signif == 2,
       "  el 2 tiene 2 retardos negativos significativos, y aun asi pasa" );

   ok( d.e[0].veredicto[0] != 0, "la frase del motor se conserva" );
   printf( "     \"%s\"\n", d.e[0].veredicto );

   ok( d.tiene_logl && fabs( d.logl + 1701.348826 ) < 1e-5,
       "la verosimilitud, -1701.348826" );

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
