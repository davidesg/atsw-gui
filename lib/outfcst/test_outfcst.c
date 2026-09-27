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

int main( int argc, char **argv )
{
   if ( argc < 2 ) {
      fprintf( stderr, "uso: test_outfcst <.out con -f> [<.out con -estwin -C>]\n" );
      return 2;
   }

   prevision( argv[1] );
   if ( argc >= 3 ) evaluacion( argv[2] );

   printf( "\n%d fallos\n", fallos );
   return fallos ? 1 : 0;
}
