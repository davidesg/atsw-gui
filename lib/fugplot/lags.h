/*
 * lags.h -- cuantos retardos tienen la ACF y la PACF: la regla del motor (la
 * misma para el .out, los graficos y las ventanas). Solo cabecera, y sin
 * nada de fugplot, para que la usen tambien los que no dibujan
 * (lib/anomalos/residuos.c, y con el atsw-cli). default_lags
 * (plotsupport.c) es esta.
 */
#ifndef ATSW_LAGS_H
#define ATSW_LAGS_H

/* Como mucho nobs - 2 retardos, y al menos 1. */
static inline int fp_lags( int nobs, int freq )
{
   int lags;

   if ( nobs < 3 * (freq + 1) )
      lags = nobs - freq / 2;
   else if ( (freq == 1) && (nobs > 200) )
      lags = 45;
   else if ( freq == 1 )
      lags = 9;
   else
      lags = 3 * (freq + 1);
   if ( lags > nobs - 2 ) lags = nobs - 2;
   if ( lags < 1 ) lags = 1;
   return lags;
}

#endif /* ATSW_LAGS_H */
