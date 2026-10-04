/* test_netfile.c -- el .dag: leerlo, ordenarlo, y cazar el ciclo.
 *
 *   test_netfile <m6_net.dag>
 *
 * El orden topologico no se comprueba contra una lista escrita a mano --eso
 * solo diria que hoy sale lo de siempre-- sino contra LA PROPIEDAD que lo
 * define: en la red del m6, toda serie aparece despues de todas las que la
 * alimentan. Si el orden fuera otro y siguiera cumpliendola, seguiria siendo
 * correcto, y la prueba tiene que aceptarlo.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "netfile.h"

/* UN FICHERO TEMPORAL, en la carpeta temporal del sistema y no en "/tmp" a
 * fuego: en Windows nativo "/tmp" es D:\tmp, que no existe, y la prueba
 * fallaba por no poder escribir. Lo vio la CI de Windows.               */
static const char *temporal( const char *nombre )
{
    static char buf[4][1024];
    static int  k;
    const char *d = getenv( "TMPDIR" );

    if ( !d || !*d ) d = getenv( "TEMP" );
    if ( !d || !*d ) d = getenv( "TMP" );
#ifdef _WIN32
    if ( !d || !*d ) d = ".";
#else
    if ( !d || !*d ) d = "/tmp";
#endif
    k = ( k + 1 ) % 4;
    snprintf( buf[k], sizeof buf[k], "%s/%s", d, nombre );
    return buf[k];
}

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-56s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

/* Las series del m6, en el orden en que se le pasan al motor. */
static const char *NOM[] = { NULL, "EP", "EI", "EC", "EU" };
#define NSER 4

static void pinta( const NetLink *l, int n )
{
   int k;

   for ( k = 0; k < n; k++ )
      printf( "     %-4s <- %-4s   b=%d r=%d s=%d\n",
              NOM[l[k].out], NOM[l[k].inp], l[k].b, l[k].r, l[k].s );
}

int main( int argc, char **argv )
{
   NetLink  lnk[NET_MAX_LINK];
   NetError e;
   char     why[512];
   int      topo[NET_MAX_SER + 1];
   int      n, k, i;

   if ( argc < 2 ) { fprintf( stderr, "uso: test_netfile <m6_net.dag>\n" ); return 2; }

   /* --- leerlo ---------------------------------------------------------- */
   n = net_read( argv[1], NOM, NSER, lnk, NET_MAX_LINK, &e );
   if ( n < 0 ) {
      fprintf( stderr, "no pude leerlo: %s\n", net_error_en( &e, why, sizeof why ) );
      return 1;
   }
   printf( "la red del m6:\n" );
   pinta( lnk, n );
   printf( "\n" );

   ok( n == 4, "el .dag del m6 tiene cuatro enlaces" );
   ok( lnk[0].out == 1 && lnk[0].inp == 2 && lnk[0].b == 1 && lnk[0].s == 1,
       "EP <- EI  con b=1, s=1" );
   ok( lnk[3].out == 4 && lnk[3].inp == 3 && lnk[3].b == 2,
       "EU <- EC  con b=2" );

   /* --- el orden topologico, por la propiedad que lo define -------------- */
   ok( net_topo( lnk, n, NSER, topo ), "la red del m6 es aciclica" );
   {
   int pos[NET_MAX_SER + 1], bien = 1;

   for ( i = 1; i <= NSER; i++ ) pos[topo[i]] = i;
   printf( "\n  orden:" );
   for ( i = 1; i <= NSER; i++ ) printf( " %s", NOM[topo[i]] );
   printf( "\n\n" );

   for ( k = 0; k < n; k++ )
      if ( pos[lnk[k].inp] >= pos[lnk[k].out] ) bien = 0;
   ok( bien, "toda serie va despues de las que la alimentan" );
   }

   /* EU alimenta a EI y EC alimenta a EU: EU es salida Y entrada a la vez.
    * Es lo que distingue una RED de una estrella, y por eso el m6 esta aqui. */
   ok( net_indegree( lnk, n, 4 ) == 1 && net_outdegree( lnk, n, 4 ) == 1,
       "EU es salida de EC y entrada de EI a la vez" );
   ok( net_indegree( lnk, n, 1 ) == 2 && net_outdegree( lnk, n, 1 ) == 0,
       "EP recibe dos y no alimenta a nadie" );

   /* --- el ciclo -------------------------------------------------------- */
   {
   NetLink c[3];
   int     ciclo[NET_MAX_SER + 2], nc = 0;

   /* EP <- EI <- EU <- EP */
   c[0].out = 1; c[0].inp = 2;  c[0].b = c[0].r = c[0].s = 0;
   c[1].out = 2; c[1].inp = 4;  c[1].b = c[1].r = c[1].s = 0;
   c[2].out = 4; c[2].inp = 1;  c[2].b = c[2].r = c[2].s = 0;

   ok( net_topo( c, 3, NSER, topo ) == 0, "un ciclo NO admite orden topologico" );
   ok( net_cycle( c, 3, NSER, ciclo, &nc ) == 1, "y se encuentra" );

   printf( "\n  el ciclo:" );
   for ( i = 0; i < nc; i++ ) printf( "%s%s", i ? " -> " : " ", NOM[ciclo[i]] );
   printf( "\n\n" );

   ok( nc == 4 && ciclo[0] == ciclo[nc - 1],
       "se devuelve cerrado, el primero repetido al final" );
   /* Los tres del ciclo tienen que estar, en algun giro. */
   {
   int visto[NET_MAX_SER + 1];

   for ( i = 1; i <= NSER; i++ ) visto[i] = 0;
   for ( i = 0; i < nc; i++ ) visto[ciclo[i]] = 1;
   ok( visto[1] && visto[2] && visto[4] && !visto[3],
       "y nombra las tres del ciclo, no la que cuelga" );
   }
   }

   /* Una serie que solo CUELGA de un ciclo no es parte del ciclo. */
   {
   NetLink c[4];
   int     ciclo[NET_MAX_SER + 2], nc = 0, visto[NET_MAX_SER + 1];

   c[0].out = 1; c[0].inp = 2;  c[1].out = 2; c[1].inp = 4;
   c[2].out = 4; c[2].inp = 1;  c[3].out = 3; c[3].inp = 1;   /* EC <- EP */
   for ( i = 0; i < 4; i++ ) c[i].b = c[i].r = c[i].s = 0;

   net_cycle( c, 4, NSER, ciclo, &nc );
   for ( i = 1; i <= NSER; i++ ) visto[i] = 0;
   for ( i = 0; i < nc; i++ ) visto[ciclo[i]] = 1;
   ok( !visto[3], "EC cuelga del ciclo pero no esta EN el ciclo" );
   }

   /* --- ida y vuelta ---------------------------------------------------- */
   {
   NetLink otra[NET_MAX_LINK];
   int     m, igual = 1;

   ok( net_write( temporal( "_test_netfile.dag" ), NOM, lnk, n, "prueba" ) == 0,
       "se puede escribir" );
   m = net_read( temporal( "_test_netfile.dag" ), NOM, NSER, otra, NET_MAX_LINK, &e );
   for ( k = 0; k < n && k < m; k++ )
      if ( memcmp( &lnk[k], &otra[k], sizeof( NetLink ) ) != 0 ) igual = 0;
   ok( m == n && igual, "y lo que se relee es identico" );
   remove( temporal( "_test_netfile.dag" ) );
   }

   /* --- los hechos que se rechazan, y COMO se cuentan -------------------- */
   {
   static const struct { const char *linea, *que; NetErr err; } mal[] = {
      { "NO_EXISTE <- EI  1 0 0\n", "una serie que no esta cargada", NET_EUNKNOWN },
      { "EP <- EP  1 0 0\n",        "una serie alimentandose a si misma", NET_ESELF },
      { "EP -> EI  1 0 0\n",        "la flecha al reves",           NET_EARROW },
      { "EP <- EI  -1 0 0\n",       "un retardo negativo",          NET_ENEG },
      { "EP <- EI\n",               "una linea sin ordenes",        NET_ESYNTAX },
   };
   size_t t;

   printf( "\n  lo que se rechaza, y lo que dice el motor:\n" );
   for ( t = 0; t < sizeof mal / sizeof *mal; t++ ) {
      FILE *f = fopen( temporal( "_test_netfile_bad.dag" ), "w" );
      int   rc;

      fputs( mal[t].linea, f );
      fclose( f );
      rc = net_read( temporal( "_test_netfile_bad.dag" ), NOM, NSER, lnk,
                     NET_MAX_LINK, &e );
      printf( "     %-34s -> \"%s\"\n", mal[t].que,
              net_error_en( &e, why, sizeof why ) );
      ok( rc < 0 && e.err == mal[t].err, mal[t].que );
      ok( e.line == 1, "  y dice en que linea" );
   }
   remove( temporal( "_test_netfile_bad.dag" ) );

   /* La bateria del motor comprueba esta frase al pie de la letra: la salida
    * del motor en ingles es una propiedad declarada del puerto.         */
   {
   FILE *f = fopen( temporal( "_test_netfile_bad.dag" ), "w" );

   fputs( "NO_EXISTE <- EI  1 0 0\n", f );
   fclose( f );
   net_read( temporal( "_test_netfile_bad.dag" ), NOM, NSER, lnk, NET_MAX_LINK, &e );
   net_error_en( &e, why, sizeof why );
   ok( strstr( why, "unknown series" ) != NULL,
       "la frase del motor sigue diciendo \"unknown series\"" );
   remove( temporal( "_test_netfile_bad.dag" ) );
   }
   }

   /* --- REORDENAR LAS SERIES -------------------------------------------- */
   /* La comprobacion no es que la permutacion sea la que yo creo: es que los
    * enlaces SIGAN NOMBRANDO A LAS MISMAS SERIES. Eso es lo unico que importa
    * y es lo unico que no se puede falsear con un indice mal puesto.     */
   {
   NetLink orig[NET_MAX_LINK], mov[NET_MAX_LINK];
   char    antes[NET_MAX_LINK][32], despues[NET_MAX_LINK][32];
   int     perm[NET_MAX_SER + 1];
   int     nm, de, a, k, bien, casos = 0;

   n = net_read( argv[1], NOM, NSER, orig, NET_MAX_LINK, &e );

   printf( "\n  reordenar: los enlaces tienen que seguir nombrando lo mismo\n" );

   for ( de = 1; de <= NSER; de++ )
      for ( a = 1; a <= NSER; a++ ) {
         const char *nom2[NSER + 1];
         int         i2;

         if ( de == a ) continue;

         /* Como se llamaban los enlaces ANTES */
         for ( k = 0; k < n; k++ )
            snprintf( antes[k], sizeof antes[k], "%s<-%s",
                      NOM[orig[k].out], NOM[orig[k].inp] );

         /* Los nombres, movidos igual que las series */
         net_perm_move( NSER, de, a, perm );
         for ( i2 = 1; i2 <= NSER; i2++ ) nom2[perm[i2]] = NOM[i2];
         nom2[0] = NULL;

         memcpy( mov, orig, sizeof orig );
         nm = net_remap( mov, n, perm );

         for ( k = 0; k < nm; k++ )
            snprintf( despues[k], sizeof despues[k], "%s<-%s",
                      nom2[mov[k].out], nom2[mov[k].inp] );

         bien = ( nm == n );
         for ( k = 0; k < nm && bien; k++ )
            if ( strcmp( antes[k], despues[k] ) ) bien = 0;
         if ( !bien ) {
            printf( "     FALLA moviendo %s de %d a %d: %s -> %s\n",
                    NOM[de], de, a, antes[0], despues[0] );
            fallos++;
         }
         casos++;
      }
   printf( "     %d movimientos probados, los %d x %d posibles\n",
           casos, NSER, NSER - 1 );
   ok( casos == NSER * ( NSER - 1 ), "se probaron todos los movimientos" );

   /* Y el caso concreto que destapo esto: si EP e EI se intercambian y nadie
    * remapea, "EP <- EI" pasa a leerse "EI <- EP". Al reves.           */
   {
   NetLink uno = { 1, 2, 1, 0, 1 };

   net_perm_move( NSER, 1, 2, perm );
   memcpy( mov, &uno, sizeof uno );
   net_remap( mov, 1, perm );
   ok( mov[0].out == 2 && mov[0].inp == 1,
       "  EP<-EI, con EP y EI intercambiadas, pasa a ser 2<-1: sigue siendo EP<-EI" );
   }

   /* Quitar una serie se lleva por delante los enlaces que la nombraban. */
   {
   int nq;

   net_perm_drop( NSER, 4, perm );          /* fuera EU */
   memcpy( mov, orig, sizeof orig );
   nq = net_remap( mov, n, perm );
   ok( nq == 2, "quitar EU deja 2 de los 4 enlaces: los otros la nombraban" );
   ok( perm[1] == 1 && perm[4] == 0 && perm[3] == 3,
       "  y las de detras corren una plaza, la quitada recibe 0" );
   }
   }

   /* --- y si nos lo piden, lo reescribimos: el guion comprueba luego que EL
    * MOTOR se lo traga y da la misma red. Que el GUI y el motor lean igual lo
    * garantiza compartir el lector; que lo que el GUI ESCRIBE sea legible por
    * el motor hay que comprobarlo con el motor.                          */
   if ( argc >= 3 ) {
      n = net_read( argv[1], NOM, NSER, lnk, NET_MAX_LINK, &e );
      ok( net_write( argv[2], NOM, lnk, n, "escrito por la prueba" ) == 0,
          "se reescribe para que lo lea el motor" );
   }

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
