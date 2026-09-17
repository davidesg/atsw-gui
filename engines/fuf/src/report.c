/*****************************************************************************/
/* report.c -- the report of FUF drawn with fugdraw, without LaTeX.          */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*                                                                           */
/* The page has the same parts as the one that pdflatex made: the heading of */
/* the series, the table of the forecasts with their standard errors, and    */
/* the graph. The forecast rows keep the grey background they had.           */
/*                                                                           */
/* The measures are those of the report as it is published                   */
/* (tesisdeg/prevision/po/colombia.*.ps): the head is a box with a rule      */
/* between its columns, the rule of \cline{2-7} under LEVEL and LOG RATE OF  */
/* CHANGE, and the rule that closes it; the rows are 9.00 pt apart; the      */
/* graph has its top against the top of the table and is as tall as it, so   */
/* the two end together. There is no rule under the last row. The whole      */
/* block is then multiplied by the factor that makes it fill the page, and   */
/* a table too long for one column runs into two or three.                   */
/*                                                                           */
/* The units: when the series is in logarithms the changes are rates, and    */
/* they are written in per cent as they always were. When it is not, they    */
/* are changes in the units of the series, and multiplying them by 100 only  */
/* filled the table with numbers that did not fit. What is in the units of   */
/* the series is then divided, if it is large, by a thousand or a million,   */
/* and the heading of the page says so after "Base/Unit:".                   */
/*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "report.h"

extern void ObsToDate( int beg_per, int beg_sub, int obs_no, int freq,
                       int *per, int *sub );

#define F_TEXT  FD_HELV
#define F_BOLD  FD_HELV_BOLD

/* The table of the published report, measured on it: one unit of what
 * follows is one point of that page. The whole block (table and graph
 * together) is then multiplied by the factor that makes the table fill the
 * page, so what changes is the size, never the proportions.                */

#define SZ        7.43               /* the data of the table                */
#define SZ_LAB    6.55               /* its headings                         */
#define ROW       9.00               /* from one row to the next             */
#define PAD       6.00               /* at each side of a column (\tabcolsep)*/

/* The head, from the top rule down (the rule itself is at 0) */
#define H_GRP     9.72               /* LEVEL, LOG RATE OF CHANGE            */
#define H_CLINE  15.24               /* the rule under them (columns 2..7)   */
#define H_LAB    27.00               /* DATE VALUE Std ...                   */
#define H_UNIT   35.75               /* the (%)                              */
#define H_RULE   39.36               /* the rule that closes the head        */
#define H_DATA   54.64               /* the first row of data                */

#define BAND_UP   7.24               /* the grey band, over the base line    */
#define GRAPH_GAP 0.0                /* the graph carries its own margin     */
#define FIT_MAX   1.55               /* a short table does not grow for ever */
#define COLSEP   14.0                /* between two columns of the table     */
#define MAXCOLS   3                  /* a long horizon runs into columns     */

#define SZ_HEAD  11.0                /* the heading of the page              */
#define GREY      0.95               /* the background of the forecast rows  */
#define LW_RULE   0.5

/* The page */
#define PW      841.89
#define PH      595.276
#define MARGIN   36.0
#define Y_NAME   65.76               /* base lines of the heading, from the  */
#define Y_DESC   79.36               /* top of the page                      */
#define Y_ORIG   92.96
#define Y_TABLE 108.48               /* the top rule of the table            */

#define NCOL 8
#define CELL 24
#define BIG  100000.0                /* more digits than a column can hold   */

typedef struct {
   char   date[CELL];
   char   text[NCOL][CELL];
   double v[NCOL];                   /* the numbers, before they are scaled  */
   char   has[NCOL];                 /* 0: the column shows a dash           */
   int    shaded;                    /* a forecast row                       */
   int    gap;                       /* a blank row before it                */
} Row;

/* The metrics above, shrunk by fit when the table would not fit the page */
typedef struct {
   double sz, lab, row, pad;
   double grp, cline, lab_y, unit, rule, data, band;
} Metrics;

static void rule( FDFig *f, double x, double y, double w )
{
   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_RULE );
   fd_dash( f, 0.0, 0.0 );
   fd_line( f, x, y, x + w, y );
}

static void vrule( FDFig *f, double x, double y0, double y1 )
{
   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_RULE );
   fd_dash( f, 0.0, 0.0 );
   fd_line( f, x, y0, x, y1 );
}

/* A grey band is drawn as a very thick line inside a clip: fugdraw has no
 * filled rectangle, and this keeps the drawing to its own primitives.      */
static void band( FDFig *f, double x, double y, double w, double h, double gray )
{
   fd_gray( f, gray );
   fd_linewidth( f, h );
   fd_dash( f, 0.0, 0.0 );
   fd_line( f, x, y + h / 2.0, x + w, y + h / 2.0 );
   fd_gray( f, 0.0 );
   fd_linewidth( f, LW_RULE );
}

/* One row of the table: a number goes against the right of its column and a
 * dash, which says that there is no number, goes in the middle.            */
static void draw_row( FDFig *f, const double *cx, double y, const Row *r,
                      const Metrics *m )
{
   int i;

   fd_text( f, cx[1] - m->pad, y, F_TEXT, m->sz, FD_RIGHT, r->date );
   for ( i = 1; i < NCOL; i++ )
       {
       if ( r->has[i] )
          fd_text( f, cx[i+1] - m->pad, y, F_TEXT, m->sz, FD_RIGHT, r->text[i] );
       else
          fd_text( f, ( cx[i] + cx[i+1] ) / 2.0, y, F_TEXT, m->sz, FD_CENTER, "-" );
       }
}

/* The head of one column of the table: the box, the two headings that span
 * several columns with their rule, the labels and the units.               */
static void draw_head( FDFig *f, const double *cx, double top, double tw,
                       const Metrics *m, const char **label, const char **unit,
                       const char *grp1, const char *grp2 )
{
   int i;

   rule( f, cx[0], top, tw );                          /* \hline          */
   rule( f, cx[1], top - m->cline, cx[7] - cx[1] );     /* \cline{2-7}     */
   rule( f, cx[0], top - m->rule, tw );                 /* \hline          */
   vrule( f, cx[0], top, top - m->rule );
   vrule( f, cx[1], top, top - m->rule );
   vrule( f, cx[3], top, top - m->rule );
   vrule( f, cx[7], top, top - m->rule );
   vrule( f, cx[8], top, top - m->rule );
   for ( i = 2; i <= 6; i++ )
       if ( i != 3 ) vrule( f, cx[i], top - m->cline, top - m->rule );

   fd_text( f, ( cx[1] + cx[3] ) / 2.0, top - m->grp, F_TEXT, m->lab,
            FD_CENTER, grp1 );
   fd_text( f, ( cx[3] + cx[7] ) / 2.0, top - m->grp, F_TEXT, m->lab,
            FD_CENTER, grp2 );
   for ( i = 0; i < NCOL; i++ )
       {
       fd_text( f, ( cx[i] + cx[i+1] ) / 2.0, top - m->lab_y, F_TEXT, m->lab,
                FD_CENTER, label[i] );
       if ( unit[i][0] != '\0' )
          fd_text( f, ( cx[i] + cx[i+1] ) / 2.0, top - m->unit, F_TEXT, m->lab,
                   FD_CENTER, unit[i] );
       }
}

/*****************************************************************************/

int report_write_pdf( const char *filename, FDFig *graph, const char *name,
                      int nobs, int freq, int begyear, int begtime, int ornsop,
                      int L, double *data, double **a, double **f1, double **f2,
                      double **f3, double ***v1, double ***v2, double ***v3,
                      double boxlam, double refactor, int full )
{
   const char  *label[NCOL], *unit[NCOL];
   const char  *grp1 = "LEVEL", *grp2;
   const char  *scale_word = "";
   double       cx[NCOL+1], w[NCOL], tw, gw, gh, s, ty, top, left, block;
   double       x[2], y[2], scale[2], need = 0.0, avail, fit, d, pct, div, big;
   double       colsep, maxw, x0;
   char         line[160];
   Metrics      m;
   Row         *rows;
   int         *lrow;
   FDFig       *page, *figs[2];
   FDPdf       *pdf;
   int          i, j, k, c, n = 0, nr = 0, cap, status, period, season;
   int          last_year = 1, logs, nlines = 0, ncols = 1, per = 0;

   logs = ( boxlam == 0.0 );
   pct  = logs ? 100.0 : 1.0;      /* in logarithms a change is a rate, in % */
   grp2 = logs ? "LOG RATE OF CHANGE" : "CHANGE";

   label[0] = "DATE";
   label[1] = "VALUE";
   label[2] = "Std";
   label[3] = ( freq == 4 ) ? "QUART" : "MONT";
   label[4] = "Std";
   label[5] = "ANUAL";
   label[6] = "Std";
   label[7] = "ERR";
   for ( i = 0; i < NCOL; i++ ) unit[i] = "";
   if ( logs ) for ( i = 2; i < NCOL; i++ ) unit[i] = "(%)";

/* [1]: the rows of the table, before anything is drawn: their width is what
 *      says how wide each column has to be.                                 */

   cap = L + L / 2 + 8;
   if ( ( rows = calloc( (size_t)cap, sizeof( Row ) ) ) == NULL ) return( 1 );

   for ( i = nobs - L / 2; i <= nobs; i++ )
       {
       Row *r = &rows[nr++];

       ObsToDate( begyear, begtime, i, freq, &period, &season );
       if ( freq == 1 ) snprintf( r->date, CELL, "%d", period );
       else snprintf( r->date, CELL, "%d/%d", season, period );
       r->v[1] = logs ? exp( data[i] / refactor ) : data[i] / refactor;
       r->v[3] = pct * ( data[i] - data[i-1] ) / refactor;
       r->v[5] = pct * ( data[i] - data[i-freq] ) / refactor;
       r->v[7] = pct * a[1][i-ornsop] / refactor;
       r->has[1] = r->has[3] = r->has[5] = r->has[7] = 1;
       }

   /* The forecasts, shaded as they were in the LaTeX table: the first L/2
    * one by one and, after a gap, only the ends of year of the rest.       */
   for ( i = 1; i <= L && nr < cap; i++ )
       {
       Row *r;

       ObsToDate( begyear, begtime, nobs + i, freq, &period, &season );
       if ( !full && i > L / 2 )
          {
          if ( freq > 1 && season != freq ) continue;
          if ( i == L / 2 + 1 || last_year ) rows[nr].gap = 1;
          last_year = 0;
          }
       r = &rows[nr++];
       r->shaded = 1;
       if ( freq == 1 ) snprintf( r->date, CELL, "%d", period );
       else snprintf( r->date, CELL, "%d/%d", season, period );
       r->v[1] = logs ? exp( f1[1][i] / refactor ) : f1[1][i] / refactor;
       r->v[2] = pct * (double)sqrtl( v1[i][1][1] ) / refactor;
       r->v[3] = pct * f2[1][i] / refactor;
       r->v[4] = pct * (double)sqrtl( v2[i][1][1] ) / refactor;
       r->v[5] = pct * f3[1][i] / refactor;
       r->v[6] = pct * (double)sqrtl( v3[i][1][1] ) / refactor;
       for ( j = 1; j <= 6; j++ ) r->has[j] = 1;
       }

/* [2]: the units. What is in the units of the series (the level always, and
 *      the changes when there are no logarithms) is divided by a thousand or
 *      a million if it does not fit in a column.                            */

   big = 0.0;
   for ( i = 0; i < nr; i++ )
       for ( j = 1; j < NCOL; j++ )
           if ( rows[i].has[j] && ( j == 1 || !logs ) )
              if ( fabs( rows[i].v[j] ) > big ) big = fabs( rows[i].v[j] );
   div = 1.0;
   while ( big / div >= BIG && div < 1e12 ) div *= 1000.0;
   if ( div >= 1e12 )     scale_word = "billions";
   else if ( div >= 1e9 ) scale_word = "thousand millions";
   else if ( div >= 1e6 ) scale_word = "millions";
   else if ( div >= 1e3 ) scale_word = "thousands";

   for ( i = 0; i < nr; i++ )
       for ( j = 1; j < NCOL; j++ )
           if ( rows[i].has[j] )
              snprintf( rows[i].text[j], CELL, "%.2f",
                        ( j == 1 || !logs ) ? rows[i].v[j] / div : rows[i].v[j] );

/* [3]: the metrics. The table is measured at the size of the published
 *      report and the whole block is then multiplied by the factor that
 *      makes it fill the page: the graph is as tall as the table, as it is
 *      there, so the two end together whatever the horizon.                 */

   for ( i = 0; i < NCOL; i++ )       /* the columns, at the published size  */
       {
       w[i] = fd_text_width( F_TEXT, SZ_LAB, label[i] );
       d = fd_text_width( F_TEXT, SZ_LAB, unit[i] );
       if ( d > w[i] ) w[i] = d;
       for ( j = 0; j < nr; j++ )
           {
           d = fd_text_width( F_TEXT, SZ,
                              ( i == 0 ) ? rows[j].date : rows[j].text[i] );
           if ( d > w[i] ) w[i] = d;
           }
       w[i] += 2 * PAD;
       }
   /* the two headings that span several columns have to fit as well */
   d = fd_text_width( F_TEXT, SZ_LAB, grp1 ) + 2 * PAD - ( w[1] + w[2] );
   if ( d > 0.0 ) { w[1] += d / 2.0; w[2] += d / 2.0; }
   d = fd_text_width( F_TEXT, SZ_LAB, grp2 ) + 2 * PAD
       - ( w[3] + w[4] + w[5] + w[6] );
   if ( d > 0.0 ) for ( i = 3; i <= 6; i++ ) w[i] += d / 4.0;

   tw = 0.0;
   for ( i = 0; i < NCOL; i++ ) tw += w[i];

   /* the lines of the table: a blank one where it leaves a gap */
   for ( i = 0; i < nr; i++ ) nlines += 1 + rows[i].gap;
   if ( ( lrow = malloc( (size_t)( nlines + 1 ) * sizeof( int ) ) ) == NULL )
      { free( rows ); return( 1 ); }
   for ( i = 0, j = 0; i < nr; i++ )
       {
       if ( rows[i].gap ) lrow[j++] = -1;
       lrow[j++] = i;
       }

   /* How many columns the table runs into: one, as the published report has
    * it, unless a long horizon would then shrink it below its own size; in
    * that case as few columns as keep it at that size or above.            */
   avail = PH - Y_TABLE - MARGIN;
   maxw  = PW - 2 * MARGIN;
   fit   = 0.0;
   for ( k = 1; k <= MAXCOLS; k++ )
       {
       int    p  = ( nlines + k - 1 ) / k;
       double hb = H_DATA + ( p - 1 ) * ROW + ( ROW - BAND_UP );
       double gb = ( graph != NULL && graph->h > 0.0 )
                   ? hb * graph->w / graph->h : 0.0;
       double bb = k * tw + ( k - 1 ) * COLSEP + gb + ( ( gb > 0.0 ) ? GRAPH_GAP : 0.0 );
       double f  = ( hb > 0.0 ) ? avail / hb : 1.0;

       if ( f > FIT_MAX ) f = FIT_MAX;
       if ( bb > 0.0 && f * bb > maxw ) f = maxw / bb;
       if ( f > fit ) { fit = f; ncols = k; per = p; need = hb; }
       if ( f >= 1.0 ) break;             /* no need to split it any further */
       }

   m.sz = SZ * fit; m.lab = SZ_LAB * fit; m.row = ROW * fit; m.pad = PAD * fit;
   m.grp = H_GRP * fit; m.cline = H_CLINE * fit; m.lab_y = H_LAB * fit;
   m.unit = H_UNIT * fit; m.rule = H_RULE * fit; m.data = H_DATA * fit;
   m.band = BAND_UP * fit;
   for ( i = 0; i < NCOL; i++ ) w[i] *= fit;
   tw    *= fit;
   colsep = COLSEP * fit;
   gh     = need * fit;                          /* as tall as the table     */
   gw     = ( graph != NULL && graph->h > 0.0 ) ? gh * graph->w / graph->h : 0.0;
   s      = ( graph != NULL && graph->h > 0.0 ) ? gh / graph->h : 1.0;
   block  = ncols * tw + ( ncols - 1 ) * colsep
            + ( ( graph != NULL ) ? GRAPH_GAP + gw : 0.0 );

/* [4]: where the table and the graph go: the block, centred on the page.    */

   left  = ( PW - block ) / 2.0;
   if ( left < MARGIN ) left = MARGIN;

   if ( ( page = fd_fig_new( PW, PH ) ) == NULL )
      { free( rows ); free( lrow ); return( 1 ); }

/* [5]: the heading of the page                                              */

   fd_text( page, PW / 2.0, PH - Y_NAME, F_BOLD, SZ_HEAD + 1.0, FD_CENTER, name );
   fd_text( page, PW / 2.0, PH - Y_DESC, F_BOLD, SZ_HEAD, FD_CENTER,
            "Series brief description" );
   ObsToDate( begyear, begtime, nobs, freq, &period, &season );
   if ( freq == 1 )
      snprintf( line, sizeof( line ),
                "Base/Unit: %s      Data Source:      Forecast Origin: %d",
                scale_word, period );
   else
      snprintf( line, sizeof( line ),
                "Base/Unit: %s      Data Source:      Forecast Origin: %d/%d",
                scale_word, season, period );
   fd_text( page, PW / 2.0, PH - Y_ORIG, F_TEXT, SZ_HEAD, FD_CENTER, line );

/* [6]: the table, one head and its rows for each column                     */

   top = PH - Y_TABLE;
   for ( c = 0; c < ncols; c++ )
       {
       x0    = left + c * ( tw + colsep );
       cx[0] = x0;
       for ( i = 0; i < NCOL; i++ ) cx[i+1] = cx[i] + w[i];
       draw_head( page, cx, top, tw, &m, label, unit, grp1, grp2 );

       ty = top - m.data;
       for ( k = c * per; k < ( c + 1 ) * per && k < nlines; k++ )
           {
           if ( lrow[k] < 0 )               /* the blank line of the gap     */
              {
              if ( k > c * per ) ty -= m.row;
              continue;
              }
           if ( rows[lrow[k]].shaded )
              band( page, cx[0], ty - ( m.row - m.band ), tw, m.row, GREY );
           draw_row( page, cx, ty, &rows[lrow[k]], &m );
           ty -= m.row;
           }
       }
   free( rows );
   free( lrow );

/* [8]: the graph, to the right of the table and with its top against the
 *      top rule, as the published report has it                             */

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
      x[n] = left + block - gw;
      y[n] = top - gh;
      scale[n] = s;
      n++;
      }
   fd_pdf_page( pdf, PW, PH, figs, x, y, scale, n );
   status = fd_pdf_close( pdf );
   fd_fig_free( page );
   return( status );
}
