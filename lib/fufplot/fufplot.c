/***************************************************************************
 *   fufplot.c -- the forecast graph of FUF, drawn with fugdraw (no        *
 *   gnuplot). Copyright (C) 2026 by A.B. Treadway & D.E. Guerrero.        *
 *                                                                         *
 *   The design is the one of the gnuplot graph of FUF 1.08: the same two  *
 *   panels, the same marks and the same bands. The sizes are those of the *
 *   EPS that gnuplot wrote (it halved them), so the graph goes on the     *
 *   page just as it did.                                                  *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fugdraw.h"
#include "fufplot.h"

#define W        324.0            /* the canvas, as the EPS of gnuplot      */
#define H        453.0
#define MARGIN_L  38.0
#define MARGIN_R   6.0

#define SZ_TITLE  16.0            /* gnuplot 'bold,32', halved              */
#define SZ_TICK   12.0            /* gnuplot 24                             */

#define LW_AXIS    0.80           /* gnuplot lw 1.6                         */
#define LW_GRID    0.50
#define LW_LINE    0.80           /* the series, solid                      */
#define LW_BAND    0.75           /* the bands, dashed                      */
#define LW_IMPULSE 1.40           /* gnuplot ls 5 lw 9, halved and thinned  */
#define TIC        3.0
/* The observed part carries a larger point than the forecast, which is what
 * tells the two apart in the published report; the line is solid in both.  */
#define DOT_OBS    2.35
#define DOT_FOR    2.00

static double mapv( double v, double v0, double v1, double p0, double p1 )
{
   if ( v1 == v0 ) return( (p0 + p1) / 2.0 );
   return( p0 + (v - v0) * (p1 - p0) / (v1 - v0) );
}

/* A step for the ticks of an axis that spans range: 1, 2, 2.5 or 5 times a
 * power of ten, so that about n divisions fit.                             */
static double nice_step( double range, int n )
{
   double raw, power, step;

   if ( !(range > 0.0) || n < 1 ) return( 1.0 );
   raw   = range / n;
   power = pow( 10.0, floor( log10( raw ) ) );
   step  = raw / power;
   if ( step <= 1.0 ) step = 1.0;
   else if ( step <= 2.0 ) step = 2.0;
   else if ( step <= 2.5 ) step = 2.5;
   else if ( step <= 5.0 ) step = 5.0;
   else step = 10.0;
   return( step * power );
}

/* The label of a tick, with the decimals its step needs and without the
 * zeros that say nothing (2.50 -> 2.5, -0.0 -> 0), as gnuplot wrote them.  */
static void tick_label( char *text, size_t size, double v, double step )
{
   int    decimals = 0;
   double s = step;
   char  *p;

   if ( v == 0.0 ) v = 0.0;                 /* a negative zero is a zero     */
   while ( decimals < 6 && fabs( s - floor( s + 0.5 ) ) > 1e-9 ) { s *= 10.0; decimals++; }
   snprintf( text, size, "%.*f", decimals, v );
   if ( strchr( text, '.' ) != NULL )       /* 2.50 -> 2.5, 2.0 -> 2         */
      {
      for ( p = text + strlen( text ) - 1; p > text && *p == '0'; p-- ) *p = '\0';
      if ( *p == '.' ) *p = '\0';
      }
   if ( strcmp( text, "-0" ) == 0 ) snprintf( text, size, "0" );
}

/* One year every how many, so that the labels do not run into one another:
 * never fewer than the graph of gnuplot had.                               */
static int year_step( double x0, double x1, int n, int freq, int every )
{
   double w = fd_text_width( FD_HELV, SZ_TICK, "0000" ) * 1.15;

   if ( n < 2 || freq < 1 ) return( every );
   while ( every < 100 &&
           ( x1 - x0 ) * every * freq / ( n - 1 ) < w ) every++;
   return( every );
}

/* The vertical lines and the labels of the years: one for each first season
 * of a year, exactly as the graph of gnuplot placed them.                  */
static void year_axis( FDFig *f, double x0, double x1, double y0, double y1,
                       int n, int freq, int first_year, int first_season, int every )
{
   char   text[32];
   double x;
   int    i, year = first_year, season = first_season;

   for ( i = 0; i < n; i++ )
       {
       if ( season == 1 && ( (year - first_year) % every ) == 0 )
          {
          x = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
          fd_gray( f, 0.0 );
          fd_linewidth( f, LW_GRID );
          fd_dash( f, 0.0, 0.0 );
          fd_line( f, x, y0, x, y1 );
          snprintf( text, sizeof( text ), "%d", year );
          fd_text( f, x, y0 - SZ_TICK - 3.0, FD_HELV, SZ_TICK, FD_CENTER, text );
          }
       if ( ++season > freq ) { season = 1; year++; }
       }
}

/* The frame of a panel: the axis at the left and at the bottom, and the
 * ticks of the vertical axis with their labels.                           */
static void yaxis( FDFig *f, double x0, double x1, double y0, double y1,
                   double v0, double v1, double step )
{
   char   text[32];
   double v, y, start;

   fd_gray( f, 0.0 );
   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_AXIS );
   fd_line( f, x0, y0, x0, y1 );
   fd_line( f, x0, y0, x1, y0 );

   start = ceil( v0 / step - 1e-9 ) * step;
   for ( v = start; v <= v1 + 1e-9 * fabs( v1 ); v += step )
       {
       y = mapv( v, v0, v1, y0, y1 );
       fd_line( f, x0, y, x0 - TIC, y );
       tick_label( text, sizeof( text ), v, step );
       fd_text( f, x0 - TIC - 2.0, y - 0.36 * SZ_TICK, FD_HELV, SZ_TICK, FD_RIGHT, text );
       }
}

/*****************************************************************************/

FDFig *fp_forecast( const double *y, const double *band, const double *band2,
                    const double *err, int L, double sigma, int freq,
                    int first_year, int first_season, const char *title )
{
   FDFig *f;
   double x0, x1, y0, y1, v0, v1, step, cmax, x, ylow, yhigh;
   double px[4096], py[4096];
   int    i, n = 2 * L, every;

   if ( L < 1 || n > 4096 ) return( NULL );
   if ( ( f = fd_fig_new( W, H ) ) == NULL ) return( NULL );

   x0 = MARGIN_L;
   x1 = W - MARGIN_R;
   every = ( freq == 12 ) ? 1 : ( freq == 4 ) ? 2 : 10;   /* as in FUF 1.08 */

/* [1]: the panel of the forecasts                                           */

   y0 = H - 200.0;
   y1 = H - 30.0;

   v0 = v1 = y[0];
   for ( i = 0; i < n; i++ )
       {
       if ( y[i] < v0 ) v0 = y[i];
       if ( y[i] > v1 ) v1 = y[i];
       if ( i >= L )
          {
          if ( band2[i] < v0 ) v0 = band2[i];
          if ( band[i] > v1 ) v1 = band[i];
          }
       }
   step = nice_step( v1 - v0, 5 );
   v0 = floor( v0 / step ) * step;                  /* whole ticks           */
   v1 = ceil( v1 / step ) * step;

   fd_text( f, x0 - 20.0, y1 + 14.0, FD_HELV_BOLD, SZ_TITLE, FD_LEFT, title );
   year_axis( f, x0, x1, y0, y1, n, freq, first_year, first_season,
              year_step( x0, x1, n, freq, every ) );
   yaxis( f, x0, x1, y0, y1, v0, v1, step );

   /* the bands of the forecast, dashed */
   fd_linewidth( f, LW_BAND );
   fd_dash( f, 1.6, 2.2 );
   for ( i = L; i < n; i++ )
       {
       px[i - L] = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
       py[i - L] = mapv( band[i], v0, v1, y0, y1 );
       }
   fd_polyline( f, px, py, L );
   for ( i = L; i < n; i++ ) py[i - L] = mapv( band2[i], v0, v1, y0, y1 );
   fd_polyline( f, px, py, L );

   /* the series, observed and forecast: one solid line, and a point at each
    * observation -- larger in the observed part than in the forecast       */
   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_LINE );
   for ( i = 0; i < n; i++ )
       {
       px[i] = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
       py[i] = mapv( y[i], v0, v1, y0, y1 );
       }
   fd_polyline( f, px, py, n );
   for ( i = 0; i < n; i++ )
       fd_disc( f, px[i], py[i], ( i < L ) ? DOT_OBS : DOT_FOR );

/* [2]: the panel of the errors, under the observations it belongs to        */

   y0 = 40.0;
   y1 = 195.0;
   /* The error of a month goes UNDER its realisation: the panel keeps the
    * scale of the one above (the same date on the same x) and stops at the
    * origin of the forecast, so it is narrower. That is how the published
    * report has it, and it is what lets the analyst read the errors the
    * model is making against the series.                                   */
   x1 = x0 + ( x1 - x0 ) * ( L - 1 ) / (double)( n - 1 );

   cmax = 4.0 * sigma;
   for ( i = 0; i < L; i++ ) if ( fabs( err[i] ) > cmax ) cmax = fabs( err[i] );
   if ( cmax > 4.0 * sigma && cmax <= 6.0 * sigma ) cmax = 6.0 * sigma;
   else if ( cmax > 6.0 * sigma && cmax <= 7.0 * sigma ) cmax = 7.0 * sigma;
   else if ( cmax > 7.0 * sigma ) cmax = 10.0 * sigma;
   if ( !(cmax > 0.0) ) cmax = 1.0;
   cmax += 0.10 * sigma;
   /* gnuplot was given the range with one decimal ("%1.1f"), so the ticks
    * fell on round values: the same rounding keeps the graph as it was.    */
   if ( floor( cmax * 10.0 + 0.5 ) > 0.0 ) cmax = floor( cmax * 10.0 + 0.5 ) / 10.0;

   fd_text( f, x0 - 20.0, y1 + 14.0, FD_HELV_BOLD, SZ_TITLE, FD_LEFT, "ERR" );
   year_axis( f, x0, x1, y0, y1, L, freq, first_year, first_season,
              year_step( x0, x1, L, freq, every ) );
   /* the ticks every two standard deviations, rounded to one decimal as
    * gnuplot rounded them ("set ytics %.1f")                                */
   step = floor( 2.0 * sigma * 10.0 + 0.5 ) / 10.0;
   if ( !(step > 0.0) ) step = nice_step( 2.0 * cmax, 4 );
   yaxis( f, x0, x1, y0, y1, -cmax, cmax, step );

   /* the zero line and the band of two standard deviations */
   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_AXIS );
   fd_dash( f, 0.0, 0.0 );
   ylow = mapv( 0.0, -cmax, cmax, y0, y1 );
   fd_line( f, x0, ylow, x1, ylow );
   if ( sigma > 0.0 )
      {
      /* the band, also with the one decimal that gnuplot was given */
      double band_v = floor( 2.0 * sigma * 10.0 + 0.5 ) / 10.0;

      if ( !(band_v > 0.0) ) band_v = 2.0 * sigma;
      fd_linewidth( f, LW_BAND );
      fd_dash( f, 3.0, 3.0 );
      yhigh = mapv( band_v, -cmax, cmax, y0, y1 );
      fd_line( f, x0, yhigh, x1, yhigh );
      yhigh = mapv( -band_v, -cmax, cmax, y0, y1 );
      fd_line( f, x0, yhigh, x1, yhigh );
      }

   /* the residuals, as impulses */
   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_IMPULSE );
   for ( i = 0; i < L; i++ )
       {
       x = mapv( (double)i, 0.0, (double)(L - 1), x0, x1 );
       fd_line( f, x, ylow, x, mapv( err[i], -cmax, cmax, y0, y1 ) );
       }

   return( f );
}

/*****************************************************************************/
/* The impulse and the step response of a transfer, in the style of the
 * forecast graph above: the same canvas, fonts, axes and dashes.           */

/* The horizontal axis of the lags: a tick at every k that fits, labelled. */
static void k_axis( FDFig *f, double x0, double x1, double y0, int n )
{
   char   text[32];
   double x, w = fd_text_width( FD_HELV, SZ_TICK, "00" ) * 1.6;
   int    k, every = 1;

   while ( n > 1 && every < n && ( x1 - x0 ) * every / ( n - 1 ) < w ) every++;
   fd_gray( f, 0.0 );
   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_AXIS );
   for ( k = 0; k < n; k += every )
       {
       x = mapv( (double)k, 0.0, (double)( n > 1 ? n - 1 : 1 ), x0, x1 );
       fd_line( f, x, y0, x, y0 - TIC );
       snprintf( text, sizeof( text ), "%d", k );
       fd_text( f, x, y0 - SZ_TICK - 3.0, FD_HELV, SZ_TICK, FD_CENTER, text );
       }
}

/* The range of a panel: the values and their bands, with whole ticks. */
static void range( const double *v, const double *sd, int n, double nsd,
                   double extra, int has_extra, double *v0, double *v1, double *step )
{
   int i;

   *v0 = *v1 = 0.0;                     /* zero is always in the panel     */
   for ( i = 0; i < n; i++ )
       {
       double e  = sd ? nsd * sd[i] : 0.0;
       double lo = v[i] - e, hi = v[i] + e;

       if ( lo < *v0 ) *v0 = lo;
       if ( hi > *v1 ) *v1 = hi;
       }
   if ( has_extra ) { if ( extra < *v0 ) *v0 = extra; if ( extra > *v1 ) *v1 = extra; }
   if ( !( *v1 > *v0 ) ) *v1 = *v0 + 1.0;
   *step = nice_step( *v1 - *v0, 5 );
   *v0 = floor( *v0 / *step ) * *step;
   *v1 = ceil( *v1 / *step ) * *step;
}

FDFig *fp_response( const double *nu, const double *nu_sd,
                    const double *cum, const double *cum_sd, int n,
                    double gain, int has_gain,
                    const char *title_irf, const char *title_srf,
                    const char *note )
{
   FDFig *f;
   double x0, x1, y0, y1, v0, v1, step, x, yz, px[4096], py[4096];
   int    i;

   if ( n < 2 || n > 4096 ) return( NULL );
   if ( ( f = fd_fig_new( W, H ) ) == NULL ) return( NULL );
   x0 = MARGIN_L;
   x1 = W - MARGIN_R;

/* [1]: the impulse response, as impulses, with the band of 2 sd             */

   y0 = H - 200.0;
   y1 = H - 30.0;
   range( nu, nu_sd, n, 2.0, 0.0, 0, &v0, &v1, &step );
   fd_text( f, x0 - 20.0, y1 + 14.0, FD_HELV_BOLD, SZ_TITLE, FD_LEFT, title_irf );
   yaxis( f, x0, x1, y0, y1, v0, v1, step );
   k_axis( f, x0, x1, y0, n );

   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_AXIS );
   fd_dash( f, 0.0, 0.0 );
   yz = mapv( 0.0, v0, v1, y0, y1 );
   fd_line( f, x0, yz, x1, yz );

   /* the band of each nu_k on its own: the lags are discrete, and a line
    * joining them would draw a band between two lags where there is none */
   fd_linewidth( f, LW_BAND );
   for ( i = 0; nu_sd && i < n; i++ )
       {
       double lo, hi, cap = 3.0;

       if ( !( nu_sd[i] > 0.0 ) ) continue;
       x  = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
       lo = mapv( nu[i] - 2.0 * nu_sd[i], v0, v1, y0, y1 );
       hi = mapv( nu[i] + 2.0 * nu_sd[i], v0, v1, y0, y1 );
       fd_dash( f, 1.6, 2.2 );
       fd_line( f, x + 2.5, lo, x + 2.5, hi );
       fd_dash( f, 0.0, 0.0 );
       fd_line( f, x + 2.5 - cap, lo, x + 2.5 + cap, lo );
       fd_line( f, x + 2.5 - cap, hi, x + 2.5 + cap, hi );
       }

   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_IMPULSE );
   for ( i = 0; i < n; i++ )
       {
       x = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
       fd_line( f, x, yz, x, mapv( nu[i], v0, v1, y0, y1 ) );
       }

/* [2]: the step response (the cumulative), with its band and the gain       */

   y0 = 40.0;
   y1 = 195.0;
   range( cum, cum_sd, n, 2.0, gain, has_gain, &v0, &v1, &step );
   fd_text( f, x0 - 20.0, y1 + 14.0, FD_HELV_BOLD, SZ_TITLE, FD_LEFT, title_srf );
   yaxis( f, x0, x1, y0, y1, v0, v1, step );
   k_axis( f, x0, x1, y0, n );

   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_AXIS );
   fd_dash( f, 0.0, 0.0 );
   yz = mapv( 0.0, v0, v1, y0, y1 );
   fd_line( f, x0, yz, x1, yz );
   if ( has_gain )                      /* where the step response goes     */
      {
      fd_linewidth( f, LW_BAND );
      fd_dash( f, 3.0, 3.0 );
      yz = mapv( gain, v0, v1, y0, y1 );
      fd_line( f, x0, yz, x1, yz );
      }

   for ( i = 0; i < n; i++ ) px[i] = mapv( (double)i, 0.0, (double)(n - 1), x0, x1 );
   if ( cum_sd )
      {
      fd_linewidth( f, LW_BAND );
      fd_dash( f, 1.6, 2.2 );
      for ( i = 0; i < n; i++ ) py[i] = mapv( cum[i] + 2.0 * cum_sd[i], v0, v1, y0, y1 );
      fd_polyline( f, px, py, n );
      for ( i = 0; i < n; i++ ) py[i] = mapv( cum[i] - 2.0 * cum_sd[i], v0, v1, y0, y1 );
      fd_polyline( f, px, py, n );
      }

   fd_dash( f, 0.0, 0.0 );
   fd_linewidth( f, LW_LINE );
   for ( i = 0; i < n; i++ ) py[i] = mapv( cum[i], v0, v1, y0, y1 );
   fd_polyline( f, px, py, n );
   for ( i = 0; i < n; i++ ) fd_disc( f, px[i], py[i], DOT_FOR );

   /* the note, under the panels: what a graph cut out of its window has to
    * keep saying (here, why there are no bands)                           */
   if ( note && *note )
      {
      /* one line per '\n', the last at the bottom                         */
      char        line[256];
      const char *p = note, *e;
      int         nl = 1, k = 0;

      for ( e = note; *e; e++ ) if ( *e == '\n' ) nl++;
      while ( p && *p && k < nl )
          {
          size_t len;

          e = strchr( p, '\n' );
          len = e ? (size_t)( e - p ) : strlen( p );
          if ( len >= sizeof( line ) ) len = sizeof( line ) - 1;
          memcpy( line, p, len );
          line[len] = '\0';
          fd_text( f, x0 - 20.0, 4.0 + 10.0 * ( nl - 1 - k ), FD_HELV, SZ_TICK * 0.75,
                   FD_LEFT, line );
          p = e ? e + 1 : NULL;
          k++;
          }
      }

   return( f );
}
