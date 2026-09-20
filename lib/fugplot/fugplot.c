/***************************************************************************
 *   fugplot.c -- FUG graphs drawn with fugdraw (no gnuplot).               *
 *   Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

/* The design reproduces the graphs of FUG 1.13 with gnuplot: the same
 * canvases, panels, fonts, line widths and scales. Sizes are in points.
 *   -a, -c   series (and acf/pacf): 720 x 252 for monthly or long series,
 *            468 x 219 otherwise
 *   -b       acf/pacf: 234, 152 or 102 x 252 (depends on the number of lags)
 *   -d       histogram: 270 x 189
 *   -e       mean - standard deviation: 360 x 252                          */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "plothost.h"
#include "fugplot.h"

typedef struct {
   double W, H;                   /* canvas                                   */
   double tick;                   /* tick labels                              */
   double year;                   /* year labels                              */
   double title;                  /* series name (bold)                       */
   double q;                      /* Ljung-Box statistic                      */
   double stat, statw, statsym, statsub;   /* mean / std. dev. label        */
   double point;                  /* radius of the points                     */
} Layout;

/* gnuplot sizes are halved in its EPS files: 'Helvetica,17.2' -> 8.6 pt */
static const Layout layout_big   = { 720.0, 252.0, 8.0, 8.6, 12.0, 8.5, 9.5, 12.0, 12.0, 9.0, 1.75 };
static const Layout layout_small = { 468.0, 219.0, 7.5, 8.6, 11.0, 7.5, 8.5, 10.5, 11.0, 9.0, 1.55 };

#define LW_AXIS   0.40            /* gnuplot lw 1.6                            */
#define LW_THIN   0.25            /* lw 1                                      */
#define LW_BAND   0.375           /* lw 1.5 dashed                             */
#define LW_ZERO   0.70            /* lw 2.8 (zero line of the acf)             */
#define LW_CBAND  0.30            /* lw 1.2 dashed (bands of the acf)          */
#define TIC_MAJOR 3.5
#define TIC_MINOR 1.75
#define TIC_X     2.0

static double mapv( double v, double v0, double v1, double p0, double p1 )
{
   return( p0 + (v - v0) / (v1 - v0) * (p1 - p0) );
}

/* "0.30" -> "0.3", "0.00" -> "0", "-0" -> "0" */
static void number( char *s, size_t size, double v )
{
   char *p;

   snprintf( s, size, "%.2f", v );
   p = s + strlen( s ) - 1;
   while ( *p == '0' ) *p-- = '\0';
   if ( *p == '.' ) *p = '\0';
   if ( strcmp( s, "-0" ) == 0 ) strcpy( s, "0" );
}

/* Baseline that centers digits of the given size on y */
static double mid( double y, double size )
{
   return( y - 0.359 * size );
}

/* Scale of the standardized series: 4, 6, 8, 10 or 12 */
static double SeriesMax( double *a, int n )
{
   double AbsMax = 4;
   int    i;

   for ( i = 1; i <= n; i++ ) if ( fabs( a[i] ) >= AbsMax ) AbsMax = fabs( a[i] );
   if ( AbsMax > 10 ) return( 12 );
   if ( AbsMax > 8 ) return( 10 );
   if ( AbsMax > 6 ) return( 8 );
   if ( AbsMax > 4 ) return( 6 );
   return( 4 );
}

/* 2: wide canvas (monthly or more than 200 observations), 1: otherwise */
static int SeriesSize( int nobs, int freq )
{
   return( (freq > 4 || nobs > 200) ? 2 : 1 );
}

/* Scale of the acf/pacf plots: 0.6, 0.8 or 1 (at least 0.4) */
static double Acf_Pacf_Max( struct Tseries *ser, int lags )
{
   double cmax = 0.40, *corr = vector( 1, lags );
   int    i;

   Acf( ser, lags, corr );
   for ( i = 1; i <= lags; i++ ) if ( fabs( corr[i] ) >= cmax ) cmax = fabs( corr[i] );
   Pacf( lags, corr );
   for ( i = 1; i <= lags; i++ ) if ( fabs( corr[i] ) >= cmax ) cmax = fabs( corr[i] );
   free_vector( corr, 1, lags );
   if ( cmax > 0.80 ) return( 1.0 );
   if ( cmax > 0.60 ) return( 0.80 );
   if ( cmax > 0.40 ) return( 0.60 );
   return( cmax );
}

/* Write the figure as <prefix><x11out>.eps.
 *
 * The prefix goes on the BASE NAME, not in front of the whole path: with an
 * x11out like "/tmp/mtram/res", "hist_" in front would give "hist_/tmp/...".
 * For a plain name -- the way fue and fug use it -- nothing changes.       */
static void save_eps( FDFig *f, const char *prefix, const char *x11out )
{
   const char *s1 = strrchr( x11out, '/' ), *s2 = strrchr( x11out, '\\' );
   const char *base = ( s1 > s2 ) ? s1 : s2;
   int         dir  = base ? (int)( base + 1 - x11out ) : 0;
   char       *file = (char *)malloc( strlen( prefix ) + strlen( x11out ) + 5 );

   sprintf( file, "%.*s%s%s.eps", dir, x11out, prefix, x11out + dir );
   if ( fd_write_eps( f, file ) != 0 )
      fprintf( stderr, "Warning: can not write %s\n", file );
   free( file );
}

/* Standardized series in a[1..n] (sd > 0) */
static double *standardize( struct Tseries *ser )
{
   double *a = vector( 1, ser->nobs ), sd = sqrt( ser->var );
   int     i;

   for ( i = 1; i <= ser->nobs; i++ ) a[i] = (ser->data[i] - ser->mean) / sd;
   return( a );
}

/*****************************************************************************/
/* Standardized series                                                       */
/*****************************************************************************/

static void series_panel( FDFig *f, const Layout *L, double x0, double x1, double y0, double y1,
                          const double *a, int n, int freq, int tsnobs, int tmornsop,
                          int tsby, double AbsMax )
{
   double xmin = -tmornsop, xmax = n - 1, px, py, *xs, *ys;
   int    i, step, count, v, label;
   char   s[32];

   /* vertical lines and year labels: every 2 years from the first year for
    * seasonal data; for annual data at the multiples of 5 years (as
    * GraphMaker), or of 10, 20, 25... if the labels do not fit              */
   fd_linewidth( f, LW_THIN );
   if ( freq > 1 )
      {
      step  = 2 * freq;
      count = (tsnobs - 1) / step;
      for ( i = 0; i <= count; i++ )
          {
          double X = xmin + step * i;
          if ( X > xmax ) break;
          px = mapv( X, xmin, xmax, x0, x1 );
          if ( i > 0 ) fd_line( f, px, y0, px, y1 );
          snprintf( s, sizeof( s ), "%d", tsby + 2 * i );
          fd_text( f, px, y0 - TIC_X - 2.5 - 0.718 * L->year, FD_HELV, L->year, FD_CENTER, s );
          }
      }
   else
      {
      count = (int)(xmax - xmin);                  /* years on the axis */
      step  = annual_step( count, x1 - x0 );
      for ( label = first_multiple( tsby, step ); label <= tsby + count; label += step )
          {
          double X = xmin + (label - tsby);
          px = mapv( X, xmin, xmax, x0, x1 );
          if ( X > xmin ) fd_line( f, px, y0, px, y1 );
          snprintf( s, sizeof( s ), "%d", label );
          fd_text( f, px, y0 - TIC_X - 2.5 - 0.718 * L->year, FD_HELV, L->year, FD_CENTER, s );
          }
      }

   /* zero line and bands of +-2 */
   fd_line( f, x0, mapv( 0, -AbsMax, AbsMax, y0, y1 ), x1, mapv( 0, -AbsMax, AbsMax, y0, y1 ) );
   fd_linewidth( f, LW_BAND );
   fd_dash( f, 2.5, 2.5 );
   for ( v = -2; v <= 2; v += 4 )
       {
       py = mapv( v, -AbsMax, AbsMax, y0, y1 );
       fd_line( f, x0, py, x1, py );
       }
   fd_dash( f, 0, 0 );

   /* axes (left and bottom), tics outside */
   fd_linewidth( f, LW_AXIS );
   fd_line( f, x0, y0, x0, y1 );
   fd_line( f, x0, y0, x1, y0 );
   for ( i = (int)ceil( xmin ); i <= (int)xmax; i++ )
       {
       px = mapv( i, xmin, xmax, x0, x1 );
       fd_line( f, px, y0, px, y0 - TIC_X );
       }
   for ( v = -(int)AbsMax; v <= (int)AbsMax; v++ )
       {
       py = mapv( v, -AbsMax, AbsMax, y0, y1 );
       if ( v % 2 == 0 )
          {
          fd_line( f, x0, py, x0 - TIC_MAJOR, py );
          snprintf( s, sizeof( s ), "%d", v );
          fd_text( f, x0 - TIC_MAJOR - 1.5, mid( py, L->tick ), FD_HELV, L->tick, FD_RIGHT, s );
          }
       else
          fd_line( f, x0, py, x0 - TIC_MINOR, py );
       }

   /* data: x = 0..n-1 */
   xs = (double *)malloc( n * sizeof( double ) );
   ys = (double *)malloc( n * sizeof( double ) );
   for ( i = 0; i < n; i++ )
       {
       xs[i] = mapv( i, xmin, xmax, x0, x1 );
       ys[i] = mapv( a[i+1], -AbsMax, AbsMax, y0, y1 );
       }
   fd_clip_begin( f, x0 - L->point - 0.5, y0, x1 - x0 + 2 * L->point + 1, y1 - y0 );
   fd_linewidth( f, LW_THIN );
   fd_polyline( f, xs, ys, n );
   for ( i = 0; i < n; i++ ) fd_disc( f, xs[i], ys[i], L->point );
   fd_clip_end( f );
   free( xs );
   free( ys );
}

/*****************************************************************************/
/* acf / pacf                                                                */
/*****************************************************************************/

static void corr_panel( FDFig *f, const Layout *L, double x0, double x1, double y0, double y1,
                        const double *c, int lags, int freq, int nobs, double cmax,
                        const char *title )
{
   double px, py, pz, band = 2.0 / sqrt( (double)nobs ), bw, v;
   int    i, k, marks[3], nmarks = 0, gap;
   char   s[32];

   /* seasonal lags: vertical lines and labels */
   if ( freq == 1 && lags > 9 )
      {
      gap = iround( lags / 3 );
      for ( k = 1; k <= 3; k++ ) marks[nmarks++] = k * gap;
      }
   else if ( freq == 1 )
      {
      marks[nmarks++] = 3;
      marks[nmarks++] = 6;
      if ( lags == 9 ) marks[nmarks++] = 9;
      }
   else
      for ( k = 1; k <= 3; k++ )
          if ( lags >= k * freq ) marks[nmarks++] = k * freq;

   fd_linewidth( f, LW_THIN );
   for ( k = 0; k < nmarks; k++ )
       {
       px = mapv( marks[k], 0, lags, x0, x1 );
       fd_line( f, px, y0, px, y1 );
       snprintf( s, sizeof( s ), "%d", marks[k] );
       fd_text( f, px, y0 - 3.0 - 0.718 * L->tick, FD_HELV, L->tick, FD_CENTER, s );
       }

   /* bands +-2/sqrt(n) and zero line */
   if ( band < cmax )
      {
      fd_linewidth( f, LW_CBAND );
      fd_dash( f, 2.0, 2.0 );
      for ( k = -1; k <= 1; k += 2 )
          {
          py = mapv( k * band, -cmax, cmax, y0, y1 );
          fd_line( f, x0, py, x1, py );
          }
      fd_dash( f, 0, 0 );
      }
   pz = mapv( 0, -cmax, cmax, y0, y1 );
   fd_linewidth( f, LW_ZERO );
   fd_line( f, x0, pz, x1, pz );

   /* coefficients as bars (butt ends) */
   bw = (lags >= 30) ? 1.75 : 2.25;
   if ( bw > 0.6 * (x1 - x0) / lags ) bw = 0.6 * (x1 - x0) / lags;
   fd_linewidth( f, bw );
   for ( i = 1; i <= lags; i++ )
       {
       v = c[i];
       if ( v > cmax ) v = cmax;
       if ( v < -cmax ) v = -cmax;
       px = mapv( i, 0, lags, x0, x1 );
       fd_line( f, px, pz, px, mapv( v, -cmax, cmax, y0, y1 ) );
       }

   /* left axis with 5 tics */
   fd_linewidth( f, LW_AXIS );
   fd_line( f, x0, y0, x0, y1 );
   for ( k = -2; k <= 2; k++ )
       {
       py = mapv( k * cmax / 2.0, -cmax, cmax, y0, y1 );
       fd_line( f, x0, py, x0 - TIC_MAJOR, py );
       number( s, sizeof( s ), k * cmax / 2.0 );
       fd_text( f, x0 - TIC_MAJOR - 1.5, mid( py, L->tick ), FD_HELV, L->tick, FD_RIGHT, s );
       }

   fd_text( f, (x0 + x1) / 2.0, y1 + 5.0, FD_HELV, (lags >= 30) ? 10.0 : 9.0, FD_CENTER, title );
}

/*****************************************************************************/
/* Title and statistics                                                      */
/*****************************************************************************/

/* nabla^d nabla_s^D ln name(lambda) */
static void title( FDFig *f, double sz, double x, double y, int align, int nrdiff,
                   int nadiff, int freq, double boxlam, const char *name )
{
   FDRun  r[8];
   char   t[8][64];
   int    n = 0;
   double small = 0.62 * sz;

   if ( nrdiff > 0 )
      {
      r[n] = (FDRun){ FD_SYMBOL, sz, 0.0, FD_SYM_NABLA, FD_ACC_NONE }; n++;
      if ( nrdiff > 1 )
         {
         snprintf( t[n], 64, "%d", nrdiff );
         r[n] = (FDRun){ FD_HELV_BOLD, small, 0.45 * sz, t[n], FD_ACC_NONE }; n++;
         }
      }
   if ( nadiff > 0 && freq > 1 )
      {
      r[n] = (FDRun){ FD_SYMBOL, sz, 0.0, FD_SYM_NABLA, FD_ACC_NONE }; n++;
      snprintf( t[n], 64, "%d", freq );
      r[n] = (FDRun){ FD_HELV_BOLD, small, -0.25 * sz, t[n], FD_ACC_NONE }; n++;
      if ( nadiff > 1 )
         {
         snprintf( t[n], 64, "%d", nadiff );
         r[n] = (FDRun){ FD_HELV_BOLD, small, 0.45 * sz, t[n], FD_ACC_NONE }; n++;
         }
      }
   snprintf( t[n], 64, "%s%s", (boxlam == 0) ? "ln" : "", name );
   r[n] = (FDRun){ FD_HELV_BOLD, sz, 0.0, t[n], FD_ACC_NONE }; n++;
   if ( boxlam != 0 && boxlam != 1 )
      {
      snprintf( t[n], 64, "(%.2f)", boxlam );
      r[n] = (FDRun){ FD_HELV_BOLD, sz, 0.0, t[n], FD_ACC_NONE }; n++;
      }
   fd_runs( f, x, y, align, r, n );
}

/* w-bar ( sigma-hat_w-bar ) = m% (se%)      sigma-hat_w = sd%   */
static void statistics( FDFig *f, const Layout *L, double xc, double y,
                        double mean, double sd, int nobs )
{
   char   t1[64], t2[64];
   FDRun  r1[5], r2[3];
   double w1, w2, gap = 2.5 * L->stat;

   snprintf( t1, sizeof( t1 ), " ) = %.2f%% (%.2f%%)", 100 * mean, 100 * sd / sqrt( (double)nobs ) );
   snprintf( t2, sizeof( t2 ), " = %.2f%%", 100 * sd );
   r1[0] = (FDRun){ FD_TIMES,  L->statw,   0.0, "w",  FD_ACC_BAR };
   r1[1] = (FDRun){ FD_HELV,   L->stat,    0.0, " ( ", FD_ACC_NONE };
   r1[2] = (FDRun){ FD_SYMBOL, L->statsym, 0.0, FD_SYM_SIGMA, FD_ACC_HAT };
   r1[3] = (FDRun){ FD_TIMES,  L->statsub, -0.3 * L->statsub, "w", FD_ACC_BAR };
   r1[4] = (FDRun){ FD_HELV,   L->stat,    0.0, t1,   FD_ACC_NONE };
   r2[0] = (FDRun){ FD_SYMBOL, L->statsym, 0.0, FD_SYM_SIGMA, FD_ACC_HAT };
   r2[1] = (FDRun){ FD_TIMES,  L->statsub, -0.3 * L->statsub, "w", FD_ACC_NONE };
   r2[2] = (FDRun){ FD_HELV,   L->stat,    0.0, t2,   FD_ACC_NONE };
   w1 = fd_runs_width( r1, 5 );
   w2 = fd_runs_width( r2, 3 );
   fd_runs( f, xc - (w1 + gap + w2) / 2.0, y, FD_LEFT, r1, 5 );
   fd_runs( f, xc - (w1 + gap + w2) / 2.0 + w1 + gap, y, FD_LEFT, r2, 3 );
}

/*****************************************************************************/
/* Graph -c                                                                  */
/*****************************************************************************/

FDFig *fp_PlotSer_CorrSer( struct Tseries *ser, int npar, int tsnobs, int tmornsop,
                           int tsby, double boxlam, int nrdiff, int nadiff, int lags,
                           double cbands, const char *x11out, const char *name )
{
   const Layout *L;
   FDFig  *f;
   double *a, *acf, *pacf, AbsMax, cmax, W, H, sx0, sx1, cx0, cx1, maxw;
   int     i, n = ser->nobs, freq = ser->freq;
   char    s[32];

   if ( ser->var <= 0.0 )
      {
      fprintf( stderr, "Warning: %s is constant: plot %s.eps not made\n", name, x11out );
      return( NULL );
      }
   L = (SeriesSize( n, freq ) == 2) ? &layout_big : &layout_small;
   W = L->W;
   H = L->H;

   a = standardize( ser );
   AbsMax = SeriesMax( a, n );

   if ( lags <= 0 ) lags = default_lags( n, freq );
   if ( lags > n - 2 ) lags = n - 2;
   cmax = (cbands > 0) ? cbands : Acf_Pacf_Max( ser, lags );
   acf  = vector( 1, lags );
   pacf = vector( 1, lags );
   Acf( ser, lags, acf );
   for ( i = 1; i <= lags; i++ ) pacf[i] = acf[i];
   Pacf( lags, pacf );

   f = fd_fig_new( W, H );

   /* panels: left margin fits the widest tick label */
   snprintf( s, sizeof( s ), "%d", -(int)AbsMax );
   maxw = fd_text_width( FD_HELV, L->tick, s );
   sx0 = 4.0 + maxw + 1.5 + TIC_MAJOR;
   sx1 = 0.632 * W;
   cx0 = 0.738 * W;
   cx1 = W - 12.0;

   series_panel( f, L, sx0, sx1, 0.26 * H, 0.77 * H, a, n, freq, tsnobs, tmornsop, tsby, AbsMax );
   title( f, L->title, sx1, H - 16.0, FD_RIGHT, nrdiff, nadiff, freq, boxlam, name );
   statistics( f, L, (sx0 + sx1) / 2.0, (L == &layout_big) ? 16.0 : 14.0, ser->mean,
               Stdev( ser->data, n ), n );

   corr_panel( f, L, cx0, cx1, 0.595 * H, 0.833 * H, acf, lags, freq, n, cmax, "acf" );
   snprintf( s, sizeof( s ), "Q( %d ) = %.1f", lags - npar, ChiTest( acf, lags, n ) );
   fd_text( f, (cx0 + cx1) / 2.0, 0.595 * H - 21.0, FD_HELV, L->q, FD_CENTER, s );
   corr_panel( f, L, cx0, cx1, 0.125 * H, 0.365 * H, pacf, lags, freq, n, cmax, "pacf" );

   save_eps( f, "", x11out );
   free_vector( a, 1, n );
   free_vector( acf, 1, lags );
   free_vector( pacf, 1, lags );
   return( f );
}

/*****************************************************************************/
/* Graph -a: standardized series                                             */
/*****************************************************************************/

FDFig *fp_PlotSer( struct Tseries *ser, int tsnobs, int tmornsop, int tsby, double boxlam,
                   int nrdiff, int nadiff, const char *x11out, const char *name )
{
   const Layout *L;
   FDFig  *f;
   double *a, AbsMax, sx0, sx1;
   int     n = ser->nobs, freq = ser->freq;
   char    s[32];

   if ( ser->var <= 0.0 )
      {
      fprintf( stderr, "Warning: %s is constant: plot %s.eps not made\n", name, x11out );
      return( NULL );
      }
   L = (SeriesSize( n, freq ) == 2) ? &layout_big : &layout_small;
   a = standardize( ser );
   AbsMax = SeriesMax( a, n );

   f = fd_fig_new( L->W, L->H );
   snprintf( s, sizeof( s ), "%d", -(int)AbsMax );
   sx0 = 4.0 + fd_text_width( FD_HELV, L->tick, s ) + 1.5 + TIC_MAJOR;
   sx1 = L->W - 12.0;
   series_panel( f, L, sx0, sx1, 53.0, L->H - 34.0, a, n, freq, tsnobs, tmornsop, tsby, AbsMax );
   title( f, L->title, sx1, L->H - 16.0, FD_RIGHT, nrdiff, nadiff, freq, boxlam, name );
   statistics( f, L, (sx0 + sx1) / 2.0, 12.0, ser->mean, Stdev( ser->data, n ), n );

   save_eps( f, "", x11out );
   free_vector( a, 1, n );
   return( f );
}

/*****************************************************************************/
/* Graph -b: acf and pacf                                                    */
/*****************************************************************************/

FDFig *fp_CorrSer( struct Tseries *ser, int npar, int lags, double cbands, const char *x11out,
                   const char *name )
{
   Layout  Lb = layout_big;
   FDFig  *f;
   double *acf, *pacf, cmax, W, H = 252.0, x0, x1;
   int     i, n = ser->nobs, freq = ser->freq;
   char    s[32];

   if ( ser->var <= 0.0 )
      {
      fprintf( stderr, "Warning: %s is constant: plot acf_%s.eps not made\n", name, x11out );
      return( NULL );
      }
   if ( lags <= 0 ) lags = default_lags( n, freq );
   if ( lags > n - 2 ) lags = n - 2;
   cmax = (cbands > 0) ? cbands : Acf_Pacf_Max( ser, lags );
   acf  = vector( 1, lags );
   pacf = vector( 1, lags );
   Acf( ser, lags, acf );
   for ( i = 1; i <= lags; i++ ) pacf[i] = acf[i];
   Pacf( lags, pacf );

   /* the canvas grows with the number of lags */
   W = (lags >= 30) ? 234.0 : (lags >= 15) ? 152.3 : 101.9;
   Lb.tick = (lags >= 30) ? 8.0 : 7.5;
   Lb.q = (freq > 4 || (freq == 1 && n > 200)) ? 8.5 : 7.5;
   x0 = 34.0;
   x1 = W - 6.0;

   f = fd_fig_new( W, H );
   corr_panel( f, &Lb, x0, x1, 0.595 * H, 0.833 * H, acf, lags, freq, n, cmax, "acf" );
   snprintf( s, sizeof( s ), "Q( %d ) = %.1f", lags - npar, ChiTest( acf, lags, n ) );
   fd_text( f, (x0 + x1) / 2.0, 0.595 * H - 21.0, FD_HELV, Lb.q, FD_CENTER, s );
   corr_panel( f, &Lb, x0, x1, 0.125 * H, 0.365 * H, pacf, lags, freq, n, cmax, "pacf" );

   save_eps( f, "acf_", x11out );
   free_vector( acf, 1, lags );
   free_vector( pacf, 1, lags );
   return( f );
}

/*****************************************************************************/
/* Graph -d: histogram of the standardized series and normal density        */
/*****************************************************************************/

#define GRID_GRAY 0.55

static void dotted_grid_line( FDFig *f, double x1, double y1, double x2, double y2 )
{
   fd_gray( f, GRID_GRAY );
   fd_linewidth( f, LW_THIN );
   fd_dash( f, 0.6, 1.4 );
   fd_line( f, x1, y1, x2, y2 );
   fd_dash( f, 0, 0 );
   fd_gray( f, 0.0 );
}

FDFig *fp_histogram( struct Tseries *ser, int nrdiff, int nadiff, double boxlam,
                     const char *x11out, const char *name )
{
   const double W = 270.0, H = 189.0, x0 = 84.0, x1 = 213.0, y0 = 32.0, y1 = 160.0;
   FDFig  *f;
   double  sd, xmax = 0.0, num, breakk[52], prob[52], ymax, maxp = 0.0, v, *xs, *ys;
   double  skew, kurt, jb;
   int     i, j, k, NumCat, n = ser->nobs;
   char    s[96];

   if ( ser->var <= 0.0 )
      {
      fprintf( stderr, "Warning: %s is constant: plot hist_%s.eps not made\n", name, x11out );
      return( NULL );
      }
   sd   = sqrt( ser->var );
   skew = Skew( ser->data, n );
   kurt = Kurt( ser->data, n );
   jb   = JarqueBera( skew, kurt, n );

   /* categories of width 0.5 between -xmax and xmax (xmax = 4 or 8) */
   for ( i = 1; i <= n; i++ )
       if ( fabs( (ser->data[i] - ser->mean) / sd ) > xmax ) xmax = fabs( (ser->data[i] - ser->mean) / sd );
   xmax = (xmax <= 4.0) ? 4.0 : 8.0;
   NumCat = 1;
   breakk[1] = -xmax + 0.5;
   while ( breakk[NumCat] < xmax - 1e-9 )
      {
      NumCat++;
      breakk[NumCat] = breakk[NumCat-1] + 0.5;
      }
   for ( j = 1; j <= NumCat; j++ ) prob[j] = 0.0;
   for ( i = 1; i <= n; i++ )
       {
       num = (ser->data[i] - ser->mean) / sd;
       if ( num <= breakk[1] ) prob[1] += 1;
       else
          for ( j = 2; j <= NumCat; j++ )
              if ( num > breakk[j-1] && num <= breakk[j] ) prob[j] += 1;
       }
   for ( j = 1; j <= NumCat; j++ )
       {
       prob[j] = 100.0 * 2.0 * prob[j] / n;          /* percentage density */
       if ( prob[j] > maxp ) maxp = prob[j];
       }
   ymax = 10.0 * ceil( ((maxp > 39.9) ? maxp : 39.9) / 10.0 - 1e-9 );

   f = fd_fig_new( W, H );

   /* dotted grid at the major and minor tics */
   for ( k = 1; k <= (int)(2 * xmax); k++ )
       {
       v = mapv( -xmax + k, -xmax, xmax, x0, x1 );
       dotted_grid_line( f, v, y0, v, y1 );
       }
   for ( v = 5.0; v <= ymax + 1e-9; v += 5.0 )
       dotted_grid_line( f, x0, mapv( v, 0, ymax, y0, y1 ), x1, mapv( v, 0, ymax, y0, y1 ) );

   /* histogram (steps) and normal density */
   xs = (double *)malloc( (2 * NumCat + 2) * sizeof( double ) );
   ys = (double *)malloc( (2 * NumCat + 2) * sizeof( double ) );
   k = 0;
   xs[k] = mapv( breakk[1] - 0.5, -xmax, xmax, x0, x1 ); ys[k++] = y0;
   for ( j = 1; j <= NumCat; j++ )
       {
       xs[k] = mapv( breakk[j] - 0.5, -xmax, xmax, x0, x1 ); ys[k++] = mapv( prob[j], 0, ymax, y0, y1 );
       xs[k] = mapv( breakk[j], -xmax, xmax, x0, x1 );       ys[k++] = mapv( prob[j], 0, ymax, y0, y1 );
       }
   xs[k] = xs[k-1]; ys[k++] = y0;
   fd_clip_begin( f, x0, y0, x1 - x0, y1 - y0 );
   fd_linewidth( f, 0.5 );
   fd_polyline( f, xs, ys, k );
   free( xs );
   free( ys );
   xs = (double *)malloc( 201 * sizeof( double ) );
   ys = (double *)malloc( 201 * sizeof( double ) );
   for ( k = 0; k <= 200; k++ )
       {
       v = -xmax + 2.0 * xmax * k / 200.0;
       xs[k] = mapv( v, -xmax, xmax, x0, x1 );
       ys[k] = mapv( 100.0 * exp( -v * v / 2.0 ) / 2.5066282746310002, 0, ymax, y0, y1 );
       }
   fd_linewidth( f, LW_THIN );
   fd_polyline( f, xs, ys, 201 );
   fd_clip_end( f );
   free( xs );
   free( ys );

   /* axes (left and bottom) with tics outside */
   fd_linewidth( f, 0.325 );
   fd_line( f, x0, y0, x0, y1 );
   fd_line( f, x0, y0, x1, y0 );
   for ( k = 0; k <= (int)(2 * xmax); k++ )
       {
       v = mapv( -xmax + k, -xmax, xmax, x0, x1 );
       if ( k % 2 == 0 )
          {
          fd_line( f, v, y0, v, y0 - TIC_MAJOR );
          snprintf( s, sizeof( s ), "%d", (int)(-xmax + k) );
          fd_text( f, v, y0 - TIC_MAJOR - 2.0 - 0.718 * 9.0, FD_HELV, 9.0, FD_CENTER, s );
          }
       else
          fd_line( f, v, y0, v, y0 - TIC_MINOR );
       }
   for ( v = 0.0; v <= ymax + 1e-9; v += 5.0 )
       {
       double py = mapv( v, 0, ymax, y0, y1 );
       if ( fmod( v, 10.0 ) < 1e-9 )
          {
          fd_line( f, x0, py, x0 - TIC_MAJOR, py );
          snprintf( s, sizeof( s ), "%d", (int)v );
          fd_text( f, x0 - TIC_MAJOR - 2.0, mid( py, 9.0 ), FD_HELV, 9.0, FD_RIGHT, s );
          }
       else
          fd_line( f, x0, py, x0 - TIC_MINOR, py );
       }

   fd_text_up( f, x0 - 28.0, (y0 + y1) / 2.0, FD_HELV, 12.0, FD_CENTER, "%" );
   snprintf( s, sizeof( s ), "S = %2.1f       K = %2.1f        JB = %2.1f", skew, kurt, jb );
   fd_text( f, (x0 + x1) / 2.0, 4.0, FD_HELV, 10.0, FD_CENTER, s );
   title( f, 11.0, (x0 + x1) / 2.0, H - 16.0, FD_CENTER, nrdiff, nadiff, ser->freq, boxlam, name );

   save_eps( f, "hist_", x11out );
   return( f );
}

/*****************************************************************************/
/* Graph -e: standardized means and standard deviations of groups           */
/*****************************************************************************/

FDFig *fp_graph_m_dt( double *y, int n, int nog, int freq, double boxlam,
                      const char *x11out, const char *name )
{
   const double W = 360.0, H = 252.0, x0 = 99.0, x1 = 293.0, y0 = 30.0, y1 = 224.0;
   FDFig  *f;
   double *ms, *dts, A = 0.0, px, py;
   int     i, k, ng;
   char    s[32];

   if ( nog < 1 ) nog = 1;
   ng = n / nog;
   if ( ng < 2 )
      {
      fprintf( stderr, "Warning: less than 2 groups of %d observations: plot m_dt_%s.eps not made\n",
               nog, x11out );
      return( NULL );
      }
   ms  = vector( 1, ng );
   dts = vector( 1, ng );
   if ( meandv( y, dts, ms, nog, ng ) != 0 )
      {
      fprintf( stderr, "Warning: the group means or standard deviations are all equal: "
                       "plot m_dt_%s.eps not made\n", x11out );
      free_vector( ms, 1, ng );
      free_vector( dts, 1, ng );
      return( NULL );
      }
   for ( i = 1; i <= ng; i++ )
       {
       if ( fabs( ms[i] ) > A ) A = fabs( ms[i] );
       if ( fabs( dts[i] ) > A ) A = fabs( dts[i] );
       }
   A = floor( 1.1 * A * 10.0 + 0.5 ) / 10.0;        /* scale with one decimal */
   if ( A < 0.1 ) A = 0.1;

   f = fd_fig_new( W, H );

   /* lines at 0 and points */
   fd_linewidth( f, LW_THIN );
   fd_line( f, mapv( 0, -A, A, x0, x1 ), y0, mapv( 0, -A, A, x0, x1 ), y1 );
   fd_line( f, x0, mapv( 0, -A, A, y0, y1 ), x1, mapv( 0, -A, A, y0, y1 ) );
   fd_clip_begin( f, x0, y0, x1 - x0, y1 - y0 );
   for ( i = 1; i <= ng; i++ )
       fd_disc( f, mapv( ms[i], -A, A, x0, x1 ), mapv( dts[i], -A, A, y0, y1 ), 1.4 );
   fd_clip_end( f );

   /* frame, tics outside at -A, 0, A */
   fd_linewidth( f, 0.5 );
   fd_line( f, x0, y0, x1, y0 );
   fd_line( f, x1, y0, x1, y1 );
   fd_line( f, x1, y1, x0, y1 );
   fd_line( f, x0, y1, x0, y0 );
   for ( k = -1; k <= 1; k++ )
       {
       number( s, sizeof( s ), k * A );
       px = mapv( k * A, -A, A, x0, x1 );
       fd_line( f, px, y0, px, y0 - TIC_MAJOR );
       fd_text( f, px, y0 - TIC_MAJOR - 2.0 - 0.718 * 8.5, FD_HELV, 8.5, FD_CENTER, s );
       py = mapv( k * A, -A, A, y0, y1 );
       fd_line( f, x0, py, x0 - TIC_MAJOR, py );
       fd_text( f, x0 - TIC_MAJOR - 2.0, mid( py, 8.5 ), FD_HELV, 8.5, FD_RIGHT, s );
       }
   fd_text( f, (x0 + x1) / 2.0, 6.0, FD_HELV, 10.0, FD_CENTER, "Mean" );
   fd_text_up( f, x0 - 34.0, (y0 + y1) / 2.0, FD_HELV, 10.0, FD_CENTER, "Standard-Deviation" );
   title( f, 11.0, (x0 + x1) / 2.0, H - 14.0, FD_CENTER, 0, 0, freq, boxlam, name );

   save_eps( f, "m_dt_", x11out );
   free_vector( ms, 1, ng );
   free_vector( dts, 1, ng );
   return( f );
}

/*****************************************************************************/
/* PDF with all the figures                                                  */
/*****************************************************************************/

int fp_write_pdf( const char *filename, FDFig **figs, const double *scale, int n, int landscape )
{
   const double margin = 36.0, gap = 14.0;
   double pw = landscape ? 841.89 : 595.28, ph = landscape ? 595.28 : 841.89;
   double top, s, *x, *y, *sc;
   FDFig **page;
   FDPdf  *pdf;
   int     i, k = 0;

   if ( n == 0 ) return( 1 );
   pdf = fd_pdf_open( filename );
   if ( pdf == NULL ) return( 1 );
   page = (FDFig **)malloc( n * sizeof( FDFig * ) );
   x  = (double *)malloc( n * sizeof( double ) );
   y  = (double *)malloc( n * sizeof( double ) );
   sc = (double *)malloc( n * sizeof( double ) );

   top = ph - margin;
   for ( i = 0; i < n; i++ )
       {
       s = scale[i];                  /* 0.90 for the series, 0.70 for mean-std */
       if ( s * figs[i]->w > pw - 2 * margin ) s = (pw - 2 * margin) / figs[i]->w;
       if ( k > 0 && top - s * figs[i]->h < margin )
          {
          fd_pdf_page( pdf, pw, ph, page, x, y, sc, k );
          k = 0;
          top = ph - margin;
          }
       page[k] = figs[i];
       sc[k] = s;
       x[k]  = (pw - s * figs[i]->w) / 2.0;
       y[k]  = top - s * figs[i]->h;
       top   = y[k] - gap;
       k++;
       }
   fd_pdf_page( pdf, pw, ph, page, x, y, sc, k );

   free( page ); free( x ); free( y ); free( sc );
   return( fd_pdf_close( pdf ) );
}
