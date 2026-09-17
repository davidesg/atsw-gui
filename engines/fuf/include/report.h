/*****************************************************************************/
/* report.h -- the report of FUF (table of forecasts and graph) drawn with   */
/* fugdraw, without LaTeX.                                                   */
/* Copyright (C) 2026 A.B. Treadway & D.E. Guerrero. GPL v2 or later.        */
/*****************************************************************************/

#ifndef REPORT_H
#define REPORT_H

#include "fugdraw.h"

/* Write <filename> (A4, landscape): the heading of the series, the table of
 * the last L/2 observations and the L/2 first forecasts with their standard
 * errors, and the graph. The arguments after graph are those of
 * forecast_table_latex(), so the table says exactly the same; with full it
 * lists every forecast instead, and the table runs into as many columns as
 * it needs.
 * Returns 0 if the file was written.                                       */
int report_write_pdf( const char *filename, FDFig *graph, const char *name,
                      int nobs, int freq, int begyear, int begtime, int ornsop,
                      int L, double *data, double **a, double **f1, double **f2,
                      double **f3, double ***v1, double ***v2, double ***v3,
                      double boxlam, double refactor, int full );

#endif
