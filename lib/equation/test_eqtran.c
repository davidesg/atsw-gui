/* test_eqtran.c -- la ecuacion de transferencia, con los coeficientes que
 * drtran estimo de verdad para el m6.
 *
 * El contraste que importa: la GANANCIA que sale de la ecuacion escrita tiene
 * que ser la que imprime drtran. Si el convenio de signo estuviera mal, la
 * ecuacion se leeria bonita y el numero seria otro.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "eqtran.h"

static int fallos = 0;

static void check( int ok, const char *que, const char *visto )
{
   if ( ok ) return;
   fallos++;
   printf( "FAIL: %s; se vio: \"%s\"\n", que, visto ? visto : "(nada)" );
}

int main( void )
{
   char texto[4096];

   /* Los tres enlaces del m6 que salen en bien.out, con sus omegas.
    * EP <- EI : b=1, r=0, s=1
    * EP <- EC : b=1, r=0, s=2   omega2 = 0.360712, -0.529417, 0.158782
    */
   double w1[] = { 0.449540, 0.0 };
   double w2[] = { 0.360712, -0.529417, 0.158782 };
   double w2se[] = { 0.235667, 0.248822, 0.263151 };

   EqLink lnk[2] = {
      { "EI", 1, 1, 0, w1, NULL, NULL,  NULL },
      { "EC", 1, 2, 0, w2, NULL, w2se,  NULL },
   };

   /* --- la ecuacion ----------------------------------------------------- */
   eqtran_texto( texto, sizeof texto, "EP", lnk, 2, NULL );
   printf( "ecuacion:\n  %s\n\n", texto );

   check( strstr( texto, "EP_t  =" ) != NULL,
          "la ecuacion empieza por la salida", texto );
   check( strstr( texto, "EI" ) && strstr( texto, "EC" ),
          "y nombra las dos entradas", texto );
   check( strstr( texto, "N_t" ) != NULL,
          "y acaba en el ruido", texto );

   /* --- EL CONTRASTE: la ganancia ---------------------------------------- */
   /* nu(1) = w(1)/d(1). Con r = 0, d(1) = 1, y con el convenio BJ
    * w(1) = w0 - w1 - ... - ws. drtran imprime 0.731347 para EP <- EC.   */
   {
   double gan_bj = w2[0] - w2[1] - w2[2];
   double gan_ing = w2[0] + w2[1] + w2[2];
   char   b[64];

   printf( "ganancia de EP <- EC\n" );
   printf( "  convenio BJ (w0 - w1 - w2) : %.6f\n", gan_bj );
   printf( "  convenio ingenuo (la suma) : %.6f\n", gan_ing );
   printf( "  lo que imprime drtran      : 0.731347\n\n" );

   snprintf( b, sizeof b, "%.6f", gan_bj );
   check( fabs( gan_bj - 0.731347 ) < 1e-6,
          "la ganancia del convenio BJ es la que imprime drtran", b );
   check( fabs( gan_ing - 0.731347 ) > 0.1,
          "y la del convenio ingenuo NO lo es (si lo fuera, la prueba no "
          "distinguiria nada)", "" );
   }

   /* --- los signos en el texto ------------------------------------------- */
   /* w1 = -0.529417 es NEGATIVO y el convenio lo RESTA, asi que en la
    * ecuacion tiene que aparecer SUMANDO: -(-0.529) = +0.529.           */
   check( strstr( texto, "+ 0.5294" ) != NULL,
          "w1 = -0.529417, restado por el convenio, sale sumando 0.5294",
          texto );
   check( strstr( texto, "- 0.1588" ) != NULL,
          "y w2 = +0.158782, restado, sale restando 0.1588", texto );

   /* --- un denominador --------------------------------------------------- */
   {
   double w[] = { 0.5 };
   double d[] = { 0.0, 0.8 };
   EqLink r1  = { "WTI", 2, 0, 1, w, d, NULL, NULL };

   eqtran_texto( texto, sizeof texto, "IPC", &r1, 1, NULL );
   printf( "transferencia racional:\n  %s\n\n", texto );
   check( strstr( texto, "/" ) != NULL,
          "una transferencia con r>0 lleva su division", texto );
   check( strstr( texto, "B^2" ) != NULL,
          "y el retardo puro b=2 sale como B^2", texto );
   }

   printf( "%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
