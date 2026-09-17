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

/* -a: standardized series                            -> <x11out>.eps       */
FDFig *fp_PlotSer( struct Tseries *ser, int tsnobs, int tmornsop, int tsby, double boxlam,
                   int nrdiff, int nadiff, const char *x11out, const char *name );

/* -b: acf and pacf                                   -> acf_<x11out>.eps   */
FDFig *fp_CorrSer( struct Tseries *ser, int npar, int lags, double cbands, const char *x11out,
                   const char *name );

/* -c: standardized series + acf/pacf                 -> <x11out>.eps       */
FDFig *fp_PlotSer_CorrSer( struct Tseries *ser, int npar, int tsnobs, int tmornsop,
                           int tsby, double boxlam, int nrdiff, int nadiff, int lags,
                           double cbands, const char *x11out, const char *name );

/* -d: histogram and normal density                   -> hist_<x11out>.eps  */
FDFig *fp_histogram( struct Tseries *ser, int nrdiff, int nadiff, double boxlam,
                     const char *x11out, const char *name );

/* -e: means and std. deviations of groups of nog observations of y[1..n]
 *                                                    -> m_dt_<x11out>.eps  */
FDFig *fp_graph_m_dt( double *y, int n, int nog, int freq, double boxlam,
                      const char *x11out, const char *name );

/* Write figs[0..n-1] as a PDF document: A4 (landscape if landscape != 0),
 * figure i scaled by scale[i] and centered, as many per page as fit.
 * Returns 0 on success.                                                    */
int fp_write_pdf( const char *filename, FDFig **figs, const double *scale, int n, int landscape );

#endif
