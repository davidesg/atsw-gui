/* test_slots.c -- la tabla de slots y el .cns, contra lo que imprime el motor.
 *
 *   test_slots <dir del m6>
 *
 * El oraculo es la salida del propio drtran sobre los mismos ficheros:
 *
 *   drtran M6_EP.pre M6_EI.pre M6_EU.pre M6_EC.pre M6_EA.pre M6_P.pre \
 *          -n m6_net.dag -c m6_net_full.cns
 *
 *   Structural parameters: 67   (free: 52, fixed/shared: 15)
 *
 * y la tabla de parametros con sus nombres. Aqui se exigen ese recuento y esos
 * nombres, y ademas las CINCO formas del lenguaje del .cns, que el m6 usa
 * todas: free, fijo, compartido, producto y combinacion lineal.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "fue_pre_reader.h"
#include "slots.h"

real macheps = 2.220446049250313e-16;
FILE *outputv;

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-58s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

static void okn( int visto, int esperado, const char *que )
{
   printf( "  %-46s %4d (%4d)  %s\n", que, visto, esperado,
           visto == esperado ? "ok" : "FALLA" );
   if ( visto != esperado ) fallos++;
}

/* El orden en que se le pasan al motor. */
static const char *BASE[] = { NULL, "M6_EP", "M6_EI", "M6_EU",
                                    "M6_EC", "M6_EA", "M6_P" };
#define NSER 6

int main( int argc, char **argv )
{
   struct Tusmodel Tm[NSER + 1];
   struct Tseries  Ts[NSER + 1];
   real          **dm[NSER + 1];
   const char     *nom[NSER + 1];
   NetLink         lnk[NET_MAX_LINK];
   NetError        ne;
   CnsError        ce;
   SlotTable       st;
   char            path[1024], why[512];
   int             i, n, nc;

   outputv = stderr;
   if ( argc < 2 ) { fprintf( stderr, "uso: test_slots <dir m6>\n" ); return 2; }

   for ( i = 1; i <= NSER; i++ ) {
      snprintf( path, sizeof path, "%s/%s.pre", argv[1], BASE[i] );
      dm[i] = NULL;
      if ( read_fue_pre( path, &Tm[i], &Ts[i], &dm[i] ) != 0 ) {
         fprintf( stderr, "no pude leer %s\n", path );
         return 1;
      }
      nom[i] = Ts[i].name;
   }
   nom[0] = NULL;

   printf( "series: " );
   for ( i = 1; i <= NSER; i++ ) printf( "%s ", nom[i] );
   printf( "\n\n" );

   /* --- la red ---------------------------------------------------------- */
   snprintf( path, sizeof path, "%s/m6_net.dag", argv[1] );
   n = net_read( path, nom, NSER, lnk, NET_MAX_LINK, &ne );
   if ( n < 0 ) { fprintf( stderr, "%s\n", net_error_en(&ne, why, sizeof why) ); return 1; }
   okn( n, 4, "enlaces en la red" );

   /* --- la tabla, ANTES del .cns ---------------------------------------- */
   slots_build( &st, Tm, NSER, lnk, n, NULL );

   printf( "\n  los primeros slots, que son los de la transferencia:\n" );
   for ( i = 1; i <= 11 && i <= st.n; i++ ) printf( "     %s\n", st.name[i] );
   printf( "\n" );

   okn( st.n, 67, "slots que genera el modelo" );

   /* Los nombres, tal como los imprime el motor en su tabla. */
   ok( slots_find( &st, "omega1[0]" ) && slots_find( &st, "omega1[1]" ),
       "el enlace 1 tiene omega1[0] y omega1[1]  (s=1)" );
   ok( slots_find( &st, "omega3[3]" ) != 0,
       "y el enlace 3 llega hasta omega3[3]  (s=3)" );
   ok( slots_find( &st, "omega3[4]" ) == 0,
       "y no mas alla" );
   ok( slots_find( &st, "theta_2[B^1]" ) != 0,
       "el MA de la serie 2 se llama theta_2[B^1]" );
   ok( slots_find( &st, "theta_5[f=1]" ) != 0,
       "y un factor de frecuencia fija, theta_5[f=1]" );
   ok( slots_find( &st, "omega_d1[1,0]" ) != 0,
       "los deterministas, omega_d1[1,0]" );
   ok( slots_find( &st, "log(var2/var1)" ) != 0,
       "las varianzas relativas, log(var2/var1)" );

   /* LA MEDIA ES EL CONTRATO EN MINIATURA. Una media con bandera 0 en el .pre
    * es ESPECIFICACION, no semilla: no hay parametro que estimar, y por tanto
    * no hay slot. Que no salga mu[i] no es un olvido, es la unica lectura
    * correcta del fichero.                                              */
   {
   int libres = 0, fijas = 0;

   for ( i = 1; i <= NSER; i++ ) {
      char nm[32];

      snprintf( nm, sizeof nm, "mu[%d]", i );
      if ( Tm[i].Imu ) { libres++;  ok( slots_find( &st, nm ) != 0,
                                       "  el .pre deja la media libre -> hay slot" ); }
      else             { fijas++;   ok( slots_find( &st, nm ) == 0,
                                       "  el .pre la declara FIJA     -> no hay slot" ); }
   }
   printf( "     (%d medias libres, %d fijas en los seis .pre)\n", libres, fijas );
   ok( fijas > 0, "y en el m6 hay alguna fija, o esto no probaria nada" );

   /* Las seis del m6 son fijas, asi que la rama contraria no se recorre con
    * estos ficheros. Se fuerza, para que la prueba cubra las dos: si un dia
    * dejara de aparecer el slot al levantar la bandera, se sabria.       */
   if ( !libres ) {
      SlotTable otra;
      int       guardado = Tm[1].Imu;

      Tm[1].Imu = 1;
      slots_build( &otra, Tm, NSER, lnk, n, NULL );
      ok( slots_find( &otra, "mu[1]" ) != 0,
          "  y con la bandera levantada a mano, el slot aparece" );
      okn( otra.n, st.n + 1, "  un slot mas, exactamente uno" );
      Tm[1].Imu = guardado;
   }
   }
   ok( slots_find( &st, "q[5,2]" ) != 0 && slots_find( &st, "q[2,5]" ) == 0,
       "las covarianzas solo por debajo de la diagonal: q[5,2], no q[2,5]" );

   /* Las q nacen FIJAS en cero: la diagonal es el caso por defecto. */
   {
   int k = slots_find( &st, "q[5,2]" );

   ok( st.kind[k] == SLOT_FIXED && st.value[k] == 0.0,
       "y nacen FIJAS en cero, no libres" );
   }

   /* Un coeficiente que el .pre marca FIJO no es un slot: es especificacion. */
   okn( slots_nfree( &st ), 67 - 15, "libres antes del .cns, sin las 15 q" );

   /* --- el .cns, con las cinco formas ----------------------------------- */
   snprintf( path, sizeof path, "%s/m6_net_full.cns", argv[1] );
   nc = cns_read( path, &st, &ce );
   if ( nc < 0 ) {
      printf( "  cns_read: %s\n", cns_error_en( &ce, why, sizeof why ) );
      fallos++;
   }
   printf( "\n  el .cns aplica %d restricciones\n\n", nc );
   okn( nc, 6, "restricciones en m6_net_full.cns" );
   /* 3 covarianzas liberadas + 2 productos + 1 combinacion lineal */

   /* free */
   {
   int k = slots_find( &st, "q[5,2]" );
   ok( st.kind[k] == SLOT_FREE, "q[5,2] = free    ->  liberada" );
   }
   /* producto */
   {
   int k = slots_find( &st, "omega1[1]" );

   ok( st.kind[k] == SLOT_PRODUCT &&
       strcmp( st.name[st.pa[k]], "omega1[0]" ) == 0 &&
       strcmp( st.name[st.pb[k]], "theta_2[B^1]" ) == 0,
       "omega1[1] = omega1[0] * theta_2[B^1]  ->  PRODUCTO" );
   }
   /* combinacion lineal */
   {
   int k = slots_find( &st, "omega3[0]" );

   ok( st.kind[k] == SLOT_LINCOMB && st.nlc[k] == 3,
       "omega3[0] = omega3[1] + omega3[2] + omega3[3]  ->  3 terminos" );
   ok( st.kind[k] == SLOT_LINCOMB &&
       strcmp( st.name[st.lc_a[k][2]], "omega3[3]" ) == 0 &&
       st.lc_b[k][2] == 0,
       "  y el tercero es omega3[3], sin producto" );
   }

   /* EL RECUENTO DEL MOTOR: 67 parametros, 52 libres, 15 fijos o compartidos */
   okn( st.n, 67, "Structural parameters" );
   okn( slots_nfree( &st ), 52, "free" );
   okn( st.n - slots_nfree( &st ), 15, "fixed/shared" );

   /* --- lo que se escribe se relee igual -------------------------------- */
   {
   SlotTable otra;
   int       m, k, igual = 1;

   m = cns_write( "/tmp/_test_slots.cns", &st, "escrito por la prueba" );
   okn( m, 6, "lineas que escribe cns_write" );

   slots_build( &otra, Tm, NSER, lnk, n, NULL );
   cns_read( "/tmp/_test_slots.cns", &otra, &ce );

   for ( k = 1; k <= st.n; k++ )
      if ( st.kind[k] != otra.kind[k] || st.alias[k] != otra.alias[k] ||
           st.pa[k] != otra.pa[k] || st.pb[k] != otra.pb[k] ||
           st.nlc[k] != otra.nlc[k] ) igual = 0;
   ok( igual, "y la tabla que sale es la misma" );
   remove( "/tmp/_test_slots.cns" );
   }

   /* --- lo que se rechaza ----------------------------------------------- */
   {
   static const struct { const char *linea, *que; CnsErr err; } mal[] = {
      { "no_existe = free\n",          "un parametro que el modelo no tiene", CNS_EUNKNOWN },
      { "omega1[0] = omega1[0]\n",     "un slot compartido consigo mismo",    CNS_ESELF },
      { "omega1[1] = omega1[1] * omega2[0]\n", "un slot como factor de si mismo", CNS_ESELF },
      { "omega1[1] = no_existe * omega2[0]\n", "un operando que no existe",    CNS_EOPERAND },
      { "omega1[0] = manzanas\n",      "un valor que no es numero ni slot",   CNS_EPARSE },
   };
   size_t t;

   printf( "\n  lo que se rechaza, y lo que dice el motor:\n" );
   for ( t = 0; t < sizeof mal / sizeof *mal; t++ ) {
      SlotTable prueba;
      FILE     *f = fopen( "/tmp/_test_slots_bad.cns", "w" );
      int       rc;

      fputs( mal[t].linea, f );
      fclose( f );
      slots_build( &prueba, Tm, NSER, lnk, n, NULL );
      rc = cns_read( "/tmp/_test_slots_bad.cns", &prueba, &ce );
      printf( "     %-38s -> \"%s\"\n", mal[t].que,
              cns_error_en( &ce, why, sizeof why ) );
      ok( rc < 0 && ce.err == mal[t].err, mal[t].que );
   }
   remove( "/tmp/_test_slots_bad.cns" );
   }

   /* Si nos lo piden, dejamos el .cns escrito: el guion comprueba luego que EL
    * MOTOR lo lee y cuenta lo mismo. Que los dos LEAN igual lo garantiza
    * compartir el lector; que lo que mtram ESCRIBE lo lea drtran hay que
    * preguntarselo a drtran.                                            */
   if ( argc >= 4 && strcmp( argv[2], "--write" ) == 0 )
      ok( cns_write( argv[3], &st, "reescrito por la prueba" ) > 0,
          "se reescribe para que lo lea el motor" );

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
