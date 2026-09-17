/***************************************************************************
 *   fufplot.h -- the forecast graph of FUF, drawn with fugdraw (no        *
 *   gnuplot). Copyright (C) 2026 by A.B. Treadway & D.E. Guerrero.        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef FUFPLOT_H
#define FUFPLOT_H

#include "fugdraw.h"

/* The graph of the forecasts: two panels, one over the other.
 *
 *   above  the last L observed values and the L forecasts, with their
 *          band of one standard deviation (title, e.g. "LRC anual (%)")
 *   below  the last L residuals, as impulses, with the zero line and the
 *          band of two standard deviations ("ERR")
 *
 * y[0..2L-1]: the series, observed and then forecast; band[] and band2[]:
 * the upper and the lower band of the forecast part (from L on); err[0..L-1]:
 * the residuals; sigma: their standard deviation. first_year and first_season
 * date y[0]. Returns the figure, to be written with fd_write_eps() or placed
 * in a page, or NULL.                                                      */
FDFig *fp_forecast( const double *y, const double *band, const double *band2,
                    const double *err, int L, double sigma, int freq,
                    int first_year, int first_season, const char *title );

#endif
