/* test_outfcst.c -- leer la prevision del .out que escribe drtran.
 *
 *   test_outfcst <.out con -f>  [<.out con -estwin -C>]
 *
 * Como test_outdiag: es un lector de TEXTO FORMATEADO y la defensa es correrlo
 * contra .out DE VERDAD, recien escritos por el motor en el mismo banco.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "outfcst.h"

static int fallos = 0;

static void ok( int cond, const char *que )
{
   printf( "  %-58s %s\n", que, cond ? "ok" : "FALLA" );
   if ( !cond ) fallos++;
}

static void okf( double visto, double esperado, double tol, const char *que )
{
   int bien = fabs( visto - esperado ) <= tol;

   printf( "  %-42s %11.6f (%10.6f)  %s\n", que, visto, esperado,
           bien ? "ok" : "FALLA" );
   if ( !bien ) fallos++;
}

/* --- la prevision del m6 ------------------------------------------------ */
static void prevision( const char *path )
{
   Forecast       f;
   const OfSerie *s;
   int            i;

   printf( "%s\n\n", path );
   if ( of_parse_file( path, &f ) != 0 ) {
      printf( "  no encontre prevision\n" );
      fallos++;
      return;
   }

   ok( f.ns == 6, "las seis series del m6, cada una con su prevision" );

   s = of_serie( &f, "EP" );
   ok( s != NULL, "se puede pedir una por su nombre: EP" );
   if ( !s ) return;

   printf( "\n  EP, origen %s, %d periodos:\n", s->origen, s->lead );
   for ( i = s->nf - s->nprev; i < s->nf; i++ )
      printf( "     %-8s nivel %10.2f  ±%7.2f    anual %9.2f  ±%7.2f\n",
              s->f[i].fecha, s->f[i].nivel, s->f[i].sd_nivel,
              s->f[i].var_anu, s->f[i].sd_anu );
   printf( "\n" );

   ok( !strcmp( s->origen, "3/1993" ), "  el origen, 3/1993" );
   ok( s->lead == 8,                   "  ocho periodos" );
   ok( s->nprev == 8,      "  y ocho filas de prevision, ni una mas" );
   ok( s->nf > s->nprev,
       "  ademas de las observadas, que van en la misma tabla" );

   /* LA DISTINCION QUE EL FORMATO HACE Y QUE HAY QUE NO PERDER: una fila
    * OBSERVADA no trae desviacion y si trae error; una PREVISTA, al reves.
    * Si el lector las mezclara, se dibujarian bandas sobre el pasado.   */
   {
   const OfFila *ult_obs = &s->f[s->nf - s->nprev - 1];
   const OfFila *pri_prev = &s->f[s->nf - s->nprev];

   ok( !ult_obs->tiene_sd,
       "  la ultima OBSERVADA no tiene desviacion tipica" );
   ok( pri_prev->tiene_sd && pri_prev->sd_nivel > 0.0,
       "  y la primera PREVISTA si la tiene" );
   ok( !pri_prev->tiene_err,
       "  al reves con el error: la prevista no lo tiene todavia" );
   }

   /* La desviacion crece con el horizonte: no es una comprobacion de formato
    * sino de que se estan leyendo las columnas que son.                */
   {
   const OfFila *a = &s->f[s->nf - s->nprev];
   const OfFila *b = &s->f[s->nf - 1];

   ok( b->sd_nivel > a->sd_nivel,
       "  la desviacion del nivel CRECE con el horizonte" );
   }

   /* La tabla tal cual, para la ventana: de «FORECAST REPORT» a la linea
    * que la cierra, y ni una fila de otra serie.                       */
   {
   static char t[65536];
   int         filas = 0;
   const char *l, *ult;
   Forecast    g;

   ok( of_tabla_file( path, "EP", t, sizeof t ) == 0,
       "  la tabla de EP, tal cual esta en el .out" );
   ok( !strncmp( t + strspn( t, " \n" ), "FORECAST REPORT", 15 ),
       "  empieza en «FORECAST REPORT»" );
   ok( strstr( t, "EP" ) != NULL && !strstr( t + 20, "FORECAST REPORT" ),
       "  y es UNA: no se mete en la tabla de la serie siguiente" );
   ult = strrchr( t, '\n' );
   ok( ult && ult[1 + strspn( ult + 1, " " )] == '+',
       "  acaba en la linea «+---» que la cierra" );
   /* las filas que lleva son las que lee of_parse */
   if ( of_parse( t, &g ) == 0 && g.ns == 1 ) filas = g.s[0].nf;
   ok( filas == s->nf, "  con las mismas filas que lee of_parse" );
   (void) l;
   }

   /* La negativa razonada del motor es un resultado, no un hueco. */
   ok( f.decomp == 0,
       "la descomposicion de la varianza NO se da: Sigma no es diagonal" );
   printf( "     (con innovaciones correlacionadas la descomposicion no es\n"
           "      unica: exige una ordenacion, que es el problema del VAR)\n" );
}

/* --- la evaluacion fuera de muestra ------------------------------------- */
static void evaluacion( const char *path )
{
   Forecast f;
   int      i, crece = 1;

   printf( "\n%s\n\n", path );
   if ( of_parse_file( path, &f ) != 0 || !f.tiene_ev ) {
      printf( "  no encontre evaluacion\n" );
      fallos++;
      return;
   }

   printf( "  %s: %d origenes, de la observacion %d a la %d, horizonte %d\n\n",
           f.ev.salida, f.ev.origenes, f.ev.desde, f.ev.hasta, f.ev.horizonte );
   for ( i = 0; i < f.ev.nh; i++ )
      printf( "     h=%d  n=%d   MAE %.6f   RMSE %.6f   MAPE %.4f %%\n",
              f.ev.h[i].h, f.ev.h[i].n, f.ev.h[i].mae, f.ev.h[i].rmse,
              f.ev.h[i].mape );
   printf( "\n" );

   ok( f.tiene_ev, "hay evaluacion recursiva fuera de muestra" );
   ok( !strcmp( f.ev.salida, "ES_CPI" ), "  la salida, ES_CPI" );
   ok( f.ev.origenes == 31 && f.ev.desde == 180 && f.ev.hasta == 210,
       "  31 origenes, de la 180 a la 210" );
   ok( f.ev.nh == 6 && f.ev.horizonte == 6, "  seis horizontes" );

   /* The numbers of drtran since BUG-55 (2026-09-27): its forecasts use the
      exact residuals. This case is the airline model (MA x SMA, Theta = 0.81)
      first, whose conditional residuals, used before, were slightly off.
      Before: 0.182159 / 0.255043 / 0.2246 / 0.750253.                     */
   okf( f.ev.h[0].mae,  0.182023, 1e-6, "  h=1: MAE" );
   okf( f.ev.h[0].rmse, 0.255132, 1e-6, "  h=1: RMSE" );
   okf( f.ev.h[0].mape, 0.2244,   1e-4, "  h=1: MAPE" );
   okf( f.ev.h[5].rmse, 0.749936, 1e-6, "  h=6: RMSE" );

   /* El error crece con el horizonte, que es lo que tiene que pasar y lo que
    * confirma que las columnas se estan leyendo en su sitio.            */
   for ( i = 1; i < f.ev.nh; i++ )
      if ( f.ev.h[i].rmse <= f.ev.h[i - 1].rmse ) crece = 0;
   ok( crece, "  y el RMSE crece con el horizonte, como debe" );

   /* RMSE >= MAE siempre, por Jensen. Si saliera al reves, las dos columnas
    * estarian cambiadas.                                                */
   {
   int bien = 1;

   for ( i = 0; i < f.ev.nh; i++ )
      if ( f.ev.h[i].rmse < f.ev.h[i].mae ) bien = 0;
   ok( bien, "  y RMSE >= MAE en todos: las columnas no estan cambiadas" );
   }
}

/* --- la tabla en el formato de fuf ------------------------------------- */
/* La de forecast_table_acii (engines/fuf/src/usfo.c): sin «+---» al final,
 * acaba en una linea en blanco. of_tabla tiene que cortar ahi.          */
static void tabla_fuf( void )
{
   static const char *out =
      "algo antes\n"
      "FORECAST REPORT: \n"
      "VARIABLE NAME: IPC_ES\n"
      "FORECAST ORIGIN : 12/2025\n"
      "LEAD TIME FOR FORECASTING: 2\n"
      "\n"
      " +------+\n"
      " | DATE +\n"
      " +------+\n"
      " 11/2025 119.5320     -   0.1934      -    2.9908     -   0.0262 \n"
      " 12/2025 119.9420     -   0.3424      -    2.8825     -   0.2054 \n"
      "  1/2026 119.0716 0.3636 -0.7283  0.3636   1.9836 0.3636 \n"
      "  2/2026 119.2272 0.5923  0.1306  0.3782   1.7226 0.5923 \n"
      "\n"
      "lo que sigue\n";
   char     t[4096];
   Forecast g;

   printf( "\nla tabla en el formato de fuf\n\n" );
   ok( of_tabla( out, "IPC_ES", t, sizeof t ) == 0, "  se encuentra" );
   ok( !strncmp( t, "FORECAST REPORT", 15 ), "  empieza en «FORECAST REPORT»" );
   ok( !strstr( t, "lo que sigue" ) && strstr( t, "2/2026" ),
       "  y acaba en la linea en blanco, con la ultima fila dentro" );
   ok( of_tabla( out, "WTI", t, sizeof t ) == 1 && !t[0],
       "  una serie que no esta: 1 y nada" );
   ok( of_parse( out, &g ) == 0 && g.s[0].nf == 4 && g.s[0].nprev == 2,
       "  of_parse la lee: 4 filas, 2 de prevision" );
   ok( g.s[0].lead == 2 && !strcmp( g.s[0].origen, "12/2025" ),
       "  con el plazo de su linea propia y el origen" );

   /* Un .out de Windows: \r\n. El «-» del final de una fila prevista no
    * puede llevarse el \r y dejar de ser hueco.                      */
   {
   static const char *crlf =
      "FORECAST REPORT: \r\n"
      "VARIABLE NAME: IPC_ES\r\n"
      "FORECAST ORIGIN : 12/2025\r\n"
      "LEAD TIME FOR FORECASTING: 1\r\n"
      "  12/2025    119.9420        -    0.3424        -    2.8825        -    0.2054\r\n"
      "   1/2026    119.0716   0.3636   -0.7283   0.3636    1.9836   0.3636         -\r\n"
      " +-----+\r\n";

   ok( of_parse( crlf, &g ) == 0 && g.s[0].nf == 2 && g.s[0].nprev == 1
       && g.s[0].f[1].sd_nivel > 0.36,
       "  un .out de Windows (CRLF): la fila prevista no se pierde" );
   }

   /* Una serie anual: la fecha es el anio solo. */
   {
   static const char *anual =
      "FORECAST REPORT: \n"
      "VARIABLE NAME: PIB\n"
      "FORECAST ORIGIN : 2024\n"
      "LEAD TIME FOR FORECASTING: 1\n"
      "     2023    100.0000        -    1.0000        -         -        -    0.1000\n"
      "     2024    101.0000        -    1.0000        -         -        -    0.2000\n"
      "     2025    102.0000   1.5000    1.0000   1.5000         -        -         -\n"
      "\n";

   ok( of_parse( anual, &g ) == 0 && g.s[0].nf == 3 && g.s[0].nprev == 1
       && !strcmp( g.s[0].f[2].fecha, "2025" ),
       "  una serie anual: la fecha es el anio solo" );
   }
}

int main( int argc, char **argv )
{
   if ( argc < 2 ) {
      fprintf( stderr, "uso: test_outfcst <.out con -f> [<.out con -estwin -C>]\n" );
      return 2;
   }

   tabla_fuf();
   prevision( argv[1] );
   if ( argc >= 3 ) evaluacion( argv[2] );

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
