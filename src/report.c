/*****************************************************************************/
/* report.c -- the report of FUF drawn with fugdraw, without LaTeX.          */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* The page has the same parts as the one that pdflatex made: the heading of */
/* the series, the table of the forecasts with their standard errors, and    */
/* the graph. The forecast rows keep the grey background they had.           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "report.h"

extern void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                       int *per, int *sub );

#define F_TEXT  FD_HELV
#define F_BOLD  FD_HELV_BOLD

#define SZ      8.0                  /* the table                            */
#define SZ_HEAD 10.0                 /* the heading of the page              */
#define ROW     (1.55 * SZ)
#define GREY    0.93                 /* the background of the forecast rows  */

/* The eight columns of the table, as in the LaTeX one */
#define NCOL 8
static const double col_w[NCOL] = { 5.6, 5.4, 4.2, 4.6, 4.2, 4.6, 4.2, 4.6 };

static double table_width( void )
{
   double w = 0.0;
   int    i;

   for ( i = 0; i < NCOL; i++ ) w += col_w[i] * SZ;
   return( w );
}

/* One row of the table: text[] has one string for each column */
static void row( FDFig *f, double x, double y, char text[NCOL][32] )
{
   double cx = x;
   int    i;

   for ( i = 0; i < NCOL; i++ )
       {
       if ( text[i][0] != '\0' )
          fd_text( f, cx + col_w[i] * SZ / 2.0, y, F_TEXT, SZ, FD_CENTER, text[i] );
       cx += col_w[i] * SZ;
       }
}

/* A grey band is drawn as a very thick line inside a clip: fugdraw has no
 * filled rectangle, and this keeps the drawing to its own primitives.      */
static void band( FDFig *f, double x, double y, double w, double h, double gray )
{
   fd_gray( f, gray );
   fd_linewidth( f, h );
   fd_line( f, x, y + h / 2.0, x + w, y + h / 2.0 );
   fd_gray( f, 0.0 );
   fd_linewidth( f, 0.4 );
}

static void rule( FDFig *f, double x, double y, double w )
{
   fd_gray( f, 0.0 );
   fd_linewidth( f, 0.5 );
   fd_dash( f, 0.0, 0.0 );
   fd_line( f, x, y, x + w, y );
}

/*****************************************************************************/

int report_write_pdf( const char *filename, FDFig *graph, const char *name,
                      int nobs, int freq, int begyear, int begtime, int ornsop,
                      int L, double *data, double **a, double **f1, double **f2,
                      double **f3, double ***v1, double ***v2, double ***v3,
                      double boxlam, double refactor )
{
   const double PW = 841.89, PH = 595.276, margin = 36.0;
   double       x[2], y[2], scale[2], gw, gh, s, tx, ty, w, top;
   char         text[NCOL][32];
   FDFig       *page, *figs[2];
   FDPdf       *pdf;
   int          i, n = 0, status, period, season, last_year = 1;

   if ( ( page = fd_fig_new( PW, PH ) ) == NULL ) return( 1 );

/* [1]: the heading                                                          */

   ty = PH - margin - SZ_HEAD;
   fd_text( page, PW / 2.0, ty, F_BOLD, SZ_HEAD + 2.0, FD_CENTER, name );
   ty -= 1.8 * SZ_HEAD;
   fd_text( page, PW / 2.0, ty, F_BOLD, SZ_HEAD, FD_CENTER, "Series brief description" );
   ty -= 1.8 * SZ_HEAD;
   ObsToDate( begyear, begtime, nobs, freq, &period, &season );
   if ( freq == 1 )
      snprintf( text[0], sizeof( text[0] ), "%d", period );
   else
      snprintf( text[0], sizeof( text[0] ), "%d/%d", season, period );
   {
   char line[160];

   snprintf( line, sizeof( line ), "Base/Unit:      Data Source:      Forecast Origin: %s",
             text[0] );
   fd_text( page, PW / 2.0, ty, F_TEXT, SZ_HEAD, FD_CENTER, line );
   }

   top = ty - 1.6 * SZ_HEAD;        /* under the heading: the table and the graph */

/* [2]: the table, at the left                                               */

   w  = table_width();
   tx = margin + 10.0;
   ty = top - 1.4 * SZ_HEAD;

   rule( page, tx, ty + 1.5 * SZ, w );
   memset( text, 0, sizeof( text ) );
   snprintf( text[1], sizeof( text[1] ), "LEVEL" );
   snprintf( text[4], sizeof( text[4] ), "LOG RATE OF CHANGE" );
   fd_text( page, tx + (col_w[1] + col_w[2]) * SZ / 2.0 + col_w[0] * SZ, ty, F_TEXT,
            SZ, FD_CENTER, "LEVEL" );
   fd_text( page, tx + (col_w[0] + col_w[1] + col_w[2]) * SZ
                  + (col_w[3] + col_w[4] + col_w[5] + col_w[6]) * SZ / 2.0, ty,
            F_TEXT, SZ, FD_CENTER, "LOG RATE OF CHANGE" );
   ty -= ROW;
   memset( text, 0, sizeof( text ) );
   snprintf( text[0], sizeof( text[0] ), "DATE" );
   snprintf( text[1], sizeof( text[1] ), "VALUE" );
   snprintf( text[2], sizeof( text[2] ), "Std" );
   snprintf( text[3], sizeof( text[3] ), "%s", ( freq == 4 ) ? "QUART" : "MONT" );
   snprintf( text[4], sizeof( text[4] ), "Std" );
   snprintf( text[5], sizeof( text[5] ), "ANUAL" );
   snprintf( text[6], sizeof( text[6] ), "Std" );
   snprintf( text[7], sizeof( text[7] ), "ERR" );
   row( page, tx, ty, text );
   ty -= ROW;
   memset( text, 0, sizeof( text ) );
   for ( i = 2; i < NCOL; i++ ) snprintf( text[i], sizeof( text[i] ), "(%%)" );
   row( page, tx, ty, text );
   ty -= 0.5 * ROW;
   rule( page, tx, ty + 0.9 * SZ, w );

   /* the last observations */
   for ( i = nobs - L / 2; i <= nobs; i++ )
       {
       ty -= ROW;
       memset( text, 0, sizeof( text ) );
       ObsToDate( begyear, begtime, i, freq, &period, &season );
       if ( freq == 1 ) snprintf( text[0], sizeof( text[0] ), "%d", period );
       else snprintf( text[0], sizeof( text[0] ), "%d/%d", season, period );
       snprintf( text[1], sizeof( text[1] ), "%.2f",
                 ( boxlam == 0 ) ? exp( data[i] / refactor ) : data[i] / refactor );
       snprintf( text[2], sizeof( text[2] ), "-" );
       snprintf( text[3], sizeof( text[3] ), "%.2f", 100 * (data[i] - data[i-1]) / refactor );
       snprintf( text[4], sizeof( text[4] ), "-" );
       snprintf( text[5], sizeof( text[5] ), "%.2f",
                 100 * (data[i] - data[i-freq]) / refactor );
       snprintf( text[6], sizeof( text[6] ), "-" );
       snprintf( text[7], sizeof( text[7] ), "%.2f", 100 * a[1][i-ornsop] / refactor );
       row( page, tx, ty, text );
       }

   /* The forecasts, shaded as they were in the LaTeX table: the first L/2
    * one by one and, after a gap, only the ends of year of the rest.       */
   for ( i = 1; i <= L; i++ )
       {
       ObsToDate( begyear, begtime, nobs + i, freq, &period, &season );
       if ( i > L / 2 )
          {
          if ( freq > 1 && season != freq ) continue;
          if ( i == L / 2 + 1 || last_year ) ty -= 0.7 * ROW;   /* the gap   */
          last_year = 0;
          }
       ty -= ROW;
       band( page, tx, ty - 0.32 * SZ, w, ROW, GREY );
       memset( text, 0, sizeof( text ) );
       if ( freq == 1 ) snprintf( text[0], sizeof( text[0] ), "%d", period );
       else snprintf( text[0], sizeof( text[0] ), "%d/%d", season, period );
       snprintf( text[1], sizeof( text[1] ), "%.2f",
                 ( boxlam == 0 ) ? exp( f1[1][i] / refactor ) : f1[1][i] / refactor );
       snprintf( text[2], sizeof( text[2] ), "%.2f",
                 100 * (double)sqrtl( v1[i][1][1] ) / refactor );
       snprintf( text[3], sizeof( text[3] ), "%.2f", 100 * f2[1][i] / refactor );
       snprintf( text[4], sizeof( text[4] ), "%.2f",
                 100 * (double)sqrtl( v2[i][1][1] ) / refactor );
       snprintf( text[5], sizeof( text[5] ), "%.2f", 100 * f3[1][i] / refactor );
       snprintf( text[6], sizeof( text[6] ), "%.2f",
                 100 * (double)sqrtl( v3[i][1][1] ) / refactor );
       row( page, tx, ty, text );
       }
   rule( page, tx, ty - 0.55 * ROW, w );

/* [3]: the graph, at the right                                              */

   /* under the heading, so that it does not run into it, and as large as
    * the page allows                                                       */
   gw = ( graph != NULL ) ? graph->w : 0.0;
   gh = ( graph != NULL ) ? graph->h : 0.0;
   s  = 1.0;
   if ( gh > 0.0 && s * gh > top - margin ) s = ( top - margin ) / gh;

   if ( ( pdf = fd_pdf_open( filename ) ) == NULL )
      {
      fd_fig_free( page );
      return( 1 );
      }
   figs[n] = page;
   x[n] = 0.0;
   y[n] = 0.0;
   scale[n] = 1.0;
   n++;
   if ( graph != NULL )
      {
      figs[n] = graph;
      x[n] = PW - margin - s * gw;
      y[n] = top - s * gh;
      scale[n] = s;
      n++;
      }
   fd_pdf_page( pdf, PW, PH, figs, x, y, scale, n );
   status = fd_pdf_close( pdf );
   fd_fig_free( page );
   return( status );
}
