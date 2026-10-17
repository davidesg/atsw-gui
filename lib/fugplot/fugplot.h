/***************************************************************************
 *   fugplot.h -- FUG graphs drawn with fugdraw (no gnuplot).               *
 *   Copyright (C) 2026 by David E. Guerrero & Arthur B. Treadway          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FUGPLOT_H
#define FUGPLOT_H

#include "plothost.h"
#include "fugdraw.h"

/* The graphs of FUG. ser is the transformed series (d, D differences),
 * name the series name, x11out the base name of the file (d1D1lnIPC...).
 * Each function writes its EPS file and returns the figure (to be placed
 * in the PDF or freed with fd_fig_free), or NULL if it can not be made.  */

/* The units of the statistics line under the series: w-bar, its standard
 * error and sigma-hat_w. The engines work with refactor * BoxCox(y), so the
 * data are divided by refactor (0: 1). pct says they are logs: 100 times a
 * change in a log is a change in %, and the line says %. Otherwise the line
 * is in the units of the series, with no %.                                */
typedef struct {
   double refactor;
   int    pct;
} FPUnits;

/* -a: standardized series                            -> <x11out>.eps       */
FDFig *fp_PlotSer( struct Tseries *ser, int tsnobs, int tmornsop, int tsby, double boxlam,
                   int nrdiff, int nadiff, const char *x11out, const char *name,
                   FPUnits u );

/* -b: acf and pacf                                   -> acf_<x11out>.eps   */
FDFig *fp_CorrSer( struct Tseries *ser, int npar, int lags, double cbands, const char *x11out,
                   const char *name );

/* -c: standardized series + acf/pacf                 -> <x11out>.eps       */
FDFig *fp_PlotSer_CorrSer( struct Tseries *ser, int npar, int tsnobs, int tmornsop,
                           int tsby, double boxlam, int nrdiff, int nadiff, int lags,
                           double cbands, const char *x11out, const char *name, FPUnits u );

/* Like -c, but with an open circle around each marked observation (marks
 * holds indices 1..nobs). For the GUI: it says which incident is being
 * calibrated. With marks NULL the drawing is the one of -c, untouched.    */
FDFig *fp_PlotSer_CorrSer_marks( struct Tseries *ser, int npar, int tsnobs, int tmornsop,
                                 int tsby, double boxlam, int nrdiff, int nadiff, int lags,
                                 double cbands, const char *x11out, const char *name,
                                 FPUnits u, const int *marks, int nmarks );

/* -d: histogram and normal density                   -> hist_<x11out>.eps  */
FDFig *fp_histogram( struct Tseries *ser, int nrdiff, int nadiff, double boxlam,
                     const char *x11out, const char *name );

/* -e: means and std. deviations of groups of nog observations of y[1..n]
 *                                                    -> m_dt_<x11out>.eps  */
FDFig *fp_graph_m_dt( double *y, int n, int nog, int freq, double boxlam,
                      const char *x11out, const char *name );

/* The forecast: one panel per quantity (the level, the annual change...),
 * stacked. Each panel has the last observations, then the forecasts with
 * their bands of +-2 sd -- the THEORETICAL bands, what the model implies.
 * y[0..n-1] and sd[0..n-1] (sd only read for the last nprev), dates[i] the
 * label of observation i. Titles and note are Latin-1 (the encoding of the
 * standard fonts); the note goes under the panels (NULL: none).
 *                                                    -> prev_<x11out>.eps  */
typedef struct {
   const char   *title;
   const double *y, *sd;
} FPPanel;

FDFig *fp_forecast( const FPPanel *p, int npanels, int n, int nprev,
                    const char *const *dates, const char *note, const char *x11out );

/* Write figs[0..n-1] as a PDF document: A4 (landscape if landscape != 0),
 * figure i scaled by scale[i] and centered, as many per page as fit.
 * Returns 0 on success.                                                    */
int fp_write_pdf( const char *filename, FDFig **figs, const double *scale, int n, int landscape );

#endif
